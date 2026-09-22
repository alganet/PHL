# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1357/1942 lines (69.88%)

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
|      - |   27 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - |   28 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - |   29 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - |   30 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - |   31 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - |   32 | ` */` |
|      8 |   33 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 |   34 | `{` |
|      9 |   35 | `	io_private *pDev = (io_private *)pResource;` |
|      9 |   36 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      7 |   37 | `		return "stream";` |
|      - |   38 | `	}` |
|      3 |   39 | `	return "Unknown";` |
|      5 |   40 | `}` |
|      - |   41 | `/*` |
|      - |   42 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|      - |   43 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|      - |   44 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|      - |   45 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|      - |   46 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|      - |   47 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|      - |   48 | ` */` |
|     58 |   49 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      4 |   50 | `{` |
|     62 |   51 | `	io_private *pDev = (io_private *)pResource;` |
|     62 |   52 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      4 |   53 | `}` |
|      - |   54 | `/*` |
|      - |   55 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - |   56 | ` *  Truncates a file to a given length.` |
|      - |   57 | ` * Parameters` |
|      - |   58 | ` *  $handle` |
|      - |   59 | ` *   The file pointer.` |
|      - |   60 | ` *   Note:` |
|      - |   61 | ` *    The handle must be open for writing.` |
|      - |   62 | ` * $size` |
|      - |   63 | ` *   The size to truncate to.` |
|      - |   64 | ` * Return` |
|      - |   65 | ` *  TRUE on success or FALSE on failure.` |
|      - |   66 | ` */` |
|      6 |   67 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   68 | `{` |
|      - |   69 | `	const ph7_io_stream *pStream;` |
|      - |   70 | `	io_private *pDev;` |
|      - |   71 | `	ph7_int64 nSize;` |
|      - |   72 | `	int rc;` |
|      7 |   73 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |   74 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |   75 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |   76 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   77 | `		return PH7_OK;` |
|      - |   78 | `	}` |
|      - |   79 | `	/* Extract our private data */` |
|      7 |   80 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |   81 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 |   82 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |   83 | `		/*Expecting an IO handle */` |
|    ! 0 |   84 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |   85 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   86 | `		return PH7_OK;` |
|      - |   87 | `	}` |
|      7 |   88 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|      7 |   89 | `	if( nSize < 0 ){` |
|      - |   90 | `		/* php 8: catchable ValueError, raised BEFORE the unsupported-stream` |
|      - |   91 | `		 * check (php-src orders the size check first). PHL used to truncate. */` |
|      3 |   92 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |   93 | `			"ftruncate(): Argument #2 ($size) must be greater than or equal to 0");` |
|      - |   94 | `	}` |
|      - |   95 | `	/* Point to the target IO stream device */` |
|      5 |   96 | `	pStream = pDev->pStream;` |
|      5 |   97 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 |   98 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   99 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  100 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  101 | `			);` |
|    ! 0 |  102 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  103 | `		return PH7_OK;` |
|      - |  104 | `	}` |
|      - |  105 | `	/* Perform the requested operation */` |
|      5 |  106 | `	rc = pStream->xTrunc(pDev->pHandle,nSize);` |
|      5 |  107 | `	if( rc == PH7_OK ){` |
|      - |  108 | `		/* Discard buffered data */` |
|      5 |  109 | `		ResetIOPrivate(pDev);` |
|      2 |  110 | `	}` |
|      - |  111 | `	/* IO result */` |
|      5 |  112 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 |  113 | `	return PH7_OK;` |
|      4 |  114 | `}` |
|      - |  115 | `/*` |
|      - |  116 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - |  117 | ` *  Seeks on a file pointer.` |
|      - |  118 | ` * Parameters` |
|      - |  119 | ` *  $handle` |
|      - |  120 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - |  121 | ` * $offset` |
|      - |  122 | ` *   The offset.` |
|      - |  123 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - |  124 | ` *   value in offset and set whence to SEEK_END.` |
|      - |  125 | ` *   whence` |
|      - |  126 | ` *   whence values are:` |
|      - |  127 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - |  128 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - |  129 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - |  130 | ` * Return` |
|      - |  131 | ` *  0 on success,-1 on failure` |
|      - |  132 | ` */` |
|     44 |  133 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  134 | `{` |
|      - |  135 | `	const ph7_io_stream *pStream;` |
|      - |  136 | `	io_private *pDev;` |
|      - |  137 | `	ph7_int64 iOfft;` |
|      - |  138 | `	int whence;` |
|      - |  139 | `	int rc;` |
|     46 |  140 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  141 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  142 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  143 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  144 | `		return PH7_OK;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Extract our private data */` |
|     46 |  147 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  148 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     46 |  149 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  150 | `		/*Expecting an IO handle */` |
|    ! 0 |  151 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  152 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  153 | `		return PH7_OK;` |
|      - |  154 | `	}` |
|      - |  155 | `	/* Point to the target IO stream device */` |
|     46 |  156 | `	pStream = pDev->pStream;` |
|     46 |  157 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  158 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  159 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  160 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  161 | `			);` |
|    ! 0 |  162 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  163 | `		return PH7_OK;` |
|      - |  164 | `	}` |
|      - |  165 | `	/* Extract the offset */` |
|     46 |  166 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     46 |  167 | `	whence = 0;/* SEEK_SET */` |
|     46 |  168 | `	if( nArg > 2 ){` |
|      - |  169 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|      - |  170 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|      - |  171 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|      - |  172 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|     37 |  173 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|     18 |  174 | `	}` |
|     46 |  175 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|      - |  176 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|      - |  177 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|      - |  178 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|      - |  179 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|      - |  180 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|     13 |  181 | `		ph7_result_int(pCtx,-1);` |
|     13 |  182 | `		return PH7_OK;` |
|      - |  183 | `	}` |
|      - |  184 | `	/* Perform the requested operation */` |
|     34 |  185 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     34 |  186 | `	if( rc == PH7_OK ){` |
|      - |  187 | `		/* Ignore buffered data */` |
|     34 |  188 | `		ResetIOPrivate(pDev);` |
|     16 |  189 | `	}` |
|      - |  190 | `	/* IO result */` |
|     34 |  191 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     34 |  192 | `	return PH7_OK;` |
|     24 |  193 | `}` |
|      - |  194 | `/*` |
|      - |  195 | ` * int64 ftell(resource $handle)` |
|      - |  196 | ` *  Returns the current position of the file read/write pointer.` |
|      - |  197 | ` * Parameters` |
|      - |  198 | ` *  $handle` |
|      - |  199 | ` *   The file pointer.` |
|      - |  200 | ` * Return` |
|      - |  201 | ` *  Returns the position of the file pointer referenced by handle` |
|      - |  202 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - |  203 | ` *  FALSE is returned on failure.` |
|      - |  204 | ` */` |
|     32 |  205 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  206 | `{` |
|      - |  207 | `	const ph7_io_stream *pStream;` |
|      - |  208 | `	io_private *pDev;` |
|      - |  209 | `	ph7_int64 iOfft;` |
|     34 |  210 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  211 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  212 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  213 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  214 | `		return PH7_OK;` |
|      - |  215 | `	}` |
|      - |  216 | `	/* Extract our private data */` |
|     34 |  217 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  218 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     34 |  219 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  220 | `		/*Expecting an IO handle */` |
|    ! 0 |  221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  223 | `		return PH7_OK;` |
|      - |  224 | `	}` |
|      - |  225 | `	/* Point to the target IO stream device */` |
|     34 |  226 | `	pStream = pDev->pStream;` |
|     34 |  227 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Perform the requested operation */` |
|     34 |  236 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - |  237 | `	/* IO result */` |
|     34 |  238 | `	ph7_result_int64(pCtx,iOfft);` |
|     34 |  239 | `	return PH7_OK;` |
|     18 |  240 | `}` |
|      - |  241 | `/*` |
|      - |  242 | ` * bool rewind(resource $handle)` |
|      - |  243 | ` *  Rewind the position of a file pointer.` |
|      - |  244 | ` * Parameters` |
|      - |  245 | ` *  $handle` |
|      - |  246 | ` *   The file pointer.` |
|      - |  247 | ` * Return` |
|      - |  248 | ` *  TRUE on success or FALSE on failure.` |
|      - |  249 | ` */` |
|     42 |  250 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  251 | `{` |
|      - |  252 | `	const ph7_io_stream *pStream;` |
|      - |  253 | `	io_private *pDev;` |
|      - |  254 | `	int rc;` |
|     43 |  255 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  256 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  257 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  259 | `		return PH7_OK;` |
|      - |  260 | `	}` |
|      - |  261 | `	/* Extract our private data */` |
|     43 |  262 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  263 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     43 |  264 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  265 | `		/*Expecting an IO handle */` |
|    ! 0 |  266 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  268 | `		return PH7_OK;` |
|      - |  269 | `	}` |
|      - |  270 | `	/* Point to the target IO stream device */` |
|     43 |  271 | `	pStream = pDev->pStream;` |
|     43 |  272 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  273 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  274 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  275 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  276 | `			);` |
|    ! 0 |  277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  278 | `		return PH7_OK;` |
|      - |  279 | `	}` |
|      - |  280 | `	/* Perform the requested operation */` |
|     43 |  281 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     43 |  282 | `	if( rc == PH7_OK ){` |
|      - |  283 | `		/* Ignore buffered data */` |
|     43 |  284 | `		ResetIOPrivate(pDev);` |
|     21 |  285 | `	}` |
|      - |  286 | `	/* IO result */` |
|     43 |  287 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     43 |  288 | `	return PH7_OK;` |
|     22 |  289 | `}` |
|      - |  290 | `/*` |
|      - |  291 | ` * bool fflush(resource $handle)` |
|      - |  292 | ` *  Flushes the output to a file.` |
|      - |  293 | ` * Parameters` |
|      - |  294 | ` *  $handle` |
|      - |  295 | ` *   The file pointer.` |
|      - |  296 | ` * Return` |
|      - |  297 | ` *  TRUE on success or FALSE on failure.` |
|      - |  298 | ` */` |
|      2 |  299 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  300 | `{` |
|      - |  301 | `	const ph7_io_stream *pStream;` |
|      - |  302 | `	io_private *pDev;` |
|      - |  303 | `	int rc;` |
|      3 |  304 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  305 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  306 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  308 | `		return PH7_OK;` |
|      - |  309 | `	}` |
|      - |  310 | `	/* Extract our private data */` |
|      3 |  311 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  312 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |  313 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  314 | `		/*Expecting an IO handle */` |
|    ! 0 |  315 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  317 | `		return PH7_OK;` |
|      - |  318 | `	}` |
|      - |  319 | `	/* Point to the target IO stream device */` |
|      3 |  320 | `	pStream = pDev->pStream;` |
|      3 |  321 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 |  322 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  323 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  324 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  325 | `			);` |
|    ! 0 |  326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  327 | `		return PH7_OK;` |
|      - |  328 | `	}` |
|      - |  329 | `	/* Perform the requested operation */` |
|      3 |  330 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - |  331 | `	/* IO result */` |
|      3 |  332 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  333 | `	return PH7_OK;` |
|      2 |  334 | `}` |
|      - |  335 | `/*` |
|      - |  336 | ` * bool feof(resource $handle)` |
|      - |  337 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  338 | ` * Parameters` |
|      - |  339 | ` *  $handle` |
|      - |  340 | ` *   The file pointer.` |
|      - |  341 | ` * Return` |
|      - |  342 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  343 | ` */` |
|  16163 |  344 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  345 | `{` |
|      - |  346 | `	const ph7_io_stream *pStream;` |
|      - |  347 | `	io_private *pDev;` |
|      - |  348 | `	int rc;` |
|  16168 |  349 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  350 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  351 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  352 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  353 | `		return PH7_OK;` |
|      - |  354 | `	}` |
|      - |  355 | `	/* Extract our private data */` |
|  16168 |  356 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  357 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  16168 |  358 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  359 | `		/*Expecting an IO handle */` |
|    ! 0 |  360 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  361 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  362 | `		return PH7_OK;` |
|      - |  363 | `	}` |
|      - |  364 | `	/* Point to the target IO stream device */` |
|  16168 |  365 | `	pStream = pDev->pStream;` |
|  16168 |  366 | `	if( pStream == 0 ){` |
|    ! 0 |  367 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  368 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  369 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  370 | `			);` |
|    ! 0 |  371 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  372 | `		return PH7_OK;` |
|      - |  373 | `	}` |
|  16168 |  374 | `	rc = SXERR_EOF;` |
|      - |  375 | `	/* Perform the requested operation */` |
|  16168 |  376 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  377 | `		/* Data is available */` |
|   9305 |  378 | `		rc = PH7_OK;` |
|   4655 |  379 | `	}else{` |
|      - |  380 | `		char zBuf[4096];` |
|      - |  381 | `		ph7_int64 n;` |
|      - |  382 | `		/* Perform a buffered read */` |
|   6868 |  383 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   6868 |  384 | `		if( n > 0 ){` |
|      - |  385 | `			/* Copy buffered data */` |
|   2334 |  386 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   2334 |  387 | `			rc = PH7_OK;` |
|   1164 |  388 | `		}` |
|      - |  389 | `	}` |
|      - |  390 | `	/* EOF or not */` |
|  16168 |  391 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  16168 |  392 | `	return PH7_OK;` |
|   8086 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  396 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  397 | ` * [i.e: IO error ] or EOF.` |
|      - |  398 | ` */` |
|     37 |  399 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      3 |  400 | `{` |
|     40 |  401 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     40 |  402 | `	char *zBuf = (char *)pBuf;` |
|      - |  403 | `	ph7_int64 n,nRead;` |
|     40 |  404 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     40 |  405 | `	if( n > 0 ){` |
|      3 |  406 | `		if( n > nLen ){` |
|    ! 0 |  407 | `			n = nLen;` |
|    ! 0 |  408 | `		}` |
|      - |  409 | `		/* Copy the buffered data */` |
|      3 |  410 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  411 | `		/* Update the read offset */` |
|      3 |  412 | `		pDev->nOfft += (sxu32)n;` |
|      3 |  413 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  414 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      3 |  415 | `			SyBlobReset(&pDev->sBuffer);` |
|      3 |  416 | `			pDev->nOfft = 0;` |
|      1 |  417 | `		}` |
|      3 |  418 | `		nLen -= n;` |
|      3 |  419 | `		if( nLen < 1 ){` |
|      - |  420 | `			/* All done */` |
|    ! 0 |  421 | `			return n;` |
|      - |  422 | `		}` |
|      - |  423 | `		/* Advance the cursor */` |
|      3 |  424 | `		zBuf += n;` |
|      1 |  425 | `	}` |
|      - |  426 | `	/* Read without buffering */` |
|     40 |  427 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     40 |  428 | `	if( nRead > 0 ){` |
|     37 |  429 | `		n += nRead;` |
|     21 |  430 | `	}else if( n < 1 ){` |
|      - |  431 | `		/* EOF or IO error */` |
|      3 |  432 | `		return nRead;` |
|      - |  433 | `	}` |
|     38 |  434 | `	return n;` |
|     21 |  435 | `}` |
|      - |  436 | `/*` |
|      - |  437 | ` * Extract a single line from the buffered input.` |
|      - |  438 | ` */` |
|  11852 |  439 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  440 | `{` |
|      - |  441 | `	const char *zIn,*zEnd,*zPtr;` |
|  11857 |  442 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|  11857 |  443 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|  11857 |  444 | `	zPtr = zIn;` |
| 596208 |  445 | `	while( zIn < zEnd ){` |
| 596014 |  446 | `		if( zIn[0] == '\n' ){` |
|      - |  447 | `			/* Line found */` |
|  11663 |  448 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|  11663 |  449 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|  11663 |  450 | `			*pzLine = zPtr;` |
|  11663 |  451 | `			return SXRET_OK;` |
|      - |  452 | `		}` |
| 584356 |  453 | `		zIn++;` |
|      5 |  454 | `	}` |
|      - |  455 | `	/* No line were found */` |
|    199 |  456 | `	return SXERR_NOTFOUND;` |
|   5931 |  457 | `}` |
|      - |  458 | `/*` |
|      - |  459 | ` * Read a single line from the underlying IO stream device.` |
|      - |  460 | ` */` |
|  11874 |  461 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  462 | `{` |
|  11879 |  463 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - |  464 | `	char zBuf[8192];` |
|      - |  465 | `	ph7_int64 n;` |
|      - |  466 | `	sxi32 rc;` |
|  11879 |  467 | `	n = 0;` |
|  11879 |  468 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  469 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    165 |  470 | `		SyBlobReset(&pDev->sBuffer);` |
|    165 |  471 | `		pDev->nOfft = 0;` |
|     80 |  472 | `	}` |
|  11879 |  473 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  474 | `		/* Check if there is a line */` |
|  11719 |  475 | `		rc = GetLine(pDev,&n,pzData);` |
|  11719 |  476 | `		if( rc == SXRET_OK ){` |
|      - |  477 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  478 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|  11567 |  479 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  480 | `				n = nMaxLen;` |
|    ! 0 |  481 | `			}` |
|  11567 |  482 | `			pDev->nOfft += (sxu32)n;` |
|  11567 |  483 | `			return n;` |
|      - |  484 | `		}` |
|     76 |  485 | `	}` |
|      - |  486 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  487 | `	 * limit is reached.` |
|      - |  488 | `	 */` |
|    160 |  489 | `	for(;;){` |
|    325 |  490 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    325 |  491 | `		if( n < 1 ){` |
|      - |  492 | `			/* EOF or IO error */` |
|    187 |  493 | `			break;` |
|      - |  494 | `		}` |
|      - |  495 | `		/* Append the data just read */` |
|    140 |  496 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  497 | `		/* Try to extract a line */` |
|    140 |  498 | `		rc = GetLine(pDev,&n,pzData);` |
|    140 |  499 | `		if( rc == SXRET_OK ){` |
|      - |  500 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|      - |  501 | `			 * cap, newline included, is left buffered for the next read. */` |
|     98 |  502 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|      5 |  503 | `				n = nMaxLen;` |
|      2 |  504 | `			}` |
|     98 |  505 | `			pDev->nOfft += (sxu32)n;` |
|     98 |  506 | `			return n;` |
|      - |  507 | `		}` |
|     43 |  508 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - |  509 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|      - |  510 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|      - |  511 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|      - |  512 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|      - |  513 | `			 * buffer once it is fully consumed. */` |
|     35 |  514 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     35 |  515 | `			n = nMaxLen;` |
|     35 |  516 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     35 |  517 | `			return n;` |
|      - |  518 | `		}` |
|      1 |  519 | `	}` |
|    187 |  520 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  521 | `		/* Read limit reached,return the available data */` |
|    153 |  522 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    153 |  523 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  524 | `		/* Reset the working buffer */` |
|    153 |  525 | `		SyBlobReset(&pDev->sBuffer);` |
|    153 |  526 | `		pDev->nOfft = 0;` |
|     74 |  527 | `	}` |
|    187 |  528 | `	return n;` |
|   5942 |  529 | `}` |
|      - |  530 | `/*` |
|      - |  531 | ` * Open an IO stream handle.` |
|      - |  532 | ` * Notes on stream:` |
|      - |  533 | ` * According to the PHP reference manual.` |
|      - |  534 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - |  535 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - |  536 | ` * to an arbitrary locations within the stream.` |
|      - |  537 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - |  538 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - |  539 | ` * on a remote server.` |
|      - |  540 | ` * A stream is referenced as: scheme://target` |
|      - |  541 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - |  542 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - |  543 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - |  544 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - |  545 | ` *  with a path appended.` |
|      - |  546 | ` *` |
|      - |  547 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - |  548 | ` * Please refer to the official documentation for a full discussion.` |
|      - |  549 | ` * This function return a handle on success. Otherwise null.` |
|      - |  550 | ` */` |
|  31088 |  551 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  552 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 |  553 | `{` |
|  31093 |  554 | `	void *pHandle = 0; /* cc warning */` |
|      - |  555 | `	SyString sFile;` |
|      - |  556 | `	ph7_value sDummy;` |
|      - |  557 | `	int rc;` |
|  31093 |  558 | `	if( pStream == 0 ){` |
|      - |  559 | `		/* No such stream device */` |
|    ! 0 |  560 | `		return 0;` |
|      - |  561 | `	}` |
|  31093 |  562 | `	if( pResource == 0 ){` |
|      - |  563 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  564 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  565 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  566 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  567 | `		 * reads it during the call, and file:// ignores it. */` |
|  30987 |  568 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30987 |  569 | `		pResource = &sDummy;` |
|  15491 |  570 | `	}` |
|  31093 |  571 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  31093 |  572 | `	if( use_include ){` |
|   9386 |  573 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - |  574 | `#ifdef __WINNT__` |
|      - |  575 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  576 | `#endif` |
|   9345 |  577 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9338 |  578 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - |  579 | `				/*  Open the file directly */` |
|     53 |  580 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     53 |  581 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - |  582 | `					/* Mark as included */` |
|     51 |  583 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     23 |  584 | `				}` |
|     29 |  585 | `		}else{` |
|      - |  586 | `			SyString *pPath;` |
|      - |  587 | `			SyBlob sWorker;` |
|      - |  588 | `#ifdef __WINNT__` |
|      - |  589 | `			static const int c = '\\';` |
|      - |  590 | `#else` |
|      - |  591 | `			static const int c = '/';` |
|      - |  592 | `#endif` |
|      - |  593 | `			/* Init the path builder working buffer */` |
|   9341 |  594 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  595 | `			/* Build a path from the set of include path */` |
|   9341 |  596 | `			SySetResetCursor(&pVm->aPaths);` |
|   9341 |  597 | `			rc = SXERR_IO;` |
|   9347 |  598 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - |  599 | `				/* Build full path */` |
|   9341 |  600 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - |  601 | `				/* Append null terminator */` |
|   9341 |  602 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  603 | `					continue;` |
|      - |  604 | `				}` |
|      - |  605 | `				/* Try to open the file */` |
|   9341 |  606 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9341 |  607 | `				if( rc == PH7_OK ){` |
|   9334 |  608 | `					if( bPushInclude ){` |
|      - |  609 | `						/* Mark as included */` |
|   9334 |  610 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4666 |  611 | `					}` |
|   9334 |  612 | `					break;` |
|      - |  613 | `				}` |
|      - |  614 | `				/* Reset the working buffer */` |
|      8 |  615 | `				SyBlobReset(&sWorker);` |
|      - |  616 | `				/* Check the next path */` |
|      2 |  617 | `			}` |
|   9341 |  618 | `			SyBlobRelease(&sWorker);` |
|      - |  619 | `		}` |
|   4698 |  620 | `	}else{` |
|      - |  621 | `		/* Open the URI direcly */` |
|  21707 |  622 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - |  623 | `	}` |
|  31093 |  624 | `	if( rc != PH7_OK ){` |
|      - |  625 | `		/* IO error */` |
|     25 |  626 | `		return 0;` |
|      - |  627 | `	}` |
|      - |  628 | `	/* Return the file handle */` |
|  31073 |  629 | `	return pHandle;` |
|  15549 |  630 | `}` |
|      - |  631 | `/*` |
|      - |  632 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - |  633 | ` * Store the read data in the given BLOB (last argument).` |
|      - |  634 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - |  635 | ` */` |
|   9368 |  636 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      5 |  637 | `{` |
|      - |  638 | `	ph7_int64 nRead;` |
|      - |  639 | `	char zBuf[8192]; /* 8K */` |
|      - |  640 | `	int rc;` |
|      - |  641 | `	/* Perform the requested operation */` |
|   9368 |  642 | `	for(;;){` |
|  18741 |  643 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  18741 |  644 | `		if( nRead < 1 ){` |
|      - |  645 | `			/* EOF or IO error */` |
|   9373 |  646 | `			break;` |
|      - |  647 | `		}` |
|      - |  648 | `		/* Append contents */` |
|   9373 |  649 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9373 |  650 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  651 | `			break;` |
|      - |  652 | `		}` |
|      5 |  653 | `	}` |
|   9373 |  654 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      5 |  655 | `}` |
|      - |  656 | `/*` |
|      - |  657 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - |  658 | ` */` |
|  31164 |  659 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 |  660 | `{` |
|  31169 |  661 | `	if( pStream->xClose ){` |
|  31169 |  662 | `		pStream->xClose(pHandle);` |
|  15582 |  663 | `	}` |
|  31169 |  664 | `}` |
|      - |  665 | `/*` |
|      - |  666 | ` * string fgetc(resource $handle)` |
|      - |  667 | ` *  Gets a character from the given file pointer.` |
|      - |  668 | ` * Parameters` |
|      - |  669 | ` *  $handle` |
|      - |  670 | ` *   The file pointer.` |
|      - |  671 | ` * Return` |
|      - |  672 | ` *  Returns a string containing a single character read from the file` |
|      - |  673 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - |  674 | ` * WARNING` |
|      - |  675 | ` *  This operation is extremely slow.Avoid using it.` |
|      - |  676 | ` */` |
|      4 |  677 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  678 | `{` |
|      - |  679 | `	const ph7_io_stream *pStream;` |
|      - |  680 | `	io_private *pDev;` |
|      - |  681 | `	int c,n;` |
|      5 |  682 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  683 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  684 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  686 | `		return PH7_OK;` |
|      - |  687 | `	}` |
|      - |  688 | `	/* Extract our private data */` |
|      5 |  689 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  690 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 |  691 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  692 | `		/*Expecting an IO handle */` |
|    ! 0 |  693 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  695 | `		return PH7_OK;` |
|      - |  696 | `	}` |
|      - |  697 | `	/* Point to the target IO stream device */` |
|      5 |  698 | `	pStream = pDev->pStream;` |
|      5 |  699 | `	if( pStream == 0  ){` |
|    ! 0 |  700 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  701 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  702 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  703 | `			);` |
|    ! 0 |  704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  705 | `		return PH7_OK;` |
|      - |  706 | `	}` |
|      - |  707 | `	/* Perform the requested operation */` |
|      5 |  708 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - |  709 | `	/* IO result */` |
|      5 |  710 | `	if( n < 1 ){` |
|      - |  711 | `		/* EOF or error,return FALSE */` |
|    ! 0 |  712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  713 | `	}else{` |
|      - |  714 | `		/* Return the string holding the character */` |
|      5 |  715 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - |  716 | `	}` |
|      5 |  717 | `	return PH7_OK;` |
|      3 |  718 | `}` |
|      - |  719 | `/*` |
|      - |  720 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - |  721 | ` *  Gets line from file pointer.` |
|      - |  722 | ` * Parameters` |
|      - |  723 | ` *  $handle` |
|      - |  724 | ` *   The file pointer.` |
|      - |  725 | ` * $length` |
|      - |  726 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  727 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  728 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  729 | ` *  the end of the line.` |
|      - |  730 | ` * Return` |
|      - |  731 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - |  732 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - |  733 | ` *  If an error occurs, FALSE is returned.` |
|      - |  734 | ` */` |
|  11750 |  735 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  736 | `{` |
|      - |  737 | `	const ph7_io_stream *pStream;` |
|      - |  738 | `	const char *zLine;` |
|      - |  739 | `	io_private *pDev;` |
|      - |  740 | `	ph7_int64 n,nLen;` |
|  11755 |  741 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  742 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  743 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  745 | `		return PH7_OK;` |
|      - |  746 | `	}` |
|      - |  747 | `	/* Extract our private data */` |
|  11755 |  748 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  749 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  11755 |  750 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  751 | `		/*Expecting an IO handle */` |
|    ! 0 |  752 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  754 | `		return PH7_OK;` |
|      - |  755 | `	}` |
|      - |  756 | `	/* Point to the target IO stream device */` |
|  11755 |  757 | `	pStream = pDev->pStream;` |
|  11755 |  758 | `	if( pStream == 0  ){` |
|    ! 0 |  759 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  760 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  761 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  762 | `			);` |
|    ! 0 |  763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  764 | `		return PH7_OK;` |
|      - |  765 | `	}` |
|  11755 |  766 | `	nLen = -1;` |
|  11755 |  767 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - |  768 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|      - |  769 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|     61 |  770 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     61 |  771 | `		if( nLen < 1 ){` |
|      5 |  772 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  773 | `				"fgets(): Argument #2 ($length) must be greater than 0");` |
|      - |  774 | `		}` |
|      - |  775 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|      - |  776 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|      - |  777 | `		 * false at any position, exactly like EOF. */` |
|     57 |  778 | `		nLen -= 1;` |
|     57 |  779 | `		if( nLen == 0 ){` |
|      3 |  780 | `			ph7_result_bool(pCtx,0);` |
|      3 |  781 | `			return PH7_OK;` |
|      - |  782 | `		}` |
|     27 |  783 | `	}` |
|      - |  784 | `	/* Perform the requested operation */` |
|  11749 |  785 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|  11749 |  786 | `	if( n < 1 ){` |
|      - |  787 | `		/* EOF or IO error,return FALSE */` |
|     13 |  788 | `		ph7_result_bool(pCtx,0);` |
|      9 |  789 | `	}else{` |
|      - |  790 | `		/* Return the freshly extracted line */` |
|  11741 |  791 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - |  792 | `	}` |
|  11749 |  793 | `	return PH7_OK;` |
|   5880 |  794 | `}` |
|      - |  795 | `/*` |
|      - |  796 | ` * string fread(resource $handle,int64 $length)` |
|      - |  797 | ` *  Binary-safe file read.` |
|      - |  798 | ` * Parameters` |
|      - |  799 | ` *  $handle` |
|      - |  800 | ` *   The file pointer.` |
|      - |  801 | ` * $length` |
|      - |  802 | ` *  Up to length number of bytes read.` |
|      - |  803 | ` * Return` |
|      - |  804 | ` *  The data readen on success or FALSE on failure.` |
|      - |  805 | ` */` |
|     33 |  806 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  807 | `{` |
|      - |  808 | `	const ph7_io_stream *pStream;` |
|      - |  809 | `	io_private *pDev;` |
|      - |  810 | `	ph7_int64 nRead;` |
|      - |  811 | `	void *pBuf;` |
|      - |  812 | `	int nLen;` |
|     36 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  815 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  816 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  817 | `		return PH7_OK;` |
|      - |  818 | `	}` |
|      - |  819 | `	/* Extract our private data */` |
|     36 |  820 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  821 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     36 |  822 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  823 | `		/*Expecting an IO handle */` |
|    ! 0 |  824 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  826 | `		return PH7_OK;` |
|      - |  827 | `	}` |
|      - |  828 | `	/* Point to the target IO stream device */` |
|     36 |  829 | `	pStream = pDev->pStream;` |
|     36 |  830 | `	if( pStream == 0  ){` |
|    ! 0 |  831 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  832 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  833 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  834 | `			);` |
|    ! 0 |  835 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  836 | `		return PH7_OK;` |
|      - |  837 | `	}` |
|     36 |  838 | `        nLen = 4096;` |
|     36 |  839 | `	if( nArg > 1 ){` |
|      - |  840 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - |  841 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - |  842 | `	   * the central type screen (the recorded null-policy divergence). */` |
|     36 |  843 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|     36 |  844 | `	  if( nWant < 1 ){` |
|      5 |  845 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  846 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - |  847 | `	  }` |
|     32 |  848 | `	  nLen = (int)nWant;` |
|     32 |  849 | `	  if( nLen < 1 ){` |
|      - |  850 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - |  851 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - |  852 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - |  853 | `		 * that do not fit an int. */` |
|    ! 0 |  854 | `		nLen = 4096;` |
|    ! 0 |  855 | `	  }` |
|     14 |  856 | `        }` |
|      - |  857 | `	/* Allocate enough buffer */` |
|     32 |  858 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     32 |  859 | `	if( pBuf == 0 ){` |
|    ! 0 |  860 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  862 | `		return PH7_OK;` |
|      - |  863 | `	}` |
|      - |  864 | `	/* Perform the requested operation */` |
|     32 |  865 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     32 |  866 | `	if( nRead < 1 ){` |
|      - |  867 | `		/* Nothing read,return FALSE */` |
|    ! 0 |  868 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  869 | `	}else{` |
|      - |  870 | `		/* Make a copy of the data just read */` |
|     32 |  871 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - |  872 | `	}` |
|      - |  873 | `	/* Release the buffer */` |
|     32 |  874 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     32 |  875 | `	return PH7_OK;` |
|     19 |  876 | `}` |
|      - |  877 | `/*` |
|      - |  878 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - |  879 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - |  880 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - |  881 | ` * Parameters` |
|      - |  882 | ` * $handle` |
|      - |  883 | ` *   The file pointer.` |
|      - |  884 | ` * $length` |
|      - |  885 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  886 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  887 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  888 | ` *  the end of the line.` |
|      - |  889 | ` * $delimiter` |
|      - |  890 | ` *   Set the field delimiter (one character only).` |
|      - |  891 | ` * $enclosure` |
|      - |  892 | ` *   Set the field enclosure character (one character only).` |
|      - |  893 | ` * $escape` |
|      - |  894 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - |  895 | ` * Return` |
|      - |  896 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - |  897 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - |  898 | ` *  If an error occurs, FALSE is returned.` |
|      - |  899 | ` */` |
|     22 |  900 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  901 | `{` |
|      - |  902 | `	const ph7_io_stream *pStream;` |
|      - |  903 | `	const char *zLine;` |
|      - |  904 | `	io_private *pDev;` |
|      - |  905 | `	ph7_int64 n,nLen;` |
|     23 |  906 | `	int delim  = ',';   /* Delimiter */` |
|     23 |  907 | `	int encl   = '"' ;  /* Enclosure */` |
|     23 |  908 | `	int escape = '\\';  /* Escape character */` |
|     23 |  909 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  910 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  911 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  912 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  913 | `		return PH7_OK;` |
|      - |  914 | `	}` |
|      - |  915 | `	/* Extract our private data */` |
|     23 |  916 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  917 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     23 |  918 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  919 | `		/*Expecting an IO handle */` |
|    ! 0 |  920 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  922 | `		return PH7_OK;` |
|      - |  923 | `	}` |
|      - |  924 | `	/* Point to the target IO stream device */` |
|     23 |  925 | `	pStream = pDev->pStream;` |
|     23 |  926 | `	if( pStream == 0  ){` |
|    ! 0 |  927 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  928 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  929 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  930 | `			);` |
|    ! 0 |  931 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  932 | `		return PH7_OK;` |
|      - |  933 | `	}` |
|     23 |  934 | `	if( nArg > 2 ){` |
|      - |  935 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|      - |  936 | `		 * ordering) and even when the stream is already at EOF. */` |
|     23 |  937 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|     23 |  938 | `		if( rc != PH7_OK ){` |
|      7 |  939 | `			return rc;` |
|      - |  940 | `		}` |
|     17 |  941 | `		if( nArg > 3 ){` |
|     17 |  942 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|     17 |  943 | `			if( rc != PH7_OK ){` |
|      3 |  944 | `				return rc;` |
|      - |  945 | `			}` |
|     15 |  946 | `			if( nArg > 4 ){` |
|     15 |  947 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|     15 |  948 | `				if( rc != PH7_OK ){` |
|      3 |  949 | `					return rc;` |
|      - |  950 | `				}` |
|      6 |  951 | `			}` |
|      6 |  952 | `		}` |
|      6 |  953 | `	}` |
|     13 |  954 | `	nLen = -1;` |
|     13 |  955 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - |  956 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|      - |  957 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|      - |  958 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|      5 |  959 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      5 |  960 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|      3 |  961 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  962 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|      - |  963 | `		}` |
|      - |  964 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|      1 |  965 | `	}` |
|      - |  966 | `	/* Perform the requested operation */` |
|     11 |  967 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|     11 |  968 | `	if( n < 1 ){` |
|      - |  969 | `		/* EOF or IO error,return FALSE */` |
|      3 |  970 | `		ph7_result_bool(pCtx,0);` |
|      2 |  971 | `	}else{` |
|      - |  972 | `		ph7_value *pArray;` |
|      - |  973 | `		/* Create our array */` |
|      9 |  974 | `		pArray = ph7_context_new_array(pCtx);` |
|      9 |  975 | `		if( pArray == 0 ){` |
|    ! 0 |  976 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  977 | `			ph7_result_null(pCtx);` |
|    ! 0 |  978 | `			return PH7_OK;` |
|      - |  979 | `		}` |
|      - |  980 | `		/* Parse the raw input */` |
|      9 |  981 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - |  982 | `		/* Return the freshly created array  */` |
|      9 |  983 | `		ph7_result_value(pCtx,pArray);` |
|      - |  984 | `	}` |
|     11 |  985 | `	return PH7_OK;` |
|     12 |  986 | `}` |
|      - |  987 | `/*` |
|      - |  988 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - |  989 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - |  990 | ` * Parameters` |
|      - |  991 | ` * $handle` |
|      - |  992 | ` *   The file pointer.` |
|      - |  993 | ` * $length` |
|      - |  994 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  995 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  996 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  997 | ` *  the end of the line.` |
|      - |  998 | ` * $allowable_tags` |
|      - |  999 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 1000 | ` * Return` |
|      - | 1001 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 1002 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 1003 | ` */` |
|      2 | 1004 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1005 | `{` |
|      - | 1006 | `	const ph7_io_stream *pStream;` |
|      - | 1007 | `	const char *zLine;` |
|      - | 1008 | `	io_private *pDev;` |
|      - | 1009 | `	ph7_int64 n,nLen;` |
|      3 | 1010 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1011 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1012 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `		return PH7_OK;` |
|      - | 1015 | `	}` |
|      - | 1016 | `	/* Extract our private data */` |
|      3 | 1017 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1018 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1019 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1020 | `		/*Expecting an IO handle */` |
|    ! 0 | 1021 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1023 | `		return PH7_OK;` |
|      - | 1024 | `	}` |
|      - | 1025 | `	/* Point to the target IO stream device */` |
|      3 | 1026 | `	pStream = pDev->pStream;` |
|      3 | 1027 | `	if( pStream == 0  ){` |
|    ! 0 | 1028 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1029 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1030 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1031 | `			);` |
|    ! 0 | 1032 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1033 | `		return PH7_OK;` |
|      - | 1034 | `	}` |
|      3 | 1035 | `	nLen = -1;` |
|      3 | 1036 | `	if( nArg > 1 ){` |
|      - | 1037 | `		/* Maximum data to read */` |
|    ! 0 | 1038 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 1039 | `	}` |
|      - | 1040 | `	/* Perform the requested operation */` |
|      3 | 1041 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 1042 | `	if( n < 1 ){` |
|      - | 1043 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 1044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1045 | `	}else{` |
|      3 | 1046 | `		const char *zTaglist = 0;` |
|      3 | 1047 | `		int nTaglen = 0;` |
|      3 | 1048 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 1049 | `			/* Allowed tag */` |
|    ! 0 | 1050 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 1051 | `		}` |
|      - | 1052 | `		/* Process data just read */` |
|      3 | 1053 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 1054 | `	}` |
|      3 | 1055 | `	return PH7_OK;` |
|      2 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * string readdir(resource $dir_handle)` |
|      - | 1059 | ` *   Read entry from directory handle.` |
|      - | 1060 | ` * Parameter` |
|      - | 1061 | ` *  $dir_handle` |
|      - | 1062 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1063 | ` * Return` |
|      - | 1064 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 1065 | ` */` |
|  11820 | 1066 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1067 | `{` |
|      - | 1068 | `	const ph7_io_stream *pStream;` |
|      - | 1069 | `	io_private *pDev;` |
|      - | 1070 | `	int rc;` |
|  11825 | 1071 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1072 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1073 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1075 | `		return PH7_OK;` |
|      - | 1076 | `	}` |
|      - | 1077 | `	/* Extract our private data */` |
|  11825 | 1078 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1079 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  11825 | 1080 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1081 | `		/*Expecting an IO handle */` |
|    ! 0 | 1082 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1083 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1084 | `		return PH7_OK;` |
|      - | 1085 | `	}` |
|      - | 1086 | `	/* Point to the target IO stream device */` |
|  11825 | 1087 | `	pStream = pDev->pStream;` |
|  11825 | 1088 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1089 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1090 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1091 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1092 | `			);` |
|    ! 0 | 1093 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1094 | `		return PH7_OK;` |
|      - | 1095 | `	}` |
|  11825 | 1096 | `	ph7_result_bool(pCtx,0);` |
|      - | 1097 | `	/* Perform the requested operation */` |
|  11825 | 1098 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  11825 | 1099 | `	if( rc != PH7_OK ){` |
|      - | 1100 | `		/* Return FALSE */` |
|   1139 | 1101 | `		ph7_result_bool(pCtx,0);` |
|    567 | 1102 | `	}` |
|  11825 | 1103 | `	return PH7_OK;` |
|   5915 | 1104 | `}` |
|      - | 1105 | `/*` |
|      - | 1106 | ` * void rewinddir(resource $dir_handle)` |
|      - | 1107 | ` *   Rewind directory handle.` |
|      - | 1108 | ` * Parameter` |
|      - | 1109 | ` *  $dir_handle` |
|      - | 1110 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1111 | ` * Return` |
|      - | 1112 | ` *  FALSE on failure.` |
|      - | 1113 | ` */` |
|      2 | 1114 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1115 | `{` |
|      - | 1116 | `	const ph7_io_stream *pStream;` |
|      - | 1117 | `	io_private *pDev;` |
|      3 | 1118 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1119 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1120 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1121 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1122 | `		return PH7_OK;` |
|      - | 1123 | `	}` |
|      - | 1124 | `	/* Extract our private data */` |
|      3 | 1125 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1126 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1127 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1128 | `		/*Expecting an IO handle */` |
|    ! 0 | 1129 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1130 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1131 | `		return PH7_OK;` |
|      - | 1132 | `	}` |
|      - | 1133 | `	/* Point to the target IO stream device */` |
|      3 | 1134 | `	pStream = pDev->pStream;` |
|      3 | 1135 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 1136 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1137 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1138 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1139 | `			);` |
|    ! 0 | 1140 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1141 | `		return PH7_OK;` |
|      - | 1142 | `	}` |
|      - | 1143 | `	/* Perform the requested operation */` |
|      3 | 1144 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 1145 | `	return PH7_OK;` |
|      2 | 1146 | ` }` |
|      - | 1147 | `/* Forward declaration */` |
|      - | 1148 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 1149 | `/*` |
|      - | 1150 | ` * void closedir(resource $dir_handle)` |
|      - | 1151 | ` *   Close directory handle.` |
|      - | 1152 | ` * Parameter` |
|      - | 1153 | ` *  $dir_handle` |
|      - | 1154 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1155 | ` * Return` |
|      - | 1156 | ` *  FALSE on failure.` |
|      - | 1157 | ` */` |
|   1138 | 1158 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1159 | `{` |
|      - | 1160 | `	const ph7_io_stream *pStream;` |
|      - | 1161 | `	io_private *pDev;` |
|   1143 | 1162 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1163 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1164 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1166 | `		return PH7_OK;` |
|      - | 1167 | `	}` |
|      - | 1168 | `	/* Extract our private data */` |
|   1143 | 1169 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1170 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1143 | 1171 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1172 | `		/*Expecting an IO handle */` |
|    ! 0 | 1173 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1174 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1175 | `		return PH7_OK;` |
|      - | 1176 | `	}` |
|      - | 1177 | `	/* Point to the target IO stream device */` |
|   1143 | 1178 | `	pStream = pDev->pStream;` |
|   1143 | 1179 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1180 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1181 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1182 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1183 | `			);` |
|    ! 0 | 1184 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1185 | `		return PH7_OK;` |
|      - | 1186 | `	}` |
|      - | 1187 | `	/* Perform the requested operation */` |
|   1143 | 1188 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1189 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1143 | 1190 | `	MarkIOPrivateClosed(pDev);` |
|   1143 | 1191 | `	return PH7_OK;` |
|    574 | 1192 | ` }` |
|      - | 1193 | `/*` |
|      - | 1194 | ` * resource opendir(string $path[,resource $context])` |
|      - | 1195 | ` *  Open directory handle.` |
|      - | 1196 | ` * Parameters` |
|      - | 1197 | ` * $path` |
|      - | 1198 | ` *   The directory path that is to be opened.` |
|      - | 1199 | ` * $context` |
|      - | 1200 | ` *   A context stream resource.` |
|      - | 1201 | ` * Return` |
|      - | 1202 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 1203 | ` */` |
|   1138 | 1204 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1205 | `{` |
|      - | 1206 | `	const ph7_io_stream *pStream;` |
|      - | 1207 | `	const char *zPath;` |
|      - | 1208 | `	io_private *pDev;` |
|      - | 1209 | `	int iLen,rc;` |
|   1143 | 1210 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1211 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1212 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1213 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1214 | `		return PH7_OK;` |
|      - | 1215 | `	}` |
|      - | 1216 | `	/* Extract the target path */` |
|   1143 | 1217 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1218 | `	/* Try to extract a stream */` |
|   1143 | 1219 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1143 | 1220 | `	if( pStream == 0 ){` |
|    ! 0 | 1221 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 1222 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 1223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1224 | `		return PH7_OK;` |
|      - | 1225 | `	}` |
|   1143 | 1226 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1227 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1228 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1229 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1230 | `			);` |
|    ! 0 | 1231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Allocate a new IO private instance */` |
|   1143 | 1235 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1143 | 1236 | `	if( pDev == 0 ){` |
|    ! 0 | 1237 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
|      - | 1241 | `	/* Initialize the structure */` |
|   1143 | 1242 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1243 | `	/* Open the target directory */` |
|   1143 | 1244 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1143 | 1245 | `	if( rc != PH7_OK ){` |
|      - | 1246 | `		/* IO error,return FALSE */` |
|    ! 0 | 1247 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 1248 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1249 | `	}else{` |
|      - | 1250 | `		/* Return the handle as a resource */` |
|   1143 | 1251 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1252 | `	}` |
|   1143 | 1253 | `	return PH7_OK;` |
|    574 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 1257 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 1258 | ` * Parameters` |
|      - | 1259 | ` *  $filename` |
|      - | 1260 | ` *   The filename being read.` |
|      - | 1261 | ` *  $use_include_path` |
|      - | 1262 | ` *   You can use the optional second parameter and set it to` |
|      - | 1263 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1264 | ` *  $context` |
|      - | 1265 | ` *   A context stream resource.` |
|      - | 1266 | ` * Return` |
|      - | 1267 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 1268 | ` */` |
|      - | 1269 | `/*` |
|      - | 1270 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 1271 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 1272 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 1273 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 1274 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 1275 | ` * trailing reason comes from.` |
|      - | 1276 | ` */` |
|      2 | 1277 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1278 | `{` |
|      3 | 1279 | `	int use_include  = FALSE;` |
|      - | 1280 | `	const ph7_io_stream *pStream;` |
|      - | 1281 | `	ph7_int64 n,nRead;` |
|      - | 1282 | `	const char *zFile;` |
|      - | 1283 | `	char zBuf[8192];` |
|      - | 1284 | `	void *pHandle;` |
|      - | 1285 | `	int rc,nLen;` |
|      3 | 1286 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1287 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1288 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1289 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1290 | `		return PH7_OK;` |
|      - | 1291 | `	}` |
|      - | 1292 | `	/* Extract the file path */` |
|      3 | 1293 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1294 | `	/* Point to the target IO stream device */` |
|      3 | 1295 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1296 | `	if( pStream == 0 ){` |
|    ! 0 | 1297 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1299 | `		return PH7_OK;` |
|      - | 1300 | `	}` |
|      3 | 1301 | `	if( nArg > 1 ){` |
|    ! 0 | 1302 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 1303 | `	}` |
|      - | 1304 | `	/* Try to open the file in read-only mode */` |
|      4 | 1305 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 1306 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1307 | `	if( pHandle == 0 ){` |
|    ! 0 | 1308 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1309 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1310 | `		return PH7_OK;` |
|      - | 1311 | `	}` |
|      - | 1312 | `	/* Perform the requested operation */` |
|      3 | 1313 | `	nRead = 0;` |
|      2 | 1314 | `	for(;;){` |
|      5 | 1315 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 1316 | `		if( n < 1 ){` |
|      - | 1317 | `			/* EOF or IO error,break immediately */` |
|      3 | 1318 | `			break;` |
|      - | 1319 | `		}` |
|      - | 1320 | `		/* Output data */` |
|      3 | 1321 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 1322 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 1323 | `			break;` |
|      - | 1324 | `		}` |
|      - | 1325 | `		/* Increment counter */` |
|      3 | 1326 | `		nRead += n;` |
|      1 | 1327 | `	}` |
|      - | 1328 | `	/* Close the stream */` |
|      3 | 1329 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1330 | `	/* Total number of bytes readen */` |
|      3 | 1331 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 1332 | `	return PH7_OK;` |
|      2 | 1333 | `}` |
|      - | 1334 | `/*` |
|      - | 1335 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 1336 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 1337 | ` *  Reads entire file into a string.` |
|      - | 1338 | ` * Parameters` |
|      - | 1339 | ` *  $filename` |
|      - | 1340 | ` *   The filename being read.` |
|      - | 1341 | ` *  $use_include_path` |
|      - | 1342 | ` *   You can use the optional second parameter and set it to` |
|      - | 1343 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1344 | ` *  $context` |
|      - | 1345 | ` *   A context stream resource.` |
|      - | 1346 | ` *  $offset` |
|      - | 1347 | ` *   The offset where the reading starts on the original stream.` |
|      - | 1348 | ` *  $maxlen` |
|      - | 1349 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 1350 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 1351 | ` * Return` |
|      - | 1352 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1353 | ` */` |
|   7272 | 1354 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1355 | `{` |
|      - | 1356 | `	const ph7_io_stream *pStream;` |
|      - | 1357 | `	ph7_int64 n,nRead,nMaxlen;` |
|   7277 | 1358 | `	int use_include  = FALSE;` |
|      - | 1359 | `	const char *zFile;` |
|      - | 1360 | `	char zBuf[8192];` |
|      - | 1361 | `	void *pHandle;` |
|      - | 1362 | `	int nLen;` |
|      - | 1363 |  |
|   7277 | 1364 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1365 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1366 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1367 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1368 | `		return PH7_OK;` |
|      - | 1369 | `	}` |
|      - | 1370 | `	/* Extract the file path */` |
|   7277 | 1371 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1372 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 1373 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 1374 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 1375 | `	 * the whole file. */` |
|   7277 | 1376 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 1377 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 1378 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1379 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 1380 | `		}` |
|     11 | 1381 | `	}` |
|      - | 1382 | `	/* Point to the target IO stream device */` |
|   7273 | 1383 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   7273 | 1384 | `	if( pStream == 0 ){` |
|    ! 0 | 1385 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1387 | `		return PH7_OK;` |
|      - | 1388 | `	}` |
|   7273 | 1389 | `	nMaxlen = -1;` |
|   7273 | 1390 | `	if( nArg > 1 ){` |
|     25 | 1391 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     12 | 1392 | `	}` |
|      - | 1393 | `	/* Try to open the file in read-only mode */` |
|   7273 | 1394 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   7273 | 1395 | `	if( pHandle == 0 ){` |
|      3 | 1396 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1397 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1398 | `		return PH7_OK;` |
|      - | 1399 | `	}` |
|   7271 | 1400 | `	if( nArg > 3 ){` |
|      - | 1401 | `		/* Extract the offset */` |
|     25 | 1402 | `		n = ph7_value_to_int64(apArg[3]);` |
|     25 | 1403 | `		if( n > 0 ){` |
|      7 | 1404 | `			if( pStream->xSeek ){` |
|      - | 1405 | `				/* Seek to the desired offset */` |
|      7 | 1406 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|      3 | 1407 | `			}` |
|      3 | 1408 | `		}` |
|     25 | 1409 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      - | 1410 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|      - | 1411 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|      - | 1412 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|     23 | 1413 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|     11 | 1414 | `		}` |
|     12 | 1415 | `	}` |
|      - | 1416 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|      - | 1417 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|      - | 1418 | `	 * through to the empty-string result below. */` |
|   7271 | 1419 | `	nRead = 0;` |
|  14525 | 1420 | `	while( nMaxlen != 0 ){` |
|      - | 1421 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 1422 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 1423 | `		 * otherwise overshoot and append past $length. */` |
|  14521 | 1424 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  14521 | 1425 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 1426 | `			nAsk = nMaxlen - nRead;` |
|      8 | 1427 | `		}` |
|  14521 | 1428 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  14521 | 1429 | `		if( n < 1 ){` |
|      - | 1430 | `			/* EOF or IO error,break immediately */` |
|   7253 | 1431 | `			break;` |
|      - | 1432 | `		}` |
|      - | 1433 | `		/* Append data */` |
|   7273 | 1434 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 1435 | `		/* Increment read counter */` |
|   7273 | 1436 | `		nRead += n;` |
|   7273 | 1437 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 1438 | `			/* Read limit reached */` |
|     15 | 1439 | `			break;` |
|      - | 1440 | `		}` |
|      5 | 1441 | `	}` |
|      - | 1442 | `	/* Close the stream */` |
|   7271 | 1443 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1444 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 1445 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 1446 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   7271 | 1447 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     18 | 1448 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 1449 | `	}` |
|   7271 | 1450 | `	return PH7_OK;` |
|   3641 | 1451 | `}` |
|      - | 1452 | `/*` |
|      - | 1453 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 1454 | ` *  Write a string to a file.` |
|      - | 1455 | ` * Parameters` |
|      - | 1456 | ` *  $filename` |
|      - | 1457 | ` *  Path to the file where to write the data.` |
|      - | 1458 | ` * $data` |
|      - | 1459 | ` *  The data to write(Must be a string).` |
|      - | 1460 | ` * $flags` |
|      - | 1461 | ` *  The value of flags can be any combination of the following` |
|      - | 1462 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 1463 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 1464 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 1465 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 1466 | ` * context` |
|      - | 1467 | ` *  A context stream resource.` |
|      - | 1468 | ` * Return` |
|      - | 1469 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 1470 | ` */` |
|  14086 | 1471 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1472 | `{` |
|  14091 | 1473 | `	int use_include  = FALSE;` |
|      - | 1474 | `	const ph7_io_stream *pStream;` |
|      - | 1475 | `	const char *zFile;` |
|      - | 1476 | `	const char *zData;` |
|      - | 1477 | `	int iOpenFlags;` |
|      - | 1478 | `	void *pHandle;` |
|      - | 1479 | `	int iFlags;` |
|      - | 1480 | `	int nLen;` |
|      - | 1481 |  |
|  14091 | 1482 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1483 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1484 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1486 | `		return PH7_OK;` |
|      - | 1487 | `	}` |
|      - | 1488 | `	/* Extract the file path */` |
|  14091 | 1489 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1490 | `	/* Point to the target IO stream device */` |
|  14091 | 1491 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  14091 | 1492 | `	if( pStream == 0 ){` |
|    ! 0 | 1493 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1495 | `		return PH7_OK;` |
|      - | 1496 | `	}` |
|      - | 1497 | `	/* Data to write */` |
|  14091 | 1498 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1499 | `	/* Try to open the file in read-write mode */` |
|  14091 | 1500 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 1501 | `	/* Extract the flags */` |
|  14091 | 1502 | `	iFlags = 0;` |
|  14091 | 1503 | `	if( nArg > 2 ){` |
|    ! 0 | 1504 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 1505 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 1506 | `			use_include = TRUE;` |
|    ! 0 | 1507 | `		}` |
|    ! 0 | 1508 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 1509 | `			/* If the file already exists, append the data to the file` |
|      - | 1510 | `			 * instead of overwriting it.` |
|      - | 1511 | `			 */` |
|    ! 0 | 1512 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 1513 | `			/* Append mode */` |
|    ! 0 | 1514 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 1515 | `		}` |
|    ! 0 | 1516 | `	}` |
|  21134 | 1517 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   7043 | 1518 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  14091 | 1519 | `	if( pHandle == 0 ){` |
|    ! 0 | 1520 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1521 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1522 | `		return PH7_OK;` |
|      - | 1523 | `	}` |
|  14091 | 1524 | `	if( nLen < 1 ){` |
|      - | 1525 | `		/* Empty data, file is created/truncated */` |
|     10 | 1526 | `		ph7_result_int64(pCtx,0);` |
|     10 | 1527 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     10 | 1528 | `		return PH7_OK;` |
|      - | 1529 | `	}` |
|  14083 | 1530 | `	if( pStream->xWrite ){` |
|      - | 1531 | `		ph7_int64 n;` |
|  14083 | 1532 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 1533 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 1534 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 1535 | `		}` |
|      - | 1536 | `		/* Perform the write operation */` |
|  14083 | 1537 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  14083 | 1538 | `		if( n < 0 ){` |
|      - | 1539 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 1540 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1541 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 1542 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 1543 | `			ph7_result_bool(pCtx,0);` |
|      1 | 1544 | `		}else{` |
|      - | 1545 | `			/* Total number of bytes written */` |
|  14083 | 1546 | `			ph7_result_int64(pCtx,n);` |
|      - | 1547 | `		}` |
|   7044 | 1548 | `	}else{` |
|      - | 1549 | `		/* Read-only stream */` |
|    ! 0 | 1550 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 1551 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 1552 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 1553 | `			);` |
|    ! 0 | 1554 | `		ph7_result_bool(pCtx,0);` |
|      - | 1555 | `	}` |
|      - | 1556 | `	/* Close the handle */` |
|  14083 | 1557 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  14083 | 1558 | `	return PH7_OK;` |
|   7048 | 1559 | `}` |
|      - | 1560 | `/*` |
|      - | 1561 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 1562 | ` *  Reads entire file into an array.` |
|      - | 1563 | ` * Parameters` |
|      - | 1564 | ` *  $filename` |
|      - | 1565 | ` *   The filename being read.` |
|      - | 1566 | ` *  $flags` |
|      - | 1567 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 1568 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 1569 | ` *       Search for the file in the include_path.` |
|      - | 1570 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 1571 | ` *       Do not add newline at the end of each array element` |
|      - | 1572 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 1573 | ` *       Skip empty lines` |
|      - | 1574 | ` *  $context` |
|      - | 1575 | ` *   A context stream resource.` |
|      - | 1576 | ` * Return` |
|      - | 1577 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1578 | ` */` |
|     42 | 1579 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1580 | `{` |
|      - | 1581 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 1582 | `	ph7_value *pArray,*pLine;` |
|      - | 1583 | `	const ph7_io_stream *pStream;` |
|     45 | 1584 | `	int use_include = 0;` |
|      - | 1585 | `	io_private *pDev;` |
|      - | 1586 | `	ph7_int64 n;` |
|      - | 1587 | `	int iFlags;` |
|      - | 1588 | `	int nLen;` |
|      - | 1589 |  |
|     45 | 1590 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1591 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1592 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1594 | `		return PH7_OK;` |
|      - | 1595 | `	}` |
|     45 | 1596 | `	iFlags = 0;` |
|     45 | 1597 | `	if( nArg > 1 ){` |
|      - | 1598 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|      - | 1599 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|      - | 1600 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|      - | 1601 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|      - | 1602 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|      - | 1603 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|      - | 1604 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|      - | 1605 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|      - | 1606 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|      - | 1607 | `		 * mask. */` |
|     33 | 1608 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     33 | 1609 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|     11 | 1610 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1611 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|      - | 1612 | `		}` |
|     23 | 1613 | `		iFlags = (int)nFlags;` |
|     11 | 1614 | `	}` |
|      - | 1615 | `	/* Extract the file path */` |
|     35 | 1616 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1617 | `	/* Point to the target IO stream device */` |
|     35 | 1618 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     35 | 1619 | `	if( pStream == 0 ){` |
|    ! 0 | 1620 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1621 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1622 | `		return PH7_OK;` |
|      - | 1623 | `	}` |
|      - | 1624 | `	/* Allocate a new IO private instance */` |
|     35 | 1625 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     35 | 1626 | `	if( pDev == 0 ){` |
|    ! 0 | 1627 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1628 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1629 | `		return PH7_OK;` |
|      - | 1630 | `	}` |
|      - | 1631 | `	/* Initialize the structure */` |
|     35 | 1632 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     35 | 1633 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 1634 | `		use_include = TRUE;` |
|      1 | 1635 | `	}` |
|      - | 1636 | `	/* Create the array and the working value */` |
|     35 | 1637 | `	pArray = ph7_context_new_array(pCtx);` |
|     35 | 1638 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     35 | 1639 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 1640 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Try to open the file in read-only mode */` |
|     35 | 1645 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     35 | 1646 | `	if( pDev->pHandle == 0 ){` |
|     10 | 1647 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 1648 | `		ph7_result_bool(pCtx,0);` |
|      - | 1649 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 1650 | `		 * as soon we return from this function.` |
|      - | 1651 | `		 */` |
|     10 | 1652 | `		return PH7_OK;` |
|      - | 1653 | `	}` |
|      - | 1654 | `	/* Perform the requested operation */` |
|     57 | 1655 | `	for(;;){` |
|      - | 1656 | `		/* Try to extract a line */` |
|    119 | 1657 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|    119 | 1658 | `		if( n < 1 ){` |
|      - | 1659 | `			/* EOF or IO error */` |
|     25 | 1660 | `			break;` |
|      - | 1661 | `		}` |
|      - | 1662 | `		/* Reset the cursor */` |
|     95 | 1663 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 1664 | `		/* Remove line ending if requested by the caller */` |
|     95 | 1665 | `		zPtr = zBuf;` |
|     95 | 1666 | `		zEnd = &zBuf[n];` |
|     95 | 1667 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 1668 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|      - | 1669 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|      - | 1670 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|      - | 1671 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|      - | 1672 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     51 | 1673 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     39 | 1674 | `				n--;` |
|     39 | 1675 | `				zEnd--;` |
|     39 | 1676 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|     13 | 1677 | `					n--;` |
|     13 | 1678 | `					zEnd--;` |
|      6 | 1679 | `				}` |
|     19 | 1680 | `			}` |
|     25 | 1681 | `		}` |
|     95 | 1682 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 1683 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|      - | 1684 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|      - | 1685 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|      - | 1686 | `			 * line (never zero-length, since the newline is still attached) was` |
|      - | 1687 | `			 * dropped too. Both are silent data loss from a read. */` |
|     31 | 1688 | `			if( zEnd <= zPtr ){` |
|      5 | 1689 | `				continue;` |
|      - | 1690 | `			}` |
|     13 | 1691 | `		}` |
|     91 | 1692 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 1693 | `		/* Insert line */` |
|     91 | 1694 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 1695 | `	}` |
|      - | 1696 | `	/* Close the stream */` |
|     25 | 1697 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 1698 | `	/* Release the io_private instance */` |
|     25 | 1699 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 1700 | `	/* Return the created array */` |
|     25 | 1701 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 1702 | `	return PH7_OK;` |
|     24 | 1703 | `}` |
|      - | 1704 | `/*` |
|      - | 1705 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 1706 | ` *  Makes a copy of the file source to dest.` |
|      - | 1707 | ` * Parameters` |
|      - | 1708 | ` *  $source` |
|      - | 1709 | ` *   Path to the source file.` |
|      - | 1710 | ` *  $dest` |
|      - | 1711 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 1712 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 1713 | ` *  $context` |
|      - | 1714 | ` *   A context stream resource.` |
|      - | 1715 | ` * Return` |
|      - | 1716 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1717 | ` */` |
|      4 | 1718 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1719 | `{` |
|      - | 1720 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 1721 | `	const char *zFile;` |
|      - | 1722 | `	char zBuf[8192];` |
|      - | 1723 | `	void *pIn,*pOut;` |
|      - | 1724 | `	ph7_int64 n;` |
|      - | 1725 | `	int nLen;` |
|      6 | 1726 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 1727 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1728 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 1729 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1730 | `		return PH7_OK;` |
|      - | 1731 | `	}` |
|      - | 1732 | `	/* Extract the source name */` |
|      6 | 1733 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1734 | `	/* Point to the target IO stream device */` |
|      6 | 1735 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 1736 | `	if( pSin == 0 ){` |
|    ! 0 | 1737 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1738 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 1742 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 1743 | `	if( pIn == 0 ){` |
|      3 | 1744 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1745 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract the destination name */` |
|      3 | 1749 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1750 | `	/* Point to the target IO stream device */` |
|      3 | 1751 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1752 | `	if( pSout == 0 ){` |
|    ! 0 | 1753 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1754 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1755 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1756 | `		return PH7_OK;` |
|      - | 1757 | `	}` |
|      3 | 1758 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 1759 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1760 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1761 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 1762 | `			);` |
|    ! 0 | 1763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1764 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 1768 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 1769 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1770 | `	if( pOut == 0 ){` |
|    ! 0 | 1771 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1772 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1773 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1774 | `		return PH7_OK;` |
|      - | 1775 | `	}` |
|      - | 1776 | `	/* Perform the requested operation */` |
|      2 | 1777 | `	for(;;){` |
|      - | 1778 | `		/* Read from source */` |
|      5 | 1779 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 1780 | `		if( n < 1 ){` |
|      - | 1781 | `			/* EOF or IO error,break immediately */` |
|      3 | 1782 | `			break;` |
|      - | 1783 | `		}` |
|      - | 1784 | `		/* Write to dest */` |
|      3 | 1785 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 1786 | `		if( n < 1 ){` |
|      - | 1787 | `			/* IO error,break immediately */` |
|    ! 0 | 1788 | `			break;` |
|      - | 1789 | `		}` |
|      1 | 1790 | `	}` |
|      - | 1791 | `	/* Close the streams */` |
|      3 | 1792 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 1793 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 1794 | `	/* Return TRUE */` |
|      3 | 1795 | `	ph7_result_bool(pCtx,1);` |
|      3 | 1796 | `	return PH7_OK;` |
|      4 | 1797 | `}` |
|      - | 1798 | `/*` |
|      - | 1799 | ` * array fstat(resource $handle)` |
|      - | 1800 | ` *  Gets information about a file using an open file pointer.` |
|      - | 1801 | ` * Parameters` |
|      - | 1802 | ` *  $handle` |
|      - | 1803 | ` *   The file pointer.` |
|      - | 1804 | ` * Return` |
|      - | 1805 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 1806 | ` */` |
|      2 | 1807 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1808 | `{` |
|      - | 1809 | `	ph7_value *pArray,*pValue;` |
|      - | 1810 | `	const ph7_io_stream *pStream;` |
|      - | 1811 | `	io_private *pDev;` |
|      3 | 1812 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1813 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1814 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1815 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract our private data */` |
|      3 | 1819 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1820 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1821 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1822 | `		/* Expecting an IO handle */` |
|    ! 0 | 1823 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1824 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1825 | `		return PH7_OK;` |
|      - | 1826 | `	}` |
|      - | 1827 | `	/* Point to the target IO stream device */` |
|      3 | 1828 | `	pStream = pDev->pStream;` |
|      3 | 1829 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 1830 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1831 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1832 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1833 | `			);` |
|    ! 0 | 1834 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1835 | `		return PH7_OK;` |
|      - | 1836 | `	}` |
|      - | 1837 | `	/* Create the array and the working value */` |
|      3 | 1838 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1839 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1840 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1841 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1842 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1843 | `		return PH7_OK;` |
|      - | 1844 | `	}` |
|      - | 1845 | `	/* Perform the requested operation */` |
|      3 | 1846 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 1847 | `	/* Return the freshly created array */` |
|      3 | 1848 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1849 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 1850 | `	 * released automatically as soon we return from this function.` |
|      - | 1851 | `	 */` |
|      3 | 1852 | `	return PH7_OK;` |
|      2 | 1853 | `}` |
|      - | 1854 | `/*` |
|      - | 1855 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 1856 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 1857 | ` * Parameters` |
|      - | 1858 | ` *  $handle` |
|      - | 1859 | ` *   The file pointer.` |
|      - | 1860 | ` *  $string` |
|      - | 1861 | ` *   The string that is to be written.` |
|      - | 1862 | ` *  $length` |
|      - | 1863 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 1864 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 1865 | ` * Return` |
|      - | 1866 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 1867 | ` */` |
|     72 | 1868 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1869 | `{` |
|      - | 1870 | `	const ph7_io_stream *pStream;` |
|      - | 1871 | `	const char *zString;` |
|      - | 1872 | `	io_private *pDev;` |
|      - | 1873 | `	int nLen,n;` |
|     74 | 1874 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1875 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1876 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1878 | `		return PH7_OK;` |
|      - | 1879 | `	}` |
|      - | 1880 | `	/* Extract our private data */` |
|     74 | 1881 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1882 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     74 | 1883 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1884 | `		/* Expecting an IO handle */` |
|    ! 0 | 1885 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1887 | `		return PH7_OK;` |
|      - | 1888 | `	}` |
|      - | 1889 | `	/* Point to the target IO stream device */` |
|     74 | 1890 | `	pStream = pDev->pStream;` |
|     74 | 1891 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 1892 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1893 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1894 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1895 | `			);` |
|    ! 0 | 1896 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1897 | `		return PH7_OK;` |
|      - | 1898 | `	}` |
|      - | 1899 | `	/* Extract the data to write */` |
|     74 | 1900 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     74 | 1901 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1902 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|      - | 1903 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|      - | 1904 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|      - | 1905 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|      - | 1906 | `		 * and write the whole string). */` |
|     13 | 1907 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|     13 | 1908 | `		if( nMax < 0 ){` |
|      3 | 1909 | `			nLen = 0;` |
|     12 | 1910 | `		}else if( nMax < (sxi64)nLen ){` |
|      5 | 1911 | `			nLen = (int)nMax;` |
|      2 | 1912 | `		}` |
|      6 | 1913 | `	}` |
|     74 | 1914 | `	if( nLen < 1 ){` |
|      - | 1915 | `		/* Nothing to write */` |
|      5 | 1916 | `		ph7_result_int(pCtx,0);` |
|      5 | 1917 | `		return PH7_OK;` |
|      - | 1918 | `	}` |
|      - | 1919 | `	/* Perform the requested operation */` |
|     70 | 1920 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     70 | 1921 | `	if( n <  0 ){` |
|      - | 1922 | `		/* IO error,return FALSE */` |
|    ! 0 | 1923 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1924 | `	}else{` |
|      - | 1925 | `		/* #Bytes written */` |
|     70 | 1926 | `		ph7_result_int(pCtx,n);` |
|      - | 1927 | `	}` |
|     70 | 1928 | `	return PH7_OK;` |
|     38 | 1929 | `}` |
|      - | 1930 | `/*` |
|      - | 1931 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|      - | 1932 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|      - | 1933 | ` * script can tell contention (1) from a plain failure (0).` |
|      - | 1934 | ` */` |
|     30 | 1935 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|      1 | 1936 | `{` |
|      - | 1937 | `	ph7_value sVal;` |
|     31 | 1938 | `	if( nArg < 3 ){` |
|     19 | 1939 | `		return;` |
|      - | 1940 | `	}` |
|     13 | 1941 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|     13 | 1942 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|     13 | 1943 | `	PH7_MemObjRelease(&sVal);` |
|     16 | 1944 | `}` |
|      - | 1945 | `/*` |
|      - | 1946 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|      - | 1947 | ` *  Portable advisory file locking.` |
|      - | 1948 | ` * Parameters` |
|      - | 1949 | ` *  $handle` |
|      - | 1950 | ` *   The file pointer.` |
|      - | 1951 | ` *  $operation` |
|      - | 1952 | ` *   operation is one of the following:` |
|      - | 1953 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 1954 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 1955 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 1956 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|      - | 1957 | ` *  &$would_block` |
|      - | 1958 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|      - | 1959 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|      - | 1960 | ` * Return` |
|      - | 1961 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 1962 | ` */` |
|     38 | 1963 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1964 | `{` |
|      - | 1965 | `	const ph7_io_stream *pStream;` |
|      - | 1966 | `	io_private *pDev;` |
|      - | 1967 | `	int nLock;` |
|      - | 1968 | `	int rc;` |
|     39 | 1969 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1970 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1971 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1972 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1973 | `		return PH7_OK;` |
|      - | 1974 | `	}` |
|      - | 1975 | `	/* Extract our private data */` |
|     39 | 1976 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1977 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     39 | 1978 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1979 | `		/*Expecting an IO handle */` |
|    ! 0 | 1980 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1982 | `		return PH7_OK;` |
|      - | 1983 | `	}` |
|      - | 1984 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|      - | 1985 | `	 * support is considered: the low two bits select the action (its bison` |
|      - | 1986 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|      - | 1987 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|     39 | 1988 | `	nLock = ph7_value_to_int(apArg[1]);` |
|     39 | 1989 | `	if( (nLock & 3) == 0 ){` |
|      9 | 1990 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1991 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|      - | 1992 | `	}` |
|      - | 1993 | `	/* Point to the target IO stream device */` |
|     31 | 1994 | `	pStream = pDev->pStream;` |
|     31 | 1995 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|      - | 1996 | `		/* php returns FALSE silently when the stream does not support locking` |
|      - | 1997 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|      7 | 1998 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|      7 | 1999 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2000 | `		return PH7_OK;` |
|      - | 2001 | `	}` |
|      - | 2002 | `	/*` |
|      - | 2003 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|      - | 2004 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|      - | 2005 | `	 */` |
|      - | 2006 | `	{` |
|     25 | 2007 | `		int iOp = nLock & 3;` |
|     25 | 2008 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|     25 | 2009 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      9 | 2010 | `			nLock = -1;` |
|     21 | 2011 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|     11 | 2012 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|      6 | 2013 | `		}else{` |
|      7 | 2014 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|      - | 2015 | `		}` |
|      - | 2016 | `	}` |
|      - | 2017 | `	/* Lock operation */` |
|     25 | 2018 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 2019 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|      - | 2020 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|     25 | 2021 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|      - | 2022 | `	/* IO result */` |
|     25 | 2023 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     25 | 2024 | `	return PH7_OK;` |
|     20 | 2025 | `}` |
|      - | 2026 | `/*` |
|      - | 2027 | ` * int fpassthru(resource $handle)` |
|      - | 2028 | ` *  Output all remaining data on a file pointer.` |
|      - | 2029 | ` * Parameters` |
|      - | 2030 | ` *  $handle` |
|      - | 2031 | ` *   The file pointer.` |
|      - | 2032 | ` * Return` |
|      - | 2033 | ` *  Total number of characters read from handle and passed through` |
|      - | 2034 | ` *  to the output on success or FALSE on failure.` |
|      - | 2035 | ` */` |
|      2 | 2036 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2037 | `{` |
|      - | 2038 | `	const ph7_io_stream *pStream;` |
|      - | 2039 | `	io_private *pDev;` |
|      - | 2040 | `	ph7_int64 n,nRead;` |
|      - | 2041 | `	char zBuf[8192];` |
|      - | 2042 | `	int rc;` |
|      3 | 2043 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2044 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2045 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2047 | `		return PH7_OK;` |
|      - | 2048 | `	}` |
|      - | 2049 | `	/* Extract our private data */` |
|      3 | 2050 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2051 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2052 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2053 | `		/*Expecting an IO handle */` |
|    ! 0 | 2054 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2055 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2056 | `		return PH7_OK;` |
|      - | 2057 | `	}` |
|      - | 2058 | `	/* Point to the target IO stream device */` |
|      3 | 2059 | `	pStream = pDev->pStream;` |
|      3 | 2060 | `	if( pStream == 0  ){` |
|    ! 0 | 2061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2062 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2063 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2064 | `			);` |
|    ! 0 | 2065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2066 | `		return PH7_OK;` |
|      - | 2067 | `	}` |
|      - | 2068 | `	/* Perform the requested operation */` |
|      3 | 2069 | `	nRead = 0;` |
|      2 | 2070 | `	for(;;){` |
|      5 | 2071 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 2072 | `		if( n < 1 ){` |
|      - | 2073 | `			/* Error or EOF */` |
|      3 | 2074 | `			break;` |
|      - | 2075 | `		}` |
|      - | 2076 | `		/* Increment the read counter */` |
|      3 | 2077 | `		nRead += n;` |
|      - | 2078 | `		/* Output data */` |
|      3 | 2079 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 2080 | `		if( rc == PH7_ABORT ){` |
|      - | 2081 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 2082 | `			break;` |
|      - | 2083 | `		}` |
|      1 | 2084 | `	}` |
|      - | 2085 | `	/* Total number of bytes readen */` |
|      3 | 2086 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 2087 | `	return PH7_OK;` |
|      2 | 2088 | `}` |
|      - | 2089 | `/* CSV reader/writer private data */` |
|      - | 2090 | `struct csv_data` |
|      - | 2091 | `{` |
|      - | 2092 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 2093 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 2094 | `	io_private *pDev; /* Open stream handle */` |
|      - | 2095 | `	int iCount;       /* Counter */` |
|      - | 2096 | `};` |
|      - | 2097 | `/*` |
|      - | 2098 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 2099 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 2100 | ` * associated data.` |
|      - | 2101 | ` */` |
|     10 | 2102 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 2103 | `{` |
|     11 | 2104 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 2105 | `	const char *zData;` |
|      - | 2106 | `	int nLen,c2;` |
|      - | 2107 | `	sxu32 n;` |
|      - | 2108 | `	/* Point to the raw data */` |
|     11 | 2109 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|     11 | 2110 | `	if( nLen < 1 ){` |
|      - | 2111 | `		/* Nothing to write */` |
|    ! 0 | 2112 | `		return PH7_OK;` |
|      - | 2113 | `	}` |
|     11 | 2114 | `	if( pData->iCount > 0 ){` |
|      - | 2115 | `		/* Write the delimiter */` |
|      7 | 2116 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      3 | 2117 | `	}` |
|     11 | 2118 | `	n = 1;` |
|     11 | 2119 | `	c2 = 0;` |
|     16 | 2120 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|     10 | 2121 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 2122 | `			c2 = 1;` |
|    ! 0 | 2123 | `			if( n == 0 ){` |
|    ! 0 | 2124 | `				c2 = 2;` |
|    ! 0 | 2125 | `			}` |
|      - | 2126 | `			/* Write the enclosure */` |
|    ! 0 | 2127 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2128 | `			if( c2 > 1 ){` |
|    ! 0 | 2129 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2130 | `			}` |
|    ! 0 | 2131 | `	}` |
|      - | 2132 | `	/* Write the data */` |
|     11 | 2133 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 2134 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 2135 | `		return PH7_ABORT;` |
|      - | 2136 | `	}` |
|     11 | 2137 | `	if( c2 > 0 ){` |
|      - | 2138 | `		/* Write the enclosure */` |
|    ! 0 | 2139 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2140 | `		if( c2 > 1 ){` |
|    ! 0 | 2141 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2142 | `		}` |
|    ! 0 | 2143 | `	}` |
|     11 | 2144 | `	pData->iCount++;` |
|     11 | 2145 | `	return PH7_OK;` |
|      6 | 2146 | `}` |
|      - | 2147 | `/*` |
|      - | 2148 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 2149 | ` *  Format line as CSV and write to file pointer.` |
|      - | 2150 | ` * Parameters` |
|      - | 2151 | ` *  $handle` |
|      - | 2152 | ` *   Open file handle.` |
|      - | 2153 | ` * $fields` |
|      - | 2154 | ` *   An array of values.` |
|      - | 2155 | ` * $delimiter` |
|      - | 2156 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 2157 | ` * $enclosure` |
|      - | 2158 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 2159 | ` */` |
|     10 | 2160 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2161 | `{` |
|      - | 2162 | `	const ph7_io_stream *pStream;` |
|      - | 2163 | `	struct csv_data sCsv;` |
|      - | 2164 | `	io_private *pDev;` |
|      - | 2165 | `	char *zEol;` |
|      - | 2166 | `	int eolen;` |
|     11 | 2167 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 2168 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2169 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 2170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Extract our private data */` |
|     11 | 2174 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2175 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 | 2176 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2177 | `		/*Expecting an IO handle */` |
|    ! 0 | 2178 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2180 | `		return PH7_OK;` |
|      - | 2181 | `	}` |
|      - | 2182 | `	/* Point to the target IO stream device */` |
|     11 | 2183 | `	pStream = pDev->pStream;` |
|     11 | 2184 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2185 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2186 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2187 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2188 | `			);` |
|    ! 0 | 2189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2190 | `		return PH7_OK;` |
|      - | 2191 | `	}` |
|      - | 2192 | `	/* Set default csv separator */` |
|     11 | 2193 | `	sCsv.delimiter = ',';` |
|     11 | 2194 | `	sCsv.enclosure = '"';` |
|     11 | 2195 | `	sCsv.pDev = pDev;` |
|     11 | 2196 | `	sCsv.iCount = 0;` |
|     11 | 2197 | `	if( nArg > 2 ){` |
|     11 | 2198 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     11 | 2199 | `		if( rc != PH7_OK ){` |
|      3 | 2200 | `			return rc;` |
|      - | 2201 | `		}` |
|      9 | 2202 | `		if( nArg > 3 ){` |
|      9 | 2203 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|      9 | 2204 | `			if( rc != PH7_OK ){` |
|      3 | 2205 | `				return rc;` |
|      - | 2206 | `			}` |
|      7 | 2207 | `			if( nArg > 4 ){` |
|      - | 2208 | `				/* The writer does not model $escape (the CSV-writer slice);` |
|      - | 2209 | `				 * validate it like php so the loud path matches. */` |
|      - | 2210 | `				int iEscape;` |
|      7 | 2211 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&iEscape);` |
|      7 | 2212 | `				if( rc != PH7_OK ){` |
|      3 | 2213 | `					return rc;` |
|      - | 2214 | `				}` |
|      2 | 2215 | `			}` |
|      2 | 2216 | `		}` |
|      2 | 2217 | `	}` |
|      - | 2218 | `	/* Iterate throw array entries and write csv data */` |
|      5 | 2219 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 2220 | `	/* Write a line ending */` |
|      - | 2221 | `#ifdef __WINNT__` |
|      1 | 2222 | `	zEol = "\r\n";` |
|      1 | 2223 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 2224 | `#else` |
|      - | 2225 | `	/* Assume UNIX LF */` |
|      4 | 2226 | `	zEol = "\n";` |
|      4 | 2227 | `	eolen = (int)sizeof(char);` |
|      - | 2228 | `#endif` |
|      5 | 2229 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      5 | 2230 | `	return PH7_OK;` |
|      6 | 2231 | `}` |
|      - | 2232 | `/*` |
|      - | 2233 | ` * fprintf,vfprintf private data.` |
|      - | 2234 | ` * An instance of the following structure is passed to the formatted` |
|      - | 2235 | ` * input consumer callback defined below.` |
|      - | 2236 | ` */` |
|      - | 2237 | `typedef struct fprintf_data fprintf_data;` |
|      - | 2238 | `struct fprintf_data` |
|      - | 2239 | `{` |
|      - | 2240 | `	io_private *pIO;        /* IO stream */` |
|      - | 2241 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 2242 | `};` |
|      - | 2243 | `/*` |
|      - | 2244 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 2245 | ` */` |
|     36 | 2246 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      2 | 2247 | `{` |
|     38 | 2248 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 2249 | `	ph7_int64 n;` |
|      - | 2250 | `	/* Write the formatted data */` |
|     38 | 2251 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     38 | 2252 | `	if( n < 1 ){` |
|    ! 0 | 2253 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 2254 | `		/* IO error,abort immediately */` |
|    ! 0 | 2255 | `		return SXERR_ABORT;` |
|      - | 2256 | `	}` |
|      - | 2257 | `	/* Increment counter */` |
|     38 | 2258 | `	pFdata->nCount += n;` |
|     38 | 2259 | `	return PH7_OK;` |
|     20 | 2260 | `}` |
|      - | 2261 | `/*` |
|      - | 2262 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 2263 | ` *  Write a formatted string to a stream.` |
|      - | 2264 | ` * Parameters` |
|      - | 2265 | ` *  $handle` |
|      - | 2266 | ` *   The file pointer.` |
|      - | 2267 | ` *  $format` |
|      - | 2268 | ` *   String format (see sprintf()).` |
|      - | 2269 | ` * Return` |
|      - | 2270 | ` *  The length of the written string.` |
|      - | 2271 | ` */` |
|     20 | 2272 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2273 | `{` |
|      - | 2274 | `	fprintf_data sFdata;` |
|      - | 2275 | `	const char *zFormat;` |
|      - | 2276 | `	io_private *pDev;` |
|      - | 2277 | `	int nLen;` |
|     22 | 2278 | `	if( nArg < 2 ){` |
|    ! 0 | 2279 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2280 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2281 | `		return PH7_OK;` |
|      - | 2282 | `	}` |
|      - | 2283 | `	{` |
|      - | 2284 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     22 | 2285 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     22 | 2286 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 2287 | `			return rcs;` |
|      - | 2288 | `		}` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Extract our private data */` |
|     22 | 2291 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2292 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     22 | 2293 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2294 | `		/*Expecting an IO handle */` |
|    ! 0 | 2295 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2296 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2297 | `		return PH7_OK;` |
|      - | 2298 | `	}` |
|      - | 2299 | `	/* Point to the target IO stream device */` |
|     22 | 2300 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2303 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2304 | `			);` |
|    ! 0 | 2305 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2306 | `		return PH7_OK;` |
|      - | 2307 | `	}` |
|      - | 2308 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 2309 | `	{` |
|     22 | 2310 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     22 | 2311 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2312 | `			return rcf;` |
|      - | 2313 | `		}` |
|      - | 2314 | `	}` |
|      - | 2315 | `	/* Extract the string format (scalars/null coerce). */` |
|     22 | 2316 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     22 | 2317 | `	if( nLen < 1 ){` |
|      - | 2318 | `		/* Empty string,return zero */` |
|    ! 0 | 2319 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2320 | `		return PH7_OK;` |
|      - | 2321 | `	}` |
|      - | 2322 | `	{` |
|      - | 2323 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 2324 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 2325 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     22 | 2326 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     22 | 2327 | `		if( rcv != PH7_OK ){` |
|      3 | 2328 | `			return rcv;` |
|      - | 2329 | `		}` |
|      - | 2330 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 2331 | `		 * before any output; propagate the throw status verbatim. */` |
|     20 | 2332 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     20 | 2333 | `		if( rcv != PH7_OK ){` |
|      5 | 2334 | `			return rcv;` |
|      - | 2335 | `		}` |
|      - | 2336 | `	}` |
|      - | 2337 | `	/* Prepare our private data */` |
|     16 | 2338 | `	sFdata.nCount = 0;` |
|     16 | 2339 | `	sFdata.pIO = pDev;` |
|      - | 2340 | `	/* Format the string */` |
|      - | 2341 | `	{` |
|     16 | 2342 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 2343 | `	/* Return total number of bytes written */` |
|     16 | 2344 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      - | 2345 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 2346 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|     16 | 2347 | `	if( rcv != SXRET_OK ){` |
|      3 | 2348 | `		pCtx->nThrowRc = rcv;` |
|      3 | 2349 | `		return rcv;` |
|      - | 2350 | `	}` |
|      - | 2351 | `	}` |
|     13 | 2352 | `	return PH7_OK;` |
|     12 | 2353 | `}` |
|      - | 2354 | `/*` |
|      - | 2355 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 2356 | ` *  Write a formatted string to a stream.` |
|      - | 2357 | ` * Parameters` |
|      - | 2358 | ` *  $handle` |
|      - | 2359 | ` *   The file pointer.` |
|      - | 2360 | ` *  $format` |
|      - | 2361 | ` *   String format (see sprintf()).` |
|      - | 2362 | ` * $args` |
|      - | 2363 | ` *   User arguments.` |
|      - | 2364 | ` * Return` |
|      - | 2365 | ` *  The length of the written string.` |
|      - | 2366 | ` */` |
|      8 | 2367 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2368 | `{` |
|      - | 2369 | `	fprintf_data sFdata;` |
|      - | 2370 | `	const char *zFormat;` |
|      - | 2371 | `	ph7_hashmap *pMap;` |
|      - | 2372 | `	io_private *pDev;` |
|      - | 2373 | `	SySet sArg;` |
|      - | 2374 | `	int n,nLen;` |
|     10 | 2375 | `	if( nArg < 3 ){` |
|    ! 0 | 2376 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2377 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2378 | `		return PH7_OK;` |
|      - | 2379 | `	}` |
|      - | 2380 | `	{` |
|      - | 2381 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     10 | 2382 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     10 | 2383 | `		if( rcs != PH7_OK ){` |
|      3 | 2384 | `			return rcs;` |
|      - | 2385 | `		}` |
|      - | 2386 | `	}` |
|      - | 2387 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 2388 | `	{` |
|      8 | 2389 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      8 | 2390 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2391 | `			return rcf;` |
|      - | 2392 | `		}` |
|      - | 2393 | `	}` |
|      8 | 2394 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 2395 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 2396 | `		char zBuf[64];` |
|    ! 0 | 2397 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2398 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 2399 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 2400 | `	}` |
|      - | 2401 | `	/* Extract our private data */` |
|      8 | 2402 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2403 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      8 | 2404 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2405 | `		/*Expecting an IO handle */` |
|    ! 0 | 2406 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2407 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2408 | `		return PH7_OK;` |
|      - | 2409 | `	}` |
|      - | 2410 | `	/* Point to the target IO stream device */` |
|      8 | 2411 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2412 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2413 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2414 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2415 | `			);` |
|    ! 0 | 2416 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2417 | `		return PH7_OK;` |
|      - | 2418 | `	}` |
|      - | 2419 | `	/* Extract the string format */` |
|      8 | 2420 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      8 | 2421 | `	if( nLen < 1 ){` |
|      - | 2422 | `		/* Empty string,return zero */` |
|    ! 0 | 2423 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2424 | `		return PH7_OK;` |
|      - | 2425 | `	}` |
|      - | 2426 | `	/* Point to hashmap */` |
|      8 | 2427 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 2428 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 2429 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 2430 | `	{` |
|      8 | 2431 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      8 | 2432 | `		if( rcc != PH7_OK ){` |
|      3 | 2433 | `			return rcc;` |
|      - | 2434 | `		}` |
|      - | 2435 | `	}` |
|      - | 2436 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 2437 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 2438 | `	{` |
|      6 | 2439 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      6 | 2440 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 2441 | `			return rcv;` |
|      - | 2442 | `		}` |
|      - | 2443 | `	}` |
|      - | 2444 | `	/* Extract arguments from the hashmap */` |
|      6 | 2445 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 2446 | `	/* Prepare our private data */` |
|      6 | 2447 | `	sFdata.nCount = 0;` |
|      6 | 2448 | `	sFdata.pIO = pDev;` |
|      - | 2449 | `	/* Format the string */` |
|      - | 2450 | `	{` |
|      6 | 2451 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 2452 | `	/* Return total number of bytes written*/` |
|      6 | 2453 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      6 | 2454 | `	SySetRelease(&sArg);` |
|      - | 2455 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 2456 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|      6 | 2457 | `	if( rcv != SXRET_OK ){` |
|      3 | 2458 | `		pCtx->nThrowRc = rcv;` |
|      3 | 2459 | `		return rcv;` |
|      - | 2460 | `	}` |
|      - | 2461 | `	}` |
|      3 | 2462 | `	return PH7_OK;` |
|      6 | 2463 | `}` |
|      - | 2464 | `/*` |
|      - | 2465 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 2466 | ` * According to the PHP reference manual:` |
|      - | 2467 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 2468 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 2469 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 2470 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 2471 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 2472 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 2473 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 2474 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 2475 | ` *         exist, attempt to create it.` |
|      - | 2476 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 2477 | ` *          not exist, attempt to create it.` |
|      - | 2478 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 2479 | ` *         already exists,` |
|      - | 2480 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 2481 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 2482 | ` *         the underlying open(2) system call.` |
|      - | 2483 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 2484 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 2485 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 2486 | ` *          is positioned on the beginning of the file.` |
|      - | 2487 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 2488 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 2489 | ` *          be used after the lock is requested).` |
|      - | 2490 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 2491 | ` */` |
|    304 | 2492 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 2493 | `{` |
|    309 | 2494 | `	const char *zEnd = &zMode[nLen];` |
|    309 | 2495 | `	int iFlag = 0;` |
|      - | 2496 | `	int c;` |
|    309 | 2497 | `	if( nLen < 1 ){` |
|      - | 2498 | `		/* Open in a read-only mode */` |
|    ! 0 | 2499 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 2500 | `	}` |
|    309 | 2501 | `	c = zMode[0];` |
|    309 | 2502 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 2503 | `		/* Read-only access */` |
|    125 | 2504 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|    125 | 2505 | `		zMode++; /* Advance */` |
|    125 | 2506 | `		if( zMode < zEnd ){` |
|     33 | 2507 | `			c = zMode[0];` |
|     33 | 2508 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 2509 | `				/* Read+Write access */` |
|     33 | 2510 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|     16 | 2511 | `			}` |
|     21 | 2512 | `		}` |
|    248 | 2513 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 2514 | `		/* Overwrite mode.` |
|      - | 2515 | `		 * If the file does not exists,try to create it` |
|      - | 2516 | `		 */` |
|     47 | 2517 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     47 | 2518 | `		zMode++; /* Advance */` |
|     47 | 2519 | `		if( zMode < zEnd ){` |
|     12 | 2520 | `			c = zMode[0];` |
|     12 | 2521 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2522 | `				/* Read+Write access */` |
|     12 | 2523 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|     12 | 2524 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      5 | 2525 | `			}` |
|      8 | 2526 | `		}` |
|    165 | 2527 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 2528 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 2529 | `		 * Create the file if it does not exists.` |
|      - | 2530 | `		 */` |
|    ! 0 | 2531 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2532 | `		zMode++; /* Advance */` |
|    ! 0 | 2533 | `		if( zMode < zEnd ){` |
|    ! 0 | 2534 | `			c = zMode[0];` |
|    ! 0 | 2535 | `			if( c == '+' ){` |
|      - | 2536 | `				/* Read-Write access */` |
|    ! 0 | 2537 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2538 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2539 | `			}` |
|    ! 0 | 2540 | `		}` |
|    143 | 2541 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 2542 | `		/* Exclusive access.` |
|      - | 2543 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 2544 | `		 * Otherwise create a new file.` |
|      - | 2545 | `		 */` |
|    143 | 2546 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    143 | 2547 | `		zMode++; /* Advance */` |
|    143 | 2548 | `		if( zMode < zEnd ){` |
|    ! 0 | 2549 | `			c = zMode[0];` |
|    ! 0 | 2550 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2551 | `				/* Read-Write access */` |
|    ! 0 | 2552 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2553 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2554 | `			}` |
|      3 | 2555 | `		}` |
|     70 | 2556 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 2557 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 2558 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2559 | `		zMode++; /* Advance */` |
|    ! 0 | 2560 | `		if( zMode < zEnd ){` |
|    ! 0 | 2561 | `			c = zMode[0];` |
|    ! 0 | 2562 | `			if( c == '+' ){` |
|      - | 2563 | `				/* Read-Write access */` |
|    ! 0 | 2564 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2565 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2566 | `			}` |
|    ! 0 | 2567 | `		}` |
|    ! 0 | 2568 | `	}else{` |
|      - | 2569 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 2570 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 2571 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 2572 | `	}` |
|    351 | 2573 | `	while( zMode < zEnd ){` |
|     44 | 2574 | `		c = zMode[0];` |
|     44 | 2575 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 2576 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2577 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     44 | 2578 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 2579 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 2580 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2581 | `		}` |
|     44 | 2582 | `		zMode++;` |
|      2 | 2583 | `	}` |
|    309 | 2584 | `	return iFlag;` |
|    157 | 2585 | `}` |
|      - | 2586 | `/*` |
|      - | 2587 | ` * Initialize the IO private structure.` |
|      - | 2588 | ` */` |
|   6140 | 2589 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 2590 | `{` |
|   6145 | 2591 | `	pOut->pStream = pStream;` |
|   6145 | 2592 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   6145 | 2593 | `	pOut->nOfft = 0;` |
|      - | 2594 | `	/* Set the magic number */` |
|   6145 | 2595 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   6145 | 2596 | `}` |
|      - | 2597 | `/*` |
|      - | 2598 | ` * Release the IO private structure.` |
|      - | 2599 | ` */` |
|     24 | 2600 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 2601 | `{` |
|     25 | 2602 | `	SyBlobRelease(&pDev->sBuffer);` |
|     25 | 2603 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 2604 | `	/* Release the whole structure */` |
|     25 | 2605 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     25 | 2606 | `}` |
|      - | 2607 | `/*` |
|      - | 2608 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 2609 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 2610 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 2611 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 2612 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 2613 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 2614 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 2615 | ` */` |
|   6026 | 2616 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 2617 | `{` |
|   6031 | 2618 | `	SyBlobRelease(&pDev->sBuffer);` |
|   6031 | 2619 | `	pDev->pHandle = 0;` |
|   6031 | 2620 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   6031 | 2621 | `}` |
|      - | 2622 | `/*` |
|      - | 2623 | ` * Reset the IO private structure.` |
|      - | 2624 | ` */` |
|     78 | 2625 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 2626 | `{` |
|     80 | 2627 | `	SyBlobReset(&pDev->sBuffer);` |
|     80 | 2628 | `	pDev->nOfft = 0;` |
|     80 | 2629 | `}` |
|      - | 2630 | `/* Forward declaration */` |
|      - | 2631 |  |
|      - | 2632 | `/*` |
|      - | 2633 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 2634 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 2635 | ` * Parameters` |
|      - | 2636 | ` *  $filename` |
|      - | 2637 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 2638 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 2639 | ` *   then a regular file is assumed.` |
|      - | 2640 | ` *  $mode` |
|      - | 2641 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 2642 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 2643 | ` *   modes.` |
|      - | 2644 | ` *  $use_include_path` |
|      - | 2645 | ` *   You can use the optional second parameter and set it to` |
|      - | 2646 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2647 | ` *  $context` |
|      - | 2648 | ` *   A context stream resource.` |
|      - | 2649 | ` * Return` |
|      - | 2650 | ` *  File handle on success or FALSE on failure.` |
|      - | 2651 | ` */` |
|      - | 2652 | `/*` |
|      - | 2653 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 2654 | ` *                                  int $offset = -1)` |
|      - | 2655 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 2656 | ` */` |
|     40 | 2657 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2658 | `{` |
|      - | 2659 | `	const ph7_io_stream *pStream;` |
|      - | 2660 | `	io_private *pDev;` |
|     41 | 2661 | `	ph7_int64 nMax = -1;` |
|      - | 2662 | `	char zBuf[4096];` |
|      - | 2663 | `	ph7_int64 nRead;` |
|     41 | 2664 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2665 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2666 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2667 | `		return PH7_OK;` |
|      - | 2668 | `	}` |
|     41 | 2669 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     41 | 2670 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2671 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2672 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2673 | `		return PH7_OK;` |
|      - | 2674 | `	}` |
|     41 | 2675 | `	pStream = pDev->pStream;` |
|     41 | 2676 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 2677 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2678 | `		return PH7_OK;` |
|      - | 2679 | `	}` |
|     41 | 2680 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2681 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 2682 | `		 * default) means "read until EOF". */` |
|      9 | 2683 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      9 | 2684 | `		if( nMax < -1 ){` |
|      3 | 2685 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2686 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 2687 | `		}` |
|      3 | 2688 | `	}` |
|     39 | 2689 | `	if( nArg > 2 ){` |
|      5 | 2690 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 2691 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 2692 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 2693 | `		}` |
|      2 | 2694 | `	}` |
|     39 | 2695 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     72 | 2696 | `	while( nMax != 0 ){` |
|     70 | 2697 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     70 | 2698 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 2699 | `			nAsk = nMax;` |
|      1 | 2700 | `		}` |
|     70 | 2701 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     70 | 2702 | `		if( nRead < 1 ){` |
|     37 | 2703 | `			break;` |
|      - | 2704 | `		}` |
|     34 | 2705 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     34 | 2706 | `		if( nMax > 0 ){` |
|      3 | 2707 | `			nMax -= nRead;` |
|      1 | 2708 | `		}` |
|      1 | 2709 | `	}` |
|     39 | 2710 | `	return PH7_OK;` |
|     21 | 2711 | `}` |
|      - | 2712 | `/*` |
|      - | 2713 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 2714 | ` */` |
|      4 | 2715 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2716 | `{` |
|      - | 2717 | `	ph7_value *pArr,*pV;` |
|      - | 2718 | `	ph7_io_stream **apDev;` |
|      - | 2719 | `	sxu32 n;` |
|      2 | 2720 | `	SXUNUSED(nArg);` |
|      2 | 2721 | `	SXUNUSED(apArg);` |
|      6 | 2722 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 2723 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 2724 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2725 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2726 | `		return PH7_OK;` |
|      - | 2727 | `	}` |
|      6 | 2728 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 2729 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 2730 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 2731 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 2732 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 2733 | `	}` |
|      6 | 2734 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 2735 | `	return PH7_OK;` |
|      4 | 2736 | `}` |
|      - | 2737 | `/*` |
|      - | 2738 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 2739 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 2740 | ` */` |
|      2 | 2741 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2742 | `{` |
|      - | 2743 | `	io_private *pDev;` |
|      - | 2744 | `	ph7_value *pArr,*pV;` |
|      3 | 2745 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2746 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2747 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2748 | `		return PH7_OK;` |
|      - | 2749 | `	}` |
|      3 | 2750 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 2751 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2752 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2754 | `		return PH7_OK;` |
|      - | 2755 | `	}` |
|      3 | 2756 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 2757 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 2758 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2759 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2760 | `		return PH7_OK;` |
|      - | 2761 | `	}` |
|      3 | 2762 | `	ph7_value_bool(pV,0);` |
|      3 | 2763 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 2764 | `	ph7_value_bool(pV,1);` |
|      3 | 2765 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 2766 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 2767 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 2768 | `	ph7_value_bool(pV,0);` |
|      3 | 2769 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 2770 | `	ph7_value_int(pV,0);` |
|      3 | 2771 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 2772 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2773 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 2774 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2775 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2776 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 2777 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2778 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 2779 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 2780 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 2781 | `	return PH7_OK;` |
|      2 | 2782 | `}` |
|      - | 2783 | `/*` |
|      - | 2784 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 2785 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 2786 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 2787 | ` * resource, options unconsumed).` |
|      - | 2788 | ` */` |
|      2 | 2789 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2790 | `{` |
|      3 | 2791 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 2792 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 2793 | `	}else{` |
|    ! 0 | 2794 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 2795 | `		if( pArr == 0 ){` |
|    ! 0 | 2796 | `			ph7_result_null(pCtx);` |
|    ! 0 | 2797 | `			return PH7_OK;` |
|      - | 2798 | `		}` |
|    ! 0 | 2799 | `		ph7_result_value(pCtx,pArr);` |
|      - | 2800 | `	}` |
|      3 | 2801 | `	return PH7_OK;` |
|      2 | 2802 | `}` |
|      - | 2803 | `/*` |
|      - | 2804 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 2805 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 2806 | ` */` |
|      - | 2807 | `#ifdef PH7_ENABLE_NET` |
|      - | 2808 | `typedef struct sock_private sock_private;` |
|      - | 2809 | `struct sock_private` |
|      - | 2810 | `{` |
|      - | 2811 | `	ph7_vm *pVm;` |
|      - | 2812 | `	ph7_socket sock;` |
|      - | 2813 | `	int bEof;` |
|      - | 2814 | `};` |
|     15 | 2815 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 2816 | `{` |
|     15 | 2817 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2818 | `	int n;` |
|     15 | 2819 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      1 | 2820 | `		return 0;` |
|      - | 2821 | `	}` |
|     14 | 2822 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     14 | 2823 | `	if( n <= 0 ){` |
|      4 | 2824 | `		pSock->bEof = 1;` |
|      4 | 2825 | `		return 0;` |
|      - | 2826 | `	}` |
|     10 | 2827 | `	return (ph7_int64)n;` |
|      5 | 2828 | `}` |
|      4 | 2829 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 2830 | `{` |
|      4 | 2831 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2832 | `	int n;` |
|      4 | 2833 | `	if( pSock == 0 ){` |
|    ! 0 | 2834 | `		return -1;` |
|      - | 2835 | `	}` |
|      4 | 2836 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 2837 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 2838 | `}` |
|      4 | 2839 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 2840 | `{` |
|      4 | 2841 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 2842 | `	if( pSock == 0 ){` |
|    ! 0 | 2843 | `		return;` |
|      - | 2844 | `	}` |
|      4 | 2845 | `	PH7_NetClose(pSock->sock);` |
|      4 | 2846 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 2847 | `}` |
|      - | 2848 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 2849 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 2850 | `{` |
|      - | 2851 | `	sock_private *pSock;` |
|      - | 2852 | `	ph7_socket sock;` |
|      - | 2853 | `	char zHost[256];` |
|      - | 2854 | `	const char *zColon;` |
|    ! 0 | 2855 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 2856 | `	const char *zErr = "";` |
|    ! 0 | 2857 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 2858 | `	SXUNUSED(iMode);` |
|    ! 0 | 2859 | `	if( pVm == 0 ){` |
|    ! 0 | 2860 | `		return -1;` |
|      - | 2861 | `	}` |
|    ! 0 | 2862 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 2863 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 2864 | `		zColon--;` |
|    ! 0 | 2865 | `	}` |
|    ! 0 | 2866 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 2867 | `		return -1;` |
|      - | 2868 | `	}` |
|      - | 2869 | `	{` |
|    ! 0 | 2870 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 2871 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 2872 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 2873 | `		}` |
|    ! 0 | 2874 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 2875 | `		zHost[n] = 0;` |
|      - | 2876 | `	}` |
|      - | 2877 | `	{` |
|    ! 0 | 2878 | `		sxi32 iTmp = 0;` |
|    ! 0 | 2879 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 2880 | `		iPort = (int)iTmp;` |
|      - | 2881 | `	}` |
|    ! 0 | 2882 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 2883 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 2884 | `		return -1;` |
|      - | 2885 | `	}` |
|    ! 0 | 2886 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 2887 | `	if( pSock == 0 ){` |
|    ! 0 | 2888 | `		PH7_NetClose(sock);` |
|    ! 0 | 2889 | `		return -1;` |
|      - | 2890 | `	}` |
|    ! 0 | 2891 | `	pSock->pVm = pVm;` |
|    ! 0 | 2892 | `	pSock->sock = sock;` |
|    ! 0 | 2893 | `	pSock->bEof = 0;` |
|    ! 0 | 2894 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 2895 | `	return PH7_OK;` |
|    ! 0 | 2896 | `}` |
|      - | 2897 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 2898 | `	"tcp",` |
|      - | 2899 | `	PH7_IO_STREAM_VERSION,` |
|      - | 2900 | `	SockStreamData_Open, /* xOpen */` |
|      - | 2901 | `	0,   /* xOpenDir */` |
|      - | 2902 | `	SockStreamData_Close,/* xClose */` |
|      - | 2903 | `	0,  /* xCloseDir */` |
|      - | 2904 | `	SockStreamData_Read, /* xRead */` |
|      - | 2905 | `	0,  /* xReadDir */` |
|      - | 2906 | `	SockStreamData_Write,/* xWrite */` |
|      - | 2907 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 2908 | `	0,  /* xLock */` |
|      - | 2909 | `	0,  /* xRewindDir */` |
|      - | 2910 | `	0,  /* xTell */` |
|      - | 2911 | `	0,  /* xTrunc */` |
|      - | 2912 | `	0,  /* xSync */` |
|      - | 2913 | `	0   /* xStat */` |
|      - | 2914 | `};` |
|      - | 2915 | `#endif /* PH7_ENABLE_NET */` |
|      - | 2916 | `/*` |
|      - | 2917 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 2918 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 2919 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 2920 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 2921 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 2922 | ` * php streamWrapper protocol method on it.` |
|      - | 2923 | ` */` |
|      - | 2924 | `#define PHL_UWRAP_MAX 8` |
|      - | 2925 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 2926 | `struct uwrap_slot` |
|      - | 2927 | `{` |
|      - | 2928 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 2929 | `	char zScheme[32];         /* protocol name */` |
|      - | 2930 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 2931 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 2932 | `};` |
|      - | 2933 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 2934 | `struct uwrap_handle` |
|      - | 2935 | `{` |
|      - | 2936 | `	ph7_vm *pVm;` |
|      - | 2937 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 2938 | `	int iSlot;` |
|      - | 2939 | `	int bEof;` |
|      - | 2940 | `};` |
|      - | 2941 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 2942 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 2943 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 2944 | `	ph7_value *pResult)` |
|      1 | 2945 | `{` |
|      - | 2946 | `	ph7_class_method *pMeth;` |
|     27 | 2947 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 2948 | `		return -1;` |
|      - | 2949 | `	}` |
|     27 | 2950 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 2951 | `	if( pMeth == 0 ){` |
|    ! 0 | 2952 | `		return -1;` |
|      - | 2953 | `	}` |
|     27 | 2954 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 2955 | `		return -1;` |
|      - | 2956 | `	}` |
|     27 | 2957 | `	return 0;` |
|     14 | 2958 | `}` |
|      8 | 2959 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 2960 | `{` |
|      9 | 2961 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2962 | `	ph7_value sArg,sRet;` |
|      - | 2963 | `	const char *zData;` |
|      9 | 2964 | `	int nData = 0;` |
|      9 | 2965 | `	ph7_int64 n = 0;` |
|      9 | 2966 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 2967 | `		return 0;` |
|      - | 2968 | `	}` |
|      9 | 2969 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 2970 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 2971 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 2972 | `	{` |
|      - | 2973 | `		ph7_value *apArg[1];` |
|      9 | 2974 | `		apArg[0] = &sArg;` |
|      9 | 2975 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 2976 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 2977 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2978 | `			return -1;` |
|      - | 2979 | `		}` |
|      - | 2980 | `	}` |
|      9 | 2981 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 2982 | `	if( nData > 0 ){` |
|      7 | 2983 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 2984 | `			nData = (int)nRead;` |
|    ! 0 | 2985 | `		}` |
|      7 | 2986 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 2987 | `		n = nData;` |
|      4 | 2988 | `	}else{` |
|      3 | 2989 | `		pH->bEof = 1;` |
|      - | 2990 | `	}` |
|      9 | 2991 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 2992 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 2993 | `	return n;` |
|      5 | 2994 | `}` |
|      2 | 2995 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 2996 | `{` |
|      3 | 2997 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2998 | `	ph7_value sArg,sRet;` |
|      - | 2999 | `	ph7_int64 n;` |
|      3 | 3000 | `	if( pH == 0 ){` |
|    ! 0 | 3001 | `		return -1;` |
|      - | 3002 | `	}` |
|      3 | 3003 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 3004 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3005 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 3006 | `	{` |
|      - | 3007 | `		ph7_value *apArg[1];` |
|      3 | 3008 | `		apArg[0] = &sArg;` |
|      3 | 3009 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 3010 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 3011 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3012 | `			return -1;` |
|      - | 3013 | `		}` |
|      - | 3014 | `	}` |
|      3 | 3015 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3016 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 3017 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3018 | `	return n;` |
|      2 | 3019 | `}` |
|      2 | 3020 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 3021 | `{` |
|      3 | 3022 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3023 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 3024 | `	ph7_value *apArg[2];` |
|      - | 3025 | `	int rc;` |
|      3 | 3026 | `	if( pH == 0 ){` |
|    ! 0 | 3027 | `		return -1;` |
|      - | 3028 | `	}` |
|      3 | 3029 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 3030 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 3031 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3032 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 3033 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 3034 | `	apArg[0] = &sOfft;` |
|      3 | 3035 | `	apArg[1] = &sWhence;` |
|      3 | 3036 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 3037 | `	if( rc == 0 ){` |
|      3 | 3038 | `		pH->bEof = 0;` |
|      3 | 3039 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 3040 | `	}` |
|      3 | 3041 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 3042 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 3043 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3044 | `	return rc;` |
|      2 | 3045 | `}` |
|      2 | 3046 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 3047 | `{` |
|      3 | 3048 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3049 | `	ph7_value sRet;` |
|      - | 3050 | `	ph7_int64 n;` |
|      3 | 3051 | `	if( pH == 0 ){` |
|    ! 0 | 3052 | `		return -1;` |
|      - | 3053 | `	}` |
|      3 | 3054 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3055 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 3056 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3057 | `		return -1;` |
|      - | 3058 | `	}` |
|      3 | 3059 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3060 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3061 | `	return n;` |
|      2 | 3062 | `}` |
|      6 | 3063 | `static void UwrapClose(void *pHandle)` |
|      1 | 3064 | `{` |
|      7 | 3065 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 3066 | `	if( pH == 0 ){` |
|    ! 0 | 3067 | `		return;` |
|      - | 3068 | `	}` |
|      7 | 3069 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 3070 | `	if( pH->pObj ){` |
|      7 | 3071 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 3072 | `	}` |
|      7 | 3073 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 3074 | `}` |
|      - | 3075 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 3076 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 3077 | `{` |
|      7 | 3078 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 3079 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 3080 | `	ph7_class *pClass;` |
|      - | 3081 | `	uwrap_handle *pH;` |
|      - | 3082 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 3083 | `	ph7_value *apArg[4];` |
|      - | 3084 | `	int rc;` |
|      7 | 3085 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 3086 | `		return -1;` |
|      - | 3087 | `	}` |
|      7 | 3088 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 3089 | `	if( pClass == 0 ){` |
|    ! 0 | 3090 | `		return -1;` |
|      - | 3091 | `	}` |
|      7 | 3092 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 3093 | `	if( pH == 0 ){` |
|    ! 0 | 3094 | `		return -1;` |
|      - | 3095 | `	}` |
|      7 | 3096 | `	pH->pVm = pVm;` |
|      7 | 3097 | `	pH->iSlot = iSlot;` |
|      7 | 3098 | `	pH->bEof = 0;` |
|      7 | 3099 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 3100 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 3101 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3102 | `		return -1;` |
|      - | 3103 | `	}` |
|      - | 3104 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 3105 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 3106 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 3107 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 3108 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 3109 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 3110 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 3111 | `	 * it as "could not be passed by reference"). */` |
|      - | 3112 | `	{` |
|      7 | 3113 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 3114 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 3115 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3116 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3117 | `			return -1;` |
|      - | 3118 | `		}` |
|      7 | 3119 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 3120 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	{` |
|      - | 3123 | `		SyBlob sUrl;` |
|      7 | 3124 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 3125 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 3126 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 3127 | `		SyBlobRelease(&sUrl);` |
|      - | 3128 | `	}` |
|      9 | 3129 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 3130 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 3131 | `	ph7_value_int(&sOpts,0);` |
|      7 | 3132 | `	apArg[0] = &sPath;` |
|      7 | 3133 | `	apArg[1] = &sMode;` |
|      7 | 3134 | `	apArg[2] = &sOpts;` |
|      7 | 3135 | `	apArg[3] = &sOpened;` |
|      7 | 3136 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 3137 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 3138 | `		rc = -1;` |
|    ! 0 | 3139 | `	}` |
|      7 | 3140 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 3141 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 3142 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 3143 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 3144 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 3145 | `	if( rc != 0 ){` |
|    ! 0 | 3146 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3147 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3148 | `		return -1;` |
|      - | 3149 | `	}` |
|      7 | 3150 | `	*ppHandle = (void *)pH;` |
|      7 | 3151 | `	return PH7_OK;` |
|      4 | 3152 | `}` |
|      - | 3153 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 3154 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 3155 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 3156 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 3157 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 3158 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 3159 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 3160 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 3161 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 3162 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 3163 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 3164 | `PHL_UWRAP_THUNK(7)` |
|      - | 3165 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 3166 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 3167 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 3168 | `};` |
|      - | 3169 | `/*` |
|      - | 3170 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 3171 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 3172 | ` */` |
|      2 | 3173 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3174 | `{` |
|      - | 3175 | `	const char *zScheme,*zClass;` |
|      3 | 3176 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 3177 | `	if( nArg < 2 ){` |
|    ! 0 | 3178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3179 | `		return PH7_OK;` |
|      - | 3180 | `	}` |
|      3 | 3181 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3182 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 3183 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 3184 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 3185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3186 | `		return PH7_OK;` |
|      - | 3187 | `	}` |
|      - | 3188 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 3189 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 3190 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 3191 | `	{` |
|      3 | 3192 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 3193 | `		sxu32 n;` |
|     11 | 3194 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 3195 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 3196 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 3197 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3198 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 3199 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3200 | `				return PH7_OK;` |
|      - | 3201 | `			}` |
|      5 | 3202 | `		}` |
|      - | 3203 | `	}` |
|      3 | 3204 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 3205 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 3206 | `			iFree = i;` |
|      3 | 3207 | `			break;` |
|      - | 3208 | `		}` |
|    ! 0 | 3209 | `	}` |
|      3 | 3210 | `	if( iFree < 0 ){` |
|    ! 0 | 3211 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3212 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 3213 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3214 | `		return PH7_OK;` |
|      - | 3215 | `	}` |
|      - | 3216 | `	{` |
|      3 | 3217 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 3218 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 3219 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 3220 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 3221 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 3222 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 3223 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 3224 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 3225 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 3226 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 3227 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 3228 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 3229 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 3230 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 3231 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 3232 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 3233 | `	}` |
|      3 | 3234 | `	ph7_result_bool(pCtx,1);` |
|      3 | 3235 | `	return PH7_OK;` |
|      2 | 3236 | `}` |
|      2 | 3237 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3238 | `{` |
|      - | 3239 | `	const char *zScheme;` |
|      - | 3240 | `	int nScheme,i;` |
|      3 | 3241 | `	if( nArg < 1 ){` |
|    ! 0 | 3242 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3243 | `		return PH7_OK;` |
|      - | 3244 | `	}` |
|      3 | 3245 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3246 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 3247 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 3248 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 3249 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 3250 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 3251 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 3252 | `			 * what unregister means to a script — recorded. */` |
|      3 | 3253 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 3254 | `			ph7_result_bool(pCtx,1);` |
|      3 | 3255 | `			return PH7_OK;` |
|      - | 3256 | `		}` |
|    ! 0 | 3257 | `	}` |
|    ! 0 | 3258 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 3259 | `	return PH7_OK;` |
|      2 | 3260 | `}` |
|      - | 3261 | `#ifdef PH7_ENABLE_NET` |
|      - | 3262 | `/*` |
|      - | 3263 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 3264 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 3265 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 3266 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 3267 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 3268 | ` */` |
|      6 | 3269 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3270 | `{` |
|      6 | 3271 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 3272 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 3273 | `	const char *zTarget,*zErr = "";` |
|      - | 3274 | `	char zHost[256];` |
|      6 | 3275 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 3276 | `	ph7_socket sock;` |
|      - | 3277 | `	io_private *pDev;` |
|      - | 3278 | `	sock_private *pSock;` |
|      6 | 3279 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 3280 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 3281 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 3282 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 3283 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3284 | `		return PH7_OK;` |
|      - | 3285 | `	}` |
|      6 | 3286 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 3287 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 3288 | `	{` |
|      6 | 3289 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 3290 | `		const char *zSep = 0;` |
|     32 | 3291 | `		while( z < zEnd - 2 ){` |
|     30 | 3292 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 3293 | `				zSep = z;` |
|      4 | 3294 | `				break;` |
|      - | 3295 | `			}` |
|     26 | 3296 | `			z++;` |
|    ! 0 | 3297 | `		}` |
|      6 | 3298 | `		if( zSep ){` |
|      4 | 3299 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 3300 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3301 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 3302 | `					nTarget,zTarget);` |
|    ! 0 | 3303 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3304 | `				return PH7_OK;` |
|      - | 3305 | `			}` |
|      4 | 3306 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 3307 | `			zTarget = zSep + 3;` |
|      2 | 3308 | `		}` |
|      - | 3309 | `	}` |
|      - | 3310 | `	/* host[:port] */` |
|      - | 3311 | `	{` |
|      6 | 3312 | `		int i = nTarget - 1;` |
|      6 | 3313 | `		int nHost = nTarget;` |
|     48 | 3314 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 3315 | `			i--;` |
|    ! 0 | 3316 | `		}` |
|      6 | 3317 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 3318 | `			sxi32 iTmp = 0;` |
|      2 | 3319 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 3320 | `			iPort = (int)iTmp;` |
|      2 | 3321 | `			nHost = i;` |
|      1 | 3322 | `		}` |
|      6 | 3323 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 3324 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 3325 | `		}` |
|      6 | 3326 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 3327 | `		zHost[nHost] = 0;` |
|      - | 3328 | `	}` |
|      6 | 3329 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 3330 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 3331 | `	}` |
|      6 | 3332 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 3333 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 3334 | `		if( rTimeout > 0 ){` |
|      6 | 3335 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 3336 | `		}` |
|      3 | 3337 | `	}` |
|      6 | 3338 | `	if( iPort < 0 ){` |
|    ! 0 | 3339 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3340 | `		return PH7_OK;` |
|      - | 3341 | `	}` |
|      6 | 3342 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 3343 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 3344 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 3345 | `		{` |
|      2 | 3346 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 3347 | `			if( pTmp ){` |
|      2 | 3348 | `				if( nArg > iArgErrno ){` |
|      2 | 3349 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 3350 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 3351 | `				}` |
|      2 | 3352 | `				if( nArg > iArgErrstr ){` |
|      2 | 3353 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 3354 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 3355 | `				}` |
|      1 | 3356 | `			}` |
|      - | 3357 | `		}` |
|      - | 3358 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 3359 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 3360 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 3361 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3362 | `		return PH7_OK;` |
|      - | 3363 | `	}` |
|      - | 3364 | `	{` |
|      4 | 3365 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 3366 | `		if( pTmp ){` |
|      4 | 3367 | `			if( nArg > iArgErrno ){` |
|      4 | 3368 | `				ph7_value_int(pTmp,0);` |
|      4 | 3369 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 3370 | `			}` |
|      4 | 3371 | `			if( nArg > iArgErrstr ){` |
|      4 | 3372 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 3373 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 3374 | `			}` |
|      2 | 3375 | `		}` |
|      - | 3376 | `	}` |
|      - | 3377 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 3378 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 3379 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 3380 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 3381 | `		PH7_NetClose(sock);` |
|    ! 0 | 3382 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3383 | `		return PH7_OK;` |
|      - | 3384 | `	}` |
|      4 | 3385 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 3386 | `	pSock->sock = sock;` |
|      4 | 3387 | `	pSock->bEof = 0;` |
|      4 | 3388 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 3389 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 3390 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 3391 | `	return PH7_OK;` |
|      3 | 3392 | `}` |
|      - | 3393 | `#endif /* PH7_ENABLE_NET */` |
|    304 | 3394 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3395 | `{` |
|      - | 3396 | `	const ph7_io_stream *pStream;` |
|      - | 3397 | `	const char *zUri,*zMode;` |
|      - | 3398 | `	ph7_value *pResource;` |
|      - | 3399 | `	io_private *pDev;` |
|      - | 3400 | `	int iLen,imLen;` |
|      - | 3401 | `	int iOpenFlags;` |
|    309 | 3402 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3403 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3404 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3406 | `		return PH7_OK;` |
|      - | 3407 | `	}` |
|      - | 3408 | `	/* Extract the URI and the desired access mode */` |
|    309 | 3409 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    309 | 3410 | `	if( nArg > 1 ){` |
|    309 | 3411 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    157 | 3412 | `	}else{` |
|      - | 3413 | `		/* Set a default read-only mode */` |
|    ! 0 | 3414 | `		zMode = "r";` |
|    ! 0 | 3415 | `		imLen = (int)sizeof(char);` |
|      - | 3416 | `	}` |
|      - | 3417 | `	/* Try to extract a stream */` |
|    309 | 3418 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    309 | 3419 | `	if( pStream == 0 ){` |
|    ! 0 | 3420 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3421 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 3422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3423 | `		return PH7_OK;` |
|      - | 3424 | `	}` |
|      - | 3425 | `	/* Allocate a new IO private instance */` |
|    309 | 3426 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    309 | 3427 | `	if( pDev == 0 ){` |
|    ! 0 | 3428 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3429 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3430 | `		return PH7_OK;` |
|      - | 3431 | `	}` |
|    309 | 3432 | `	pResource = 0;` |
|    309 | 3433 | `	if( nArg > 3 ){` |
|    ! 0 | 3434 | `		pResource = apArg[3];` |
|    309 | 3435 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 3436 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 3437 | `		 * access the underlying virtual machine.` |
|      - | 3438 | `		 */` |
|     86 | 3439 | `		pResource = apArg[0];` |
|     41 | 3440 | `	}` |
|      - | 3441 | `	/* Initialize the structure */` |
|    309 | 3442 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3443 | `	/* Convert open mode to PH7 flags */` |
|    309 | 3444 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 3445 | `	/* Try to get a handle */` |
|    461 | 3446 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    152 | 3447 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    309 | 3448 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3449 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|      3 | 3450 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3451 | `		ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 3452 | `		return PH7_OK;` |
|      - | 3453 | `	}` |
|      - | 3454 | `	/* All done,return the io_private instance as a resource */` |
|    307 | 3455 | `	ph7_result_resource(pCtx,pDev);` |
|    307 | 3456 | `	return PH7_OK;` |
|    157 | 3457 | `}` |
|      - | 3458 | `/*` |
|      - | 3459 | ` * bool fclose(resource $handle)` |
|      - | 3460 | ` *  Closes an open file pointer` |
|      - | 3461 | ` * Parameters` |
|      - | 3462 | ` *  $handle` |
|      - | 3463 | ` *   The file pointer.` |
|      - | 3464 | ` * Return` |
|      - | 3465 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3466 | ` */` |
|    400 | 3467 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3468 | `{` |
|      - | 3469 | `	const ph7_io_stream *pStream;` |
|      - | 3470 | `	io_private *pDev;` |
|      - | 3471 | `	ph7_vm *pVm;` |
|    405 | 3472 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3473 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3474 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3476 | `		return PH7_OK;` |
|      - | 3477 | `	}` |
|      - | 3478 | `	/* Extract our private data */` |
|    405 | 3479 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3480 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    405 | 3481 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 3482 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3483 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 3484 | `	}` |
|      - | 3485 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    403 | 3486 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3487 | `		/*Expecting an IO handle */` |
|    ! 0 | 3488 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3489 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3490 | `		return PH7_OK;` |
|      - | 3491 | `	}` |
|      - | 3492 | `	/* Point to the target IO stream device */` |
|    403 | 3493 | `	pStream = pDev->pStream;` |
|    403 | 3494 | `	if( pStream == 0 ){` |
|    ! 0 | 3495 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3496 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3497 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3498 | `			);` |
|    ! 0 | 3499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3500 | `		return PH7_OK;` |
|      - | 3501 | `	}` |
|      - | 3502 | `	/* Point to the VM that own this context */` |
|    403 | 3503 | `	pVm = pCtx->pVm;` |
|      - | 3504 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    403 | 3505 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 3506 | `		/* Perform the requested operation */` |
|    403 | 3507 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3508 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    403 | 3509 | `		MarkIOPrivateClosed(pDev);` |
|    199 | 3510 | `	}` |
|      - | 3511 | `	/* Return TRUE */` |
|    403 | 3512 | `	ph7_result_bool(pCtx,1);` |
|    403 | 3513 | `	return PH7_OK;` |
|    205 | 3514 | `}` |
|      - | 3515 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3516 | `/*` |
|      - | 3517 | ` * MD5/SHA1 digest consumer.` |
|      - | 3518 | ` */` |
|     72 | 3519 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3520 | `{` |
|      - | 3521 | `	/* Append hex chunk verbatim */` |
|     73 | 3522 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 3523 | `	return SXRET_OK;` |
|      1 | 3524 | `}` |
|      - | 3525 | `/*` |
|      - | 3526 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 3527 | ` *  Calculates the md5 hash of a given file.` |
|      - | 3528 | ` * Parameters` |
|      - | 3529 | ` *  $uri` |
|      - | 3530 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3531 | ` *  $raw_output` |
|      - | 3532 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 3533 | ` * Return` |
|      - | 3534 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 3535 | ` */` |
|      2 | 3536 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3537 | `{` |
|      - | 3538 | `	const ph7_io_stream *pStream;` |
|      - | 3539 | `	unsigned char zDigest[16];` |
|      3 | 3540 | `	int raw_output  = FALSE;` |
|      - | 3541 | `	const char *zFile;` |
|      - | 3542 | `	MD5Context sCtx;` |
|      - | 3543 | `	char zBuf[8192];` |
|      - | 3544 | `	void *pHandle;` |
|      - | 3545 | `	ph7_int64 n;` |
|      - | 3546 | `	int nLen;` |
|      3 | 3547 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3548 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3549 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3550 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3551 | `		return PH7_OK;` |
|      - | 3552 | `	}` |
|      - | 3553 | `	/* Extract the file path */` |
|      3 | 3554 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3555 | `	/* Point to the target IO stream device */` |
|      3 | 3556 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3557 | `	if( pStream == 0 ){` |
|    ! 0 | 3558 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3560 | `		return PH7_OK;` |
|      - | 3561 | `	}` |
|      3 | 3562 | `	if( nArg > 1 ){` |
|    ! 0 | 3563 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3564 | `	}` |
|      - | 3565 | `	/* Try to open the file in read-only mode */` |
|      3 | 3566 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3567 | `	if( pHandle == 0 ){` |
|    ! 0 | 3568 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3569 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3570 | `		return PH7_OK;` |
|      - | 3571 | `	}` |
|      - | 3572 | `	/* Init the MD5 context */` |
|      3 | 3573 | `	MD5Init(&sCtx);` |
|      - | 3574 | `	/* Perform the requested operation */` |
|      2 | 3575 | `	for(;;){` |
|      5 | 3576 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3577 | `		if( n < 1 ){` |
|      - | 3578 | `			/* EOF or IO error,break immediately */` |
|      3 | 3579 | `			break;` |
|      - | 3580 | `		}` |
|      3 | 3581 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3582 | `	}` |
|      - | 3583 | `	/* Close the stream */` |
|      3 | 3584 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3585 | `	/* Extract the digest */` |
|      3 | 3586 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 3587 | `	if( raw_output ){` |
|      - | 3588 | `		/* Output raw digest */` |
|    ! 0 | 3589 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3590 | `	}else{` |
|      - | 3591 | `		/* Perform a binary to hex conversion */` |
|      3 | 3592 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3593 | `	}` |
|      3 | 3594 | `	return PH7_OK;` |
|      2 | 3595 | `}` |
|      - | 3596 | `/*` |
|      - | 3597 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 3598 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 3599 | ` * Parameters` |
|      - | 3600 | ` *  $uri` |
|      - | 3601 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3602 | ` *  $raw_output` |
|      - | 3603 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 3604 | ` * Return` |
|      - | 3605 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 3606 | ` */` |
|      2 | 3607 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3608 | `{` |
|      - | 3609 | `	const ph7_io_stream *pStream;` |
|      - | 3610 | `	unsigned char zDigest[20];` |
|      3 | 3611 | `	int raw_output  = FALSE;` |
|      - | 3612 | `	const char *zFile;` |
|      - | 3613 | `	SHA1Context sCtx;` |
|      - | 3614 | `	char zBuf[8192];` |
|      - | 3615 | `	void *pHandle;` |
|      - | 3616 | `	ph7_int64 n;` |
|      - | 3617 | `	int nLen;` |
|      3 | 3618 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3619 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3620 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3621 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3622 | `		return PH7_OK;` |
|      - | 3623 | `	}` |
|      - | 3624 | `	/* Extract the file path */` |
|      3 | 3625 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3626 | `	/* Point to the target IO stream device */` |
|      3 | 3627 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3628 | `	if( pStream == 0 ){` |
|    ! 0 | 3629 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3630 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3631 | `		return PH7_OK;` |
|      - | 3632 | `	}` |
|      3 | 3633 | `	if( nArg > 1 ){` |
|    ! 0 | 3634 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3635 | `	}` |
|      - | 3636 | `	/* Try to open the file in read-only mode */` |
|      3 | 3637 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3638 | `	if( pHandle == 0 ){` |
|    ! 0 | 3639 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3640 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3641 | `		return PH7_OK;` |
|      - | 3642 | `	}` |
|      - | 3643 | `	/* Init the SHA1 context */` |
|      3 | 3644 | `	SHA1Init(&sCtx);` |
|      - | 3645 | `	/* Perform the requested operation */` |
|      2 | 3646 | `	for(;;){` |
|      5 | 3647 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3648 | `		if( n < 1 ){` |
|      - | 3649 | `			/* EOF or IO error,break immediately */` |
|      3 | 3650 | `			break;` |
|      - | 3651 | `		}` |
|      3 | 3652 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3653 | `	}` |
|      - | 3654 | `	/* Close the stream */` |
|      3 | 3655 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3656 | `	/* Extract the digest */` |
|      3 | 3657 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 3658 | `	if( raw_output ){` |
|      - | 3659 | `		/* Output raw digest */` |
|    ! 0 | 3660 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3661 | `	}else{` |
|      - | 3662 | `		/* Perform a binary to hex conversion */` |
|      3 | 3663 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3664 | `	}` |
|      3 | 3665 | `	return PH7_OK;` |
|      2 | 3666 | `}` |
|      - | 3667 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3668 | `/*` |
|      - | 3669 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 3670 | ` *  Parse a configuration file.` |
|      - | 3671 | ` * Parameters` |
|      - | 3672 | ` * $filename` |
|      - | 3673 | ` *  The filename of the ini file being parsed.` |
|      - | 3674 | ` * $process_sections` |
|      - | 3675 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 3676 | ` *  with the section names and settings included.` |
|      - | 3677 | ` *  The default for process_sections is FALSE.` |
|      - | 3678 | ` * $scanner_mode` |
|      - | 3679 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 3680 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 3681 | ` * Return` |
|      - | 3682 | ` *  The settings are returned as an associative array on success.` |
|      - | 3683 | ` *  Otherwise is returned.` |
|      - | 3684 | ` */` |
|      2 | 3685 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3686 | `{` |
|      - | 3687 | `	const ph7_io_stream *pStream;` |
|      - | 3688 | `	const char *zFile;` |
|      - | 3689 | `	SyBlob sContents;` |
|      - | 3690 | `	void *pHandle;` |
|      - | 3691 | `	int nLen;` |
|      3 | 3692 | `	sxi32 rc = PH7_OK;` |
|      3 | 3693 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3694 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3695 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3697 | `		return PH7_OK;` |
|      - | 3698 | `	}` |
|      - | 3699 | `	/* Extract the file path */` |
|      3 | 3700 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3701 | `	/* Point to the target IO stream device */` |
|      3 | 3702 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3703 | `	if( pStream == 0 ){` |
|    ! 0 | 3704 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|      - | 3708 | `	/* Try to open the file in read-only mode */` |
|      3 | 3709 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3710 | `	if( pHandle == 0 ){` |
|    ! 0 | 3711 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      3 | 3715 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 3716 | `	/* Read the whole file */` |
|      3 | 3717 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 3718 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 3719 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 3720 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3721 | `	}else{` |
|      - | 3722 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 3723 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 3724 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 3725 | `	}` |
|      - | 3726 | `	/* Close the stream */` |
|      3 | 3727 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3728 | `	/* Release the working buffer */` |
|      3 | 3729 | `	SyBlobRelease(&sContents);` |
|      - | 3730 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 3731 | `	return rc;` |
|      2 | 3732 | `}` |
|      - | 3733 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 3734 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 3735 | `/*` |
|      - | 3736 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 3737 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 3738 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 3739 | ` * to any non-VFS resource).` |
|      - | 3740 | ` */` |
|      - | 3741 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 3742 | `{` |
|      - | 3743 | `	SXUNUSED(pResource);` |
|      - | 3744 | `	return "Unknown";` |
|      - | 3745 | `}` |
|      - | 3746 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 3747 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 3748 | `{` |
|      - | 3749 | `	SXUNUSED(pResource);` |
|      - | 3750 | `	return 0;` |
|      - | 3751 | `}` |
|      - | 3752 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3753 |  |
