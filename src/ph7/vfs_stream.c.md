# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1267/1894 lines (66.90%)

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
|      2 |   67 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   68 | `{` |
|      - |   69 | `	const ph7_io_stream *pStream;` |
|      - |   70 | `	io_private *pDev;` |
|      - |   71 | `	int rc;` |
|      3 |   72 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |   73 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |   74 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |   75 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   76 | `		return PH7_OK;` |
|      - |   77 | `	}` |
|      - |   78 | `	/* Extract our private data */` |
|      3 |   79 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |   80 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |   81 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |   82 | `		/*Expecting an IO handle */` |
|    ! 0 |   83 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the target IO stream device */` |
|      3 |   88 | `	pStream = pDev->pStream;` |
|      3 |   89 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 |   90 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   91 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |   92 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |   93 | `			);` |
|    ! 0 |   94 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   95 | `		return PH7_OK;` |
|      - |   96 | `	}` |
|      - |   97 | `	/* Perform the requested operation */` |
|      3 |   98 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      3 |   99 | `	if( rc == PH7_OK ){` |
|      - |  100 | `		/* Discard buffered data */` |
|      3 |  101 | `		ResetIOPrivate(pDev);` |
|      1 |  102 | `	}` |
|      - |  103 | `	/* IO result */` |
|      3 |  104 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  105 | `	return PH7_OK;` |
|      2 |  106 | `}` |
|      - |  107 | `/*` |
|      - |  108 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - |  109 | ` *  Seeks on a file pointer.` |
|      - |  110 | ` * Parameters` |
|      - |  111 | ` *  $handle` |
|      - |  112 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - |  113 | ` * $offset` |
|      - |  114 | ` *   The offset.` |
|      - |  115 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - |  116 | ` *   value in offset and set whence to SEEK_END.` |
|      - |  117 | ` *   whence` |
|      - |  118 | ` *   whence values are:` |
|      - |  119 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - |  120 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - |  121 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - |  122 | ` * Return` |
|      - |  123 | ` *  0 on success,-1 on failure` |
|      - |  124 | ` */` |
|     10 |  125 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  126 | `{` |
|      - |  127 | `	const ph7_io_stream *pStream;` |
|      - |  128 | `	io_private *pDev;` |
|      - |  129 | `	ph7_int64 iOfft;` |
|      - |  130 | `	int whence;` |
|      - |  131 | `	int rc;` |
|     12 |  132 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  133 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  134 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  135 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  136 | `		return PH7_OK;` |
|      - |  137 | `	}` |
|      - |  138 | `	/* Extract our private data */` |
|     12 |  139 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  140 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 |  141 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  142 | `		/*Expecting an IO handle */` |
|    ! 0 |  143 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  144 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  145 | `		return PH7_OK;` |
|      - |  146 | `	}` |
|      - |  147 | `	/* Point to the target IO stream device */` |
|     12 |  148 | `	pStream = pDev->pStream;` |
|     12 |  149 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  150 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  151 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  152 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  153 | `			);` |
|    ! 0 |  154 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  155 | `		return PH7_OK;` |
|      - |  156 | `	}` |
|      - |  157 | `	/* Extract the offset */` |
|     12 |  158 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 |  159 | `	whence = 0;/* SEEK_SET */` |
|     12 |  160 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 |  161 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 |  162 | `	}` |
|      - |  163 | `	/* Perform the requested operation */` |
|     12 |  164 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 |  165 | `	if( rc == PH7_OK ){` |
|      - |  166 | `		/* Ignore buffered data */` |
|     12 |  167 | `		ResetIOPrivate(pDev);` |
|      5 |  168 | `	}` |
|      - |  169 | `	/* IO result */` |
|     12 |  170 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 |  171 | `	return PH7_OK;` |
|      7 |  172 | `}` |
|      - |  173 | `/*` |
|      - |  174 | ` * int64 ftell(resource $handle)` |
|      - |  175 | ` *  Returns the current position of the file read/write pointer.` |
|      - |  176 | ` * Parameters` |
|      - |  177 | ` *  $handle` |
|      - |  178 | ` *   The file pointer.` |
|      - |  179 | ` * Return` |
|      - |  180 | ` *  Returns the position of the file pointer referenced by handle` |
|      - |  181 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - |  182 | ` *  FALSE is returned on failure.` |
|      - |  183 | ` */` |
|     12 |  184 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  185 | `{` |
|      - |  186 | `	const ph7_io_stream *pStream;` |
|      - |  187 | `	io_private *pDev;` |
|      - |  188 | `	ph7_int64 iOfft;` |
|     14 |  189 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  190 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  191 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  192 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  193 | `		return PH7_OK;` |
|      - |  194 | `	}` |
|      - |  195 | `	/* Extract our private data */` |
|     14 |  196 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  197 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 |  198 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  199 | `		/*Expecting an IO handle */` |
|    ! 0 |  200 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  201 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  202 | `		return PH7_OK;` |
|      - |  203 | `	}` |
|      - |  204 | `	/* Point to the target IO stream device */` |
|     14 |  205 | `	pStream = pDev->pStream;` |
|     14 |  206 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 |  207 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  208 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  209 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  210 | `			);` |
|    ! 0 |  211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  212 | `		return PH7_OK;` |
|      - |  213 | `	}` |
|      - |  214 | `	/* Perform the requested operation */` |
|     14 |  215 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - |  216 | `	/* IO result */` |
|     14 |  217 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 |  218 | `	return PH7_OK;` |
|      8 |  219 | `}` |
|      - |  220 | `/*` |
|      - |  221 | ` * bool rewind(resource $handle)` |
|      - |  222 | ` *  Rewind the position of a file pointer.` |
|      - |  223 | ` * Parameters` |
|      - |  224 | ` *  $handle` |
|      - |  225 | ` *   The file pointer.` |
|      - |  226 | ` * Return` |
|      - |  227 | ` *  TRUE on success or FALSE on failure.` |
|      - |  228 | ` */` |
|     14 |  229 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  230 | `{` |
|      - |  231 | `	const ph7_io_stream *pStream;` |
|      - |  232 | `	io_private *pDev;` |
|      - |  233 | `	int rc;` |
|     15 |  234 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  235 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  238 | `		return PH7_OK;` |
|      - |  239 | `	}` |
|      - |  240 | `	/* Extract our private data */` |
|     15 |  241 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  242 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 |  243 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  244 | `		/*Expecting an IO handle */` |
|    ! 0 |  245 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  247 | `		return PH7_OK;` |
|      - |  248 | `	}` |
|      - |  249 | `	/* Point to the target IO stream device */` |
|     15 |  250 | `	pStream = pDev->pStream;` |
|     15 |  251 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  252 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  253 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  254 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  255 | `			);` |
|    ! 0 |  256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  257 | `		return PH7_OK;` |
|      - |  258 | `	}` |
|      - |  259 | `	/* Perform the requested operation */` |
|     15 |  260 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 |  261 | `	if( rc == PH7_OK ){` |
|      - |  262 | `		/* Ignore buffered data */` |
|     15 |  263 | `		ResetIOPrivate(pDev);` |
|      7 |  264 | `	}` |
|      - |  265 | `	/* IO result */` |
|     15 |  266 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 |  267 | `	return PH7_OK;` |
|      8 |  268 | `}` |
|      - |  269 | `/*` |
|      - |  270 | ` * bool fflush(resource $handle)` |
|      - |  271 | ` *  Flushes the output to a file.` |
|      - |  272 | ` * Parameters` |
|      - |  273 | ` *  $handle` |
|      - |  274 | ` *   The file pointer.` |
|      - |  275 | ` * Return` |
|      - |  276 | ` *  TRUE on success or FALSE on failure.` |
|      - |  277 | ` */` |
|      2 |  278 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  279 | `{` |
|      - |  280 | `	const ph7_io_stream *pStream;` |
|      - |  281 | `	io_private *pDev;` |
|      - |  282 | `	int rc;` |
|      3 |  283 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  284 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  285 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  286 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  287 | `		return PH7_OK;` |
|      - |  288 | `	}` |
|      - |  289 | `	/* Extract our private data */` |
|      3 |  290 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  291 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |  292 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  293 | `		/*Expecting an IO handle */` |
|    ! 0 |  294 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  295 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  296 | `		return PH7_OK;` |
|      - |  297 | `	}` |
|      - |  298 | `	/* Point to the target IO stream device */` |
|      3 |  299 | `	pStream = pDev->pStream;` |
|      3 |  300 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 |  301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  303 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  304 | `			);` |
|    ! 0 |  305 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|      - |  308 | `	/* Perform the requested operation */` |
|      3 |  309 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - |  310 | `	/* IO result */` |
|      3 |  311 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  312 | `	return PH7_OK;` |
|      2 |  313 | `}` |
|      - |  314 | `/*` |
|      - |  315 | ` * bool feof(resource $handle)` |
|      - |  316 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  317 | ` * Parameters` |
|      - |  318 | ` *  $handle` |
|      - |  319 | ` *   The file pointer.` |
|      - |  320 | ` * Return` |
|      - |  321 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  322 | ` */` |
|  10403 |  323 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  324 | `{` |
|      - |  325 | `	const ph7_io_stream *pStream;` |
|      - |  326 | `	io_private *pDev;` |
|      - |  327 | `	int rc;` |
|  10408 |  328 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  329 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  330 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  331 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  332 | `		return PH7_OK;` |
|      - |  333 | `	}` |
|      - |  334 | `	/* Extract our private data */` |
|  10408 |  335 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  336 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10408 |  337 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  338 | `		/*Expecting an IO handle */` |
|    ! 0 |  339 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  340 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  341 | `		return PH7_OK;` |
|      - |  342 | `	}` |
|      - |  343 | `	/* Point to the target IO stream device */` |
|  10408 |  344 | `	pStream = pDev->pStream;` |
|  10408 |  345 | `	if( pStream == 0 ){` |
|    ! 0 |  346 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  347 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  348 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  349 | `			);` |
|    ! 0 |  350 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  351 | `		return PH7_OK;` |
|      - |  352 | `	}` |
|  10408 |  353 | `	rc = SXERR_EOF;` |
|      - |  354 | `	/* Perform the requested operation */` |
|  10408 |  355 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  356 | `		/* Data is available */` |
|   4595 |  357 | `		rc = PH7_OK;` |
|   2300 |  358 | `	}else{` |
|      - |  359 | `		char zBuf[4096];` |
|      - |  360 | `		ph7_int64 n;` |
|      - |  361 | `		/* Perform a buffered read */` |
|   5818 |  362 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5818 |  363 | `		if( n > 0 ){` |
|      - |  364 | `			/* Copy buffered data */` |
|   1930 |  365 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1930 |  366 | `			rc = PH7_OK;` |
|    962 |  367 | `		}` |
|      - |  368 | `	}` |
|      - |  369 | `	/* EOF or not */` |
|  10408 |  370 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10408 |  371 | `	return PH7_OK;` |
|   5206 |  372 | `}` |
|      - |  373 | `/*` |
|      - |  374 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  375 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  376 | ` * [i.e: IO error ] or EOF.` |
|      - |  377 | ` */` |
|     37 |  378 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      3 |  379 | `{` |
|     40 |  380 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     40 |  381 | `	char *zBuf = (char *)pBuf;` |
|      - |  382 | `	ph7_int64 n,nRead;` |
|     40 |  383 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     40 |  384 | `	if( n > 0 ){` |
|      3 |  385 | `		if( n > nLen ){` |
|    ! 0 |  386 | `			n = nLen;` |
|    ! 0 |  387 | `		}` |
|      - |  388 | `		/* Copy the buffered data */` |
|      3 |  389 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  390 | `		/* Update the read offset */` |
|      3 |  391 | `		pDev->nOfft += (sxu32)n;` |
|      3 |  392 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  393 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      3 |  394 | `			SyBlobReset(&pDev->sBuffer);` |
|      3 |  395 | `			pDev->nOfft = 0;` |
|      1 |  396 | `		}` |
|      3 |  397 | `		nLen -= n;` |
|      3 |  398 | `		if( nLen < 1 ){` |
|      - |  399 | `			/* All done */` |
|    ! 0 |  400 | `			return n;` |
|      - |  401 | `		}` |
|      - |  402 | `		/* Advance the cursor */` |
|      3 |  403 | `		zBuf += n;` |
|      1 |  404 | `	}` |
|      - |  405 | `	/* Read without buffering */` |
|     40 |  406 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     40 |  407 | `	if( nRead > 0 ){` |
|     36 |  408 | `		n += nRead;` |
|     21 |  409 | `	}else if( n < 1 ){` |
|      - |  410 | `		/* EOF or IO error */` |
|      3 |  411 | `		return nRead;` |
|      - |  412 | `	}` |
|     38 |  413 | `	return n;` |
|     21 |  414 | `}` |
|      - |  415 | `/*` |
|      - |  416 | ` * Extract a single line from the buffered input.` |
|      - |  417 | ` */` |
|   6580 |  418 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  419 | `{` |
|      - |  420 | `	const char *zIn,*zEnd,*zPtr;` |
|   6585 |  421 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6585 |  422 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6585 |  423 | `	zPtr = zIn;` |
| 408262 |  424 | `	while( zIn < zEnd ){` |
| 408160 |  425 | `		if( zIn[0] == '\n' ){` |
|      - |  426 | `			/* Line found */` |
|   6483 |  427 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6483 |  428 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6483 |  429 | `			*pzLine = zPtr;` |
|   6483 |  430 | `			return SXRET_OK;` |
|      - |  431 | `		}` |
| 401682 |  432 | `		zIn++;` |
|      5 |  433 | `	}` |
|      - |  434 | `	/* No line were found */` |
|    107 |  435 | `	return SXERR_NOTFOUND;` |
|   3295 |  436 | `}` |
|      - |  437 | `/*` |
|      - |  438 | ` * Read a single line from the underlying IO stream device.` |
|      - |  439 | ` */` |
|   6584 |  440 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  441 | `{` |
|   6589 |  442 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - |  443 | `	char zBuf[8192];` |
|      - |  444 | `	ph7_int64 n;` |
|      - |  445 | `	sxi32 rc;` |
|   6589 |  446 | `	n = 0;` |
|   6589 |  447 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  448 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 |  449 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 |  450 | `		pDev->nOfft = 0;` |
|     34 |  451 | `	}` |
|   6589 |  452 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  453 | `		/* Check if there is a line */` |
|   6521 |  454 | `		rc = GetLine(pDev,&n,pzData);` |
|   6521 |  455 | `		if( rc == SXRET_OK ){` |
|      - |  456 | `			/* Got line,update the cursor  */` |
|   6423 |  457 | `			pDev->nOfft += (sxu32)n;` |
|   6423 |  458 | `			return n;` |
|      - |  459 | `		}` |
|     49 |  460 | `	}` |
|      - |  461 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  462 | `	 * limit is reached.` |
|      - |  463 | `	 */` |
|     85 |  464 | `	for(;;){` |
|    175 |  465 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    175 |  466 | `		if( n < 1 ){` |
|      - |  467 | `			/* EOF or IO error */` |
|    111 |  468 | `			break;` |
|      - |  469 | `		}` |
|      - |  470 | `		/* Append the data just read */` |
|     66 |  471 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  472 | `		/* Try to extract a line */` |
|     66 |  473 | `		rc = GetLine(pDev,&n,pzData);` |
|     66 |  474 | `		if( rc == SXRET_OK ){` |
|      - |  475 | `			/* Got one,return immediately */` |
|     62 |  476 | `			pDev->nOfft += (sxu32)n;` |
|     62 |  477 | `			return n;` |
|      - |  478 | `		}` |
|      5 |  479 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - |  480 | `			/* Read limit reached,return the available data */` |
|    ! 0 |  481 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 |  482 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  483 | `			/* Reset the working buffer */` |
|    ! 0 |  484 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 |  485 | `			pDev->nOfft = 0;` |
|    ! 0 |  486 | `			return n;` |
|      - |  487 | `		}` |
|      1 |  488 | `	}` |
|    111 |  489 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  490 | `		/* Read limit reached,return the available data */` |
|    107 |  491 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    107 |  492 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  493 | `		/* Reset the working buffer */` |
|    107 |  494 | `		SyBlobReset(&pDev->sBuffer);` |
|    107 |  495 | `		pDev->nOfft = 0;` |
|     51 |  496 | `	}` |
|    111 |  497 | `	return n;` |
|   3297 |  498 | `}` |
|      - |  499 | `/*` |
|      - |  500 | ` * Open an IO stream handle.` |
|      - |  501 | ` * Notes on stream:` |
|      - |  502 | ` * According to the PHP reference manual.` |
|      - |  503 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - |  504 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - |  505 | ` * to an arbitrary locations within the stream.` |
|      - |  506 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - |  507 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - |  508 | ` * on a remote server.` |
|      - |  509 | ` * A stream is referenced as: scheme://target` |
|      - |  510 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - |  511 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - |  512 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - |  513 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - |  514 | ` *  with a path appended.` |
|      - |  515 | ` *` |
|      - |  516 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - |  517 | ` * Please refer to the official documentation for a full discussion.` |
|      - |  518 | ` * This function return a handle on success. Otherwise null.` |
|      - |  519 | ` */` |
|  28962 |  520 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  521 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 |  522 | `{` |
|  28967 |  523 | `	void *pHandle = 0; /* cc warning */` |
|      - |  524 | `	SyString sFile;` |
|      - |  525 | `	ph7_value sDummy;` |
|      - |  526 | `	int rc;` |
|  28967 |  527 | `	if( pStream == 0 ){` |
|      - |  528 | `		/* No such stream device */` |
|    ! 0 |  529 | `		return 0;` |
|      - |  530 | `	}` |
|  28967 |  531 | `	if( pResource == 0 ){` |
|      - |  532 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  533 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  534 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  535 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  536 | `		 * reads it during the call, and file:// ignores it. */` |
|  28943 |  537 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  28943 |  538 | `		pResource = &sDummy;` |
|  14469 |  539 | `	}` |
|  28967 |  540 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  28967 |  541 | `	if( use_include ){` |
|   9070 |  542 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - |  543 | `#ifdef __WINNT__` |
|      - |  544 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  545 | `#endif` |
|   9034 |  546 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9028 |  547 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - |  548 | `				/*  Open the file directly */` |
|     46 |  549 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     46 |  550 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - |  551 | `					/* Mark as included */` |
|     46 |  552 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     21 |  553 | `				}` |
|     25 |  554 | `		}else{` |
|      - |  555 | `			SyString *pPath;` |
|      - |  556 | `			SyBlob sWorker;` |
|      - |  557 | `#ifdef __WINNT__` |
|      - |  558 | `			static const int c = '\\';` |
|      - |  559 | `#else` |
|      - |  560 | `			static const int c = '/';` |
|      - |  561 | `#endif` |
|      - |  562 | `			/* Init the path builder working buffer */` |
|   9030 |  563 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  564 | `			/* Build a path from the set of include path */` |
|   9030 |  565 | `			SySetResetCursor(&pVm->aPaths);` |
|   9030 |  566 | `			rc = SXERR_IO;` |
|   9036 |  567 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - |  568 | `				/* Build full path */` |
|   9030 |  569 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - |  570 | `				/* Append null terminator */` |
|   9030 |  571 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  572 | `					continue;` |
|      - |  573 | `				}` |
|      - |  574 | `				/* Try to open the file */` |
|   9030 |  575 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9030 |  576 | `				if( rc == PH7_OK ){` |
|   9024 |  577 | `					if( bPushInclude ){` |
|      - |  578 | `						/* Mark as included */` |
|   9024 |  579 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4511 |  580 | `					}` |
|   9024 |  581 | `					break;` |
|      - |  582 | `				}` |
|      - |  583 | `				/* Reset the working buffer */` |
|      8 |  584 | `				SyBlobReset(&sWorker);` |
|      - |  585 | `				/* Check the next path */` |
|      2 |  586 | `			}` |
|   9030 |  587 | `			SyBlobRelease(&sWorker);` |
|      - |  588 | `		}` |
|   4539 |  589 | `	}else{` |
|      - |  590 | `		/* Open the URI direcly */` |
|  19897 |  591 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - |  592 | `	}` |
|  28967 |  593 | `	if( rc != PH7_OK ){` |
|      - |  594 | `		/* IO error */` |
|     22 |  595 | `		return 0;` |
|      - |  596 | `	}` |
|      - |  597 | `	/* Return the file handle */` |
|  28949 |  598 | `	return pHandle;` |
|  14486 |  599 | `}` |
|      - |  600 | `/*` |
|      - |  601 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - |  602 | ` * Store the read data in the given BLOB (last argument).` |
|      - |  603 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - |  604 | ` */` |
|   9054 |  605 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 |  606 | `{` |
|      - |  607 | `	ph7_int64 nRead;` |
|      - |  608 | `	char zBuf[8192]; /* 8K */` |
|      - |  609 | `	int rc;` |
|      - |  610 | `	/* Perform the requested operation */` |
|   9054 |  611 | `	for(;;){` |
|  18112 |  612 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  18112 |  613 | `		if( nRead < 1 ){` |
|      - |  614 | `			/* EOF or IO error */` |
|   9058 |  615 | `			break;` |
|      - |  616 | `		}` |
|      - |  617 | `		/* Append contents */` |
|   9058 |  618 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9058 |  619 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  620 | `			break;` |
|      - |  621 | `		}` |
|      4 |  622 | `	}` |
|   9058 |  623 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 |  624 | `}` |
|      - |  625 | `/*` |
|      - |  626 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - |  627 | ` */` |
|  29072 |  628 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 |  629 | `{` |
|  29077 |  630 | `	if( pStream->xClose ){` |
|  29077 |  631 | `		pStream->xClose(pHandle);` |
|  14536 |  632 | `	}` |
|  29077 |  633 | `}` |
|      - |  634 | `/*` |
|      - |  635 | ` * string fgetc(resource $handle)` |
|      - |  636 | ` *  Gets a character from the given file pointer.` |
|      - |  637 | ` * Parameters` |
|      - |  638 | ` *  $handle` |
|      - |  639 | ` *   The file pointer.` |
|      - |  640 | ` * Return` |
|      - |  641 | ` *  Returns a string containing a single character read from the file` |
|      - |  642 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - |  643 | ` * WARNING` |
|      - |  644 | ` *  This operation is extremely slow.Avoid using it.` |
|      - |  645 | ` */` |
|      4 |  646 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  647 | `{` |
|      - |  648 | `	const ph7_io_stream *pStream;` |
|      - |  649 | `	io_private *pDev;` |
|      - |  650 | `	int c,n;` |
|      5 |  651 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  652 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  653 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  655 | `		return PH7_OK;` |
|      - |  656 | `	}` |
|      - |  657 | `	/* Extract our private data */` |
|      5 |  658 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  659 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 |  660 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  661 | `		/*Expecting an IO handle */` |
|    ! 0 |  662 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  664 | `		return PH7_OK;` |
|      - |  665 | `	}` |
|      - |  666 | `	/* Point to the target IO stream device */` |
|      5 |  667 | `	pStream = pDev->pStream;` |
|      5 |  668 | `	if( pStream == 0  ){` |
|    ! 0 |  669 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  670 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  671 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  672 | `			);` |
|    ! 0 |  673 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  674 | `		return PH7_OK;` |
|      - |  675 | `	}` |
|      - |  676 | `	/* Perform the requested operation */` |
|      5 |  677 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - |  678 | `	/* IO result */` |
|      5 |  679 | `	if( n < 1 ){` |
|      - |  680 | `		/* EOF or error,return FALSE */` |
|    ! 0 |  681 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  682 | `	}else{` |
|      - |  683 | `		/* Return the string holding the character */` |
|      5 |  684 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - |  685 | `	}` |
|      5 |  686 | `	return PH7_OK;` |
|      3 |  687 | `}` |
|      - |  688 | `/*` |
|      - |  689 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - |  690 | ` *  Gets line from file pointer.` |
|      - |  691 | ` * Parameters` |
|      - |  692 | ` *  $handle` |
|      - |  693 | ` *   The file pointer.` |
|      - |  694 | ` * $length` |
|      - |  695 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  696 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  697 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  698 | ` *  the end of the line.` |
|      - |  699 | ` * Return` |
|      - |  700 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - |  701 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - |  702 | ` *  If an error occurs, FALSE is returned.` |
|      - |  703 | ` */` |
|   6574 |  704 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  705 | `{` |
|      - |  706 | `	const ph7_io_stream *pStream;` |
|      - |  707 | `	const char *zLine;` |
|      - |  708 | `	io_private *pDev;` |
|      - |  709 | `	ph7_int64 n,nLen;` |
|   6579 |  710 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  711 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  712 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  713 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  714 | `		return PH7_OK;` |
|      - |  715 | `	}` |
|      - |  716 | `	/* Extract our private data */` |
|   6579 |  717 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  718 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6579 |  719 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  720 | `		/*Expecting an IO handle */` |
|    ! 0 |  721 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  723 | `		return PH7_OK;` |
|      - |  724 | `	}` |
|      - |  725 | `	/* Point to the target IO stream device */` |
|   6579 |  726 | `	pStream = pDev->pStream;` |
|   6579 |  727 | `	if( pStream == 0  ){` |
|    ! 0 |  728 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  729 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  730 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  731 | `			);` |
|    ! 0 |  732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  733 | `		return PH7_OK;` |
|      - |  734 | `	}` |
|   6579 |  735 | `	nLen = -1;` |
|   6579 |  736 | `	if( nArg > 1 ){` |
|      - |  737 | `		/* Maximum data to read */` |
|    ! 0 |  738 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 |  739 | `	}` |
|      - |  740 | `	/* Perform the requested operation */` |
|   6579 |  741 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6579 |  742 | `	if( n < 1 ){` |
|      - |  743 | `		/* EOF or IO error,return FALSE */` |
|      7 |  744 | `		ph7_result_bool(pCtx,0);` |
|      6 |  745 | `	}else{` |
|      - |  746 | `		/* Return the freshly extracted line */` |
|   6577 |  747 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - |  748 | `	}` |
|   6579 |  749 | `	return PH7_OK;` |
|   3292 |  750 | `}` |
|      - |  751 | `/*` |
|      - |  752 | ` * string fread(resource $handle,int64 $length)` |
|      - |  753 | ` *  Binary-safe file read.` |
|      - |  754 | ` * Parameters` |
|      - |  755 | ` *  $handle` |
|      - |  756 | ` *   The file pointer.` |
|      - |  757 | ` * $length` |
|      - |  758 | ` *  Up to length number of bytes read.` |
|      - |  759 | ` * Return` |
|      - |  760 | ` *  The data readen on success or FALSE on failure.` |
|      - |  761 | ` */` |
|     29 |  762 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  763 | `{` |
|      - |  764 | `	const ph7_io_stream *pStream;` |
|      - |  765 | `	io_private *pDev;` |
|      - |  766 | `	ph7_int64 nRead;` |
|      - |  767 | `	void *pBuf;` |
|      - |  768 | `	int nLen;` |
|     32 |  769 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  770 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  771 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  772 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  773 | `		return PH7_OK;` |
|      - |  774 | `	}` |
|      - |  775 | `	/* Extract our private data */` |
|     32 |  776 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  777 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     32 |  778 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  779 | `		/*Expecting an IO handle */` |
|    ! 0 |  780 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  781 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  782 | `		return PH7_OK;` |
|      - |  783 | `	}` |
|      - |  784 | `	/* Point to the target IO stream device */` |
|     32 |  785 | `	pStream = pDev->pStream;` |
|     32 |  786 | `	if( pStream == 0  ){` |
|    ! 0 |  787 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  788 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  789 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  790 | `			);` |
|    ! 0 |  791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  792 | `		return PH7_OK;` |
|      - |  793 | `	}` |
|     32 |  794 | `        nLen = 4096;` |
|     32 |  795 | `	if( nArg > 1 ){` |
|     32 |  796 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     32 |  797 | `	  if( nLen < 1 ){` |
|      - |  798 | `		/* Invalid length,set a default length */` |
|    ! 0 |  799 | `		nLen = 4096;` |
|    ! 0 |  800 | `	  }` |
|     14 |  801 | `        }` |
|      - |  802 | `	/* Allocate enough buffer */` |
|     32 |  803 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     32 |  804 | `	if( pBuf == 0 ){` |
|    ! 0 |  805 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  807 | `		return PH7_OK;` |
|      - |  808 | `	}` |
|      - |  809 | `	/* Perform the requested operation */` |
|     32 |  810 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     32 |  811 | `	if( nRead < 1 ){` |
|      - |  812 | `		/* Nothing read,return FALSE */` |
|    ! 0 |  813 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  814 | `	}else{` |
|      - |  815 | `		/* Make a copy of the data just read */` |
|     32 |  816 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - |  817 | `	}` |
|      - |  818 | `	/* Release the buffer */` |
|     32 |  819 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     32 |  820 | `	return PH7_OK;` |
|     17 |  821 | `}` |
|      - |  822 | `/*` |
|      - |  823 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - |  824 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - |  825 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - |  826 | ` * Parameters` |
|      - |  827 | ` * $handle` |
|      - |  828 | ` *   The file pointer.` |
|      - |  829 | ` * $length` |
|      - |  830 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  831 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  832 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  833 | ` *  the end of the line.` |
|      - |  834 | ` * $delimiter` |
|      - |  835 | ` *   Set the field delimiter (one character only).` |
|      - |  836 | ` * $enclosure` |
|      - |  837 | ` *   Set the field enclosure character (one character only).` |
|      - |  838 | ` * $escape` |
|      - |  839 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - |  840 | ` * Return` |
|      - |  841 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - |  842 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - |  843 | ` *  If an error occurs, FALSE is returned.` |
|      - |  844 | ` */` |
|      2 |  845 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  846 | `{` |
|      - |  847 | `	const ph7_io_stream *pStream;` |
|      - |  848 | `	const char *zLine;` |
|      - |  849 | `	io_private *pDev;` |
|      - |  850 | `	ph7_int64 n,nLen;` |
|      3 |  851 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  852 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  853 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  854 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  855 | `		return PH7_OK;` |
|      - |  856 | `	}` |
|      - |  857 | `	/* Extract our private data */` |
|      3 |  858 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  859 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |  860 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  861 | `		/*Expecting an IO handle */` |
|    ! 0 |  862 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  863 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  864 | `		return PH7_OK;` |
|      - |  865 | `	}` |
|      - |  866 | `	/* Point to the target IO stream device */` |
|      3 |  867 | `	pStream = pDev->pStream;` |
|      3 |  868 | `	if( pStream == 0  ){` |
|    ! 0 |  869 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  870 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  871 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  872 | `			);` |
|    ! 0 |  873 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  874 | `		return PH7_OK;` |
|      - |  875 | `	}` |
|      3 |  876 | `	nLen = -1;` |
|      3 |  877 | `	if( nArg > 1 ){` |
|      - |  878 | `		/* Maximum data to read */` |
|      3 |  879 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 |  880 | `	}` |
|      - |  881 | `	/* Perform the requested operation */` |
|      3 |  882 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 |  883 | `	if( n < 1 ){` |
|      - |  884 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 |  885 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  886 | `	}else{` |
|      - |  887 | `		ph7_value *pArray;` |
|      3 |  888 | `		int delim  = ',';   /* Delimiter */` |
|      3 |  889 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 |  890 | `		int escape = '\\';  /* Escape character */` |
|      3 |  891 | `		if( nArg > 2 ){` |
|      - |  892 | `			const char *zPtr;` |
|      - |  893 | `			int i;` |
|      3 |  894 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - |  895 | `				/* Extract the delimiter */` |
|      3 |  896 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 |  897 | `				if( i > 0 ){` |
|      3 |  898 | `					delim = zPtr[0];` |
|      1 |  899 | `				}` |
|      1 |  900 | `			}` |
|      3 |  901 | `			if( nArg > 3 ){` |
|      3 |  902 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - |  903 | `					/* Extract the enclosure */` |
|      3 |  904 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 |  905 | `					if( i > 0 ){` |
|      3 |  906 | `						encl = zPtr[0];` |
|      1 |  907 | `					}` |
|      1 |  908 | `				}` |
|      3 |  909 | `				if( nArg > 4 ){` |
|      3 |  910 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - |  911 | `						/* Extract the escape character */` |
|      3 |  912 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 |  913 | `						if( i > 0 ){` |
|      3 |  914 | `							escape = zPtr[0];` |
|      1 |  915 | `						}` |
|      1 |  916 | `					}` |
|      1 |  917 | `				}` |
|      1 |  918 | `			}` |
|      1 |  919 | `		}` |
|      - |  920 | `		/* Create our array */` |
|      3 |  921 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 |  922 | `		if( pArray == 0 ){` |
|    ! 0 |  923 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  924 | `			ph7_result_null(pCtx);` |
|    ! 0 |  925 | `			return PH7_OK;` |
|      - |  926 | `		}` |
|      - |  927 | `		/* Parse the raw input */` |
|      3 |  928 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - |  929 | `		/* Return the freshly created array  */` |
|      3 |  930 | `		ph7_result_value(pCtx,pArray);` |
|      - |  931 | `	}` |
|      3 |  932 | `	return PH7_OK;` |
|      2 |  933 | `}` |
|      - |  934 | `/*` |
|      - |  935 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - |  936 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - |  937 | ` * Parameters` |
|      - |  938 | ` * $handle` |
|      - |  939 | ` *   The file pointer.` |
|      - |  940 | ` * $length` |
|      - |  941 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - |  942 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - |  943 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - |  944 | ` *  the end of the line.` |
|      - |  945 | ` * $allowable_tags` |
|      - |  946 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - |  947 | ` * Return` |
|      - |  948 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - |  949 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - |  950 | ` */` |
|      2 |  951 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  952 | `{` |
|      - |  953 | `	const ph7_io_stream *pStream;` |
|      - |  954 | `	const char *zLine;` |
|      - |  955 | `	io_private *pDev;` |
|      - |  956 | `	ph7_int64 n,nLen;` |
|      3 |  957 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  958 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  959 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  960 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  961 | `		return PH7_OK;` |
|      - |  962 | `	}` |
|      - |  963 | `	/* Extract our private data */` |
|      3 |  964 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  965 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 |  966 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  967 | `		/*Expecting an IO handle */` |
|    ! 0 |  968 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  969 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  970 | `		return PH7_OK;` |
|      - |  971 | `	}` |
|      - |  972 | `	/* Point to the target IO stream device */` |
|      3 |  973 | `	pStream = pDev->pStream;` |
|      3 |  974 | `	if( pStream == 0  ){` |
|    ! 0 |  975 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  976 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  977 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  978 | `			);` |
|    ! 0 |  979 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  980 | `		return PH7_OK;` |
|      - |  981 | `	}` |
|      3 |  982 | `	nLen = -1;` |
|      3 |  983 | `	if( nArg > 1 ){` |
|      - |  984 | `		/* Maximum data to read */` |
|    ! 0 |  985 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 |  986 | `	}` |
|      - |  987 | `	/* Perform the requested operation */` |
|      3 |  988 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 |  989 | `	if( n < 1 ){` |
|      - |  990 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 |  991 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  992 | `	}else{` |
|      3 |  993 | `		const char *zTaglist = 0;` |
|      3 |  994 | `		int nTaglen = 0;` |
|      3 |  995 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - |  996 | `			/* Allowed tag */` |
|    ! 0 |  997 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 |  998 | `		}` |
|      - |  999 | `		/* Process data just read */` |
|      3 | 1000 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 1001 | `	}` |
|      3 | 1002 | `	return PH7_OK;` |
|      2 | 1003 | `}` |
|      - | 1004 | `/*` |
|      - | 1005 | ` * string readdir(resource $dir_handle)` |
|      - | 1006 | ` *   Read entry from directory handle.` |
|      - | 1007 | ` * Parameter` |
|      - | 1008 | ` *  $dir_handle` |
|      - | 1009 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1010 | ` * Return` |
|      - | 1011 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 1012 | ` */` |
|  10832 | 1013 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1014 | `{` |
|      - | 1015 | `	const ph7_io_stream *pStream;` |
|      - | 1016 | `	io_private *pDev;` |
|      - | 1017 | `	int rc;` |
|  10837 | 1018 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1019 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1020 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1021 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1022 | `		return PH7_OK;` |
|      - | 1023 | `	}` |
|      - | 1024 | `	/* Extract our private data */` |
|  10837 | 1025 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1026 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10837 | 1027 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1028 | `		/*Expecting an IO handle */` |
|    ! 0 | 1029 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1030 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1031 | `		return PH7_OK;` |
|      - | 1032 | `	}` |
|      - | 1033 | `	/* Point to the target IO stream device */` |
|  10837 | 1034 | `	pStream = pDev->pStream;` |
|  10837 | 1035 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1036 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1037 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1038 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1039 | `			);` |
|    ! 0 | 1040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1041 | `		return PH7_OK;` |
|      - | 1042 | `	}` |
|  10837 | 1043 | `	ph7_result_bool(pCtx,0);` |
|      - | 1044 | `	/* Perform the requested operation */` |
|  10837 | 1045 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  10837 | 1046 | `	if( rc != PH7_OK ){` |
|      - | 1047 | `		/* Return FALSE */` |
|   1065 | 1048 | `		ph7_result_bool(pCtx,0);` |
|    530 | 1049 | `	}` |
|  10837 | 1050 | `	return PH7_OK;` |
|   5421 | 1051 | `}` |
|      - | 1052 | `/*` |
|      - | 1053 | ` * void rewinddir(resource $dir_handle)` |
|      - | 1054 | ` *   Rewind directory handle.` |
|      - | 1055 | ` * Parameter` |
|      - | 1056 | ` *  $dir_handle` |
|      - | 1057 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1058 | ` * Return` |
|      - | 1059 | ` *  FALSE on failure.` |
|      - | 1060 | ` */` |
|      2 | 1061 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1062 | `{` |
|      - | 1063 | `	const ph7_io_stream *pStream;` |
|      - | 1064 | `	io_private *pDev;` |
|      3 | 1065 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1066 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1067 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1068 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1069 | `		return PH7_OK;` |
|      - | 1070 | `	}` |
|      - | 1071 | `	/* Extract our private data */` |
|      3 | 1072 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1073 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1074 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1075 | `		/*Expecting an IO handle */` |
|    ! 0 | 1076 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1077 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1078 | `		return PH7_OK;` |
|      - | 1079 | `	}` |
|      - | 1080 | `	/* Point to the target IO stream device */` |
|      3 | 1081 | `	pStream = pDev->pStream;` |
|      3 | 1082 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 1083 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1084 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1085 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1086 | `			);` |
|    ! 0 | 1087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1088 | `		return PH7_OK;` |
|      - | 1089 | `	}` |
|      - | 1090 | `	/* Perform the requested operation */` |
|      3 | 1091 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 1092 | `	return PH7_OK;` |
|      2 | 1093 | ` }` |
|      - | 1094 | `/* Forward declaration */` |
|      - | 1095 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * void closedir(resource $dir_handle)` |
|      - | 1098 | ` *   Close directory handle.` |
|      - | 1099 | ` * Parameter` |
|      - | 1100 | ` *  $dir_handle` |
|      - | 1101 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1102 | ` * Return` |
|      - | 1103 | ` *  FALSE on failure.` |
|      - | 1104 | ` */` |
|   1064 | 1105 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1106 | `{` |
|      - | 1107 | `	const ph7_io_stream *pStream;` |
|      - | 1108 | `	io_private *pDev;` |
|   1069 | 1109 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1110 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1111 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1112 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1113 | `		return PH7_OK;` |
|      - | 1114 | `	}` |
|      - | 1115 | `	/* Extract our private data */` |
|   1069 | 1116 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1117 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1069 | 1118 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1119 | `		/*Expecting an IO handle */` |
|    ! 0 | 1120 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1121 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1122 | `		return PH7_OK;` |
|      - | 1123 | `	}` |
|      - | 1124 | `	/* Point to the target IO stream device */` |
|   1069 | 1125 | `	pStream = pDev->pStream;` |
|   1069 | 1126 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1127 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1128 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1129 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1130 | `			);` |
|    ! 0 | 1131 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1132 | `		return PH7_OK;` |
|      - | 1133 | `	}` |
|      - | 1134 | `	/* Perform the requested operation */` |
|   1069 | 1135 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1136 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1069 | 1137 | `	MarkIOPrivateClosed(pDev);` |
|   1069 | 1138 | `	return PH7_OK;` |
|    537 | 1139 | ` }` |
|      - | 1140 | `/*` |
|      - | 1141 | ` * resource opendir(string $path[,resource $context])` |
|      - | 1142 | ` *  Open directory handle.` |
|      - | 1143 | ` * Parameters` |
|      - | 1144 | ` * $path` |
|      - | 1145 | ` *   The directory path that is to be opened.` |
|      - | 1146 | ` * $context` |
|      - | 1147 | ` *   A context stream resource.` |
|      - | 1148 | ` * Return` |
|      - | 1149 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 1150 | ` */` |
|   1064 | 1151 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1152 | `{` |
|      - | 1153 | `	const ph7_io_stream *pStream;` |
|      - | 1154 | `	const char *zPath;` |
|      - | 1155 | `	io_private *pDev;` |
|      - | 1156 | `	int iLen,rc;` |
|   1069 | 1157 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1158 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1159 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1160 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1161 | `		return PH7_OK;` |
|      - | 1162 | `	}` |
|      - | 1163 | `	/* Extract the target path */` |
|   1069 | 1164 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1165 | `	/* Try to extract a stream */` |
|   1069 | 1166 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1069 | 1167 | `	if( pStream == 0 ){` |
|    ! 0 | 1168 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 1169 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 1170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1171 | `		return PH7_OK;` |
|      - | 1172 | `	}` |
|   1069 | 1173 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1174 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1175 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1176 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1177 | `			);` |
|    ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1179 | `		return PH7_OK;` |
|      - | 1180 | `	}` |
|      - | 1181 | `	/* Allocate a new IO private instance */` |
|   1069 | 1182 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1069 | 1183 | `	if( pDev == 0 ){` |
|    ! 0 | 1184 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1186 | `		return PH7_OK;` |
|      - | 1187 | `	}` |
|      - | 1188 | `	/* Initialize the structure */` |
|   1069 | 1189 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1190 | `	/* Open the target directory */` |
|   1069 | 1191 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1069 | 1192 | `	if( rc != PH7_OK ){` |
|      - | 1193 | `		/* IO error,return FALSE */` |
|    ! 0 | 1194 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 1195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1196 | `	}else{` |
|      - | 1197 | `		/* Return the handle as a resource */` |
|   1069 | 1198 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1199 | `	}` |
|   1069 | 1200 | `	return PH7_OK;` |
|    537 | 1201 | `}` |
|      - | 1202 | `/*` |
|      - | 1203 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 1204 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 1205 | ` * Parameters` |
|      - | 1206 | ` *  $filename` |
|      - | 1207 | ` *   The filename being read.` |
|      - | 1208 | ` *  $use_include_path` |
|      - | 1209 | ` *   You can use the optional second parameter and set it to` |
|      - | 1210 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1211 | ` *  $context` |
|      - | 1212 | ` *   A context stream resource.` |
|      - | 1213 | ` * Return` |
|      - | 1214 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 1215 | ` */` |
|      - | 1216 | `/*` |
|      - | 1217 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 1218 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 1219 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 1220 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 1221 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 1222 | ` * trailing reason comes from.` |
|      - | 1223 | ` */` |
|      2 | 1224 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1225 | `{` |
|      3 | 1226 | `	int use_include  = FALSE;` |
|      - | 1227 | `	const ph7_io_stream *pStream;` |
|      - | 1228 | `	ph7_int64 n,nRead;` |
|      - | 1229 | `	const char *zFile;` |
|      - | 1230 | `	char zBuf[8192];` |
|      - | 1231 | `	void *pHandle;` |
|      - | 1232 | `	int rc,nLen;` |
|      3 | 1233 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1234 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1235 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1237 | `		return PH7_OK;` |
|      - | 1238 | `	}` |
|      - | 1239 | `	/* Extract the file path */` |
|      3 | 1240 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1241 | `	/* Point to the target IO stream device */` |
|      3 | 1242 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1243 | `	if( pStream == 0 ){` |
|    ! 0 | 1244 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1245 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1246 | `		return PH7_OK;` |
|      - | 1247 | `	}` |
|      3 | 1248 | `	if( nArg > 1 ){` |
|    ! 0 | 1249 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 1250 | `	}` |
|      - | 1251 | `	/* Try to open the file in read-only mode */` |
|      4 | 1252 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 1253 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1254 | `	if( pHandle == 0 ){` |
|    ! 0 | 1255 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1257 | `		return PH7_OK;` |
|      - | 1258 | `	}` |
|      - | 1259 | `	/* Perform the requested operation */` |
|      3 | 1260 | `	nRead = 0;` |
|      2 | 1261 | `	for(;;){` |
|      5 | 1262 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 1263 | `		if( n < 1 ){` |
|      - | 1264 | `			/* EOF or IO error,break immediately */` |
|      3 | 1265 | `			break;` |
|      - | 1266 | `		}` |
|      - | 1267 | `		/* Output data */` |
|      3 | 1268 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 1269 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 1270 | `			break;` |
|      - | 1271 | `		}` |
|      - | 1272 | `		/* Increment counter */` |
|      3 | 1273 | `		nRead += n;` |
|      1 | 1274 | `	}` |
|      - | 1275 | `	/* Close the stream */` |
|      3 | 1276 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1277 | `	/* Total number of bytes readen */` |
|      3 | 1278 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 1279 | `	return PH7_OK;` |
|      2 | 1280 | `}` |
|      - | 1281 | `/*` |
|      - | 1282 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 1283 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 1284 | ` *  Reads entire file into a string.` |
|      - | 1285 | ` * Parameters` |
|      - | 1286 | ` *  $filename` |
|      - | 1287 | ` *   The filename being read.` |
|      - | 1288 | ` *  $use_include_path` |
|      - | 1289 | ` *   You can use the optional second parameter and set it to` |
|      - | 1290 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1291 | ` *  $context` |
|      - | 1292 | ` *   A context stream resource.` |
|      - | 1293 | ` *  $offset` |
|      - | 1294 | ` *   The offset where the reading starts on the original stream.` |
|      - | 1295 | ` *  $maxlen` |
|      - | 1296 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 1297 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 1298 | ` * Return` |
|      - | 1299 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1300 | ` */` |
|   6594 | 1301 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1302 | `{` |
|      - | 1303 | `	const ph7_io_stream *pStream;` |
|      - | 1304 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6599 | 1305 | `	int use_include  = FALSE;` |
|      - | 1306 | `	const char *zFile;` |
|      - | 1307 | `	char zBuf[8192];` |
|      - | 1308 | `	void *pHandle;` |
|      - | 1309 | `	int nLen;` |
|      - | 1310 |  |
|   6599 | 1311 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1312 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1313 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1314 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1315 | `		return PH7_OK;` |
|      - | 1316 | `	}` |
|      - | 1317 | `	/* Extract the file path */` |
|   6599 | 1318 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1319 | `	/* Point to the target IO stream device */` |
|   6599 | 1320 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6599 | 1321 | `	if( pStream == 0 ){` |
|    ! 0 | 1322 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1324 | `		return PH7_OK;` |
|      - | 1325 | `	}` |
|   6599 | 1326 | `	nMaxlen = -1;` |
|   6599 | 1327 | `	if( nArg > 1 ){` |
|      5 | 1328 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 1329 | `	}` |
|      - | 1330 | `	/* Try to open the file in read-only mode */` |
|   6599 | 1331 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6599 | 1332 | `	if( pHandle == 0 ){` |
|      3 | 1333 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1334 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1335 | `		return PH7_OK;` |
|      - | 1336 | `	}` |
|   6597 | 1337 | `	if( nArg > 3 ){` |
|      - | 1338 | `		/* Extract the offset */` |
|      5 | 1339 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 1340 | `		if( n > 0 ){` |
|    ! 0 | 1341 | `			if( pStream->xSeek ){` |
|      - | 1342 | `				/* Seek to the desired offset */` |
|    ! 0 | 1343 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 1344 | `			}` |
|    ! 0 | 1345 | `		}` |
|      5 | 1346 | `		if( nArg > 4 ){` |
|      - | 1347 | `			/* Maximum data to read */` |
|      5 | 1348 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 1349 | `		}` |
|      2 | 1350 | `	}` |
|      - | 1351 | `	/* Perform the requested operation */` |
|   6597 | 1352 | `	nRead = 0;` |
|   6588 | 1353 | `	for(;;){` |
|  19772 | 1354 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6591 | 1355 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13181 | 1356 | `		if( n < 1 ){` |
|      - | 1357 | `			/* EOF or IO error,break immediately */` |
|   6595 | 1358 | `			break;` |
|      - | 1359 | `		}` |
|      - | 1360 | `		/* Append data */` |
|   6591 | 1361 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 1362 | `		/* Increment read counter */` |
|   6591 | 1363 | `		nRead += n;` |
|   6591 | 1364 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 1365 | `			/* Read limit reached */` |
|      3 | 1366 | `			break;` |
|      - | 1367 | `		}` |
|      5 | 1368 | `	}` |
|      - | 1369 | `	/* Close the stream */` |
|   6597 | 1370 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 1371 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 1372 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 1373 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   6597 | 1374 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      8 | 1375 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1376 | `	}` |
|   6597 | 1377 | `	return PH7_OK;` |
|   3302 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 1381 | ` *  Write a string to a file.` |
|      - | 1382 | ` * Parameters` |
|      - | 1383 | ` *  $filename` |
|      - | 1384 | ` *  Path to the file where to write the data.` |
|      - | 1385 | ` * $data` |
|      - | 1386 | ` *  The data to write(Must be a string).` |
|      - | 1387 | ` * $flags` |
|      - | 1388 | ` *  The value of flags can be any combination of the following` |
|      - | 1389 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 1390 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 1391 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 1392 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 1393 | ` * context` |
|      - | 1394 | ` *  A context stream resource.` |
|      - | 1395 | ` * Return` |
|      - | 1396 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 1397 | ` */` |
|  13062 | 1398 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1399 | `{` |
|  13067 | 1400 | `	int use_include  = FALSE;` |
|      - | 1401 | `	const ph7_io_stream *pStream;` |
|      - | 1402 | `	const char *zFile;` |
|      - | 1403 | `	const char *zData;` |
|      - | 1404 | `	int iOpenFlags;` |
|      - | 1405 | `	void *pHandle;` |
|      - | 1406 | `	int iFlags;` |
|      - | 1407 | `	int nLen;` |
|      - | 1408 |  |
|  13067 | 1409 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1410 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1411 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1413 | `		return PH7_OK;` |
|      - | 1414 | `	}` |
|      - | 1415 | `	/* Extract the file path */` |
|  13067 | 1416 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1417 | `	/* Point to the target IO stream device */` |
|  13067 | 1418 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13067 | 1419 | `	if( pStream == 0 ){` |
|    ! 0 | 1420 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1422 | `		return PH7_OK;` |
|      - | 1423 | `	}` |
|      - | 1424 | `	/* Data to write */` |
|  13067 | 1425 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1426 | `	/* Try to open the file in read-write mode */` |
|  13067 | 1427 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 1428 | `	/* Extract the flags */` |
|  13067 | 1429 | `	iFlags = 0;` |
|  13067 | 1430 | `	if( nArg > 2 ){` |
|    ! 0 | 1431 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 1432 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 1433 | `			use_include = TRUE;` |
|    ! 0 | 1434 | `		}` |
|    ! 0 | 1435 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 1436 | `			/* If the file already exists, append the data to the file` |
|      - | 1437 | `			 * instead of overwriting it.` |
|      - | 1438 | `			 */` |
|    ! 0 | 1439 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 1440 | `			/* Append mode */` |
|    ! 0 | 1441 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 1442 | `		}` |
|    ! 0 | 1443 | `	}` |
|  19598 | 1444 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6531 | 1445 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13067 | 1446 | `	if( pHandle == 0 ){` |
|    ! 0 | 1447 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1449 | `		return PH7_OK;` |
|      - | 1450 | `	}` |
|  13067 | 1451 | `	if( nLen < 1 ){` |
|      - | 1452 | `		/* Empty data, file is created/truncated */` |
|     10 | 1453 | `		ph7_result_int64(pCtx,0);` |
|     10 | 1454 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     10 | 1455 | `		return PH7_OK;` |
|      - | 1456 | `	}` |
|  13059 | 1457 | `	if( pStream->xWrite ){` |
|      - | 1458 | `		ph7_int64 n;` |
|  13059 | 1459 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 1460 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 1461 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 1462 | `		}` |
|      - | 1463 | `		/* Perform the write operation */` |
|  13059 | 1464 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13059 | 1465 | `		if( n < 0 ){` |
|      - | 1466 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 1467 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1468 | `				"%s(): Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 1469 | `				ph7_function_name(pCtx),(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 1470 | `			ph7_result_bool(pCtx,0);` |
|      1 | 1471 | `		}else{` |
|      - | 1472 | `			/* Total number of bytes written */` |
|  13059 | 1473 | `			ph7_result_int64(pCtx,n);` |
|      - | 1474 | `		}` |
|   6532 | 1475 | `	}else{` |
|      - | 1476 | `		/* Read-only stream */` |
|    ! 0 | 1477 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 1478 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 1479 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 1480 | `			);` |
|    ! 0 | 1481 | `		ph7_result_bool(pCtx,0);` |
|      - | 1482 | `	}` |
|      - | 1483 | `	/* Close the handle */` |
|  13059 | 1484 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13059 | 1485 | `	return PH7_OK;` |
|   6536 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 1489 | ` *  Reads entire file into an array.` |
|      - | 1490 | ` * Parameters` |
|      - | 1491 | ` *  $filename` |
|      - | 1492 | ` *   The filename being read.` |
|      - | 1493 | ` *  $flags` |
|      - | 1494 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 1495 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 1496 | ` *       Search for the file in the include_path.` |
|      - | 1497 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 1498 | ` *       Do not add newline at the end of each array element` |
|      - | 1499 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 1500 | ` *       Skip empty lines` |
|      - | 1501 | ` *  $context` |
|      - | 1502 | ` *   A context stream resource.` |
|      - | 1503 | ` * Return` |
|      - | 1504 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 1505 | ` */` |
|     10 | 1506 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1507 | `{` |
|      - | 1508 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 1509 | `	ph7_value *pArray,*pLine;` |
|      - | 1510 | `	const ph7_io_stream *pStream;` |
|     12 | 1511 | `	int use_include = 0;` |
|      - | 1512 | `	io_private *pDev;` |
|      - | 1513 | `	ph7_int64 n;` |
|      - | 1514 | `	int iFlags;` |
|      - | 1515 | `	int nLen;` |
|      - | 1516 |  |
|     12 | 1517 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1518 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1519 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1520 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1521 | `		return PH7_OK;` |
|      - | 1522 | `	}` |
|      - | 1523 | `	/* Extract the file path */` |
|     12 | 1524 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1525 | `	/* Point to the target IO stream device */` |
|     12 | 1526 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     12 | 1527 | `	if( pStream == 0 ){` |
|    ! 0 | 1528 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1529 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1530 | `		return PH7_OK;` |
|      - | 1531 | `	}` |
|      - | 1532 | `	/* Allocate a new IO private instance */` |
|     12 | 1533 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     12 | 1534 | `	if( pDev == 0 ){` |
|    ! 0 | 1535 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1537 | `		return PH7_OK;` |
|      - | 1538 | `	}` |
|      - | 1539 | `	/* Initialize the structure */` |
|     12 | 1540 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     12 | 1541 | `	iFlags = 0;` |
|     12 | 1542 | `	if( nArg > 1 ){` |
|    ! 0 | 1543 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 1544 | `	}` |
|     12 | 1545 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 1546 | `		use_include = TRUE;` |
|    ! 0 | 1547 | `	}` |
|      - | 1548 | `	/* Create the array and the working value */` |
|     12 | 1549 | `	pArray = ph7_context_new_array(pCtx);` |
|     12 | 1550 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     12 | 1551 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 1552 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1554 | `		return PH7_OK;` |
|      - | 1555 | `	}` |
|      - | 1556 | `	/* Try to open the file in read-only mode */` |
|     12 | 1557 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     12 | 1558 | `	if( pDev->pHandle == 0 ){` |
|      9 | 1559 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      9 | 1560 | `		ph7_result_bool(pCtx,0);` |
|      - | 1561 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 1562 | `		 * as soon we return from this function.` |
|      - | 1563 | `		 */` |
|      9 | 1564 | `		return PH7_OK;` |
|      - | 1565 | `	}` |
|      - | 1566 | `	/* Perform the requested operation */` |
|      3 | 1567 | `	for(;;){` |
|      - | 1568 | `		/* Try to extract a line */` |
|      7 | 1569 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 1570 | `		if( n < 1 ){` |
|      - | 1571 | `			/* EOF or IO error */` |
|      3 | 1572 | `			break;` |
|      - | 1573 | `		}` |
|      - | 1574 | `		/* Reset the cursor */` |
|      5 | 1575 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 1576 | `		/* Remove line ending if requested by the caller */` |
|      5 | 1577 | `		zPtr = zBuf;` |
|      5 | 1578 | `		zEnd = &zBuf[n];` |
|      5 | 1579 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 1580 | `			/* Ignore trailig lines */` |
|    ! 0 | 1581 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 1582 | `#ifdef __WINNT__` |
|      - | 1583 | `				\|\| zEnd[-1] == '\r'` |
|      - | 1584 | `#endif` |
|      - | 1585 | `				)){` |
|    ! 0 | 1586 | `					n--;` |
|    ! 0 | 1587 | `					zEnd--;` |
|    ! 0 | 1588 | `			}` |
|    ! 0 | 1589 | `		}` |
|      5 | 1590 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 1591 | `			/* Ignore empty lines */` |
|    ! 0 | 1592 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 1593 | `				zPtr++;` |
|    ! 0 | 1594 | `			}` |
|    ! 0 | 1595 | `			if( zPtr >= zEnd ){` |
|      - | 1596 | `				/* Empty line */` |
|    ! 0 | 1597 | `				continue;` |
|      - | 1598 | `			}` |
|    ! 0 | 1599 | `		}` |
|      5 | 1600 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 1601 | `		/* Insert line */` |
|      5 | 1602 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 1603 | `	}` |
|      - | 1604 | `	/* Close the stream */` |
|      3 | 1605 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 1606 | `	/* Release the io_private instance */` |
|      3 | 1607 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 1608 | `	/* Return the created array */` |
|      3 | 1609 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 1610 | `	return PH7_OK;` |
|      7 | 1611 | `}` |
|      - | 1612 | `/*` |
|      - | 1613 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 1614 | ` *  Makes a copy of the file source to dest.` |
|      - | 1615 | ` * Parameters` |
|      - | 1616 | ` *  $source` |
|      - | 1617 | ` *   Path to the source file.` |
|      - | 1618 | ` *  $dest` |
|      - | 1619 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 1620 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 1621 | ` *  $context` |
|      - | 1622 | ` *   A context stream resource.` |
|      - | 1623 | ` * Return` |
|      - | 1624 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1625 | ` */` |
|      4 | 1626 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1627 | `{` |
|      - | 1628 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 1629 | `	const char *zFile;` |
|      - | 1630 | `	char zBuf[8192];` |
|      - | 1631 | `	void *pIn,*pOut;` |
|      - | 1632 | `	ph7_int64 n;` |
|      - | 1633 | `	int nLen;` |
|      6 | 1634 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 1635 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1636 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 1637 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1638 | `		return PH7_OK;` |
|      - | 1639 | `	}` |
|      - | 1640 | `	/* Extract the source name */` |
|      6 | 1641 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1642 | `	/* Point to the target IO stream device */` |
|      6 | 1643 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 1644 | `	if( pSin == 0 ){` |
|    ! 0 | 1645 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1646 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1647 | `		return PH7_OK;` |
|      - | 1648 | `	}` |
|      - | 1649 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 1650 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 1651 | `	if( pIn == 0 ){` |
|      3 | 1652 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1653 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1654 | `		return PH7_OK;` |
|      - | 1655 | `	}` |
|      - | 1656 | `	/* Extract the destination name */` |
|      3 | 1657 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 1658 | `	/* Point to the target IO stream device */` |
|      3 | 1659 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 1660 | `	if( pSout == 0 ){` |
|    ! 0 | 1661 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 1662 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1663 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1664 | `		return PH7_OK;` |
|      - | 1665 | `	}` |
|      3 | 1666 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 1667 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1668 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1669 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 1670 | `			);` |
|    ! 0 | 1671 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1672 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1673 | `		return PH7_OK;` |
|      - | 1674 | `	}` |
|      - | 1675 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 1676 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 1677 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 1678 | `	if( pOut == 0 ){` |
|    ! 0 | 1679 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 1680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1681 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 1682 | `		return PH7_OK;` |
|      - | 1683 | `	}` |
|      - | 1684 | `	/* Perform the requested operation */` |
|      2 | 1685 | `	for(;;){` |
|      - | 1686 | `		/* Read from source */` |
|      5 | 1687 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 1688 | `		if( n < 1 ){` |
|      - | 1689 | `			/* EOF or IO error,break immediately */` |
|      3 | 1690 | `			break;` |
|      - | 1691 | `		}` |
|      - | 1692 | `		/* Write to dest */` |
|      3 | 1693 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 1694 | `		if( n < 1 ){` |
|      - | 1695 | `			/* IO error,break immediately */` |
|    ! 0 | 1696 | `			break;` |
|      - | 1697 | `		}` |
|      1 | 1698 | `	}` |
|      - | 1699 | `	/* Close the streams */` |
|      3 | 1700 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 1701 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 1702 | `	/* Return TRUE */` |
|      3 | 1703 | `	ph7_result_bool(pCtx,1);` |
|      3 | 1704 | `	return PH7_OK;` |
|      4 | 1705 | `}` |
|      - | 1706 | `/*` |
|      - | 1707 | ` * array fstat(resource $handle)` |
|      - | 1708 | ` *  Gets information about a file using an open file pointer.` |
|      - | 1709 | ` * Parameters` |
|      - | 1710 | ` *  $handle` |
|      - | 1711 | ` *   The file pointer.` |
|      - | 1712 | ` * Return` |
|      - | 1713 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 1714 | ` */` |
|      2 | 1715 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1716 | `{` |
|      - | 1717 | `	ph7_value *pArray,*pValue;` |
|      - | 1718 | `	const ph7_io_stream *pStream;` |
|      - | 1719 | `	io_private *pDev;` |
|      3 | 1720 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1721 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1722 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1723 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1724 | `		return PH7_OK;` |
|      - | 1725 | `	}` |
|      - | 1726 | `	/* Extract our private data */` |
|      3 | 1727 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1728 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1729 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1730 | `		/* Expecting an IO handle */` |
|    ! 0 | 1731 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1733 | `		return PH7_OK;` |
|      - | 1734 | `	}` |
|      - | 1735 | `	/* Point to the target IO stream device */` |
|      3 | 1736 | `	pStream = pDev->pStream;` |
|      3 | 1737 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 1738 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1739 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1740 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1741 | `			);` |
|    ! 0 | 1742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1743 | `		return PH7_OK;` |
|      - | 1744 | `	}` |
|      - | 1745 | `	/* Create the array and the working value */` |
|      3 | 1746 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1747 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1748 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1749 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1751 | `		return PH7_OK;` |
|      - | 1752 | `	}` |
|      - | 1753 | `	/* Perform the requested operation */` |
|      3 | 1754 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 1755 | `	/* Return the freshly created array */` |
|      3 | 1756 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1757 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 1758 | `	 * released automatically as soon we return from this function.` |
|      - | 1759 | `	 */` |
|      3 | 1760 | `	return PH7_OK;` |
|      2 | 1761 | `}` |
|      - | 1762 | `/*` |
|      - | 1763 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 1764 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 1765 | ` * Parameters` |
|      - | 1766 | ` *  $handle` |
|      - | 1767 | ` *   The file pointer.` |
|      - | 1768 | ` *  $string` |
|      - | 1769 | ` *   The string that is to be written.` |
|      - | 1770 | ` *  $length` |
|      - | 1771 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 1772 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 1773 | ` * Return` |
|      - | 1774 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 1775 | ` */` |
|     44 | 1776 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1777 | `{` |
|      - | 1778 | `	const ph7_io_stream *pStream;` |
|      - | 1779 | `	const char *zString;` |
|      - | 1780 | `	io_private *pDev;` |
|      - | 1781 | `	int nLen,n;` |
|     46 | 1782 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1783 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1784 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1785 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1786 | `		return PH7_OK;` |
|      - | 1787 | `	}` |
|      - | 1788 | `	/* Extract our private data */` |
|     46 | 1789 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1790 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     46 | 1791 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1792 | `		/* Expecting an IO handle */` |
|    ! 0 | 1793 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1794 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1795 | `		return PH7_OK;` |
|      - | 1796 | `	}` |
|      - | 1797 | `	/* Point to the target IO stream device */` |
|     46 | 1798 | `	pStream = pDev->pStream;` |
|     46 | 1799 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 1800 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1801 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1802 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1803 | `			);` |
|    ! 0 | 1804 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1805 | `		return PH7_OK;` |
|      - | 1806 | `	}` |
|      - | 1807 | `	/* Extract the data to write */` |
|     46 | 1808 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     46 | 1809 | `	if( nArg > 2 ){` |
|      - | 1810 | `		/* Maximum data length to write */` |
|    ! 0 | 1811 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 1812 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 1813 | `			nLen = n;` |
|    ! 0 | 1814 | `		}` |
|    ! 0 | 1815 | `	}` |
|     46 | 1816 | `	if( nLen < 1 ){` |
|      - | 1817 | `		/* Nothing to write */` |
|    ! 0 | 1818 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1819 | `		return PH7_OK;` |
|      - | 1820 | `	}` |
|      - | 1821 | `	/* Perform the requested operation */` |
|     46 | 1822 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     46 | 1823 | `	if( n <  0 ){` |
|      - | 1824 | `		/* IO error,return FALSE */` |
|    ! 0 | 1825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1826 | `	}else{` |
|      - | 1827 | `		/* #Bytes written */` |
|     46 | 1828 | `		ph7_result_int(pCtx,n);` |
|      - | 1829 | `	}` |
|     46 | 1830 | `	return PH7_OK;` |
|     24 | 1831 | `}` |
|      - | 1832 | `/*` |
|      - | 1833 | ` * bool flock(resource $handle,int $operation)` |
|      - | 1834 | ` *  Portable advisory file locking.` |
|      - | 1835 | ` * Parameters` |
|      - | 1836 | ` *  $handle` |
|      - | 1837 | ` *   The file pointer.` |
|      - | 1838 | ` *  $operation` |
|      - | 1839 | ` *   operation is one of the following:` |
|      - | 1840 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 1841 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 1842 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 1843 | ` * Return` |
|      - | 1844 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 1845 | ` */` |
|      4 | 1846 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1847 | `{` |
|      - | 1848 | `	const ph7_io_stream *pStream;` |
|      - | 1849 | `	io_private *pDev;` |
|      - | 1850 | `	int nLock;` |
|      - | 1851 | `	int rc;` |
|      5 | 1852 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1853 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1854 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1855 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1856 | `		return PH7_OK;` |
|      - | 1857 | `	}` |
|      - | 1858 | `	/* Extract our private data */` |
|      5 | 1859 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1860 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 1861 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1862 | `		/*Expecting an IO handle */` |
|    ! 0 | 1863 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1865 | `		return PH7_OK;` |
|      - | 1866 | `	}` |
|      - | 1867 | `	/* Point to the target IO stream device */` |
|      5 | 1868 | `	pStream = pDev->pStream;` |
|      5 | 1869 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 1870 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1871 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1872 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1873 | `			);` |
|    ! 0 | 1874 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1875 | `		return PH7_OK;` |
|      - | 1876 | `	}` |
|      - | 1877 | `	/* Requested lock operation */` |
|      5 | 1878 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 1879 | `	/*` |
|      - | 1880 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 1881 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 1882 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 1883 | `	 */` |
|      - | 1884 | `	{` |
|      5 | 1885 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      5 | 1886 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      3 | 1887 | `			nLock = -1;` |
|      4 | 1888 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      3 | 1889 | `			nLock = 1;` |
|      2 | 1890 | `		}else{` |
|    ! 0 | 1891 | `			nLock = 0; /* LOCK_SH */` |
|      - | 1892 | `		}` |
|      - | 1893 | `	}` |
|      - | 1894 | `	/* Lock operation */` |
|      5 | 1895 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 1896 | `	/* IO result */` |
|      5 | 1897 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1898 | `	return PH7_OK;` |
|      3 | 1899 | `}` |
|      - | 1900 | `/*` |
|      - | 1901 | ` * int fpassthru(resource $handle)` |
|      - | 1902 | ` *  Output all remaining data on a file pointer.` |
|      - | 1903 | ` * Parameters` |
|      - | 1904 | ` *  $handle` |
|      - | 1905 | ` *   The file pointer.` |
|      - | 1906 | ` * Return` |
|      - | 1907 | ` *  Total number of characters read from handle and passed through` |
|      - | 1908 | ` *  to the output on success or FALSE on failure.` |
|      - | 1909 | ` */` |
|      2 | 1910 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1911 | `{` |
|      - | 1912 | `	const ph7_io_stream *pStream;` |
|      - | 1913 | `	io_private *pDev;` |
|      - | 1914 | `	ph7_int64 n,nRead;` |
|      - | 1915 | `	char zBuf[8192];` |
|      - | 1916 | `	int rc;` |
|      3 | 1917 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1918 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1919 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1921 | `		return PH7_OK;` |
|      - | 1922 | `	}` |
|      - | 1923 | `	/* Extract our private data */` |
|      3 | 1924 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1925 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1926 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1927 | `		/*Expecting an IO handle */` |
|    ! 0 | 1928 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1930 | `		return PH7_OK;` |
|      - | 1931 | `	}` |
|      - | 1932 | `	/* Point to the target IO stream device */` |
|      3 | 1933 | `	pStream = pDev->pStream;` |
|      3 | 1934 | `	if( pStream == 0  ){` |
|    ! 0 | 1935 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1936 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1937 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1938 | `			);` |
|    ! 0 | 1939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1940 | `		return PH7_OK;` |
|      - | 1941 | `	}` |
|      - | 1942 | `	/* Perform the requested operation */` |
|      3 | 1943 | `	nRead = 0;` |
|      2 | 1944 | `	for(;;){` |
|      5 | 1945 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 1946 | `		if( n < 1 ){` |
|      - | 1947 | `			/* Error or EOF */` |
|      3 | 1948 | `			break;` |
|      - | 1949 | `		}` |
|      - | 1950 | `		/* Increment the read counter */` |
|      3 | 1951 | `		nRead += n;` |
|      - | 1952 | `		/* Output data */` |
|      3 | 1953 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 1954 | `		if( rc == PH7_ABORT ){` |
|      - | 1955 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 1956 | `			break;` |
|      - | 1957 | `		}` |
|      1 | 1958 | `	}` |
|      - | 1959 | `	/* Total number of bytes readen */` |
|      3 | 1960 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 1961 | `	return PH7_OK;` |
|      2 | 1962 | `}` |
|      - | 1963 | `/* CSV reader/writer private data */` |
|      - | 1964 | `struct csv_data` |
|      - | 1965 | `{` |
|      - | 1966 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 1967 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 1968 | `	io_private *pDev; /* Open stream handle */` |
|      - | 1969 | `	int iCount;       /* Counter */` |
|      - | 1970 | `};` |
|      - | 1971 | `/*` |
|      - | 1972 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 1973 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 1974 | ` * associated data.` |
|      - | 1975 | ` */` |
|      6 | 1976 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 1977 | `{` |
|      7 | 1978 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 1979 | `	const char *zData;` |
|      - | 1980 | `	int nLen,c2;` |
|      - | 1981 | `	sxu32 n;` |
|      - | 1982 | `	/* Point to the raw data */` |
|      7 | 1983 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 1984 | `	if( nLen < 1 ){` |
|      - | 1985 | `		/* Nothing to write */` |
|    ! 0 | 1986 | `		return PH7_OK;` |
|      - | 1987 | `	}` |
|      7 | 1988 | `	if( pData->iCount > 0 ){` |
|      - | 1989 | `		/* Write the delimiter */` |
|      5 | 1990 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 1991 | `	}` |
|      7 | 1992 | `	n = 1;` |
|      7 | 1993 | `	c2 = 0;` |
|     10 | 1994 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 1995 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 1996 | `			c2 = 1;` |
|    ! 0 | 1997 | `			if( n == 0 ){` |
|    ! 0 | 1998 | `				c2 = 2;` |
|    ! 0 | 1999 | `			}` |
|      - | 2000 | `			/* Write the enclosure */` |
|    ! 0 | 2001 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2002 | `			if( c2 > 1 ){` |
|    ! 0 | 2003 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2004 | `			}` |
|    ! 0 | 2005 | `	}` |
|      - | 2006 | `	/* Write the data */` |
|      7 | 2007 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 2008 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 2009 | `		return PH7_ABORT;` |
|      - | 2010 | `	}` |
|      7 | 2011 | `	if( c2 > 0 ){` |
|      - | 2012 | `		/* Write the enclosure */` |
|    ! 0 | 2013 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2014 | `		if( c2 > 1 ){` |
|    ! 0 | 2015 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 2016 | `		}` |
|    ! 0 | 2017 | `	}` |
|      7 | 2018 | `	pData->iCount++;` |
|      7 | 2019 | `	return PH7_OK;` |
|      4 | 2020 | `}` |
|      - | 2021 | `/*` |
|      - | 2022 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 2023 | ` *  Format line as CSV and write to file pointer.` |
|      - | 2024 | ` * Parameters` |
|      - | 2025 | ` *  $handle` |
|      - | 2026 | ` *   Open file handle.` |
|      - | 2027 | ` * $fields` |
|      - | 2028 | ` *   An array of values.` |
|      - | 2029 | ` * $delimiter` |
|      - | 2030 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 2031 | ` * $enclosure` |
|      - | 2032 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 2033 | ` */` |
|      2 | 2034 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2035 | `{` |
|      - | 2036 | `	const ph7_io_stream *pStream;` |
|      - | 2037 | `	struct csv_data sCsv;` |
|      - | 2038 | `	io_private *pDev;` |
|      - | 2039 | `	char *zEol;` |
|      - | 2040 | `	int eolen;` |
|      3 | 2041 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 2042 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2043 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 2044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Extract our private data */` |
|      3 | 2048 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2049 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2050 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2051 | `		/*Expecting an IO handle */` |
|    ! 0 | 2052 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2054 | `		return PH7_OK;` |
|      - | 2055 | `	}` |
|      - | 2056 | `	/* Point to the target IO stream device */` |
|      3 | 2057 | `	pStream = pDev->pStream;` |
|      3 | 2058 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2059 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2060 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2061 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2062 | `			);` |
|    ! 0 | 2063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2064 | `		return PH7_OK;` |
|      - | 2065 | `	}` |
|      - | 2066 | `	/* Set default csv separator */` |
|      3 | 2067 | `	sCsv.delimiter = ',';` |
|      3 | 2068 | `	sCsv.enclosure = '"';` |
|      3 | 2069 | `	sCsv.pDev = pDev;` |
|      3 | 2070 | `	sCsv.iCount = 0;` |
|      3 | 2071 | `	if( nArg > 2 ){` |
|      - | 2072 | `		/* User delimiter */` |
|      - | 2073 | `		const char *z;` |
|      - | 2074 | `		int n;` |
|      3 | 2075 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 2076 | `		if( n > 0 ){` |
|      3 | 2077 | `			sCsv.delimiter = z[0];` |
|      1 | 2078 | `		}` |
|      3 | 2079 | `		if( nArg > 3 ){` |
|      3 | 2080 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 2081 | `			if( n > 0 ){` |
|      3 | 2082 | `				sCsv.enclosure = z[0];` |
|      1 | 2083 | `			}` |
|      1 | 2084 | `		}` |
|      1 | 2085 | `	}` |
|      - | 2086 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 2087 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 2088 | `	/* Write a line ending */` |
|      - | 2089 | `#ifdef __WINNT__` |
|      1 | 2090 | `	zEol = "\r\n";` |
|      1 | 2091 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 2092 | `#else` |
|      - | 2093 | `	/* Assume UNIX LF */` |
|      2 | 2094 | `	zEol = "\n";` |
|      2 | 2095 | `	eolen = (int)sizeof(char);` |
|      - | 2096 | `#endif` |
|      3 | 2097 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 2098 | `	return PH7_OK;` |
|      2 | 2099 | `}` |
|      - | 2100 | `/*` |
|      - | 2101 | ` * fprintf,vfprintf private data.` |
|      - | 2102 | ` * An instance of the following structure is passed to the formatted` |
|      - | 2103 | ` * input consumer callback defined below.` |
|      - | 2104 | ` */` |
|      - | 2105 | `typedef struct fprintf_data fprintf_data;` |
|      - | 2106 | `struct fprintf_data` |
|      - | 2107 | `{` |
|      - | 2108 | `	io_private *pIO;        /* IO stream */` |
|      - | 2109 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 2110 | `};` |
|      - | 2111 | `/*` |
|      - | 2112 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 2113 | ` */` |
|     28 | 2114 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 2115 | `{` |
|     29 | 2116 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 2117 | `	ph7_int64 n;` |
|      - | 2118 | `	/* Write the formatted data */` |
|     29 | 2119 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     29 | 2120 | `	if( n < 1 ){` |
|    ! 0 | 2121 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 2122 | `		/* IO error,abort immediately */` |
|    ! 0 | 2123 | `		return SXERR_ABORT;` |
|      - | 2124 | `	}` |
|      - | 2125 | `	/* Increment counter */` |
|     29 | 2126 | `	pFdata->nCount += n;` |
|     29 | 2127 | `	return PH7_OK;` |
|     15 | 2128 | `}` |
|      - | 2129 | `/*` |
|      - | 2130 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 2131 | ` *  Write a formatted string to a stream.` |
|      - | 2132 | ` * Parameters` |
|      - | 2133 | ` *  $handle` |
|      - | 2134 | ` *   The file pointer.` |
|      - | 2135 | ` *  $format` |
|      - | 2136 | ` *   String format (see sprintf()).` |
|      - | 2137 | ` * Return` |
|      - | 2138 | ` *  The length of the written string.` |
|      - | 2139 | ` */` |
|     18 | 2140 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2141 | `{` |
|      - | 2142 | `	fprintf_data sFdata;` |
|      - | 2143 | `	const char *zFormat;` |
|      - | 2144 | `	io_private *pDev;` |
|      - | 2145 | `	int nLen;` |
|     19 | 2146 | `	if( nArg < 2 ){` |
|    ! 0 | 2147 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2148 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2149 | `		return PH7_OK;` |
|      - | 2150 | `	}` |
|      - | 2151 | `	{` |
|      - | 2152 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 2153 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 2154 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 2155 | `			return rcs;` |
|      - | 2156 | `		}` |
|      - | 2157 | `	}` |
|      - | 2158 | `	/* Extract our private data */` |
|     19 | 2159 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2160 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 2161 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2162 | `		/*Expecting an IO handle */` |
|    ! 0 | 2163 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2164 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2165 | `		return PH7_OK;` |
|      - | 2166 | `	}` |
|      - | 2167 | `	/* Point to the target IO stream device */` |
|     19 | 2168 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2169 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2170 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2171 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2172 | `			);` |
|    ! 0 | 2173 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|      - | 2176 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 2177 | `	{` |
|     19 | 2178 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 2179 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2180 | `			return rcf;` |
|      - | 2181 | `		}` |
|      - | 2182 | `	}` |
|      - | 2183 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 2184 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 2185 | `	if( nLen < 1 ){` |
|      - | 2186 | `		/* Empty string,return zero */` |
|    ! 0 | 2187 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2188 | `		return PH7_OK;` |
|      - | 2189 | `	}` |
|      - | 2190 | `	{` |
|      - | 2191 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 2192 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 2193 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     19 | 2194 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     19 | 2195 | `		if( rcv != PH7_OK ){` |
|      3 | 2196 | `			return rcv;` |
|      - | 2197 | `		}` |
|      - | 2198 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 2199 | `		 * before any output; propagate the throw status verbatim. */` |
|     17 | 2200 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 2201 | `		if( rcv != PH7_OK ){` |
|      5 | 2202 | `			return rcv;` |
|      - | 2203 | `		}` |
|      - | 2204 | `	}` |
|      - | 2205 | `	/* Prepare our private data */` |
|     13 | 2206 | `	sFdata.nCount = 0;` |
|     13 | 2207 | `	sFdata.pIO = pDev;` |
|      - | 2208 | `	/* Format the string */` |
|     13 | 2209 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 2210 | `	/* Return total number of bytes written */` |
|     13 | 2211 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     13 | 2212 | `	return PH7_OK;` |
|     10 | 2213 | `}` |
|      - | 2214 | `/*` |
|      - | 2215 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 2216 | ` *  Write a formatted string to a stream.` |
|      - | 2217 | ` * Parameters` |
|      - | 2218 | ` *  $handle` |
|      - | 2219 | ` *   The file pointer.` |
|      - | 2220 | ` *  $format` |
|      - | 2221 | ` *   String format (see sprintf()).` |
|      - | 2222 | ` * $args` |
|      - | 2223 | ` *   User arguments.` |
|      - | 2224 | ` * Return` |
|      - | 2225 | ` *  The length of the written string.` |
|      - | 2226 | ` */` |
|      6 | 2227 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2228 | `{` |
|      - | 2229 | `	fprintf_data sFdata;` |
|      - | 2230 | `	const char *zFormat;` |
|      - | 2231 | `	ph7_hashmap *pMap;` |
|      - | 2232 | `	io_private *pDev;` |
|      - | 2233 | `	SySet sArg;` |
|      - | 2234 | `	int n,nLen;` |
|      7 | 2235 | `	if( nArg < 3 ){` |
|    ! 0 | 2236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 2237 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2238 | `		return PH7_OK;` |
|      - | 2239 | `	}` |
|      - | 2240 | `	{` |
|      - | 2241 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 2242 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 2243 | `		if( rcs != PH7_OK ){` |
|      3 | 2244 | `			return rcs;` |
|      - | 2245 | `		}` |
|      - | 2246 | `	}` |
|      - | 2247 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 2248 | `	{` |
|      5 | 2249 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 2250 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 2251 | `			return rcf;` |
|      - | 2252 | `		}` |
|      - | 2253 | `	}` |
|      5 | 2254 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 2255 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 2256 | `		char zBuf[64];` |
|    ! 0 | 2257 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2258 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 2259 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 2260 | `	}` |
|      - | 2261 | `	/* Extract our private data */` |
|      5 | 2262 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2263 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2264 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2265 | `		/*Expecting an IO handle */` |
|    ! 0 | 2266 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2267 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2268 | `		return PH7_OK;` |
|      - | 2269 | `	}` |
|      - | 2270 | `	/* Point to the target IO stream device */` |
|      5 | 2271 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 2272 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2273 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2274 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 2275 | `			);` |
|    ! 0 | 2276 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2277 | `		return PH7_OK;` |
|      - | 2278 | `	}` |
|      - | 2279 | `	/* Extract the string format */` |
|      5 | 2280 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 2281 | `	if( nLen < 1 ){` |
|      - | 2282 | `		/* Empty string,return zero */` |
|    ! 0 | 2283 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2284 | `		return PH7_OK;` |
|      - | 2285 | `	}` |
|      - | 2286 | `	/* Point to hashmap */` |
|      5 | 2287 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 2288 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 2289 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 2290 | `	{` |
|      5 | 2291 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 2292 | `		if( rcc != PH7_OK ){` |
|      3 | 2293 | `			return rcc;` |
|      - | 2294 | `		}` |
|      - | 2295 | `	}` |
|      - | 2296 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 2297 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 2298 | `	{` |
|      3 | 2299 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 2300 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 2301 | `			return rcv;` |
|      - | 2302 | `		}` |
|      - | 2303 | `	}` |
|      - | 2304 | `	/* Extract arguments from the hashmap */` |
|      3 | 2305 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 2306 | `	/* Prepare our private data */` |
|      3 | 2307 | `	sFdata.nCount = 0;` |
|      3 | 2308 | `	sFdata.pIO = pDev;` |
|      - | 2309 | `	/* Format the string */` |
|      3 | 2310 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 2311 | `	/* Return total number of bytes written*/` |
|      3 | 2312 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 2313 | `	SySetRelease(&sArg);` |
|      3 | 2314 | `	return PH7_OK;` |
|      4 | 2315 | `}` |
|      - | 2316 | `/*` |
|      - | 2317 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 2318 | ` * According to the PHP reference manual:` |
|      - | 2319 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 2320 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 2321 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 2322 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 2323 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 2324 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 2325 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 2326 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 2327 | ` *         exist, attempt to create it.` |
|      - | 2328 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 2329 | ` *          not exist, attempt to create it.` |
|      - | 2330 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 2331 | ` *         already exists,` |
|      - | 2332 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 2333 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 2334 | ` *         the underlying open(2) system call.` |
|      - | 2335 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 2336 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 2337 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 2338 | ` *          is positioned on the beginning of the file.` |
|      - | 2339 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 2340 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 2341 | ` *          be used after the lock is requested).` |
|      - | 2342 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 2343 | ` */` |
|    212 | 2344 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 2345 | `{` |
|    217 | 2346 | `	const char *zEnd = &zMode[nLen];` |
|    217 | 2347 | `	int iFlag = 0;` |
|      - | 2348 | `	int c;` |
|    217 | 2349 | `	if( nLen < 1 ){` |
|      - | 2350 | `		/* Open in a read-only mode */` |
|    ! 0 | 2351 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 2352 | `	}` |
|    217 | 2353 | `	c = zMode[0];` |
|    217 | 2354 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 2355 | `		/* Read-only access */` |
|     58 | 2356 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     58 | 2357 | `		zMode++; /* Advance */` |
|     58 | 2358 | `		if( zMode < zEnd ){` |
|     13 | 2359 | `			c = zMode[0];` |
|     13 | 2360 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 2361 | `				/* Read+Write access */` |
|     13 | 2362 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      6 | 2363 | `			}` |
|     10 | 2364 | `		}` |
|    188 | 2365 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 2366 | `		/* Overwrite mode.` |
|      - | 2367 | `		 * If the file does not exists,try to create it` |
|      - | 2368 | `		 */` |
|     34 | 2369 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     34 | 2370 | `		zMode++; /* Advance */` |
|     34 | 2371 | `		if( zMode < zEnd ){` |
|      5 | 2372 | `			c = zMode[0];` |
|      5 | 2373 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2374 | `				/* Read+Write access */` |
|      5 | 2375 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 2376 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 2377 | `			}` |
|      4 | 2378 | `		}` |
|    144 | 2379 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 2380 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 2381 | `		 * Create the file if it does not exists.` |
|      - | 2382 | `		 */` |
|    ! 0 | 2383 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2384 | `		zMode++; /* Advance */` |
|    ! 0 | 2385 | `		if( zMode < zEnd ){` |
|    ! 0 | 2386 | `			c = zMode[0];` |
|    ! 0 | 2387 | `			if( c == '+' ){` |
|      - | 2388 | `				/* Read-Write access */` |
|    ! 0 | 2389 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2390 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2391 | `			}` |
|    ! 0 | 2392 | `		}` |
|    128 | 2393 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 2394 | `		/* Exclusive access.` |
|      - | 2395 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 2396 | `		 * Otherwise create a new file.` |
|      - | 2397 | `		 */` |
|    128 | 2398 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    128 | 2399 | `		zMode++; /* Advance */` |
|    128 | 2400 | `		if( zMode < zEnd ){` |
|    ! 0 | 2401 | `			c = zMode[0];` |
|    ! 0 | 2402 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 2403 | `				/* Read-Write access */` |
|    ! 0 | 2404 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2405 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2406 | `			}` |
|      2 | 2407 | `		}` |
|     63 | 2408 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 2409 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 2410 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 2411 | `		zMode++; /* Advance */` |
|    ! 0 | 2412 | `		if( zMode < zEnd ){` |
|    ! 0 | 2413 | `			c = zMode[0];` |
|    ! 0 | 2414 | `			if( c == '+' ){` |
|      - | 2415 | `				/* Read-Write access */` |
|    ! 0 | 2416 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 2417 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 2418 | `			}` |
|    ! 0 | 2419 | `		}` |
|    ! 0 | 2420 | `	}else{` |
|      - | 2421 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 2422 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 2423 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 2424 | `	}` |
|    233 | 2425 | `	while( zMode < zEnd ){` |
|     17 | 2426 | `		c = zMode[0];` |
|     17 | 2427 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 2428 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2429 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     17 | 2430 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 2431 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 2432 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 2433 | `		}` |
|     17 | 2434 | `		zMode++;` |
|      1 | 2435 | `	}` |
|    217 | 2436 | `	return iFlag;` |
|    111 | 2437 | `}` |
|      - | 2438 | `/*` |
|      - | 2439 | ` * Initialize the IO private structure.` |
|      - | 2440 | ` */` |
|   5306 | 2441 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 2442 | `{` |
|   5311 | 2443 | `	pOut->pStream = pStream;` |
|   5311 | 2444 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5311 | 2445 | `	pOut->nOfft = 0;` |
|      - | 2446 | `	/* Set the magic number */` |
|   5311 | 2447 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5311 | 2448 | `}` |
|      - | 2449 | `/*` |
|      - | 2450 | ` * Release the IO private structure.` |
|      - | 2451 | ` */` |
|      2 | 2452 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 2453 | `{` |
|      3 | 2454 | `	SyBlobRelease(&pDev->sBuffer);` |
|      3 | 2455 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 2456 | `	/* Release the whole structure */` |
|      3 | 2457 | `	ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 2458 | `}` |
|      - | 2459 | `/*` |
|      - | 2460 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 2461 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 2462 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 2463 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 2464 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 2465 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 2466 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 2467 | ` */` |
|   5248 | 2468 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 2469 | `{` |
|   5253 | 2470 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5253 | 2471 | `	pDev->pHandle = 0;` |
|   5253 | 2472 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5253 | 2473 | `}` |
|      - | 2474 | `/*` |
|      - | 2475 | ` * Reset the IO private structure.` |
|      - | 2476 | ` */` |
|     26 | 2477 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 2478 | `{` |
|     28 | 2479 | `	SyBlobReset(&pDev->sBuffer);` |
|     28 | 2480 | `	pDev->nOfft = 0;` |
|     28 | 2481 | `}` |
|      - | 2482 | `/* Forward declaration */` |
|      - | 2483 |  |
|      - | 2484 | `/*` |
|      - | 2485 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 2486 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 2487 | ` * Parameters` |
|      - | 2488 | ` *  $filename` |
|      - | 2489 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 2490 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 2491 | ` *   then a regular file is assumed.` |
|      - | 2492 | ` *  $mode` |
|      - | 2493 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 2494 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 2495 | ` *   modes.` |
|      - | 2496 | ` *  $use_include_path` |
|      - | 2497 | ` *   You can use the optional second parameter and set it to` |
|      - | 2498 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2499 | ` *  $context` |
|      - | 2500 | ` *   A context stream resource.` |
|      - | 2501 | ` * Return` |
|      - | 2502 | ` *  File handle on success or FALSE on failure.` |
|      - | 2503 | ` */` |
|      - | 2504 | `/*` |
|      - | 2505 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 2506 | ` *                                  int $offset = -1)` |
|      - | 2507 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 2508 | ` */` |
|     28 | 2509 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2510 | `{` |
|      - | 2511 | `	const ph7_io_stream *pStream;` |
|      - | 2512 | `	io_private *pDev;` |
|     29 | 2513 | `	ph7_int64 nMax = -1;` |
|      - | 2514 | `	char zBuf[4096];` |
|      - | 2515 | `	ph7_int64 nRead;` |
|     29 | 2516 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2517 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2519 | `		return PH7_OK;` |
|      - | 2520 | `	}` |
|     29 | 2521 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     29 | 2522 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2523 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|     29 | 2527 | `	pStream = pDev->pStream;` |
|     29 | 2528 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 2529 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|     29 | 2532 | `	if( nArg > 1 ){` |
|      5 | 2533 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 2534 | `	}` |
|     29 | 2535 | `	if( nArg > 2 ){` |
|      5 | 2536 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 2537 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 2538 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 2539 | `		}` |
|      2 | 2540 | `	}` |
|     29 | 2541 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     52 | 2542 | `	while( nMax != 0 ){` |
|     50 | 2543 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     50 | 2544 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 2545 | `			nAsk = nMax;` |
|      1 | 2546 | `		}` |
|     50 | 2547 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     50 | 2548 | `		if( nRead < 1 ){` |
|     27 | 2549 | `			break;` |
|      - | 2550 | `		}` |
|     24 | 2551 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     24 | 2552 | `		if( nMax > 0 ){` |
|      3 | 2553 | `			nMax -= nRead;` |
|      1 | 2554 | `		}` |
|      1 | 2555 | `	}` |
|     29 | 2556 | `	return PH7_OK;` |
|     15 | 2557 | `}` |
|      - | 2558 | `/*` |
|      - | 2559 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 2560 | ` */` |
|      4 | 2561 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2562 | `{` |
|      - | 2563 | `	ph7_value *pArr,*pV;` |
|      - | 2564 | `	ph7_io_stream **apDev;` |
|      - | 2565 | `	sxu32 n;` |
|      2 | 2566 | `	SXUNUSED(nArg);` |
|      2 | 2567 | `	SXUNUSED(apArg);` |
|      6 | 2568 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 2569 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 2570 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2571 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2572 | `		return PH7_OK;` |
|      - | 2573 | `	}` |
|      6 | 2574 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 2575 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 2576 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 2577 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 2578 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 2579 | `	}` |
|      6 | 2580 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 2581 | `	return PH7_OK;` |
|      4 | 2582 | `}` |
|      - | 2583 | `/*` |
|      - | 2584 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 2585 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 2586 | ` */` |
|      2 | 2587 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2588 | `{` |
|      - | 2589 | `	io_private *pDev;` |
|      - | 2590 | `	ph7_value *pArr,*pV;` |
|      3 | 2591 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 2592 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2594 | `		return PH7_OK;` |
|      - | 2595 | `	}` |
|      3 | 2596 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 2597 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 2598 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2599 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2600 | `		return PH7_OK;` |
|      - | 2601 | `	}` |
|      3 | 2602 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 2603 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 2604 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 2605 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2606 | `		return PH7_OK;` |
|      - | 2607 | `	}` |
|      3 | 2608 | `	ph7_value_bool(pV,0);` |
|      3 | 2609 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 2610 | `	ph7_value_bool(pV,1);` |
|      3 | 2611 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 2612 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 2613 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 2614 | `	ph7_value_bool(pV,0);` |
|      3 | 2615 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 2616 | `	ph7_value_int(pV,0);` |
|      3 | 2617 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 2618 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2619 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 2620 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2621 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 2622 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 2623 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 2624 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 2625 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 2626 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 2627 | `	return PH7_OK;` |
|      2 | 2628 | `}` |
|      - | 2629 | `/*` |
|      - | 2630 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 2631 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 2632 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 2633 | ` * resource, options unconsumed).` |
|      - | 2634 | ` */` |
|      2 | 2635 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2636 | `{` |
|      3 | 2637 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 2638 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 2639 | `	}else{` |
|    ! 0 | 2640 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 2641 | `		if( pArr == 0 ){` |
|    ! 0 | 2642 | `			ph7_result_null(pCtx);` |
|    ! 0 | 2643 | `			return PH7_OK;` |
|      - | 2644 | `		}` |
|    ! 0 | 2645 | `		ph7_result_value(pCtx,pArr);` |
|      - | 2646 | `	}` |
|      3 | 2647 | `	return PH7_OK;` |
|      2 | 2648 | `}` |
|      - | 2649 | `/*` |
|      - | 2650 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 2651 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 2652 | ` */` |
|      - | 2653 | `#ifdef PH7_ENABLE_NET` |
|      - | 2654 | `typedef struct sock_private sock_private;` |
|      - | 2655 | `struct sock_private` |
|      - | 2656 | `{` |
|      - | 2657 | `	ph7_vm *pVm;` |
|      - | 2658 | `	ph7_socket sock;` |
|      - | 2659 | `	int bEof;` |
|      - | 2660 | `};` |
|     15 | 2661 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 2662 | `{` |
|     15 | 2663 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2664 | `	int n;` |
|     15 | 2665 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 2666 | `		return 0;` |
|      - | 2667 | `	}` |
|     13 | 2668 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     13 | 2669 | `	if( n <= 0 ){` |
|      4 | 2670 | `		pSock->bEof = 1;` |
|      4 | 2671 | `		return 0;` |
|      - | 2672 | `	}` |
|      9 | 2673 | `	return (ph7_int64)n;` |
|      5 | 2674 | `}` |
|      4 | 2675 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 2676 | `{` |
|      4 | 2677 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 2678 | `	int n;` |
|      4 | 2679 | `	if( pSock == 0 ){` |
|    ! 0 | 2680 | `		return -1;` |
|      - | 2681 | `	}` |
|      4 | 2682 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 2683 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 2684 | `}` |
|      4 | 2685 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 2686 | `{` |
|      4 | 2687 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 2688 | `	if( pSock == 0 ){` |
|    ! 0 | 2689 | `		return;` |
|      - | 2690 | `	}` |
|      4 | 2691 | `	PH7_NetClose(pSock->sock);` |
|      4 | 2692 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 2693 | `}` |
|      - | 2694 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 2695 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 2696 | `{` |
|      - | 2697 | `	sock_private *pSock;` |
|      - | 2698 | `	ph7_socket sock;` |
|      - | 2699 | `	char zHost[256];` |
|      - | 2700 | `	const char *zColon;` |
|    ! 0 | 2701 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 2702 | `	const char *zErr = "";` |
|    ! 0 | 2703 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 2704 | `	SXUNUSED(iMode);` |
|    ! 0 | 2705 | `	if( pVm == 0 ){` |
|    ! 0 | 2706 | `		return -1;` |
|      - | 2707 | `	}` |
|    ! 0 | 2708 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 2709 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 2710 | `		zColon--;` |
|    ! 0 | 2711 | `	}` |
|    ! 0 | 2712 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 2713 | `		return -1;` |
|      - | 2714 | `	}` |
|      - | 2715 | `	{` |
|    ! 0 | 2716 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 2717 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 2718 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 2719 | `		}` |
|    ! 0 | 2720 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 2721 | `		zHost[n] = 0;` |
|      - | 2722 | `	}` |
|      - | 2723 | `	{` |
|    ! 0 | 2724 | `		sxi32 iTmp = 0;` |
|    ! 0 | 2725 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 2726 | `		iPort = (int)iTmp;` |
|      - | 2727 | `	}` |
|    ! 0 | 2728 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 2729 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 2730 | `		return -1;` |
|      - | 2731 | `	}` |
|    ! 0 | 2732 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 2733 | `	if( pSock == 0 ){` |
|    ! 0 | 2734 | `		PH7_NetClose(sock);` |
|    ! 0 | 2735 | `		return -1;` |
|      - | 2736 | `	}` |
|    ! 0 | 2737 | `	pSock->pVm = pVm;` |
|    ! 0 | 2738 | `	pSock->sock = sock;` |
|    ! 0 | 2739 | `	pSock->bEof = 0;` |
|    ! 0 | 2740 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 2741 | `	return PH7_OK;` |
|    ! 0 | 2742 | `}` |
|      - | 2743 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 2744 | `	"tcp",` |
|      - | 2745 | `	PH7_IO_STREAM_VERSION,` |
|      - | 2746 | `	SockStreamData_Open, /* xOpen */` |
|      - | 2747 | `	0,   /* xOpenDir */` |
|      - | 2748 | `	SockStreamData_Close,/* xClose */` |
|      - | 2749 | `	0,  /* xCloseDir */` |
|      - | 2750 | `	SockStreamData_Read, /* xRead */` |
|      - | 2751 | `	0,  /* xReadDir */` |
|      - | 2752 | `	SockStreamData_Write,/* xWrite */` |
|      - | 2753 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 2754 | `	0,  /* xLock */` |
|      - | 2755 | `	0,  /* xRewindDir */` |
|      - | 2756 | `	0,  /* xTell */` |
|      - | 2757 | `	0,  /* xTrunc */` |
|      - | 2758 | `	0,  /* xSync */` |
|      - | 2759 | `	0   /* xStat */` |
|      - | 2760 | `};` |
|      - | 2761 | `#endif /* PH7_ENABLE_NET */` |
|      - | 2762 | `/*` |
|      - | 2763 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 2764 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 2765 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 2766 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 2767 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 2768 | ` * php streamWrapper protocol method on it.` |
|      - | 2769 | ` */` |
|      - | 2770 | `#define PHL_UWRAP_MAX 8` |
|      - | 2771 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 2772 | `struct uwrap_slot` |
|      - | 2773 | `{` |
|      - | 2774 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 2775 | `	char zScheme[32];         /* protocol name */` |
|      - | 2776 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 2777 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 2778 | `};` |
|      - | 2779 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 2780 | `struct uwrap_handle` |
|      - | 2781 | `{` |
|      - | 2782 | `	ph7_vm *pVm;` |
|      - | 2783 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 2784 | `	int iSlot;` |
|      - | 2785 | `	int bEof;` |
|      - | 2786 | `};` |
|      - | 2787 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 2788 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 2789 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 2790 | `	ph7_value *pResult)` |
|      1 | 2791 | `{` |
|      - | 2792 | `	ph7_class_method *pMeth;` |
|     27 | 2793 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 2794 | `		return -1;` |
|      - | 2795 | `	}` |
|     27 | 2796 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 2797 | `	if( pMeth == 0 ){` |
|    ! 0 | 2798 | `		return -1;` |
|      - | 2799 | `	}` |
|     27 | 2800 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 2801 | `		return -1;` |
|      - | 2802 | `	}` |
|     27 | 2803 | `	return 0;` |
|     14 | 2804 | `}` |
|      8 | 2805 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 2806 | `{` |
|      9 | 2807 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2808 | `	ph7_value sArg,sRet;` |
|      - | 2809 | `	const char *zData;` |
|      9 | 2810 | `	int nData = 0;` |
|      9 | 2811 | `	ph7_int64 n = 0;` |
|      9 | 2812 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 2813 | `		return 0;` |
|      - | 2814 | `	}` |
|      9 | 2815 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 2816 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 2817 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 2818 | `	{` |
|      - | 2819 | `		ph7_value *apArg[1];` |
|      9 | 2820 | `		apArg[0] = &sArg;` |
|      9 | 2821 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 2822 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 2823 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2824 | `			return -1;` |
|      - | 2825 | `		}` |
|      - | 2826 | `	}` |
|      9 | 2827 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 2828 | `	if( nData > 0 ){` |
|      7 | 2829 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 2830 | `			nData = (int)nRead;` |
|    ! 0 | 2831 | `		}` |
|      7 | 2832 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 2833 | `		n = nData;` |
|      4 | 2834 | `	}else{` |
|      3 | 2835 | `		pH->bEof = 1;` |
|      - | 2836 | `	}` |
|      9 | 2837 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 2838 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 2839 | `	return n;` |
|      5 | 2840 | `}` |
|      2 | 2841 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 2842 | `{` |
|      3 | 2843 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2844 | `	ph7_value sArg,sRet;` |
|      - | 2845 | `	ph7_int64 n;` |
|      3 | 2846 | `	if( pH == 0 ){` |
|    ! 0 | 2847 | `		return -1;` |
|      - | 2848 | `	}` |
|      3 | 2849 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 2850 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 2851 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 2852 | `	{` |
|      - | 2853 | `		ph7_value *apArg[1];` |
|      3 | 2854 | `		apArg[0] = &sArg;` |
|      3 | 2855 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 2856 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 2857 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2858 | `			return -1;` |
|      - | 2859 | `		}` |
|      - | 2860 | `	}` |
|      3 | 2861 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 2862 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 2863 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 2864 | `	return n;` |
|      2 | 2865 | `}` |
|      2 | 2866 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 2867 | `{` |
|      3 | 2868 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2869 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 2870 | `	ph7_value *apArg[2];` |
|      - | 2871 | `	int rc;` |
|      3 | 2872 | `	if( pH == 0 ){` |
|    ! 0 | 2873 | `		return -1;` |
|      - | 2874 | `	}` |
|      3 | 2875 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 2876 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 2877 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 2878 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 2879 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 2880 | `	apArg[0] = &sOfft;` |
|      3 | 2881 | `	apArg[1] = &sWhence;` |
|      3 | 2882 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 2883 | `	if( rc == 0 ){` |
|      3 | 2884 | `		pH->bEof = 0;` |
|      3 | 2885 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 2886 | `	}` |
|      3 | 2887 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 2888 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 2889 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 2890 | `	return rc;` |
|      2 | 2891 | `}` |
|      2 | 2892 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 2893 | `{` |
|      3 | 2894 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 2895 | `	ph7_value sRet;` |
|      - | 2896 | `	ph7_int64 n;` |
|      3 | 2897 | `	if( pH == 0 ){` |
|    ! 0 | 2898 | `		return -1;` |
|      - | 2899 | `	}` |
|      3 | 2900 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 2901 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 2902 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 2903 | `		return -1;` |
|      - | 2904 | `	}` |
|      3 | 2905 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 2906 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 2907 | `	return n;` |
|      2 | 2908 | `}` |
|      6 | 2909 | `static void UwrapClose(void *pHandle)` |
|      1 | 2910 | `{` |
|      7 | 2911 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 2912 | `	if( pH == 0 ){` |
|    ! 0 | 2913 | `		return;` |
|      - | 2914 | `	}` |
|      7 | 2915 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 2916 | `	if( pH->pObj ){` |
|      7 | 2917 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 2918 | `	}` |
|      7 | 2919 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 2920 | `}` |
|      - | 2921 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 2922 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 2923 | `{` |
|      7 | 2924 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 2925 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 2926 | `	ph7_class *pClass;` |
|      - | 2927 | `	uwrap_handle *pH;` |
|      - | 2928 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 2929 | `	ph7_value *apArg[4];` |
|      - | 2930 | `	int rc;` |
|      7 | 2931 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 2932 | `		return -1;` |
|      - | 2933 | `	}` |
|      7 | 2934 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 2935 | `	if( pClass == 0 ){` |
|    ! 0 | 2936 | `		return -1;` |
|      - | 2937 | `	}` |
|      7 | 2938 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 2939 | `	if( pH == 0 ){` |
|    ! 0 | 2940 | `		return -1;` |
|      - | 2941 | `	}` |
|      7 | 2942 | `	pH->pVm = pVm;` |
|      7 | 2943 | `	pH->iSlot = iSlot;` |
|      7 | 2944 | `	pH->bEof = 0;` |
|      7 | 2945 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 2946 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 2947 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 2948 | `		return -1;` |
|      - | 2949 | `	}` |
|      - | 2950 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 2951 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 2952 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 2953 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 2954 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 2955 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 2956 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 2957 | `	 * it as "could not be passed by reference"). */` |
|      - | 2958 | `	{` |
|      7 | 2959 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 2960 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 2961 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 2962 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 2963 | `			return -1;` |
|      - | 2964 | `		}` |
|      7 | 2965 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 2966 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 2967 | `	}` |
|      - | 2968 | `	{` |
|      - | 2969 | `		SyBlob sUrl;` |
|      7 | 2970 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 2971 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 2972 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 2973 | `		SyBlobRelease(&sUrl);` |
|      - | 2974 | `	}` |
|      9 | 2975 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 2976 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 2977 | `	ph7_value_int(&sOpts,0);` |
|      7 | 2978 | `	apArg[0] = &sPath;` |
|      7 | 2979 | `	apArg[1] = &sMode;` |
|      7 | 2980 | `	apArg[2] = &sOpts;` |
|      7 | 2981 | `	apArg[3] = &sOpened;` |
|      7 | 2982 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 2983 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 2984 | `		rc = -1;` |
|    ! 0 | 2985 | `	}` |
|      7 | 2986 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 2987 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 2988 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 2989 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 2990 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 2991 | `	if( rc != 0 ){` |
|    ! 0 | 2992 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 2993 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 2994 | `		return -1;` |
|      - | 2995 | `	}` |
|      7 | 2996 | `	*ppHandle = (void *)pH;` |
|      7 | 2997 | `	return PH7_OK;` |
|      4 | 2998 | `}` |
|      - | 2999 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 3000 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 3001 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 3002 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 3003 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 3004 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 3005 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 3006 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 3007 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 3008 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 3009 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 3010 | `PHL_UWRAP_THUNK(7)` |
|      - | 3011 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 3012 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 3013 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 3014 | `};` |
|      - | 3015 | `/*` |
|      - | 3016 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 3017 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 3018 | ` */` |
|      2 | 3019 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3020 | `{` |
|      - | 3021 | `	const char *zScheme,*zClass;` |
|      3 | 3022 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 3023 | `	if( nArg < 2 ){` |
|    ! 0 | 3024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3025 | `		return PH7_OK;` |
|      - | 3026 | `	}` |
|      3 | 3027 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3028 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 3029 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 3030 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 3031 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3032 | `		return PH7_OK;` |
|      - | 3033 | `	}` |
|      - | 3034 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 3035 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 3036 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 3037 | `	{` |
|      3 | 3038 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 3039 | `		sxu32 n;` |
|     11 | 3040 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 3041 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 3042 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 3043 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3044 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 3045 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3046 | `				return PH7_OK;` |
|      - | 3047 | `			}` |
|      5 | 3048 | `		}` |
|      - | 3049 | `	}` |
|      3 | 3050 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 3051 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 3052 | `			iFree = i;` |
|      3 | 3053 | `			break;` |
|      - | 3054 | `		}` |
|    ! 0 | 3055 | `	}` |
|      3 | 3056 | `	if( iFree < 0 ){` |
|    ! 0 | 3057 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3058 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 3059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3060 | `		return PH7_OK;` |
|      - | 3061 | `	}` |
|      - | 3062 | `	{` |
|      3 | 3063 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 3064 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 3065 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 3066 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 3067 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 3068 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 3069 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 3070 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 3071 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 3072 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 3073 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 3074 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 3075 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 3076 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 3077 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 3078 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 3079 | `	}` |
|      3 | 3080 | `	ph7_result_bool(pCtx,1);` |
|      3 | 3081 | `	return PH7_OK;` |
|      2 | 3082 | `}` |
|      2 | 3083 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3084 | `{` |
|      - | 3085 | `	const char *zScheme;` |
|      - | 3086 | `	int nScheme,i;` |
|      3 | 3087 | `	if( nArg < 1 ){` |
|    ! 0 | 3088 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3089 | `		return PH7_OK;` |
|      - | 3090 | `	}` |
|      3 | 3091 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 3092 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 3093 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 3094 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 3095 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 3096 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 3097 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 3098 | `			 * what unregister means to a script — recorded. */` |
|      3 | 3099 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 3100 | `			ph7_result_bool(pCtx,1);` |
|      3 | 3101 | `			return PH7_OK;` |
|      - | 3102 | `		}` |
|    ! 0 | 3103 | `	}` |
|    ! 0 | 3104 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 3105 | `	return PH7_OK;` |
|      2 | 3106 | `}` |
|      - | 3107 | `#ifdef PH7_ENABLE_NET` |
|      - | 3108 | `/*` |
|      - | 3109 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 3110 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 3111 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 3112 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 3113 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 3114 | ` */` |
|      6 | 3115 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3116 | `{` |
|      6 | 3117 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 3118 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 3119 | `	const char *zTarget,*zErr = "";` |
|      - | 3120 | `	char zHost[256];` |
|      6 | 3121 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 3122 | `	ph7_socket sock;` |
|      - | 3123 | `	io_private *pDev;` |
|      - | 3124 | `	sock_private *pSock;` |
|      6 | 3125 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 3126 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 3127 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 3128 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 3129 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3130 | `		return PH7_OK;` |
|      - | 3131 | `	}` |
|      6 | 3132 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 3133 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 3134 | `	{` |
|      6 | 3135 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 3136 | `		const char *zSep = 0;` |
|     32 | 3137 | `		while( z < zEnd - 2 ){` |
|     30 | 3138 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 3139 | `				zSep = z;` |
|      4 | 3140 | `				break;` |
|      - | 3141 | `			}` |
|     26 | 3142 | `			z++;` |
|    ! 0 | 3143 | `		}` |
|      6 | 3144 | `		if( zSep ){` |
|      4 | 3145 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 3146 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3147 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 3148 | `					nTarget,zTarget);` |
|    ! 0 | 3149 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 3150 | `				return PH7_OK;` |
|      - | 3151 | `			}` |
|      4 | 3152 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 3153 | `			zTarget = zSep + 3;` |
|      2 | 3154 | `		}` |
|      - | 3155 | `	}` |
|      - | 3156 | `	/* host[:port] */` |
|      - | 3157 | `	{` |
|      6 | 3158 | `		int i = nTarget - 1;` |
|      6 | 3159 | `		int nHost = nTarget;` |
|     48 | 3160 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 3161 | `			i--;` |
|    ! 0 | 3162 | `		}` |
|      6 | 3163 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 3164 | `			sxi32 iTmp = 0;` |
|      2 | 3165 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 3166 | `			iPort = (int)iTmp;` |
|      2 | 3167 | `			nHost = i;` |
|      1 | 3168 | `		}` |
|      6 | 3169 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 3170 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 3171 | `		}` |
|      6 | 3172 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 3173 | `		zHost[nHost] = 0;` |
|      - | 3174 | `	}` |
|      6 | 3175 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 3176 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 3177 | `	}` |
|      6 | 3178 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 3179 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 3180 | `		if( rTimeout > 0 ){` |
|      6 | 3181 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 3182 | `		}` |
|      3 | 3183 | `	}` |
|      6 | 3184 | `	if( iPort < 0 ){` |
|    ! 0 | 3185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3186 | `		return PH7_OK;` |
|      - | 3187 | `	}` |
|      6 | 3188 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 3189 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 3190 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 3191 | `		{` |
|      2 | 3192 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 3193 | `			if( pTmp ){` |
|      2 | 3194 | `				if( nArg > iArgErrno ){` |
|      2 | 3195 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 3196 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 3197 | `				}` |
|      2 | 3198 | `				if( nArg > iArgErrstr ){` |
|      2 | 3199 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 3200 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 3201 | `				}` |
|      1 | 3202 | `			}` |
|      - | 3203 | `		}` |
|      - | 3204 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 3205 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 3206 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 3207 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3208 | `		return PH7_OK;` |
|      - | 3209 | `	}` |
|      - | 3210 | `	{` |
|      4 | 3211 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 3212 | `		if( pTmp ){` |
|      4 | 3213 | `			if( nArg > iArgErrno ){` |
|      4 | 3214 | `				ph7_value_int(pTmp,0);` |
|      4 | 3215 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 3216 | `			}` |
|      4 | 3217 | `			if( nArg > iArgErrstr ){` |
|      4 | 3218 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 3219 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 3220 | `			}` |
|      2 | 3221 | `		}` |
|      - | 3222 | `	}` |
|      - | 3223 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 3224 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 3225 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 3226 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 3227 | `		PH7_NetClose(sock);` |
|    ! 0 | 3228 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3229 | `		return PH7_OK;` |
|      - | 3230 | `	}` |
|      4 | 3231 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 3232 | `	pSock->sock = sock;` |
|      4 | 3233 | `	pSock->bEof = 0;` |
|      4 | 3234 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 3235 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 3236 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 3237 | `	return PH7_OK;` |
|      3 | 3238 | `}` |
|      - | 3239 | `#endif /* PH7_ENABLE_NET */` |
|    212 | 3240 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3241 | `{` |
|      - | 3242 | `	const ph7_io_stream *pStream;` |
|      - | 3243 | `	const char *zUri,*zMode;` |
|      - | 3244 | `	ph7_value *pResource;` |
|      - | 3245 | `	io_private *pDev;` |
|      - | 3246 | `	int iLen,imLen;` |
|      - | 3247 | `	int iOpenFlags;` |
|    217 | 3248 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3249 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3250 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 3251 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Extract the URI and the desired access mode */` |
|    217 | 3255 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    217 | 3256 | `	if( nArg > 1 ){` |
|    217 | 3257 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    111 | 3258 | `	}else{` |
|      - | 3259 | `		/* Set a default read-only mode */` |
|    ! 0 | 3260 | `		zMode = "r";` |
|    ! 0 | 3261 | `		imLen = (int)sizeof(char);` |
|      - | 3262 | `	}` |
|      - | 3263 | `	/* Try to extract a stream */` |
|    217 | 3264 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    217 | 3265 | `	if( pStream == 0 ){` |
|    ! 0 | 3266 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3267 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 3268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3269 | `		return PH7_OK;` |
|      - | 3270 | `	}` |
|      - | 3271 | `	/* Allocate a new IO private instance */` |
|    217 | 3272 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    217 | 3273 | `	if( pDev == 0 ){` |
|    ! 0 | 3274 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3276 | `		return PH7_OK;` |
|      - | 3277 | `	}` |
|    217 | 3278 | `	pResource = 0;` |
|    217 | 3279 | `	if( nArg > 3 ){` |
|    ! 0 | 3280 | `		pResource = apArg[3];` |
|    217 | 3281 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 3282 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 3283 | `		 * access the underlying virtual machine.` |
|      - | 3284 | `		 */` |
|     22 | 3285 | `		pResource = apArg[0];` |
|     10 | 3286 | `	}` |
|      - | 3287 | `	/* Initialize the structure */` |
|    217 | 3288 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3289 | `	/* Convert open mode to PH7 flags */` |
|    217 | 3290 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 3291 | `	/* Try to get a handle */` |
|    323 | 3292 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    106 | 3293 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    217 | 3294 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 3295 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 3296 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3297 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 3298 | `		return PH7_OK;` |
|      - | 3299 | `	}` |
|      - | 3300 | `	/* All done,return the io_private instance as a resource */` |
|    217 | 3301 | `	ph7_result_resource(pCtx,pDev);` |
|    217 | 3302 | `	return PH7_OK;` |
|    111 | 3303 | `}` |
|      - | 3304 | `/*` |
|      - | 3305 | ` * bool fclose(resource $handle)` |
|      - | 3306 | ` *  Closes an open file pointer` |
|      - | 3307 | ` * Parameters` |
|      - | 3308 | ` *  $handle` |
|      - | 3309 | ` *   The file pointer.` |
|      - | 3310 | ` * Return` |
|      - | 3311 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3312 | ` */` |
|    342 | 3313 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3314 | `{` |
|      - | 3315 | `	const ph7_io_stream *pStream;` |
|      - | 3316 | `	io_private *pDev;` |
|      - | 3317 | `	ph7_vm *pVm;` |
|    347 | 3318 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3319 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3320 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3321 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3322 | `		return PH7_OK;` |
|      - | 3323 | `	}` |
|      - | 3324 | `	/* Extract our private data */` |
|    347 | 3325 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3326 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    347 | 3327 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 3328 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3329 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 3330 | `	}` |
|      - | 3331 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    345 | 3332 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3333 | `		/*Expecting an IO handle */` |
|    ! 0 | 3334 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3335 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3336 | `		return PH7_OK;` |
|      - | 3337 | `	}` |
|      - | 3338 | `	/* Point to the target IO stream device */` |
|    345 | 3339 | `	pStream = pDev->pStream;` |
|    345 | 3340 | `	if( pStream == 0 ){` |
|    ! 0 | 3341 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3342 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3343 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3344 | `			);` |
|    ! 0 | 3345 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3346 | `		return PH7_OK;` |
|      - | 3347 | `	}` |
|      - | 3348 | `	/* Point to the VM that own this context */` |
|    345 | 3349 | `	pVm = pCtx->pVm;` |
|      - | 3350 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    345 | 3351 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 3352 | `		/* Perform the requested operation */` |
|    345 | 3353 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3354 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    345 | 3355 | `		MarkIOPrivateClosed(pDev);` |
|    170 | 3356 | `	}` |
|      - | 3357 | `	/* Return TRUE */` |
|    345 | 3358 | `	ph7_result_bool(pCtx,1);` |
|    345 | 3359 | `	return PH7_OK;` |
|    176 | 3360 | `}` |
|      - | 3361 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3362 | `/*` |
|      - | 3363 | ` * MD5/SHA1 digest consumer.` |
|      - | 3364 | ` */` |
|     72 | 3365 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3366 | `{` |
|      - | 3367 | `	/* Append hex chunk verbatim */` |
|     73 | 3368 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 3369 | `	return SXRET_OK;` |
|      1 | 3370 | `}` |
|      - | 3371 | `/*` |
|      - | 3372 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 3373 | ` *  Calculates the md5 hash of a given file.` |
|      - | 3374 | ` * Parameters` |
|      - | 3375 | ` *  $uri` |
|      - | 3376 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3377 | ` *  $raw_output` |
|      - | 3378 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 3379 | ` * Return` |
|      - | 3380 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 3381 | ` */` |
|      2 | 3382 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3383 | `{` |
|      - | 3384 | `	const ph7_io_stream *pStream;` |
|      - | 3385 | `	unsigned char zDigest[16];` |
|      3 | 3386 | `	int raw_output  = FALSE;` |
|      - | 3387 | `	const char *zFile;` |
|      - | 3388 | `	MD5Context sCtx;` |
|      - | 3389 | `	char zBuf[8192];` |
|      - | 3390 | `	void *pHandle;` |
|      - | 3391 | `	ph7_int64 n;` |
|      - | 3392 | `	int nLen;` |
|      3 | 3393 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3394 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3395 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3397 | `		return PH7_OK;` |
|      - | 3398 | `	}` |
|      - | 3399 | `	/* Extract the file path */` |
|      3 | 3400 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3401 | `	/* Point to the target IO stream device */` |
|      3 | 3402 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3403 | `	if( pStream == 0 ){` |
|    ! 0 | 3404 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3406 | `		return PH7_OK;` |
|      - | 3407 | `	}` |
|      3 | 3408 | `	if( nArg > 1 ){` |
|    ! 0 | 3409 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3410 | `	}` |
|      - | 3411 | `	/* Try to open the file in read-only mode */` |
|      3 | 3412 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3413 | `	if( pHandle == 0 ){` |
|    ! 0 | 3414 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3416 | `		return PH7_OK;` |
|      - | 3417 | `	}` |
|      - | 3418 | `	/* Init the MD5 context */` |
|      3 | 3419 | `	MD5Init(&sCtx);` |
|      - | 3420 | `	/* Perform the requested operation */` |
|      2 | 3421 | `	for(;;){` |
|      5 | 3422 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3423 | `		if( n < 1 ){` |
|      - | 3424 | `			/* EOF or IO error,break immediately */` |
|      3 | 3425 | `			break;` |
|      - | 3426 | `		}` |
|      3 | 3427 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3428 | `	}` |
|      - | 3429 | `	/* Close the stream */` |
|      3 | 3430 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3431 | `	/* Extract the digest */` |
|      3 | 3432 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 3433 | `	if( raw_output ){` |
|      - | 3434 | `		/* Output raw digest */` |
|    ! 0 | 3435 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3436 | `	}else{` |
|      - | 3437 | `		/* Perform a binary to hex conversion */` |
|      3 | 3438 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3439 | `	}` |
|      3 | 3440 | `	return PH7_OK;` |
|      2 | 3441 | `}` |
|      - | 3442 | `/*` |
|      - | 3443 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 3444 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 3445 | ` * Parameters` |
|      - | 3446 | ` *  $uri` |
|      - | 3447 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 3448 | ` *  $raw_output` |
|      - | 3449 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 3450 | ` * Return` |
|      - | 3451 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 3452 | ` */` |
|      2 | 3453 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3454 | `{` |
|      - | 3455 | `	const ph7_io_stream *pStream;` |
|      - | 3456 | `	unsigned char zDigest[20];` |
|      3 | 3457 | `	int raw_output  = FALSE;` |
|      - | 3458 | `	const char *zFile;` |
|      - | 3459 | `	SHA1Context sCtx;` |
|      - | 3460 | `	char zBuf[8192];` |
|      - | 3461 | `	void *pHandle;` |
|      - | 3462 | `	ph7_int64 n;` |
|      - | 3463 | `	int nLen;` |
|      3 | 3464 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3465 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3466 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3468 | `		return PH7_OK;` |
|      - | 3469 | `	}` |
|      - | 3470 | `	/* Extract the file path */` |
|      3 | 3471 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3472 | `	/* Point to the target IO stream device */` |
|      3 | 3473 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3474 | `	if( pStream == 0 ){` |
|    ! 0 | 3475 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3476 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3477 | `		return PH7_OK;` |
|      - | 3478 | `	}` |
|      3 | 3479 | `	if( nArg > 1 ){` |
|    ! 0 | 3480 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3481 | `	}` |
|      - | 3482 | `	/* Try to open the file in read-only mode */` |
|      3 | 3483 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3484 | `	if( pHandle == 0 ){` |
|    ! 0 | 3485 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3487 | `		return PH7_OK;` |
|      - | 3488 | `	}` |
|      - | 3489 | `	/* Init the SHA1 context */` |
|      3 | 3490 | `	SHA1Init(&sCtx);` |
|      - | 3491 | `	/* Perform the requested operation */` |
|      2 | 3492 | `	for(;;){` |
|      5 | 3493 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3494 | `		if( n < 1 ){` |
|      - | 3495 | `			/* EOF or IO error,break immediately */` |
|      3 | 3496 | `			break;` |
|      - | 3497 | `		}` |
|      3 | 3498 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 3499 | `	}` |
|      - | 3500 | `	/* Close the stream */` |
|      3 | 3501 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3502 | `	/* Extract the digest */` |
|      3 | 3503 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 3504 | `	if( raw_output ){` |
|      - | 3505 | `		/* Output raw digest */` |
|    ! 0 | 3506 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 3507 | `	}else{` |
|      - | 3508 | `		/* Perform a binary to hex conversion */` |
|      3 | 3509 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 3510 | `	}` |
|      3 | 3511 | `	return PH7_OK;` |
|      2 | 3512 | `}` |
|      - | 3513 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3514 | `/*` |
|      - | 3515 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 3516 | ` *  Parse a configuration file.` |
|      - | 3517 | ` * Parameters` |
|      - | 3518 | ` * $filename` |
|      - | 3519 | ` *  The filename of the ini file being parsed.` |
|      - | 3520 | ` * $process_sections` |
|      - | 3521 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 3522 | ` *  with the section names and settings included.` |
|      - | 3523 | ` *  The default for process_sections is FALSE.` |
|      - | 3524 | ` * $scanner_mode` |
|      - | 3525 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 3526 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 3527 | ` * Return` |
|      - | 3528 | ` *  The settings are returned as an associative array on success.` |
|      - | 3529 | ` *  Otherwise is returned.` |
|      - | 3530 | ` */` |
|      2 | 3531 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3532 | `{` |
|      - | 3533 | `	const ph7_io_stream *pStream;` |
|      - | 3534 | `	const char *zFile;` |
|      - | 3535 | `	SyBlob sContents;` |
|      - | 3536 | `	void *pHandle;` |
|      - | 3537 | `	int nLen;` |
|      3 | 3538 | `	sxi32 rc = PH7_OK;` |
|      3 | 3539 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3540 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3541 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3543 | `		return PH7_OK;` |
|      - | 3544 | `	}` |
|      - | 3545 | `	/* Extract the file path */` |
|      3 | 3546 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3547 | `	/* Point to the target IO stream device */` |
|      3 | 3548 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3549 | `	if( pStream == 0 ){` |
|    ! 0 | 3550 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3551 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3552 | `		return PH7_OK;` |
|      - | 3553 | `	}` |
|      - | 3554 | `	/* Try to open the file in read-only mode */` |
|      3 | 3555 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 3556 | `	if( pHandle == 0 ){` |
|    ! 0 | 3557 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3558 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3559 | `		return PH7_OK;` |
|      - | 3560 | `	}` |
|      3 | 3561 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 3562 | `	/* Read the whole file */` |
|      3 | 3563 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 3564 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 3565 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 3566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3567 | `	}else{` |
|      - | 3568 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 3569 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 3570 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 3571 | `	}` |
|      - | 3572 | `	/* Close the stream */` |
|      3 | 3573 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3574 | `	/* Release the working buffer */` |
|      3 | 3575 | `	SyBlobRelease(&sContents);` |
|      - | 3576 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 3577 | `	return rc;` |
|      2 | 3578 | `}` |
|      - | 3579 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 3580 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 3581 | `/*` |
|      - | 3582 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 3583 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 3584 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 3585 | ` * to any non-VFS resource).` |
|      - | 3586 | ` */` |
|      - | 3587 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 3588 | `{` |
|      - | 3589 | `	SXUNUSED(pResource);` |
|      - | 3590 | `	return "Unknown";` |
|      - | 3591 | `}` |
|      - | 3592 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 3593 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 3594 | `{` |
|      - | 3595 | `	SXUNUSED(pResource);` |
|      - | 3596 | `	return 0;` |
|      - | 3597 | `}` |
|      - | 3598 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3599 |  |
