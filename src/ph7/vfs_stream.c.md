# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1351/1936 lines (69.78%)

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
|     44 |   49 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      2 |   50 | `{` |
|     46 |   51 | `	io_private *pDev = (io_private *)pResource;` |
|     46 |   52 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      2 |   53 | `}` |
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
|  12494 |  344 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  345 | `{` |
|      - |  346 | `	const ph7_io_stream *pStream;` |
|      - |  347 | `	io_private *pDev;` |
|      - |  348 | `	int rc;` |
|  12499 |  349 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  350 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  351 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  352 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  353 | `		return PH7_OK;` |
|      - |  354 | `	}` |
|      - |  355 | `	/* Extract our private data */` |
|  12499 |  356 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  357 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  12499 |  358 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  359 | `		/*Expecting an IO handle */` |
|    ! 0 |  360 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  361 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  362 | `		return PH7_OK;` |
|      - |  363 | `	}` |
|      - |  364 | `	/* Point to the target IO stream device */` |
|  12499 |  365 | `	pStream = pDev->pStream;` |
|  12499 |  366 | `	if( pStream == 0 ){` |
|    ! 0 |  367 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  368 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  369 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  370 | `			);` |
|    ! 0 |  371 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  372 | `		return PH7_OK;` |
|      - |  373 | `	}` |
|  12499 |  374 | `	rc = SXERR_EOF;` |
|      - |  375 | `	/* Perform the requested operation */` |
|  12499 |  376 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  377 | `		/* Data is available */` |
|   6257 |  378 | `		rc = PH7_OK;` |
|   3131 |  379 | `	}else{` |
|      - |  380 | `		char zBuf[4096];` |
|      - |  381 | `		ph7_int64 n;` |
|      - |  382 | `		/* Perform a buffered read */` |
|   6247 |  383 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   6247 |  384 | `		if( n > 0 ){` |
|      - |  385 | `			/* Copy buffered data */` |
|   2101 |  386 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   2101 |  387 | `			rc = PH7_OK;` |
|   1048 |  388 | `		}` |
|      - |  389 | `	}` |
|      - |  390 | `	/* EOF or not */` |
|  12499 |  391 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  12499 |  392 | `	return PH7_OK;` |
|   6252 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  396 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  397 | ` * [i.e: IO error ] or EOF.` |
|      - |  398 | ` */` |
|     36 |  399 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      3 |  400 | `{` |
|     39 |  401 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     39 |  402 | `	char *zBuf = (char *)pBuf;` |
|      - |  403 | `	ph7_int64 n,nRead;` |
|     39 |  404 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     39 |  405 | `	if( n > 0 ){` |
|      2 |  406 | `		if( n > nLen ){` |
|    ! 0 |  407 | `			n = nLen;` |
|    ! 0 |  408 | `		}` |
|      - |  409 | `		/* Copy the buffered data */` |
|      2 |  410 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  411 | `		/* Update the read offset */` |
|      2 |  412 | `		pDev->nOfft += (sxu32)n;` |
|      2 |  413 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  414 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 |  415 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 |  416 | `			pDev->nOfft = 0;` |
|      1 |  417 | `		}` |
|      2 |  418 | `		nLen -= n;` |
|      2 |  419 | `		if( nLen < 1 ){` |
|      - |  420 | `			/* All done */` |
|    ! 0 |  421 | `			return n;` |
|      - |  422 | `		}` |
|      - |  423 | `		/* Advance the cursor */` |
|      2 |  424 | `		zBuf += n;` |
|      1 |  425 | `	}` |
|      - |  426 | `	/* Read without buffering */` |
|     39 |  427 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     39 |  428 | `	if( nRead > 0 ){` |
|     36 |  429 | `		n += nRead;` |
|     20 |  430 | `	}else if( n < 1 ){` |
|      - |  431 | `		/* EOF or IO error */` |
|      3 |  432 | `		return nRead;` |
|      - |  433 | `	}` |
|     37 |  434 | `	return n;` |
|     21 |  435 | `}` |
|      - |  436 | `/*` |
|      - |  437 | ` * Extract a single line from the buffered input.` |
|      - |  438 | ` */` |
|   8572 |  439 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  440 | `{` |
|      - |  441 | `	const char *zIn,*zEnd,*zPtr;` |
|   8577 |  442 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   8577 |  443 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   8577 |  444 | `	zPtr = zIn;` |
| 463017 |  445 | `	while( zIn < zEnd ){` |
| 462835 |  446 | `		if( zIn[0] == '\n' ){` |
|      - |  447 | `			/* Line found */` |
|   8395 |  448 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   8395 |  449 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   8395 |  450 | `			*pzLine = zPtr;` |
|   8395 |  451 | `			return SXRET_OK;` |
|      - |  452 | `		}` |
| 454445 |  453 | `		zIn++;` |
|      5 |  454 | `	}` |
|      - |  455 | `	/* No line were found */` |
|    187 |  456 | `	return SXERR_NOTFOUND;` |
|   4291 |  457 | `}` |
|      - |  458 | `/*` |
|      - |  459 | ` * Read a single line from the underlying IO stream device.` |
|      - |  460 | ` */` |
|   8594 |  461 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  462 | `{` |
|   8599 |  463 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - |  464 | `	char zBuf[8192];` |
|      - |  465 | `	ph7_int64 n;` |
|      - |  466 | `	sxi32 rc;` |
|   8599 |  467 | `	n = 0;` |
|   8599 |  468 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  469 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    165 |  470 | `		SyBlobReset(&pDev->sBuffer);` |
|    165 |  471 | `		pDev->nOfft = 0;` |
|     80 |  472 | `	}` |
|   8599 |  473 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  474 | `		/* Check if there is a line */` |
|   8439 |  475 | `		rc = GetLine(pDev,&n,pzData);` |
|   8439 |  476 | `		if( rc == SXRET_OK ){` |
|      - |  477 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  478 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|   8299 |  479 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  480 | `				n = nMaxLen;` |
|    ! 0 |  481 | `			}` |
|   8299 |  482 | `			pDev->nOfft += (sxu32)n;` |
|   8299 |  483 | `			return n;` |
|      - |  484 | `		}` |
|     70 |  485 | `	}` |
|      - |  486 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  487 | `	 * limit is reached.` |
|      - |  488 | `	 */` |
|    154 |  489 | `	for(;;){` |
|    313 |  490 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    313 |  491 | `		if( n < 1 ){` |
|      - |  492 | `			/* EOF or IO error */` |
|    175 |  493 | `			break;` |
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
|    175 |  520 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  521 | `		/* Read limit reached,return the available data */` |
|    141 |  522 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    141 |  523 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  524 | `		/* Reset the working buffer */` |
|    141 |  525 | `		SyBlobReset(&pDev->sBuffer);` |
|    141 |  526 | `		pDev->nOfft = 0;` |
|     68 |  527 | `	}` |
|    175 |  528 | `	return n;` |
|   4302 |  529 | `}` |
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
|  29974 |  551 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  552 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 |  553 | `{` |
|  29979 |  554 | `	void *pHandle = 0; /* cc warning */` |
|      - |  555 | `	SyString sFile;` |
|      - |  556 | `	ph7_value sDummy;` |
|      - |  557 | `	int rc;` |
|  29979 |  558 | `	if( pStream == 0 ){` |
|      - |  559 | `		/* No such stream device */` |
|    ! 0 |  560 | `		return 0;` |
|      - |  561 | `	}` |
|  29979 |  562 | `	if( pResource == 0 ){` |
|      - |  563 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  564 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  565 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  566 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  567 | `		 * reads it during the call, and file:// ignores it. */` |
|  29911 |  568 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  29911 |  569 | `		pResource = &sDummy;` |
|  14953 |  570 | `	}` |
|  29979 |  571 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  29979 |  572 | `	if( use_include ){` |
|   9214 |  573 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - |  574 | `#ifdef __WINNT__` |
|      - |  575 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  576 | `#endif` |
|   9175 |  577 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9170 |  578 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - |  579 | `				/*  Open the file directly */` |
|     47 |  580 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     47 |  581 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - |  582 | `					/* Mark as included */` |
|     45 |  583 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     21 |  584 | `				}` |
|     25 |  585 | `		}else{` |
|      - |  586 | `			SyString *pPath;` |
|      - |  587 | `			SyBlob sWorker;` |
|      - |  588 | `#ifdef __WINNT__` |
|      - |  589 | `			static const int c = '\\';` |
|      - |  590 | `#else` |
|      - |  591 | `			static const int c = '/';` |
|      - |  592 | `#endif` |
|      - |  593 | `			/* Init the path builder working buffer */` |
|   9172 |  594 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  595 | `			/* Build a path from the set of include path */` |
|   9172 |  596 | `			SySetResetCursor(&pVm->aPaths);` |
|   9172 |  597 | `			rc = SXERR_IO;` |
|   9178 |  598 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - |  599 | `				/* Build full path */` |
|   9172 |  600 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - |  601 | `				/* Append null terminator */` |
|   9172 |  602 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  603 | `					continue;` |
|      - |  604 | `				}` |
|      - |  605 | `				/* Try to open the file */` |
|   9172 |  606 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9172 |  607 | `				if( rc == PH7_OK ){` |
|   9166 |  608 | `					if( bPushInclude ){` |
|      - |  609 | `						/* Mark as included */` |
|   9166 |  610 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4582 |  611 | `					}` |
|   9166 |  612 | `					break;` |
|      - |  613 | `				}` |
|      - |  614 | `				/* Reset the working buffer */` |
|      8 |  615 | `				SyBlobReset(&sWorker);` |
|      - |  616 | `				/* Check the next path */` |
|      2 |  617 | `			}` |
|   9172 |  618 | `			SyBlobRelease(&sWorker);` |
|      - |  619 | `		}` |
|   4610 |  620 | `	}else{` |
|      - |  621 | `		/* Open the URI direcly */` |
|  20765 |  622 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - |  623 | `	}` |
|  29979 |  624 | `	if( rc != PH7_OK ){` |
|      - |  625 | `		/* IO error */` |
|     24 |  626 | `		return 0;` |
|      - |  627 | `	}` |
|      - |  628 | `	/* Return the file handle */` |
|  29959 |  629 | `	return pHandle;` |
|  14992 |  630 | `}` |
|      - |  631 | `/*` |
|      - |  632 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - |  633 | ` * Store the read data in the given BLOB (last argument).` |
|      - |  634 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - |  635 | ` */` |
|   9196 |  636 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      3 |  637 | `{` |
|      - |  638 | `	ph7_int64 nRead;` |
|      - |  639 | `	char zBuf[8192]; /* 8K */` |
|      - |  640 | `	int rc;` |
|      - |  641 | `	/* Perform the requested operation */` |
|   9196 |  642 | `	for(;;){` |
|  18395 |  643 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  18395 |  644 | `		if( nRead < 1 ){` |
|      - |  645 | `			/* EOF or IO error */` |
|   9199 |  646 | `			break;` |
|      - |  647 | `		}` |
|      - |  648 | `		/* Append contents */` |
|   9199 |  649 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9199 |  650 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  651 | `			break;` |
|      - |  652 | `		}` |
|      3 |  653 | `	}` |
|   9199 |  654 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      3 |  655 | `}` |
|      - |  656 | `/*` |
|      - |  657 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - |  658 | ` */` |
|  30078 |  659 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 |  660 | `{` |
|  30083 |  661 | `	if( pStream->xClose ){` |
|  30083 |  662 | `		pStream->xClose(pHandle);` |
|  15039 |  663 | `	}` |
|  30083 |  664 | `}` |
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
|   8470 |  735 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  736 | `{` |
|      - |  737 | `	const ph7_io_stream *pStream;` |
|      - |  738 | `	const char *zLine;` |
|      - |  739 | `	io_private *pDev;` |
|      - |  740 | `	ph7_int64 n,nLen;` |
|   8475 |  741 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  742 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  743 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  745 | `		return PH7_OK;` |
|      - |  746 | `	}` |
|      - |  747 | `	/* Extract our private data */` |
|   8475 |  748 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  749 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8475 |  750 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  751 | `		/*Expecting an IO handle */` |
|    ! 0 |  752 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  754 | `		return PH7_OK;` |
|      - |  755 | `	}` |
|      - |  756 | `	/* Point to the target IO stream device */` |
|   8475 |  757 | `	pStream = pDev->pStream;` |
|   8475 |  758 | `	if( pStream == 0  ){` |
|    ! 0 |  759 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  760 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  761 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  762 | `			);` |
|    ! 0 |  763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  764 | `		return PH7_OK;` |
|      - |  765 | `	}` |
|   8475 |  766 | `	nLen = -1;` |
|   8475 |  767 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
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
|   8469 |  785 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   8469 |  786 | `	if( n < 1 ){` |
|      - |  787 | `		/* EOF or IO error,return FALSE */` |
|     13 |  788 | `		ph7_result_bool(pCtx,0);` |
|      9 |  789 | `	}else{` |
|      - |  790 | `		/* Return the freshly extracted line */` |
|   8461 |  791 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - |  792 | `	}` |
|   8469 |  793 | `	return PH7_OK;` |
|   4240 |  794 | `}` |
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
|     32 |  806 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  807 | `{` |
|      - |  808 | `	const ph7_io_stream *pStream;` |
|      - |  809 | `	io_private *pDev;` |
|      - |  810 | `	ph7_int64 nRead;` |
|      - |  811 | `	void *pBuf;` |
|      - |  812 | `	int nLen;` |
|     35 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  815 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  816 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  817 | `		return PH7_OK;` |
|      - |  818 | `	}` |
|      - |  819 | `	/* Extract our private data */` |
|     35 |  820 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  821 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     35 |  822 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  823 | `		/*Expecting an IO handle */` |
|    ! 0 |  824 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  826 | `		return PH7_OK;` |
|      - |  827 | `	}` |
|      - |  828 | `	/* Point to the target IO stream device */` |
|     35 |  829 | `	pStream = pDev->pStream;` |
|     35 |  830 | `	if( pStream == 0  ){` |
|    ! 0 |  831 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  832 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  833 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  834 | `			);` |
|    ! 0 |  835 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  836 | `		return PH7_OK;` |
|      - |  837 | `	}` |
|     35 |  838 | `        nLen = 4096;` |
|     35 |  839 | `	if( nArg > 1 ){` |
|      - |  840 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - |  841 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - |  842 | `	   * the central type screen (the recorded null-policy divergence). */` |
|     35 |  843 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|     35 |  844 | `	  if( nWant < 1 ){` |
|      5 |  845 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  846 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - |  847 | `	  }` |
|     31 |  848 | `	  nLen = (int)nWant;` |
|     31 |  849 | `	  if( nLen < 1 ){` |
|      - |  850 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - |  851 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - |  852 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - |  853 | `		 * that do not fit an int. */` |
|    ! 0 |  854 | `		nLen = 4096;` |
|    ! 0 |  855 | `	  }` |
|     14 |  856 | `        }` |
|      - |  857 | `	/* Allocate enough buffer */` |
|     31 |  858 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     31 |  859 | `	if( pBuf == 0 ){` |
|    ! 0 |  860 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  862 | `		return PH7_OK;` |
|      - |  863 | `	}` |
|      - |  864 | `	/* Perform the requested operation */` |
|     31 |  865 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     31 |  866 | `	if( nRead < 1 ){` |
|      - |  867 | `		/* Nothing read,return FALSE */` |
|    ! 0 |  868 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  869 | `	}else{` |
|      - |  870 | `		/* Make a copy of the data just read */` |
|     31 |  871 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - |  872 | `	}` |
|      - |  873 | `	/* Release the buffer */` |
|     31 |  874 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     31 |  875 | `	return PH7_OK;` |
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
|  11374 | 1066 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1067 | `{` |
|      - | 1068 | `	const ph7_io_stream *pStream;` |
|      - | 1069 | `	io_private *pDev;` |
|      - | 1070 | `	int rc;` |
|  11379 | 1071 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1072 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1073 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1075 | `		return PH7_OK;` |
|      - | 1076 | `	}` |
|      - | 1077 | `	/* Extract our private data */` |
|  11379 | 1078 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1079 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  11379 | 1080 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1081 | `		/*Expecting an IO handle */` |
|    ! 0 | 1082 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1083 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1084 | `		return PH7_OK;` |
|      - | 1085 | `	}` |
|      - | 1086 | `	/* Point to the target IO stream device */` |
|  11379 | 1087 | `	pStream = pDev->pStream;` |
|  11379 | 1088 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1089 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1090 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1091 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1092 | `			);` |
|    ! 0 | 1093 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1094 | `		return PH7_OK;` |
|      - | 1095 | `	}` |
|  11379 | 1096 | `	ph7_result_bool(pCtx,0);` |
|      - | 1097 | `	/* Perform the requested operation */` |
|  11379 | 1098 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  11379 | 1099 | `	if( rc != PH7_OK ){` |
|      - | 1100 | `		/* Return FALSE */` |
|   1117 | 1101 | `		ph7_result_bool(pCtx,0);` |
|    556 | 1102 | `	}` |
|  11379 | 1103 | `	return PH7_OK;` |
|   5692 | 1104 | `}` |
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
|   1116 | 1158 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1159 | `{` |
|      - | 1160 | `	const ph7_io_stream *pStream;` |
|      - | 1161 | `	io_private *pDev;` |
|   1121 | 1162 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1163 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1164 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1166 | `		return PH7_OK;` |
|      - | 1167 | `	}` |
|      - | 1168 | `	/* Extract our private data */` |
|   1121 | 1169 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1170 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1121 | 1171 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1172 | `		/*Expecting an IO handle */` |
|    ! 0 | 1173 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1174 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1175 | `		return PH7_OK;` |
|      - | 1176 | `	}` |
|      - | 1177 | `	/* Point to the target IO stream device */` |
|   1121 | 1178 | `	pStream = pDev->pStream;` |
|   1121 | 1179 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1180 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1181 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1182 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1183 | `			);` |
|    ! 0 | 1184 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1185 | `		return PH7_OK;` |
|      - | 1186 | `	}` |
|      - | 1187 | `	/* Perform the requested operation */` |
|   1121 | 1188 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1189 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1121 | 1190 | `	MarkIOPrivateClosed(pDev);` |
|   1121 | 1191 | `	return PH7_OK;` |
|    563 | 1192 | ` }` |
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
|   1116 | 1204 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1205 | `{` |
|      - | 1206 | `	const ph7_io_stream *pStream;` |
|      - | 1207 | `	const char *zPath;` |
|      - | 1208 | `	io_private *pDev;` |
|      - | 1209 | `	int iLen,rc;` |
|   1121 | 1210 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1211 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1212 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1213 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1214 | `		return PH7_OK;` |
|      - | 1215 | `	}` |
|      - | 1216 | `	/* Extract the target path */` |
|   1121 | 1217 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1218 | `	/* Try to extract a stream */` |
|   1121 | 1219 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1121 | 1220 | `	if( pStream == 0 ){` |
|    ! 0 | 1221 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 1222 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 1223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1224 | `		return PH7_OK;` |
|      - | 1225 | `	}` |
|   1121 | 1226 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1227 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1228 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1229 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1230 | `			);` |
|    ! 0 | 1231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Allocate a new IO private instance */` |
|   1121 | 1235 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1121 | 1236 | `	if( pDev == 0 ){` |
|    ! 0 | 1237 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
|      - | 1241 | `	/* Initialize the structure */` |
|   1121 | 1242 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1243 | `	/* Open the target directory */` |
|   1121 | 1244 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1121 | 1245 | `	if( rc != PH7_OK ){` |
|      - | 1246 | `		/* IO error,return FALSE */` |
|    ! 0 | 1247 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 1248 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1249 | `	}else{` |
|      - | 1250 | `		/* Return the handle as a resource */` |
|   1121 | 1251 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1252 | `	}` |
|   1121 | 1253 | `	return PH7_OK;` |
|    563 | 1254 | `}` |
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
|   6944 | 1354 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1355 | `{` |
|      - | 1356 | `	const ph7_io_stream *pStream;` |
|      - | 1357 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6949 | 1358 | `	int use_include  = FALSE;` |
|      - | 1359 | `	const char *zFile;` |
|      - | 1360 | `	char zBuf[8192];` |
|      - | 1361 | `	void *pHandle;` |
|      - | 1362 | `	int nLen;` |
|      - | 1363 |  |
|   6949 | 1364 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1365 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1366 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1367 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1368 | `		return PH7_OK;` |
|      - | 1369 | `	}` |
|      - | 1370 | `	/* Extract the file path */` |
|   6949 | 1371 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1372 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 1373 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 1374 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 1375 | `	 * the whole file. */` |
|   6949 | 1376 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 1377 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 1378 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1379 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 1380 | `		}` |
|     11 | 1381 | `	}` |
|      - | 1382 | `	/* Point to the target IO stream device */` |
|   6945 | 1383 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6945 | 1384 | `	if( pStream == 0 ){` |
|    ! 0 | 1385 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1387 | `		return PH7_OK;` |
|      - | 1388 | `	}` |
|   6945 | 1389 | `	nMaxlen = -1;` |
|   6945 | 1390 | `	if( nArg > 1 ){` |
|     25 | 1391 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     12 | 1392 | `	}` |
|      - | 1393 | `	/* Try to open the file in read-only mode */` |
|   6945 | 1394 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6945 | 1395 | `	if( pHandle == 0 ){` |
|      3 | 1396 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1397 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1398 | `		return PH7_OK;` |
|      - | 1399 | `	}` |
|   6943 | 1400 | `	if( nArg > 3 ){` |
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
|   6943 | 1419 | `	nRead = 0;` |
|  13861 | 1420 | `	while( nMaxlen != 0 ){` |
|      - | 1421 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 1422 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 1423 | `		 * otherwise overshoot and append past $length. */` |
|  13857 | 1424 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  13857 | 1425 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 1426 | `			nAsk = nMaxlen - nRead;` |
|      8 | 1427 | `		}` |
|  13857 | 1428 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  13857 | 1429 | `		if( n < 1 ){` |
|      - | 1430 | `			/* EOF or IO error,break immediately */` |
|   6925 | 1431 | `			break;` |
|      - | 1432 | `		}` |
|      - | 1433 | `		/* Append data */` |
|   6937 | 1434 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 1435 | `		/* Increment read counter */` |
|   6937 | 1436 | `		nRead += n;` |
|   6937 | 1437 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 1438 | `			/* Read limit reached */` |
|     15 | 1439 | `			break;` |
|      - | 1440 | `		}` |
|      5 | 1441 | `	}` |
|      - | 1442 | `	/* Close the stream */` |
|   6943 | 1443 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1444 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 1445 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 1446 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   6943 | 1447 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     18 | 1448 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 1449 | `	}` |
|   6943 | 1450 | `	return PH7_OK;` |
|   3477 | 1451 | `}` |
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
|  13510 | 1471 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1472 | `{` |
|  13515 | 1473 | `	int use_include  = FALSE;` |
|      - | 1474 | `	const ph7_io_stream *pStream;` |
|      - | 1475 | `	const char *zFile;` |
|      - | 1476 | `	const char *zData;` |
|      - | 1477 | `	int iOpenFlags;` |
|      - | 1478 | `	void *pHandle;` |
|      - | 1479 | `	int iFlags;` |
|      - | 1480 | `	int nLen;` |
|      - | 1481 |  |
|  13515 | 1482 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1483 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1484 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1486 | `		return PH7_OK;` |
|      - | 1487 | `	}` |
|      - | 1488 | `	/* Extract the file path */` |
|  13515 | 1489 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1490 | `	/* Point to the target IO stream device */` |
|  13515 | 1491 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13515 | 1492 | `	if( pStream == 0 ){` |
|    ! 0 | 1493 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1495 | `		return PH7_OK;` |
|      - | 1496 | `	}` |
|      - | 1497 | `	/* Data to write */` |
|  13515 | 1498 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1499 | `	/* Try to open the file in read-write mode */` |
|  13515 | 1500 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 1501 | `	/* Extract the flags */` |
|  13515 | 1502 | `	iFlags = 0;` |
|  13515 | 1503 | `	if( nArg > 2 ){` |
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
|  20270 | 1517 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6755 | 1518 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13515 | 1519 | `	if( pHandle == 0 ){` |
|    ! 0 | 1520 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1521 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1522 | `		return PH7_OK;` |
|      - | 1523 | `	}` |
|  13515 | 1524 | `	if( nLen < 1 ){` |
|      - | 1525 | `		/* Empty data, file is created/truncated */` |
|     10 | 1526 | `		ph7_result_int64(pCtx,0);` |
|     10 | 1527 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     10 | 1528 | `		return PH7_OK;` |
|      - | 1529 | `	}` |
|  13507 | 1530 | `	if( pStream->xWrite ){` |
|      - | 1531 | `		ph7_int64 n;` |
|  13507 | 1532 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 1533 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 1534 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 1535 | `		}` |
|      - | 1536 | `		/* Perform the write operation */` |
|  13507 | 1537 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13507 | 1538 | `		if( n < 0 ){` |
|      - | 1539 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 1540 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1541 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 1542 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 1543 | `			ph7_result_bool(pCtx,0);` |
|      1 | 1544 | `		}else{` |
|      - | 1545 | `			/* Total number of bytes written */` |
|  13507 | 1546 | `			ph7_result_int64(pCtx,n);` |
|      - | 1547 | `		}` |
|   6756 | 1548 | `	}else{` |
|      - | 1549 | `		/* Read-only stream */` |
|    ! 0 | 1550 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 1551 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 1552 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 1553 | `			);` |
|    ! 0 | 1554 | `		ph7_result_bool(pCtx,0);` |
|      - | 1555 | `	}` |
|      - | 1556 | `	/* Close the handle */` |
|  13507 | 1557 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13507 | 1558 | `	return PH7_OK;` |
|   6760 | 1559 | `}` |
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
|      2 | 1580 | `{` |
|      - | 1581 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 1582 | `	ph7_value *pArray,*pLine;` |
|      - | 1583 | `	const ph7_io_stream *pStream;` |
|     44 | 1584 | `	int use_include = 0;` |
|      - | 1585 | `	io_private *pDev;` |
|      - | 1586 | `	ph7_int64 n;` |
|      - | 1587 | `	int iFlags;` |
|      - | 1588 | `	int nLen;` |
|      - | 1589 |  |
|     44 | 1590 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1591 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1592 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1594 | `		return PH7_OK;` |
|      - | 1595 | `	}` |
|     44 | 1596 | `	iFlags = 0;` |
|     44 | 1597 | `	if( nArg > 1 ){` |
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
|     34 | 1616 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1617 | `	/* Point to the target IO stream device */` |
|     34 | 1618 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     34 | 1619 | `	if( pStream == 0 ){` |
|    ! 0 | 1620 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1621 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1622 | `		return PH7_OK;` |
|      - | 1623 | `	}` |
|      - | 1624 | `	/* Allocate a new IO private instance */` |
|     34 | 1625 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     34 | 1626 | `	if( pDev == 0 ){` |
|    ! 0 | 1627 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1628 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1629 | `		return PH7_OK;` |
|      - | 1630 | `	}` |
|      - | 1631 | `	/* Initialize the structure */` |
|     34 | 1632 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     34 | 1633 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 1634 | `		use_include = TRUE;` |
|      1 | 1635 | `	}` |
|      - | 1636 | `	/* Create the array and the working value */` |
|     34 | 1637 | `	pArray = ph7_context_new_array(pCtx);` |
|     34 | 1638 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     34 | 1639 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 1640 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Try to open the file in read-only mode */` |
|     34 | 1645 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     34 | 1646 | `	if( pDev->pHandle == 0 ){` |
|      9 | 1647 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      9 | 1648 | `		ph7_result_bool(pCtx,0);` |
|      - | 1649 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 1650 | `		 * as soon we return from this function.` |
|      - | 1651 | `		 */` |
|      9 | 1652 | `		return PH7_OK;` |
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
|     23 | 1703 | `}` |
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
|     28 | 2246 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 2247 | `{` |
|     29 | 2248 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 2249 | `	ph7_int64 n;` |
|      - | 2250 | `	/* Write the formatted data */` |
|     29 | 2251 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     29 | 2252 | `	if( n < 1 ){` |
|    ! 0 | 2253 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 2254 | `		/* IO error,abort immediately */` |
|    ! 0 | 2255 | `		return SXERR_ABORT;` |
|      - | 2256 | `	}` |
|      - | 2257 | `	/* Increment counter */` |
|     29 | 2258 | `	pFdata->nCount += n;` |
|     29 | 2259 | `	return PH7_OK;` |
|     15 | 2260 | `}` |
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
|     18 | 2272 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2273 | `{` |
|      - | 2274 | `	fprintf_data sFdata;` |
|      - | 2275 | `	const char *zFormat;` |
|      - | 2276 | `	io_private *pDev;` |
|      - | 2277 | `	int nLen;` |
|     19 | 2278 | `	if( nArg < 2 ){` |
|    ! 0 | 2279 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2280 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2281 | `		return PH7_OK;` |
|      - | 2282 | `	}` |
|      - | 2283 | `	{` |
|      - | 2284 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 2285 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 2286 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 2287 | `			return rcs;` |
|      - | 2288 | `		}` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Extract our private data */` |
|     19 | 2291 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2292 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 2293 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2294 | `		/*Expecting an IO handle */` |
|    ! 0 | 2295 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2296 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2297 | `		return PH7_OK;` |
|      - | 2298 | `	}` |
|      - | 2299 | `	/* Point to the target IO stream device */` |
|     19 | 2300 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2303 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2304 | `			);` |
|    ! 0 | 2305 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2306 | `		return PH7_OK;` |
|      - | 2307 | `	}` |
|      - | 2308 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 2309 | `	{` |
|     19 | 2310 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 2311 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2312 | `			return rcf;` |
|      - | 2313 | `		}` |
|      - | 2314 | `	}` |
|      - | 2315 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 2316 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 2317 | `	if( nLen < 1 ){` |
|      - | 2318 | `		/* Empty string,return zero */` |
|    ! 0 | 2319 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2320 | `		return PH7_OK;` |
|      - | 2321 | `	}` |
|      - | 2322 | `	{` |
|      - | 2323 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 2324 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 2325 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     19 | 2326 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     19 | 2327 | `		if( rcv != PH7_OK ){` |
|      3 | 2328 | `			return rcv;` |
|      - | 2329 | `		}` |
|      - | 2330 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 2331 | `		 * before any output; propagate the throw status verbatim. */` |
|     17 | 2332 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 2333 | `		if( rcv != PH7_OK ){` |
|      5 | 2334 | `			return rcv;` |
|      - | 2335 | `		}` |
|      - | 2336 | `	}` |
|      - | 2337 | `	/* Prepare our private data */` |
|     13 | 2338 | `	sFdata.nCount = 0;` |
|     13 | 2339 | `	sFdata.pIO = pDev;` |
|      - | 2340 | `	/* Format the string */` |
|     13 | 2341 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 2342 | `	/* Return total number of bytes written */` |
|     13 | 2343 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     13 | 2344 | `	return PH7_OK;` |
|     10 | 2345 | `}` |
|      - | 2346 | `/*` |
|      - | 2347 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 2348 | ` *  Write a formatted string to a stream.` |
|      - | 2349 | ` * Parameters` |
|      - | 2350 | ` *  $handle` |
|      - | 2351 | ` *   The file pointer.` |
|      - | 2352 | ` *  $format` |
|      - | 2353 | ` *   String format (see sprintf()).` |
|      - | 2354 | ` * $args` |
|      - | 2355 | ` *   User arguments.` |
|      - | 2356 | ` * Return` |
|      - | 2357 | ` *  The length of the written string.` |
|      - | 2358 | ` */` |
|      6 | 2359 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2360 | `{` |
|      - | 2361 | `	fprintf_data sFdata;` |
|      - | 2362 | `	const char *zFormat;` |
|      - | 2363 | `	ph7_hashmap *pMap;` |
|      - | 2364 | `	io_private *pDev;` |
|      - | 2365 | `	SySet sArg;` |
|      - | 2366 | `	int n,nLen;` |
|      7 | 2367 | `	if( nArg < 3 ){` |
|    ! 0 | 2368 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2369 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2370 | `		return PH7_OK;` |
|      - | 2371 | `	}` |
|      - | 2372 | `	{` |
|      - | 2373 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 2374 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 2375 | `		if( rcs != PH7_OK ){` |
|      3 | 2376 | `			return rcs;` |
|      - | 2377 | `		}` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 2380 | `	{` |
|      5 | 2381 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 2382 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2383 | `			return rcf;` |
|      - | 2384 | `		}` |
|      - | 2385 | `	}` |
|      5 | 2386 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 2387 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 2388 | `		char zBuf[64];` |
|    ! 0 | 2389 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2390 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 2391 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 2392 | `	}` |
|      - | 2393 | `	/* Extract our private data */` |
|      5 | 2394 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2395 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2396 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2397 | `		/*Expecting an IO handle */` |
|    ! 0 | 2398 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2399 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2400 | `		return PH7_OK;` |
|      - | 2401 | `	}` |
|      - | 2402 | `	/* Point to the target IO stream device */` |
|      5 | 2403 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2404 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2405 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2406 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2407 | `			);` |
|    ! 0 | 2408 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2409 | `		return PH7_OK;` |
|      - | 2410 | `	}` |
|      - | 2411 | `	/* Extract the string format */` |
|      5 | 2412 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 2413 | `	if( nLen < 1 ){` |
|      - | 2414 | `		/* Empty string,return zero */` |
|    ! 0 | 2415 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2416 | `		return PH7_OK;` |
|      - | 2417 | `	}` |
|      - | 2418 | `	/* Point to hashmap */` |
|      5 | 2419 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 2420 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 2421 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 2422 | `	{` |
|      5 | 2423 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 2424 | `		if( rcc != PH7_OK ){` |
|      3 | 2425 | `			return rcc;` |
|      - | 2426 | `		}` |
|      - | 2427 | `	}` |
|      - | 2428 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 2429 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 2430 | `	{` |
|      3 | 2431 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 2432 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 2433 | `			return rcv;` |
|      - | 2434 | `		}` |
|      - | 2435 | `	}` |
|      - | 2436 | `	/* Extract arguments from the hashmap */` |
|      3 | 2437 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 2438 | `	/* Prepare our private data */` |
|      3 | 2439 | `	sFdata.nCount = 0;` |
|      3 | 2440 | `	sFdata.pIO = pDev;` |
|      - | 2441 | `	/* Format the string */` |
|      3 | 2442 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 2443 | `	/* Return total number of bytes written*/` |
|      3 | 2444 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 2445 | `	SySetRelease(&sArg);` |
|      3 | 2446 | `	return PH7_OK;` |
|      4 | 2447 | `}` |
|      - | 2448 | `/*` |
|      - | 2449 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 2450 | ` * According to the PHP reference manual:` |
|      - | 2451 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 2452 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 2453 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 2454 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 2455 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 2456 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 2457 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 2458 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 2459 | ` *         exist, attempt to create it.` |
|      - | 2460 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 2461 | ` *          not exist, attempt to create it.` |
|      - | 2462 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 2463 | ` *         already exists,` |
|      - | 2464 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 2465 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 2466 | ` *         the underlying open(2) system call.` |
|      - | 2467 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 2468 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 2469 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 2470 | ` *          is positioned on the beginning of the file.` |
|      - | 2471 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 2472 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 2473 | ` *          be used after the lock is requested).` |
|      - | 2474 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 2475 | ` */` |
|    266 | 2476 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 2477 | `{` |
|    271 | 2478 | `	const char *zEnd = &zMode[nLen];` |
|    271 | 2479 | `	int iFlag = 0;` |
|      - | 2480 | `	int c;` |
|    271 | 2481 | `	if( nLen < 1 ){` |
|      - | 2482 | `		/* Open in a read-only mode */` |
|    ! 0 | 2483 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 2484 | `	}` |
|    271 | 2485 | `	c = zMode[0];` |
|    271 | 2486 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 2487 | `		/* Read-only access */` |
|     91 | 2488 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     91 | 2489 | `		zMode++; /* Advance */` |
|     91 | 2490 | `		if( zMode < zEnd ){` |
|     33 | 2491 | `			c = zMode[0];` |
|     33 | 2492 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 2493 | `				/* Read+Write access */` |
|     33 | 2494 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|     16 | 2495 | `			}` |
|     21 | 2496 | `		}` |
|    226 | 2497 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 2498 | `		/* Overwrite mode.` |
|      - | 2499 | `		 * If the file does not exists,try to create it` |
|      - | 2500 | `		 */` |
|     42 | 2501 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     42 | 2502 | `		zMode++; /* Advance */` |
|     42 | 2503 | `		if( zMode < zEnd ){` |
|      7 | 2504 | `			c = zMode[0];` |
|      7 | 2505 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2506 | `				/* Read+Write access */` |
|      7 | 2507 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      7 | 2508 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      3 | 2509 | `			}` |
|      5 | 2510 | `		}` |
|    162 | 2511 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 2512 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 2513 | `		 * Create the file if it does not exists.` |
|      - | 2514 | `		 */` |
|    ! 0 | 2515 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2516 | `		zMode++; /* Advance */` |
|    ! 0 | 2517 | `		if( zMode < zEnd ){` |
|    ! 0 | 2518 | `			c = zMode[0];` |
|    ! 0 | 2519 | `			if( c == '+' ){` |
|      - | 2520 | `				/* Read-Write access */` |
|    ! 0 | 2521 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2522 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2523 | `			}` |
|    ! 0 | 2524 | `		}` |
|    142 | 2525 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 2526 | `		/* Exclusive access.` |
|      - | 2527 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 2528 | `		 * Otherwise create a new file.` |
|      - | 2529 | `		 */` |
|    142 | 2530 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    142 | 2531 | `		zMode++; /* Advance */` |
|    142 | 2532 | `		if( zMode < zEnd ){` |
|    ! 0 | 2533 | `			c = zMode[0];` |
|    ! 0 | 2534 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2535 | `				/* Read-Write access */` |
|    ! 0 | 2536 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2537 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2538 | `			}` |
|      2 | 2539 | `		}` |
|     70 | 2540 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 2541 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 2542 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2543 | `		zMode++; /* Advance */` |
|    ! 0 | 2544 | `		if( zMode < zEnd ){` |
|    ! 0 | 2545 | `			c = zMode[0];` |
|    ! 0 | 2546 | `			if( c == '+' ){` |
|      - | 2547 | `				/* Read-Write access */` |
|    ! 0 | 2548 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2549 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2550 | `			}` |
|    ! 0 | 2551 | `		}` |
|    ! 0 | 2552 | `	}else{` |
|      - | 2553 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 2554 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 2555 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 2556 | `	}` |
|    309 | 2557 | `	while( zMode < zEnd ){` |
|     39 | 2558 | `		c = zMode[0];` |
|     39 | 2559 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 2560 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2561 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     39 | 2562 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 2563 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 2564 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2565 | `		}` |
|     39 | 2566 | `		zMode++;` |
|      1 | 2567 | `	}` |
|    271 | 2568 | `	return iFlag;` |
|    138 | 2569 | `}` |
|      - | 2570 | `/*` |
|      - | 2571 | ` * Initialize the IO private structure.` |
|      - | 2572 | ` */` |
|   5692 | 2573 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 2574 | `{` |
|   5697 | 2575 | `	pOut->pStream = pStream;` |
|   5697 | 2576 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5697 | 2577 | `	pOut->nOfft = 0;` |
|      - | 2578 | `	/* Set the magic number */` |
|   5697 | 2579 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5697 | 2580 | `}` |
|      - | 2581 | `/*` |
|      - | 2582 | ` * Release the IO private structure.` |
|      - | 2583 | ` */` |
|     24 | 2584 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 2585 | `{` |
|     25 | 2586 | `	SyBlobRelease(&pDev->sBuffer);` |
|     25 | 2587 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 2588 | `	/* Release the whole structure */` |
|     25 | 2589 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     25 | 2590 | `}` |
|      - | 2591 | `/*` |
|      - | 2592 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 2593 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 2594 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 2595 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 2596 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 2597 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 2598 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 2599 | ` */` |
|   5606 | 2600 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 2601 | `{` |
|   5611 | 2602 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5611 | 2603 | `	pDev->pHandle = 0;` |
|   5611 | 2604 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5611 | 2605 | `}` |
|      - | 2606 | `/*` |
|      - | 2607 | ` * Reset the IO private structure.` |
|      - | 2608 | ` */` |
|     78 | 2609 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 2610 | `{` |
|     80 | 2611 | `	SyBlobReset(&pDev->sBuffer);` |
|     80 | 2612 | `	pDev->nOfft = 0;` |
|     80 | 2613 | `}` |
|      - | 2614 | `/* Forward declaration */` |
|      - | 2615 |  |
|      - | 2616 | `/*` |
|      - | 2617 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 2618 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 2619 | ` * Parameters` |
|      - | 2620 | ` *  $filename` |
|      - | 2621 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 2622 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 2623 | ` *   then a regular file is assumed.` |
|      - | 2624 | ` *  $mode` |
|      - | 2625 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 2626 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 2627 | ` *   modes.` |
|      - | 2628 | ` *  $use_include_path` |
|      - | 2629 | ` *   You can use the optional second parameter and set it to` |
|      - | 2630 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2631 | ` *  $context` |
|      - | 2632 | ` *   A context stream resource.` |
|      - | 2633 | ` * Return` |
|      - | 2634 | ` *  File handle on success or FALSE on failure.` |
|      - | 2635 | ` */` |
|      - | 2636 | `/*` |
|      - | 2637 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 2638 | ` *                                  int $offset = -1)` |
|      - | 2639 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 2640 | ` */` |
|     40 | 2641 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2642 | `{` |
|      - | 2643 | `	const ph7_io_stream *pStream;` |
|      - | 2644 | `	io_private *pDev;` |
|     41 | 2645 | `	ph7_int64 nMax = -1;` |
|      - | 2646 | `	char zBuf[4096];` |
|      - | 2647 | `	ph7_int64 nRead;` |
|     41 | 2648 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2649 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2650 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2651 | `		return PH7_OK;` |
|      - | 2652 | `	}` |
|     41 | 2653 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     41 | 2654 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2655 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|     41 | 2659 | `	pStream = pDev->pStream;` |
|     41 | 2660 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 2661 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2662 | `		return PH7_OK;` |
|      - | 2663 | `	}` |
|     41 | 2664 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2665 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 2666 | `		 * default) means "read until EOF". */` |
|      9 | 2667 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      9 | 2668 | `		if( nMax < -1 ){` |
|      3 | 2669 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2670 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 2671 | `		}` |
|      3 | 2672 | `	}` |
|     39 | 2673 | `	if( nArg > 2 ){` |
|      5 | 2674 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 2675 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 2676 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 2677 | `		}` |
|      2 | 2678 | `	}` |
|     39 | 2679 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     72 | 2680 | `	while( nMax != 0 ){` |
|     70 | 2681 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     70 | 2682 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 2683 | `			nAsk = nMax;` |
|      1 | 2684 | `		}` |
|     70 | 2685 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     70 | 2686 | `		if( nRead < 1 ){` |
|     37 | 2687 | `			break;` |
|      - | 2688 | `		}` |
|     34 | 2689 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     34 | 2690 | `		if( nMax > 0 ){` |
|      3 | 2691 | `			nMax -= nRead;` |
|      1 | 2692 | `		}` |
|      1 | 2693 | `	}` |
|     39 | 2694 | `	return PH7_OK;` |
|     21 | 2695 | `}` |
|      - | 2696 | `/*` |
|      - | 2697 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 2698 | ` */` |
|      4 | 2699 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2700 | `{` |
|      - | 2701 | `	ph7_value *pArr,*pV;` |
|      - | 2702 | `	ph7_io_stream **apDev;` |
|      - | 2703 | `	sxu32 n;` |
|      2 | 2704 | `	SXUNUSED(nArg);` |
|      2 | 2705 | `	SXUNUSED(apArg);` |
|      6 | 2706 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 2707 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 2708 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2709 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2710 | `		return PH7_OK;` |
|      - | 2711 | `	}` |
|      6 | 2712 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 2713 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 2714 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 2715 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 2716 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 2717 | `	}` |
|      6 | 2718 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 2719 | `	return PH7_OK;` |
|      4 | 2720 | `}` |
|      - | 2721 | `/*` |
|      - | 2722 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 2723 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 2724 | ` */` |
|      2 | 2725 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2726 | `{` |
|      - | 2727 | `	io_private *pDev;` |
|      - | 2728 | `	ph7_value *pArr,*pV;` |
|      3 | 2729 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2730 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2731 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2732 | `		return PH7_OK;` |
|      - | 2733 | `	}` |
|      3 | 2734 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 2735 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2736 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2737 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2738 | `		return PH7_OK;` |
|      - | 2739 | `	}` |
|      3 | 2740 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 2741 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 2742 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2743 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|      3 | 2746 | `	ph7_value_bool(pV,0);` |
|      3 | 2747 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 2748 | `	ph7_value_bool(pV,1);` |
|      3 | 2749 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 2750 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 2751 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 2752 | `	ph7_value_bool(pV,0);` |
|      3 | 2753 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 2754 | `	ph7_value_int(pV,0);` |
|      3 | 2755 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 2756 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2757 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 2758 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2759 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2760 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 2761 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2762 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 2763 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 2764 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 2765 | `	return PH7_OK;` |
|      2 | 2766 | `}` |
|      - | 2767 | `/*` |
|      - | 2768 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 2769 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 2770 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 2771 | ` * resource, options unconsumed).` |
|      - | 2772 | ` */` |
|      2 | 2773 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2774 | `{` |
|      3 | 2775 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 2776 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 2777 | `	}else{` |
|    ! 0 | 2778 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 2779 | `		if( pArr == 0 ){` |
|    ! 0 | 2780 | `			ph7_result_null(pCtx);` |
|    ! 0 | 2781 | `			return PH7_OK;` |
|      - | 2782 | `		}` |
|    ! 0 | 2783 | `		ph7_result_value(pCtx,pArr);` |
|      - | 2784 | `	}` |
|      3 | 2785 | `	return PH7_OK;` |
|      2 | 2786 | `}` |
|      - | 2787 | `/*` |
|      - | 2788 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 2789 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 2790 | ` */` |
|      - | 2791 | `#ifdef PH7_ENABLE_NET` |
|      - | 2792 | `typedef struct sock_private sock_private;` |
|      - | 2793 | `struct sock_private` |
|      - | 2794 | `{` |
|      - | 2795 | `	ph7_vm *pVm;` |
|      - | 2796 | `	ph7_socket sock;` |
|      - | 2797 | `	int bEof;` |
|      - | 2798 | `};` |
|     13 | 2799 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 2800 | `{` |
|     13 | 2801 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2802 | `	int n;` |
|     13 | 2803 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      1 | 2804 | `		return 0;` |
|      - | 2805 | `	}` |
|     12 | 2806 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     12 | 2807 | `	if( n <= 0 ){` |
|      4 | 2808 | `		pSock->bEof = 1;` |
|      4 | 2809 | `		return 0;` |
|      - | 2810 | `	}` |
|      8 | 2811 | `	return (ph7_int64)n;` |
|      5 | 2812 | `}` |
|      4 | 2813 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 2814 | `{` |
|      4 | 2815 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2816 | `	int n;` |
|      4 | 2817 | `	if( pSock == 0 ){` |
|    ! 0 | 2818 | `		return -1;` |
|      - | 2819 | `	}` |
|      4 | 2820 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 2821 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 2822 | `}` |
|      4 | 2823 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 2824 | `{` |
|      4 | 2825 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 2826 | `	if( pSock == 0 ){` |
|    ! 0 | 2827 | `		return;` |
|      - | 2828 | `	}` |
|      4 | 2829 | `	PH7_NetClose(pSock->sock);` |
|      4 | 2830 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 2831 | `}` |
|      - | 2832 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 2833 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 2834 | `{` |
|      - | 2835 | `	sock_private *pSock;` |
|      - | 2836 | `	ph7_socket sock;` |
|      - | 2837 | `	char zHost[256];` |
|      - | 2838 | `	const char *zColon;` |
|    ! 0 | 2839 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 2840 | `	const char *zErr = "";` |
|    ! 0 | 2841 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 2842 | `	SXUNUSED(iMode);` |
|    ! 0 | 2843 | `	if( pVm == 0 ){` |
|    ! 0 | 2844 | `		return -1;` |
|      - | 2845 | `	}` |
|    ! 0 | 2846 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 2847 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 2848 | `		zColon--;` |
|    ! 0 | 2849 | `	}` |
|    ! 0 | 2850 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 2851 | `		return -1;` |
|      - | 2852 | `	}` |
|      - | 2853 | `	{` |
|    ! 0 | 2854 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 2855 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 2856 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 2857 | `		}` |
|    ! 0 | 2858 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 2859 | `		zHost[n] = 0;` |
|      - | 2860 | `	}` |
|      - | 2861 | `	{` |
|    ! 0 | 2862 | `		sxi32 iTmp = 0;` |
|    ! 0 | 2863 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 2864 | `		iPort = (int)iTmp;` |
|      - | 2865 | `	}` |
|    ! 0 | 2866 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 2867 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 2868 | `		return -1;` |
|      - | 2869 | `	}` |
|    ! 0 | 2870 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 2871 | `	if( pSock == 0 ){` |
|    ! 0 | 2872 | `		PH7_NetClose(sock);` |
|    ! 0 | 2873 | `		return -1;` |
|      - | 2874 | `	}` |
|    ! 0 | 2875 | `	pSock->pVm = pVm;` |
|    ! 0 | 2876 | `	pSock->sock = sock;` |
|    ! 0 | 2877 | `	pSock->bEof = 0;` |
|    ! 0 | 2878 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 2879 | `	return PH7_OK;` |
|    ! 0 | 2880 | `}` |
|      - | 2881 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 2882 | `	"tcp",` |
|      - | 2883 | `	PH7_IO_STREAM_VERSION,` |
|      - | 2884 | `	SockStreamData_Open, /* xOpen */` |
|      - | 2885 | `	0,   /* xOpenDir */` |
|      - | 2886 | `	SockStreamData_Close,/* xClose */` |
|      - | 2887 | `	0,  /* xCloseDir */` |
|      - | 2888 | `	SockStreamData_Read, /* xRead */` |
|      - | 2889 | `	0,  /* xReadDir */` |
|      - | 2890 | `	SockStreamData_Write,/* xWrite */` |
|      - | 2891 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 2892 | `	0,  /* xLock */` |
|      - | 2893 | `	0,  /* xRewindDir */` |
|      - | 2894 | `	0,  /* xTell */` |
|      - | 2895 | `	0,  /* xTrunc */` |
|      - | 2896 | `	0,  /* xSync */` |
|      - | 2897 | `	0   /* xStat */` |
|      - | 2898 | `};` |
|      - | 2899 | `#endif /* PH7_ENABLE_NET */` |
|      - | 2900 | `/*` |
|      - | 2901 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 2902 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 2903 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 2904 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 2905 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 2906 | ` * php streamWrapper protocol method on it.` |
|      - | 2907 | ` */` |
|      - | 2908 | `#define PHL_UWRAP_MAX 8` |
|      - | 2909 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 2910 | `struct uwrap_slot` |
|      - | 2911 | `{` |
|      - | 2912 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 2913 | `	char zScheme[32];         /* protocol name */` |
|      - | 2914 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 2915 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 2916 | `};` |
|      - | 2917 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 2918 | `struct uwrap_handle` |
|      - | 2919 | `{` |
|      - | 2920 | `	ph7_vm *pVm;` |
|      - | 2921 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 2922 | `	int iSlot;` |
|      - | 2923 | `	int bEof;` |
|      - | 2924 | `};` |
|      - | 2925 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 2926 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 2927 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 2928 | `	ph7_value *pResult)` |
|      1 | 2929 | `{` |
|      - | 2930 | `	ph7_class_method *pMeth;` |
|     27 | 2931 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 2932 | `		return -1;` |
|      - | 2933 | `	}` |
|     27 | 2934 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 2935 | `	if( pMeth == 0 ){` |
|    ! 0 | 2936 | `		return -1;` |
|      - | 2937 | `	}` |
|     27 | 2938 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 2939 | `		return -1;` |
|      - | 2940 | `	}` |
|     27 | 2941 | `	return 0;` |
|     14 | 2942 | `}` |
|      8 | 2943 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 2944 | `{` |
|      9 | 2945 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2946 | `	ph7_value sArg,sRet;` |
|      - | 2947 | `	const char *zData;` |
|      9 | 2948 | `	int nData = 0;` |
|      9 | 2949 | `	ph7_int64 n = 0;` |
|      9 | 2950 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 2951 | `		return 0;` |
|      - | 2952 | `	}` |
|      9 | 2953 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 2954 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 2955 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 2956 | `	{` |
|      - | 2957 | `		ph7_value *apArg[1];` |
|      9 | 2958 | `		apArg[0] = &sArg;` |
|      9 | 2959 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 2960 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 2961 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2962 | `			return -1;` |
|      - | 2963 | `		}` |
|      - | 2964 | `	}` |
|      9 | 2965 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 2966 | `	if( nData > 0 ){` |
|      7 | 2967 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 2968 | `			nData = (int)nRead;` |
|    ! 0 | 2969 | `		}` |
|      7 | 2970 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 2971 | `		n = nData;` |
|      4 | 2972 | `	}else{` |
|      3 | 2973 | `		pH->bEof = 1;` |
|      - | 2974 | `	}` |
|      9 | 2975 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 2976 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 2977 | `	return n;` |
|      5 | 2978 | `}` |
|      2 | 2979 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 2980 | `{` |
|      3 | 2981 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2982 | `	ph7_value sArg,sRet;` |
|      - | 2983 | `	ph7_int64 n;` |
|      3 | 2984 | `	if( pH == 0 ){` |
|    ! 0 | 2985 | `		return -1;` |
|      - | 2986 | `	}` |
|      3 | 2987 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 2988 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 2989 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 2990 | `	{` |
|      - | 2991 | `		ph7_value *apArg[1];` |
|      3 | 2992 | `		apArg[0] = &sArg;` |
|      3 | 2993 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 2994 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 2995 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2996 | `			return -1;` |
|      - | 2997 | `		}` |
|      - | 2998 | `	}` |
|      3 | 2999 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3000 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 3001 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3002 | `	return n;` |
|      2 | 3003 | `}` |
|      2 | 3004 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 3005 | `{` |
|      3 | 3006 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3007 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 3008 | `	ph7_value *apArg[2];` |
|      - | 3009 | `	int rc;` |
|      3 | 3010 | `	if( pH == 0 ){` |
|    ! 0 | 3011 | `		return -1;` |
|      - | 3012 | `	}` |
|      3 | 3013 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 3014 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 3015 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3016 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 3017 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 3018 | `	apArg[0] = &sOfft;` |
|      3 | 3019 | `	apArg[1] = &sWhence;` |
|      3 | 3020 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 3021 | `	if( rc == 0 ){` |
|      3 | 3022 | `		pH->bEof = 0;` |
|      3 | 3023 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 3024 | `	}` |
|      3 | 3025 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 3026 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 3027 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3028 | `	return rc;` |
|      2 | 3029 | `}` |
|      2 | 3030 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 3031 | `{` |
|      3 | 3032 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3033 | `	ph7_value sRet;` |
|      - | 3034 | `	ph7_int64 n;` |
|      3 | 3035 | `	if( pH == 0 ){` |
|    ! 0 | 3036 | `		return -1;` |
|      - | 3037 | `	}` |
|      3 | 3038 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3039 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 3040 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3041 | `		return -1;` |
|      - | 3042 | `	}` |
|      3 | 3043 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3044 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3045 | `	return n;` |
|      2 | 3046 | `}` |
|      6 | 3047 | `static void UwrapClose(void *pHandle)` |
|      1 | 3048 | `{` |
|      7 | 3049 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 3050 | `	if( pH == 0 ){` |
|    ! 0 | 3051 | `		return;` |
|      - | 3052 | `	}` |
|      7 | 3053 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 3054 | `	if( pH->pObj ){` |
|      7 | 3055 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 3056 | `	}` |
|      7 | 3057 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 3058 | `}` |
|      - | 3059 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 3060 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 3061 | `{` |
|      7 | 3062 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 3063 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 3064 | `	ph7_class *pClass;` |
|      - | 3065 | `	uwrap_handle *pH;` |
|      - | 3066 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 3067 | `	ph7_value *apArg[4];` |
|      - | 3068 | `	int rc;` |
|      7 | 3069 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 3070 | `		return -1;` |
|      - | 3071 | `	}` |
|      7 | 3072 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 3073 | `	if( pClass == 0 ){` |
|    ! 0 | 3074 | `		return -1;` |
|      - | 3075 | `	}` |
|      7 | 3076 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 3077 | `	if( pH == 0 ){` |
|    ! 0 | 3078 | `		return -1;` |
|      - | 3079 | `	}` |
|      7 | 3080 | `	pH->pVm = pVm;` |
|      7 | 3081 | `	pH->iSlot = iSlot;` |
|      7 | 3082 | `	pH->bEof = 0;` |
|      7 | 3083 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 3084 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 3085 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3086 | `		return -1;` |
|      - | 3087 | `	}` |
|      - | 3088 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 3089 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 3090 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 3091 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 3092 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 3093 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 3094 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 3095 | `	 * it as "could not be passed by reference"). */` |
|      - | 3096 | `	{` |
|      7 | 3097 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 3098 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 3099 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3100 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3101 | `			return -1;` |
|      - | 3102 | `		}` |
|      7 | 3103 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 3104 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 3105 | `	}` |
|      - | 3106 | `	{` |
|      - | 3107 | `		SyBlob sUrl;` |
|      7 | 3108 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 3109 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 3110 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 3111 | `		SyBlobRelease(&sUrl);` |
|      - | 3112 | `	}` |
|      9 | 3113 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 3114 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 3115 | `	ph7_value_int(&sOpts,0);` |
|      7 | 3116 | `	apArg[0] = &sPath;` |
|      7 | 3117 | `	apArg[1] = &sMode;` |
|      7 | 3118 | `	apArg[2] = &sOpts;` |
|      7 | 3119 | `	apArg[3] = &sOpened;` |
|      7 | 3120 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 3121 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 3122 | `		rc = -1;` |
|    ! 0 | 3123 | `	}` |
|      7 | 3124 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 3125 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 3126 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 3127 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 3128 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 3129 | `	if( rc != 0 ){` |
|    ! 0 | 3130 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3131 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3132 | `		return -1;` |
|      - | 3133 | `	}` |
|      7 | 3134 | `	*ppHandle = (void *)pH;` |
|      7 | 3135 | `	return PH7_OK;` |
|      4 | 3136 | `}` |
|      - | 3137 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 3138 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 3139 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 3140 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 3141 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 3142 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 3143 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 3144 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 3145 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 3146 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 3147 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 3148 | `PHL_UWRAP_THUNK(7)` |
|      - | 3149 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 3150 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 3151 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 3152 | `};` |
|      - | 3153 | `/*` |
|      - | 3154 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 3155 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 3156 | ` */` |
|      2 | 3157 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3158 | `{` |
|      - | 3159 | `	const char *zScheme,*zClass;` |
|      3 | 3160 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 3161 | `	if( nArg < 2 ){` |
|    ! 0 | 3162 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3163 | `		return PH7_OK;` |
|      - | 3164 | `	}` |
|      3 | 3165 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3166 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 3167 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 3168 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 3169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3170 | `		return PH7_OK;` |
|      - | 3171 | `	}` |
|      - | 3172 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 3173 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 3174 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 3175 | `	{` |
|      3 | 3176 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 3177 | `		sxu32 n;` |
|     11 | 3178 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 3179 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 3180 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 3181 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3182 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 3183 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3184 | `				return PH7_OK;` |
|      - | 3185 | `			}` |
|      5 | 3186 | `		}` |
|      - | 3187 | `	}` |
|      3 | 3188 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 3189 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 3190 | `			iFree = i;` |
|      3 | 3191 | `			break;` |
|      - | 3192 | `		}` |
|    ! 0 | 3193 | `	}` |
|      3 | 3194 | `	if( iFree < 0 ){` |
|    ! 0 | 3195 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3196 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 3197 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3198 | `		return PH7_OK;` |
|      - | 3199 | `	}` |
|      - | 3200 | `	{` |
|      3 | 3201 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 3202 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 3203 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 3204 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 3205 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 3206 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 3207 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 3208 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 3209 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 3210 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 3211 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 3212 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 3213 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 3214 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 3215 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 3216 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 3217 | `	}` |
|      3 | 3218 | `	ph7_result_bool(pCtx,1);` |
|      3 | 3219 | `	return PH7_OK;` |
|      2 | 3220 | `}` |
|      2 | 3221 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3222 | `{` |
|      - | 3223 | `	const char *zScheme;` |
|      - | 3224 | `	int nScheme,i;` |
|      3 | 3225 | `	if( nArg < 1 ){` |
|    ! 0 | 3226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3227 | `		return PH7_OK;` |
|      - | 3228 | `	}` |
|      3 | 3229 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3230 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 3231 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 3232 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 3233 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 3234 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 3235 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 3236 | `			 * what unregister means to a script — recorded. */` |
|      3 | 3237 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 3238 | `			ph7_result_bool(pCtx,1);` |
|      3 | 3239 | `			return PH7_OK;` |
|      - | 3240 | `		}` |
|    ! 0 | 3241 | `	}` |
|    ! 0 | 3242 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 3243 | `	return PH7_OK;` |
|      2 | 3244 | `}` |
|      - | 3245 | `#ifdef PH7_ENABLE_NET` |
|      - | 3246 | `/*` |
|      - | 3247 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 3248 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 3249 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 3250 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 3251 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 3252 | ` */` |
|      6 | 3253 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3254 | `{` |
|      6 | 3255 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 3256 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 3257 | `	const char *zTarget,*zErr = "";` |
|      - | 3258 | `	char zHost[256];` |
|      6 | 3259 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 3260 | `	ph7_socket sock;` |
|      - | 3261 | `	io_private *pDev;` |
|      - | 3262 | `	sock_private *pSock;` |
|      6 | 3263 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 3264 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 3265 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 3266 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 3267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3268 | `		return PH7_OK;` |
|      - | 3269 | `	}` |
|      6 | 3270 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 3271 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 3272 | `	{` |
|      6 | 3273 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 3274 | `		const char *zSep = 0;` |
|     32 | 3275 | `		while( z < zEnd - 2 ){` |
|     30 | 3276 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 3277 | `				zSep = z;` |
|      4 | 3278 | `				break;` |
|      - | 3279 | `			}` |
|     26 | 3280 | `			z++;` |
|    ! 0 | 3281 | `		}` |
|      6 | 3282 | `		if( zSep ){` |
|      4 | 3283 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 3284 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3285 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 3286 | `					nTarget,zTarget);` |
|    ! 0 | 3287 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3288 | `				return PH7_OK;` |
|      - | 3289 | `			}` |
|      4 | 3290 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 3291 | `			zTarget = zSep + 3;` |
|      2 | 3292 | `		}` |
|      - | 3293 | `	}` |
|      - | 3294 | `	/* host[:port] */` |
|      - | 3295 | `	{` |
|      6 | 3296 | `		int i = nTarget - 1;` |
|      6 | 3297 | `		int nHost = nTarget;` |
|     48 | 3298 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 3299 | `			i--;` |
|    ! 0 | 3300 | `		}` |
|      6 | 3301 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 3302 | `			sxi32 iTmp = 0;` |
|      2 | 3303 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 3304 | `			iPort = (int)iTmp;` |
|      2 | 3305 | `			nHost = i;` |
|      1 | 3306 | `		}` |
|      6 | 3307 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 3308 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 3309 | `		}` |
|      6 | 3310 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 3311 | `		zHost[nHost] = 0;` |
|      - | 3312 | `	}` |
|      6 | 3313 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 3314 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 3315 | `	}` |
|      6 | 3316 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 3317 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 3318 | `		if( rTimeout > 0 ){` |
|      6 | 3319 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 3320 | `		}` |
|      3 | 3321 | `	}` |
|      6 | 3322 | `	if( iPort < 0 ){` |
|    ! 0 | 3323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3324 | `		return PH7_OK;` |
|      - | 3325 | `	}` |
|      6 | 3326 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 3327 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 3328 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 3329 | `		{` |
|      2 | 3330 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 3331 | `			if( pTmp ){` |
|      2 | 3332 | `				if( nArg > iArgErrno ){` |
|      2 | 3333 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 3334 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 3335 | `				}` |
|      2 | 3336 | `				if( nArg > iArgErrstr ){` |
|      2 | 3337 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 3338 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 3339 | `				}` |
|      1 | 3340 | `			}` |
|      - | 3341 | `		}` |
|      - | 3342 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 3343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 3344 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 3345 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3346 | `		return PH7_OK;` |
|      - | 3347 | `	}` |
|      - | 3348 | `	{` |
|      4 | 3349 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 3350 | `		if( pTmp ){` |
|      4 | 3351 | `			if( nArg > iArgErrno ){` |
|      4 | 3352 | `				ph7_value_int(pTmp,0);` |
|      4 | 3353 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 3354 | `			}` |
|      4 | 3355 | `			if( nArg > iArgErrstr ){` |
|      4 | 3356 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 3357 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 3358 | `			}` |
|      2 | 3359 | `		}` |
|      - | 3360 | `	}` |
|      - | 3361 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 3362 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 3363 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 3364 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 3365 | `		PH7_NetClose(sock);` |
|    ! 0 | 3366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3367 | `		return PH7_OK;` |
|      - | 3368 | `	}` |
|      4 | 3369 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 3370 | `	pSock->sock = sock;` |
|      4 | 3371 | `	pSock->bEof = 0;` |
|      4 | 3372 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 3373 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 3374 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 3375 | `	return PH7_OK;` |
|      3 | 3376 | `}` |
|      - | 3377 | `#endif /* PH7_ENABLE_NET */` |
|    266 | 3378 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3379 | `{` |
|      - | 3380 | `	const ph7_io_stream *pStream;` |
|      - | 3381 | `	const char *zUri,*zMode;` |
|      - | 3382 | `	ph7_value *pResource;` |
|      - | 3383 | `	io_private *pDev;` |
|      - | 3384 | `	int iLen,imLen;` |
|      - | 3385 | `	int iOpenFlags;` |
|    271 | 3386 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3387 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3388 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 3389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3390 | `		return PH7_OK;` |
|      - | 3391 | `	}` |
|      - | 3392 | `	/* Extract the URI and the desired access mode */` |
|    271 | 3393 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    271 | 3394 | `	if( nArg > 1 ){` |
|    271 | 3395 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    138 | 3396 | `	}else{` |
|      - | 3397 | `		/* Set a default read-only mode */` |
|    ! 0 | 3398 | `		zMode = "r";` |
|    ! 0 | 3399 | `		imLen = (int)sizeof(char);` |
|      - | 3400 | `	}` |
|      - | 3401 | `	/* Try to extract a stream */` |
|    271 | 3402 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    271 | 3403 | `	if( pStream == 0 ){` |
|    ! 0 | 3404 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3405 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 3406 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3407 | `		return PH7_OK;` |
|      - | 3408 | `	}` |
|      - | 3409 | `	/* Allocate a new IO private instance */` |
|    271 | 3410 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    271 | 3411 | `	if( pDev == 0 ){` |
|    ! 0 | 3412 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3414 | `		return PH7_OK;` |
|      - | 3415 | `	}` |
|    271 | 3416 | `	pResource = 0;` |
|    271 | 3417 | `	if( nArg > 3 ){` |
|    ! 0 | 3418 | `		pResource = apArg[3];` |
|    271 | 3419 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 3420 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 3421 | `		 * access the underlying virtual machine.` |
|      - | 3422 | `		 */` |
|     46 | 3423 | `		pResource = apArg[0];` |
|     22 | 3424 | `	}` |
|      - | 3425 | `	/* Initialize the structure */` |
|    271 | 3426 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3427 | `	/* Convert open mode to PH7 flags */` |
|    271 | 3428 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 3429 | `	/* Try to get a handle */` |
|    404 | 3430 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    133 | 3431 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    271 | 3432 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3433 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|      3 | 3434 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3435 | `		ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 3436 | `		return PH7_OK;` |
|      - | 3437 | `	}` |
|      - | 3438 | `	/* All done,return the io_private instance as a resource */` |
|    269 | 3439 | `	ph7_result_resource(pCtx,pDev);` |
|    269 | 3440 | `	return PH7_OK;` |
|    138 | 3441 | `}` |
|      - | 3442 | `/*` |
|      - | 3443 | ` * bool fclose(resource $handle)` |
|      - | 3444 | ` *  Closes an open file pointer` |
|      - | 3445 | ` * Parameters` |
|      - | 3446 | ` *  $handle` |
|      - | 3447 | ` *   The file pointer.` |
|      - | 3448 | ` * Return` |
|      - | 3449 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3450 | ` */` |
|    390 | 3451 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3452 | `{` |
|      - | 3453 | `	const ph7_io_stream *pStream;` |
|      - | 3454 | `	io_private *pDev;` |
|      - | 3455 | `	ph7_vm *pVm;` |
|    395 | 3456 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3457 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3458 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3459 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3460 | `		return PH7_OK;` |
|      - | 3461 | `	}` |
|      - | 3462 | `	/* Extract our private data */` |
|    395 | 3463 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3464 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    395 | 3465 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 3466 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3467 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 3468 | `	}` |
|      - | 3469 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    393 | 3470 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3471 | `		/*Expecting an IO handle */` |
|    ! 0 | 3472 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3473 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3474 | `		return PH7_OK;` |
|      - | 3475 | `	}` |
|      - | 3476 | `	/* Point to the target IO stream device */` |
|    393 | 3477 | `	pStream = pDev->pStream;` |
|    393 | 3478 | `	if( pStream == 0 ){` |
|    ! 0 | 3479 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3480 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3481 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3482 | `			);` |
|    ! 0 | 3483 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3484 | `		return PH7_OK;` |
|      - | 3485 | `	}` |
|      - | 3486 | `	/* Point to the VM that own this context */` |
|    393 | 3487 | `	pVm = pCtx->pVm;` |
|      - | 3488 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    393 | 3489 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 3490 | `		/* Perform the requested operation */` |
|    393 | 3491 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3492 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    393 | 3493 | `		MarkIOPrivateClosed(pDev);` |
|    194 | 3494 | `	}` |
|      - | 3495 | `	/* Return TRUE */` |
|    393 | 3496 | `	ph7_result_bool(pCtx,1);` |
|    393 | 3497 | `	return PH7_OK;` |
|    200 | 3498 | `}` |
|      - | 3499 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3500 | `/*` |
|      - | 3501 | ` * MD5/SHA1 digest consumer.` |
|      - | 3502 | ` */` |
|     72 | 3503 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3504 | `{` |
|      - | 3505 | `	/* Append hex chunk verbatim */` |
|     73 | 3506 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 3507 | `	return SXRET_OK;` |
|      1 | 3508 | `}` |
|      - | 3509 | `/*` |
|      - | 3510 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 3511 | ` *  Calculates the md5 hash of a given file.` |
|      - | 3512 | ` * Parameters` |
|      - | 3513 | ` *  $uri` |
|      - | 3514 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3515 | ` *  $raw_output` |
|      - | 3516 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 3517 | ` * Return` |
|      - | 3518 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 3519 | ` */` |
|      2 | 3520 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3521 | `{` |
|      - | 3522 | `	const ph7_io_stream *pStream;` |
|      - | 3523 | `	unsigned char zDigest[16];` |
|      3 | 3524 | `	int raw_output  = FALSE;` |
|      - | 3525 | `	const char *zFile;` |
|      - | 3526 | `	MD5Context sCtx;` |
|      - | 3527 | `	char zBuf[8192];` |
|      - | 3528 | `	void *pHandle;` |
|      - | 3529 | `	ph7_int64 n;` |
|      - | 3530 | `	int nLen;` |
|      3 | 3531 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3532 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3533 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3534 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3535 | `		return PH7_OK;` |
|      - | 3536 | `	}` |
|      - | 3537 | `	/* Extract the file path */` |
|      3 | 3538 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3539 | `	/* Point to the target IO stream device */` |
|      3 | 3540 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3541 | `	if( pStream == 0 ){` |
|    ! 0 | 3542 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3543 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3544 | `		return PH7_OK;` |
|      - | 3545 | `	}` |
|      3 | 3546 | `	if( nArg > 1 ){` |
|    ! 0 | 3547 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3548 | `	}` |
|      - | 3549 | `	/* Try to open the file in read-only mode */` |
|      3 | 3550 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3551 | `	if( pHandle == 0 ){` |
|    ! 0 | 3552 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3554 | `		return PH7_OK;` |
|      - | 3555 | `	}` |
|      - | 3556 | `	/* Init the MD5 context */` |
|      3 | 3557 | `	MD5Init(&sCtx);` |
|      - | 3558 | `	/* Perform the requested operation */` |
|      2 | 3559 | `	for(;;){` |
|      5 | 3560 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3561 | `		if( n < 1 ){` |
|      - | 3562 | `			/* EOF or IO error,break immediately */` |
|      3 | 3563 | `			break;` |
|      - | 3564 | `		}` |
|      3 | 3565 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3566 | `	}` |
|      - | 3567 | `	/* Close the stream */` |
|      3 | 3568 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3569 | `	/* Extract the digest */` |
|      3 | 3570 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 3571 | `	if( raw_output ){` |
|      - | 3572 | `		/* Output raw digest */` |
|    ! 0 | 3573 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3574 | `	}else{` |
|      - | 3575 | `		/* Perform a binary to hex conversion */` |
|      3 | 3576 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3577 | `	}` |
|      3 | 3578 | `	return PH7_OK;` |
|      2 | 3579 | `}` |
|      - | 3580 | `/*` |
|      - | 3581 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 3582 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 3583 | ` * Parameters` |
|      - | 3584 | ` *  $uri` |
|      - | 3585 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3586 | ` *  $raw_output` |
|      - | 3587 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 3588 | ` * Return` |
|      - | 3589 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 3590 | ` */` |
|      2 | 3591 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3592 | `{` |
|      - | 3593 | `	const ph7_io_stream *pStream;` |
|      - | 3594 | `	unsigned char zDigest[20];` |
|      3 | 3595 | `	int raw_output  = FALSE;` |
|      - | 3596 | `	const char *zFile;` |
|      - | 3597 | `	SHA1Context sCtx;` |
|      - | 3598 | `	char zBuf[8192];` |
|      - | 3599 | `	void *pHandle;` |
|      - | 3600 | `	ph7_int64 n;` |
|      - | 3601 | `	int nLen;` |
|      3 | 3602 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3603 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3604 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3605 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3606 | `		return PH7_OK;` |
|      - | 3607 | `	}` |
|      - | 3608 | `	/* Extract the file path */` |
|      3 | 3609 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3610 | `	/* Point to the target IO stream device */` |
|      3 | 3611 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3612 | `	if( pStream == 0 ){` |
|    ! 0 | 3613 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3615 | `		return PH7_OK;` |
|      - | 3616 | `	}` |
|      3 | 3617 | `	if( nArg > 1 ){` |
|    ! 0 | 3618 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3619 | `	}` |
|      - | 3620 | `	/* Try to open the file in read-only mode */` |
|      3 | 3621 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3622 | `	if( pHandle == 0 ){` |
|    ! 0 | 3623 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3624 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3625 | `		return PH7_OK;` |
|      - | 3626 | `	}` |
|      - | 3627 | `	/* Init the SHA1 context */` |
|      3 | 3628 | `	SHA1Init(&sCtx);` |
|      - | 3629 | `	/* Perform the requested operation */` |
|      2 | 3630 | `	for(;;){` |
|      5 | 3631 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3632 | `		if( n < 1 ){` |
|      - | 3633 | `			/* EOF or IO error,break immediately */` |
|      3 | 3634 | `			break;` |
|      - | 3635 | `		}` |
|      3 | 3636 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3637 | `	}` |
|      - | 3638 | `	/* Close the stream */` |
|      3 | 3639 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3640 | `	/* Extract the digest */` |
|      3 | 3641 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 3642 | `	if( raw_output ){` |
|      - | 3643 | `		/* Output raw digest */` |
|    ! 0 | 3644 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3645 | `	}else{` |
|      - | 3646 | `		/* Perform a binary to hex conversion */` |
|      3 | 3647 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3648 | `	}` |
|      3 | 3649 | `	return PH7_OK;` |
|      2 | 3650 | `}` |
|      - | 3651 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3652 | `/*` |
|      - | 3653 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 3654 | ` *  Parse a configuration file.` |
|      - | 3655 | ` * Parameters` |
|      - | 3656 | ` * $filename` |
|      - | 3657 | ` *  The filename of the ini file being parsed.` |
|      - | 3658 | ` * $process_sections` |
|      - | 3659 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 3660 | ` *  with the section names and settings included.` |
|      - | 3661 | ` *  The default for process_sections is FALSE.` |
|      - | 3662 | ` * $scanner_mode` |
|      - | 3663 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 3664 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 3665 | ` * Return` |
|      - | 3666 | ` *  The settings are returned as an associative array on success.` |
|      - | 3667 | ` *  Otherwise is returned.` |
|      - | 3668 | ` */` |
|      2 | 3669 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3670 | `{` |
|      - | 3671 | `	const ph7_io_stream *pStream;` |
|      - | 3672 | `	const char *zFile;` |
|      - | 3673 | `	SyBlob sContents;` |
|      - | 3674 | `	void *pHandle;` |
|      - | 3675 | `	int nLen;` |
|      3 | 3676 | `	sxi32 rc = PH7_OK;` |
|      3 | 3677 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3678 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3679 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3681 | `		return PH7_OK;` |
|      - | 3682 | `	}` |
|      - | 3683 | `	/* Extract the file path */` |
|      3 | 3684 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3685 | `	/* Point to the target IO stream device */` |
|      3 | 3686 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3687 | `	if( pStream == 0 ){` |
|    ! 0 | 3688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3690 | `		return PH7_OK;` |
|      - | 3691 | `	}` |
|      - | 3692 | `	/* Try to open the file in read-only mode */` |
|      3 | 3693 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3694 | `	if( pHandle == 0 ){` |
|    ! 0 | 3695 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3697 | `		return PH7_OK;` |
|      - | 3698 | `	}` |
|      3 | 3699 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 3700 | `	/* Read the whole file */` |
|      3 | 3701 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 3702 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 3703 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 3704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3705 | `	}else{` |
|      - | 3706 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 3707 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 3708 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 3709 | `	}` |
|      - | 3710 | `	/* Close the stream */` |
|      3 | 3711 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3712 | `	/* Release the working buffer */` |
|      3 | 3713 | `	SyBlobRelease(&sContents);` |
|      - | 3714 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 3715 | `	return rc;` |
|      2 | 3716 | `}` |
|      - | 3717 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 3718 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 3719 | `/*` |
|      - | 3720 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 3721 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 3722 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 3723 | ` * to any non-VFS resource).` |
|      - | 3724 | ` */` |
|      - | 3725 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 3726 | `{` |
|      - | 3727 | `	SXUNUSED(pResource);` |
|      - | 3728 | `	return "Unknown";` |
|      - | 3729 | `}` |
|      - | 3730 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 3731 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 3732 | `{` |
|      - | 3733 | `	SXUNUSED(pResource);` |
|      - | 3734 | `	return 0;` |
|      - | 3735 | `}` |
|      - | 3736 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3737 |  |
