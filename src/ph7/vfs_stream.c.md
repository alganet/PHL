# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1478/2073 lines (71.30%)

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
|     50 |   49 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      3 |   50 | `{` |
|     53 |   51 | `	io_private *pDev = (io_private *)pResource;` |
|     53 |   52 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      3 |   53 | `}` |
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
|     46 |  133 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  134 | `{` |
|      - |  135 | `	const ph7_io_stream *pStream;` |
|      - |  136 | `	io_private *pDev;` |
|      - |  137 | `	ph7_int64 iOfft;` |
|      - |  138 | `	int whence;` |
|      - |  139 | `	int rc;` |
|     49 |  140 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  141 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  142 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  143 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  144 | `		return PH7_OK;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Extract our private data */` |
|     49 |  147 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  148 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     49 |  149 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  150 | `		/*Expecting an IO handle */` |
|    ! 0 |  151 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  152 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  153 | `		return PH7_OK;` |
|      - |  154 | `	}` |
|      - |  155 | `	/* Point to the target IO stream device */` |
|     49 |  156 | `	pStream = pDev->pStream;` |
|     49 |  157 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  158 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  159 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  160 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  161 | `			);` |
|    ! 0 |  162 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  163 | `		return PH7_OK;` |
|      - |  164 | `	}` |
|      - |  165 | `	/* Extract the offset */` |
|     49 |  166 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     49 |  167 | `	whence = 0;/* SEEK_SET */` |
|     49 |  168 | `	if( nArg > 2 ){` |
|      - |  169 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|      - |  170 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|      - |  171 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|      - |  172 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|     40 |  173 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|     19 |  174 | `	}` |
|     49 |  175 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|      - |  176 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|      - |  177 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|      - |  178 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|      - |  179 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|      - |  180 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|     13 |  181 | `		ph7_result_int(pCtx,-1);` |
|     13 |  182 | `		return PH7_OK;` |
|      - |  183 | `	}` |
|     37 |  184 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|      - |  185 | `		/* The CURRENT position is the LOGICAL one: the device sits past the` |
|      - |  186 | `		 * read-ahead the line readers buffer, so seek relative to where the` |
|      - |  187 | `		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without` |
|      - |  188 | `		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead` |
|      - |  189 | `		 * skipped everything still sitting in the buffer. */` |
|      8 |  190 | `		iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);` |
|      3 |  191 | `	}` |
|      - |  192 | `	/* Perform the requested operation */` |
|     37 |  193 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     37 |  194 | `	if( rc == PH7_OK ){` |
|      - |  195 | `		/* Ignore buffered data */` |
|     37 |  196 | `		ResetIOPrivate(pDev);` |
|     17 |  197 | `	}` |
|      - |  198 | `	/* IO result */` |
|     37 |  199 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     37 |  200 | `	return PH7_OK;` |
|     26 |  201 | `}` |
|      - |  202 | `/*` |
|      - |  203 | ` * int64 ftell(resource $handle)` |
|      - |  204 | ` *  Returns the current position of the file read/write pointer.` |
|      - |  205 | ` * Parameters` |
|      - |  206 | ` *  $handle` |
|      - |  207 | ` *   The file pointer.` |
|      - |  208 | ` * Return` |
|      - |  209 | ` *  Returns the position of the file pointer referenced by handle` |
|      - |  210 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - |  211 | ` *  FALSE is returned on failure.` |
|      - |  212 | ` */` |
|     42 |  213 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  214 | `{` |
|      - |  215 | `	const ph7_io_stream *pStream;` |
|      - |  216 | `	io_private *pDev;` |
|      - |  217 | `	ph7_int64 iOfft;` |
|     45 |  218 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  219 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  220 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Extract our private data */` |
|     45 |  225 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  226 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     45 |  227 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  228 | `		/*Expecting an IO handle */` |
|    ! 0 |  229 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  230 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  231 | `		return PH7_OK;` |
|      - |  232 | `	}` |
|      - |  233 | `	/* Point to the target IO stream device */` |
|     45 |  234 | `	pStream = pDev->pStream;` |
|     45 |  235 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 |  236 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  237 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  238 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  239 | `			);` |
|    ! 0 |  240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  241 | `		return PH7_OK;` |
|      - |  242 | `	}` |
|      - |  243 | `	/* Perform the requested operation. The device sits past whatever the line` |
|      - |  244 | `	 * readers buffered ahead, so the SCRIPT's position is the device position` |
|      - |  245 | `	 * less the unconsumed remainder — ftell() after fgets("abcdefghij\nrest")` |
|      - |  246 | `	 * is php's 11, not the 15 the device already read. */` |
|     45 |  247 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|     45 |  248 | `	iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);` |
|      - |  249 | `	/* IO result */` |
|     45 |  250 | `	ph7_result_int64(pCtx,iOfft);` |
|     45 |  251 | `	return PH7_OK;` |
|     24 |  252 | `}` |
|      - |  253 | `/*` |
|      - |  254 | ` * bool rewind(resource $handle)` |
|      - |  255 | ` *  Rewind the position of a file pointer.` |
|      - |  256 | ` * Parameters` |
|      - |  257 | ` *  $handle` |
|      - |  258 | ` *   The file pointer.` |
|      - |  259 | ` * Return` |
|      - |  260 | ` *  TRUE on success or FALSE on failure.` |
|      - |  261 | ` */` |
|     84 |  262 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  263 | `{` |
|      - |  264 | `	const ph7_io_stream *pStream;` |
|      - |  265 | `	io_private *pDev;` |
|      - |  266 | `	int rc;` |
|     87 |  267 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  268 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  269 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  270 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  271 | `		return PH7_OK;` |
|      - |  272 | `	}` |
|      - |  273 | `	/* Extract our private data */` |
|     87 |  274 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  275 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     87 |  276 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  277 | `		/*Expecting an IO handle */` |
|    ! 0 |  278 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  279 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  280 | `		return PH7_OK;` |
|      - |  281 | `	}` |
|      - |  282 | `	/* Point to the target IO stream device */` |
|     87 |  283 | `	pStream = pDev->pStream;` |
|     87 |  284 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  285 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  286 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  287 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  288 | `			);` |
|    ! 0 |  289 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  290 | `		return PH7_OK;` |
|      - |  291 | `	}` |
|      - |  292 | `	/* Perform the requested operation */` |
|     87 |  293 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     87 |  294 | `	if( rc == PH7_OK ){` |
|      - |  295 | `		/* Ignore buffered data */` |
|     87 |  296 | `		ResetIOPrivate(pDev);` |
|     42 |  297 | `	}` |
|      - |  298 | `	/* IO result */` |
|     87 |  299 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     87 |  300 | `	return PH7_OK;` |
|     45 |  301 | `}` |
|      - |  302 | `/*` |
|      - |  303 | ` * bool fflush(resource $handle)` |
|      - |  304 | ` *  Flushes the output to a file.` |
|      - |  305 | ` * Parameters` |
|      - |  306 | ` *  $handle` |
|      - |  307 | ` *   The file pointer.` |
|      - |  308 | ` * Return` |
|      - |  309 | ` *  TRUE on success or FALSE on failure.` |
|      - |  310 | ` */` |
|      2 |  311 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  312 | `{` |
|      - |  313 | `	const ph7_io_stream *pStream;` |
|      - |  314 | `	io_private *pDev;` |
|      - |  315 | `	int rc;` |
|      3 |  316 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  317 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  320 | `		return PH7_OK;` |
|      - |  321 | `	}` |
|      - |  322 | `	/* Extract our private data */` |
|      3 |  323 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  324 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |  325 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  326 | `		/*Expecting an IO handle */` |
|    ! 0 |  327 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  329 | `		return PH7_OK;` |
|      - |  330 | `	}` |
|      - |  331 | `	/* Point to the target IO stream device */` |
|      3 |  332 | `	pStream = pDev->pStream;` |
|      3 |  333 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 |  334 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  335 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  336 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  337 | `			);` |
|    ! 0 |  338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  339 | `		return PH7_OK;` |
|      - |  340 | `	}` |
|      - |  341 | `	/* Perform the requested operation */` |
|      3 |  342 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - |  343 | `	/* IO result */` |
|      3 |  344 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  345 | `	return PH7_OK;` |
|      2 |  346 | `}` |
|      - |  347 | `/*` |
|      - |  348 | ` * bool feof(resource $handle)` |
|      - |  349 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  350 | ` * Parameters` |
|      - |  351 | ` *  $handle` |
|      - |  352 | ` *   The file pointer.` |
|      - |  353 | ` * Return` |
|      - |  354 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  355 | ` */` |
|  17882 |  356 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  357 | `{` |
|      - |  358 | `	const ph7_io_stream *pStream;` |
|      - |  359 | `	io_private *pDev;` |
|      - |  360 | `	int rc;` |
|  17887 |  361 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  362 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  363 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  364 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  365 | `		return PH7_OK;` |
|      - |  366 | `	}` |
|      - |  367 | `	/* Extract our private data */` |
|  17887 |  368 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  369 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  17887 |  370 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  371 | `		/*Expecting an IO handle */` |
|    ! 0 |  372 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  373 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  374 | `		return PH7_OK;` |
|      - |  375 | `	}` |
|      - |  376 | `	/* Point to the target IO stream device */` |
|  17887 |  377 | `	pStream = pDev->pStream;` |
|  17887 |  378 | `	if( pStream == 0 ){` |
|    ! 0 |  379 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  380 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  381 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  382 | `			);` |
|    ! 0 |  383 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  384 | `		return PH7_OK;` |
|      - |  385 | `	}` |
|  17887 |  386 | `	rc = SXERR_EOF;` |
|      - |  387 | `	/* Perform the requested operation */` |
|  17887 |  388 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  389 | `		/* Data is available */` |
|  10765 |  390 | `		rc = PH7_OK;` |
|   5385 |  391 | `	}else{` |
|      - |  392 | `		char zBuf[4096];` |
|      - |  393 | `		ph7_int64 n;` |
|      - |  394 | `		/* Perform a buffered read */` |
|   7127 |  395 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   7127 |  396 | `		if( n > 0 ){` |
|      - |  397 | `			/* Copy buffered data */` |
|   2445 |  398 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   2445 |  399 | `			rc = PH7_OK;` |
|   1220 |  400 | `		}` |
|      - |  401 | `	}` |
|      - |  402 | `	/* EOF or not */` |
|  17887 |  403 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  17887 |  404 | `	return PH7_OK;` |
|   8946 |  405 | `}` |
|      - |  406 | `/*` |
|      - |  407 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  408 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  409 | ` * [i.e: IO error ] or EOF.` |
|      - |  410 | ` */` |
|     62 |  411 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      4 |  412 | `{` |
|     66 |  413 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     66 |  414 | `	char *zBuf = (char *)pBuf;` |
|      - |  415 | `	ph7_int64 n,nRead;` |
|     66 |  416 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     66 |  417 | `	if( n > 0 ){` |
|      8 |  418 | `		if( n > nLen ){` |
|      3 |  419 | `			n = nLen;` |
|      1 |  420 | `		}` |
|      - |  421 | `		/* Copy the buffered data */` |
|      8 |  422 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  423 | `		/* Update the read offset */` |
|      8 |  424 | `		pDev->nOfft += (sxu32)n;` |
|      8 |  425 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  426 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      5 |  427 | `			SyBlobReset(&pDev->sBuffer);` |
|      5 |  428 | `			pDev->nOfft = 0;` |
|      2 |  429 | `		}` |
|      8 |  430 | `		nLen -= n;` |
|      8 |  431 | `		if( nLen < 1 ){` |
|      - |  432 | `			/* All done */` |
|      3 |  433 | `			return n;` |
|      - |  434 | `		}` |
|      - |  435 | `		/* Advance the cursor */` |
|      5 |  436 | `		zBuf += n;` |
|      2 |  437 | `	}` |
|      - |  438 | `	/* Read without buffering */` |
|     64 |  439 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     64 |  440 | `	if( nRead > 0 ){` |
|     46 |  441 | `		n += nRead;` |
|     40 |  442 | `	}else if( n < 1 ){` |
|      - |  443 | `		/* EOF or IO error */` |
|     16 |  444 | `		return nRead;` |
|      - |  445 | `	}` |
|     49 |  446 | `	return n;` |
|     35 |  447 | `}` |
|      - |  448 | `/*` |
|      - |  449 | ` * Extract a single line from the buffered input.` |
|      - |  450 | ` */` |
|  13440 |  451 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  452 | `{` |
|      - |  453 | `	const char *zIn,*zEnd,*zPtr;` |
|  13445 |  454 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|  13445 |  455 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|  13445 |  456 | `	zPtr = zIn;` |
| 648106 |  457 | `	while( zIn < zEnd ){` |
| 647902 |  458 | `		if( zIn[0] == '\n' ){` |
|      - |  459 | `			/* Line found */` |
|  13241 |  460 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|  13241 |  461 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|  13241 |  462 | `			*pzLine = zPtr;` |
|  13241 |  463 | `			return SXRET_OK;` |
|      - |  464 | `		}` |
| 634666 |  465 | `		zIn++;` |
|      5 |  466 | `	}` |
|      - |  467 | `	/* No line were found */` |
|    209 |  468 | `	return SXERR_NOTFOUND;` |
|   6725 |  469 | `}` |
|      - |  470 | `/*` |
|      - |  471 | ` * Read a single line from the underlying IO stream device.` |
|      - |  472 | ` */` |
|  13464 |  473 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  474 | `{` |
|  13469 |  475 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - |  476 | `	char zBuf[8192];` |
|      - |  477 | `	ph7_int64 n;` |
|      - |  478 | `	sxi32 rc;` |
|  13469 |  479 | `	n = 0;` |
|  13469 |  480 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  481 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    183 |  482 | `		SyBlobReset(&pDev->sBuffer);` |
|    183 |  483 | `		pDev->nOfft = 0;` |
|     89 |  484 | `	}` |
|  13469 |  485 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  486 | `		/* Check if there is a line */` |
|  13291 |  487 | `		rc = GetLine(pDev,&n,pzData);` |
|  13291 |  488 | `		if( rc == SXRET_OK ){` |
|      - |  489 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  490 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|  13131 |  491 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  492 | `				n = nMaxLen;` |
|    ! 0 |  493 | `			}` |
|  13131 |  494 | `			pDev->nOfft += (sxu32)n;` |
|  13131 |  495 | `			return n;` |
|      - |  496 | `		}` |
|     80 |  497 | `	}` |
|      - |  498 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  499 | `	 * limit is reached.` |
|      - |  500 | `	 */` |
|    174 |  501 | `	for(;;){` |
|    353 |  502 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    353 |  503 | `		if( n < 1 ){` |
|      - |  504 | `			/* EOF or IO error */` |
|    199 |  505 | `			break;` |
|      - |  506 | `		}` |
|      - |  507 | `		/* Append the data just read */` |
|    157 |  508 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  509 | `		/* Try to extract a line */` |
|    157 |  510 | `		rc = GetLine(pDev,&n,pzData);` |
|    157 |  511 | `		if( rc == SXRET_OK ){` |
|      - |  512 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|      - |  513 | `			 * cap, newline included, is left buffered for the next read. */` |
|    113 |  514 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|      5 |  515 | `				n = nMaxLen;` |
|      2 |  516 | `			}` |
|    113 |  517 | `			pDev->nOfft += (sxu32)n;` |
|    113 |  518 | `			return n;` |
|      - |  519 | `		}` |
|     46 |  520 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - |  521 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|      - |  522 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|      - |  523 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|      - |  524 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|      - |  525 | `			 * buffer once it is fully consumed. */` |
|     35 |  526 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     35 |  527 | `			n = nMaxLen;` |
|     35 |  528 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     35 |  529 | `			return n;` |
|      - |  530 | `		}` |
|      2 |  531 | `	}` |
|    199 |  532 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  533 | `		/* Read limit reached,return the available data */` |
|    163 |  534 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    163 |  535 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  536 | `		/* Reset the working buffer */` |
|    163 |  537 | `		SyBlobReset(&pDev->sBuffer);` |
|    163 |  538 | `		pDev->nOfft = 0;` |
|     79 |  539 | `	}` |
|    199 |  540 | `	return n;` |
|   6737 |  541 | `}` |
|      - |  542 | `/*` |
|      - |  543 | ` * Open an IO stream handle.` |
|      - |  544 | ` * Notes on stream:` |
|      - |  545 | ` * According to the PHP reference manual.` |
|      - |  546 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - |  547 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - |  548 | ` * to an arbitrary locations within the stream.` |
|      - |  549 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - |  550 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - |  551 | ` * on a remote server.` |
|      - |  552 | ` * A stream is referenced as: scheme://target` |
|      - |  553 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - |  554 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - |  555 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - |  556 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - |  557 | ` *  with a path appended.` |
|      - |  558 | ` *` |
|      - |  559 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - |  560 | ` * Please refer to the official documentation for a full discussion.` |
|      - |  561 | ` * This function return a handle on success. Otherwise null.` |
|      - |  562 | ` */` |
|  33048 |  563 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  564 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 |  565 | `{` |
|  33053 |  566 | `	void *pHandle = 0; /* cc warning */` |
|      - |  567 | `	SyString sFile;` |
|      - |  568 | `	ph7_value sDummy;` |
|      - |  569 | `	int rc;` |
|  33053 |  570 | `	if( pStream == 0 ){` |
|      - |  571 | `		/* No such stream device */` |
|    ! 0 |  572 | `		return 0;` |
|      - |  573 | `	}` |
|  33053 |  574 | `	if( pResource == 0 ){` |
|      - |  575 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  576 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  577 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  578 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  579 | `		 * reads it during the call, and file:// ignores it. */` |
|  32899 |  580 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  32899 |  581 | `		pResource = &sDummy;` |
|  16447 |  582 | `	}` |
|  33053 |  583 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  33053 |  584 | `	if( use_include ){` |
|   9976 |  585 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - |  586 | `#ifdef __WINNT__` |
|      - |  587 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  588 | `#endif` |
|   9909 |  589 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9902 |  590 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - |  591 | `				/*  Open the file directly */` |
|     79 |  592 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     79 |  593 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - |  594 | `					/* Mark as included */` |
|     77 |  595 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     36 |  596 | `				}` |
|     42 |  597 | `		}else{` |
|      - |  598 | `			SyString *pPath;` |
|      - |  599 | `			SyBlob sWorker;` |
|      - |  600 | `#ifdef __WINNT__` |
|      - |  601 | `			static const int c = '\\';` |
|      - |  602 | `#else` |
|      - |  603 | `			static const int c = '/';` |
|      - |  604 | `#endif` |
|      - |  605 | `			/* Init the path builder working buffer */` |
|   9906 |  606 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  607 | `			/* Build a path from the set of include path */` |
|   9906 |  608 | `			SySetResetCursor(&pVm->aPaths);` |
|   9906 |  609 | `			rc = SXERR_IO;` |
|   9912 |  610 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - |  611 | `				/* Build full path */` |
|   9906 |  612 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - |  613 | `				/* Append null terminator */` |
|   9906 |  614 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  615 | `					continue;` |
|      - |  616 | `				}` |
|      - |  617 | `				/* Try to open the file */` |
|   9906 |  618 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9906 |  619 | `				if( rc == PH7_OK ){` |
|   9899 |  620 | `					if( bPushInclude ){` |
|      - |  621 | `						/* Mark as included */` |
|   9899 |  622 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4948 |  623 | `					}` |
|   9899 |  624 | `					break;` |
|      - |  625 | `				}` |
|      - |  626 | `				/* Reset the working buffer */` |
|      8 |  627 | `				SyBlobReset(&sWorker);` |
|      - |  628 | `				/* Check the next path */` |
|      2 |  629 | `			}` |
|   9906 |  630 | `			SyBlobRelease(&sWorker);` |
|      - |  631 | `		}` |
|   4993 |  632 | `	}else{` |
|      - |  633 | `		/* Open the URI direcly */` |
|  23077 |  634 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - |  635 | `	}` |
|  33053 |  636 | `	if( rc != PH7_OK ){` |
|      - |  637 | `		/* IO error */` |
|     26 |  638 | `		return 0;` |
|      - |  639 | `	}` |
|      - |  640 | `	/* Return the file handle */` |
|  33031 |  641 | `	return pHandle;` |
|  16529 |  642 | `}` |
|      - |  643 | `/*` |
|      - |  644 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - |  645 | ` * Store the read data in the given BLOB (last argument).` |
|      - |  646 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - |  647 | ` */` |
|   9960 |  648 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      5 |  649 | `{` |
|      - |  650 | `	ph7_int64 nRead;` |
|      - |  651 | `	char zBuf[8192]; /* 8K */` |
|      - |  652 | `	int rc;` |
|      - |  653 | `	/* Perform the requested operation */` |
|   9961 |  654 | `	for(;;){` |
|  19927 |  655 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19927 |  656 | `		if( nRead < 1 ){` |
|      - |  657 | `			/* EOF or IO error */` |
|   9965 |  658 | `			break;` |
|      - |  659 | `		}` |
|      - |  660 | `		/* Append contents */` |
|   9967 |  661 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9967 |  662 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  663 | `			break;` |
|      - |  664 | `		}` |
|      5 |  665 | `	}` |
|   9965 |  666 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      5 |  667 | `}` |
|      - |  668 | `/*` |
|      - |  669 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - |  670 | ` */` |
|  33086 |  671 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 |  672 | `{` |
|  33091 |  673 | `	if( pStream->xClose ){` |
|  33091 |  674 | `		pStream->xClose(pHandle);` |
|  16543 |  675 | `	}` |
|  33091 |  676 | `}` |
|      - |  677 | `/*` |
|      - |  678 | ` * string fgetc(resource $handle)` |
|      - |  679 | ` *  Gets a character from the given file pointer.` |
|      - |  680 | ` * Parameters` |
|      - |  681 | ` *  $handle` |
|      - |  682 | ` *   The file pointer.` |
|      - |  683 | ` * Return` |
|      - |  684 | ` *  Returns a string containing a single character read from the file` |
|      - |  685 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - |  686 | ` * WARNING` |
|      - |  687 | ` *  This operation is extremely slow.Avoid using it.` |
|      - |  688 | ` */` |
|      4 |  689 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  690 | `{` |
|      - |  691 | `	const ph7_io_stream *pStream;` |
|      - |  692 | `	io_private *pDev;` |
|      - |  693 | `	int c,n;` |
|      5 |  694 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  695 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  696 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  697 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  698 | `		return PH7_OK;` |
|      - |  699 | `	}` |
|      - |  700 | `	/* Extract our private data */` |
|      5 |  701 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  702 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 |  703 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  704 | `		/*Expecting an IO handle */` |
|    ! 0 |  705 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  707 | `		return PH7_OK;` |
|      - |  708 | `	}` |
|      - |  709 | `	/* Point to the target IO stream device */` |
|      5 |  710 | `	pStream = pDev->pStream;` |
|      5 |  711 | `	if( pStream == 0  ){` |
|    ! 0 |  712 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  713 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  714 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  715 | `			);` |
|    ! 0 |  716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  717 | `		return PH7_OK;` |
|      - |  718 | `	}` |
|      - |  719 | `	/* Perform the requested operation */` |
|      5 |  720 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - |  721 | `	/* IO result */` |
|      5 |  722 | `	if( n < 1 ){` |
|      - |  723 | `		/* EOF or error,return FALSE */` |
|    ! 0 |  724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  725 | `	}else{` |
|      - |  726 | `		/* Return the string holding the character */` |
|      5 |  727 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - |  728 | `	}` |
|      5 |  729 | `	return PH7_OK;` |
|      3 |  730 | `}` |
|      - |  731 | `/*` |
|      - |  732 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - |  733 | ` *  Gets line from file pointer.` |
|      - |  734 | ` * Parameters` |
|      - |  735 | ` *  $handle` |
|      - |  736 | ` *   The file pointer.` |
|      - |  737 | ` * $length` |
|      - |  738 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  739 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  740 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  741 | ` *  the end of the line.` |
|      - |  742 | ` * Return` |
|      - |  743 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - |  744 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - |  745 | ` *  If an error occurs, FALSE is returned.` |
|      - |  746 | ` */` |
|  13332 |  747 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  748 | `{` |
|      - |  749 | `	const ph7_io_stream *pStream;` |
|      - |  750 | `	const char *zLine;` |
|      - |  751 | `	io_private *pDev;` |
|      - |  752 | `	ph7_int64 n,nLen;` |
|  13337 |  753 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  754 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  755 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  756 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  757 | `		return PH7_OK;` |
|      - |  758 | `	}` |
|      - |  759 | `	/* Extract our private data */` |
|  13337 |  760 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  761 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  13337 |  762 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  763 | `		/*Expecting an IO handle */` |
|    ! 0 |  764 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  766 | `		return PH7_OK;` |
|      - |  767 | `	}` |
|      - |  768 | `	/* Point to the target IO stream device */` |
|  13337 |  769 | `	pStream = pDev->pStream;` |
|  13337 |  770 | `	if( pStream == 0  ){` |
|    ! 0 |  771 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  772 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  773 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  774 | `			);` |
|    ! 0 |  775 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  776 | `		return PH7_OK;` |
|      - |  777 | `	}` |
|  13337 |  778 | `	nLen = -1;` |
|  13337 |  779 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - |  780 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|      - |  781 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|     61 |  782 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     61 |  783 | `		if( nLen < 1 ){` |
|      5 |  784 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  785 | `				"fgets(): Argument #2 ($length) must be greater than 0");` |
|      - |  786 | `		}` |
|      - |  787 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|      - |  788 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|      - |  789 | `		 * false at any position, exactly like EOF. */` |
|     57 |  790 | `		nLen -= 1;` |
|     57 |  791 | `		if( nLen == 0 ){` |
|      3 |  792 | `			ph7_result_bool(pCtx,0);` |
|      3 |  793 | `			return PH7_OK;` |
|      - |  794 | `		}` |
|     27 |  795 | `	}` |
|      - |  796 | `	/* Perform the requested operation */` |
|  13331 |  797 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|  13331 |  798 | `	if( n < 1 ){` |
|      - |  799 | `		/* EOF or IO error,return FALSE */` |
|     13 |  800 | `		ph7_result_bool(pCtx,0);` |
|      9 |  801 | `	}else{` |
|      - |  802 | `		/* Return the freshly extracted line */` |
|  13323 |  803 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - |  804 | `	}` |
|  13331 |  805 | `	return PH7_OK;` |
|   6671 |  806 | `}` |
|      - |  807 | `/*` |
|      - |  808 | ` * string\|false stream_get_line(resource $stream, int $length, string $ending = "")` |
|      - |  809 | ` *  Read a line from a stream, up to $length bytes or the FIRST occurrence of` |
|      - |  810 | ` *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but` |
|      - |  811 | ` *  never returned, and it may be any string.` |
|      - |  812 | ` *  php's window rule (php_stream_get_record), pinned by probe: the ending` |
|      - |  813 | ` *  counts only when it fits ENTIRELY inside the first $length bytes —` |
|      - |  814 | ` *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,` |
|      - |  815 | ` *  because the ending straddles its edge — and a capped read consumes no` |
|      - |  816 | ` *  ending that starts at the boundary. $length 0 means php's 8192 default; at` |
|      - |  817 | ` *  EOF the remainder is returned as-is, and false only when nothing is left.` |
|      - |  818 | ` */` |
|     62 |  819 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  820 | `{` |
|      - |  821 | `	const ph7_io_stream *pStream;` |
|     64 |  822 | `	const char *zEnding = "";` |
|      - |  823 | `	io_private *pDev;` |
|      - |  824 | `	ph7_int64 nMaxLen;` |
|     64 |  825 | `	int nEndLen = 0;` |
|     64 |  826 | `	sxu32 iScanFrom = 0;` |
|     64 |  827 | `	int bEof = 0;` |
|     64 |  828 | `	if( nArg < 2 ){` |
|      - |  829 | `		/* The central arity screen reports this; keep a refusal for a direct call. */` |
|    ! 0 |  830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  831 | `		return PH7_OK;` |
|      - |  832 | `	}` |
|     64 |  833 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|      4 |  834 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  835 | `			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",` |
|      1 |  836 | `			ph7_type_name(apArg[0]));` |
|      - |  837 | `	}` |
|     62 |  838 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     62 |  839 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  840 | `		/* A closed or foreign resource is php's own TypeError, not a warning. */` |
|      3 |  841 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  842 | `			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");` |
|      - |  843 | `	}` |
|     60 |  844 | `	pStream = pDev->pStream;` |
|     60 |  845 | `	if( pStream == 0 ){` |
|    ! 0 |  846 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  847 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  848 | `			ph7_function_name(pCtx),"null_stream"` |
|      - |  849 | `			);` |
|    ! 0 |  850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  851 | `		return PH7_OK;` |
|      - |  852 | `	}` |
|     60 |  853 | `	nMaxLen = ph7_value_to_int64(apArg[1]);` |
|     60 |  854 | `	if( nMaxLen < 0 ){` |
|      3 |  855 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  856 | `			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");` |
|      - |  857 | `	}` |
|     58 |  858 | `	if( nMaxLen == 0 ){` |
|      - |  859 | `		/* php's documented default window */` |
|      3 |  860 | `		nMaxLen = 8192;` |
|      1 |  861 | `	}` |
|     58 |  862 | `	if( nArg > 2 ){` |
|     54 |  863 | `		zEnding = ph7_value_to_string(apArg[2],&nEndLen);` |
|     26 |  864 | `	}` |
|     58 |  865 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  866 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     30 |  867 | `		SyBlobReset(&pDev->sBuffer);` |
|     30 |  868 | `		pDev->nOfft = 0;` |
|     14 |  869 | `	}` |
|      - |  870 | `	/* Fill-and-scan: buffer chunks until the ending fits inside the window,` |
|      - |  871 | `	 * the window itself fills, or the stream dries up. */` |
|     62 |  872 | `	for(;;){` |
|    100 |  873 | `		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    100 |  874 | `		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    100 |  875 | `		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;` |
|      - |  876 | `		ph7_int64 n;` |
|      - |  877 | `		char zBuf[8192];` |
|    100 |  878 | `		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){` |
|      - |  879 | `			/* The ending must END inside the window to count. Resume the scan` |
|      - |  880 | `			 * where the previous fill left off — a candidate can straddle two` |
|      - |  881 | `			 * fills, so back up by the ending's length less one. */` |
|      - |  882 | `			sxu32 i;` |
|  40178 |  883 | `			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){` |
|  40146 |  884 | `				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){` |
|     26 |  885 | `					pDev->nOfft += i + (sxu32)nEndLen;` |
|     26 |  886 | `					ph7_result_string(pCtx,zData,(int)i);` |
|     42 |  887 | `					return PH7_OK;` |
|      - |  888 | `				}` |
|  20062 |  889 | `			}` |
|     34 |  890 | `			iScanFrom = i;` |
|     16 |  891 | `		}` |
|     76 |  892 | `		if( (ph7_int64)nAvail >= nMaxLen ){` |
|      - |  893 | `			/* Window full with no ending inside it: hand the window back raw,` |
|      - |  894 | `			 * anything past it (an ending included) stays buffered. */` |
|     18 |  895 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     18 |  896 | `			ph7_result_string(pCtx,zData,(int)nMaxLen);` |
|     18 |  897 | `			return PH7_OK;` |
|      - |  898 | `		}` |
|     60 |  899 | `		if( bEof ){` |
|      - |  900 | `			/* EOF: the remainder as-is, false when nothing is left. */` |
|     18 |  901 | `			if( nAvail > 0 ){` |
|     12 |  902 | `				pDev->nOfft += nAvail;` |
|     12 |  903 | `				ph7_result_string(pCtx,zData,(int)nAvail);` |
|      7 |  904 | `			}else{` |
|      8 |  905 | `				ph7_result_bool(pCtx,0);` |
|      - |  906 | `			}` |
|     18 |  907 | `			return PH7_OK;` |
|      - |  908 | `		}` |
|     44 |  909 | `		n = pStream->xRead(pDev->pHandle,zBuf,(ph7_int64)sizeof(zBuf));` |
|     44 |  910 | `		if( n < 1 ){` |
|     18 |  911 | `			bEof = 1;` |
|     18 |  912 | `			continue;` |
|      - |  913 | `		}` |
|     28 |  914 | `		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){` |
|    ! 0 |  915 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  916 | `		}` |
|      2 |  917 | `	}` |
|     33 |  918 | `}` |
|      - |  919 | `/*` |
|      - |  920 | ` * string fread(resource $handle,int64 $length)` |
|      - |  921 | ` *  Binary-safe file read.` |
|      - |  922 | ` * Parameters` |
|      - |  923 | ` *  $handle` |
|      - |  924 | ` *   The file pointer.` |
|      - |  925 | ` * $length` |
|      - |  926 | ` *  Up to length number of bytes read.` |
|      - |  927 | ` * Return` |
|      - |  928 | ` *  The data readen on success or FALSE on failure.` |
|      - |  929 | ` */` |
|     52 |  930 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  931 | `{` |
|      - |  932 | `	const ph7_io_stream *pStream;` |
|      - |  933 | `	io_private *pDev;` |
|      - |  934 | `	ph7_int64 nRead;` |
|      - |  935 | `	void *pBuf;` |
|      - |  936 | `	int nLen;` |
|     56 |  937 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  938 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  939 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  941 | `		return PH7_OK;` |
|      - |  942 | `	}` |
|      - |  943 | `	/* Extract our private data */` |
|     56 |  944 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  945 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     56 |  946 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  947 | `		/*Expecting an IO handle */` |
|    ! 0 |  948 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  949 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  950 | `		return PH7_OK;` |
|      - |  951 | `	}` |
|      - |  952 | `	/* Point to the target IO stream device */` |
|     56 |  953 | `	pStream = pDev->pStream;` |
|     56 |  954 | `	if( pStream == 0  ){` |
|    ! 0 |  955 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  956 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  957 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  958 | `			);` |
|    ! 0 |  959 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  960 | `		return PH7_OK;` |
|      - |  961 | `	}` |
|     56 |  962 | `        nLen = 4096;` |
|     56 |  963 | `	if( nArg > 1 ){` |
|      - |  964 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - |  965 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - |  966 | `	   * the central type screen (the recorded null-policy divergence). */` |
|     56 |  967 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|     56 |  968 | `	  if( nWant < 1 ){` |
|      5 |  969 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  970 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - |  971 | `	  }` |
|     52 |  972 | `	  nLen = (int)nWant;` |
|     52 |  973 | `	  if( nLen < 1 ){` |
|      - |  974 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - |  975 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - |  976 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - |  977 | `		 * that do not fit an int. */` |
|    ! 0 |  978 | `		nLen = 4096;` |
|    ! 0 |  979 | `	  }` |
|     24 |  980 | `        }` |
|      - |  981 | `	/* Allocate enough buffer */` |
|     52 |  982 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     52 |  983 | `	if( pBuf == 0 ){` |
|    ! 0 |  984 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  985 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|      - |  988 | `	/* Perform the requested operation */` |
|     52 |  989 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     52 |  990 | `	if( nRead < 0 ){` |
|      - |  991 | `		/* An IO ERROR, which is php's only false here */` |
|    ! 0 |  992 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  993 | `	}else{` |
|      - |  994 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|      - |  995 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|      - |  996 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|      - |  997 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|      - |  998 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|     52 |  999 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 1000 | `	}` |
|      - | 1001 | `	/* Release the buffer */` |
|     52 | 1002 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     52 | 1003 | `	return PH7_OK;` |
|     30 | 1004 | `}` |
|      - | 1005 | `/*` |
|      - | 1006 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 1007 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 1008 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 1009 | ` * Parameters` |
|      - | 1010 | ` * $handle` |
|      - | 1011 | ` *   The file pointer.` |
|      - | 1012 | ` * $length` |
|      - | 1013 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1014 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1015 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1016 | ` *  the end of the line.` |
|      - | 1017 | ` * $delimiter` |
|      - | 1018 | ` *   Set the field delimiter (one character only).` |
|      - | 1019 | ` * $enclosure` |
|      - | 1020 | ` *   Set the field enclosure character (one character only).` |
|      - | 1021 | ` * $escape` |
|      - | 1022 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 1023 | ` * Return` |
|      - | 1024 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 1025 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 1026 | ` *  If an error occurs, FALSE is returned.` |
|      - | 1027 | ` */` |
|     24 | 1028 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1029 | `{` |
|      - | 1030 | `	const ph7_io_stream *pStream;` |
|      - | 1031 | `	const char *zLine;` |
|      - | 1032 | `	io_private *pDev;` |
|      - | 1033 | `	ph7_int64 n,nLen;` |
|     25 | 1034 | `	int delim  = ',';   /* Delimiter */` |
|     25 | 1035 | `	int encl   = '"' ;  /* Enclosure */` |
|     25 | 1036 | `	int escape = '\\';  /* Escape character */` |
|     25 | 1037 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1038 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1039 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1041 | `		return PH7_OK;` |
|      - | 1042 | `	}` |
|      - | 1043 | `	/* Extract our private data */` |
|     25 | 1044 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1045 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     25 | 1046 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1047 | `		/*Expecting an IO handle */` |
|    ! 0 | 1048 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1050 | `		return PH7_OK;` |
|      - | 1051 | `	}` |
|      - | 1052 | `	/* Point to the target IO stream device */` |
|     25 | 1053 | `	pStream = pDev->pStream;` |
|     25 | 1054 | `	if( pStream == 0  ){` |
|    ! 0 | 1055 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1056 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1057 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1058 | `			);` |
|    ! 0 | 1059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1060 | `		return PH7_OK;` |
|      - | 1061 | `	}` |
|     25 | 1062 | `	if( nArg > 2 ){` |
|      - | 1063 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|      - | 1064 | `		 * ordering) and even when the stream is already at EOF. */` |
|     23 | 1065 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|     23 | 1066 | `		if( rc != PH7_OK ){` |
|      7 | 1067 | `			return rc;` |
|      - | 1068 | `		}` |
|     17 | 1069 | `		if( nArg > 3 ){` |
|     17 | 1070 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|     17 | 1071 | `			if( rc != PH7_OK ){` |
|      3 | 1072 | `				return rc;` |
|      - | 1073 | `			}` |
|     15 | 1074 | `			if( nArg > 4 ){` |
|     15 | 1075 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|     15 | 1076 | `				if( rc != PH7_OK ){` |
|      3 | 1077 | `					return rc;` |
|      - | 1078 | `				}` |
|      6 | 1079 | `			}` |
|      6 | 1080 | `		}` |
|      6 | 1081 | `	}` |
|     15 | 1082 | `	nLen = -1;` |
|     15 | 1083 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1084 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|      - | 1085 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|      - | 1086 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|      5 | 1087 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      5 | 1088 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|      3 | 1089 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1090 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|      - | 1091 | `		}` |
|      - | 1092 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|      1 | 1093 | `	}` |
|      - | 1094 | `	/* Perform the requested operation */` |
|     13 | 1095 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|     13 | 1096 | `	if( n < 1 ){` |
|      - | 1097 | `		/* EOF or IO error,return FALSE */` |
|      3 | 1098 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1099 | `	}else{` |
|      - | 1100 | `		ph7_value *pArray;` |
|      - | 1101 | `		/* Create our array */` |
|     11 | 1102 | `		pArray = ph7_context_new_array(pCtx);` |
|     11 | 1103 | `		if( pArray == 0 ){` |
|    ! 0 | 1104 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1105 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1106 | `			return PH7_OK;` |
|      - | 1107 | `		}` |
|      - | 1108 | `		/* Parse the raw input */` |
|     11 | 1109 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 1110 | `		/* Return the freshly created array  */` |
|     11 | 1111 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1112 | `	}` |
|     13 | 1113 | `	return PH7_OK;` |
|     13 | 1114 | `}` |
|      - | 1115 | `/*` |
|      - | 1116 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 1117 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 1118 | ` * Parameters` |
|      - | 1119 | ` * $handle` |
|      - | 1120 | ` *   The file pointer.` |
|      - | 1121 | ` * $length` |
|      - | 1122 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1123 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1124 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1125 | ` *  the end of the line.` |
|      - | 1126 | ` * $allowable_tags` |
|      - | 1127 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 1128 | ` * Return` |
|      - | 1129 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 1130 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 1131 | ` */` |
|      2 | 1132 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1133 | `{` |
|      - | 1134 | `	const ph7_io_stream *pStream;` |
|      - | 1135 | `	const char *zLine;` |
|      - | 1136 | `	io_private *pDev;` |
|      - | 1137 | `	ph7_int64 n,nLen;` |
|      3 | 1138 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1139 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1140 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1141 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1142 | `		return PH7_OK;` |
|      - | 1143 | `	}` |
|      - | 1144 | `	/* Extract our private data */` |
|      3 | 1145 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1146 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1147 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1148 | `		/*Expecting an IO handle */` |
|    ! 0 | 1149 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1150 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1151 | `		return PH7_OK;` |
|      - | 1152 | `	}` |
|      - | 1153 | `	/* Point to the target IO stream device */` |
|      3 | 1154 | `	pStream = pDev->pStream;` |
|      3 | 1155 | `	if( pStream == 0  ){` |
|    ! 0 | 1156 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1157 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1158 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1159 | `			);` |
|    ! 0 | 1160 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1161 | `		return PH7_OK;` |
|      - | 1162 | `	}` |
|      3 | 1163 | `	nLen = -1;` |
|      3 | 1164 | `	if( nArg > 1 ){` |
|      - | 1165 | `		/* Maximum data to read */` |
|    ! 0 | 1166 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 1167 | `	}` |
|      - | 1168 | `	/* Perform the requested operation */` |
|      3 | 1169 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 1170 | `	if( n < 1 ){` |
|      - | 1171 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 1172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1173 | `	}else{` |
|      3 | 1174 | `		const char *zTaglist = 0;` |
|      3 | 1175 | `		int nTaglen = 0;` |
|      3 | 1176 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 1177 | `			/* Allowed tag */` |
|    ! 0 | 1178 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 1179 | `		}` |
|      - | 1180 | `		/* Process data just read */` |
|      3 | 1181 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 1182 | `	}` |
|      3 | 1183 | `	return PH7_OK;` |
|      2 | 1184 | `}` |
|      - | 1185 | `/*` |
|      - | 1186 | ` * string readdir(resource $dir_handle)` |
|      - | 1187 | ` *   Read entry from directory handle.` |
|      - | 1188 | ` * Parameter` |
|      - | 1189 | ` *  $dir_handle` |
|      - | 1190 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1191 | ` * Return` |
|      - | 1192 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 1193 | ` */` |
|  12722 | 1194 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1195 | `{` |
|      - | 1196 | `	const ph7_io_stream *pStream;` |
|      - | 1197 | `	io_private *pDev;` |
|      - | 1198 | `	int rc;` |
|  12727 | 1199 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1200 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1201 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1203 | `		return PH7_OK;` |
|      - | 1204 | `	}` |
|      - | 1205 | `	/* Extract our private data */` |
|  12727 | 1206 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1207 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  12727 | 1208 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1209 | `		/*Expecting an IO handle */` |
|    ! 0 | 1210 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1212 | `		return PH7_OK;` |
|      - | 1213 | `	}` |
|      - | 1214 | `	/* Point to the target IO stream device */` |
|  12727 | 1215 | `	pStream = pDev->pStream;` |
|  12727 | 1216 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1217 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1218 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1219 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1220 | `			);` |
|    ! 0 | 1221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1222 | `		return PH7_OK;` |
|      - | 1223 | `	}` |
|  12727 | 1224 | `	ph7_result_bool(pCtx,0);` |
|      - | 1225 | `	/* Perform the requested operation */` |
|  12727 | 1226 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  12727 | 1227 | `	if( rc != PH7_OK ){` |
|      - | 1228 | `		/* Return FALSE */` |
|   1211 | 1229 | `		ph7_result_bool(pCtx,0);` |
|    603 | 1230 | `	}` |
|  12727 | 1231 | `	return PH7_OK;` |
|   6366 | 1232 | `}` |
|      - | 1233 | `/*` |
|      - | 1234 | ` * void rewinddir(resource $dir_handle)` |
|      - | 1235 | ` *   Rewind directory handle.` |
|      - | 1236 | ` * Parameter` |
|      - | 1237 | ` *  $dir_handle` |
|      - | 1238 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1239 | ` * Return` |
|      - | 1240 | ` *  FALSE on failure.` |
|      - | 1241 | ` */` |
|      4 | 1242 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1243 | `{` |
|      - | 1244 | `	const ph7_io_stream *pStream;` |
|      - | 1245 | `	io_private *pDev;` |
|      6 | 1246 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1247 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1248 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1250 | `		return PH7_OK;` |
|      - | 1251 | `	}` |
|      - | 1252 | `	/* Extract our private data */` |
|      6 | 1253 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1254 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      6 | 1255 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1256 | `		/*Expecting an IO handle */` |
|    ! 0 | 1257 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1259 | `		return PH7_OK;` |
|      - | 1260 | `	}` |
|      - | 1261 | `	/* Point to the target IO stream device */` |
|      6 | 1262 | `	pStream = pDev->pStream;` |
|      6 | 1263 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 1264 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1265 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1266 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1267 | `			);` |
|    ! 0 | 1268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1269 | `		return PH7_OK;` |
|      - | 1270 | `	}` |
|      - | 1271 | `	/* Perform the requested operation */` |
|      6 | 1272 | `	pStream->xRewindDir(pDev->pHandle);` |
|      6 | 1273 | `	return PH7_OK;` |
|      4 | 1274 | ` }` |
|      - | 1275 | `/* Forward declaration */` |
|      - | 1276 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 1277 | `/*` |
|      - | 1278 | ` * void closedir(resource $dir_handle)` |
|      - | 1279 | ` *   Close directory handle.` |
|      - | 1280 | ` * Parameter` |
|      - | 1281 | ` *  $dir_handle` |
|      - | 1282 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1283 | ` * Return` |
|      - | 1284 | ` *  FALSE on failure.` |
|      - | 1285 | ` */` |
|   1210 | 1286 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1287 | `{` |
|      - | 1288 | `	const ph7_io_stream *pStream;` |
|      - | 1289 | `	io_private *pDev;` |
|   1215 | 1290 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1291 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1292 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Extract our private data */` |
|   1215 | 1297 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1298 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1215 | 1299 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1300 | `		/*Expecting an IO handle */` |
|    ! 0 | 1301 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1303 | `		return PH7_OK;` |
|      - | 1304 | `	}` |
|      - | 1305 | `	/* Point to the target IO stream device */` |
|   1215 | 1306 | `	pStream = pDev->pStream;` |
|   1215 | 1307 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1308 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1309 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1310 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1311 | `			);` |
|    ! 0 | 1312 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1313 | `		return PH7_OK;` |
|      - | 1314 | `	}` |
|      - | 1315 | `	/* Perform the requested operation */` |
|   1215 | 1316 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1317 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1215 | 1318 | `	MarkIOPrivateClosed(pDev);` |
|   1215 | 1319 | `	return PH7_OK;` |
|    610 | 1320 | ` }` |
|      - | 1321 | `/*` |
|      - | 1322 | ` * resource opendir(string $path[,resource $context])` |
|      - | 1323 | ` *  Open directory handle.` |
|      - | 1324 | ` * Parameters` |
|      - | 1325 | ` * $path` |
|      - | 1326 | ` *   The directory path that is to be opened.` |
|      - | 1327 | ` * $context` |
|      - | 1328 | ` *   A context stream resource.` |
|      - | 1329 | ` * Return` |
|      - | 1330 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 1331 | ` */` |
|   1220 | 1332 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1333 | `{` |
|      - | 1334 | `	const ph7_io_stream *pStream;` |
|      - | 1335 | `	const char *zPath;` |
|      - | 1336 | `	io_private *pDev;` |
|      - | 1337 | `	int iLen,rc;` |
|   1225 | 1338 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1339 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1340 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1341 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1342 | `		return PH7_OK;` |
|      - | 1343 | `	}` |
|      - | 1344 | `	/* Extract the target path */` |
|   1225 | 1345 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1346 | `	/* Try to extract a stream */` |
|   1225 | 1347 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1225 | 1348 | `	if( pStream == 0 ){` |
|    ! 0 | 1349 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 1350 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 1351 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1352 | `		return PH7_OK;` |
|      - | 1353 | `	}` |
|   1225 | 1354 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1355 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1356 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1357 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1358 | `			);` |
|    ! 0 | 1359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1360 | `		return PH7_OK;` |
|      - | 1361 | `	}` |
|      - | 1362 | `	/* Allocate a new IO private instance */` |
|   1225 | 1363 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1225 | 1364 | `	if( pDev == 0 ){` |
|    ! 0 | 1365 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1367 | `		return PH7_OK;` |
|      - | 1368 | `	}` |
|      - | 1369 | `	/* Initialize the structure */` |
|   1225 | 1370 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1371 | `	/* Open the target directory */` |
|   1225 | 1372 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1225 | 1373 | `	if( rc != PH7_OK ){` |
|      - | 1374 | ``		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No`` |
|      - | 1375 | ``		 * such file or directory` — and PHL returned FALSE in silence. The message`` |
|      - | 1376 | `` 		 * names the ACTIVE function, which is how dir() gets php's `dir(...)` `` |
|      - | 1377 | `		 * wording out of the same call. */` |
|     17 | 1378 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",` |
|     10 | 1379 | `			ph7_function_name(pCtx),zPath,VfsStrerror(errno));` |
|     12 | 1380 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     12 | 1381 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1382 | `	}else{` |
|      - | 1383 | `		/* Return the handle as a resource */` |
|   1215 | 1384 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1385 | `	}` |
|   1225 | 1386 | `	return PH7_OK;` |
|    615 | 1387 | `}` |
|      - | 1388 | `/*` |
|      - | 1389 | ``  * `dir(string $directory, $context = null): Directory\|false` `` |
|      - | 1390 | ` *` |
|      - | 1391 | ` * php's own dir() opens the stream and fills the object itself, which is why its` |
|      - | 1392 | ` * class needs no constructor. The open goes through the engine's opendir builtin` |
|      - | 1393 | `` * with THIS context, so the failure warning names `dir(...)` exactly as php's`` |
|      - | 1394 | ` * does; a failed open is FALSE, where the chunk's version handed back a Directory` |
|      - | 1395 | `` * whose handle was `false`.`` |
|      - | 1396 | ` */` |
|      4 | 1397 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1398 | `{` |
|      - | 1399 | `	ph7_class_instance *pObj;` |
|      - | 1400 | `	ph7_class *pClass;` |
|      - | 1401 | `	ph7_value *pRet;` |
|      - | 1402 | `	int rc;` |
|      5 | 1403 | `	rc = PH7_builtin_opendir(pCtx,nArg,apArg);` |
|      5 | 1404 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1405 | `		return rc;` |
|      - | 1406 | `	}` |
|      5 | 1407 | `	pRet = pCtx->pRet;` |
|      5 | 1408 | `	if( pRet == 0 \|\| (pRet->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 | 1409 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1410 | `		return PH7_OK;` |
|      - | 1411 | `	}` |
|      3 | 1412 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);` |
|      3 | 1413 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|      3 | 1414 | `	if( pObj == 0 ){` |
|    ! 0 | 1415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1416 | `		return PH7_OK;` |
|      - | 1417 | `	}` |
|      - | 1418 | `	/* php's order: the path first, then the handle (var_dump shows both). */` |
|      - | 1419 | `	{` |
|      3 | 1420 | `		int nPath = 0;` |
|      3 | 1421 | `		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";` |
|      3 | 1422 | `		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);` |
|      - | 1423 | `	}` |
|      3 | 1424 | `	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);` |
|      3 | 1425 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      3 | 1426 | `	return PH7_OK;` |
|      3 | 1427 | `}` |
|      - | 1428 | `/*` |
|      - | 1429 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 1430 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 1431 | ` * Parameters` |
|      - | 1432 | ` *  $filename` |
|      - | 1433 | ` *   The filename being read.` |
|      - | 1434 | ` *  $use_include_path` |
|      - | 1435 | ` *   You can use the optional second parameter and set it to` |
|      - | 1436 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1437 | ` *  $context` |
|      - | 1438 | ` *   A context stream resource.` |
|      - | 1439 | ` * Return` |
|      - | 1440 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 1441 | ` */` |
|      - | 1442 | `/*` |
|      - | 1443 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 1444 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 1445 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 1446 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 1447 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 1448 | ` * trailing reason comes from.` |
|      - | 1449 | ` */` |
|      2 | 1450 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1451 | `{` |
|      3 | 1452 | `	int use_include  = FALSE;` |
|      - | 1453 | `	const ph7_io_stream *pStream;` |
|      - | 1454 | `	ph7_int64 n,nRead;` |
|      - | 1455 | `	const char *zFile;` |
|      - | 1456 | `	char zBuf[8192];` |
|      - | 1457 | `	void *pHandle;` |
|      - | 1458 | `	int rc,nLen;` |
|      3 | 1459 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1460 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1461 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1462 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1463 | `		return PH7_OK;` |
|      - | 1464 | `	}` |
|      - | 1465 | `	/* Extract the file path */` |
|      3 | 1466 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1467 | `	/* Point to the target IO stream device */` |
|      3 | 1468 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1469 | `	if( pStream == 0 ){` |
|    ! 0 | 1470 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1471 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1472 | `		return PH7_OK;` |
|      - | 1473 | `	}` |
|      3 | 1474 | `	if( nArg > 1 ){` |
|    ! 0 | 1475 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 1476 | `	}` |
|      - | 1477 | `	/* Try to open the file in read-only mode */` |
|      4 | 1478 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 1479 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1480 | `	if( pHandle == 0 ){` |
|    ! 0 | 1481 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1483 | `		return PH7_OK;` |
|      - | 1484 | `	}` |
|      - | 1485 | `	/* Perform the requested operation */` |
|      3 | 1486 | `	nRead = 0;` |
|      2 | 1487 | `	for(;;){` |
|      5 | 1488 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 1489 | `		if( n < 1 ){` |
|      - | 1490 | `			/* EOF or IO error,break immediately */` |
|      3 | 1491 | `			break;` |
|      - | 1492 | `		}` |
|      - | 1493 | `		/* Output data */` |
|      3 | 1494 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 1495 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 1496 | `			break;` |
|      - | 1497 | `		}` |
|      - | 1498 | `		/* Increment counter */` |
|      3 | 1499 | `		nRead += n;` |
|      1 | 1500 | `	}` |
|      - | 1501 | `	/* Close the stream */` |
|      3 | 1502 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1503 | `	/* Total number of bytes readen */` |
|      3 | 1504 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 1505 | `	return PH7_OK;` |
|      2 | 1506 | `}` |
|      - | 1507 | `/*` |
|      - | 1508 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 1509 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 1510 | ` *  Reads entire file into a string.` |
|      - | 1511 | ` * Parameters` |
|      - | 1512 | ` *  $filename` |
|      - | 1513 | ` *   The filename being read.` |
|      - | 1514 | ` *  $use_include_path` |
|      - | 1515 | ` *   You can use the optional second parameter and set it to` |
|      - | 1516 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1517 | ` *  $context` |
|      - | 1518 | ` *   A context stream resource.` |
|      - | 1519 | ` *  $offset` |
|      - | 1520 | ` *   The offset where the reading starts on the original stream.` |
|      - | 1521 | ` *  $maxlen` |
|      - | 1522 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 1523 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 1524 | ` * Return` |
|      - | 1525 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1526 | ` */` |
|   7778 | 1527 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1528 | `{` |
|      - | 1529 | `	const ph7_io_stream *pStream;` |
|      - | 1530 | `	ph7_int64 n,nRead,nMaxlen;` |
|   7783 | 1531 | `	int use_include  = FALSE;` |
|      - | 1532 | `	const char *zFile;` |
|      - | 1533 | `	char zBuf[8192];` |
|      - | 1534 | `	void *pHandle;` |
|      - | 1535 | `	int nLen;` |
|      - | 1536 |  |
|   7783 | 1537 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1538 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1539 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1541 | `		return PH7_OK;` |
|      - | 1542 | `	}` |
|      - | 1543 | `	/* Extract the file path */` |
|   7783 | 1544 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1545 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 1546 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 1547 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 1548 | `	 * the whole file. */` |
|   7783 | 1549 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 1550 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 1551 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1552 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 1553 | `		}` |
|     11 | 1554 | `	}` |
|      - | 1555 | `	/* Point to the target IO stream device */` |
|   7779 | 1556 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   7779 | 1557 | `	if( pStream == 0 ){` |
|    ! 0 | 1558 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1560 | `		return PH7_OK;` |
|      - | 1561 | `	}` |
|   7779 | 1562 | `	nMaxlen = -1;` |
|   7779 | 1563 | `	if( nArg > 1 ){` |
|     25 | 1564 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     12 | 1565 | `	}` |
|      - | 1566 | `	/* Try to open the file in read-only mode */` |
|   7779 | 1567 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   7779 | 1568 | `	if( pHandle == 0 ){` |
|      3 | 1569 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1570 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1571 | `		return PH7_OK;` |
|      - | 1572 | `	}` |
|   7777 | 1573 | `	if( nArg > 3 ){` |
|      - | 1574 | `		/* Extract the offset */` |
|     25 | 1575 | `		n = ph7_value_to_int64(apArg[3]);` |
|     25 | 1576 | `		if( n > 0 ){` |
|      7 | 1577 | `			if( pStream->xSeek ){` |
|      - | 1578 | `				/* Seek to the desired offset */` |
|      7 | 1579 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|      3 | 1580 | `			}` |
|      3 | 1581 | `		}` |
|     25 | 1582 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      - | 1583 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|      - | 1584 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|      - | 1585 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|     23 | 1586 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|     11 | 1587 | `		}` |
|     12 | 1588 | `	}` |
|      - | 1589 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|      - | 1590 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|      - | 1591 | `	 * through to the empty-string result below. */` |
|   7777 | 1592 | `	nRead = 0;` |
|  15559 | 1593 | `	while( nMaxlen != 0 ){` |
|      - | 1594 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 1595 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 1596 | `		 * otherwise overshoot and append past $length. */` |
|  15555 | 1597 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  15555 | 1598 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 1599 | `			nAsk = nMaxlen - nRead;` |
|      8 | 1600 | `		}` |
|  15555 | 1601 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  15555 | 1602 | `		if( n < 1 ){` |
|      - | 1603 | `			/* EOF or IO error,break immediately */` |
|   7759 | 1604 | `			break;` |
|      - | 1605 | `		}` |
|      - | 1606 | `		/* Append data */` |
|   7801 | 1607 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 1608 | `		/* Increment read counter */` |
|   7801 | 1609 | `		nRead += n;` |
|   7801 | 1610 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 1611 | `			/* Read limit reached */` |
|     15 | 1612 | `			break;` |
|      - | 1613 | `		}` |
|      5 | 1614 | `	}` |
|      - | 1615 | `	/* Close the stream */` |
|   7777 | 1616 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1617 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 1618 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 1619 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   7777 | 1620 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     17 | 1621 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 1622 | `	}` |
|   7777 | 1623 | `	return PH7_OK;` |
|   3894 | 1624 | `}` |
|      - | 1625 | `/*` |
|      - | 1626 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 1627 | ` *  Write a string to a file.` |
|      - | 1628 | ` * Parameters` |
|      - | 1629 | ` *  $filename` |
|      - | 1630 | ` *  Path to the file where to write the data.` |
|      - | 1631 | ` * $data` |
|      - | 1632 | ` *  The data to write(Must be a string).` |
|      - | 1633 | ` * $flags` |
|      - | 1634 | ` *  The value of flags can be any combination of the following` |
|      - | 1635 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 1636 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 1637 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 1638 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 1639 | ` * context` |
|      - | 1640 | ` *  A context stream resource.` |
|      - | 1641 | ` * Return` |
|      - | 1642 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 1643 | ` */` |
|      - | 1644 | `/*` |
|      - | 1645 | ` * Append a buffer to a file, creating it when absent, and raise php's open` |
|      - | 1646 | `` * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's`` |
|      - | 1647 | ` * name when it cannot be opened. Returns PH7_OK or -1.` |
|      - | 1648 | ` *` |
|      - | 1649 | ` * This is error_log()'s message_type 3, factored here because that is where the` |
|      - | 1650 | ` * stream device, the open flags and the warning shape already live.` |
|      - | 1651 | ` */` |
|      6 | 1652 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)` |
|      1 | 1653 | `{` |
|      - | 1654 | `	const ph7_io_stream *pStream;` |
|      - | 1655 | `	void *pHandle;` |
|      - | 1656 | `	int nPath;` |
|      7 | 1657 | `	if( zFile == 0 \|\| zFile[0] == 0 ){` |
|    ! 0 | 1658 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1659 | `		return -1;` |
|      - | 1660 | `	}` |
|      7 | 1661 | `	nPath = (int)SyStrlen(zFile);` |
|      7 | 1662 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);` |
|      7 | 1663 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|    ! 0 | 1664 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1665 | `		return -1;` |
|      - | 1666 | `	}` |
|      7 | 1667 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|      - | 1668 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0);` |
|      7 | 1669 | `	if( pHandle == 0 ){` |
|      3 | 1670 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1671 | `		return -1;` |
|      - | 1672 | `	}` |
|      5 | 1673 | `	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){` |
|    ! 0 | 1674 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 1675 | `		return -1;` |
|      - | 1676 | `	}` |
|      5 | 1677 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      5 | 1678 | `	return PH7_OK;` |
|      4 | 1679 | `}` |
|  14866 | 1680 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1681 | `{` |
|  14871 | 1682 | `	int use_include  = FALSE;` |
|      - | 1683 | `	const ph7_io_stream *pStream;` |
|      - | 1684 | `	const char *zFile;` |
|      - | 1685 | `	const char *zData;` |
|      - | 1686 | `	int iOpenFlags;` |
|      - | 1687 | `	void *pHandle;` |
|      - | 1688 | `	int iFlags;` |
|      - | 1689 | `	int nLen;` |
|      - | 1690 |  |
|  14871 | 1691 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1692 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1693 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Extract the file path */` |
|  14871 | 1698 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1699 | `	/* Point to the target IO stream device */` |
|  14871 | 1700 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  14871 | 1701 | `	if( pStream == 0 ){` |
|    ! 0 | 1702 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1703 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1704 | `		return PH7_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Data to write */` |
|  14871 | 1707 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1708 | `	/* Try to open the file in read-write mode */` |
|  14871 | 1709 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 1710 | `	/* Extract the flags */` |
|  14871 | 1711 | `	iFlags = 0;` |
|  14871 | 1712 | `	if( nArg > 2 ){` |
|    ! 0 | 1713 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 1714 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 1715 | `			use_include = TRUE;` |
|    ! 0 | 1716 | `		}` |
|    ! 0 | 1717 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 1718 | `			/* If the file already exists, append the data to the file` |
|      - | 1719 | `			 * instead of overwriting it.` |
|      - | 1720 | `			 */` |
|    ! 0 | 1721 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 1722 | `			/* Append mode */` |
|    ! 0 | 1723 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 1724 | `		}` |
|    ! 0 | 1725 | `	}` |
|  22304 | 1726 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   7433 | 1727 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  14871 | 1728 | `	if( pHandle == 0 ){` |
|    ! 0 | 1729 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1731 | `		return PH7_OK;` |
|      - | 1732 | `	}` |
|  14871 | 1733 | `	if( nLen < 1 ){` |
|      - | 1734 | `		/* Empty data, file is created/truncated */` |
|     12 | 1735 | `		ph7_result_int64(pCtx,0);` |
|     12 | 1736 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     12 | 1737 | `		return PH7_OK;` |
|      - | 1738 | `	}` |
|  14861 | 1739 | `	if( pStream->xWrite ){` |
|      - | 1740 | `		ph7_int64 n;` |
|  14861 | 1741 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 1742 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 1743 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 1744 | `		}` |
|      - | 1745 | `		/* Perform the write operation */` |
|  14861 | 1746 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  14861 | 1747 | `		if( n < 0 ){` |
|      - | 1748 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 1749 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1750 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 1751 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 1752 | `			ph7_result_bool(pCtx,0);` |
|      1 | 1753 | `		}else{` |
|      - | 1754 | `			/* Total number of bytes written */` |
|  14861 | 1755 | `			ph7_result_int64(pCtx,n);` |
|      - | 1756 | `		}` |
|   7433 | 1757 | `	}else{` |
|      - | 1758 | `		/* Read-only stream */` |
|    ! 0 | 1759 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 1760 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 1761 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 1762 | `			);` |
|    ! 0 | 1763 | `		ph7_result_bool(pCtx,0);` |
|      - | 1764 | `	}` |
|      - | 1765 | `	/* Close the handle */` |
|  14861 | 1766 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  14861 | 1767 | `	return PH7_OK;` |
|   7438 | 1768 | `}` |
|      - | 1769 | `/*` |
|      - | 1770 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 1771 | ` *  Reads entire file into an array.` |
|      - | 1772 | ` * Parameters` |
|      - | 1773 | ` *  $filename` |
|      - | 1774 | ` *   The filename being read.` |
|      - | 1775 | ` *  $flags` |
|      - | 1776 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 1777 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 1778 | ` *       Search for the file in the include_path.` |
|      - | 1779 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 1780 | ` *       Do not add newline at the end of each array element` |
|      - | 1781 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 1782 | ` *       Skip empty lines` |
|      - | 1783 | ` *  $context` |
|      - | 1784 | ` *   A context stream resource.` |
|      - | 1785 | ` * Return` |
|      - | 1786 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1787 | ` */` |
|     46 | 1788 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1789 | `{` |
|      - | 1790 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 1791 | `	ph7_value *pArray,*pLine;` |
|      - | 1792 | `	const ph7_io_stream *pStream;` |
|     49 | 1793 | `	int use_include = 0;` |
|      - | 1794 | `	io_private *pDev;` |
|      - | 1795 | `	ph7_int64 n;` |
|      - | 1796 | `	int iFlags;` |
|      - | 1797 | `	int nLen;` |
|      - | 1798 |  |
|     49 | 1799 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1800 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1801 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1802 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1803 | `		return PH7_OK;` |
|      - | 1804 | `	}` |
|     49 | 1805 | `	iFlags = 0;` |
|     49 | 1806 | `	if( nArg > 1 ){` |
|      - | 1807 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|      - | 1808 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|      - | 1809 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|      - | 1810 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|      - | 1811 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|      - | 1812 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|      - | 1813 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|      - | 1814 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|      - | 1815 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|      - | 1816 | `		 * mask. */` |
|     37 | 1817 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     37 | 1818 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|     13 | 1819 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1820 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|      - | 1821 | `		}` |
|     25 | 1822 | `		iFlags = (int)nFlags;` |
|     12 | 1823 | `	}` |
|      - | 1824 | `	/* Extract the file path */` |
|     37 | 1825 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1826 | `	/* Point to the target IO stream device */` |
|     37 | 1827 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     37 | 1828 | `	if( pStream == 0 ){` |
|    ! 0 | 1829 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|      - | 1833 | `	/* Allocate a new IO private instance */` |
|     37 | 1834 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     37 | 1835 | `	if( pDev == 0 ){` |
|    ! 0 | 1836 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1837 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1838 | `		return PH7_OK;` |
|      - | 1839 | `	}` |
|      - | 1840 | `	/* Initialize the structure */` |
|     37 | 1841 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     37 | 1842 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 1843 | `		use_include = TRUE;` |
|      1 | 1844 | `	}` |
|      - | 1845 | `	/* Create the array and the working value */` |
|     37 | 1846 | `	pArray = ph7_context_new_array(pCtx);` |
|     37 | 1847 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     37 | 1848 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 1849 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1851 | `		return PH7_OK;` |
|      - | 1852 | `	}` |
|      - | 1853 | `	/* Try to open the file in read-only mode */` |
|     37 | 1854 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     37 | 1855 | `	if( pDev->pHandle == 0 ){` |
|     10 | 1856 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 1857 | `		ph7_result_bool(pCtx,0);` |
|      - | 1858 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 1859 | `		 * as soon we return from this function.` |
|      - | 1860 | `		 */` |
|     10 | 1861 | `		return PH7_OK;` |
|      - | 1862 | `	}` |
|      - | 1863 | `	/* Perform the requested operation */` |
|     60 | 1864 | `	for(;;){` |
|      - | 1865 | `		/* Try to extract a line */` |
|    125 | 1866 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|    125 | 1867 | `		if( n < 1 ){` |
|      - | 1868 | `			/* EOF or IO error */` |
|     27 | 1869 | `			break;` |
|      - | 1870 | `		}` |
|      - | 1871 | `		/* Reset the cursor */` |
|     99 | 1872 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 1873 | `		/* Remove line ending if requested by the caller */` |
|     99 | 1874 | `		zPtr = zBuf;` |
|     99 | 1875 | `		zEnd = &zBuf[n];` |
|     99 | 1876 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 1877 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|      - | 1878 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|      - | 1879 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|      - | 1880 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|      - | 1881 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     55 | 1882 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     43 | 1883 | `				n--;` |
|     43 | 1884 | `				zEnd--;` |
|     43 | 1885 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|     13 | 1886 | `					n--;` |
|     13 | 1887 | `					zEnd--;` |
|      6 | 1888 | `				}` |
|     21 | 1889 | `			}` |
|     27 | 1890 | `		}` |
|     99 | 1891 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 1892 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|      - | 1893 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|      - | 1894 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|      - | 1895 | `			 * line (never zero-length, since the newline is still attached) was` |
|      - | 1896 | `			 * dropped too. Both are silent data loss from a read. */` |
|     31 | 1897 | `			if( zEnd <= zPtr ){` |
|      5 | 1898 | `				continue;` |
|      - | 1899 | `			}` |
|     13 | 1900 | `		}` |
|     95 | 1901 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 1902 | `		/* Insert line */` |
|     95 | 1903 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 1904 | `	}` |
|      - | 1905 | `	/* Close the stream */` |
|     27 | 1906 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 1907 | `	/* Release the io_private instance */` |
|     27 | 1908 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 1909 | `	/* Return the created array */` |
|     27 | 1910 | `	ph7_result_value(pCtx,pArray);` |
|     27 | 1911 | `	return PH7_OK;` |
|     26 | 1912 | `}` |
|      - | 1913 | `/*` |
|      - | 1914 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 1915 | ` *  Makes a copy of the file source to dest.` |
|      - | 1916 | ` * Parameters` |
|      - | 1917 | ` *  $source` |
|      - | 1918 | ` *   Path to the source file.` |
|      - | 1919 | ` *  $dest` |
|      - | 1920 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 1921 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 1922 | ` *  $context` |
|      - | 1923 | ` *   A context stream resource.` |
|      - | 1924 | ` * Return` |
|      - | 1925 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1926 | ` */` |
|      4 | 1927 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1928 | `{` |
|      - | 1929 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 1930 | `	const char *zFile;` |
|      - | 1931 | `	char zBuf[8192];` |
|      - | 1932 | `	void *pIn,*pOut;` |
|      - | 1933 | `	ph7_int64 n;` |
|      - | 1934 | `	int nLen;` |
|      6 | 1935 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 1936 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1937 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 1938 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1939 | `		return PH7_OK;` |
|      - | 1940 | `	}` |
|      - | 1941 | `	/* Extract the source name */` |
|      6 | 1942 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1943 | `	/* Point to the target IO stream device */` |
|      6 | 1944 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 1945 | `	if( pSin == 0 ){` |
|    ! 0 | 1946 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|      - | 1950 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 1951 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 1952 | `	if( pIn == 0 ){` |
|      3 | 1953 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1954 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1955 | `		return PH7_OK;` |
|      - | 1956 | `	}` |
|      - | 1957 | `	/* Extract the destination name */` |
|      3 | 1958 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1959 | `	/* Point to the target IO stream device */` |
|      3 | 1960 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1961 | `	if( pSout == 0 ){` |
|    ! 0 | 1962 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1963 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1964 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1965 | `		return PH7_OK;` |
|      - | 1966 | `	}` |
|      3 | 1967 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 1968 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1969 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1970 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 1971 | `			);` |
|    ! 0 | 1972 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1973 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1974 | `		return PH7_OK;` |
|      - | 1975 | `	}` |
|      - | 1976 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 1977 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 1978 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1979 | `	if( pOut == 0 ){` |
|    ! 0 | 1980 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1982 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1983 | `		return PH7_OK;` |
|      - | 1984 | `	}` |
|      - | 1985 | `	/* Perform the requested operation */` |
|      2 | 1986 | `	for(;;){` |
|      - | 1987 | `		/* Read from source */` |
|      5 | 1988 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 1989 | `		if( n < 1 ){` |
|      - | 1990 | `			/* EOF or IO error,break immediately */` |
|      3 | 1991 | `			break;` |
|      - | 1992 | `		}` |
|      - | 1993 | `		/* Write to dest */` |
|      3 | 1994 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 1995 | `		if( n < 1 ){` |
|      - | 1996 | `			/* IO error,break immediately */` |
|    ! 0 | 1997 | `			break;` |
|      - | 1998 | `		}` |
|      1 | 1999 | `	}` |
|      - | 2000 | `	/* Close the streams */` |
|      3 | 2001 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 2002 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 2003 | `	/* Return TRUE */` |
|      3 | 2004 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2005 | `	return PH7_OK;` |
|      4 | 2006 | `}` |
|      - | 2007 | `/*` |
|      - | 2008 | ` * array fstat(resource $handle)` |
|      - | 2009 | ` *  Gets information about a file using an open file pointer.` |
|      - | 2010 | ` * Parameters` |
|      - | 2011 | ` *  $handle` |
|      - | 2012 | ` *   The file pointer.` |
|      - | 2013 | ` * Return` |
|      - | 2014 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 2015 | ` */` |
|      4 | 2016 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2017 | `{` |
|      - | 2018 | `	ph7_value *pArray,*pValue;` |
|      - | 2019 | `	const ph7_io_stream *pStream;` |
|      - | 2020 | `	io_private *pDev;` |
|      5 | 2021 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2022 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2023 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2025 | `		return PH7_OK;` |
|      - | 2026 | `	}` |
|      - | 2027 | `	/* Extract our private data */` |
|      5 | 2028 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2029 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2030 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2031 | `		/* Expecting an IO handle */` |
|    ! 0 | 2032 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2034 | `		return PH7_OK;` |
|      - | 2035 | `	}` |
|      - | 2036 | `	/* Point to the target IO stream device */` |
|      5 | 2037 | `	pStream = pDev->pStream;` |
|      5 | 2038 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 2039 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2040 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2041 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2042 | `			);` |
|    ! 0 | 2043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2044 | `		return PH7_OK;` |
|      - | 2045 | `	}` |
|      - | 2046 | `	/* Create the array and the working value */` |
|      5 | 2047 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 2048 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 2049 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2050 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2051 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2052 | `		return PH7_OK;` |
|      - | 2053 | `	}` |
|      - | 2054 | `	/* Perform the requested operation */` |
|      5 | 2055 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 2056 | `	/* php answers the same thirteen fields twice -- numeric 0..12, then named` |
|      - | 2057 | `	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and` |
|      - | 2058 | `	 * had the same missing half. */` |
|      - | 2059 | `	{` |
|      5 | 2060 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      5 | 2061 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      5 | 2062 | `			ph7_result_value(pCtx,pFull);` |
|      5 | 2063 | `			return PH7_OK;` |
|      - | 2064 | `		}` |
|      - | 2065 | `	}` |
|      - | 2066 | `	/* Return the freshly created array */` |
|    ! 0 | 2067 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2068 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 2069 | `	 * released automatically as soon we return from this function.` |
|      - | 2070 | `	 */` |
|    ! 0 | 2071 | `	return PH7_OK;` |
|      3 | 2072 | `}` |
|      - | 2073 | `/*` |
|      - | 2074 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 2075 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 2076 | ` * Parameters` |
|      - | 2077 | ` *  $handle` |
|      - | 2078 | ` *   The file pointer.` |
|      - | 2079 | ` *  $string` |
|      - | 2080 | ` *   The string that is to be written.` |
|      - | 2081 | ` *  $length` |
|      - | 2082 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 2083 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 2084 | ` * Return` |
|      - | 2085 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 2086 | ` */` |
|    112 | 2087 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2088 | `{` |
|      - | 2089 | `	const ph7_io_stream *pStream;` |
|      - | 2090 | `	const char *zString;` |
|      - | 2091 | `	io_private *pDev;` |
|      - | 2092 | `	int nLen,n;` |
|    116 | 2093 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2094 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2095 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2096 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2097 | `		return PH7_OK;` |
|      - | 2098 | `	}` |
|      - | 2099 | `	/* Extract our private data */` |
|    116 | 2100 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2101 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    116 | 2102 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2103 | `		/* Expecting an IO handle */` |
|    ! 0 | 2104 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2106 | `		return PH7_OK;` |
|      - | 2107 | `	}` |
|      - | 2108 | `	/* Point to the target IO stream device */` |
|    116 | 2109 | `	pStream = pDev->pStream;` |
|    116 | 2110 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2111 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2112 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2113 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2114 | `			);` |
|    ! 0 | 2115 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Extract the data to write */` |
|    116 | 2119 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|    116 | 2120 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 2121 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|      - | 2122 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|      - | 2123 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|      - | 2124 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|      - | 2125 | `		 * and write the whole string). */` |
|     13 | 2126 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|     13 | 2127 | `		if( nMax < 0 ){` |
|      3 | 2128 | `			nLen = 0;` |
|     12 | 2129 | `		}else if( nMax < (sxi64)nLen ){` |
|      5 | 2130 | `			nLen = (int)nMax;` |
|      2 | 2131 | `		}` |
|      6 | 2132 | `	}` |
|    116 | 2133 | `	if( nLen < 1 ){` |
|      - | 2134 | `		/* Nothing to write */` |
|      5 | 2135 | `		ph7_result_int(pCtx,0);` |
|      5 | 2136 | `		return PH7_OK;` |
|      - | 2137 | `	}` |
|    112 | 2138 | `	if( pDev->nOfft < SyBlobLength(&pDev->sBuffer) && pStream->xSeek ){` |
|      - | 2139 | `		/* The device sits PAST the line readers' read-ahead: php writes at the` |
|      - | 2140 | `		 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|      - | 2141 | `		 * unread), so step the device back by the unconsumed remainder and` |
|      - | 2142 | `		 * drop the buffer — the ftell()/SEEK_CUR rule, applied to the write. */` |
|      7 | 2143 | `		pStream->xSeek(pDev->pHandle,` |
|      4 | 2144 | `			-(ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft),1/*SEEK_CUR*/);` |
|      5 | 2145 | `		ResetIOPrivate(pDev);` |
|      2 | 2146 | `	}` |
|      - | 2147 | `	/* Perform the requested operation */` |
|    112 | 2148 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|    112 | 2149 | `	if( n <  0 ){` |
|      - | 2150 | `		/* IO error,return FALSE */` |
|    ! 0 | 2151 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2152 | `	}else{` |
|      - | 2153 | `		/* #Bytes written */` |
|    112 | 2154 | `		ph7_result_int(pCtx,n);` |
|      - | 2155 | `	}` |
|    112 | 2156 | `	return PH7_OK;` |
|     60 | 2157 | `}` |
|      - | 2158 | `/*` |
|      - | 2159 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|      - | 2160 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|      - | 2161 | ` * script can tell contention (1) from a plain failure (0).` |
|      - | 2162 | ` */` |
|     30 | 2163 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|      1 | 2164 | `{` |
|      - | 2165 | `	ph7_value sVal;` |
|     31 | 2166 | `	if( nArg < 3 ){` |
|     19 | 2167 | `		return;` |
|      - | 2168 | `	}` |
|     13 | 2169 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|     13 | 2170 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|     13 | 2171 | `	PH7_MemObjRelease(&sVal);` |
|     16 | 2172 | `}` |
|      - | 2173 | `/*` |
|      - | 2174 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|      - | 2175 | ` *  Portable advisory file locking.` |
|      - | 2176 | ` * Parameters` |
|      - | 2177 | ` *  $handle` |
|      - | 2178 | ` *   The file pointer.` |
|      - | 2179 | ` *  $operation` |
|      - | 2180 | ` *   operation is one of the following:` |
|      - | 2181 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 2182 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 2183 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 2184 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|      - | 2185 | ` *  &$would_block` |
|      - | 2186 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|      - | 2187 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|      - | 2188 | ` * Return` |
|      - | 2189 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 2190 | ` */` |
|     38 | 2191 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2192 | `{` |
|      - | 2193 | `	const ph7_io_stream *pStream;` |
|      - | 2194 | `	io_private *pDev;` |
|      - | 2195 | `	int nLock;` |
|      - | 2196 | `	int rc;` |
|     39 | 2197 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2198 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2199 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2200 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2201 | `		return PH7_OK;` |
|      - | 2202 | `	}` |
|      - | 2203 | `	/* Extract our private data */` |
|     39 | 2204 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2205 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     39 | 2206 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2207 | `		/*Expecting an IO handle */` |
|    ! 0 | 2208 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2209 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2210 | `		return PH7_OK;` |
|      - | 2211 | `	}` |
|      - | 2212 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|      - | 2213 | `	 * support is considered: the low two bits select the action (its bison` |
|      - | 2214 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|      - | 2215 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|     39 | 2216 | `	nLock = ph7_value_to_int(apArg[1]);` |
|     39 | 2217 | `	if( (nLock & 3) == 0 ){` |
|      9 | 2218 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2219 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|      - | 2220 | `	}` |
|      - | 2221 | `	/* Point to the target IO stream device */` |
|     31 | 2222 | `	pStream = pDev->pStream;` |
|     31 | 2223 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|      - | 2224 | `		/* php returns FALSE silently when the stream does not support locking` |
|      - | 2225 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|      7 | 2226 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|      7 | 2227 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2228 | `		return PH7_OK;` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/*` |
|      - | 2231 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|      - | 2232 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|      - | 2233 | `	 */` |
|      - | 2234 | `	{` |
|     25 | 2235 | `		int iOp = nLock & 3;` |
|     25 | 2236 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|     25 | 2237 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      9 | 2238 | `			nLock = -1;` |
|     21 | 2239 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|     11 | 2240 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|      6 | 2241 | `		}else{` |
|      7 | 2242 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|      - | 2243 | `		}` |
|      - | 2244 | `	}` |
|      - | 2245 | `	/* Lock operation */` |
|     25 | 2246 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 2247 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|      - | 2248 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|     25 | 2249 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|      - | 2250 | `	/* IO result */` |
|     25 | 2251 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     25 | 2252 | `	return PH7_OK;` |
|     20 | 2253 | `}` |
|      - | 2254 | `/*` |
|      - | 2255 | ` * int fpassthru(resource $handle)` |
|      - | 2256 | ` *  Output all remaining data on a file pointer.` |
|      - | 2257 | ` * Parameters` |
|      - | 2258 | ` *  $handle` |
|      - | 2259 | ` *   The file pointer.` |
|      - | 2260 | ` * Return` |
|      - | 2261 | ` *  Total number of characters read from handle and passed through` |
|      - | 2262 | ` *  to the output on success or FALSE on failure.` |
|      - | 2263 | ` */` |
|      4 | 2264 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2265 | `{` |
|      - | 2266 | `	const ph7_io_stream *pStream;` |
|      - | 2267 | `	io_private *pDev;` |
|      - | 2268 | `	ph7_int64 n,nRead;` |
|      - | 2269 | `	char zBuf[8192];` |
|      - | 2270 | `	int rc;` |
|      5 | 2271 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2272 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2273 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2274 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2275 | `		return PH7_OK;` |
|      - | 2276 | `	}` |
|      - | 2277 | `	/* Extract our private data */` |
|      5 | 2278 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2279 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2280 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2281 | `		/*Expecting an IO handle */` |
|    ! 0 | 2282 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2283 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2284 | `		return PH7_OK;` |
|      - | 2285 | `	}` |
|      - | 2286 | `	/* Point to the target IO stream device */` |
|      5 | 2287 | `	pStream = pDev->pStream;` |
|      5 | 2288 | `	if( pStream == 0  ){` |
|    ! 0 | 2289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2290 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2291 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2292 | `			);` |
|    ! 0 | 2293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2294 | `		return PH7_OK;` |
|      - | 2295 | `	}` |
|      - | 2296 | `	/* Perform the requested operation */` |
|      5 | 2297 | `	nRead = 0;` |
|      5 | 2298 | `	for(;;){` |
|     11 | 2299 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|     11 | 2300 | `		if( n < 1 ){` |
|      - | 2301 | `			/* Error or EOF */` |
|      5 | 2302 | `			break;` |
|      - | 2303 | `		}` |
|      - | 2304 | `		/* Increment the read counter */` |
|      7 | 2305 | `		nRead += n;` |
|      - | 2306 | `		/* Output the bytes THIS read produced. Handing the running total to` |
|      - | 2307 | `		 * ph7_context_output() instead read past the end of zBuf from the second` |
|      - | 2308 | `		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:` |
|      - | 2309 | `		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */` |
|      7 | 2310 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      7 | 2311 | `		if( rc == PH7_ABORT ){` |
|      - | 2312 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 2313 | `			break;` |
|      - | 2314 | `		}` |
|      1 | 2315 | `	}` |
|      - | 2316 | `	/* Total number of bytes readen */` |
|      5 | 2317 | `	ph7_result_int64(pCtx,nRead);` |
|      5 | 2318 | `	return PH7_OK;` |
|      3 | 2319 | `}` |
|      - | 2320 | `/* CSV reader/writer private data */` |
|      - | 2321 | `struct csv_data` |
|      - | 2322 | `{` |
|      - | 2323 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 2324 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 2325 | `	io_private *pDev; /* Open stream handle */` |
|      - | 2326 | `	int iCount;       /* Counter */` |
|      - | 2327 | `};` |
|      - | 2328 | `/*` |
|      - | 2329 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 2330 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 2331 | ` * associated data.` |
|      - | 2332 | ` */` |
|     10 | 2333 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 2334 | `{` |
|     11 | 2335 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 2336 | `	const char *zData;` |
|      - | 2337 | `	int nLen,c2;` |
|      - | 2338 | `	sxu32 n;` |
|      - | 2339 | `	/* Point to the raw data */` |
|     11 | 2340 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|     11 | 2341 | `	if( nLen < 1 ){` |
|      - | 2342 | `		/* Nothing to write */` |
|    ! 0 | 2343 | `		return PH7_OK;` |
|      - | 2344 | `	}` |
|     11 | 2345 | `	if( pData->iCount > 0 ){` |
|      - | 2346 | `		/* Write the delimiter */` |
|      7 | 2347 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      3 | 2348 | `	}` |
|     11 | 2349 | `	n = 1;` |
|     11 | 2350 | `	c2 = 0;` |
|     16 | 2351 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|     10 | 2352 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 2353 | `			c2 = 1;` |
|    ! 0 | 2354 | `			if( n == 0 ){` |
|    ! 0 | 2355 | `				c2 = 2;` |
|    ! 0 | 2356 | `			}` |
|      - | 2357 | `			/* Write the enclosure */` |
|    ! 0 | 2358 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2359 | `			if( c2 > 1 ){` |
|    ! 0 | 2360 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2361 | `			}` |
|    ! 0 | 2362 | `	}` |
|      - | 2363 | `	/* Write the data */` |
|     11 | 2364 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 2365 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 2366 | `		return PH7_ABORT;` |
|      - | 2367 | `	}` |
|     11 | 2368 | `	if( c2 > 0 ){` |
|      - | 2369 | `		/* Write the enclosure */` |
|    ! 0 | 2370 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2371 | `		if( c2 > 1 ){` |
|    ! 0 | 2372 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2373 | `		}` |
|    ! 0 | 2374 | `	}` |
|     11 | 2375 | `	pData->iCount++;` |
|     11 | 2376 | `	return PH7_OK;` |
|      6 | 2377 | `}` |
|      - | 2378 | `/*` |
|      - | 2379 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 2380 | ` *  Format line as CSV and write to file pointer.` |
|      - | 2381 | ` * Parameters` |
|      - | 2382 | ` *  $handle` |
|      - | 2383 | ` *   Open file handle.` |
|      - | 2384 | ` * $fields` |
|      - | 2385 | ` *   An array of values.` |
|      - | 2386 | ` * $delimiter` |
|      - | 2387 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 2388 | ` * $enclosure` |
|      - | 2389 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 2390 | ` */` |
|     10 | 2391 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2392 | `{` |
|      - | 2393 | `	const ph7_io_stream *pStream;` |
|      - | 2394 | `	struct csv_data sCsv;` |
|      - | 2395 | `	io_private *pDev;` |
|      - | 2396 | `	char *zEol;` |
|      - | 2397 | `	int eolen;` |
|     11 | 2398 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 2399 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2400 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 2401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2402 | `		return PH7_OK;` |
|      - | 2403 | `	}` |
|      - | 2404 | `	/* Extract our private data */` |
|     11 | 2405 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2406 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 | 2407 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2408 | `		/*Expecting an IO handle */` |
|    ! 0 | 2409 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2411 | `		return PH7_OK;` |
|      - | 2412 | `	}` |
|      - | 2413 | `	/* Point to the target IO stream device */` |
|     11 | 2414 | `	pStream = pDev->pStream;` |
|     11 | 2415 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2416 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2417 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2418 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2419 | `			);` |
|    ! 0 | 2420 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2421 | `		return PH7_OK;` |
|      - | 2422 | `	}` |
|      - | 2423 | `	/* Set default csv separator */` |
|     11 | 2424 | `	sCsv.delimiter = ',';` |
|     11 | 2425 | `	sCsv.enclosure = '"';` |
|     11 | 2426 | `	sCsv.pDev = pDev;` |
|     11 | 2427 | `	sCsv.iCount = 0;` |
|     11 | 2428 | `	if( nArg > 2 ){` |
|     11 | 2429 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     11 | 2430 | `		if( rc != PH7_OK ){` |
|      3 | 2431 | `			return rc;` |
|      - | 2432 | `		}` |
|      9 | 2433 | `		if( nArg > 3 ){` |
|      9 | 2434 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|      9 | 2435 | `			if( rc != PH7_OK ){` |
|      3 | 2436 | `				return rc;` |
|      - | 2437 | `			}` |
|      7 | 2438 | `			if( nArg > 4 ){` |
|      - | 2439 | `				/* The writer does not model $escape (the CSV-writer slice);` |
|      - | 2440 | `				 * validate it like php so the loud path matches. */` |
|      - | 2441 | `				int iEscape;` |
|      7 | 2442 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&iEscape);` |
|      7 | 2443 | `				if( rc != PH7_OK ){` |
|      3 | 2444 | `					return rc;` |
|      - | 2445 | `				}` |
|      2 | 2446 | `			}` |
|      2 | 2447 | `		}` |
|      2 | 2448 | `	}` |
|      - | 2449 | `	/* Iterate throw array entries and write csv data */` |
|      5 | 2450 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 2451 | `	/* Write a line ending */` |
|      - | 2452 | `#ifdef __WINNT__` |
|      1 | 2453 | `	zEol = "\r\n";` |
|      1 | 2454 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 2455 | `#else` |
|      - | 2456 | `	/* Assume UNIX LF */` |
|      4 | 2457 | `	zEol = "\n";` |
|      4 | 2458 | `	eolen = (int)sizeof(char);` |
|      - | 2459 | `#endif` |
|      5 | 2460 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      5 | 2461 | `	return PH7_OK;` |
|      6 | 2462 | `}` |
|      - | 2463 | `/*` |
|      - | 2464 | ` * fprintf,vfprintf private data.` |
|      - | 2465 | ` * An instance of the following structure is passed to the formatted` |
|      - | 2466 | ` * input consumer callback defined below.` |
|      - | 2467 | ` */` |
|      - | 2468 | `typedef struct fprintf_data fprintf_data;` |
|      - | 2469 | `struct fprintf_data` |
|      - | 2470 | `{` |
|      - | 2471 | `	io_private *pIO;        /* IO stream */` |
|      - | 2472 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 2473 | `};` |
|      - | 2474 | `/*` |
|      - | 2475 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 2476 | ` */` |
|     38 | 2477 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      2 | 2478 | `{` |
|     40 | 2479 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 2480 | `	ph7_int64 n;` |
|      - | 2481 | `	/* Write the formatted data */` |
|     40 | 2482 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     40 | 2483 | `	if( n < 1 ){` |
|    ! 0 | 2484 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 2485 | `		/* IO error,abort immediately */` |
|    ! 0 | 2486 | `		return SXERR_ABORT;` |
|      - | 2487 | `	}` |
|      - | 2488 | `	/* Increment counter */` |
|     40 | 2489 | `	pFdata->nCount += n;` |
|     40 | 2490 | `	return PH7_OK;` |
|     21 | 2491 | `}` |
|      - | 2492 | `/*` |
|      - | 2493 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 2494 | ` *  Write a formatted string to a stream.` |
|      - | 2495 | ` * Parameters` |
|      - | 2496 | ` *  $handle` |
|      - | 2497 | ` *   The file pointer.` |
|      - | 2498 | ` *  $format` |
|      - | 2499 | ` *   String format (see sprintf()).` |
|      - | 2500 | ` * Return` |
|      - | 2501 | ` *  The length of the written string.` |
|      - | 2502 | ` */` |
|     20 | 2503 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2504 | `{` |
|      - | 2505 | `	fprintf_data sFdata;` |
|      - | 2506 | `	const char *zFormat;` |
|      - | 2507 | `	io_private *pDev;` |
|      - | 2508 | `	int nLen;` |
|     22 | 2509 | `	if( nArg < 2 ){` |
|    ! 0 | 2510 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2511 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2512 | `		return PH7_OK;` |
|      - | 2513 | `	}` |
|      - | 2514 | `	{` |
|      - | 2515 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     22 | 2516 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     22 | 2517 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 2518 | `			return rcs;` |
|      - | 2519 | `		}` |
|      - | 2520 | `	}` |
|      - | 2521 | `	/* Extract our private data */` |
|     22 | 2522 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2523 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     22 | 2524 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2525 | `		/*Expecting an IO handle */` |
|    ! 0 | 2526 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2527 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2528 | `		return PH7_OK;` |
|      - | 2529 | `	}` |
|      - | 2530 | `	/* Point to the target IO stream device */` |
|     22 | 2531 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2533 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2534 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2535 | `			);` |
|    ! 0 | 2536 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2537 | `		return PH7_OK;` |
|      - | 2538 | `	}` |
|      - | 2539 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 2540 | `	{` |
|     22 | 2541 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     22 | 2542 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2543 | `			return rcf;` |
|      - | 2544 | `		}` |
|      - | 2545 | `	}` |
|      - | 2546 | `	/* Extract the string format (scalars/null coerce). */` |
|     22 | 2547 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     22 | 2548 | `	if( nLen < 1 ){` |
|      - | 2549 | `		/* Empty string,return zero */` |
|    ! 0 | 2550 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2551 | `		return PH7_OK;` |
|      - | 2552 | `	}` |
|      - | 2553 | `	{` |
|      - | 2554 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 2555 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 2556 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     22 | 2557 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     22 | 2558 | `		if( rcv != PH7_OK ){` |
|      3 | 2559 | `			return rcv;` |
|      - | 2560 | `		}` |
|      - | 2561 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 2562 | `		 * before any output; propagate the throw status verbatim. */` |
|     20 | 2563 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     20 | 2564 | `		if( rcv != PH7_OK ){` |
|      5 | 2565 | `			return rcv;` |
|      - | 2566 | `		}` |
|      - | 2567 | `	}` |
|      - | 2568 | `	/* Prepare our private data */` |
|     16 | 2569 | `	sFdata.nCount = 0;` |
|     16 | 2570 | `	sFdata.pIO = pDev;` |
|      - | 2571 | `	/* Format the string */` |
|      - | 2572 | `	{` |
|     16 | 2573 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 2574 | `	/* Return total number of bytes written */` |
|     16 | 2575 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      - | 2576 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 2577 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|     16 | 2578 | `	if( rcv != SXRET_OK ){` |
|      3 | 2579 | `		pCtx->nThrowRc = rcv;` |
|      3 | 2580 | `		return rcv;` |
|      - | 2581 | `	}` |
|      - | 2582 | `	}` |
|     13 | 2583 | `	return PH7_OK;` |
|     12 | 2584 | `}` |
|      - | 2585 | `/*` |
|      - | 2586 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 2587 | ` *  Write a formatted string to a stream.` |
|      - | 2588 | ` * Parameters` |
|      - | 2589 | ` *  $handle` |
|      - | 2590 | ` *   The file pointer.` |
|      - | 2591 | ` *  $format` |
|      - | 2592 | ` *   String format (see sprintf()).` |
|      - | 2593 | ` * $args` |
|      - | 2594 | ` *   User arguments.` |
|      - | 2595 | ` * Return` |
|      - | 2596 | ` *  The length of the written string.` |
|      - | 2597 | ` */` |
|      8 | 2598 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2599 | `{` |
|      - | 2600 | `	fprintf_data sFdata;` |
|      - | 2601 | `	const char *zFormat;` |
|      - | 2602 | `	ph7_hashmap *pMap;` |
|      - | 2603 | `	io_private *pDev;` |
|      - | 2604 | `	SySet sArg;` |
|      - | 2605 | `	int n,nLen;` |
|     10 | 2606 | `	if( nArg < 3 ){` |
|    ! 0 | 2607 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2608 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2609 | `		return PH7_OK;` |
|      - | 2610 | `	}` |
|      - | 2611 | `	{` |
|      - | 2612 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     10 | 2613 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     10 | 2614 | `		if( rcs != PH7_OK ){` |
|      3 | 2615 | `			return rcs;` |
|      - | 2616 | `		}` |
|      - | 2617 | `	}` |
|      - | 2618 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 2619 | `	{` |
|      8 | 2620 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      8 | 2621 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2622 | `			return rcf;` |
|      - | 2623 | `		}` |
|      - | 2624 | `	}` |
|      8 | 2625 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 2626 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 2627 | `		char zBuf[64];` |
|    ! 0 | 2628 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2629 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 2630 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 2631 | `	}` |
|      - | 2632 | `	/* Extract our private data */` |
|      8 | 2633 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2634 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      8 | 2635 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2636 | `		/*Expecting an IO handle */` |
|    ! 0 | 2637 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2638 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2639 | `		return PH7_OK;` |
|      - | 2640 | `	}` |
|      - | 2641 | `	/* Point to the target IO stream device */` |
|      8 | 2642 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2643 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2644 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2645 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2646 | `			);` |
|    ! 0 | 2647 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2648 | `		return PH7_OK;` |
|      - | 2649 | `	}` |
|      - | 2650 | `	/* Extract the string format */` |
|      8 | 2651 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      8 | 2652 | `	if( nLen < 1 ){` |
|      - | 2653 | `		/* Empty string,return zero */` |
|    ! 0 | 2654 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2655 | `		return PH7_OK;` |
|      - | 2656 | `	}` |
|      - | 2657 | `	/* Point to hashmap */` |
|      8 | 2658 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 2659 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 2660 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 2661 | `	{` |
|      8 | 2662 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      8 | 2663 | `		if( rcc != PH7_OK ){` |
|      3 | 2664 | `			return rcc;` |
|      - | 2665 | `		}` |
|      - | 2666 | `	}` |
|      - | 2667 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 2668 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 2669 | `	{` |
|      6 | 2670 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      6 | 2671 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 2672 | `			return rcv;` |
|      - | 2673 | `		}` |
|      - | 2674 | `	}` |
|      - | 2675 | `	/* Extract arguments from the hashmap */` |
|      6 | 2676 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 2677 | `	/* Prepare our private data */` |
|      6 | 2678 | `	sFdata.nCount = 0;` |
|      6 | 2679 | `	sFdata.pIO = pDev;` |
|      - | 2680 | `	/* Format the string */` |
|      - | 2681 | `	{` |
|      6 | 2682 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 2683 | `	/* Return total number of bytes written*/` |
|      6 | 2684 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      6 | 2685 | `	SySetRelease(&sArg);` |
|      - | 2686 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 2687 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|      6 | 2688 | `	if( rcv != SXRET_OK ){` |
|      3 | 2689 | `		pCtx->nThrowRc = rcv;` |
|      3 | 2690 | `		return rcv;` |
|      - | 2691 | `	}` |
|      - | 2692 | `	}` |
|      3 | 2693 | `	return PH7_OK;` |
|      6 | 2694 | `}` |
|      - | 2695 | `/*` |
|      - | 2696 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 2697 | ` * According to the PHP reference manual:` |
|      - | 2698 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 2699 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 2700 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 2701 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 2702 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 2703 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 2704 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 2705 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 2706 | ` *         exist, attempt to create it.` |
|      - | 2707 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 2708 | ` *          not exist, attempt to create it.` |
|      - | 2709 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 2710 | ` *         already exists,` |
|      - | 2711 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 2712 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 2713 | ` *         the underlying open(2) system call.` |
|      - | 2714 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 2715 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 2716 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 2717 | ` *          is positioned on the beginning of the file.` |
|      - | 2718 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 2719 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 2720 | ` *          be used after the lock is requested).` |
|      - | 2721 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 2722 | ` */` |
|    376 | 2723 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 2724 | `{` |
|    381 | 2725 | `	const char *zEnd = &zMode[nLen];` |
|    381 | 2726 | `	int iFlag = 0;` |
|      - | 2727 | `	int c;` |
|    381 | 2728 | `	if( nLen < 1 ){` |
|      - | 2729 | `		/* Open in a read-only mode */` |
|    ! 0 | 2730 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 2731 | `	}` |
|    381 | 2732 | `	c = zMode[0];` |
|    381 | 2733 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 2734 | `		/* Read-only access */` |
|    181 | 2735 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|    181 | 2736 | `		zMode++; /* Advance */` |
|    181 | 2737 | `		if( zMode < zEnd ){` |
|     77 | 2738 | `			c = zMode[0];` |
|     77 | 2739 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 2740 | `				/* Read+Write access */` |
|     69 | 2741 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|     33 | 2742 | `			}` |
|     42 | 2743 | `		}` |
|    292 | 2744 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 2745 | `		/* Overwrite mode.` |
|      - | 2746 | `		 * If the file does not exists,try to create it` |
|      - | 2747 | `		 */` |
|     49 | 2748 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     49 | 2749 | `		zMode++; /* Advance */` |
|     49 | 2750 | `		if( zMode < zEnd ){` |
|     14 | 2751 | `			c = zMode[0];` |
|     14 | 2752 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2753 | `				/* Read+Write access */` |
|     14 | 2754 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|     14 | 2755 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      6 | 2756 | `			}` |
|      9 | 2757 | `		}` |
|    179 | 2758 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 2759 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 2760 | `		 * Create the file if it does not exists.` |
|      - | 2761 | `		 */` |
|    ! 0 | 2762 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2763 | `		zMode++; /* Advance */` |
|    ! 0 | 2764 | `		if( zMode < zEnd ){` |
|    ! 0 | 2765 | `			c = zMode[0];` |
|    ! 0 | 2766 | `			if( c == '+' ){` |
|      - | 2767 | `				/* Read-Write access */` |
|    ! 0 | 2768 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2769 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2770 | `			}` |
|    ! 0 | 2771 | `		}` |
|    156 | 2772 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 2773 | `		/* Exclusive access.` |
|      - | 2774 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 2775 | `		 * Otherwise create a new file.` |
|      - | 2776 | `		 */` |
|    156 | 2777 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    156 | 2778 | `		zMode++; /* Advance */` |
|    156 | 2779 | `		if( zMode < zEnd ){` |
|    ! 0 | 2780 | `			c = zMode[0];` |
|    ! 0 | 2781 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2782 | `				/* Read-Write access */` |
|    ! 0 | 2783 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2784 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2785 | `			}` |
|      2 | 2786 | `		}` |
|     77 | 2787 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 2788 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 2789 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2790 | `		zMode++; /* Advance */` |
|    ! 0 | 2791 | `		if( zMode < zEnd ){` |
|    ! 0 | 2792 | `			c = zMode[0];` |
|    ! 0 | 2793 | `			if( c == '+' ){` |
|      - | 2794 | `				/* Read-Write access */` |
|    ! 0 | 2795 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2796 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2797 | `			}` |
|    ! 0 | 2798 | `		}` |
|    ! 0 | 2799 | `	}else{` |
|      - | 2800 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 2801 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 2802 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 2803 | `	}` |
|    469 | 2804 | `	while( zMode < zEnd ){` |
|     91 | 2805 | `		c = zMode[0];` |
|     91 | 2806 | `		if( c == 'b' \|\| c == 'B' ){` |
|     11 | 2807 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|     11 | 2808 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     86 | 2809 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 2810 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 2811 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2812 | `		}` |
|     91 | 2813 | `		zMode++;` |
|      3 | 2814 | `	}` |
|    381 | 2815 | `	return iFlag;` |
|    193 | 2816 | `}` |
|      - | 2817 | `/*` |
|      - | 2818 | ` * Initialize the IO private structure.` |
|      - | 2819 | ` */` |
|   6458 | 2820 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 2821 | `{` |
|   6463 | 2822 | `	pOut->pStream = pStream;` |
|   6463 | 2823 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   6463 | 2824 | `	pOut->nOfft = 0;` |
|      - | 2825 | `	/* Set the magic number */` |
|   6463 | 2826 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   6463 | 2827 | `}` |
|      - | 2828 | `/*` |
|      - | 2829 | ` * Release the IO private structure.` |
|      - | 2830 | ` */` |
|     36 | 2831 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      2 | 2832 | `{` |
|     38 | 2833 | `	SyBlobRelease(&pDev->sBuffer);` |
|     38 | 2834 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 2835 | `	/* Release the whole structure */` |
|     38 | 2836 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     38 | 2837 | `}` |
|      - | 2838 | `/*` |
|      - | 2839 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 2840 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 2841 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 2842 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 2843 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 2844 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 2845 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 2846 | ` */` |
|   6296 | 2847 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 2848 | `{` |
|   6301 | 2849 | `	SyBlobRelease(&pDev->sBuffer);` |
|   6301 | 2850 | `	pDev->pHandle = 0;` |
|   6301 | 2851 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   6301 | 2852 | `}` |
|      - | 2853 | `/*` |
|      - | 2854 | ` * Reset the IO private structure.` |
|      - | 2855 | ` */` |
|    126 | 2856 | `static void ResetIOPrivate(io_private *pDev)` |
|      4 | 2857 | `{` |
|    130 | 2858 | `	SyBlobReset(&pDev->sBuffer);` |
|    130 | 2859 | `	pDev->nOfft = 0;` |
|    130 | 2860 | `}` |
|      - | 2861 | `/* Forward declaration */` |
|      - | 2862 |  |
|      - | 2863 | `/*` |
|      - | 2864 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 2865 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 2866 | ` * Parameters` |
|      - | 2867 | ` *  $filename` |
|      - | 2868 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 2869 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 2870 | ` *   then a regular file is assumed.` |
|      - | 2871 | ` *  $mode` |
|      - | 2872 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 2873 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 2874 | ` *   modes.` |
|      - | 2875 | ` *  $use_include_path` |
|      - | 2876 | ` *   You can use the optional second parameter and set it to` |
|      - | 2877 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2878 | ` *  $context` |
|      - | 2879 | ` *   A context stream resource.` |
|      - | 2880 | ` * Return` |
|      - | 2881 | ` *  File handle on success or FALSE on failure.` |
|      - | 2882 | ` */` |
|      - | 2883 | `/*` |
|      - | 2884 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 2885 | ` *                                  int $offset = -1)` |
|      - | 2886 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 2887 | ` */` |
|     44 | 2888 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2889 | `{` |
|      - | 2890 | `	const ph7_io_stream *pStream;` |
|      - | 2891 | `	io_private *pDev;` |
|     46 | 2892 | `	ph7_int64 nMax = -1;` |
|      - | 2893 | `	char zBuf[4096];` |
|      - | 2894 | `	ph7_int64 nRead;` |
|     46 | 2895 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2896 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2897 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2898 | `		return PH7_OK;` |
|      - | 2899 | `	}` |
|     46 | 2900 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     46 | 2901 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2902 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2904 | `		return PH7_OK;` |
|      - | 2905 | `	}` |
|     46 | 2906 | `	pStream = pDev->pStream;` |
|     46 | 2907 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 2908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2909 | `		return PH7_OK;` |
|      - | 2910 | `	}` |
|     46 | 2911 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2912 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 2913 | `		 * default) means "read until EOF". */` |
|      9 | 2914 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      9 | 2915 | `		if( nMax < -1 ){` |
|      3 | 2916 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2917 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 2918 | `		}` |
|      3 | 2919 | `	}` |
|     44 | 2920 | `	if( nArg > 2 ){` |
|      5 | 2921 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 2922 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 2923 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 2924 | `		}` |
|      2 | 2925 | `	}` |
|     44 | 2926 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     81 | 2927 | `	while( nMax != 0 ){` |
|     79 | 2928 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     79 | 2929 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 2930 | `			nAsk = nMax;` |
|      1 | 2931 | `		}` |
|     79 | 2932 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     79 | 2933 | `		if( nRead < 1 ){` |
|     42 | 2934 | `			break;` |
|      - | 2935 | `		}` |
|     39 | 2936 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     39 | 2937 | `		if( nMax > 0 ){` |
|      3 | 2938 | `			nMax -= nRead;` |
|      1 | 2939 | `		}` |
|      2 | 2940 | `	}` |
|     44 | 2941 | `	return PH7_OK;` |
|     24 | 2942 | `}` |
|      - | 2943 | `/*` |
|      - | 2944 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 2945 | ` */` |
|      4 | 2946 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2947 | `{` |
|      - | 2948 | `	ph7_value *pArr,*pV;` |
|      - | 2949 | `	ph7_io_stream **apDev;` |
|      - | 2950 | `	sxu32 n;` |
|      2 | 2951 | `	SXUNUSED(nArg);` |
|      2 | 2952 | `	SXUNUSED(apArg);` |
|      6 | 2953 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 2954 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 2955 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2956 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2957 | `		return PH7_OK;` |
|      - | 2958 | `	}` |
|      6 | 2959 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 2960 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 2961 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 2962 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 2963 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 2964 | `	}` |
|      6 | 2965 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 2966 | `	return PH7_OK;` |
|      4 | 2967 | `}` |
|      - | 2968 | `/*` |
|      - | 2969 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 2970 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 2971 | ` */` |
|      2 | 2972 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2973 | `{` |
|      - | 2974 | `	io_private *pDev;` |
|      - | 2975 | `	ph7_value *pArr,*pV;` |
|      3 | 2976 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2977 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2978 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2979 | `		return PH7_OK;` |
|      - | 2980 | `	}` |
|      3 | 2981 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 2982 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2983 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2984 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2985 | `		return PH7_OK;` |
|      - | 2986 | `	}` |
|      3 | 2987 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 2988 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 2989 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2990 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2991 | `		return PH7_OK;` |
|      - | 2992 | `	}` |
|      3 | 2993 | `	ph7_value_bool(pV,0);` |
|      3 | 2994 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 2995 | `	ph7_value_bool(pV,1);` |
|      3 | 2996 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 2997 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 2998 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 2999 | `	ph7_value_bool(pV,0);` |
|      3 | 3000 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 3001 | `	ph7_value_int(pV,0);` |
|      3 | 3002 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 3003 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 3004 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 3005 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 3006 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 3007 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 3008 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 3009 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 3010 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 3011 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 3012 | `	return PH7_OK;` |
|      2 | 3013 | `}` |
|      - | 3014 | `/*` |
|      - | 3015 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 3016 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 3017 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 3018 | ` * resource, options unconsumed).` |
|      - | 3019 | ` */` |
|      2 | 3020 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3021 | `{` |
|      3 | 3022 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 3023 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 3024 | `	}else{` |
|    ! 0 | 3025 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 3026 | `		if( pArr == 0 ){` |
|    ! 0 | 3027 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3028 | `			return PH7_OK;` |
|      - | 3029 | `		}` |
|    ! 0 | 3030 | `		ph7_result_value(pCtx,pArr);` |
|      - | 3031 | `	}` |
|      3 | 3032 | `	return PH7_OK;` |
|      2 | 3033 | `}` |
|      - | 3034 | `/*` |
|      - | 3035 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 3036 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 3037 | ` */` |
|      - | 3038 | `#ifdef PH7_ENABLE_NET` |
|      - | 3039 | `typedef struct sock_private sock_private;` |
|      - | 3040 | `struct sock_private` |
|      - | 3041 | `{` |
|      - | 3042 | `	ph7_vm *pVm;` |
|      - | 3043 | `	ph7_socket sock;` |
|      - | 3044 | `	int bEof;` |
|      - | 3045 | `};` |
|     13 | 3046 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 3047 | `{` |
|     13 | 3048 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 3049 | `	int n;` |
|     13 | 3050 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      1 | 3051 | `		return 0;` |
|      - | 3052 | `	}` |
|     12 | 3053 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     12 | 3054 | `	if( n <= 0 ){` |
|      4 | 3055 | `		pSock->bEof = 1;` |
|      4 | 3056 | `		return 0;` |
|      - | 3057 | `	}` |
|      8 | 3058 | `	return (ph7_int64)n;` |
|      5 | 3059 | `}` |
|      4 | 3060 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 3061 | `{` |
|      4 | 3062 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 3063 | `	int n;` |
|      4 | 3064 | `	if( pSock == 0 ){` |
|    ! 0 | 3065 | `		return -1;` |
|      - | 3066 | `	}` |
|      4 | 3067 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 3068 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 3069 | `}` |
|      4 | 3070 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 3071 | `{` |
|      4 | 3072 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 3073 | `	if( pSock == 0 ){` |
|    ! 0 | 3074 | `		return;` |
|      - | 3075 | `	}` |
|      4 | 3076 | `	PH7_NetClose(pSock->sock);` |
|      4 | 3077 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 3078 | `}` |
|      - | 3079 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 3080 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 3081 | `{` |
|      - | 3082 | `	sock_private *pSock;` |
|      - | 3083 | `	ph7_socket sock;` |
|      - | 3084 | `	char zHost[256];` |
|      - | 3085 | `	const char *zColon;` |
|    ! 0 | 3086 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 3087 | `	const char *zErr = "";` |
|    ! 0 | 3088 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 3089 | `	SXUNUSED(iMode);` |
|    ! 0 | 3090 | `	if( pVm == 0 ){` |
|    ! 0 | 3091 | `		return -1;` |
|      - | 3092 | `	}` |
|    ! 0 | 3093 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 3094 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 3095 | `		zColon--;` |
|    ! 0 | 3096 | `	}` |
|    ! 0 | 3097 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 3098 | `		return -1;` |
|      - | 3099 | `	}` |
|      - | 3100 | `	{` |
|    ! 0 | 3101 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 3102 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 3103 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 3104 | `		}` |
|    ! 0 | 3105 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 3106 | `		zHost[n] = 0;` |
|      - | 3107 | `	}` |
|      - | 3108 | `	{` |
|    ! 0 | 3109 | `		sxi32 iTmp = 0;` |
|    ! 0 | 3110 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 3111 | `		iPort = (int)iTmp;` |
|      - | 3112 | `	}` |
|    ! 0 | 3113 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 3114 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 3115 | `		return -1;` |
|      - | 3116 | `	}` |
|    ! 0 | 3117 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 3118 | `	if( pSock == 0 ){` |
|    ! 0 | 3119 | `		PH7_NetClose(sock);` |
|    ! 0 | 3120 | `		return -1;` |
|      - | 3121 | `	}` |
|    ! 0 | 3122 | `	pSock->pVm = pVm;` |
|    ! 0 | 3123 | `	pSock->sock = sock;` |
|    ! 0 | 3124 | `	pSock->bEof = 0;` |
|    ! 0 | 3125 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 3126 | `	return PH7_OK;` |
|    ! 0 | 3127 | `}` |
|      - | 3128 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 3129 | `	"tcp",` |
|      - | 3130 | `	PH7_IO_STREAM_VERSION,` |
|      - | 3131 | `	SockStreamData_Open, /* xOpen */` |
|      - | 3132 | `	0,   /* xOpenDir */` |
|      - | 3133 | `	SockStreamData_Close,/* xClose */` |
|      - | 3134 | `	0,  /* xCloseDir */` |
|      - | 3135 | `	SockStreamData_Read, /* xRead */` |
|      - | 3136 | `	0,  /* xReadDir */` |
|      - | 3137 | `	SockStreamData_Write,/* xWrite */` |
|      - | 3138 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 3139 | `	0,  /* xLock */` |
|      - | 3140 | `	0,  /* xRewindDir */` |
|      - | 3141 | `	0,  /* xTell */` |
|      - | 3142 | `	0,  /* xTrunc */` |
|      - | 3143 | `	0,  /* xSync */` |
|      - | 3144 | `	0   /* xStat */` |
|      - | 3145 | `};` |
|      - | 3146 | `#endif /* PH7_ENABLE_NET */` |
|      - | 3147 | `/*` |
|      - | 3148 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 3149 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 3150 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 3151 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 3152 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 3153 | ` * php streamWrapper protocol method on it.` |
|      - | 3154 | ` */` |
|      - | 3155 | `#define PHL_UWRAP_MAX 8` |
|      - | 3156 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 3157 | `struct uwrap_slot` |
|      - | 3158 | `{` |
|      - | 3159 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 3160 | `	char zScheme[32];         /* protocol name */` |
|      - | 3161 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 3162 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 3163 | `};` |
|      - | 3164 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 3165 | `struct uwrap_handle` |
|      - | 3166 | `{` |
|      - | 3167 | `	ph7_vm *pVm;` |
|      - | 3168 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 3169 | `	int iSlot;` |
|      - | 3170 | `	int bEof;` |
|      - | 3171 | `};` |
|      - | 3172 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 3173 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 3174 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 3175 | `	ph7_value *pResult)` |
|      1 | 3176 | `{` |
|      - | 3177 | `	ph7_class_method *pMeth;` |
|     27 | 3178 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 3179 | `		return -1;` |
|      - | 3180 | `	}` |
|     27 | 3181 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 3182 | `	if( pMeth == 0 ){` |
|    ! 0 | 3183 | `		return -1;` |
|      - | 3184 | `	}` |
|     27 | 3185 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 3186 | `		return -1;` |
|      - | 3187 | `	}` |
|     27 | 3188 | `	return 0;` |
|     14 | 3189 | `}` |
|      8 | 3190 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 3191 | `{` |
|      9 | 3192 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3193 | `	ph7_value sArg,sRet;` |
|      - | 3194 | `	const char *zData;` |
|      9 | 3195 | `	int nData = 0;` |
|      9 | 3196 | `	ph7_int64 n = 0;` |
|      9 | 3197 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 3198 | `		return 0;` |
|      - | 3199 | `	}` |
|      9 | 3200 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 3201 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 3202 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 3203 | `	{` |
|      - | 3204 | `		ph7_value *apArg[1];` |
|      9 | 3205 | `		apArg[0] = &sArg;` |
|      9 | 3206 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 3207 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 3208 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3209 | `			return -1;` |
|      - | 3210 | `		}` |
|      - | 3211 | `	}` |
|      9 | 3212 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 3213 | `	if( nData > 0 ){` |
|      7 | 3214 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 3215 | `			nData = (int)nRead;` |
|    ! 0 | 3216 | `		}` |
|      7 | 3217 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 3218 | `		n = nData;` |
|      4 | 3219 | `	}else{` |
|      3 | 3220 | `		pH->bEof = 1;` |
|      - | 3221 | `	}` |
|      9 | 3222 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 3223 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 3224 | `	return n;` |
|      5 | 3225 | `}` |
|      2 | 3226 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 3227 | `{` |
|      3 | 3228 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3229 | `	ph7_value sArg,sRet;` |
|      - | 3230 | `	ph7_int64 n;` |
|      3 | 3231 | `	if( pH == 0 ){` |
|    ! 0 | 3232 | `		return -1;` |
|      - | 3233 | `	}` |
|      3 | 3234 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 3235 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3236 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 3237 | `	{` |
|      - | 3238 | `		ph7_value *apArg[1];` |
|      3 | 3239 | `		apArg[0] = &sArg;` |
|      3 | 3240 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 3241 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 3242 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3243 | `			return -1;` |
|      - | 3244 | `		}` |
|      - | 3245 | `	}` |
|      3 | 3246 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3247 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 3248 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3249 | `	return n;` |
|      2 | 3250 | `}` |
|      2 | 3251 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 3252 | `{` |
|      3 | 3253 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3254 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 3255 | `	ph7_value *apArg[2];` |
|      - | 3256 | `	int rc;` |
|      3 | 3257 | `	if( pH == 0 ){` |
|    ! 0 | 3258 | `		return -1;` |
|      - | 3259 | `	}` |
|      3 | 3260 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 3261 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 3262 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3263 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 3264 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 3265 | `	apArg[0] = &sOfft;` |
|      3 | 3266 | `	apArg[1] = &sWhence;` |
|      3 | 3267 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 3268 | `	if( rc == 0 ){` |
|      3 | 3269 | `		pH->bEof = 0;` |
|      3 | 3270 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 3271 | `	}` |
|      3 | 3272 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 3273 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 3274 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3275 | `	return rc;` |
|      2 | 3276 | `}` |
|      2 | 3277 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 3278 | `{` |
|      3 | 3279 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 3280 | `	ph7_value sRet;` |
|      - | 3281 | `	ph7_int64 n;` |
|      3 | 3282 | `	if( pH == 0 ){` |
|    ! 0 | 3283 | `		return -1;` |
|      - | 3284 | `	}` |
|      3 | 3285 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 3286 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 3287 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 3288 | `		return -1;` |
|      - | 3289 | `	}` |
|      3 | 3290 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 3291 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 3292 | `	return n;` |
|      2 | 3293 | `}` |
|      6 | 3294 | `static void UwrapClose(void *pHandle)` |
|      1 | 3295 | `{` |
|      7 | 3296 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 3297 | `	if( pH == 0 ){` |
|    ! 0 | 3298 | `		return;` |
|      - | 3299 | `	}` |
|      7 | 3300 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 3301 | `	if( pH->pObj ){` |
|      7 | 3302 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 3303 | `	}` |
|      7 | 3304 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 3305 | `}` |
|      - | 3306 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 3307 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 3308 | `{` |
|      7 | 3309 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 3310 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 3311 | `	ph7_class *pClass;` |
|      - | 3312 | `	uwrap_handle *pH;` |
|      - | 3313 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 3314 | `	ph7_value *apArg[4];` |
|      - | 3315 | `	int rc;` |
|      7 | 3316 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 3317 | `		return -1;` |
|      - | 3318 | `	}` |
|      7 | 3319 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 3320 | `	if( pClass == 0 ){` |
|    ! 0 | 3321 | `		return -1;` |
|      - | 3322 | `	}` |
|      7 | 3323 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 3324 | `	if( pH == 0 ){` |
|    ! 0 | 3325 | `		return -1;` |
|      - | 3326 | `	}` |
|      7 | 3327 | `	pH->pVm = pVm;` |
|      7 | 3328 | `	pH->iSlot = iSlot;` |
|      7 | 3329 | `	pH->bEof = 0;` |
|      7 | 3330 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 3331 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 3332 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3333 | `		return -1;` |
|      - | 3334 | `	}` |
|      - | 3335 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 3336 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 3337 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 3338 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 3339 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 3340 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 3341 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 3342 | `	 * it as "could not be passed by reference"). */` |
|      - | 3343 | `	{` |
|      7 | 3344 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 3345 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 3346 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3347 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3348 | `			return -1;` |
|      - | 3349 | `		}` |
|      7 | 3350 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 3351 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 3352 | `	}` |
|      - | 3353 | `	{` |
|      - | 3354 | `		SyBlob sUrl;` |
|      7 | 3355 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 3356 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 3357 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 3358 | `		SyBlobRelease(&sUrl);` |
|      - | 3359 | `	}` |
|      9 | 3360 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 3361 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 3362 | `	ph7_value_int(&sOpts,0);` |
|      7 | 3363 | `	apArg[0] = &sPath;` |
|      7 | 3364 | `	apArg[1] = &sMode;` |
|      7 | 3365 | `	apArg[2] = &sOpts;` |
|      7 | 3366 | `	apArg[3] = &sOpened;` |
|      7 | 3367 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 3368 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 3369 | `		rc = -1;` |
|    ! 0 | 3370 | `	}` |
|      7 | 3371 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 3372 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 3373 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 3374 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 3375 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 3376 | `	if( rc != 0 ){` |
|    ! 0 | 3377 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 3378 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 3379 | `		return -1;` |
|      - | 3380 | `	}` |
|      7 | 3381 | `	*ppHandle = (void *)pH;` |
|      7 | 3382 | `	return PH7_OK;` |
|      4 | 3383 | `}` |
|      - | 3384 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 3385 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 3386 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 3387 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 3388 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 3389 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 3390 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 3391 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 3392 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 3393 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 3394 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 3395 | `PHL_UWRAP_THUNK(7)` |
|      - | 3396 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 3397 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 3398 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 3399 | `};` |
|      - | 3400 | `/*` |
|      - | 3401 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 3402 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 3403 | ` */` |
|      2 | 3404 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3405 | `{` |
|      - | 3406 | `	const char *zScheme,*zClass;` |
|      3 | 3407 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 3408 | `	if( nArg < 2 ){` |
|    ! 0 | 3409 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3410 | `		return PH7_OK;` |
|      - | 3411 | `	}` |
|      3 | 3412 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3413 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 3414 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 3415 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 3416 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3417 | `		return PH7_OK;` |
|      - | 3418 | `	}` |
|      - | 3419 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 3420 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 3421 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 3422 | `	{` |
|      3 | 3423 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 3424 | `		sxu32 n;` |
|     11 | 3425 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 3426 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 3427 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 3428 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3429 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 3430 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3431 | `				return PH7_OK;` |
|      - | 3432 | `			}` |
|      5 | 3433 | `		}` |
|      - | 3434 | `	}` |
|      3 | 3435 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 3436 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 3437 | `			iFree = i;` |
|      3 | 3438 | `			break;` |
|      - | 3439 | `		}` |
|    ! 0 | 3440 | `	}` |
|      3 | 3441 | `	if( iFree < 0 ){` |
|    ! 0 | 3442 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3443 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 3444 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3445 | `		return PH7_OK;` |
|      - | 3446 | `	}` |
|      - | 3447 | `	{` |
|      3 | 3448 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 3449 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 3450 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 3451 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 3452 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 3453 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 3454 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 3455 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 3456 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 3457 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 3458 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 3459 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 3460 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 3461 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 3462 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 3463 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 3464 | `	}` |
|      3 | 3465 | `	ph7_result_bool(pCtx,1);` |
|      3 | 3466 | `	return PH7_OK;` |
|      2 | 3467 | `}` |
|      2 | 3468 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3469 | `{` |
|      - | 3470 | `	const char *zScheme;` |
|      - | 3471 | `	int nScheme,i;` |
|      3 | 3472 | `	if( nArg < 1 ){` |
|    ! 0 | 3473 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3474 | `		return PH7_OK;` |
|      - | 3475 | `	}` |
|      3 | 3476 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3477 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 3478 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 3479 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 3480 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 3481 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 3482 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 3483 | `			 * what unregister means to a script — recorded. */` |
|      3 | 3484 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 3485 | `			ph7_result_bool(pCtx,1);` |
|      3 | 3486 | `			return PH7_OK;` |
|      - | 3487 | `		}` |
|    ! 0 | 3488 | `	}` |
|    ! 0 | 3489 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 3490 | `	return PH7_OK;` |
|      2 | 3491 | `}` |
|      - | 3492 | `#ifdef PH7_ENABLE_NET` |
|      - | 3493 | `/*` |
|      - | 3494 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 3495 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 3496 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 3497 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 3498 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 3499 | ` */` |
|     32 | 3500 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3501 | `{` |
|     32 | 3502 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     32 | 3503 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|     32 | 3504 | `	const char *zTarget,*zErr = "";` |
|      - | 3505 | `	char zHost[256];` |
|     32 | 3506 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 3507 | `	ph7_socket sock;` |
|      - | 3508 | `	io_private *pDev;` |
|      - | 3509 | `	sock_private *pSock;` |
|     32 | 3510 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|     32 | 3511 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|     32 | 3512 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|     32 | 3513 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 3514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3515 | `		return PH7_OK;` |
|      - | 3516 | `	}` |
|     32 | 3517 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 3518 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 3519 | `	{` |
|     32 | 3520 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|     32 | 3521 | `		const char *zSep = 0;` |
|    240 | 3522 | `		while( z < zEnd - 2 ){` |
|    212 | 3523 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 3524 | `				zSep = z;` |
|      4 | 3525 | `				break;` |
|      - | 3526 | `			}` |
|    208 | 3527 | `			z++;` |
|    ! 0 | 3528 | `		}` |
|     32 | 3529 | `		if( zSep ){` |
|      4 | 3530 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 3531 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3532 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 3533 | `					nTarget,zTarget);` |
|    ! 0 | 3534 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3535 | `				return PH7_OK;` |
|      - | 3536 | `			}` |
|      4 | 3537 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 3538 | `			zTarget = zSep + 3;` |
|      2 | 3539 | `		}` |
|      - | 3540 | `	}` |
|      - | 3541 | `	/* host[:port] */` |
|      - | 3542 | `	{` |
|     32 | 3543 | `		int i = nTarget - 1;` |
|     32 | 3544 | `		int nHost = nTarget;` |
|    282 | 3545 | `		while( i > 0 && zTarget[i] != ':' ){` |
|    250 | 3546 | `			i--;` |
|    ! 0 | 3547 | `		}` |
|     32 | 3548 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 3549 | `			sxi32 iTmp = 0;` |
|      2 | 3550 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 3551 | `			iPort = (int)iTmp;` |
|      2 | 3552 | `			nHost = i;` |
|      1 | 3553 | `		}` |
|     32 | 3554 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 3555 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 3556 | `		}` |
|     32 | 3557 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|     32 | 3558 | `		zHost[nHost] = 0;` |
|      - | 3559 | `	}` |
|     32 | 3560 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     30 | 3561 | `		iPort = ph7_value_to_int(apArg[1]);` |
|     15 | 3562 | `	}` |
|     32 | 3563 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|     32 | 3564 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|     32 | 3565 | `		if( rTimeout > 0 ){` |
|     32 | 3566 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     16 | 3567 | `		}` |
|     16 | 3568 | `	}` |
|     32 | 3569 | `	if( iPort < 0 ){` |
|    ! 0 | 3570 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3571 | `		return PH7_OK;` |
|      - | 3572 | `	}` |
|     32 | 3573 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|     32 | 3574 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 3575 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 3576 | `		{` |
|     28 | 3577 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|     28 | 3578 | `			if( pTmp ){` |
|     28 | 3579 | `				if( nArg > iArgErrno ){` |
|     28 | 3580 | `					ph7_value_int(pTmp,iErrno);` |
|     28 | 3581 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     14 | 3582 | `				}` |
|     28 | 3583 | `				if( nArg > iArgErrstr ){` |
|     28 | 3584 | `					ph7_value_string(pTmp,zErr,-1);` |
|     28 | 3585 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     14 | 3586 | `				}` |
|     14 | 3587 | `			}` |
|      - | 3588 | `		}` |
|      - | 3589 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|     42 | 3590 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     14 | 3591 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|     28 | 3592 | `		ph7_result_bool(pCtx,0);` |
|     28 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|      - | 3595 | `	{` |
|      4 | 3596 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 3597 | `		if( pTmp ){` |
|      4 | 3598 | `			if( nArg > iArgErrno ){` |
|      4 | 3599 | `				ph7_value_int(pTmp,0);` |
|      4 | 3600 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 3601 | `			}` |
|      4 | 3602 | `			if( nArg > iArgErrstr ){` |
|      4 | 3603 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 3604 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 3605 | `			}` |
|      2 | 3606 | `		}` |
|      - | 3607 | `	}` |
|      - | 3608 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 3609 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 3610 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 3611 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 3612 | `		PH7_NetClose(sock);` |
|    ! 0 | 3613 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3614 | `		return PH7_OK;` |
|      - | 3615 | `	}` |
|      4 | 3616 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 3617 | `	pSock->sock = sock;` |
|      4 | 3618 | `	pSock->bEof = 0;` |
|      4 | 3619 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 3620 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 3621 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 3622 | `	return PH7_OK;` |
|     16 | 3623 | `}` |
|      - | 3624 | `#endif /* PH7_ENABLE_NET */` |
|    376 | 3625 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3626 | `{` |
|      - | 3627 | `	const ph7_io_stream *pStream;` |
|      - | 3628 | `	const char *zUri,*zMode;` |
|      - | 3629 | `	ph7_value *pResource;` |
|      - | 3630 | `	io_private *pDev;` |
|      - | 3631 | `	int iLen,imLen;` |
|      - | 3632 | `	int iOpenFlags;` |
|    381 | 3633 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3634 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3635 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 3636 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3637 | `		return PH7_OK;` |
|      - | 3638 | `	}` |
|      - | 3639 | `	/* Extract the URI and the desired access mode */` |
|    381 | 3640 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    381 | 3641 | `	if( nArg > 1 ){` |
|    381 | 3642 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    193 | 3643 | `	}else{` |
|      - | 3644 | `		/* Set a default read-only mode */` |
|    ! 0 | 3645 | `		zMode = "r";` |
|    ! 0 | 3646 | `		imLen = (int)sizeof(char);` |
|      - | 3647 | `	}` |
|      - | 3648 | `	/* Try to extract a stream */` |
|    381 | 3649 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    381 | 3650 | `	if( pStream == 0 ){` |
|    ! 0 | 3651 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3652 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 3653 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3654 | `		return PH7_OK;` |
|      - | 3655 | `	}` |
|      - | 3656 | `	/* Allocate a new IO private instance */` |
|    381 | 3657 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    381 | 3658 | `	if( pDev == 0 ){` |
|    ! 0 | 3659 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3661 | `		return PH7_OK;` |
|      - | 3662 | `	}` |
|    381 | 3663 | `	pResource = 0;` |
|    381 | 3664 | `	if( nArg > 3 ){` |
|    ! 0 | 3665 | `		pResource = apArg[3];` |
|    381 | 3666 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 3667 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 3668 | `		 * access the underlying virtual machine.` |
|      - | 3669 | `		 */` |
|    135 | 3670 | `		pResource = apArg[0];` |
|     65 | 3671 | `	}` |
|      - | 3672 | `	/* Initialize the structure */` |
|    381 | 3673 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3674 | `	/* Convert open mode to PH7 flags */` |
|    381 | 3675 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 3676 | `	/* Try to get a handle */` |
|    569 | 3677 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    188 | 3678 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    381 | 3679 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3680 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|      3 | 3681 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3682 | `		ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 3683 | `		return PH7_OK;` |
|      - | 3684 | `	}` |
|      - | 3685 | `	/* All done,return the io_private instance as a resource */` |
|    379 | 3686 | `	ph7_result_resource(pCtx,pDev);` |
|    379 | 3687 | `	return PH7_OK;` |
|    193 | 3688 | `}` |
|      - | 3689 | `/*` |
|      - | 3690 | ` * bool fclose(resource $handle)` |
|      - | 3691 | ` *  Closes an open file pointer` |
|      - | 3692 | ` * Parameters` |
|      - | 3693 | ` *  $handle` |
|      - | 3694 | ` *   The file pointer.` |
|      - | 3695 | ` * Return` |
|      - | 3696 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3697 | ` */` |
|    436 | 3698 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3699 | `{` |
|      - | 3700 | `	const ph7_io_stream *pStream;` |
|      - | 3701 | `	io_private *pDev;` |
|      - | 3702 | `	ph7_vm *pVm;` |
|    441 | 3703 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3704 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3705 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3707 | `		return PH7_OK;` |
|      - | 3708 | `	}` |
|      - | 3709 | `	/* Extract our private data */` |
|    441 | 3710 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3711 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    441 | 3712 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 3713 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3714 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 3715 | `	}` |
|      - | 3716 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    439 | 3717 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3718 | `		/*Expecting an IO handle */` |
|    ! 0 | 3719 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3720 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3721 | `		return PH7_OK;` |
|      - | 3722 | `	}` |
|      - | 3723 | `	/* Point to the target IO stream device */` |
|    439 | 3724 | `	pStream = pDev->pStream;` |
|    439 | 3725 | `	if( pStream == 0 ){` |
|    ! 0 | 3726 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3727 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3728 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3729 | `			);` |
|    ! 0 | 3730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3731 | `		return PH7_OK;` |
|      - | 3732 | `	}` |
|      - | 3733 | `	/* Point to the VM that own this context */` |
|    439 | 3734 | `	pVm = pCtx->pVm;` |
|      - | 3735 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    439 | 3736 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 3737 | `		/* Perform the requested operation */` |
|    439 | 3738 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3739 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    439 | 3740 | `		MarkIOPrivateClosed(pDev);` |
|    217 | 3741 | `	}` |
|      - | 3742 | `	/* Return TRUE */` |
|    439 | 3743 | `	ph7_result_bool(pCtx,1);` |
|    439 | 3744 | `	return PH7_OK;` |
|    223 | 3745 | `}` |
|      - | 3746 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3747 | `/*` |
|      - | 3748 | ` * MD5/SHA1 digest consumer.` |
|      - | 3749 | ` */` |
|     72 | 3750 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3751 | `{` |
|      - | 3752 | `	/* Append hex chunk verbatim */` |
|     73 | 3753 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 3754 | `	return SXRET_OK;` |
|      1 | 3755 | `}` |
|      - | 3756 | `/*` |
|      - | 3757 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 3758 | ` *  Calculates the md5 hash of a given file.` |
|      - | 3759 | ` * Parameters` |
|      - | 3760 | ` *  $uri` |
|      - | 3761 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3762 | ` *  $raw_output` |
|      - | 3763 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 3764 | ` * Return` |
|      - | 3765 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 3766 | ` */` |
|      2 | 3767 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3768 | `{` |
|      - | 3769 | `	const ph7_io_stream *pStream;` |
|      - | 3770 | `	unsigned char zDigest[16];` |
|      3 | 3771 | `	int raw_output  = FALSE;` |
|      - | 3772 | `	const char *zFile;` |
|      - | 3773 | `	MD5Context sCtx;` |
|      - | 3774 | `	char zBuf[8192];` |
|      - | 3775 | `	void *pHandle;` |
|      - | 3776 | `	ph7_int64 n;` |
|      - | 3777 | `	int nLen;` |
|      3 | 3778 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3779 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3780 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3781 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3782 | `		return PH7_OK;` |
|      - | 3783 | `	}` |
|      - | 3784 | `	/* Extract the file path */` |
|      3 | 3785 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3786 | `	/* Point to the target IO stream device */` |
|      3 | 3787 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3788 | `	if( pStream == 0 ){` |
|    ! 0 | 3789 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3790 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3791 | `		return PH7_OK;` |
|      - | 3792 | `	}` |
|      3 | 3793 | `	if( nArg > 1 ){` |
|    ! 0 | 3794 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3795 | `	}` |
|      - | 3796 | `	/* Try to open the file in read-only mode */` |
|      3 | 3797 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3798 | `	if( pHandle == 0 ){` |
|    ! 0 | 3799 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3801 | `		return PH7_OK;` |
|      - | 3802 | `	}` |
|      - | 3803 | `	/* Init the MD5 context */` |
|      3 | 3804 | `	MD5Init(&sCtx);` |
|      - | 3805 | `	/* Perform the requested operation */` |
|      2 | 3806 | `	for(;;){` |
|      5 | 3807 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3808 | `		if( n < 1 ){` |
|      - | 3809 | `			/* EOF or IO error,break immediately */` |
|      3 | 3810 | `			break;` |
|      - | 3811 | `		}` |
|      3 | 3812 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3813 | `	}` |
|      - | 3814 | `	/* Close the stream */` |
|      3 | 3815 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3816 | `	/* Extract the digest */` |
|      3 | 3817 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 3818 | `	if( raw_output ){` |
|      - | 3819 | `		/* Output raw digest */` |
|    ! 0 | 3820 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3821 | `	}else{` |
|      - | 3822 | `		/* Perform a binary to hex conversion */` |
|      3 | 3823 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3824 | `	}` |
|      3 | 3825 | `	return PH7_OK;` |
|      2 | 3826 | `}` |
|      - | 3827 | `/*` |
|      - | 3828 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 3829 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 3830 | ` * Parameters` |
|      - | 3831 | ` *  $uri` |
|      - | 3832 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3833 | ` *  $raw_output` |
|      - | 3834 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 3835 | ` * Return` |
|      - | 3836 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 3837 | ` */` |
|      2 | 3838 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3839 | `{` |
|      - | 3840 | `	const ph7_io_stream *pStream;` |
|      - | 3841 | `	unsigned char zDigest[20];` |
|      3 | 3842 | `	int raw_output  = FALSE;` |
|      - | 3843 | `	const char *zFile;` |
|      - | 3844 | `	SHA1Context sCtx;` |
|      - | 3845 | `	char zBuf[8192];` |
|      - | 3846 | `	void *pHandle;` |
|      - | 3847 | `	ph7_int64 n;` |
|      - | 3848 | `	int nLen;` |
|      3 | 3849 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3850 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3851 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|      - | 3855 | `	/* Extract the file path */` |
|      3 | 3856 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3857 | `	/* Point to the target IO stream device */` |
|      3 | 3858 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3859 | `	if( pStream == 0 ){` |
|    ! 0 | 3860 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3862 | `		return PH7_OK;` |
|      - | 3863 | `	}` |
|      3 | 3864 | `	if( nArg > 1 ){` |
|    ! 0 | 3865 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3866 | `	}` |
|      - | 3867 | `	/* Try to open the file in read-only mode */` |
|      3 | 3868 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3869 | `	if( pHandle == 0 ){` |
|    ! 0 | 3870 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3871 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3872 | `		return PH7_OK;` |
|      - | 3873 | `	}` |
|      - | 3874 | `	/* Init the SHA1 context */` |
|      3 | 3875 | `	SHA1Init(&sCtx);` |
|      - | 3876 | `	/* Perform the requested operation */` |
|      2 | 3877 | `	for(;;){` |
|      5 | 3878 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3879 | `		if( n < 1 ){` |
|      - | 3880 | `			/* EOF or IO error,break immediately */` |
|      3 | 3881 | `			break;` |
|      - | 3882 | `		}` |
|      3 | 3883 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3884 | `	}` |
|      - | 3885 | `	/* Close the stream */` |
|      3 | 3886 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3887 | `	/* Extract the digest */` |
|      3 | 3888 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 3889 | `	if( raw_output ){` |
|      - | 3890 | `		/* Output raw digest */` |
|    ! 0 | 3891 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3892 | `	}else{` |
|      - | 3893 | `		/* Perform a binary to hex conversion */` |
|      3 | 3894 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3895 | `	}` |
|      3 | 3896 | `	return PH7_OK;` |
|      2 | 3897 | `}` |
|      - | 3898 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3899 | `/*` |
|      - | 3900 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 3901 | ` *  Parse a configuration file.` |
|      - | 3902 | ` * Parameters` |
|      - | 3903 | ` * $filename` |
|      - | 3904 | ` *  The filename of the ini file being parsed.` |
|      - | 3905 | ` * $process_sections` |
|      - | 3906 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 3907 | ` *  with the section names and settings included.` |
|      - | 3908 | ` *  The default for process_sections is FALSE.` |
|      - | 3909 | ` * $scanner_mode` |
|      - | 3910 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 3911 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 3912 | ` * Return` |
|      - | 3913 | ` *  The settings are returned as an associative array on success.` |
|      - | 3914 | ` *  Otherwise is returned.` |
|      - | 3915 | ` */` |
|      8 | 3916 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3917 | `{` |
|      - | 3918 | `	const ph7_io_stream *pStream;` |
|      - | 3919 | `	const char *zFile;` |
|      - | 3920 | `	SyBlob sContents;` |
|      - | 3921 | `	void *pHandle;` |
|      - | 3922 | `	int nLen;` |
|      9 | 3923 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      9 | 3924 | `	sxi32 rc = PH7_OK;` |
|      9 | 3925 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3926 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3927 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3928 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3929 | `		return PH7_OK;` |
|      - | 3930 | `	}` |
|      9 | 3931 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 3932 | `		iMode = ph7_value_to_int(apArg[2]);` |
|      6 | 3933 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|      6 | 3934 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|      - | 3935 | `			/* php screens the mode BEFORE touching the file */` |
|      - | 3936 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|      3 | 3937 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|      3 | 3938 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3939 | `			return PH7_OK;` |
|      - | 3940 | `		}` |
|      2 | 3941 | `	}` |
|      - | 3942 | `	/* Extract the file path */` |
|      7 | 3943 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3944 | `	/* Point to the target IO stream device */` |
|      7 | 3945 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      7 | 3946 | `	if( pStream == 0 ){` |
|    ! 0 | 3947 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3949 | `		return PH7_OK;` |
|      - | 3950 | `	}` |
|      - | 3951 | `	/* Try to open the file in read-only mode */` |
|      7 | 3952 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      7 | 3953 | `	if( pHandle == 0 ){` |
|    ! 0 | 3954 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3955 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3956 | `		return PH7_OK;` |
|      - | 3957 | `	}` |
|      7 | 3958 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 3959 | `	/* Read the whole file */` |
|      7 | 3960 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      7 | 3961 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 3962 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 3963 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3964 | `	}else{` |
|      - | 3965 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|     13 | 3966 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      6 | 3967 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|      - | 3968 | `	}` |
|      - | 3969 | `	/* Close the stream */` |
|      7 | 3970 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3971 | `	/* Release the working buffer */` |
|      7 | 3972 | `	SyBlobRelease(&sContents);` |
|      - | 3973 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      7 | 3974 | `	return rc;` |
|      5 | 3975 | `}` |
|      - | 3976 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 3977 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 3978 | `/*` |
|      - | 3979 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 3980 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 3981 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 3982 | ` * to any non-VFS resource).` |
|      - | 3983 | ` */` |
|      - | 3984 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 3985 | `{` |
|      - | 3986 | `	SXUNUSED(pResource);` |
|      - | 3987 | `	return "Unknown";` |
|      - | 3988 | `}` |
|      - | 3989 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 3990 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 3991 | `{` |
|      - | 3992 | `	SXUNUSED(pResource);` |
|      - | 3993 | `	return 0;` |
|      - | 3994 | `}` |
|      - | 3995 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3996 |  |
