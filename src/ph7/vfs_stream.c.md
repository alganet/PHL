# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4340/4992 lines (86.94%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <stdio.h>` |
|       - |    8 | `#include <errno.h>` |
|       - |    9 | `#include <string.h>` |
|       - |   10 |  |
|       - |   11 | `#ifdef __UNIXES__` |
|       - |   12 | `#include <unistd.h>` |
|       - |   13 | `#include <sys/wait.h>` |
|       - |   14 | `#include <fcntl.h>` |
|       - |   15 | `#include <signal.h>` |
|       - |   16 | `#endif` |
|       - |   17 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |   18 | `/*` |
|       - |   19 | ` * Section:` |
|       - |   20 | ` *    IO stream implementation.` |
|       - |   21 | ` * Status:` |
|       - |   22 | ` *    Stable.` |
|       - |   23 | ` */` |
|       - |   24 | `/* Forward declaration */` |
|       - |   25 | `static void ResetIOPrivate(io_private *pDev);` |
|       - |   26 | `/*` |
|       - |   27 | ` * How many bytes sit AHEAD of where the script is: the line readers' read-ahead` |
|       - |   28 | ` * plus whatever the read filter chain has already produced and nobody has taken` |
|       - |   29 | ` * yet. Both are past the position a script observes, so ftell(), a SEEK_CUR` |
|       - |   30 | `` * seek and stream_get_meta_data()'s `unread_bytes` all have to discount them.`` |
|       - |   31 | ` */` |
|       - |   32 | `static void ResetIOPrivate(io_private *pDev);` |
|       - |   33 | `static sxu32 StreamAheadBytes(io_private *pDev);` |
|       - |   34 | `/*` |
|       - |   35 | ` * A write lands where the SCRIPT is, not where the device is. Everything the` |
|       - |   36 | ` * readers pulled ahead — the line buffer and the filter chain's output alike —` |
|       - |   37 | ` * sits between the two, so it is stepped over and dropped before the write.` |
|       - |   38 | ` * php does the same by seeking to the logical position it tracks.` |
|       - |   39 | ` */` |
|     669 |   40 | `static void StreamSeekBackForWrite(io_private *pDev)` |
|       5 |   41 | `{` |
|     674 |   42 | `	sxu32 nAhead = StreamAheadBytes(pDev);` |
|     674 |   43 | `	if( nAhead > 0 && pDev->pStream && pDev->pStream->xSeek ){` |
|      16 |   44 | `		pDev->pStream->xSeek(pDev->pHandle,-(ph7_int64)nAhead,1/*SEEK_CUR*/);` |
|      16 |   45 | `		ResetIOPrivate(pDev);` |
|       7 |   46 | `	}` |
|     674 |   47 | `}` |
|     158 |   48 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev)` |
|       5 |   49 | `{` |
|       - |   50 | `	ph7_int64 iOfft;` |
|     163 |   51 | `	if( pDev == 0 ){` |
|     ! 0 |   52 | `		return -1;` |
|       - |   53 | `	}` |
|     163 |   54 | `	if( pDev->pReadFilters ){` |
|       - |   55 | `		/* A read filter breaks the tie between the device's offset and the` |
|       - |   56 | `		 * script's: four base64 characters come out of three bytes, so the two` |
|       - |   57 | `		 * numbers are not even the same magnitude. php counts what it` |
|       - |   58 | `		 * DELIVERED, and so does this — less whatever a line reader is still` |
|       - |   59 | `		 * holding on the script's behalf. */` |
|      27 |   60 | `		iOfft = pDev->iFiltPos;` |
|      27 |   61 | `		if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|     ! 0 |   62 | `			iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);` |
|     ! 0 |   63 | `		}` |
|      27 |   64 | `		return iOfft;` |
|       - |   65 | `	}` |
|     137 |   66 | `	if( pDev->bDir ){` |
|       - |   67 | `		/* A directory handle's position is php's own record counter and` |
|       - |   68 | `		 * nothing the device knows about. */` |
|       7 |   69 | `		return pDev->iPos;` |
|       - |   70 | `	}` |
|     131 |   71 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xTell == 0 ){` |
|       - |   72 | `		/* php's stream layer tracks a position for EVERY stream and only asks` |
|       - |   73 | `		 * the device when it seeks, so a pipe or a socket -- neither of which` |
|       - |   74 | `		 * can be asked -- still reports how far it has got. */` |
|     ! 0 |   75 | `		return pDev->iPos - (ph7_int64)StreamAheadBytes(pDev);` |
|       - |   76 | `	}` |
|     131 |   77 | `	iOfft = pDev->pStream->xTell(pDev->pHandle);` |
|     131 |   78 | `	if( iOfft < 0 ){` |
|       8 |   79 | `		return iOfft;` |
|       - |   80 | `	}` |
|     125 |   81 | `	return iOfft - (ph7_int64)StreamAheadBytes(pDev);` |
|      84 |   82 | `}` |
|       - |   83 | `/*` |
|       - |   84 | ` * Seek the stream a php://filter proxy wraps. Same model as fseek() on a` |
|       - |   85 | ` * filtered handle: a relative move is resolved against the position the SCRIPT` |
|       - |   86 | ` * sees, because the device's own offset is not comparable to it.` |
|       - |   87 | ` */` |
|      44 |   88 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence)` |
|       2 |   89 | `{` |
|       - |   90 | `	int rc;` |
|      46 |   91 | `	if( pDev == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xSeek == 0 ){` |
|     ! 0 |   92 | `		return -1;` |
|       - |   93 | `	}` |
|      46 |   94 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|       9 |   95 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|       3 |   96 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|       1 |   97 | `		}` |
|       9 |   98 | `		whence = 0; /* SEEK_SET */` |
|       4 |   99 | `	}` |
|      46 |  100 | `	rc = pDev->pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      46 |  101 | `	if( rc == PH7_OK ){` |
|      46 |  102 | `		SyBlobReset(&pDev->sBuffer);` |
|      46 |  103 | `		pDev->nOfft = 0;` |
|      46 |  104 | `		SyBlobReset(&pDev->sFilt);` |
|      46 |  105 | `		pDev->nFiltOfft = 0;` |
|      46 |  106 | `		pDev->bFiltDone = 0;` |
|      46 |  107 | `		pDev->bEof = 0;` |
|      46 |  108 | `		PH7_StreamFilterRewound(pDev);` |
|      46 |  109 | `		pDev->iFiltPos = whence == 0 ? iOfft` |
|      22 |  110 | `			: (pDev->pStream->xTell ? pDev->pStream->xTell(pDev->pHandle) : 0);` |
|      22 |  111 | `	}` |
|      46 |  112 | `	return rc;` |
|      24 |  113 | `}` |
|     260 |  114 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev)` |
|       3 |  115 | `{` |
|     263 |  116 | `	if( pDev && pDev->pStream && is_php_stream(pDev->pStream) ){` |
|      96 |  117 | `		io_private *pInner = PH7_PhpStreamInner(pDev->pHandle);` |
|      96 |  118 | `		if( pInner ){` |
|      15 |  119 | `			return pInner;` |
|       - |  120 | `		}` |
|      38 |  121 | `	}` |
|     249 |  122 | `	return pDev;` |
|     130 |  123 | `}` |
|       - |  124 | `/*` |
|       - |  125 | `` * Can this handle take bytes at all? php answers `Stream is not writable` -- an`` |
|       - |  126 | ` * E_NOTICE naming the caller -- for a stream whose ops carry no writer: a` |
|       - |  127 | ` * directory handle, data://, glob://. That is a different event from a write` |
|       - |  128 | `` * that WAS attempted and failed (`Write of N bytes failed with errno=9`), which`` |
|       - |  129 | ` * a read-only descriptor produces and which the device path already reports.` |
|       - |  130 | ` */` |
|     691 |  131 | `static int StreamRefuseUnwritable(ph7_context *pCtx,io_private *pDev)` |
|       5 |  132 | `{` |
|     696 |  133 | `	if( pDev && !pDev->bDir && pDev->pStream && pDev->pStream->xWrite ){` |
|     686 |  134 | `		return 0;` |
|       - |  135 | `	}` |
|      11 |  136 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|      11 |  137 | `	return 1;` |
|     327 |  138 | `}` |
|       - |  139 | `/*` |
|       - |  140 | ` * php's stdio device announces a failed WRITE itself -- an E_NOTICE naming the` |
|       - |  141 | ` * caller, the count and the errno -- and only that device does: a write` |
|       - |  142 | ` * php://input or php://output refuses is silent, which is why the two` |
|       - |  143 | ` * whole-file writers cannot simply report every failure they see. They hold a` |
|       - |  144 | ` * bare handle rather than an io_private, so they ask by DEVICE.` |
|       - |  145 | ` */` |
|       4 |  146 | `static void StreamReportRawWriteFailure(ph7_context *pCtx,const ph7_io_stream *pStream,int nLen,int iErr)` |
|       1 |  147 | `{` |
|       5 |  148 | `	if( pStream == 0 \|\| pStream != pCtx->pVm->pDefStream ){` |
|       5 |  149 | `		return;` |
|       - |  150 | `	}` |
|       1 |  151 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     ! 0 |  152 | `		"Write of %d bytes failed with errno=%d %s",nLen,iErr,VfsStrerror(iErr));` |
|       3 |  153 | `}` |
|       - |  154 | `/*` |
|       - |  155 | ` * ...and can it give any? php's readers answer a silent FALSE on a handle whose` |
|       - |  156 | ` * ops carry no reader, with no diagnostic of any kind. A DIRECTORY handle is` |
|       - |  157 | ` * one of those: its pHandle is a DIR*, so a byte op that reached the file` |
|       - |  158 | ` * device would hand an opendir() pointer to read()/lseek()/ftruncate() as if it` |
|       - |  159 | ` * were a descriptor number.` |
|       - |  160 | ` */` |
|   98220 |  161 | `static int StreamHasReader(io_private *pDev)` |
|       5 |  162 | `{` |
|   98225 |  163 | `	return pDev && !pDev->bDir && pDev->pStream && pDev->pStream->xRead;` |
|       5 |  164 | `}` |
|     973 |  165 | `static sxu32 StreamAheadBytes(io_private *pDev)` |
|       5 |  166 | `{` |
|     978 |  167 | `	sxu32 n = 0;` |
|     978 |  168 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      50 |  169 | `		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      23 |  170 | `	}` |
|     978 |  171 | `	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|       5 |  172 | `		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|       2 |  173 | `	}` |
|     978 |  174 | `	return n;` |
|       5 |  175 | `}` |
|       - |  176 | `#ifdef PH7_ENABLE_NET` |
|       - |  177 | `/* The socket handle, declared here because stream_get_meta_data()'s labels ask` |
|       - |  178 | ` * whether a socket has a transport under it. */` |
|       - |  179 | `typedef struct sock_private sock_private;` |
|       - |  180 | `struct sock_private` |
|       - |  181 | `{` |
|       - |  182 | `	ph7_vm *pVm;` |
|       - |  183 | `	ph7_socket sock;` |
|       - |  184 | `	int bEof;` |
|       - |  185 | `	int iLastErr; /* the OS code a failed send left, for php's own notice */` |
|       - |  186 | `	int bGeneric; /* a socketpair: no transport, and php labels it apart */` |
|       - |  187 | `	int bDgram;   /* udp://: a DATAGRAM socket, which php names apart again */` |
|       - |  188 | ``	const char *zLabel; /* an explicit `stream_type`, or 0 to derive it from the two`` |
|       - |  189 | `	                     * flags above. socket_export_stream() states one: php picks` |
|       - |  190 | `	                     * the ops from the DOMAIN as well as the type there, so an` |
|       - |  191 | ``	                     * AF_UNIX datagram socket reports `udg_socket`, a label no`` |
|       - |  192 | `	                     * URI this device stack opens can produce. Static storage. */` |
|       - |  193 | `};` |
|       - |  194 | `#endif` |
|       - |  195 | `/*` |
|       - |  196 | ``  * The dir trio's argument, which php declares `?resource $dir_handle = null` `` |
|       - |  197 | ` * and which therefore has a THIRD case the byte-stream doors do not: given` |
|       - |  198 | `` * nothing (or null), php raises `Passing null is deprecated, instead the last`` |
|       - |  199 | `` * opened directory stream should be provided` and falls back to whatever`` |
|       - |  200 | ` * opendir() handed out last -- and, when there is none or it was closed,` |
|       - |  201 | ` * refuses with a TypeError that names neither the function nor the argument.` |
|       - |  202 | ` */` |
|   19970 |  203 | `static io_private * StreamDirArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int *pRc)` |
|       5 |  204 | `{` |
|       - |  205 | `	io_private *pDev;` |
|   19975 |  206 | `	*pRc = PH7_OK;` |
|   19975 |  207 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      25 |  208 | `		VmErrorFormat(pCtx->pVm,8192 /* E_DEPRECATED */,` |
|       - |  209 | `			"%s(): Passing null is deprecated, instead the last opened "` |
|       8 |  210 | `			"directory stream should be provided",ph7_function_name(pCtx));` |
|      17 |  211 | `		pDev = (io_private *)pCtx->pVm->pLastDir;` |
|      17 |  212 | `		if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) ){` |
|       9 |  213 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError","No resource supplied");` |
|       9 |  214 | `			return 0;` |
|       - |  215 | `		}` |
|       9 |  216 | `		return pDev;` |
|       - |  217 | `	}` |
|   19959 |  218 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - |  219 | `		char zGiven[64];` |
|      16 |  220 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  221 | `			"%s(): Argument #1 ($dir_handle) must be of type resource or null, %s given",` |
|       5 |  222 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      11 |  223 | `		return 0;` |
|       - |  224 | `	}` |
|   19949 |  225 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|   19949 |  226 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      10 |  227 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  228 | `			"%s(): Argument #1 ($dir_handle) must be an open stream resource",` |
|       3 |  229 | `			ph7_function_name(pCtx));` |
|       7 |  230 | `		return 0;` |
|       - |  231 | `	}` |
|   19943 |  232 | `	return pDev;` |
|    9986 |  233 | `}` |
|       - |  234 | `/*` |
|       - |  235 | ` * php's screen for a stream-handle ARGUMENT, forward-declared here because every` |
|       - |  236 | ` * f* door below runs it and it is defined with the settings family further down.` |
|       - |  237 | `` * It answers php's TWO TypeErrors -- `Argument #N ($stream) must be of type`` |
|       - |  238 | `` * resource, X given` for something that was never a handle, and `...must be an`` |
|       - |  239 | `` * open stream resource` for a resource whose device is gone, which is what an`` |
|       - |  240 | ` * already-CLOSED handle is -- and 0, with *pRc carrying the throw.` |
|       - |  241 | ` *` |
|       - |  242 | `` * Both used to be one legacy warning ("Expecting an IO handle") and a `false`,`` |
|       - |  243 | ` * at ~24 doors. The name and position are the DOOR's: php calls the same` |
|       - |  244 | `` * argument `$stream` for the byte-stream verbs, `$dir_handle` for the directory`` |
|       - |  245 | `` * trio and `$handle` for pclose(), and the function in the message is the one`` |
|       - |  246 | ` * that was CALLED -- which is how gzread()'s refusal says gzread() and not` |
|       - |  247 | ` * fread(), the same body under another name.` |
|       - |  248 | ` */` |
|       - |  249 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - |  250 | `	const char *zName,int *pRc);` |
|       - |  251 | `/*` |
|       - |  252 | ` * Return the PHP resource-type name for a raw resource handle.` |
|       - |  253 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|       - |  254 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|       - |  255 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|       - |  256 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|       - |  257 | ` */` |
|      90 |  258 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|       4 |  259 | `{` |
|      94 |  260 | `	io_private *pDev = (io_private *)pResource;` |
|      94 |  261 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|       - |  262 | `		/* php names a persistent stream apart, and that name is the only way a` |
|       - |  263 | `		 * script can see that its handle is one. */` |
|      48 |  264 | `		return pDev->bPersist ? "persistent stream" : "stream";` |
|       - |  265 | `	}` |
|      48 |  266 | `	if( pDev && pDev->iMagic == PROC_PRIVATE_MAGIC ){` |
|       - |  267 | `		/* proc_open()'s handle is not a stream and php does not call it one: it` |
|       - |  268 | `` 		 * answered "Unknown" here, so `get_resource_type($proc) === 'process'` `` |
|       - |  269 | `		 * — the documented way to tell a process handle from a pipe — was` |
|       - |  270 | `		 * false. Its header IS an io_private, magic field included, which is` |
|       - |  271 | `		 * what one probe can tell them apart by. */` |
|       4 |  272 | `		return "process";` |
|       - |  273 | `	}` |
|      44 |  274 | `	if( pDev && pDev->iMagic == STREAM_CTX_MAGIC ){` |
|       - |  275 | `		/* stream_context_create()'s handle, and the name php gives it. */` |
|      22 |  276 | `		return "stream-context";` |
|       - |  277 | `	}` |
|      24 |  278 | `	if( pDev && pDev->iMagic == STREAM_BUCKET_MAGIC ){` |
|       - |  279 | `		/* The handle a StreamBucket carries; php shows one there. */` |
|       5 |  280 | `		return "userfilter.bucket";` |
|       - |  281 | `	}` |
|      20 |  282 | `	if( pDev && pDev->iMagic == STREAM_BRIGADE_MAGIC ){` |
|       - |  283 | ``		/* The `$in` and `$out` a userland filter() is handed. */`` |
|       3 |  284 | `		return "userfilter.bucket brigade";` |
|       - |  285 | `	}` |
|      18 |  286 | `	if( pDev && pDev->iMagic == STREAM_FILTER_MAGIC ){` |
|       - |  287 | `		/* stream_filter_append()'s handle. Note the SPACE: php names the context` |
|       - |  288 | ``		 * `stream-context` and the filter `stream filter`. */`` |
|       5 |  289 | `		return "stream filter";` |
|       - |  290 | `	}` |
|       - |  291 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|       - |  292 | `	{` |
|       - |  293 | `		/* ext/zip's two procedural handles, which are not streams at all:` |
|       - |  294 | ``		 * `zip_open()` hands out a `Zip Directory` and `zip_read()` a`` |
|       - |  295 | ``		 * `Zip Entry`, and get_resource_type() is the only way a script tells`` |
|       - |  296 | `		 * one from the other. */` |
|      14 |  297 | `		const char *zZip = PH7_ZipResourceType(pResource);` |
|      14 |  298 | `		if( zZip ){` |
|      11 |  299 | `			return zZip;` |
|       - |  300 | `		}` |
|       - |  301 | `	}` |
|       - |  302 | `#endif` |
|       3 |  303 | `	return "Unknown";` |
|      49 |  304 | `}` |
|       - |  305 | `/*` |
|       - |  306 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|       - |  307 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|       - |  308 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|       - |  309 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|       - |  310 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|       - |  311 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|       - |  312 | ` */` |
|     616 |  313 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|       5 |  314 | `{` |
|     621 |  315 | `	io_private *pDev = (io_private *)pResource;` |
|     621 |  316 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|       5 |  317 | `}` |
|       - |  318 | `/*` |
|       - |  319 | ` * bool ftruncate(resource $handle,int64 $size)` |
|       - |  320 | ` *  Truncates a file to a given length.` |
|       - |  321 | ` * Parameters` |
|       - |  322 | ` *  $handle` |
|       - |  323 | ` *   The file pointer.` |
|       - |  324 | ` *   Note:` |
|       - |  325 | ` *    The handle must be open for writing.` |
|       - |  326 | ` * $size` |
|       - |  327 | ` *   The size to truncate to.` |
|       - |  328 | ` * Return` |
|       - |  329 | ` *  TRUE on success or FALSE on failure.` |
|       - |  330 | ` */` |
|      32 |  331 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  332 | `{` |
|       - |  333 | `	const ph7_io_stream *pStream;` |
|       - |  334 | `	io_private *pDev;` |
|       - |  335 | `	ph7_int64 nSize;` |
|       - |  336 | `	int rc;` |
|      34 |  337 | `	if( nArg < 2 ){` |
|       - |  338 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  339 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  340 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  341 | `		return PH7_OK;` |
|       - |  342 | `	}` |
|       - |  343 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      34 |  344 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      34 |  345 | `	if( pDev == 0 ){` |
|       3 |  346 | `		return rc;` |
|       - |  347 | `	}` |
|      31 |  348 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|      31 |  349 | `	if( nSize < 0 ){` |
|       - |  350 | `		/* php 8: catchable ValueError, raised BEFORE the unsupported-stream` |
|       - |  351 | `		 * check (php-src orders the size check first). PHL used to truncate. */` |
|       3 |  352 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  353 | `			"ftruncate(): Argument #2 ($size) must be greater than or equal to 0");` |
|       - |  354 | `	}` |
|       - |  355 | `	/* Point to the target IO stream device */` |
|      29 |  356 | `	pStream = pDev->pStream;` |
|      29 |  357 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xTrunc == 0 ){` |
|       - |  358 | `		/* php asks the stream whether it supports truncation AT ALL and says so` |
|       - |  359 | `		 * when it does not -- a socket, php://output, an http:// body, a` |
|       - |  360 | `		 * directory handle. What it says is this sentence, and it is a` |
|       - |  361 | `		 * different event from a truncation that was tried and refused. */` |
|       3 |  362 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Can\'t truncate this stream!");` |
|       3 |  363 | `		ph7_result_bool(pCtx,0);` |
|       3 |  364 | `		return PH7_OK;` |
|       - |  365 | `	}` |
|       - |  366 | `	/* Perform the requested operation */` |
|      27 |  367 | `	rc = pStream->xTrunc(pDev->pHandle,nSize);` |
|      27 |  368 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       5 |  369 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Can\'t truncate this stream!");` |
|       5 |  370 | `		ph7_result_bool(pCtx,0);` |
|       5 |  371 | `		return PH7_OK;` |
|       - |  372 | `	}` |
|       - |  373 | `	/* php does NOT touch the read buffer here: truncating is not a seek, the` |
|       - |  374 | `	 * position does not move, and what the readers already pulled ahead is` |
|       - |  375 | `	 * still what the next read answers. Dropping it made ftell() jump to the` |
|       - |  376 | `	 * device's own offset and the next read start there — past the new end` |
|       - |  377 | ``	 * (`""` where php answers the buffered line) or, after a truncation that`` |
|       - |  378 | `	 * GREW the file, over the NUL padding no php ever hands back. */` |
|       - |  379 | `	/* IO result */` |
|      23 |  380 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      23 |  381 | `	return PH7_OK;` |
|      18 |  382 | `}` |
|       - |  383 | `/*` |
|       - |  384 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|       - |  385 | ` *  Seeks on a file pointer.` |
|       - |  386 | ` * Parameters` |
|       - |  387 | ` *  $handle` |
|       - |  388 | ` *   A file system pointer resource that is typically created using fopen().` |
|       - |  389 | ` * $offset` |
|       - |  390 | ` *   The offset.` |
|       - |  391 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|       - |  392 | ` *   value in offset and set whence to SEEK_END.` |
|       - |  393 | ` *   whence` |
|       - |  394 | ` *   whence values are:` |
|       - |  395 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|       - |  396 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|       - |  397 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|       - |  398 | ` * Return` |
|       - |  399 | ` *  0 on success,-1 on failure` |
|       - |  400 | ` */` |
|     156 |  401 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  402 | `{` |
|       - |  403 | `	const ph7_io_stream *pStream;` |
|       - |  404 | `	io_private *pDev;` |
|       - |  405 | `	ph7_int64 iOfft;` |
|       - |  406 | `	int whence;` |
|       - |  407 | `	int rc;` |
|     160 |  408 | `	if( nArg < 2 ){` |
|       - |  409 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  410 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  411 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 |  412 | `		return PH7_OK;` |
|       - |  413 | `	}` |
|       - |  414 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     160 |  415 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     160 |  416 | `	if( pDev == 0 ){` |
|       5 |  417 | `		return rc;` |
|       - |  418 | `	}` |
|       - |  419 | `	/* Point to the target IO stream device */` |
|     156 |  420 | `	pStream = pDev->pStream;` |
|     156 |  421 | `	if( !pDev->bDir && (pStream == 0 \|\| pStream->xSeek == 0) ){` |
|       3 |  422 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       3 |  423 | `		ph7_result_int(pCtx,-1);` |
|       3 |  424 | `		return PH7_OK;` |
|       - |  425 | `	}` |
|       - |  426 | `	/* Extract the offset */` |
|     153 |  427 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     153 |  428 | `	whence = 0;/* SEEK_SET */` |
|     153 |  429 | `	if( nArg > 2 ){` |
|       - |  430 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|       - |  431 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|       - |  432 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|       - |  433 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|      58 |  434 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|      28 |  435 | `	}` |
|     153 |  436 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|       - |  437 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|       - |  438 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|       - |  439 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|       - |  440 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|       - |  441 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|      13 |  442 | `		ph7_result_int(pCtx,-1);` |
|      13 |  443 | `		return PH7_OK;` |
|       - |  444 | `	}` |
|     141 |  445 | `	if( pDev->bDir ){` |
|       - |  446 | `		/* php's directory stream seeks by REWINDING, whatever the offset and` |
|       - |  447 | `		 * whatever the whence: it answers 0, the next readdir() is the first` |
|       - |  448 | `		 * entry again, and the position it REPORTS does not go back with it. */` |
|       3 |  449 | `		if( pStream && pStream->xRewindDir ){` |
|       3 |  450 | `			pStream->xRewindDir(pDev->pHandle);` |
|       1 |  451 | `		}` |
|       3 |  452 | `		ph7_result_int(pCtx,0);` |
|       3 |  453 | `		return PH7_OK;` |
|       - |  454 | `	}` |
|     139 |  455 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|       - |  456 | `		/* On a FILTERED stream the two positions are unrelated, so a relative` |
|       - |  457 | `		 * seek is resolved against the one the script sees and the device is` |
|       - |  458 | `		 * then placed at the result — php's own model, and the only one under` |
|       - |  459 | ``		 * which `fseek($f,0,SEEK_CUR)` is the no-op it looks like. */`` |
|       7 |  460 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|       5 |  461 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|       2 |  462 | `		}` |
|       7 |  463 | `		rc = pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|       7 |  464 | `		if( rc == PH7_OK ){` |
|       7 |  465 | `			ResetIOPrivate(pDev);` |
|       7 |  466 | `			pDev->iFiltPos = iOfft;` |
|       3 |  467 | `		}else if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 |  468 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|     ! 0 |  469 | `		}` |
|       7 |  470 | `		ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|       7 |  471 | `		return PH7_OK;` |
|       - |  472 | `	}` |
|     133 |  473 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|       - |  474 | `		/* The CURRENT position is the LOGICAL one: the device sits past the` |
|       - |  475 | `		 * read-ahead the line readers buffer, so seek relative to where the` |
|       - |  476 | `		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without` |
|       - |  477 | `		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead` |
|       - |  478 | `		 * skipped everything still sitting in the buffer. */` |
|      12 |  479 | `		iOfft -= (ph7_int64)StreamAheadBytes(pDev);` |
|       5 |  480 | `	}` |
|       - |  481 | `	/* Perform the requested operation */` |
|     133 |  482 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     133 |  483 | `	if( rc == PH7_OK ){` |
|       - |  484 | `		/* Ignore buffered data */` |
|     123 |  485 | `		ResetIOPrivate(pDev);` |
|     123 |  486 | `		if( pDev->pReadFilters ){` |
|     ! 0 |  487 | `			pDev->iFiltPos = pStream->xTell ? pStream->xTell(pDev->pHandle) : 0;` |
|       3 |  488 | `		}` |
|      72 |  489 | `	}else if( rc == SXERR_NOTIMPLEMENTED ){` |
|       - |  490 | `		/* The device HAS a seek and this handle cannot use it -- php's` |
|       - |  491 | `		 * php://stdout on a pipe or a terminal. Same sentence as no seek. */` |
|       3 |  492 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       1 |  493 | `	}` |
|       - |  494 | `	/* IO result */` |
|     133 |  495 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     133 |  496 | `	return PH7_OK;` |
|      82 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * int64 ftell(resource $handle)` |
|       - |  500 | ` *  Returns the current position of the file read/write pointer.` |
|       - |  501 | ` * Parameters` |
|       - |  502 | ` *  $handle` |
|       - |  503 | ` *   The file pointer.` |
|       - |  504 | ` * Return` |
|       - |  505 | ` *  Returns the position of the file pointer referenced by handle` |
|       - |  506 | ` *  as an integer; i.e., its offset into the file stream.` |
|       - |  507 | ` *  FALSE is returned on failure.` |
|       - |  508 | ` */` |
|     140 |  509 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  510 | `{` |
|       - |  511 | `	io_private *pDev;` |
|       - |  512 | `	ph7_int64 iOfft;` |
|       - |  513 | `	int rc;` |
|     145 |  514 | `	if( nArg < 1 ){` |
|       - |  515 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  516 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  517 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  518 | `		return PH7_OK;` |
|       - |  519 | `	}` |
|       - |  520 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     145 |  521 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     145 |  522 | `	if( pDev == 0 ){` |
|       5 |  523 | `		return rc;` |
|       - |  524 | `	}` |
|       - |  525 | `	/* Point to the target IO stream device */` |
|       - |  526 | `	/* No "unimplemented" arm: php's ftell() never refuses a stream. A device` |
|       - |  527 | `	 * that cannot be asked where it is has the engine's own counter answer` |
|       - |  528 | `	 * for it (PH7_StreamLogicalTell). */` |
|       - |  529 | `	/* Perform the requested operation. The device sits past whatever the line` |
|       - |  530 | `	 * readers buffered ahead, so the SCRIPT's position is the device position` |
|       - |  531 | `	 * less the unconsumed remainder — ftell() after fgets("abcdefghij\nrest")` |
|       - |  532 | `	 * is php's 11, not the 15 the device already read. */` |
|     141 |  533 | `	iOfft = PH7_StreamLogicalTell(pDev);` |
|     141 |  534 | `	if( iOfft < 0 ){` |
|       - |  535 | `		/* The device does not know where it is -- php's answer for a stream` |
|       - |  536 | `		 * whose last seek FAILED (PDO's blob handle refuses one past its own` |
|       - |  537 | `		 * end and leaves the position unknown until a seek succeeds). */` |
|       8 |  538 | `		ph7_result_bool(pCtx,0);` |
|       8 |  539 | `		return PH7_OK;` |
|       - |  540 | `	}` |
|       - |  541 | `	/* IO result */` |
|     135 |  542 | `	ph7_result_int64(pCtx,iOfft);` |
|     135 |  543 | `	return PH7_OK;` |
|      75 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * bool rewind(resource $handle)` |
|       - |  547 | ` *  Rewind the position of a file pointer.` |
|       - |  548 | ` * Parameters` |
|       - |  549 | ` *  $handle` |
|       - |  550 | ` *   The file pointer.` |
|       - |  551 | ` * Return` |
|       - |  552 | ` *  TRUE on success or FALSE on failure.` |
|       - |  553 | ` */` |
|     324 |  554 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  555 | `{` |
|       - |  556 | `	const ph7_io_stream *pStream;` |
|       - |  557 | `	io_private *pDev;` |
|       - |  558 | `	int rc;` |
|     328 |  559 | `	if( nArg < 1 ){` |
|       - |  560 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  561 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  562 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  563 | `		return PH7_OK;` |
|       - |  564 | `	}` |
|       - |  565 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     328 |  566 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     328 |  567 | `	if( pDev == 0 ){` |
|       5 |  568 | `		return rc;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* Point to the target IO stream device */` |
|     324 |  571 | `	pStream = pDev->pStream;` |
|     324 |  572 | `	if( pDev->bDir ){` |
|       - |  573 | `		/* Same rewind fseek() gets, and php's rewind() answers TRUE for it. */` |
|       3 |  574 | `		if( pStream && pStream->xRewindDir ){` |
|       3 |  575 | `			pStream->xRewindDir(pDev->pHandle);` |
|       1 |  576 | `		}` |
|       3 |  577 | `		ph7_result_bool(pCtx,1);` |
|       3 |  578 | `		return PH7_OK;` |
|       - |  579 | `	}` |
|     322 |  580 | `	if( pStream == 0 \|\| pStream->xSeek == 0 ){` |
|     ! 0 |  581 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|     ! 0 |  582 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  583 | `		return PH7_OK;` |
|       - |  584 | `	}` |
|       - |  585 | `	/* Perform the requested operation */` |
|     322 |  586 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     322 |  587 | `	if( rc == PH7_OK ){` |
|       - |  588 | `		/* Ignore buffered data */` |
|     320 |  589 | `		ResetIOPrivate(pDev);` |
|     161 |  590 | `	}else if( rc == SXERR_NOTIMPLEMENTED ){` |
|       3 |  591 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       1 |  592 | `	}` |
|       - |  593 | `	/* IO result */` |
|     322 |  594 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     322 |  595 | `	return PH7_OK;` |
|     166 |  596 | `}` |
|       - |  597 | `/*` |
|       - |  598 | ` * bool fflush(resource $handle)` |
|       - |  599 | ` *  Flushes the output to a file.` |
|       - |  600 | ` * Parameters` |
|       - |  601 | ` *  $handle` |
|       - |  602 | ` *   The file pointer.` |
|       - |  603 | ` * Return` |
|       - |  604 | ` *  TRUE on success or FALSE on failure.` |
|       - |  605 | ` */` |
|      18 |  606 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  607 | `{` |
|       - |  608 | `	const ph7_io_stream *pStream;` |
|       - |  609 | `	io_private *pDev;` |
|       - |  610 | `	int rc;` |
|      20 |  611 | `	if( nArg < 1 ){` |
|       - |  612 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  613 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  614 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  615 | `		return PH7_OK;` |
|       - |  616 | `	}` |
|       - |  617 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      20 |  618 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      20 |  619 | `	if( pDev == 0 ){` |
|       3 |  620 | `		return rc;` |
|       - |  621 | `	}` |
|       - |  622 | `	/* Point to the target IO stream device */` |
|      17 |  623 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      17 |  624 | `	pStream = pDev->pStream;` |
|       - |  625 | `	/* The chain first, and whatever the device can do about it second. */` |
|      17 |  626 | `	PH7_StreamFlushWriteChain(pDev);` |
|      17 |  627 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xSync == 0 ){` |
|       - |  628 | `		/* php's php_stream_flush SUCCEEDS when the device has nothing to flush,` |
|       - |  629 | `		 * silently -- which is why fflush() on php://memory, php://output, a` |
|       - |  630 | `		 * pipe, a socket or a directory handle is simply TRUE. Every` |
|       - |  631 | `		 * symfony/console write ends in one of these. */` |
|       5 |  632 | `		ph7_result_bool(pCtx,1);` |
|       5 |  633 | `		return PH7_OK;` |
|       - |  634 | `	}` |
|       - |  635 | `	/* Perform the requested operation */` |
|      13 |  636 | `	rc = pStream->xSync(pDev->pHandle);` |
|       - |  637 | `	/* IO result */` |
|      13 |  638 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      13 |  639 | `	return PH7_OK;` |
|      11 |  640 | `}` |
|       - |  641 | `/*` |
|       - |  642 | ` * php's end-of-file flag is set AFTER THE FACT: a stream is at EOF once one of` |
|       - |  643 | ` * its OWN reads has come back empty, and asking the question never reads. PHL` |
|       - |  644 | ` * used to probe the device instead — a read-ahead of up to 4 KB from inside` |
|       - |  645 | ` * feof() — which answered TRUE on a handle nothing had read yet (an empty file,` |
|       - |  646 | ` * a fresh php://memory), answered TRUE on a WRITE-only handle because the` |
|       - |  647 | `` * refused read looked like an end, and BLOCKED on `feof(STDIN)` with no input`` |
|       - |  648 | ` * waiting: a question about a stream is not a read of it. bEof is that flag,` |
|       - |  649 | ` * set wherever a read here comes back with nothing and cleared by every seek.` |
|       - |  650 | ` */` |
|       - |  651 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer);` |
|   43481 |  652 | `static int StreamEofCommon(io_private *pDev,int bLive)` |
|       5 |  653 | `{` |
|       - |  654 | `	int bEof;` |
|   43486 |  655 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - |  656 | `		/* Buffered bytes are not an end. */` |
|   33444 |  657 | `		return 0;` |
|       - |  658 | `	}` |
|   10047 |  659 | `	if( IoPrivateUwrapEof(pDev,&bEof) ){` |
|       - |  660 | `		/* A userland wrapper answers the question itself — php calls its` |
|       - |  661 | `		 * streamWrapper::stream_eof() rather than inferring anything. */` |
|       8 |  662 | `		return bEof;` |
|       - |  663 | `	}` |
|       - |  664 | `#ifdef PH7_ENABLE_NET` |
|   10041 |  665 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - |  666 | `		/* The http handle owns the end: the socket may have closed while its` |
|       - |  667 | `		 * dechunker still holds bytes nobody has taken. */` |
|      32 |  668 | `		return PH7_HttpStreamAtEof(pDev->pHandle);` |
|       - |  669 | `	}` |
|   10009 |  670 | `	if( bLive && !pDev->bEof && pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|       - |  671 | `		/* php asks the SOCKET whether it is still alive rather than remembering` |
|       - |  672 | `		 * a read that came back empty, and LATCHES a "no" — which is why the` |
|       - |  673 | ``		 * `eof` key of stream_get_meta_data() reads true only after a feof()`` |
|       - |  674 | `		 * has run, and false before one. */` |
|      89 |  675 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|      89 |  676 | `		if( !PH7_NetIsAlive(pSock->sock) ){` |
|      24 |  677 | `			pSock->bEof = 1;` |
|      24 |  678 | `			pDev->bEof = 1;` |
|      18 |  679 | `		}` |
|      43 |  680 | `	}` |
|       - |  681 | `#else` |
|       - |  682 | `	SXUNUSED(bLive);` |
|       - |  683 | `#endif` |
|   10009 |  684 | `	return pDev->bEof != 0;` |
|   21436 |  685 | `}` |
|       - |  686 | `/*` |
|       - |  687 | ` * php's php_stream_eof(): the question feof() asks, which for a socket PROBES` |
|       - |  688 | ` * the descriptor (see PH7_NetIsAlive) and latches what it finds.` |
|       - |  689 | ` */` |
|   43363 |  690 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev)` |
|       5 |  691 | `{` |
|   43368 |  692 | `	return StreamEofCommon(pDev,1);` |
|       5 |  693 | `}` |
|       - |  694 | `/*` |
|       - |  695 | ` * bool feof(resource $handle)` |
|       - |  696 | ` *  Tests for end-of-file on a file pointer.` |
|       - |  697 | ` * Parameters` |
|       - |  698 | ` *  $handle` |
|       - |  699 | ` *   The file pointer.` |
|       - |  700 | ` * Return` |
|       - |  701 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|       - |  702 | ` */` |
|   43071 |  703 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  704 | `{` |
|       - |  705 | `	io_private *pDev;` |
|       - |  706 | `	int rc;` |
|   43076 |  707 | `	if( nArg < 1 ){` |
|       - |  708 | `		/* Missing/Invalid arguments */` |
|     ! 0 |  709 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  710 | `		ph7_result_bool(pCtx,1);` |
|     ! 0 |  711 | `		return PH7_OK;` |
|       - |  712 | `	}` |
|       - |  713 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|   43076 |  714 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   43076 |  715 | `	if( pDev == 0 ){` |
|      17 |  716 | `		return rc;` |
|       - |  717 | `	}` |
|   43060 |  718 | `	if( !StreamHasReader(pDev) ){` |
|       - |  719 | `		/* php's end-of-file flag is raised by a READ that came back empty, and` |
|       - |  720 | `		 * a handle nothing can read never had one: feof() is FALSE, silently. */` |
|       3 |  721 | `		ph7_result_bool(pCtx,0);` |
|       3 |  722 | `		return PH7_OK;` |
|       - |  723 | `	}` |
|   43058 |  724 | `	rc = PH7_StreamAtEof(pDev);` |
|       - |  725 | `	/* EOF or not */` |
|   43058 |  726 | `	ph7_result_bool(pCtx,rc != 0);` |
|   43058 |  727 | `	return PH7_OK;` |
|   21231 |  728 | `}` |
|       - |  729 | `/*` |
|       - |  730 | ` * Read n bytes from the underlying IO stream device.` |
|       - |  731 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|       - |  732 | ` * [i.e: IO error ] or EOF.` |
|       - |  733 | ` *` |
|       - |  734 | ` * This is the read every SCRIPT-level reader goes through, because it drains` |
|       - |  735 | ` * the line readers' read-ahead buffer first: a stream that fgets() has already` |
|       - |  736 | ` * pulled a block out of is positioned where the SCRIPT thinks it is, not where` |
|       - |  737 | ` * the device is. Anything reading from a caller's handle has to use this and` |
|       - |  738 | ` * not the device's own xRead.` |
|       - |  739 | ` */` |
|       - |  740 | `/*` |
|       - |  741 | ` * One read from the device, with the timeout bookkeeping php does for EVERY` |
|       - |  742 | `` * reader: `timed_out` describes the last read, so it is cleared on the way in`` |
|       - |  743 | ` * and set only by a wait that expired. Without the clear, one quiet period marks` |
|       - |  744 | ` * a handle timed out for the rest of its life — and now that every socket` |
|       - |  745 | ` * carries default_socket_timeout, that is every socket that ever waited. And` |
|       - |  746 | ` * without the set being here, only fread() would ever report one: fgets(),` |
|       - |  747 | ` * fgetc(), stream_get_line(), stream_get_contents() and fpassthru() all read` |
|       - |  748 | ` * through their own loops.` |
|       - |  749 | ` */` |
|   12972 |  750 | `static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |  751 | `{` |
|       - |  752 | `	ph7_int64 n;` |
|   12977 |  753 | `	pDev->bTimedOut = 0;` |
|   12977 |  754 | `	errno = 0;` |
|   12977 |  755 | `	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);` |
|   12972 |  756 | `	if( n < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK)` |
|      34 |  757 | `	 && pDev->bHasTimeout && !pDev->bNonBlock ){` |
|       5 |  758 | `		pDev->bTimedOut = 1;` |
|       2 |  759 | `	}` |
|   12977 |  760 | `	return n;` |
|       5 |  761 | `}` |
|       - |  762 | `/*` |
|       - |  763 | ` * Serve a read from the FILTERED side of a handle. A filter changes the byte` |
|       - |  764 | ` * count — base64 makes four out of three, dechunk throws whole runs away — so` |
|       - |  765 | ` * what the chain produced cannot go straight into the caller's buffer: it waits` |
|       - |  766 | ` * in sFilt and is handed out from there.` |
|       - |  767 | ` *` |
|       - |  768 | ` * The fill loop runs until sFilt holds what was asked for or the device is` |
|       - |  769 | ` * spent, which is what keeps the caller's invariant intact: a SHORT answer here` |
|       - |  770 | ` * still means end of file, exactly as it does for an unfiltered read.` |
|       - |  771 | ` */` |
|     304 |  772 | `static ph7_int64 IoPrivateFilteredRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       3 |  773 | `{` |
|     307 |  774 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pReadFilters;` |
|       - |  775 | `	sxu32 nAvail;` |
|       - |  776 | `	ph7_int64 n;` |
|       - |  777 |  |
|    2380 |  778 | `	while( pChain != 0 && !pDev->bFiltDone` |
|    2231 |  779 | `	    && (ph7_int64)(SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft) < nLen ){` |
|       - |  780 | `		char zRaw[8192];` |
|    1373 |  781 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zRaw);` |
|       - |  782 | `		ph7_int64 nRaw;` |
|       - |  783 | `		int iStatus;` |
|    1373 |  784 | `		if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|    1099 |  785 | `			nAsk = (ph7_int64)pDev->nChunk;` |
|     549 |  786 | `		}` |
|    1373 |  787 | `		nRaw = IoPrivateRawRead(pDev,zRaw,nAsk);` |
|    1373 |  788 | `		if( nRaw < 0 ){` |
|     ! 0 |  789 | `			if( SyBlobLength(&pDev->sFilt) <= pDev->nFiltOfft ){` |
|       - |  790 | `				/* Nothing was ever produced: the IO error is the answer. */` |
|     ! 0 |  791 | `				return nRaw;` |
|       - |  792 | `			}` |
|     ! 0 |  793 | `			break;` |
|       - |  794 | `		}` |
|       - |  795 | `		{` |
|    1373 |  796 | `			int iF = nRaw > 0 ? PHL_PSFS_FLAG_NORMAL : PHL_PSFS_FLAG_FLUSH_CLOSE;` |
|       - |  797 | `			/* The device's end closes EVERY filter on the stream, not just the` |
|       - |  798 | `			 * head: each one's tail has to travel through the rest. */` |
|    1373 |  799 | `			iStatus = PH7_FilterChainProcess(pChain,zRaw,(sxu32)nRaw,iF,iF,&pDev->sFilt,0);` |
|       - |  800 | `		}` |
|    1373 |  801 | `		if( nRaw == 0 ){` |
|       - |  802 | `			/* The device is spent, and the call above was the chain's CLOSING` |
|       - |  803 | `			 * one: running it again would make a buffering filter emit its tail` |
|       - |  804 | `			 * twice, so the chain is finished for good. */` |
|     143 |  805 | `			pDev->bFiltDone = 1;` |
|     143 |  806 | `			break;` |
|       - |  807 | `		}` |
|    1233 |  808 | `		if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|       - |  809 | `			/* A refusal ends the reading. php reports it to the reader as a` |
|       - |  810 | ``			 * FAILURE — `fread()` answers false, once — and only then as an end`` |
|       - |  811 | `			 * of file; what earlier calls already produced is still the` |
|       - |  812 | `			 * reader's, so the failure waits behind it. */` |
|       5 |  813 | `			pDev->bFiltDone = 1;` |
|       5 |  814 | `			pDev->bFiltErr = 1;` |
|       5 |  815 | `			break;` |
|       - |  816 | `		}` |
|       3 |  817 | `	}` |
|     307 |  818 | `	nAvail = SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|     307 |  819 | `	if( nAvail < 1 ){` |
|     143 |  820 | `		SyBlobReset(&pDev->sFilt);` |
|     143 |  821 | `		pDev->nFiltOfft = 0;` |
|     143 |  822 | `		if( pDev->bFiltErr ){` |
|       5 |  823 | `			pDev->bFiltErr = 0;   /* reported once; the read after it is an end */` |
|       5 |  824 | `			return -1;` |
|       - |  825 | `		}` |
|     139 |  826 | `		return pChain != 0 ? 0 : IoPrivateRawRead(pDev,pBuf,nLen);` |
|       - |  827 | `	}` |
|     167 |  828 | `	n = (ph7_int64)nAvail;` |
|     167 |  829 | `	if( n > nLen ){` |
|      25 |  830 | `		n = nLen;` |
|      12 |  831 | `	}` |
|     167 |  832 | `	SyMemcpy(SyBlobDataAt(&pDev->sFilt,pDev->nFiltOfft),pBuf,(sxu32)n);` |
|     167 |  833 | `	pDev->nFiltOfft += (sxu32)n;` |
|     167 |  834 | `	pDev->iFiltPos += n;` |
|     167 |  835 | `	if( pDev->nFiltOfft >= SyBlobLength(&pDev->sFilt) ){` |
|     143 |  836 | `		SyBlobReset(&pDev->sFilt);` |
|     143 |  837 | `		pDev->nFiltOfft = 0;` |
|      70 |  838 | `	}` |
|     167 |  839 | `	return n;` |
|     155 |  840 | `}` |
|   11906 |  841 | `static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |  842 | `{` |
|       - |  843 | `	ph7_int64 n;` |
|   11911 |  844 | `	if( !StreamHasReader(pDev) ){` |
|       - |  845 | `		/* No reader at all: php's silent false, and never the device's byte op` |
|       - |  846 | `		 * on a handle it does not own. */` |
|     ! 0 |  847 | `		return -1;` |
|       - |  848 | `	}` |
|   11911 |  849 | `	if( pDev->pReadFilters != 0 \|\| SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|       - |  850 | `		/* Bytes can still be waiting after the last read filter was REMOVED:` |
|       - |  851 | `		 * php flushes a filter on its way out and what it emitted belongs to` |
|       - |  852 | `		 * the reader that comes next. */` |
|     307 |  853 | `		return IoPrivateFilteredRead(pDev,pBuf,nLen);` |
|       - |  854 | `	}` |
|   11607 |  855 | `	errno = 0;` |
|   11607 |  856 | `	n = IoPrivateRawRead(pDev,pBuf,nLen);` |
|   11602 |  857 | `	if( n > 0 && is_php_stream(pDev->pStream)` |
|    2387 |  858 | `	 && PH7_PhpStreamTempDrained(pDev->pHandle) ){` |
|       - |  859 | `		/* php's php://temp raises its end flag as soon as a read has consumed` |
|       - |  860 | `		 * the buffer, one read before php://memory does. feof() still answers` |
|       - |  861 | `		 * false while the line readers hold bytes -- PH7_StreamAtEof() asks the` |
|       - |  862 | `		 * buffer first, which is php's own rule. */` |
|       5 |  863 | `		pDev->bEof = 1;` |
|       2 |  864 | `	}` |
|   11607 |  865 | `	if( n > 0 ){` |
|       - |  866 | `		/* Where the SCRIPT will be once it has taken these bytes: the counter` |
|       - |  867 | `		 * ftell() reads on a device that cannot be asked. */` |
|    4583 |  868 | `		pDev->iPos += n;` |
|    2289 |  869 | `	}` |
|   11607 |  870 | `	if( n < 0 ){` |
|       - |  871 | `		/* LATCH the failure for the reader to report. php's notice comes from` |
|       - |  872 | `		 * the stream op, which knows the errno but not which builtin is asking;` |
|       - |  873 | `		 * here the builtin knows how to report and the device knows why, so the` |
|       - |  874 | `		 * two meet at the latch -- the same shape the socket write already uses.` |
|       - |  875 | `		 * Cleared by whoever reports it, so one failure is announced once.` |
|       - |  876 | `		 *` |
|       - |  877 | `		 * EAGAIN is not a failure: on a NON-BLOCKING stream it means "nothing to` |
|       - |  878 | `		 * read right now", which php answers with an empty read and no diagnostic` |
|       - |  879 | `		 * at all (its read op only reports when the operation itself failed). An` |
|       - |  880 | `		 * EINTR read is the same "try again". Latching either made` |
|       - |  881 | ``		 * `stream_get_contents()` on a non-blocking proc_open() pipe -- the`` |
|       - |  882 | `		 * everyday shape, and what monolog's ProcessHandler does on every write --` |
|       - |  883 | `		 * raise a notice php never raises. */` |
|      51 |  884 | `		int iRdErr = errno ? errno : EIO;` |
|      46 |  885 | `		if( iRdErr != EAGAIN && iRdErr != EINTR` |
|       - |  886 | `#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN` |
|       5 |  887 | `		 && iRdErr != EWOULDBLOCK` |
|       - |  888 | `#endif` |
|       - |  889 | `		){` |
|      39 |  890 | `			pDev->iLastReadErr = iRdErr;` |
|      17 |  891 | `		}` |
|      23 |  892 | `	}` |
|   11607 |  893 | `	return n;` |
|    5947 |  894 | `}` |
|       - |  895 | `/*` |
|       - |  896 | `` * php's `fread(): Read of 8192 bytes failed with errno=9 Bad file descriptor`:`` |
|       - |  897 | ` * the NOTICE its plain-file read op raises when the device refuses -- a read` |
|       - |  898 | ` * from a handle opened write-only being the everyday case. The COUNT is not` |
|       - |  899 | ` * what the caller asked for: php fills its read buffer, so it reports the` |
|       - |  900 | ` * CHUNK size (8192 by default, whatever stream_set_chunk_size() left` |
|       - |  901 | ` * otherwise) at every reader. Silent for every other device, as php's is.` |
|       - |  902 | ` */` |
|    6610 |  903 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev)` |
|       5 |  904 | `{` |
|       - |  905 | `	int iErr;` |
|    6615 |  906 | `	if( pDev == 0 \|\| pDev->iLastReadErr == 0 ){` |
|    6581 |  907 | `		return;` |
|       - |  908 | `	}` |
|      39 |  909 | `	iErr = pDev->iLastReadErr;` |
|      39 |  910 | `	pDev->iLastReadErr = 0;` |
|      39 |  911 | `	if( pDev->pStream != pCtx->pVm->pDefStream ){` |
|      12 |  912 | `		return;` |
|       - |  913 | `	}` |
|      27 |  914 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       - |  915 | `		"Read of %u bytes failed with errno=%d %s",` |
|      26 |  916 | `		pDev->nChunk > 0 ? pDev->nChunk : 8192u,iErr,VfsStrerror(iErr));` |
|    3306 |  917 | `}` |
|    1792 |  918 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |  919 | `{` |
|    1797 |  920 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|    1797 |  921 | `	char *zBuf = (char *)pBuf;` |
|       - |  922 | `	ph7_int64 n,nRead;` |
|    1797 |  923 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    1797 |  924 | `	if( n > 0 ){` |
|      16 |  925 | `		if( n > nLen ){` |
|       8 |  926 | `			n = nLen;` |
|       3 |  927 | `		}` |
|       - |  928 | `		/* Copy the buffered data */` |
|      16 |  929 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|       - |  930 | `		/* Update the read offset */` |
|      16 |  931 | `		pDev->nOfft += (sxu32)n;` |
|      16 |  932 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - |  933 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|       9 |  934 | `			SyBlobReset(&pDev->sBuffer);` |
|       9 |  935 | `			pDev->nOfft = 0;` |
|       4 |  936 | `		}` |
|      16 |  937 | `		nLen -= n;` |
|      16 |  938 | `		if( nLen < 1 ){` |
|       - |  939 | `			/* All done */` |
|       8 |  940 | `			return n;` |
|       - |  941 | `		}` |
|       - |  942 | `		/* Advance the cursor */` |
|       9 |  943 | `		zBuf += n;` |
|       4 |  944 | `	}` |
|       - |  945 | `	/* Read without buffering */` |
|    1791 |  946 | `	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);` |
|    1786 |  947 | `	if( nRead == 0` |
|    1392 |  948 | `	 \|\| (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0` |
|     722 |  949 | `	     && pStream->xTell(pDev->pHandle) >= 0) ){` |
|       - |  950 | `		/* A read that came back with nothing IS php's end-of-file event, and` |
|       - |  951 | `		 * so is a SHORT one on a device that can say where it IS: php fills` |
|       - |  952 | ``		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs`` |
|       - |  953 | `		 * the second read that finds the end. The position query is what tells` |
|       - |  954 | `		 * a regular file from a FIFO — both arrive here through the same file` |
|       - |  955 | `		 * device, and a short read from a fifo, a pipe or a socket means only` |
|       - |  956 | `		 * that less had arrived, so latching there would end` |
|       - |  957 | ``		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A`` |
|       - |  958 | `		 * NEGATIVE answer is an IO error and never latches. */` |
|    1187 |  959 | `		pDev->bEof = 1;` |
|     583 |  960 | `	}` |
|    1791 |  961 | `	if( nRead > 0 ){` |
|     961 |  962 | `		n += nRead;` |
|    1313 |  963 | `	}else if( n < 1 ){` |
|       - |  964 | `		/* EOF or IO error */` |
|     829 |  965 | `		return nRead;` |
|       - |  966 | `	}` |
|     967 |  967 | `	return n;` |
|     894 |  968 | `}` |
|       - |  969 | `/*` |
|       - |  970 | ` * Every SCRIPT-level write goes through here, because a handle can carry a` |
|       - |  971 | ` * WRITE chain: php runs what the script wrote through the filters before the` |
|       - |  972 | ` * device sees any of it, and a filter changes the byte count — so what reaches` |
|       - |  973 | ` * the device is not what was handed in, while what fwrite() ANSWERS still is` |
|       - |  974 | ` * (php reports the bytes it CONSUMED, not the bytes it emitted).` |
|       - |  975 | ` */` |
|     831 |  976 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)` |
|       5 |  977 | `{` |
|     836 |  978 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|       - |  979 | `	SyBlob sOut;` |
|       - |  980 | `	ph7_int64 nWr;` |
|       - |  981 | `	int iStatus;` |
|     836 |  982 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 \|\| pDev->bDir ){` |
|     ! 0 |  983 | `		return -1;` |
|       - |  984 | `	}` |
|     836 |  985 | `	if( pChain == 0 ){` |
|     798 |  986 | `		ph7_int64 nRaw = pDev->pStream->xWrite(pDev->pHandle,pData,nLen);` |
|     798 |  987 | `		if( nRaw > 0 ){` |
|     724 |  988 | `			pDev->iPos += nRaw;` |
|     338 |  989 | `		}` |
|     798 |  990 | `		return nRaw;` |
|       - |  991 | `	}` |
|      41 |  992 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|      41 |  993 | `	iStatus = PH7_FilterChainProcess(pChain,pData,(sxu32)nLen,` |
|       - |  994 | `		PHL_PSFS_FLAG_NORMAL,PHL_PSFS_FLAG_NORMAL,&sOut,0);` |
|      41 |  995 | `	if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|       5 |  996 | `		SyBlobRelease(&sOut);` |
|       5 |  997 | `		return -1;` |
|       - |  998 | `	}` |
|      37 |  999 | `	nWr = 0;` |
|      37 | 1000 | `	if( SyBlobLength(&sOut) > 0 ){` |
|      48 | 1001 | `		nWr = pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|      30 | 1002 | `			(ph7_int64)SyBlobLength(&sOut));` |
|      15 | 1003 | `	}` |
|      37 | 1004 | `	SyBlobRelease(&sOut);` |
|      37 | 1005 | `	if( nWr < 0 ){` |
|     ! 0 | 1006 | `		return -1;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* A filter that held its input back (FEED_ME) still consumed it: php's` |
|       - | 1009 | `	 * fwrite() answers the length it was given, and moves the position by it. */` |
|      37 | 1010 | `	pDev->iPos += nLen;` |
|      37 | 1011 | `	return nLen;` |
|     399 | 1012 | `}` |
|       - | 1013 | `/*` |
|       - | 1014 | ` * php's fflush() flushes the WRITE CHAIN before the device: its` |
|       - | 1015 | ` * php_stream_flush runs every filter with a NON-closing flush, so a filter that` |
|       - | 1016 | ` * has been holding bytes back emits what it has and stays open. PHL flushed the` |
|       - | 1017 | ` * device alone, which is invisible for a filter that buffers nothing and very` |
|       - | 1018 | `` * visible for one that does -- a `zlib.deflate` chain wrote NOTHING until the`` |
|       - | 1019 | ` * handle closed, where php's had already emitted the deflate stream up to a` |
|       - | 1020 | ` * sync point.` |
|       - | 1021 | ` */` |
|      16 | 1022 | `PH7_PRIVATE void PH7_StreamFlushWriteChain(io_private *pDev)` |
|       1 | 1023 | `{` |
|      17 | 1024 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|       - | 1025 | `	SyBlob sOut;` |
|      17 | 1026 | `	if( pChain == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|      15 | 1027 | `		return;` |
|       - | 1028 | `	}` |
|       3 | 1029 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|       2 | 1030 | `	if( PH7_FilterChainProcess(pChain,0,0,PHL_PSFS_FLAG_FLUSH_INC,` |
|       1 | 1031 | `			PHL_PSFS_FLAG_FLUSH_INC,&sOut,0) != PHL_PSFS_ERR_FATAL` |
|       3 | 1032 | `	 && SyBlobLength(&sOut) > 0 ){` |
|     ! 0 | 1033 | `		pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|     ! 0 | 1034 | `			(ph7_int64)SyBlobLength(&sOut));` |
|     ! 0 | 1035 | `	}` |
|       3 | 1036 | `	SyBlobRelease(&sOut);` |
|       9 | 1037 | `}` |
|       - | 1038 | `/*` |
|       - | 1039 | ` * Extract a single line from the buffered input.` |
|       - | 1040 | ` */` |
|   37325 | 1041 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|       5 | 1042 | `{` |
|       - | 1043 | `	const char *zIn,*zEnd,*zPtr;` |
|   37330 | 1044 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   37330 | 1045 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   37330 | 1046 | `	zPtr = zIn;` |
| 1533377 | 1047 | `	while( zIn < zEnd ){` |
| 1533033 | 1048 | `		if( zIn[0] == '\n' ){` |
|       - | 1049 | `			/* Line found */` |
|   36986 | 1050 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   36986 | 1051 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   36986 | 1052 | `			*pzLine = zPtr;` |
|   36986 | 1053 | `			return SXRET_OK;` |
|       - | 1054 | `		}` |
| 1496052 | 1055 | `		zIn++;` |
|       5 | 1056 | `	}` |
|       - | 1057 | `	/* No line were found */` |
|     349 | 1058 | `	return SXERR_NOTFOUND;` |
|   18362 | 1059 | `}` |
|       - | 1060 | `/*` |
|       - | 1061 | ` * Read a single line from the underlying IO stream device.` |
|       - | 1062 | ` */` |
|   43337 | 1063 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|       5 | 1064 | `{` |
|       - | 1065 | `	char zBuf[8192];` |
|       - | 1066 | `	ph7_int64 n;` |
|       - | 1067 | `	sxi32 rc;` |
|   43342 | 1068 | `	n = 0;` |
|   43342 | 1069 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - | 1070 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    9775 | 1071 | `		SyBlobReset(&pDev->sBuffer);` |
|    9775 | 1072 | `		pDev->nOfft = 0;` |
|    4881 | 1073 | `	}` |
|   43342 | 1074 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - | 1075 | `		/* Check if there is a line */` |
|   33572 | 1076 | `		rc = GetLine(pDev,&n,pzData);` |
|   33572 | 1077 | `		if( rc == SXRET_OK ){` |
|       - | 1078 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|       - | 1079 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|   33482 | 1080 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|       3 | 1081 | `				n = nMaxLen;` |
|       1 | 1082 | `			}` |
|   33482 | 1083 | `			pDev->nOfft += (sxu32)n;` |
|   33482 | 1084 | `			return n;` |
|       - | 1085 | `		}` |
|      45 | 1086 | `	}` |
|       - | 1087 | `	/* Perform the read operation until a new line is extracted or length` |
|       - | 1088 | `	 * limit is reached.` |
|       - | 1089 | `	 */` |
|    5034 | 1090 | `	for(;;){` |
|     108 | 1091 | `		{` |
|       - | 1092 | `			/* php fills its read buffer one CHUNK at a time, and` |
|       - | 1093 | `			 * stream_set_chunk_size() is how a script asks for a smaller one. */` |
|   10081 | 1094 | `			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   10081 | 1095 | `			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|      30 | 1096 | `				nAsk = (ph7_int64)pDev->nChunk;` |
|      15 | 1097 | `			}` |
|   10081 | 1098 | `			if( nMaxLen > 0 && nMaxLen < nAsk ){` |
|      75 | 1099 | `				nAsk = nMaxLen;` |
|      37 | 1100 | `			}` |
|   10081 | 1101 | `			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);` |
|       - | 1102 | `		}` |
|   10081 | 1103 | `		if( n == 0 ){` |
|    6319 | 1104 | `			pDev->bEof = 1;` |
|    3153 | 1105 | `		}` |
|   10081 | 1106 | `		if( n < 1 ){` |
|       - | 1107 | `			/* EOF or IO error */` |
|    6323 | 1108 | `			break;` |
|       - | 1109 | `		}` |
|       - | 1110 | `		/* Append the data just read */` |
|    3763 | 1111 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|       - | 1112 | `		/* Try to extract a line */` |
|    3763 | 1113 | `		rc = GetLine(pDev,&n,pzData);` |
|    3763 | 1114 | `		if( rc == SXRET_OK ){` |
|       - | 1115 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|       - | 1116 | `			 * cap, newline included, is left buffered for the next read. */` |
|    3509 | 1117 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|       7 | 1118 | `				n = nMaxLen;` |
|       3 | 1119 | `			}` |
|    3509 | 1120 | `			pDev->nOfft += (sxu32)n;` |
|    3509 | 1121 | `			return n;` |
|       - | 1122 | `		}` |
|     259 | 1123 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|       - | 1124 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|       - | 1125 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|       - | 1126 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|       - | 1127 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|       - | 1128 | `			 * buffer once it is fully consumed. */` |
|      39 | 1129 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|      39 | 1130 | `			n = nMaxLen;` |
|      39 | 1131 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|      39 | 1132 | `			return n;` |
|       - | 1133 | `		}` |
|       5 | 1134 | `	}` |
|    6323 | 1135 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - | 1136 | `		/* Read limit reached,return the available data */` |
|     257 | 1137 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     257 | 1138 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|       - | 1139 | `		/* Reset the working buffer */` |
|     257 | 1140 | `		SyBlobReset(&pDev->sBuffer);` |
|     257 | 1141 | `		pDev->nOfft = 0;` |
|     126 | 1142 | `	}` |
|    6323 | 1143 | `	return n;` |
|   21364 | 1144 | `}` |
|       - | 1145 | `/*` |
|       - | 1146 | ` * Open an IO stream handle.` |
|       - | 1147 | ` * Notes on stream:` |
|       - | 1148 | ` * According to the PHP reference manual.` |
|       - | 1149 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|       - | 1150 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|       - | 1151 | ` * to an arbitrary locations within the stream.` |
|       - | 1152 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|       - | 1153 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|       - | 1154 | ` * on a remote server.` |
|       - | 1155 | ` * A stream is referenced as: scheme://target` |
|       - | 1156 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|       - | 1157 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|       - | 1158 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|       - | 1159 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|       - | 1160 | ` *  with a path appended.` |
|       - | 1161 | ` *` |
|       - | 1162 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|       - | 1163 | ` * Please refer to the official documentation for a full discussion.` |
|       - | 1164 | ` * This function return a handle on success. Otherwise null.` |
|       - | 1165 | ` */` |
|   43988 | 1166 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|       - | 1167 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)` |
|       5 | 1168 | `{` |
|   43993 | 1169 | `	void *pHandle = 0; /* cc warning */` |
|       - | 1170 | `	SyString sFile;` |
|       - | 1171 | `	ph7_value sDummy;` |
|       - | 1172 | `	int rc;` |
|   43993 | 1173 | `	if( pStream == 0 ){` |
|       - | 1174 | `		/* No such stream device. The armed context describes THIS open and` |
|       - | 1175 | `		 * nothing else, so it is dropped on every exit — a caller that armed one` |
|       - | 1176 | `		 * and returned early must not leave it for the next open to pick up. */` |
|     ! 0 | 1177 | `		pVm->pOpenCtx = 0;` |
|     ! 0 | 1178 | `		pVm->zOpenMode[0] = 0;` |
|     ! 0 | 1179 | `		if( pVm->nOpenDepth < 1 ){` |
|     ! 0 | 1180 | `			pVm->zOpenErr = 0;` |
|     ! 0 | 1181 | `		}` |
|     ! 0 | 1182 | `		return 0;` |
|       - | 1183 | `	}` |
|       - | 1184 | `	/* Arm the reason THIS open would report. php's default for a wrapper that` |
|       - | 1185 | `	 * logs nothing of its own is a flat "operation failed"; only the plain-file` |
|       - | 1186 | `	 * wrapper reports an errno, which is why every other one used to print` |
|       - | 1187 | ``	 * whatever errno was left over — `Success` for a failure, among others. An`` |
|       - | 1188 | `	 * xOpen body may replace it through PH7_StreamSetOpenError(). */` |
|   43993 | 1189 | `	if( pVm->nOpenDepth < 1 ){` |
|   43891 | 1190 | `		pVm->zOpenErr = pStream == pVm->pDefStream ? 0 : "operation failed";` |
|   43891 | 1191 | `		pVm->zOpenCaller = zCaller;` |
|   21904 | 1192 | `	}` |
|   43993 | 1193 | `	if( pStream->xOpen == 0 ){` |
|       - | 1194 | `		/* A wrapper with a dir_opener and NOTHING else — glob:// is php's one,` |
|       - | 1195 | `		 * and this is php's sentence for it. Reached before the call, because` |
|       - | 1196 | `		 * the call would be through a null pointer. */` |
|       7 | 1197 | `		pVm->pOpenCtx = 0;` |
|       7 | 1198 | `		pVm->zOpenMode[0] = 0;` |
|       7 | 1199 | `		if( pVm->nOpenDepth < 1 ){` |
|       7 | 1200 | `			pVm->zOpenErr = "wrapper does not support stream open";` |
|       3 | 1201 | `		}` |
|       7 | 1202 | `		return 0;` |
|       - | 1203 | `	}` |
|       - | 1204 | `	/* A wrapper registered with STREAM_IS_URL speaks to the network, and php lets` |
|       - | 1205 | `	 * the configuration turn that off: allow_url_fopen for an ordinary open,` |
|       - | 1206 | `	 * allow_url_include for the one that EXECUTES what comes back — which is off` |
|       - | 1207 | `	 * by default, because including a remote file is the classic RFI. */` |
|   43987 | 1208 | `	if( PH7_StreamIsUrlWrapper(pStream) ){` |
|       - | 1209 | `		/* php tests BOTH, in this order, and words them differently. Without` |
|       - | 1210 | `		 * allow_url_fopen the wrapper is not FOUND at all -- php's lookup` |
|       - | 1211 | `		 * declines to hand it over, so the caller reports the missing-wrapper` |
|       - | 1212 | `		 * sentence and names the URI -- while allow_url_include, off by default,` |
|       - | 1213 | `		 * refuses only the open that would EXECUTE what came back, and that one` |
|       - | 1214 | `		 * names the wrapper and the directive. */` |
|       - | 1215 | `		SyString sCaller;` |
|     347 | 1216 | `		SyStringInitFromBuf(&sCaller,zCaller ? zCaller : "",zCaller ? SyStrlen(zCaller) : 0);` |
|     347 | 1217 | `		if( !PH7_VmIniGetBool(pVm,"allow_url_fopen",1) ){` |
|       - | 1218 | `			/* TWO sentences, as php raises them: its own about the directive,` |
|       - | 1219 | `			 * and then the caller's about an open that found no wrapper -- which` |
|       - | 1220 | `			 * is what this open's REASON has to be, since php's lookup is where` |
|       - | 1221 | `			 * the switch lives and a lookup that declines has nothing else to` |
|       - | 1222 | `			 * say. This engine used to raise only the first, leaving the` |
|       - | 1223 | `			 * caller to print whatever reason was armed ("operation failed"). */` |
|       - | 1224 | `			char zMsg[160];` |
|      13 | 1225 | `			pVm->pOpenCtx = 0;` |
|      13 | 1226 | `			pVm->zOpenMode[0] = 0;` |
|      13 | 1227 | `			if( pVm->nOpenDepth < 1 ){` |
|      13 | 1228 | `				pVm->zOpenErr = "no suitable wrapper could be found";` |
|       6 | 1229 | `			}` |
|      19 | 1230 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1231 | `				"%s:// wrapper is disabled in the server configuration by allow_url_fopen=0",` |
|      12 | 1232 | `				pStream->zName);` |
|      13 | 1233 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|      13 | 1234 | `			return 0;` |
|       - | 1235 | `		}` |
|     334 | 1236 | `		if( bPushInclude && !PH7_VmIniGetBool(pVm,"allow_url_include",0) ){` |
|       - | 1237 | `			char zMsg[160];` |
|       7 | 1238 | `			pVm->pOpenCtx = 0;` |
|       7 | 1239 | `			pVm->zOpenMode[0] = 0;` |
|      10 | 1240 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1241 | `				"%s:// wrapper is disabled in the server configuration by allow_url_include=0",` |
|       6 | 1242 | `				pStream->zName);` |
|       7 | 1243 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|       7 | 1244 | `			return 0;` |
|       - | 1245 | `		}` |
|     162 | 1246 | `	}` |
|   43969 | 1247 | `	if( pResource == 0 ){` |
|       - | 1248 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|       - | 1249 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|       - | 1250 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|       - | 1251 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|       - | 1252 | `		 * reads it during the call, and file:// ignores it. */` |
|   43124 | 1253 | `		PH7_MemObjInit(pVm,&sDummy);` |
|   43124 | 1254 | `		pResource = &sDummy;` |
|   21522 | 1255 | `	}` |
|   43969 | 1256 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|       - | 1257 | `	/* Everything from here to the matching decrement is INSIDE an open, so a` |
|       - | 1258 | `	 * wrapper that opens something of its own does not get to rename the` |
|       - | 1259 | `	 * failure its caller will report. */` |
|   43969 | 1260 | `	pVm->nOpenDepth++;` |
|   43969 | 1261 | `	if( use_include ){` |
|    5693 | 1262 | `		if(	/* include_path names DIRECTORIES, so it has nothing to say about a` |
|       - | 1263 | `` 			 * URL: walking it for a `php://filter/…` one built `<dir>/filter/…` `` |
|       - | 1264 | `			 * and reported the whole open as an IO error. The direct arm is the` |
|       - | 1265 | `			 * one that also marks the file as included, which is what` |
|       - | 1266 | `			 * include_once needs. */` |
|   11386 | 1267 | `			pStream != pVm->pDefStream \|\|` |
|   11352 | 1268 | `			sFile.zString[0] == '/' \|\|` |
|       - | 1269 | `#ifdef __WINNT__` |
|       - | 1270 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|       - | 1271 | `#endif` |
|   11183 | 1272 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   11176 | 1273 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|       - | 1274 | `				/*  Open the file directly */` |
|     215 | 1275 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     215 | 1276 | `				if( rc == PH7_OK && bPushInclude ){` |
|       - | 1277 | `					/* Mark as included -- under the name the SCRIPT wrote, scheme` |
|       - | 1278 | `					 * included. The lookup handed the wrapper a stripped path, and` |
|       - | 1279 | ``					 * pushing THAT made `__FILE__` inside an included phar entry`` |
|       - | 1280 | ``					 * `x.phar/src/f.php` rather than `phar://x.phar/src/f.php` --`` |
|       - | 1281 | ``					 * so the `__DIR__ . '/../vendor/autoload.php'` every real stub`` |
|       - | 1282 | `					 * writes resolved to a path outside the archive. */` |
|     213 | 1283 | `					const char *zPush = sFile.zString;` |
|     213 | 1284 | `					sxu32 nPush = sFile.nByte;` |
|     213 | 1285 | `					if( zPush == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){` |
|      37 | 1286 | `						zPush = pVm->zOpenUri;` |
|      37 | 1287 | `						nPush = (sxu32)pVm->nOpenUri;` |
|      17 | 1288 | `					}` |
|     213 | 1289 | `					PH7_VmPushFilePath(pVm,zPush,nPush,FALSE,pNew);` |
|     104 | 1290 | `				}` |
|     110 | 1291 | `		}else{` |
|       - | 1292 | `			SyString *pPath;` |
|       - | 1293 | `			SyBlob sWorker;` |
|       - | 1294 | `#ifdef __WINNT__` |
|       - | 1295 | `			static const int c = '\\';` |
|       - | 1296 | `#else` |
|       - | 1297 | `			static const int c = '/';` |
|       - | 1298 | `#endif` |
|       - | 1299 | `			/* Init the path builder working buffer */` |
|   11180 | 1300 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       - | 1301 | `			/* Build a path from the set of include path */` |
|   11180 | 1302 | `			SySetResetCursor(&pVm->aPaths);` |
|   11180 | 1303 | `			rc = SXERR_IO;` |
|   11204 | 1304 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|       - | 1305 | `				/* Build full path */` |
|   11188 | 1306 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|       - | 1307 | `				/* Append null terminator */` |
|   11188 | 1308 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|     ! 0 | 1309 | `					continue;` |
|       - | 1310 | `				}` |
|       - | 1311 | `				/* Try to open the file */` |
|   11188 | 1312 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   11188 | 1313 | `				if( rc == PH7_OK ){` |
|   11164 | 1314 | `					if( bPushInclude ){` |
|       - | 1315 | `						/* Mark as included */` |
|   11164 | 1316 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|    5580 | 1317 | `					}` |
|   11164 | 1318 | `					break;` |
|       - | 1319 | `				}` |
|       - | 1320 | `				/* Reset the working buffer */` |
|      26 | 1321 | `				SyBlobReset(&sWorker);` |
|       - | 1322 | `				/* Check the next path */` |
|       2 | 1323 | `			}` |
|   11180 | 1324 | `			if( rc != PH7_OK ){` |
|       - | 1325 | `				/* php's LAST RESORT, and the one PHL never had: the directory of` |
|       - | 1326 | ``				 * the file that is EXECUTING. `include 'lib.php'` next to the`` |
|       - | 1327 | `				 * script has to work whatever directory the script was started` |
|       - | 1328 | `				 * from -- see PH7_VmExecutingDir(). Tried after the include_path` |
|       - | 1329 | `				 * entries, as php tries it. */` |
|       - | 1330 | `				SyString sDir;` |
|      18 | 1331 | `				if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      18 | 1332 | `					SyBlobReset(&sWorker);` |
|      18 | 1333 | `					SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      18 | 1334 | `					if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      18 | 1335 | `						rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|      18 | 1336 | `						if( rc == PH7_OK && bPushInclude ){` |
|       4 | 1337 | `							PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),` |
|       2 | 1338 | `								SyBlobLength(&sWorker),FALSE,pNew);` |
|       1 | 1339 | `						}` |
|       8 | 1340 | `					}` |
|       8 | 1341 | `				}` |
|       8 | 1342 | `			}` |
|   11180 | 1343 | `			SyBlobRelease(&sWorker);` |
|       - | 1344 | `		}` |
|    5698 | 1345 | `	}else{` |
|       - | 1346 | `		/* Open the URI direcly */` |
|   32583 | 1347 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|       - | 1348 | `	}` |
|   43969 | 1349 | `	pVm->nOpenDepth--;` |
|       - | 1350 | `	/* The armed context describes exactly ONE open — every attempt of the` |
|       - | 1351 | `	 * include-path walk above included — so it is dropped here whether the open` |
|       - | 1352 | `	 * worked or not. A device that wanted it (a userland wrapper) read it while` |
|       - | 1353 | `	 * its xOpen was running. */` |
|   43969 | 1354 | `	pVm->pOpenCtx = 0;` |
|   43969 | 1355 | `	pVm->zOpenMode[0] = 0;` |
|       - | 1356 | ``	/* An http:// exchange publishes `$http_response_header` into the frame that`` |
|       - | 1357 | `	 * called the opener, and it does so whether the open SUCCEEDED or not: a 404` |
|       - | 1358 | `	 * is a failed open with a complete set of headers behind it. Only the` |
|       - | 1359 | `	 * OUTERMOST open publishes -- a php://filter that opened its own resource is` |
|       - | 1360 | `	 * not what the script asked about. */` |
|   43969 | 1361 | `	if( pVm->nOpenDepth < 1 ){` |
|   43867 | 1362 | `		PH7_HttpFlushResponseHeaders(pVm);` |
|   21892 | 1363 | `	}` |
|   43969 | 1364 | `	if( rc != PH7_OK ){` |
|       - | 1365 | `		/* IO error */` |
|     359 | 1366 | `		return 0;` |
|       - | 1367 | `	}` |
|       - | 1368 | `	/* Nothing failed, so nothing is owed a reason: a later warning must not` |
|       - | 1369 | `	 * find this one still armed. An INNER open succeeding says nothing about` |
|       - | 1370 | `	 * the outer one, which may still be on its way to failing. */` |
|   43615 | 1371 | `	if( pVm->nOpenDepth < 1 ){` |
|   43521 | 1372 | `		pVm->zOpenErr = 0;` |
|   21735 | 1373 | `	}` |
|       - | 1374 | `	/* Return the file handle */` |
|   43615 | 1375 | `	return pHandle;` |
|   21960 | 1376 | `}` |
|       - | 1377 | `/* See ph7int.h: the wrapper's own reason for the open in flight. */` |
|     134 | 1378 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason)` |
|       4 | 1379 | `{` |
|       - | 1380 | `	/* Only the OUTERMOST wrapper's own body may name the failure: an inner` |
|       - | 1381 | `	 * open's wrapper is describing something the caller never asked for. */` |
|     138 | 1382 | `	if( pVm->nOpenDepth == 1 ){` |
|     138 | 1383 | `		pVm->zOpenErr = zReason;` |
|      66 | 1384 | `	}` |
|     138 | 1385 | `}` |
|       - | 1386 | `/* See ph7int.h. */` |
|       - | 1387 | `/*` |
|       - | 1388 | ` * Remember the mode STRING an open was asked with. php hands a userland wrapper's` |
|       - | 1389 | ` * stream_open() the caller's own spelling, and only fopen() and SplFileObject have` |
|       - | 1390 | ` * one -- every other opener is C code with a fixed mode, which UwrapOpenSlot spells` |
|       - | 1391 | ` * back from the flag bits. Cleared after each open, exactly like pOpenCtx.` |
|       - | 1392 | ` */` |
|    1872 | 1393 | `PH7_PRIVATE void PH7_StreamArmOpenMode(ph7_vm *pVm,const char *zMode,int nMode)` |
|       5 | 1394 | `{` |
|    1877 | 1395 | `	int n = nMode;` |
|    1877 | 1396 | `	if( zMode == 0 \|\| n < 1 ){` |
|     ! 0 | 1397 | `		pVm->zOpenMode[0] = 0;` |
|     ! 0 | 1398 | `		return;` |
|       - | 1399 | `	}` |
|    1877 | 1400 | `	if( n > (int)sizeof(pVm->zOpenMode) - 1 ){` |
|     ! 0 | 1401 | `		n = (int)sizeof(pVm->zOpenMode) - 1;` |
|     ! 0 | 1402 | `	}` |
|    1877 | 1403 | `	SyMemcpy(zMode,pVm->zOpenMode,(sxu32)n);` |
|    1877 | 1404 | `	pVm->zOpenMode[n] = 0;` |
|     938 | 1405 | `}` |
|      40 | 1406 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod)` |
|       2 | 1407 | `{` |
|      42 | 1408 | `	if( pVm->nOpenDepth != 1 ){` |
|     ! 0 | 1409 | `		return;` |
|       - | 1410 | `	}` |
|      62 | 1411 | `	SyBufferFormat(pVm->zOpenErrBuf,sizeof(pVm->zOpenErrBuf),"\"%s::%s\" call failed",` |
|      20 | 1412 | `		zClass ? zClass : "",zMethod);` |
|      42 | 1413 | `	pVm->zOpenErr = pVm->zOpenErrBuf;` |
|      22 | 1414 | `}` |
|       - | 1415 | `/*` |
|       - | 1416 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|       - | 1417 | ` * Store the read data in the given BLOB (last argument).` |
|       - | 1418 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|       - | 1419 | ` */` |
|   11802 | 1420 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|       5 | 1421 | `{` |
|       - | 1422 | `	ph7_int64 nRead;` |
|       - | 1423 | `	char zBuf[8192]; /* 8K */` |
|       - | 1424 | `	int rc;` |
|       - | 1425 | `	/* Perform the requested operation */` |
|   11797 | 1426 | `	for(;;){` |
|   23639 | 1427 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|   23639 | 1428 | `		if( nRead < 1 ){` |
|       - | 1429 | `			/* EOF or IO error */` |
|   11807 | 1430 | `			break;` |
|       - | 1431 | `		}` |
|       - | 1432 | `		/* Append contents */` |
|   11837 | 1433 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   11837 | 1434 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1435 | `			break;` |
|       - | 1436 | `		}` |
|       5 | 1437 | `	}` |
|       - | 1438 | `	/* An EMPTY file is read, not failed: php's include of a 0-byte file is a` |
|       - | 1439 | `	 * silent no-op where this answered -1 and the include warned "IO error while` |
|       - | 1440 | `	 * importing". The device's own failure still is one -- it answers a NEGATIVE` |
|       - | 1441 | `	 * count, where end-of-file is 0. */` |
|   11807 | 1442 | `	return (SyBlobLength(pOut) > 0 \|\| nRead == 0) ? SXRET_OK : -1;` |
|       5 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|       - | 1446 | ` */` |
|   44503 | 1447 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|       5 | 1448 | `{` |
|   44508 | 1449 | `	if( pStream->xClose ){` |
|   44508 | 1450 | `		pStream->xClose(pHandle);` |
|   22229 | 1451 | `	}` |
|   44508 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * string fgetc(resource $handle)` |
|       - | 1455 | ` *  Gets a character from the given file pointer.` |
|       - | 1456 | ` * Parameters` |
|       - | 1457 | ` *  $handle` |
|       - | 1458 | ` *   The file pointer.` |
|       - | 1459 | ` * Return` |
|       - | 1460 | ` *  Returns a string containing a single character read from the file` |
|       - | 1461 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|       - | 1462 | ` * WARNING` |
|       - | 1463 | ` *  This operation is extremely slow.Avoid using it.` |
|       - | 1464 | ` */` |
|      14 | 1465 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1466 | `{` |
|       - | 1467 | `	io_private *pDev;` |
|       - | 1468 | `	int c,n;` |
|       - | 1469 | `	int rc;` |
|      17 | 1470 | `	if( nArg < 1 ){` |
|       - | 1471 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1472 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1473 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1474 | `		return PH7_OK;` |
|       - | 1475 | `	}` |
|       - | 1476 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      17 | 1477 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      17 | 1478 | `	if( pDev == 0 ){` |
|       5 | 1479 | `		return rc;` |
|       - | 1480 | `	}` |
|      12 | 1481 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1482 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 | 1483 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1484 | `		return PH7_OK;` |
|       - | 1485 | `	}` |
|       - | 1486 | `	/* Perform the requested operation */` |
|      10 | 1487 | `	n = (int)PH7_StreamRead(pDev,(void *)&c,sizeof(char));` |
|       - | 1488 | `	/* IO result */` |
|      10 | 1489 | `	if( n < 1 ){` |
|       - | 1490 | `		/* EOF or error,return FALSE */` |
|       3 | 1491 | `		StreamReportReadFailure(pCtx,pDev);` |
|       3 | 1492 | `		ph7_result_bool(pCtx,0);` |
|       2 | 1493 | `	}else{` |
|       - | 1494 | `		/* Return the string holding the character */` |
|       8 | 1495 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|       - | 1496 | `	}` |
|      10 | 1497 | `	return PH7_OK;` |
|      10 | 1498 | `}` |
|       - | 1499 | `/*` |
|       - | 1500 | ` * array\|int\|false\|null fscanf(resource $stream, string $format, mixed &...$vars)` |
|       - | 1501 | ` *  Parse the NEXT LINE of $stream according to $format.` |
|       - | 1502 | ` *` |
|       - | 1503 | ` *  php reads one whole line -- the newline included, which is what makes a` |
|       - | 1504 | `` *  trailing `%s` stop where it does -- and hands it to the same scanner`` |
|       - | 1505 | ` *  sscanf() runs, so every rule of that family (the two-pass format read, the` |
|       - | 1506 | ` *  -1 / NULL "nothing converted" answer) is this function's too. The one` |
|       - | 1507 | ` *  answer of its own is FALSE, and it means the STREAM was at its end: a line` |
|       - | 1508 | ` *  that scans to nothing is still NULL or -1, exactly as sscanf's would be.` |
|       - | 1509 | ` */` |
|      48 | 1510 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1511 | `{` |
|       - | 1512 | `	const char *zLine,*zFmt;` |
|       - | 1513 | `	io_private *pDev;` |
|       - | 1514 | `	ph7_int64 n;` |
|      51 | 1515 | `	int nFmt = 0;` |
|      51 | 1516 | `	if( nArg < 2 ){` |
|     ! 0 | 1517 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1518 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1519 | `		return PH7_OK;` |
|       - | 1520 | `	}` |
|      51 | 1521 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - | 1522 | `		char zGiven[64];` |
|      19 | 1523 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1524 | `			"%s(): Argument #1 ($stream) must be of type resource, %s given",` |
|       6 | 1525 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 1526 | `	}` |
|       - | 1527 | `	/* ...but a resource whose DEVICE is gone is the one refusal in this family` |
|       - | 1528 | `	 * php words its own way: it names neither the argument nor its position,` |
|       - | 1529 | `	 * and calls the thing a File-Handle. */` |
|      39 | 1530 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      39 | 1531 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       4 | 1532 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1533 | `			"%s(): supplied resource is not a valid File-Handle resource",` |
|       1 | 1534 | `			ph7_function_name(pCtx));` |
|       - | 1535 | `	}` |
|      36 | 1536 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1537 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 | 1538 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1539 | `		return PH7_OK;` |
|       - | 1540 | `	}` |
|      33 | 1541 | `	n = StreamReadLine(pDev,&zLine,-1);` |
|      33 | 1542 | `	if( n < 1 ){` |
|       - | 1543 | `		/* Nothing left in the stream at all. */` |
|       5 | 1544 | `		ph7_result_bool(pCtx,0);` |
|       5 | 1545 | `		return PH7_OK;` |
|       - | 1546 | `	}` |
|      29 | 1547 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      29 | 1548 | `	return (int)PH7_ScanfRun(pCtx,zLine,(int)n,zFmt,nFmt,&apArg[2],nArg - 2);` |
|      27 | 1549 | `}` |
|       - | 1550 | `/*` |
|       - | 1551 | ` * string fgets(resource $handle[,int64 $length ])` |
|       - | 1552 | ` *  Gets line from file pointer.` |
|       - | 1553 | ` * Parameters` |
|       - | 1554 | ` *  $handle` |
|       - | 1555 | ` *   The file pointer.` |
|       - | 1556 | ` * $length` |
|       - | 1557 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - | 1558 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - | 1559 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - | 1560 | ` *  the end of the line.` |
|       - | 1561 | ` * Return` |
|       - | 1562 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|       - | 1563 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|       - | 1564 | ` *  If an error occurs, FALSE is returned.` |
|       - | 1565 | ` */` |
|   42743 | 1566 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1567 | `{` |
|       - | 1568 | `	const char *zLine;` |
|       - | 1569 | `	io_private *pDev;` |
|       - | 1570 | `	ph7_int64 n,nLen;` |
|       - | 1571 | `	int rc;` |
|   42748 | 1572 | `	if( nArg < 1 ){` |
|       - | 1573 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1574 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1575 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1576 | `		return PH7_OK;` |
|       - | 1577 | `	}` |
|       - | 1578 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|   42748 | 1579 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   42748 | 1580 | `	if( pDev == 0 ){` |
|       5 | 1581 | `		return rc;` |
|       - | 1582 | `	}` |
|   42744 | 1583 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1584 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 | 1585 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1586 | `		return PH7_OK;` |
|       - | 1587 | `	}` |
|   42742 | 1588 | `	nLen = -1;` |
|   42742 | 1589 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 1590 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|       - | 1591 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|      66 | 1592 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      66 | 1593 | `		if( nLen < 1 ){` |
|       7 | 1594 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1595 | `				"%s(): Argument #2 ($length) must be greater than 0",` |
|       2 | 1596 | `				ph7_function_name(pCtx));` |
|       - | 1597 | `		}` |
|       - | 1598 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|       - | 1599 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|       - | 1600 | `		 * false at any position, exactly like EOF. */` |
|      62 | 1601 | `		nLen -= 1;` |
|      62 | 1602 | `		if( nLen == 0 ){` |
|       6 | 1603 | `			ph7_result_bool(pCtx,0);` |
|       6 | 1604 | `			return PH7_OK;` |
|       - | 1605 | `		}` |
|      28 | 1606 | `	}` |
|       - | 1607 | `	/* Perform the requested operation */` |
|   42734 | 1608 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   42734 | 1609 | `	if( n < 1 ){` |
|       - | 1610 | `		/* EOF or IO error,return FALSE */` |
|    5907 | 1611 | `		StreamReportReadFailure(pCtx,pDev);` |
|    5907 | 1612 | `		ph7_result_bool(pCtx,0);` |
|    2952 | 1613 | `	}else{` |
|       - | 1614 | `		/* Return the freshly extracted line */` |
|   36832 | 1615 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|       - | 1616 | `	}` |
|   42734 | 1617 | `	return PH7_OK;` |
|   21067 | 1618 | `}` |
|       - | 1619 | `/*` |
|       - | 1620 | ` * string\|false stream_get_line(resource $stream, int $length, string $ending = "")` |
|       - | 1621 | ` *  Read a line from a stream, up to $length bytes or the FIRST occurrence of` |
|       - | 1622 | ` *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but` |
|       - | 1623 | ` *  never returned, and it may be any string.` |
|       - | 1624 | ` *  php's window rule (php_stream_get_record), pinned by probe: the ending` |
|       - | 1625 | ` *  counts only when it fits ENTIRELY inside the first $length bytes —` |
|       - | 1626 | ` *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,` |
|       - | 1627 | ` *  because the ending straddles its edge — and a capped read consumes no` |
|       - | 1628 | ` *  ending that starts at the boundary. $length 0 means php's 8192 default; at` |
|       - | 1629 | ` *  EOF the remainder is returned as-is, and false only when nothing is left.` |
|       - | 1630 | ` */` |
|      66 | 1631 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1632 | `{` |
|       - | 1633 | `	char zGiven[64];` |
|      69 | 1634 | `	const char *zEnding = "";` |
|       - | 1635 | `	io_private *pDev;` |
|       - | 1636 | `	ph7_int64 nMaxLen;` |
|      69 | 1637 | `	int nEndLen = 0;` |
|      69 | 1638 | `	sxu32 iScanFrom = 0;` |
|      69 | 1639 | `	int bEof = 0;` |
|      69 | 1640 | `	if( nArg < 2 ){` |
|       - | 1641 | `		/* The central arity screen reports this; keep a refusal for a direct call. */` |
|     ! 0 | 1642 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1643 | `		return PH7_OK;` |
|       - | 1644 | `	}` |
|      69 | 1645 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       4 | 1646 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1647 | `			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",` |
|       1 | 1648 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 1649 | `	}` |
|      67 | 1650 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      67 | 1651 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 1652 | `		/* A closed or foreign resource is php's own TypeError, not a warning. */` |
|       3 | 1653 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1654 | `			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");` |
|       - | 1655 | `	}` |
|      65 | 1656 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1657 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 | 1658 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1659 | `		return PH7_OK;` |
|       - | 1660 | `	}` |
|      63 | 1661 | `	nMaxLen = ph7_value_to_int64(apArg[1]);` |
|      63 | 1662 | `	if( nMaxLen < 0 ){` |
|       3 | 1663 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1664 | `			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");` |
|       - | 1665 | `	}` |
|      61 | 1666 | `	if( nMaxLen == 0 ){` |
|       - | 1667 | `		/* php's documented default window */` |
|       3 | 1668 | `		nMaxLen = 8192;` |
|       1 | 1669 | `	}` |
|      61 | 1670 | `	if( nArg > 2 ){` |
|      57 | 1671 | `		zEnding = ph7_value_to_string(apArg[2],&nEndLen);` |
|      27 | 1672 | `	}` |
|      61 | 1673 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - | 1674 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      33 | 1675 | `		SyBlobReset(&pDev->sBuffer);` |
|      33 | 1676 | `		pDev->nOfft = 0;` |
|      15 | 1677 | `	}` |
|       - | 1678 | `	/* Fill-and-scan: buffer chunks until the ending fits inside the window,` |
|       - | 1679 | `	 * the window itself fills, or the stream dries up. */` |
|      65 | 1680 | `	for(;;){` |
|     105 | 1681 | `		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     105 | 1682 | `		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     105 | 1683 | `		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;` |
|       - | 1684 | `		ph7_int64 n;` |
|       - | 1685 | `		char zBuf[8192];` |
|     105 | 1686 | `		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){` |
|       - | 1687 | `			/* The ending must END inside the window to count. Resume the scan` |
|       - | 1688 | `			 * where the previous fill left off — a candidate can straddle two` |
|       - | 1689 | `			 * fills, so back up by the ending's length less one. */` |
|       - | 1690 | `			sxu32 i;` |
|   40183 | 1691 | `			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){` |
|   40151 | 1692 | `				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){` |
|      29 | 1693 | `					pDev->nOfft += i + (sxu32)nEndLen;` |
|      29 | 1694 | `					ph7_result_string(pCtx,zData,(int)i);` |
|      45 | 1695 | `					return PH7_OK;` |
|       - | 1696 | `				}` |
|   20064 | 1697 | `			}` |
|      34 | 1698 | `			iScanFrom = i;` |
|      16 | 1699 | `		}` |
|      79 | 1700 | `		if( (ph7_int64)nAvail >= nMaxLen ){` |
|       - | 1701 | `			/* Window full with no ending inside it: hand the window back raw,` |
|       - | 1702 | `			 * anything past it (an ending included) stays buffered. */` |
|      18 | 1703 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|      18 | 1704 | `			ph7_result_string(pCtx,zData,(int)nMaxLen);` |
|      18 | 1705 | `			return PH7_OK;` |
|       - | 1706 | `		}` |
|      63 | 1707 | `		if( bEof ){` |
|       - | 1708 | `			/* EOF: the remainder as-is, false when nothing is left. */` |
|      18 | 1709 | `			if( nAvail > 0 ){` |
|      12 | 1710 | `				pDev->nOfft += nAvail;` |
|      12 | 1711 | `				ph7_result_string(pCtx,zData,(int)nAvail);` |
|       7 | 1712 | `			}else{` |
|       8 | 1713 | `				ph7_result_bool(pCtx,0);` |
|       - | 1714 | `			}` |
|      18 | 1715 | `			return PH7_OK;` |
|       - | 1716 | `		}` |
|      47 | 1717 | `		n = IoPrivateDeviceRead(pDev,zBuf,(ph7_int64)sizeof(zBuf));` |
|      47 | 1718 | `		if( n < 1 ){` |
|      18 | 1719 | `			bEof = 1;` |
|      18 | 1720 | `			if( n == 0 ){` |
|      18 | 1721 | `				pDev->bEof = 1;` |
|       8 | 1722 | `			}` |
|      18 | 1723 | `			continue;` |
|       - | 1724 | `		}` |
|      31 | 1725 | `		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){` |
|     ! 0 | 1726 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1727 | `		}` |
|       3 | 1728 | `	}` |
|      36 | 1729 | `}` |
|       - | 1730 | `/*` |
|       - | 1731 | ` * string fread(resource $handle,int64 $length)` |
|       - | 1732 | ` *  Binary-safe file read.` |
|       - | 1733 | ` * Parameters` |
|       - | 1734 | ` *  $handle` |
|       - | 1735 | ` *   The file pointer.` |
|       - | 1736 | ` * $length` |
|       - | 1737 | ` *  Up to length number of bytes read.` |
|       - | 1738 | ` * Return` |
|       - | 1739 | ` *  The data readen on success or FALSE on failure.` |
|       - | 1740 | ` */` |
|     348 | 1741 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1742 | `{` |
|       - | 1743 | `	io_private *pDev;` |
|       - | 1744 | `	ph7_int64 nRead;` |
|       - | 1745 | `	void *pBuf;` |
|       - | 1746 | `	int nLen;` |
|       - | 1747 | `	int rc;` |
|     353 | 1748 | `	if( nArg < 1 ){` |
|       - | 1749 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1750 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1751 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1752 | `		return PH7_OK;` |
|       - | 1753 | `	}` |
|       - | 1754 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     353 | 1755 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     353 | 1756 | `	if( pDev == 0 ){` |
|      17 | 1757 | `		return rc;` |
|       - | 1758 | `	}` |
|     337 | 1759 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1760 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 | 1761 | `		ph7_result_bool(pCtx,0);` |
|       3 | 1762 | `		return PH7_OK;` |
|       - | 1763 | `	}` |
|     335 | 1764 | `        nLen = 4096;` |
|     335 | 1765 | `	if( nArg > 1 ){` |
|       - | 1766 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|       - | 1767 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|       - | 1768 | `	   * the central type screen (the recorded null-policy divergence). */` |
|     335 | 1769 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|     335 | 1770 | `	  if( nWant < 1 ){` |
|       7 | 1771 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1772 | `			"%s(): Argument #2 ($length) must be greater than 0",` |
|       2 | 1773 | `			ph7_function_name(pCtx));` |
|       - | 1774 | `	  }` |
|     331 | 1775 | `	  nLen = (int)nWant;` |
|     331 | 1776 | `	  if( nLen < 1 ){` |
|       - | 1777 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|       - | 1778 | `		 * (StreamRead returns only what the stream holds) instead of` |
|       - | 1779 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|       - | 1780 | `		 * that do not fit an int. */` |
|     ! 0 | 1781 | `		nLen = 4096;` |
|     ! 0 | 1782 | `	  }` |
|     156 | 1783 | `        }` |
|       - | 1784 | `	/* Allocate enough buffer */` |
|     331 | 1785 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     331 | 1786 | `	if( pBuf == 0 ){` |
|     ! 0 | 1787 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 1788 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1789 | `		return PH7_OK;` |
|       - | 1790 | `	}` |
|       - | 1791 | `	/* Perform the requested operation */` |
|     331 | 1792 | `	errno = 0;` |
|     331 | 1793 | `	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     331 | 1794 | `	if( nRead < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK) ){` |
|       - | 1795 | `		/* Nothing had ARRIVED yet, which is not a failure: php answers "" for a` |
|       - | 1796 | ``		 * read that could not proceed and reserves `false` for one that broke.`` |
|       - | 1797 | `		 * The question is answered by errno rather than by a per-handle flag —` |
|       - | 1798 | `		 * two handles can share one descriptor (every php://stdin is fd 0), so` |
|       - | 1799 | `		 * a flag on the handle that set the mode answers wrongly for its` |
|       - | 1800 | `		 * siblings, and a genuine EBADF on a non-blocking write-only handle` |
|       - | 1801 | `		 * would come back as "" rather than false. When a TIMEOUT is what` |
|       - | 1802 | ``		 * expired, php reports false and sets the metadata's `timed_out`. */`` |
|       9 | 1803 | `		if( pDev->bHasTimeout && !pDev->bNonBlock ){` |
|       - | 1804 | `			/* A handle in NON-BLOCKING mode is the other case: it answers "" for` |
|       - | 1805 | `			 * a read that found nothing whether or not a timeout is armed, and` |
|       - | 1806 | ``			 * every socket now carries `default_socket_timeout`. */`` |
|       3 | 1807 | `			pDev->bTimedOut = 1;` |
|       3 | 1808 | `			ph7_result_bool(pCtx,0);` |
|       2 | 1809 | `		}else{` |
|       7 | 1810 | `			ph7_result_string(pCtx,"",0);` |
|       1 | 1811 | `		}` |
|     327 | 1812 | `	}else if( nRead < 0 ){` |
|       - | 1813 | `		/* A real IO error, which is php's other false here. */` |
|      33 | 1814 | `		StreamReportReadFailure(pCtx,pDev);` |
|      33 | 1815 | `		ph7_result_bool(pCtx,0);` |
|      19 | 1816 | `	}else{` |
|       - | 1817 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|       - | 1818 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|       - | 1819 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|       - | 1820 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|       - | 1821 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|     295 | 1822 | `		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Release the buffer */` |
|     331 | 1825 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     331 | 1826 | `	return PH7_OK;` |
|     172 | 1827 | `}` |
|       - | 1828 | `/*` |
|       - | 1829 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|       - | 1830 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|       - | 1831 | ` * Gets line from file pointer and parse for CSV fields.` |
|       - | 1832 | ` * Parameters` |
|       - | 1833 | ` * $handle` |
|       - | 1834 | ` *   The file pointer.` |
|       - | 1835 | ` * $length` |
|       - | 1836 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - | 1837 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - | 1838 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - | 1839 | ` *  the end of the line.` |
|       - | 1840 | ` * $delimiter` |
|       - | 1841 | ` *   Set the field delimiter (one character only).` |
|       - | 1842 | ` * $enclosure` |
|       - | 1843 | ` *   Set the field enclosure character (one character only).` |
|       - | 1844 | ` * $escape` |
|       - | 1845 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|       - | 1846 | ` * Return` |
|       - | 1847 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|       - | 1848 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|       - | 1849 | ` *  If an error occurs, FALSE is returned.` |
|       - | 1850 | ` */` |
|      70 | 1851 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1852 | `{` |
|       - | 1853 | `	const char *zLine;` |
|       - | 1854 | `	io_private *pDev;` |
|       - | 1855 | `	ph7_int64 n,nLen;` |
|      72 | 1856 | `	int delim  = ',';   /* Delimiter */` |
|      72 | 1857 | `	int encl   = '"' ;  /* Enclosure */` |
|      72 | 1858 | `	int escape = '\\';  /* Escape character */` |
|       - | 1859 | ``	int rcArg;   /* the handle screen's; the CSV screens below shadow `rc` */`` |
|      72 | 1860 | `	if( nArg < 1 ){` |
|       - | 1861 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1862 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1863 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1864 | `		return PH7_OK;` |
|       - | 1865 | `	}` |
|       - | 1866 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      72 | 1867 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcArg);` |
|      72 | 1868 | `	if( pDev == 0 ){` |
|       3 | 1869 | `		return rcArg;` |
|       - | 1870 | `	}` |
|      69 | 1871 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1872 | `		/* No reader: php answers FALSE and says nothing. */` |
|     ! 0 | 1873 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1874 | `		return PH7_OK;` |
|       - | 1875 | `	}` |
|      69 | 1876 | `	if( nArg > 2 ){` |
|       - | 1877 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|       - | 1878 | `		 * ordering) and even when the stream is already at EOF. */` |
|      67 | 1879 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|      67 | 1880 | `		if( rc != PH7_OK ){` |
|       7 | 1881 | `			return rc;` |
|       - | 1882 | `		}` |
|      61 | 1883 | `		if( nArg > 3 ){` |
|      61 | 1884 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|      61 | 1885 | `			if( rc != PH7_OK ){` |
|       3 | 1886 | `				return rc;` |
|       - | 1887 | `			}` |
|      59 | 1888 | `			if( nArg > 4 ){` |
|      59 | 1889 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|      59 | 1890 | `				if( rc != PH7_OK ){` |
|       3 | 1891 | `					return rc;` |
|       - | 1892 | `				}` |
|      28 | 1893 | `			}` |
|      28 | 1894 | `		}` |
|      28 | 1895 | `	}` |
|      59 | 1896 | `	nLen = -1;` |
|      59 | 1897 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 1898 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|       - | 1899 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|       - | 1900 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|      49 | 1901 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      49 | 1902 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|       3 | 1903 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1904 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|       - | 1905 | `		}` |
|       - | 1906 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|      23 | 1907 | `	}` |
|       - | 1908 | `	/* Perform the requested operation */` |
|      57 | 1909 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      57 | 1910 | `	if( n < 1 ){` |
|       - | 1911 | `		/* EOF or IO error,return FALSE */` |
|      13 | 1912 | `		StreamReportReadFailure(pCtx,pDev);` |
|      13 | 1913 | `		ph7_result_bool(pCtx,0);` |
|       7 | 1914 | `	}else{` |
|       - | 1915 | `		ph7_value *pArray;` |
|       - | 1916 | `		SyBlob sRec;` |
|       - | 1917 | `		PH7_CsvScan sScan;` |
|       - | 1918 | `		/* Create our array */` |
|      45 | 1919 | `		pArray = ph7_context_new_array(pCtx);` |
|      45 | 1920 | `		if( pArray == 0 ){` |
|     ! 0 | 1921 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 1922 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1923 | `			return PH7_OK;` |
|       - | 1924 | `		}` |
|       - | 1925 | `		/* A RECORD is not a line: an enclosure that is still open when the line` |
|       - | 1926 | `		 * ends means the value contains the newline and the record continues on` |
|       - | 1927 | `		 * the next one. Parsing a single line and stopping split such a value` |
|       - | 1928 | `		 * across two rows, with the halves quoted wrong. The whole record is` |
|       - | 1929 | `		 * gathered FIRST and parsed once -- the scan below carries its position` |
|       - | 1930 | `		 * across the appends, so a stray quote costs one pass over the file` |
|       - | 1931 | `		 * rather than one per line. */` |
|      45 | 1932 | `		SyBlobInit(&sRec,&pCtx->pVm->sAllocator);` |
|      45 | 1933 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|      45 | 1934 | `		PH7_CsvScanInit(&sScan);` |
|      55 | 1935 | `		while( PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|      27 | 1936 | `				SyBlobLength(&sRec),delim,encl,escape) ){` |
|      13 | 1937 | `			if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|       - | 1938 | `				/* The parser measures in int; stop rather than wrap negative. */` |
|     ! 0 | 1939 | `				break;` |
|       - | 1940 | `			}` |
|       - | 1941 | `			/* Continuation reads are NOT capped by $length: php's limit applies` |
|       - | 1942 | `			 * to the first read of the record, and reusing it here ended the` |
|       - | 1943 | `			 * record on a chunk boundary in the middle of a quoted value. */` |
|      13 | 1944 | `			n = StreamReadLine(pDev,&zLine,0);` |
|      13 | 1945 | `			if( n < 1 ){` |
|       - | 1946 | `				/* EOF inside the enclosure: php answers what it has. */` |
|       3 | 1947 | `				break;` |
|       - | 1948 | `			}` |
|      11 | 1949 | `			SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|       1 | 1950 | `		}` |
|      67 | 1951 | `		PH7_ProcessCsv(pArray,(const char *)SyBlobData(&sRec),` |
|      44 | 1952 | `			(int)SyBlobLength(&sRec),delim,encl,escape,0);` |
|      45 | 1953 | `		SyBlobRelease(&sRec);` |
|       - | 1954 | `		/* Return the freshly created array  */` |
|      45 | 1955 | `		ph7_result_value(pCtx,pArray);` |
|       - | 1956 | `	}` |
|      57 | 1957 | `	return PH7_OK;` |
|      37 | 1958 | `}` |
|       - | 1959 | `/*` |
|       - | 1960 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|       - | 1961 | ` *  Gets line from file pointer and strip HTML tags.` |
|       - | 1962 | ` * Parameters` |
|       - | 1963 | ` * $handle` |
|       - | 1964 | ` *   The file pointer.` |
|       - | 1965 | ` * $length` |
|       - | 1966 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - | 1967 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - | 1968 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - | 1969 | ` *  the end of the line.` |
|       - | 1970 | ` * $allowable_tags` |
|       - | 1971 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|       - | 1972 | ` * Return` |
|       - | 1973 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|       - | 1974 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|       - | 1975 | ` */` |
|       2 | 1976 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1977 | `{` |
|       - | 1978 | `	const char *zLine;` |
|       - | 1979 | `	io_private *pDev;` |
|       - | 1980 | `	ph7_int64 n,nLen;` |
|       3 | 1981 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|       - | 1982 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1983 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1984 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1985 | `		return PH7_OK;` |
|       - | 1986 | `	}` |
|       - | 1987 | `	/* Extract our private data */` |
|       3 | 1988 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|       - | 1989 | `	/* Make sure we are dealing with a valid io_private instance */` |
|       3 | 1990 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 1991 | `		/*Expecting an IO handle */` |
|     ! 0 | 1992 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 1993 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1994 | `		return PH7_OK;` |
|       - | 1995 | `	}` |
|       3 | 1996 | `	if( !StreamHasReader(pDev) ){` |
|       - | 1997 | `		/* No reader: php answers FALSE and says nothing. */` |
|     ! 0 | 1998 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1999 | `		return PH7_OK;` |
|       - | 2000 | `	}` |
|       3 | 2001 | `	nLen = -1;` |
|       3 | 2002 | `	if( nArg > 1 ){` |
|       - | 2003 | `		/* Maximum data to read */` |
|     ! 0 | 2004 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     ! 0 | 2005 | `	}` |
|       - | 2006 | `	/* Perform the requested operation */` |
|       3 | 2007 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|       3 | 2008 | `	if( n < 1 ){` |
|       - | 2009 | `		/* EOF or IO error,return FALSE */` |
|     ! 0 | 2010 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2011 | `	}else{` |
|       3 | 2012 | `		const char *zTaglist = 0;` |
|       3 | 2013 | `		int nTaglen = 0;` |
|       3 | 2014 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|       - | 2015 | `			/* Allowed tag */` |
|     ! 0 | 2016 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|     ! 0 | 2017 | `		}` |
|       - | 2018 | `		/* Process data just read */` |
|       3 | 2019 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen,0);` |
|       - | 2020 | `	}` |
|       3 | 2021 | `	return PH7_OK;` |
|       2 | 2022 | `}` |
|       - | 2023 | `/*` |
|       - | 2024 | ` * string readdir(resource $dir_handle)` |
|       - | 2025 | ` *   Read entry from directory handle.` |
|       - | 2026 | ` * Parameter` |
|       - | 2027 | ` *  $dir_handle` |
|       - | 2028 | ` *   The directory handle resource previously opened with opendir().` |
|       - | 2029 | ` * Return` |
|       - | 2030 | ` *  Returns the filename on success or FALSE on failure.` |
|       - | 2031 | ` */` |
|   18171 | 2032 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2033 | `{` |
|       - | 2034 | `	const ph7_io_stream *pStream;` |
|       - | 2035 | `	io_private *pDev;` |
|       - | 2036 | `	int rc;` |
|   18176 | 2037 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|   18176 | 2038 | `	if( pDev == 0 ){` |
|      19 | 2039 | `		return rc;` |
|       - | 2040 | `	}` |
|       - | 2041 | `	/* Point to the target IO stream device */` |
|   18158 | 2042 | `	pStream = pDev->pStream;` |
|   18158 | 2043 | `	if( pStream == 0 \|\| pStream->xReadDir == 0 ){` |
|       - | 2044 | `		/* No entries to give: php's readdir() answers FALSE in silence. */` |
|     ! 0 | 2045 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2046 | `		return PH7_OK;` |
|       - | 2047 | `	}` |
|   18158 | 2048 | `	ph7_result_bool(pCtx,0);` |
|       - | 2049 | `	/* Perform the requested operation */` |
|   18158 | 2050 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   18158 | 2051 | `	if( rc != PH7_OK ){` |
|       - | 2052 | `		/* Return FALSE */` |
|    1774 | 2053 | `		ph7_result_bool(pCtx,0);` |
|     889 | 2054 | `	}else{` |
|       - | 2055 | `		/* php's directory stream moves by one record per entry it PRODUCED --` |
|       - | 2056 | `		 * the read that finds the end moves nothing -- and that product is the` |
|       - | 2057 | `		 * only thing ftell() on a directory handle reports. */` |
|   16389 | 2058 | `		pDev->iPos += PHL_DIR_RECORD;` |
|       - | 2059 | `	}` |
|   18158 | 2060 | `	return PH7_OK;` |
|    9087 | 2061 | `}` |
|       - | 2062 | `/*` |
|       - | 2063 | ` * void rewinddir(resource $dir_handle)` |
|       - | 2064 | ` *   Rewind directory handle.` |
|       - | 2065 | ` * Parameter` |
|       - | 2066 | ` *  $dir_handle` |
|       - | 2067 | ` *   The directory handle resource previously opened with opendir().` |
|       - | 2068 | ` * Return` |
|       - | 2069 | ` *  FALSE on failure.` |
|       - | 2070 | ` */` |
|      12 | 2071 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2072 | `{` |
|       - | 2073 | `	const ph7_io_stream *pStream;` |
|       - | 2074 | `	io_private *pDev;` |
|       - | 2075 | `	int rc;` |
|      15 | 2076 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|      15 | 2077 | `	if( pDev == 0 ){` |
|       3 | 2078 | `		return rc;` |
|       - | 2079 | `	}` |
|       - | 2080 | `	/* Point to the target IO stream device */` |
|      13 | 2081 | `	pStream = pDev->pStream;` |
|      13 | 2082 | `	if( pStream == 0 \|\| pStream->xRewindDir == 0 ){` |
|       - | 2083 | `		/* Nothing to rewind, and php says nothing about it. */` |
|     ! 0 | 2084 | `		return PH7_OK;` |
|       - | 2085 | `	}` |
|       - | 2086 | `	/* Perform the requested operation */` |
|      13 | 2087 | `	pStream->xRewindDir(pDev->pHandle);` |
|      13 | 2088 | `	return PH7_OK;` |
|       9 | 2089 | ` }` |
|       - | 2090 | `/* Forward declaration */` |
|       - | 2091 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|       - | 2092 | `/*` |
|       - | 2093 | ` * void closedir(resource $dir_handle)` |
|       - | 2094 | ` *   Close directory handle.` |
|       - | 2095 | ` * Parameter` |
|       - | 2096 | ` *  $dir_handle` |
|       - | 2097 | ` *   The directory handle resource previously opened with opendir().` |
|       - | 2098 | ` * Return` |
|       - | 2099 | ` *  FALSE on failure.` |
|       - | 2100 | ` */` |
|    1787 | 2101 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2102 | `{` |
|       - | 2103 | `	const ph7_io_stream *pStream;` |
|       - | 2104 | `	io_private *pDev;` |
|       - | 2105 | `	int rc;` |
|    1792 | 2106 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|    1792 | 2107 | `	if( pDev == 0 ){` |
|       5 | 2108 | `		return rc;` |
|       - | 2109 | `	}` |
|       - | 2110 | `	/* Point to the target IO stream device */` |
|    1788 | 2111 | `	pStream = pDev->pStream;` |
|    1788 | 2112 | `	if( pStream == 0 \|\| pStream->xCloseDir == 0 ){` |
|       - | 2113 | `		/* Nothing to close, and php says nothing about it. */` |
|     ! 0 | 2114 | `		return PH7_OK;` |
|       - | 2115 | `	}` |
|       - | 2116 | `	/* Perform the requested operation */` |
|    1788 | 2117 | `	PH7_StreamFilterReleaseChains(pDev);` |
|    1788 | 2118 | `	pStream->xCloseDir(pDev->pHandle);` |
|       - | 2119 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|    1788 | 2120 | `	MarkIOPrivateClosed(pDev);` |
|    1788 | 2121 | `	if( pCtx->pVm->pLastDir == (void *)pDev ){` |
|       - | 2122 | `		/* php's fallback is the last opened stream, not the last LIVE one: once` |
|       - | 2123 | `		 * it is closed, readdir() with no argument is "No resource supplied". */` |
|    1786 | 2124 | `		pCtx->pVm->pLastDir = 0;` |
|     890 | 2125 | `	}` |
|    1788 | 2126 | `	return PH7_OK;` |
|     898 | 2127 | ` }` |
|       - | 2128 | `/*` |
|       - | 2129 | ` * resource opendir(string $path[,resource $context])` |
|       - | 2130 | ` *  Open directory handle.` |
|       - | 2131 | ` * Parameters` |
|       - | 2132 | ` * $path` |
|       - | 2133 | ` *   The directory path that is to be opened.` |
|       - | 2134 | ` * $context` |
|       - | 2135 | ` *   A context stream resource.` |
|       - | 2136 | ` * Return` |
|       - | 2137 | ` *  A directory handle resource on success,or FALSE on failure.` |
|       - | 2138 | ` */` |
|    1839 | 2139 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2140 | `{` |
|       - | 2141 | `	const ph7_io_stream *pStream;` |
|       - | 2142 | `	const char *zPath,*zAsked;` |
|       - | 2143 | `	io_private *pDev;` |
|    1844 | 2144 | `	int iLen,rc,bThrew = 0;` |
|    1844 | 2145 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 2146 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2147 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|     ! 0 | 2148 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2149 | `		return PH7_OK;` |
|       - | 2150 | `	}` |
|       - | 2151 | `	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it` |
|       - | 2152 | `	 * here — dir_opendir() over a userland wrapper is not dispatched (§7.4` |
|       - | 2153 | `	 * slice-2 (e)) — but the refusal is the argument's contract. */` |
|    1844 | 2154 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    1844 | 2155 | `	if( bThrew ){` |
|       3 | 2156 | `		return PH7_OK;` |
|       - | 2157 | `	}` |
|       - | 2158 | `	/* Extract the target path */` |
|    1842 | 2159 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|    1842 | 2160 | `	if( iLen < 1 ){` |
|       - | 2161 | `		/* php answers FALSE for an empty directory name and says nothing about` |
|       - | 2162 | `		 * it -- the ValueError its file openers raise is not this door's. */` |
|       2 | 2163 | `		ph7_result_bool(pCtx,0);` |
|       2 | 2164 | `		return PH7_OK;` |
|       - | 2165 | `	}` |
|       - | 2166 | `	/* php names the path AS WRITTEN in every diagnostic below, scheme included,` |
|       - | 2167 | `	 * and the device lookup advances zPath past that scheme. */` |
|    1840 | 2168 | `	zAsked = zPath;` |
|       - | 2169 | `	/* Try to extract a stream */` |
|    1840 | 2170 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    1840 | 2171 | `	if( pStream == 0 ){` |
|     ! 0 | 2172 | `		VfsThrowNoDeviceWarning(pCtx,zPath,TRUE);` |
|     ! 0 | 2173 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2174 | `		return PH7_OK;` |
|       - | 2175 | `	}` |
|    1840 | 2176 | `	if( pStream->xOpenDir == 0 ){` |
|       - | 2177 | `		/* php words a wrapper with no directory opener as an ordinary failed` |
|       - | 2178 | ``		 * open whose reason is `not implemented` -- the same sentence a missing`` |
|       - | 2179 | `		 * directory gets, so a caller's error handling does not have to know` |
|       - | 2180 | `		 * that this one is about the WRAPPER. */` |
|      18 | 2181 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: not implemented",` |
|       5 | 2182 | `			ph7_function_name(pCtx),zAsked);` |
|      13 | 2183 | `		ph7_result_bool(pCtx,0);` |
|      13 | 2184 | `		return PH7_OK;` |
|       - | 2185 | `	}` |
|       - | 2186 | `	/* Allocate a new IO private instance */` |
|    1830 | 2187 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    1830 | 2188 | `	if( pDev == 0 ){` |
|     ! 0 | 2189 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 2190 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2191 | `		return PH7_OK;` |
|       - | 2192 | `	}` |
|       - | 2193 | `	/* Initialize the structure */` |
|    1830 | 2194 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|       - | 2195 | `	/* Open the target directory. A device reaches the VM only through this` |
|       - | 2196 | `	 * argument -- its xOpenDir has no vm parameter -- and glob:// needs one to` |
|       - | 2197 | `	 * walk the filesystem and hold what it finds, so a caller that passed no` |
|       - | 2198 | `	 * context hands over a synthesized stack value carrying the VM, exactly as` |
|       - | 2199 | `	 * PH7_StreamOpenHandle() does for the byte-stream openers. */` |
|       - | 2200 | `	{` |
|       - | 2201 | `		ph7_value sDummy;` |
|    1830 | 2202 | `		ph7_value *pRes = nArg > 1 ? apArg[1] : 0;` |
|    1830 | 2203 | `		if( pRes == 0 ){` |
|     396 | 2204 | `			PH7_MemObjInit(pCtx->pVm,&sDummy);` |
|     396 | 2205 | `			pRes = &sDummy;` |
|     195 | 2206 | `		}` |
|    1830 | 2207 | `		rc = pStream->xOpenDir(zPath,pRes,&pDev->pHandle);` |
|    1830 | 2208 | `		if( pRes == &sDummy ){` |
|     396 | 2209 | `			PH7_MemObjRelease(&sDummy);` |
|     195 | 2210 | `		}` |
|       - | 2211 | `	}` |
|    1830 | 2212 | `	if( rc != PH7_OK ){` |
|       - | 2213 | ``		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No`` |
|       - | 2214 | ``		 * such file or directory` — and PHL returned FALSE in silence. The message`` |
|       - | 2215 | `` 		 * names the ACTIVE function, which is how dir() gets php's `dir(...)` `` |
|       - | 2216 | `		 * wording out of the same call.` |
|       - | 2217 | `		 *` |
|       - | 2218 | `		 * A userland wrapper's refusal is not an errno: php names the METHOD it` |
|       - | 2219 | `		 * called and whether the wrapper has one at all, the same way a failed` |
|       - | 2220 | `		 * stream_open() is reported. */` |
|       - | 2221 | `		char zWhy[160];` |
|      47 | 2222 | `		const char *zReason = PH7_StreamUserDirReason(pCtx->pVm,pStream,zWhy,(int)sizeof(zWhy))` |
|      42 | 2223 | `			? zWhy : VfsStrerror(errno);` |
|       - | 2224 | `#ifdef __WINNT__` |
|       5 | 2225 | `		if( pStream == &sWinFileStream ){` |
|       - | 2226 | `			/* php's plain-files opener on Windows warns with the system's own` |
|       - | 2227 | `			 * reason first, and only then fails the way every platform does. */` |
|       - | 2228 | `			char zSys[256];` |
|       5 | 2229 | `			int iSaved = errno;` |
|       5 | 2230 | `			unsigned long nCode = PH7_WinOpenDirReason(zSys,(int)sizeof(zSys));` |
|       5 | 2231 | `			if( nCode ){` |
|       5 | 2232 | `				PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s (code: %lu)",` |
|       - | 2233 | `					ph7_function_name(pCtx),zAsked,zSys,nCode);` |
|       - | 2234 | `			}` |
|       5 | 2235 | `			errno = iSaved;` |
|       - | 2236 | `		}` |
|       - | 2237 | `#endif` |
|      68 | 2238 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",` |
|      21 | 2239 | `			ph7_function_name(pCtx),zAsked,zReason);` |
|      47 | 2240 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      47 | 2241 | `		ph7_result_bool(pCtx,0);` |
|      26 | 2242 | `	}else{` |
|       - | 2243 | `		/* php's directory handles carry a mode and NO uri, and name their own` |
|       - | 2244 | ``		 * ops `dir` rather than the byte-stream STDIO. */`` |
|    1788 | 2245 | `		SetIOPrivateOpenedAs(pDev,0,0,"r",1);` |
|    1788 | 2246 | `		pDev->bDir = 1;` |
|       - | 2247 | `		/* php remembers this one as the "last opened directory stream", which is` |
|       - | 2248 | `		 * what readdir()/rewinddir()/closedir() reach for when they are given` |
|       - | 2249 | `		 * null. Only opendir() sets it; dir() goes through here too. */` |
|    1788 | 2250 | `		pCtx->pVm->pLastDir = (void *)pDev;` |
|       - | 2251 | `		/* Return the handle as a resource */` |
|    1788 | 2252 | `		ph7_result_resource(pCtx,pDev);` |
|       - | 2253 | `	}` |
|    1830 | 2254 | `	return PH7_OK;` |
|     924 | 2255 | `}` |
|       - | 2256 | `/*` |
|       - | 2257 | ``  * `dir(string $directory, $context = null): Directory\|false` `` |
|       - | 2258 | ` *` |
|       - | 2259 | ` * php's own dir() opens the stream and fills the object itself, which is why its` |
|       - | 2260 | ` * class needs no constructor. The open goes through the engine's opendir builtin` |
|       - | 2261 | `` * with THIS context, so the failure warning names `dir(...)` exactly as php's`` |
|       - | 2262 | ` * does; a failed open is FALSE, where the chunk's version handed back a Directory` |
|       - | 2263 | `` * whose handle was `false`.`` |
|       - | 2264 | ` */` |
|       8 | 2265 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2266 | `{` |
|       - | 2267 | `	ph7_class_instance *pObj;` |
|       - | 2268 | `	ph7_class *pClass;` |
|       - | 2269 | `	ph7_value *pRet;` |
|       - | 2270 | `	int rc;` |
|      10 | 2271 | `	rc = PH7_builtin_opendir(pCtx,nArg,apArg);` |
|      10 | 2272 | `	if( rc != PH7_OK ){` |
|     ! 0 | 2273 | `		return rc;` |
|       - | 2274 | `	}` |
|      10 | 2275 | `	pRet = pCtx->pRet;` |
|      10 | 2276 | `	if( pRet == 0 \|\| (pRet->iFlags & MEMOBJ_RES) == 0 ){` |
|       3 | 2277 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2278 | `		return PH7_OK;` |
|       - | 2279 | `	}` |
|       8 | 2280 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);` |
|       8 | 2281 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|       8 | 2282 | `	if( pObj == 0 ){` |
|     ! 0 | 2283 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2284 | `		return PH7_OK;` |
|       - | 2285 | `	}` |
|       - | 2286 | `	/* php's order: the path first, then the handle (var_dump shows both). */` |
|       - | 2287 | `	{` |
|       8 | 2288 | `		int nPath = 0;` |
|       8 | 2289 | `		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";` |
|       8 | 2290 | `		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);` |
|       - | 2291 | `	}` |
|       8 | 2292 | `	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);` |
|       8 | 2293 | `	PH7_NativeResultObject(pCtx,pObj);` |
|       8 | 2294 | `	return PH7_OK;` |
|       6 | 2295 | `}` |
|       - | 2296 | `/*` |
|       - | 2297 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|       - | 2298 | ` *  Reads a file and writes it to the output buffer.` |
|       - | 2299 | ` * Parameters` |
|       - | 2300 | ` *  $filename` |
|       - | 2301 | ` *   The filename being read.` |
|       - | 2302 | ` *  $use_include_path` |
|       - | 2303 | ` *   You can use the optional second parameter and set it to` |
|       - | 2304 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - | 2305 | ` *  $context` |
|       - | 2306 | ` *   A context stream resource.` |
|       - | 2307 | ` * Return` |
|       - | 2308 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|       - | 2309 | ` */` |
|       - | 2310 | `/*` |
|       - | 2311 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|       - | 2312 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|       - | 2313 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|       - | 2314 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|       - | 2315 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|       - | 2316 | ` * trailing reason comes from.` |
|       - | 2317 | ` */` |
|      18 | 2318 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2319 | `{` |
|      21 | 2320 | `	int use_include  = FALSE;` |
|       - | 2321 | `	const ph7_io_stream *pStream;` |
|       - | 2322 | `	ph7_int64 n,nRead;` |
|       - | 2323 | `	const char *zFile;` |
|       - | 2324 | `	char zBuf[8192];` |
|       - | 2325 | `	void *pHandle;` |
|       - | 2326 | `	phl_stream_ctx *pCtxRes;` |
|      21 | 2327 | `	int rc,nLen,bThrew = 0;` |
|      21 | 2328 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 2329 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2330 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 2331 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2332 | `		return PH7_OK;` |
|       - | 2333 | `	}` |
|       - | 2334 | `	/* Extract the file path */` |
|      21 | 2335 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      21 | 2336 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 2337 | `		return PH7_OK;` |
|       - | 2338 | `	}` |
|       - | 2339 | `	/* Point to the target IO stream device */` |
|      19 | 2340 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      19 | 2341 | `	if( pStream == 0 ){` |
|     ! 0 | 2342 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 2343 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2344 | `		return PH7_OK;` |
|       - | 2345 | `	}` |
|      19 | 2346 | `	if( nArg > 1 ){` |
|       6 | 2347 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|       2 | 2348 | `	}` |
|       - | 2349 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - | 2350 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - | 2351 | `	 * The armed one describes exactly this open. */` |
|      19 | 2352 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      19 | 2353 | `	if( bThrew ){` |
|       6 | 2354 | `		return PH7_OK;` |
|       - | 2355 | `	}` |
|      15 | 2356 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 2357 | `	/* Try to open the file in read-only mode */` |
|      21 | 2358 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|       6 | 2359 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      15 | 2360 | `	if( pHandle == 0 ){` |
|      11 | 2361 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      11 | 2362 | `		ph7_result_bool(pCtx,0);` |
|      11 | 2363 | `		return PH7_OK;` |
|       - | 2364 | `	}` |
|       - | 2365 | `	/* Perform the requested operation */` |
|       5 | 2366 | `	nRead = 0;` |
|       4 | 2367 | `	for(;;){` |
|       9 | 2368 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|       9 | 2369 | `		if( n < 1 ){` |
|       - | 2370 | `			/* EOF or IO error,break immediately */` |
|       5 | 2371 | `			break;` |
|       - | 2372 | `		}` |
|       - | 2373 | `		/* Output data */` |
|       5 | 2374 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|       5 | 2375 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2376 | `			break;` |
|       - | 2377 | `		}` |
|       - | 2378 | `		/* Increment counter */` |
|       5 | 2379 | `		nRead += n;` |
|       1 | 2380 | `	}` |
|       - | 2381 | `	/* Close the stream */` |
|       5 | 2382 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 2383 | `	/* Total number of bytes readen */` |
|       5 | 2384 | `	ph7_result_int64(pCtx,nRead);` |
|       5 | 2385 | `	return PH7_OK;` |
|      12 | 2386 | `}` |
|       - | 2387 | `/*` |
|       - | 2388 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|       - | 2389 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|       - | 2390 | ` *  Reads entire file into a string.` |
|       - | 2391 | ` * Parameters` |
|       - | 2392 | ` *  $filename` |
|       - | 2393 | ` *   The filename being read.` |
|       - | 2394 | ` *  $use_include_path` |
|       - | 2395 | ` *   You can use the optional second parameter and set it to` |
|       - | 2396 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - | 2397 | ` *  $context` |
|       - | 2398 | ` *   A context stream resource.` |
|       - | 2399 | ` *  $offset` |
|       - | 2400 | ` *   The offset where the reading starts on the original stream.` |
|       - | 2401 | ` *  $maxlen` |
|       - | 2402 | ` *    Maximum length of data read. The default is to read until end of file` |
|       - | 2403 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|       - | 2404 | ` * Return` |
|       - | 2405 | ` *   The function returns the read data or FALSE on failure.` |
|       - | 2406 | ` */` |
|   10240 | 2407 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2408 | `{` |
|       - | 2409 | `	const ph7_io_stream *pStream;` |
|       - | 2410 | `	ph7_int64 n,nRead,nMaxlen;` |
|   10245 | 2411 | `	int use_include  = FALSE;` |
|       - | 2412 | `	const char *zFile;` |
|       - | 2413 | `	char zBuf[8192];` |
|       - | 2414 | `	void *pHandle;` |
|       - | 2415 | `	phl_stream_ctx *pCtxRes;` |
|   10245 | 2416 | `	int nLen,bThrew = 0;` |
|       - | 2417 |  |
|   10245 | 2418 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 2419 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2420 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 2421 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2422 | `		return PH7_OK;` |
|       - | 2423 | `	}` |
|       - | 2424 | `	/* Extract the file path */` |
|   10245 | 2425 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|   10245 | 2426 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 2427 | `		return PH7_OK;` |
|       - | 2428 | `	}` |
|       - | 2429 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|       - | 2430 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|       - | 2431 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|       - | 2432 | `	 * the whole file. */` |
|   10243 | 2433 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      27 | 2434 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|       5 | 2435 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2436 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|       - | 2437 | `		}` |
|      11 | 2438 | `	}` |
|       - | 2439 | `	/* Point to the target IO stream device */` |
|   10239 | 2440 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   10239 | 2441 | `	if( pStream == 0 ){` |
|      24 | 2442 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|      24 | 2443 | `		ph7_result_bool(pCtx,0);` |
|      24 | 2444 | `		return PH7_OK;` |
|       - | 2445 | `	}` |
|   10217 | 2446 | `	nMaxlen = -1;` |
|   10217 | 2447 | `	if( nArg > 1 ){` |
|     217 | 2448 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     107 | 2449 | `	}` |
|       - | 2450 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - | 2451 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - | 2452 | `	 * The armed one describes exactly this open. */` |
|   10217 | 2453 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|   10217 | 2454 | `	if( bThrew ){` |
|       5 | 2455 | `		return PH7_OK;` |
|       - | 2456 | `	}` |
|   10213 | 2457 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 2458 | `	/* Try to open the file in read-only mode */` |
|   10213 | 2459 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|   10213 | 2460 | `	if( pHandle == 0 ){` |
|      88 | 2461 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      88 | 2462 | `		ph7_result_bool(pCtx,0);` |
|      88 | 2463 | `		return PH7_OK;` |
|       - | 2464 | `	}` |
|   10130 | 2465 | `	if( nArg > 3 ){` |
|       - | 2466 | `		/* Extract the offset */` |
|      25 | 2467 | `		n = ph7_value_to_int64(apArg[3]);` |
|      25 | 2468 | `		if( n > 0 ){` |
|       7 | 2469 | `			if( pStream->xSeek ){` |
|       - | 2470 | `				/* Seek to the desired offset */` |
|       7 | 2471 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|       3 | 2472 | `			}` |
|       3 | 2473 | `		}` |
|      25 | 2474 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       - | 2475 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|       - | 2476 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|       - | 2477 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|      23 | 2478 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      11 | 2479 | `		}` |
|      12 | 2480 | `	}` |
|       - | 2481 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|       - | 2482 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|       - | 2483 | `	 * through to the empty-string result below. */` |
|   10130 | 2484 | `	nRead = 0;` |
|   20421 | 2485 | `	while( nMaxlen != 0 ){` |
|       - | 2486 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|       - | 2487 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|       - | 2488 | `		 * otherwise overshoot and append past $length. */` |
|   20417 | 2489 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   20417 | 2490 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|      17 | 2491 | `			nAsk = nMaxlen - nRead;` |
|       8 | 2492 | `		}` |
|   20417 | 2493 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|   20417 | 2494 | `		if( n < 1 ){` |
|       - | 2495 | `			/* EOF or IO error,break immediately */` |
|   10112 | 2496 | `			break;` |
|       - | 2497 | `		}` |
|       - | 2498 | `		/* Append data */` |
|   10310 | 2499 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|       - | 2500 | `		/* Increment read counter */` |
|   10310 | 2501 | `		nRead += n;` |
|   10310 | 2502 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|       - | 2503 | `			/* Read limit reached */` |
|      15 | 2504 | `			break;` |
|       - | 2505 | `		}` |
|       5 | 2506 | `	}` |
|       - | 2507 | `	/* Close the stream */` |
|   10130 | 2508 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 2509 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|       - | 2510 | `	 * open failure, handled above); the read loop never set a string result, so` |
|       - | 2511 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   10130 | 2512 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     117 | 2513 | `		ph7_result_string(pCtx,"",0);` |
|      56 | 2514 | `	}` |
|   10130 | 2515 | `	return PH7_OK;` |
|    5123 | 2516 | `}` |
|       - | 2517 | `/*` |
|       - | 2518 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|       - | 2519 | ` *  Write a string to a file.` |
|       - | 2520 | ` * Parameters` |
|       - | 2521 | ` *  $filename` |
|       - | 2522 | ` *  Path to the file where to write the data.` |
|       - | 2523 | ` * $data` |
|       - | 2524 | ` *  The data to write(Must be a string).` |
|       - | 2525 | ` * $flags` |
|       - | 2526 | ` *  The value of flags can be any combination of the following` |
|       - | 2527 | ` * flags, joined with the binary OR (\|) operator.` |
|       - | 2528 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|       - | 2529 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|       - | 2530 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|       - | 2531 | ` * context` |
|       - | 2532 | ` *  A context stream resource.` |
|       - | 2533 | ` * Return` |
|       - | 2534 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|       - | 2535 | ` */` |
|       - | 2536 | `/*` |
|       - | 2537 | ` * Append a buffer to a file, creating it when absent, and raise php's open` |
|       - | 2538 | `` * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's`` |
|       - | 2539 | ` * name when it cannot be opened. Returns PH7_OK or -1.` |
|       - | 2540 | ` *` |
|       - | 2541 | ` * This is error_log()'s message_type 3, factored here because that is where the` |
|       - | 2542 | ` * stream device, the open flags and the warning shape already live.` |
|       - | 2543 | ` */` |
|       6 | 2544 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)` |
|       1 | 2545 | `{` |
|       - | 2546 | `	const ph7_io_stream *pStream;` |
|       - | 2547 | `	void *pHandle;` |
|       - | 2548 | `	int nPath;` |
|       7 | 2549 | `	if( zFile == 0 \|\| zFile[0] == 0 ){` |
|     ! 0 | 2550 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 2551 | `		return -1;` |
|       - | 2552 | `	}` |
|       7 | 2553 | `	nPath = (int)SyStrlen(zFile);` |
|       7 | 2554 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);` |
|       7 | 2555 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|     ! 0 | 2556 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 2557 | `		return -1;` |
|       - | 2558 | `	}` |
|       - | 2559 | `	/* php opens for WRITING only here ("ab"), and nothing reads back through the` |
|       - | 2560 | `	 * handle -- which is also the mode string a userland wrapper is handed. */` |
|      10 | 2561 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|       3 | 2562 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|       7 | 2563 | `	if( pHandle == 0 ){` |
|       3 | 2564 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 | 2565 | `		return -1;` |
|       - | 2566 | `	}` |
|       5 | 2567 | `	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){` |
|     ! 0 | 2568 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     ! 0 | 2569 | `		return -1;` |
|       - | 2570 | `	}` |
|       5 | 2571 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       5 | 2572 | `	return PH7_OK;` |
|       4 | 2573 | `}` |
|       - | 2574 | `/*` |
|       - | 2575 | ` * file_put_contents()'s array $data: php walks the array and writes the` |
|       - | 2576 | ` * elements one after another with nothing between them, casting each one` |
|       - | 2577 | `` * user-visibly -- so a nested array warns `Array to string conversion` and`` |
|       - | 2578 | ` * contributes "Array", and an element that is an object with no __toString()` |
|       - | 2579 | ` * is php's catchable Error and the file is left as the open truncated it.` |
|       - | 2580 | ` */` |
|       - | 2581 | `typedef struct VfsPutContentsJoin VfsPutContentsJoin;` |
|       - | 2582 | `struct VfsPutContentsJoin` |
|       - | 2583 | `{` |
|       - | 2584 | `	ph7_context *pCtx;` |
|       - | 2585 | `	SyBlob *pOut;` |
|       - | 2586 | `	sxi32 rcThrow;      /* Allocation failure only */` |
|       - | 2587 | `	ph7_class *pOwed;   /* First class that could not be coerced; raised after the write */` |
|       - | 2588 | `};` |
|      18 | 2589 | `static int VfsPutContentsWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 | 2590 | `{` |
|      19 | 2591 | `	VfsPutContentsJoin *pJn = (VfsPutContentsJoin *)pUserData;` |
|       - | 2592 | `	const char *zElem;` |
|       - | 2593 | `	int nElem;` |
|       - | 2594 | `	ph7_class *pBad;` |
|       9 | 2595 | `	SXUNUSED(pKey);` |
|       - | 2596 | `	/* php does not stop for the coercion's throw: the failing element contributes` |
|       - | 2597 | `	 * nothing and the ELEMENTS AFTER IT are still written, so` |
|       - | 2598 | ``	 * `file_put_contents($f,['A',$obj,'B'])` leaves "AB" in the file and reports`` |
|       - | 2599 | `	 * the Error afterwards -- which is why the raise is deferred to the caller. */` |
|      19 | 2600 | `	pBad = PH7_ValueToStringUVDefer(pJn->pCtx,pData,&zElem,&nElem);` |
|      19 | 2601 | `	if( pBad && pJn->pOwed == 0 ){` |
|       3 | 2602 | `		pJn->pOwed = pBad;` |
|       1 | 2603 | `	}` |
|      19 | 2604 | `	if( nElem > 0 && SyBlobAppend(pJn->pOut,(const void *)zElem,(sxu32)nElem) != SXRET_OK ){` |
|     ! 0 | 2605 | `		pJn->rcThrow = PH7_ContextMemoryError(pJn->pCtx);` |
|     ! 0 | 2606 | `		return PH7_ABORT;` |
|       - | 2607 | `	}` |
|      19 | 2608 | `	return PH7_OK;` |
|      10 | 2609 | `}` |
|   19307 | 2610 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2611 | `{` |
|   19312 | 2612 | `	int use_include  = FALSE;` |
|       - | 2613 | `	const ph7_io_stream *pStream;` |
|       - | 2614 | `	const char *zFile;` |
|       - | 2615 | `	const char *zData;` |
|       - | 2616 | `	int iOpenFlags;` |
|       - | 2617 | `	void *pHandle;` |
|       - | 2618 | `	phl_stream_ctx *pCtxRes;` |
|       - | 2619 | `	int iFlags;` |
|   19312 | 2620 | `	int nLen,bThrew = 0;` |
|       - | 2621 | `	SyBlob sJoin;        /* array/stream $data, joined (see below) */` |
|   19312 | 2622 | `	ph7_class *pOwed = 0;/* A $data element php stringifies to nothing and throws for */` |
|   19312 | 2623 | `	int bScalarBad = 0;  /* Scalar $data php cannot stringify: FALSE, and no throw */` |
|       - | 2624 |  |
|   19312 | 2625 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 2626 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2627 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 2628 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2629 | `		return PH7_OK;` |
|       - | 2630 | `	}` |
|       - | 2631 | `	/* Extract the file path */` |
|   19312 | 2632 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|   19312 | 2633 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 2634 | `		return PH7_OK;` |
|       - | 2635 | `	}` |
|       - | 2636 | `	/* Point to the target IO stream device */` |
|   19310 | 2637 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   19310 | 2638 | `	if( pStream == 0 ){` |
|     ! 0 | 2639 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 2640 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2641 | `		return PH7_OK;` |
|       - | 2642 | `	}` |
|       - | 2643 | ``	/* The bytes to write, which php reads from `mixed $data` in THREE shapes and`` |
|       - | 2644 | `	 * PHL string-cast wholesale -- so an ARRAY wrote the five bytes "Array" and a` |
|       - | 2645 | `	 * STREAM wrote "Resource id #N", both instead of the content the program` |
|       - | 2646 | `	 * meant. php JOINS an array's elements with nothing between them (its own` |
|       - | 2647 | `	 * comment calls it "an array-to-string with no glue"), casting each element` |
|       - | 2648 | `	 * user-visibly, and COPIES a stream handle's remaining contents. Anything` |
|       - | 2649 | `	 * else is the ordinary user-visible cast: an object hands over its` |
|       - | 2650 | `	 * __toString(), one without it is php's catchable Error.` |
|       - | 2651 | `	 *` |
|       - | 2652 | `	 * The joined bytes have to outlive this block, so they live in sJoin until` |
|       - | 2653 | `	 * the write below; the scalar path keeps pointing straight at the value. */` |
|   19310 | 2654 | `	SyBlobInit(&sJoin,&pCtx->pVm->sAllocator);` |
|   19310 | 2655 | `	if( ph7_value_is_array(apArg[1]) ){` |
|       - | 2656 | `		VfsPutContentsJoin sJn;` |
|       5 | 2657 | `		sJn.pCtx = pCtx;` |
|       5 | 2658 | `		sJn.pOut = &sJoin;` |
|       5 | 2659 | `		sJn.rcThrow = SXRET_OK;` |
|       5 | 2660 | `		sJn.pOwed = 0;` |
|       5 | 2661 | `		ph7_array_walk(apArg[1],VfsPutContentsWalker,&sJn);` |
|       5 | 2662 | `		if( sJn.rcThrow != SXRET_OK ){` |
|       - | 2663 | `			/* Allocation failure only -- a coercion carries on above. */` |
|     ! 0 | 2664 | `			SyBlobRelease(&sJoin);` |
|     ! 0 | 2665 | `			return sJn.rcThrow;` |
|       - | 2666 | `		}` |
|       5 | 2667 | `		pOwed = sJn.pOwed;` |
|       5 | 2668 | `		zData = (const char *)SyBlobData(&sJoin);` |
|       5 | 2669 | `		nLen = (int)SyBlobLength(&sJoin);` |
|   19308 | 2670 | `	}else if( ph7_value_is_resource(apArg[1]) ){` |
|       3 | 2671 | `		io_private *pSrc = (io_private *)ph7_value_to_resource(apArg[1]);` |
|       3 | 2672 | `		if( !IO_PRIVATE_INVALID(pSrc) && pSrc->pStream && pSrc->pStream->xRead ){` |
|       3 | 2673 | `			PH7_StreamReadWholeFile(pSrc->pHandle,pSrc->pStream,&sJoin);` |
|       1 | 2674 | `		}` |
|       3 | 2675 | `		zData = (const char *)SyBlobData(&sJoin);` |
|       3 | 2676 | `		nLen = (int)SyBlobLength(&sJoin);` |
|       2 | 2677 | `	}else{` |
|   19304 | 2678 | `		if( PH7_ValueToStringUVDefer(pCtx,apArg[1],&zData,&nLen) != 0 ){` |
|       - | 2679 | `			/* php's SCALAR $data path is not its array one: an object it cannot` |
|       - | 2680 | `			 * stringify raises NOTHING here -- the file is still opened and` |
|       - | 2681 | `			 * TRUNCATED, nothing is written, and the call answers FALSE. The` |
|       - | 2682 | `			 * asymmetry with the array branch above (which does throw) is php's. */` |
|       3 | 2683 | `			bScalarBad = 1;` |
|       1 | 2684 | `		}` |
|       - | 2685 | `	}` |
|       - | 2686 | `	/* php opens for WRITING only -- "wb", or "ab" once FILE_APPEND turns up below --` |
|       - | 2687 | `	 * and that is the mode string a userland wrapper is handed. */` |
|   19310 | 2688 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC;` |
|       - | 2689 | `	/* Extract the flags */` |
|   19310 | 2690 | `	iFlags = 0;` |
|   19310 | 2691 | `	if( nArg > 2 ){` |
|      11 | 2692 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|      11 | 2693 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|     ! 0 | 2694 | `			use_include = TRUE;` |
|     ! 0 | 2695 | `		}` |
|      11 | 2696 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|       - | 2697 | `			/* If the file already exists, append the data to the file` |
|       - | 2698 | `			 * instead of overwriting it.` |
|       - | 2699 | `			 */` |
|       3 | 2700 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|       - | 2701 | `			/* Append mode */` |
|       3 | 2702 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|       1 | 2703 | `		}` |
|       4 | 2704 | `	}` |
|       - | 2705 | `	/* FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|       - | 2706 | `	 * back to the default context" — which is why it needed one to exist. */` |
|   28958 | 2707 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",` |
|   19305 | 2708 | `		(iFlags & 0x10) != 0,&bThrew);` |
|   19310 | 2709 | `	if( bThrew ){` |
|       6 | 2710 | `		SyBlobRelease(&sJoin);` |
|       6 | 2711 | `		return PH7_OK;` |
|       - | 2712 | `	}` |
|   19306 | 2713 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|   28952 | 2714 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|    9646 | 2715 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));` |
|   19306 | 2716 | `	if( pHandle == 0 ){` |
|      13 | 2717 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      13 | 2718 | `		ph7_result_bool(pCtx,0);` |
|      13 | 2719 | `		SyBlobRelease(&sJoin);` |
|      13 | 2720 | `		return PH7_OK;` |
|       - | 2721 | `	}` |
|   19296 | 2722 | `	if( bScalarBad ){` |
|       - | 2723 | `		/* Opened and truncated, then refused — php's answer, and its order. */` |
|       3 | 2724 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2725 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|       3 | 2726 | `		SyBlobRelease(&sJoin);` |
|       3 | 2727 | `		return PH7_OK;` |
|       - | 2728 | `	}` |
|   19294 | 2729 | `	if( nLen < 1 ){` |
|       - | 2730 | `		/* Empty data, file is created/truncated */` |
|     221 | 2731 | `		ph7_result_int64(pCtx,0);` |
|     221 | 2732 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     221 | 2733 | `		SyBlobRelease(&sJoin);` |
|     221 | 2734 | `		if( pOwed ){` |
|     ! 0 | 2735 | `			return PH7_VmThrowException(pCtx,"Error",` |
|       - | 2736 | `				"Object of class %.*s could not be converted to string",` |
|     ! 0 | 2737 | `				(int)pOwed->sName.nByte,pOwed->sName.zString);` |
|       - | 2738 | `		}` |
|     221 | 2739 | `		return PH7_OK;` |
|       - | 2740 | `	}` |
|   19078 | 2741 | `	if( pStream->xWrite ){` |
|       - | 2742 | `		ph7_int64 n;` |
|   19076 | 2743 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|       - | 2744 | `			/* Try to acquire an exclusive lock */` |
|     ! 0 | 2745 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|     ! 0 | 2746 | `		}` |
|       - | 2747 | `		/* Perform the write operation */` |
|   19076 | 2748 | `		errno = 0;` |
|   19076 | 2749 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|   19076 | 2750 | `		if( n < 0 ){` |
|       - | 2751 | `			/* IO error,return FALSE — with php's write-failure diagnostic,` |
|       - | 2752 | `			 * which is a NOTICE and comes from the device rather than from` |
|       - | 2753 | ``			 * here: `file_put_contents('php://input','q')` is a silent false. */`` |
|       3 | 2754 | `			StreamReportRawWriteFailure(pCtx,pStream,(int)nLen,errno);` |
|       3 | 2755 | `			ph7_result_bool(pCtx,0);` |
|       2 | 2756 | `		}else{` |
|       - | 2757 | `			/* Total number of bytes written */` |
|   19074 | 2758 | `			ph7_result_int64(pCtx,n);` |
|       - | 2759 | `		}` |
|    9536 | 2760 | `	}else{` |
|       - | 2761 | ``		/* A wrapper with no writer at all: php's `Stream is not writable`,`` |
|       - | 2762 | `		 * the same notice fwrite() gives, and not a fatal of our own. */` |
|       3 | 2763 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|       3 | 2764 | `		ph7_result_bool(pCtx,0);` |
|       - | 2765 | `	}` |
|       - | 2766 | `	/* Close the handle */` |
|   19078 | 2767 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   19078 | 2768 | `	SyBlobRelease(&sJoin);` |
|   19078 | 2769 | `	if( pOwed ){` |
|       4 | 2770 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - | 2771 | `			"Object of class %.*s could not be converted to string",` |
|       2 | 2772 | `			(int)pOwed->sName.nByte,pOwed->sName.zString);` |
|       - | 2773 | `	}` |
|   19076 | 2774 | `	return PH7_OK;` |
|    9654 | 2775 | `}` |
|       - | 2776 | `/*` |
|       - | 2777 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|       - | 2778 | ` *  Reads entire file into an array.` |
|       - | 2779 | ` * Parameters` |
|       - | 2780 | ` *  $filename` |
|       - | 2781 | ` *   The filename being read.` |
|       - | 2782 | ` *  $flags` |
|       - | 2783 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|       - | 2784 | ` *   FILE_USE_INCLUDE_PATH` |
|       - | 2785 | ` *       Search for the file in the include_path.` |
|       - | 2786 | ` *   FILE_IGNORE_NEW_LINES` |
|       - | 2787 | ` *       Do not add newline at the end of each array element` |
|       - | 2788 | ` *   FILE_SKIP_EMPTY_LINES` |
|       - | 2789 | ` *       Skip empty lines` |
|       - | 2790 | ` *  $context` |
|       - | 2791 | ` *   A context stream resource.` |
|       - | 2792 | ` * Return` |
|       - | 2793 | ` *   The function returns the read data or FALSE on failure.` |
|       - | 2794 | ` */` |
|     146 | 2795 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2796 | `{` |
|       - | 2797 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|       - | 2798 | `	ph7_value *pArray,*pLine;` |
|       - | 2799 | `	const ph7_io_stream *pStream;` |
|     148 | 2800 | `	int use_include = 0;` |
|       - | 2801 | `	io_private *pDev;` |
|       - | 2802 | `	phl_stream_ctx *pCtxRes;` |
|       - | 2803 | `	ph7_int64 n;` |
|       - | 2804 | `	int iFlags;` |
|     148 | 2805 | `	int nLen,bThrew = 0;` |
|       - | 2806 |  |
|     148 | 2807 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 2808 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2809 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 2810 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2811 | `		return PH7_OK;` |
|       - | 2812 | `	}` |
|     148 | 2813 | `	iFlags = 0;` |
|     148 | 2814 | `	if( nArg > 1 ){` |
|       - | 2815 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|       - | 2816 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|       - | 2817 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|       - | 2818 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|       - | 2819 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|       - | 2820 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|       - | 2821 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|       - | 2822 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|       - | 2823 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|       - | 2824 | `		 * mask. */` |
|     130 | 2825 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     130 | 2826 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|      13 | 2827 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2828 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|       - | 2829 | `		}` |
|     118 | 2830 | `		iFlags = (int)nFlags;` |
|      58 | 2831 | `	}` |
|       - | 2832 | `	/* Resolved here for the same reason the flag mask is: a refused $context` |
|       - | 2833 | `	 * must not strand the io_private chunk allocated below.` |
|       - | 2834 | `	 * FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|       - | 2835 | `	 * back to the default context" — which is why it needed one to exist. */` |
|     203 | 2836 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",` |
|     134 | 2837 | `		(iFlags & 0x10) != 0,&bThrew);` |
|     136 | 2838 | `	if( bThrew ){` |
|       3 | 2839 | `		return PH7_OK;` |
|       - | 2840 | `	}` |
|       - | 2841 | `	/* Extract the file path */` |
|     134 | 2842 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|     134 | 2843 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 2844 | `		return PH7_OK;` |
|       - | 2845 | `	}` |
|       - | 2846 | `	/* Point to the target IO stream device */` |
|     132 | 2847 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     132 | 2848 | `	if( pStream == 0 ){` |
|     ! 0 | 2849 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 2850 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2851 | `		return PH7_OK;` |
|       - | 2852 | `	}` |
|       - | 2853 | `	/* Allocate a new IO private instance */` |
|     132 | 2854 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     132 | 2855 | `	if( pDev == 0 ){` |
|     ! 0 | 2856 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 2857 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2858 | `		return PH7_OK;` |
|       - | 2859 | `	}` |
|       - | 2860 | `	/* Initialize the structure */` |
|     132 | 2861 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     132 | 2862 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|       3 | 2863 | `		use_include = TRUE;` |
|       1 | 2864 | `	}` |
|       - | 2865 | `	/* Create the array and the working value */` |
|     132 | 2866 | `	pArray = ph7_context_new_array(pCtx);` |
|     132 | 2867 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     132 | 2868 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|     ! 0 | 2869 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 2870 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2871 | `		return PH7_OK;` |
|       - | 2872 | `	}` |
|     132 | 2873 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 2874 | `	/* Try to open the file in read-only mode */` |
|     132 | 2875 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     132 | 2876 | `	if( pDev->pHandle == 0 ){` |
|      12 | 2877 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      12 | 2878 | `		ph7_result_bool(pCtx,0);` |
|       - | 2879 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|       - | 2880 | `		 * as soon we return from this function.` |
|       - | 2881 | `		 */` |
|      12 | 2882 | `		return PH7_OK;` |
|       - | 2883 | `	}` |
|       - | 2884 | `	/* Perform the requested operation */` |
|     171 | 2885 | `	for(;;){` |
|       - | 2886 | `		/* Try to extract a line */` |
|     348 | 2887 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|     348 | 2888 | `		if( n < 1 ){` |
|       - | 2889 | `			/* EOF or IO error */` |
|     122 | 2890 | `			break;` |
|       - | 2891 | `		}` |
|       - | 2892 | `		/* Reset the cursor */` |
|     228 | 2893 | `		ph7_value_reset_string_cursor(pLine);` |
|       - | 2894 | `		/* Remove line ending if requested by the caller */` |
|     228 | 2895 | `		zPtr = zBuf;` |
|     228 | 2896 | `		zEnd = &zBuf[n];` |
|     228 | 2897 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|       - | 2898 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|       - | 2899 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|       - | 2900 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|       - | 2901 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|       - | 2902 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     171 | 2903 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     159 | 2904 | `				n--;` |
|     159 | 2905 | `				zEnd--;` |
|     159 | 2906 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|      13 | 2907 | `					n--;` |
|      13 | 2908 | `					zEnd--;` |
|       6 | 2909 | `				}` |
|      79 | 2910 | `			}` |
|      85 | 2911 | `		}` |
|     228 | 2912 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|       - | 2913 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|       - | 2914 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|       - | 2915 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|       - | 2916 | `			 * line (never zero-length, since the newline is still attached) was` |
|       - | 2917 | `			 * dropped too. Both are silent data loss from a read. */` |
|     147 | 2918 | `			if( zEnd <= zPtr ){` |
|       5 | 2919 | `				continue;` |
|       - | 2920 | `			}` |
|      71 | 2921 | `		}` |
|     224 | 2922 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|       - | 2923 | `		/* Insert line */` |
|     224 | 2924 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|       2 | 2925 | `	}` |
|       - | 2926 | `	/* Close the stream */` |
|     122 | 2927 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|       - | 2928 | `	/* Release the io_private instance */` |
|     122 | 2929 | `	ReleaseIOPrivate(pCtx,pDev);` |
|       - | 2930 | `	/* Return the created array */` |
|     122 | 2931 | `	ph7_result_value(pCtx,pArray);` |
|     122 | 2932 | `	return PH7_OK;` |
|      75 | 2933 | `}` |
|       - | 2934 | `/*` |
|       - | 2935 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|       - | 2936 | ` *  Makes a copy of the file source to dest.` |
|       - | 2937 | ` * Parameters` |
|       - | 2938 | ` *  $source` |
|       - | 2939 | ` *   Path to the source file.` |
|       - | 2940 | ` *  $dest` |
|       - | 2941 | ` *   The destination path. If dest is a URL, the copy operation` |
|       - | 2942 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|       - | 2943 | ` *  $context` |
|       - | 2944 | ` *   A context stream resource.` |
|       - | 2945 | ` * Return` |
|       - | 2946 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2947 | ` */` |
|      16 | 2948 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2949 | `{` |
|       - | 2950 | `	const ph7_io_stream *pSin,*pSout;` |
|       - | 2951 | `	const char *zFile;` |
|       - | 2952 | `	char zBuf[8192];` |
|       - | 2953 | `	void *pIn,*pOut;` |
|       - | 2954 | `	phl_stream_ctx *pCtxRes;` |
|       - | 2955 | `	ph7_int64 n;` |
|      18 | 2956 | `	int nLen,bThrew = 0;` |
|      18 | 2957 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|       - | 2958 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2959 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|     ! 0 | 2960 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2961 | `		return PH7_OK;` |
|       - | 2962 | `	}` |
|       - | 2963 | `	/* Extract the source name */` |
|      18 | 2964 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      18 | 2965 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 2966 | `		return PH7_OK;` |
|       - | 2967 | `	}` |
|       - | 2968 | `	/* Point to the target IO stream device */` |
|      16 | 2969 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      16 | 2970 | `	if( pSin == 0 ){` |
|     ! 0 | 2971 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 2972 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2973 | `		return PH7_OK;` |
|       - | 2974 | `	}` |
|       - | 2975 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - | 2976 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - | 2977 | `	 * The armed one describes exactly this open. */` |
|      16 | 2978 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      16 | 2979 | `	if( bThrew ){` |
|       3 | 2980 | `		return PH7_OK;` |
|       - | 2981 | `	}` |
|      14 | 2982 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 2983 | `	/* Try to open the source file in a read-only mode */` |
|      14 | 2984 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      14 | 2985 | `	if( pIn == 0 ){` |
|       3 | 2986 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 | 2987 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2988 | `		return PH7_OK;` |
|       - | 2989 | `	}` |
|       - | 2990 | `	/* Extract the destination name */` |
|      11 | 2991 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      11 | 2992 | `	if( nLen < 1 ){` |
|       - | 2993 | `		/* php's stream layer refuses this end of the copy the same way it` |
|       - | 2994 | `		 * refused the other -- with the source already open, which is what the` |
|       - | 2995 | `		 * close here is for. */` |
|       2 | 2996 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|       2 | 2997 | `		PH7_VfsEmptyPathRefused(pCtx,nLen);` |
|       2 | 2998 | `		return PH7_OK;` |
|       - | 2999 | `	}` |
|       - | 3000 | `	/* Point to the target IO stream device */` |
|       9 | 3001 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|       9 | 3002 | `	if( pSout == 0 ){` |
|     ! 0 | 3003 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 3004 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3005 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|     ! 0 | 3006 | `		return PH7_OK;` |
|       - | 3007 | `	}` |
|       9 | 3008 | `	if( pSout->xOpen != 0 && pSout->xWrite == 0 ){` |
|       - | 3009 | ``		/* php's copy() reaches the same `Stream is not writable` notice the`` |
|       - | 3010 | `		 * write doors do -- a destination wrapper with a stream opener and no` |
|       - | 3011 | `		 * writer behind it. A wrapper with no OPENER either (glob://) is` |
|       - | 3012 | `		 * refused one step earlier, by the open below, and says so. */` |
|     ! 0 | 3013 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|     ! 0 | 3014 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3015 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|     ! 0 | 3016 | `		return PH7_OK;` |
|       - | 3017 | `	}` |
|       - | 3018 | `	/* php hands the ONE context to both halves of the copy. */` |
|       9 | 3019 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 3020 | `	/* php opens the destination for WRITING only ("wb"). */` |
|      13 | 3021 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|       4 | 3022 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_WRONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|       9 | 3023 | `	if( pOut == 0 ){` |
|     ! 0 | 3024 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 3025 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3026 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|     ! 0 | 3027 | `		return PH7_OK;` |
|       - | 3028 | `	}` |
|       - | 3029 | `	/* Perform the requested operation */` |
|       - | 3030 | `	{` |
|       9 | 3031 | `	int bFailed = 0,iErr = 0,nAsked = 0;` |
|       7 | 3032 | `	for(;;){` |
|       - | 3033 | `		/* Read from source */` |
|      15 | 3034 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      15 | 3035 | `		if( n < 1 ){` |
|       - | 3036 | `			/* EOF or IO error,break immediately */` |
|       7 | 3037 | `			break;` |
|       - | 3038 | `		}` |
|       - | 3039 | `		/* Write to dest */` |
|       9 | 3040 | `		nAsked = (int)n;` |
|       9 | 3041 | `		errno = 0;` |
|       9 | 3042 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|       9 | 3043 | `		if( n < 1 ){` |
|       - | 3044 | `			/* php's copy() is FALSE when the destination would not take the` |
|       - | 3045 | `			 * bytes -- it answered TRUE here whatever the write did, so a copy` |
|       - | 3046 | `			 * onto a full filesystem reported success. The device's own notice` |
|       - | 3047 | `			 * goes with it, for the device that raises one. */` |
|       3 | 3048 | `			bFailed = 1;` |
|       3 | 3049 | `			iErr = errno;` |
|       3 | 3050 | `			break;` |
|       - | 3051 | `		}` |
|       1 | 3052 | `	}` |
|       - | 3053 | `	/* Close the streams */` |
|       9 | 3054 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|       9 | 3055 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|       9 | 3056 | `	if( bFailed ){` |
|       3 | 3057 | `		StreamReportRawWriteFailure(pCtx,pSout,nAsked,iErr);` |
|       1 | 3058 | `	}` |
|       9 | 3059 | `	ph7_result_bool(pCtx,!bFailed);` |
|       - | 3060 | `	}` |
|       9 | 3061 | `	return PH7_OK;` |
|      10 | 3062 | `}` |
|       - | 3063 | `/*` |
|       - | 3064 | ` * array fstat(resource $handle)` |
|       - | 3065 | ` *  Gets information about a file using an open file pointer.` |
|       - | 3066 | ` * Parameters` |
|       - | 3067 | ` *  $handle` |
|       - | 3068 | ` *   The file pointer.` |
|       - | 3069 | ` * Return` |
|       - | 3070 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|       - | 3071 | ` */` |
|      48 | 3072 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3073 | `{` |
|       - | 3074 | `	ph7_value *pArray,*pValue;` |
|       - | 3075 | `	const ph7_io_stream *pStream;` |
|       - | 3076 | `	io_private *pDev;` |
|       - | 3077 | `	int rc;` |
|      51 | 3078 | `	if( nArg < 1 ){` |
|       - | 3079 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3080 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 3081 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3082 | `		return PH7_OK;` |
|       - | 3083 | `	}` |
|       - | 3084 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      51 | 3085 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      51 | 3086 | `	if( pDev == 0 ){` |
|       3 | 3087 | `		return rc;` |
|       - | 3088 | `	}` |
|       - | 3089 | `	/* A php://filter handle is the stream underneath it, and that is the one` |
|       - | 3090 | `	 * with a stat to answer. */` |
|      48 | 3091 | `	pDev = PH7_StreamUnwrap(pDev);` |
|       - | 3092 | `	/* Point to the target IO stream device */` |
|      48 | 3093 | `	pStream = pDev->pStream;` |
|      48 | 3094 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xStat == 0 ){` |
|       - | 3095 | `		/* php's fstat() on a stream with no stat -- a directory handle, an` |
|       - | 3096 | `		 * http:// body, php://output -- is FALSE and no diagnostic. */` |
|       3 | 3097 | `		ph7_result_bool(pCtx,0);` |
|       3 | 3098 | `		return PH7_OK;` |
|       - | 3099 | `	}` |
|       - | 3100 | `	/* Create the array and the working value */` |
|      46 | 3101 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 3102 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      46 | 3103 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 | 3104 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 | 3105 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3106 | `		return PH7_OK;` |
|       - | 3107 | `	}` |
|       - | 3108 | `	/* Perform the requested operation */` |
|      46 | 3109 | `	if( pStream->xStat(pDev->pHandle,pArray,pValue) != PH7_OK ){` |
|       - | 3110 | `		/* php's fstat() is FALSE when the device could not answer, and says` |
|       - | 3111 | `		 * nothing about it -- php://output has no descriptor to stat. */` |
|       8 | 3112 | `		ph7_result_bool(pCtx,0);` |
|       8 | 3113 | `		return PH7_OK;` |
|       - | 3114 | `	}` |
|       - | 3115 | `	/* php answers the same thirteen fields twice -- numeric 0..12, then named` |
|       - | 3116 | `	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and` |
|       - | 3117 | `	 * had the same missing half. */` |
|       - | 3118 | `	{` |
|      40 | 3119 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      40 | 3120 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      36 | 3121 | `			ph7_result_value(pCtx,pFull);` |
|      36 | 3122 | `			return PH7_OK;` |
|       - | 3123 | `		}` |
|       - | 3124 | `	}` |
|       - | 3125 | `	/* Return the freshly created array */` |
|       5 | 3126 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3127 | `	/* Don't worry about freeing memory here,everything will be` |
|       - | 3128 | `	 * released automatically as soon we return from this function.` |
|       - | 3129 | `	 */` |
|       5 | 3130 | `	return PH7_OK;` |
|      27 | 3131 | `}` |
|       - | 3132 | `/*` |
|       - | 3133 | ` * php's socket ops report a failed send THEMSELVES, as an E_NOTICE naming the` |
|       - | 3134 | ` * count, the errno and its text, before the caller ever sees the false — so a` |
|       - | 3135 | ` * write to a peer that has gone is diagnosed rather than silent. Defined with` |
|       - | 3136 | ` * the socket device further down; the write paths that can reach a socket call` |
|       - | 3137 | ` * it where php's own do.` |
|       - | 3138 | ` */` |
|       - | 3139 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen);` |
|       - | 3140 | `/*` |
|       - | 3141 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|       - | 3142 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|       - | 3143 | ` * Parameters` |
|       - | 3144 | ` *  $handle` |
|       - | 3145 | ` *   The file pointer.` |
|       - | 3146 | ` *  $string` |
|       - | 3147 | ` *   The string that is to be written.` |
|       - | 3148 | ` *  $length` |
|       - | 3149 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|       - | 3150 | ` *   or the end of string is reached, whichever comes first.` |
|       - | 3151 | ` * Return` |
|       - | 3152 | ` *  Returns the number of bytes written, or FALSE on error.` |
|       - | 3153 | ` */` |
|     571 | 3154 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3155 | `{` |
|       - | 3156 | `	const char *zString;` |
|       - | 3157 | `	io_private *pDev;` |
|       - | 3158 | `	int nLen,n;` |
|       - | 3159 | `	int rc;` |
|     576 | 3160 | `	if( nArg < 2 ){` |
|       - | 3161 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3162 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 3163 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3164 | `		return PH7_OK;` |
|       - | 3165 | `	}` |
|       - | 3166 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     576 | 3167 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     576 | 3168 | `	if( pDev == 0 ){` |
|       7 | 3169 | `		return rc;` |
|       - | 3170 | `	}` |
|       - | 3171 | `	/* Point to the target IO stream device */` |
|     570 | 3172 | `	if( StreamRefuseUnwritable(pCtx,pDev) ){` |
|       5 | 3173 | `		ph7_result_bool(pCtx,0);` |
|       5 | 3174 | `		return PH7_OK;` |
|       - | 3175 | `	}` |
|       - | 3176 | `	/* Extract the data to write */` |
|     566 | 3177 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     566 | 3178 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       - | 3179 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|       - | 3180 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|       - | 3181 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|       - | 3182 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|       - | 3183 | `		 * and write the whole string). */` |
|      20 | 3184 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|      20 | 3185 | `		if( nMax < 0 ){` |
|       5 | 3186 | `			nLen = 0;` |
|      18 | 3187 | `		}else if( nMax < (sxi64)nLen ){` |
|      10 | 3188 | `			nLen = (int)nMax;` |
|       4 | 3189 | `		}` |
|       9 | 3190 | `	}` |
|     566 | 3191 | `	if( nLen < 1 ){` |
|       - | 3192 | `		/* Nothing to write */` |
|      12 | 3193 | `		ph7_result_int(pCtx,0);` |
|      12 | 3194 | `		return PH7_OK;` |
|       - | 3195 | `	}` |
|       - | 3196 | `	/* The device sits PAST what the readers pulled ahead: php writes at the` |
|       - | 3197 | `	 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|       - | 3198 | `	 * unread) — the ftell()/SEEK_CUR rule, applied to the write. */` |
|     556 | 3199 | `	StreamSeekBackForWrite(pDev);` |
|       - | 3200 | `	/* Perform the requested operation */` |
|     556 | 3201 | `	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);` |
|     556 | 3202 | `	if( n <  0 ){` |
|       - | 3203 | `		/* IO error,return FALSE */` |
|      60 | 3204 | `		SockReportWriteFailure(pCtx,pDev,nLen);` |
|      60 | 3205 | `		ph7_result_bool(pCtx,0);` |
|      32 | 3206 | `	}else{` |
|       - | 3207 | `		/* #Bytes written */` |
|     500 | 3208 | `		ph7_result_int(pCtx,n);` |
|       - | 3209 | `	}` |
|     556 | 3210 | `	return PH7_OK;` |
|     267 | 3211 | `}` |
|       - | 3212 | `/*` |
|       - | 3213 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|       - | 3214 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|       - | 3215 | ` * script can tell contention (1) from a plain failure (0).` |
|       - | 3216 | ` */` |
|      38 | 3217 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|       1 | 3218 | `{` |
|       - | 3219 | `	ph7_value sVal;` |
|      39 | 3220 | `	if( nArg < 3 ){` |
|      25 | 3221 | `		return;` |
|       - | 3222 | `	}` |
|      15 | 3223 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|      15 | 3224 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|      15 | 3225 | `	PH7_MemObjRelease(&sVal);` |
|      20 | 3226 | `}` |
|       - | 3227 | `/*` |
|       - | 3228 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|       - | 3229 | ` *  Portable advisory file locking.` |
|       - | 3230 | ` * Parameters` |
|       - | 3231 | ` *  $handle` |
|       - | 3232 | ` *   The file pointer.` |
|       - | 3233 | ` *  $operation` |
|       - | 3234 | ` *   operation is one of the following:` |
|       - | 3235 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|       - | 3236 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|       - | 3237 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|       - | 3238 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|       - | 3239 | ` *  &$would_block` |
|       - | 3240 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|       - | 3241 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|       - | 3242 | ` * Return` |
|       - | 3243 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 3244 | ` */` |
|      48 | 3245 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3246 | `{` |
|       - | 3247 | `	const ph7_io_stream *pStream;` |
|       - | 3248 | `	io_private *pDev;` |
|       - | 3249 | `	int nLock;` |
|       - | 3250 | `	int rc;` |
|      50 | 3251 | `	if( nArg < 2 ){` |
|       - | 3252 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3253 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 3254 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3255 | `		return PH7_OK;` |
|       - | 3256 | `	}` |
|       - | 3257 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      50 | 3258 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      50 | 3259 | `	if( pDev == 0 ){` |
|       3 | 3260 | `		return rc;` |
|       - | 3261 | `	}` |
|       - | 3262 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|       - | 3263 | `	 * support is considered: the low two bits select the action (its bison` |
|       - | 3264 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|       - | 3265 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|      47 | 3266 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      47 | 3267 | `	if( (nLock & 3) == 0 ){` |
|       9 | 3268 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3269 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|       - | 3270 | `	}` |
|      39 | 3271 | `	pDev = PH7_StreamUnwrap(pDev);` |
|       - | 3272 | `	/* Point to the target IO stream device */` |
|      39 | 3273 | `	pStream = pDev->pStream;` |
|      39 | 3274 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|       - | 3275 | `		/* php returns FALSE silently when the stream does not support locking` |
|       - | 3276 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|       7 | 3277 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|       7 | 3278 | `		ph7_result_bool(pCtx,0);` |
|       7 | 3279 | `		return PH7_OK;` |
|       - | 3280 | `	}` |
|       - | 3281 | `	/*` |
|       - | 3282 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|       - | 3283 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|       - | 3284 | `	 */` |
|       - | 3285 | `	{` |
|      33 | 3286 | `		int iOp = nLock & 3;` |
|      33 | 3287 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|      33 | 3288 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      13 | 3289 | `			nLock = -1;` |
|      27 | 3290 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      11 | 3291 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|       6 | 3292 | `		}else{` |
|      11 | 3293 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|       - | 3294 | `		}` |
|       - | 3295 | `	}` |
|       - | 3296 | `	/* Lock operation */` |
|      33 | 3297 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|       - | 3298 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|       - | 3299 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|      33 | 3300 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|       - | 3301 | `	/* IO result */` |
|      33 | 3302 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      33 | 3303 | `	return PH7_OK;` |
|      26 | 3304 | `}` |
|       - | 3305 | `/*` |
|       - | 3306 | ` * int fpassthru(resource $handle)` |
|       - | 3307 | ` *  Output all remaining data on a file pointer.` |
|       - | 3308 | ` * Parameters` |
|       - | 3309 | ` *  $handle` |
|       - | 3310 | ` *   The file pointer.` |
|       - | 3311 | ` * Return` |
|       - | 3312 | ` *  Total number of characters read from handle and passed through` |
|       - | 3313 | ` *  to the output on success or FALSE on failure.` |
|       - | 3314 | ` */` |
|      16 | 3315 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3316 | `{` |
|       - | 3317 | `	io_private *pDev;` |
|       - | 3318 | `	ph7_int64 n,nRead;` |
|       - | 3319 | `	char zBuf[8192];` |
|       - | 3320 | `	int rc;` |
|      19 | 3321 | `	if( nArg < 1 ){` |
|       - | 3322 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3323 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 3324 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3325 | `		return PH7_OK;` |
|       - | 3326 | `	}` |
|       - | 3327 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      19 | 3328 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      19 | 3329 | `	if( pDev == 0 ){` |
|       5 | 3330 | `		return rc;` |
|       - | 3331 | `	}` |
|      14 | 3332 | `	if( !StreamHasReader(pDev) ){` |
|       - | 3333 | `		/* php's fpassthru() reports the FAILING READ's own -1 when nothing was` |
|       - | 3334 | `		 * passed through, which is what a handle with no reader gives. */` |
|       3 | 3335 | `		ph7_result_int(pCtx,-1);` |
|       3 | 3336 | `		return PH7_OK;` |
|       - | 3337 | `	}` |
|       - | 3338 | `	/* Perform the requested operation */` |
|      12 | 3339 | `	nRead = 0;` |
|      10 | 3340 | `	for(;;){` |
|      22 | 3341 | `		n = PH7_StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      22 | 3342 | `		if( n < 1 ){` |
|       - | 3343 | `			/* Error or EOF */` |
|      12 | 3344 | `			StreamReportReadFailure(pCtx,pDev);` |
|      12 | 3345 | `			if( n < 0 && nRead == 0 ){` |
|       - | 3346 | `				/* php answers the failing read's own -1 when NOTHING was passed` |
|       - | 3347 | `				 * through; a failure after some bytes reports those bytes. */` |
|       3 | 3348 | `				ph7_result_int64(pCtx,-1);` |
|       3 | 3349 | `				return PH7_OK;` |
|       - | 3350 | `			}` |
|      10 | 3351 | `			break;` |
|       - | 3352 | `		}` |
|       - | 3353 | `		/* Increment the read counter */` |
|      12 | 3354 | `		nRead += n;` |
|       - | 3355 | `		/* Output the bytes THIS read produced. Handing the running total to` |
|       - | 3356 | `		 * ph7_context_output() instead read past the end of zBuf from the second` |
|       - | 3357 | `		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:` |
|       - | 3358 | `		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */` |
|      12 | 3359 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      12 | 3360 | `		if( rc == PH7_ABORT ){` |
|       - | 3361 | `			/* Consumer callback request an operation abort */` |
|     ! 0 | 3362 | `			break;` |
|       - | 3363 | `		}` |
|       2 | 3364 | `	}` |
|       - | 3365 | `	/* Total number of bytes readen */` |
|      10 | 3366 | `	ph7_result_int64(pCtx,nRead);` |
|      10 | 3367 | `	return PH7_OK;` |
|      11 | 3368 | `}` |
|       - | 3369 | `/* CSV writer private data */` |
|       - | 3370 | `struct csv_data` |
|       - | 3371 | `{` |
|       - | 3372 | `	int delimiter;     /* Delimiter. Default ',' */` |
|       - | 3373 | `	int enclosure;     /* Enclosure. Default '"' */` |
|       - | 3374 | `	int escape;        /* Escape, or PH7_CSV_NO_ESCAPE when "" disabled it */` |
|       - | 3375 | `	SyBlob *pLine;     /* The line being built */` |
|       - | 3376 | `	sxu32 nCount;      /* Fields still to write after this one */` |
|       - | 3377 | `	ph7_context *pCtx; /* Call context (the field cast's diagnostics) */` |
|       - | 3378 | `	ph7_class *pOwed;  /* First field class that could not be coerced */` |
|       - | 3379 | `};` |
|       - | 3380 | `/*` |
|       - | 3381 | ` * The following callback is used by fputcsv() to walk the $fields array and` |
|       - | 3382 | ` * append each entry to the line under construction. It is a port of php's own` |
|       - | 3383 | ` * php_fputcsv (ext/standard/file.c), and the parts a re-derivation gets wrong` |
|       - | 3384 | ` * are all here:` |
|       - | 3385 | ` *  - WHICH fields are enclosed. php quotes a field containing the delimiter,` |
|       - | 3386 | ` *    the enclosure, the escape (when one is enabled) or any of \n, \r, \t and` |
|       - | 3387 | ` *    SPACE. PH7 tested the first two only, so a field with an embedded newline` |
|       - | 3388 | ` *    was written raw and became two CSV ROWS on the way back in.` |
|       - | 3389 | ` *  - HOW an embedded enclosure is written: doubled, unless the escape character` |
|       - | 3390 | ` *    came immediately before it (then the pair is passed through as-is and the` |
|       - | 3391 | ` *    escape does NOT arm again for the byte after).` |
|       - | 3392 | ` *  - that an EMPTY field is still a field. PH7 returned early for a zero-length` |
|       - | 3393 | `` *    value and skipped its delimiter with it, so `['', 'a']` wrote "a" -- one`` |
|       - | 3394 | ` *    column where the caller wrote two, silently shifting every later column.` |
|       - | 3395 | ` * The delimiter goes BETWEEN fields, so it is written from the remaining count` |
|       - | 3396 | ` * rather than from a "not the first" flag: php appends it after every field but` |
|       - | 3397 | ` * the last, and an empty first field must still be followed by one.` |
|       - | 3398 | ` */` |
|     140 | 3399 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       1 | 3400 | `{` |
|     141 | 3401 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|       - | 3402 | `	const char *zData;` |
|       - | 3403 | `	int nLen,i;` |
|     141 | 3404 | `	int bEnclose = 0;` |
|      70 | 3405 | `	SXUNUSED(pKey); /* cc warning */` |
|       - | 3406 | `	/* php casts each field USER-VISIBLY: a field that is itself an array warns` |
|       - | 3407 | ``	 * `Array to string conversion` and is written as "Array", and one that is an`` |
|       - | 3408 | `	 * object with no __toString() is php's catchable Error -- where PHL wrote the` |
|       - | 3409 | `	 * placeholder "Object" into the file, in silence. */` |
|       - | 3410 | `	{` |
|     141 | 3411 | `		ph7_class *pBad = PH7_ValueToStringUVDefer(pData->pCtx,pValue,&zData,&nLen);` |
|     141 | 3412 | `		if( pBad && pData->pOwed == 0 ){` |
|       3 | 3413 | `			pData->pOwed = pBad;` |
|       1 | 3414 | `		}` |
|       - | 3415 | `	}` |
|     275 | 3416 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     183 | 3417 | `		int c = (unsigned char)zData[i];` |
|     182 | 3418 | `		if( c == pData->delimiter \|\| c == pData->enclosure` |
|     163 | 3419 | `		 \|\| (pData->escape != PH7_CSV_NO_ESCAPE && c == pData->escape)` |
|     153 | 3420 | `		 \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ' ' ){` |
|      49 | 3421 | `			bEnclose = 1;` |
|      49 | 3422 | `			break;` |
|       - | 3423 | `		}` |
|      68 | 3424 | `	}` |
|     141 | 3425 | `	if( bEnclose ){` |
|      49 | 3426 | `		char cEnc = (char)pData->enclosure;` |
|      49 | 3427 | `		int bEscaped = 0;` |
|      49 | 3428 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     197 | 3429 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     149 | 3430 | `			char c = zData[i];` |
|     149 | 3431 | `			if( pData->escape != PH7_CSV_NO_ESCAPE && (unsigned char)c == pData->escape ){` |
|       9 | 3432 | `				bEscaped = 1;` |
|     145 | 3433 | `			}else if( !bEscaped && (unsigned char)c == pData->enclosure ){` |
|      21 | 3434 | `				SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|      11 | 3435 | `			}else{` |
|     121 | 3436 | `				bEscaped = 0;` |
|       - | 3437 | `			}` |
|     149 | 3438 | `			SyBlobAppend(pData->pLine,(const void *)&c,sizeof(char));` |
|      75 | 3439 | `		}` |
|      49 | 3440 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     117 | 3441 | `	}else if( nLen > 0 ){` |
|      71 | 3442 | `		SyBlobAppend(pData->pLine,(const void *)zData,(sxu32)nLen);` |
|      35 | 3443 | `	}` |
|     141 | 3444 | `	if( pData->nCount > 0 ){` |
|     141 | 3445 | `		pData->nCount--;` |
|      70 | 3446 | `	}` |
|     141 | 3447 | `	if( pData->nCount > 0 ){` |
|      55 | 3448 | `		char cDel = (char)pData->delimiter;` |
|      55 | 3449 | `		SyBlobAppend(pData->pLine,(const void *)&cDel,sizeof(char));` |
|      27 | 3450 | `	}` |
|     141 | 3451 | `	return PH7_OK;` |
|       1 | 3452 | `}` |
|       - | 3453 | `/*` |
|       - | 3454 | ` * int\|false fputcsv(resource $stream, array $fields, string $separator = ',',` |
|       - | 3455 | ` *                   string $enclosure = '"', string $escape = '\\',` |
|       - | 3456 | ` *                   string $eol = "\n")` |
|       - | 3457 | ` *  Format line as CSV and write to file pointer.` |
|       - | 3458 | ` * Parameters` |
|       - | 3459 | ` *  $stream` |
|       - | 3460 | ` *   Open file handle.` |
|       - | 3461 | ` *  $fields` |
|       - | 3462 | ` *   An array of values.` |
|       - | 3463 | ` *  $separator` |
|       - | 3464 | ` *   The optional separator parameter sets the field delimiter (one character only).` |
|       - | 3465 | ` *  $enclosure` |
|       - | 3466 | ` *   The optional enclosure parameter sets the field enclosure (one character only).` |
|       - | 3467 | ` *  $escape` |
|       - | 3468 | ` *   The escape character (one character), or "" to disable escaping entirely.` |
|       - | 3469 | ` *  $eol` |
|       - | 3470 | ` *   php 8.1's line ending. It is "\n" on EVERY platform -- php does not follow` |
|       - | 3471 | ` *   the host's convention here, and PHL used to write CRLF on Windows, so the` |
|       - | 3472 | ` *   same program produced a different FILE depending on where it ran.` |
|       - | 3473 | ` * Return` |
|       - | 3474 | ` *  The number of bytes written, or FALSE when the write fails. The count was` |
|       - | 3475 | ` *  missing entirely (the call answered NULL), so the documented` |
|       - | 3476 | `` *  `if (fputcsv(...) === false)` check never fired and a caller totalling the`` |
|       - | 3477 | ` *  bytes it wrote added nothing.` |
|       - | 3478 | ` */` |
|     102 | 3479 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3480 | `{` |
|       - | 3481 | `	struct csv_data sCsv;` |
|       - | 3482 | `	io_private *pDev;` |
|       - | 3483 | `	SyBlob sLine;` |
|     104 | 3484 | `	const char *zEol = "\n";` |
|     104 | 3485 | `	int nEol = 1;` |
|       - | 3486 | `	ph7_int64 nWr;` |
|       - | 3487 | ``	int rcArg;   /* the handle screen's; the CSV screens below shadow `rc` */`` |
|     104 | 3488 | `	if( nArg < 2 ){` |
|       - | 3489 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3490 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|     ! 0 | 3491 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3492 | `		return PH7_OK;` |
|       - | 3493 | `	}` |
|       - | 3494 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     104 | 3495 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcArg);` |
|     104 | 3496 | `	if( pDev == 0 ){` |
|       3 | 3497 | `		return rcArg;` |
|       - | 3498 | `	}` |
|     101 | 3499 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3500 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 3501 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|     ! 0 | 3502 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3503 | `		return PH7_OK;` |
|       - | 3504 | `	}` |
|       - | 3505 | `	/* Point to the target IO stream device */` |
|     101 | 3506 | `	if( StreamRefuseUnwritable(pCtx,pDev) ){` |
|     ! 0 | 3507 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3508 | `		return PH7_OK;` |
|       - | 3509 | `	}` |
|       - | 3510 | `	/* Set default csv separator */` |
|     101 | 3511 | `	sCsv.delimiter = ',';` |
|     101 | 3512 | `	sCsv.enclosure = '"';` |
|     101 | 3513 | `	sCsv.escape = '\\';` |
|     101 | 3514 | `	if( nArg > 2 ){` |
|     101 | 3515 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     101 | 3516 | `		if( rc != PH7_OK ){` |
|       5 | 3517 | `			return rc;` |
|       - | 3518 | `		}` |
|      97 | 3519 | `		if( nArg > 3 ){` |
|      97 | 3520 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|      97 | 3521 | `			if( rc != PH7_OK ){` |
|       5 | 3522 | `				return rc;` |
|       - | 3523 | `			}` |
|      93 | 3524 | `			if( nArg > 4 ){` |
|      93 | 3525 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&sCsv.escape);` |
|      93 | 3526 | `				if( rc != PH7_OK ){` |
|       5 | 3527 | `					return rc;` |
|       - | 3528 | `				}` |
|      89 | 3529 | `				if( nArg > 5 ){` |
|       - | 3530 | `					/* $eol takes ANY string, the empty one included -- it is not` |
|       - | 3531 | `					 * a single-character argument like the three above. */` |
|      75 | 3532 | `					zEol = ph7_value_to_string(apArg[5],&nEol);` |
|      37 | 3533 | `				}` |
|      44 | 3534 | `			}` |
|      44 | 3535 | `		}` |
|      44 | 3536 | `	}` |
|       - | 3537 | `	/* php builds the whole line first and writes it ONCE, which is what makes the` |
|       - | 3538 | `	 * byte count meaningful and keeps a partly-written row off the stream. */` |
|      89 | 3539 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|      89 | 3540 | `	sCsv.pLine = &sLine;` |
|      89 | 3541 | `	sCsv.nCount = (sxu32)ph7_array_count(apArg[1]);` |
|      89 | 3542 | `	sCsv.pCtx = pCtx;` |
|      89 | 3543 | `	sCsv.pOwed = 0;` |
|      89 | 3544 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      89 | 3545 | `	if( nEol > 0 ){` |
|      87 | 3546 | `		SyBlobAppend(&sLine,(const void *)zEol,(sxu32)nEol);` |
|      43 | 3547 | `	}` |
|       - | 3548 | `	/* Write at the LOGICAL position, not the device one -- the same rule` |
|       - | 3549 | `	 * PH7_builtin_fwrite applies after a buffered read (fgets() then` |
|       - | 3550 | `	 * fputcsv() overwrites what fgets left unread). */` |
|      89 | 3551 | `	StreamSeekBackForWrite(pDev);` |
|     133 | 3552 | `	nWr = PH7_StreamWrite(pDev,(const void *)SyBlobData(&sLine),` |
|      88 | 3553 | `		(ph7_int64)SyBlobLength(&sLine));` |
|      89 | 3554 | `	if( nWr < 0 ){` |
|       3 | 3555 | `		SockReportWriteFailure(pCtx,pDev,(int)SyBlobLength(&sLine));` |
|       1 | 3556 | `	}` |
|      89 | 3557 | `	SyBlobRelease(&sLine);` |
|      89 | 3558 | `	if( nWr < 0 ){` |
|       3 | 3559 | `		ph7_result_bool(pCtx,0);` |
|       2 | 3560 | `	}else{` |
|      87 | 3561 | `		ph7_result_int64(pCtx,nWr);` |
|       - | 3562 | `	}` |
|      89 | 3563 | `	if( sCsv.pOwed ){` |
|       - | 3564 | `		/* php writes the line first and reports the field it could not stringify` |
|       - | 3565 | `		 * afterwards; raising it during the build would put the catch's own` |
|       - | 3566 | `		 * output in front of the row. */` |
|       4 | 3567 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - | 3568 | `			"Object of class %.*s could not be converted to string",` |
|       2 | 3569 | `			(int)sCsv.pOwed->sName.nByte,sCsv.pOwed->sName.zString);` |
|       - | 3570 | `	}` |
|      87 | 3571 | `	return PH7_OK;` |
|      53 | 3572 | `}` |
|       - | 3573 | `/*` |
|       - | 3574 | ` * fprintf,vfprintf private data.` |
|       - | 3575 | ` * An instance of the following structure is passed to the formatted` |
|       - | 3576 | ` * input consumer callback defined below.` |
|       - | 3577 | ` */` |
|       - | 3578 | `typedef struct fprintf_data fprintf_data;` |
|       - | 3579 | `struct fprintf_data` |
|       - | 3580 | `{` |
|       - | 3581 | `	io_private *pIO;        /* IO stream */` |
|       - | 3582 | `	ph7_int64 nCount;       /* Total bytes FORMATTED (php's answer, not the bytes` |
|       - | 3583 | `	                         * the device took: php builds the whole string, writes` |
|       - | 3584 | `	                         * it once and returns its length whatever the write` |
|       - | 3585 | `	                         * did) */` |
|       - | 3586 | `	int bNoWriter;          /* the stream has no writer AT ALL: announced once,` |
|       - | 3587 | `	                         * before the format runs, and then every chunk is` |
|       - | 3588 | `	                         * counted and dropped rather than offered */` |
|       - | 3589 | `	int bIoErr;             /* the device refused, so stop feeding it -- but this` |
|       - | 3590 | `	                         * is an IO failure and not a mid-format THROW, and the` |
|       - | 3591 | `	                         * caller must not confuse the two */` |
|       - | 3592 | `};` |
|       - | 3593 | `/*` |
|       - | 3594 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|       - | 3595 | ` */` |
|      58 | 3596 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       2 | 3597 | `{` |
|      60 | 3598 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|       - | 3599 | `	ph7_int64 n;` |
|      60 | 3600 | `	if( pFdata->bNoWriter ){` |
|       - | 3601 | `		/* Nowhere to put it, and php builds the string anyway: its fprintf()` |
|       - | 3602 | `		 * formats first and writes once, so the LENGTH it answers is the same` |
|       - | 3603 | `		 * whether the stream took the bytes or refused them all. Counting` |
|       - | 3604 | ``		 * without writing is what keeps `fprintf($dir, "a%sb", "q")` at 3`` |
|       - | 3605 | `		 * rather than the first chunk's 1. */` |
|      19 | 3606 | `		pFdata->nCount += nLen;` |
|      19 | 3607 | `		return PH7_OK;` |
|       - | 3608 | `	}` |
|       - | 3609 | `	/* Write the formatted data */` |
|      42 | 3610 | `	n = PH7_StreamWrite(pFdata->pIO,(const void *)zInput,nLen);` |
|      42 | 3611 | `	pFdata->nCount += nLen;` |
|      42 | 3612 | `	if( n < 0 ){` |
|       3 | 3613 | `		SockReportWriteFailure(pCtx,pFdata->pIO,nLen);` |
|       - | 3614 | `		/* Nothing more can reach the device; stop, and let the caller answer.` |
|       - | 3615 | `		 * Propagating this as a THROW status aborted the whole script -- a` |
|       - | 3616 | `		 * failed fprintf() ended the program where php returns a number. */` |
|       3 | 3617 | `		pFdata->bIoErr = 1;` |
|       3 | 3618 | `		return SXERR_ABORT;` |
|       - | 3619 | `	}` |
|      40 | 3620 | `	return PH7_OK;` |
|      31 | 3621 | `}` |
|       - | 3622 | `/*` |
|       - | 3623 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|       - | 3624 | ` *  Write a formatted string to a stream.` |
|       - | 3625 | ` * Parameters` |
|       - | 3626 | ` *  $handle` |
|       - | 3627 | ` *   The file pointer.` |
|       - | 3628 | ` *  $format` |
|       - | 3629 | ` *   String format (see sprintf()).` |
|       - | 3630 | ` * Return` |
|       - | 3631 | ` *  The length of the written string.` |
|       - | 3632 | ` */` |
|      34 | 3633 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3634 | `{` |
|       - | 3635 | `	fprintf_data sFdata;` |
|       - | 3636 | `	const char *zFormat;` |
|       - | 3637 | `	io_private *pDev;` |
|       - | 3638 | `	int nLen;` |
|      37 | 3639 | `	if( nArg < 2 ){` |
|     ! 0 | 3640 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|     ! 0 | 3641 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 3642 | `		return PH7_OK;` |
|       - | 3643 | `	}` |
|       - | 3644 | `	{` |
|       - | 3645 | `		/* php: the $stream argument is refused BEFORE $format and $values -- a` |
|       - | 3646 | `		 * non-resource names the type it got, and one whose device is gone is` |
|       - | 3647 | `		 * "must be an open stream resource". Both are TypeErrors, not the` |
|       - | 3648 | `		 * warn-and-return-0 this door used to give. */` |
|       - | 3649 | `		int rcs;` |
|      37 | 3650 | `		pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcs);` |
|      37 | 3651 | `		if( pDev == 0 ){` |
|       5 | 3652 | `			return rcs;` |
|       - | 3653 | `		}` |
|       - | 3654 | `	}` |
|       - | 3655 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|       - | 3656 | `	{` |
|      32 | 3657 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      32 | 3658 | `		if( rcf != PH7_OK ){` |
|     ! 0 | 3659 | `			return rcf;` |
|       - | 3660 | `		}` |
|       - | 3661 | `	}` |
|       - | 3662 | `	/* Extract the string format (scalars/null coerce). */` |
|      32 | 3663 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      32 | 3664 | `	if( nLen < 1 ){` |
|       - | 3665 | `		/* Empty string,return zero */` |
|       3 | 3666 | `		ph7_result_int(pCtx,0);` |
|       3 | 3667 | `		return PH7_OK;` |
|       - | 3668 | `	}` |
|       - | 3669 | `	{` |
|       - | 3670 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|       - | 3671 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|       - | 3672 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|      30 | 3673 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|      30 | 3674 | `		if( rcv != PH7_OK ){` |
|       3 | 3675 | `			return rcv;` |
|       - | 3676 | `		}` |
|       - | 3677 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|       - | 3678 | `		 * before any output; propagate the throw status verbatim. */` |
|      28 | 3679 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      28 | 3680 | `		if( rcv != PH7_OK ){` |
|       7 | 3681 | `			return rcv;` |
|       - | 3682 | `		}` |
|       - | 3683 | `	}` |
|       - | 3684 | `	/* Prepare our private data */` |
|      22 | 3685 | `	sFdata.nCount = 0;` |
|      22 | 3686 | `	sFdata.pIO = pDev;` |
|      22 | 3687 | `	sFdata.bIoErr = 0;` |
|       - | 3688 | `	/* A handle with no writer at all is announced HERE -- after every argument` |
|       - | 3689 | `	 * screen, because php's TypeError/ValueError/ArgumentCountError come first` |
|       - | 3690 | `	 * and carry no notice with them, and after the empty format, which writes` |
|       - | 3691 | `	 * nothing and says nothing. */` |
|      22 | 3692 | `	sFdata.bNoWriter = StreamRefuseUnwritable(pCtx,pDev);` |
|       - | 3693 | `	/* Format the string */` |
|       - | 3694 | `	{` |
|      22 | 3695 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|       - | 3696 | `	/* Return total number of bytes written */` |
|      22 | 3697 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|       - | 3698 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|       - | 3699 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|       - | 3700 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|      22 | 3701 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|       3 | 3702 | `		pCtx->nThrowRc = rcv;` |
|       3 | 3703 | `		return rcv;` |
|       - | 3704 | `	}` |
|       - | 3705 | `	}` |
|      19 | 3706 | `	return PH7_OK;` |
|      20 | 3707 | `}` |
|       - | 3708 | `/*` |
|       - | 3709 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|       - | 3710 | ` *  Write a formatted string to a stream.` |
|       - | 3711 | ` * Parameters` |
|       - | 3712 | ` *  $handle` |
|       - | 3713 | ` *   The file pointer.` |
|       - | 3714 | ` *  $format` |
|       - | 3715 | ` *   String format (see sprintf()).` |
|       - | 3716 | ` * $args` |
|       - | 3717 | ` *   User arguments.` |
|       - | 3718 | ` * Return` |
|       - | 3719 | ` *  The length of the written string.` |
|       - | 3720 | ` */` |
|      18 | 3721 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3722 | `{` |
|       - | 3723 | `	fprintf_data sFdata;` |
|       - | 3724 | `	const char *zFormat;` |
|       - | 3725 | `	ph7_hashmap *pMap;` |
|       - | 3726 | `	io_private *pDev;` |
|       - | 3727 | `	SySet sArg;` |
|       - | 3728 | `	int n,nLen;` |
|      21 | 3729 | `	if( nArg < 3 ){` |
|     ! 0 | 3730 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|     ! 0 | 3731 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 3732 | `		return PH7_OK;` |
|       - | 3733 | `	}` |
|       - | 3734 | `	{` |
|       - | 3735 | `		/* php: the $stream argument is refused BEFORE $format and $values -- a` |
|       - | 3736 | `		 * non-resource names the type it got, and one whose device is gone is` |
|       - | 3737 | `		 * "must be an open stream resource". Both are TypeErrors, not the` |
|       - | 3738 | `		 * warn-and-return-0 this door used to give. */` |
|       - | 3739 | `		int rcs;` |
|      21 | 3740 | `		pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcs);` |
|      21 | 3741 | `		if( pDev == 0 ){` |
|      10 | 3742 | `			return rcs;` |
|       - | 3743 | `		}` |
|       - | 3744 | `	}` |
|       - | 3745 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|       - | 3746 | `	{` |
|      13 | 3747 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      13 | 3748 | `		if( rcf != PH7_OK ){` |
|     ! 0 | 3749 | `			return rcf;` |
|       - | 3750 | `		}` |
|       - | 3751 | `	}` |
|      13 | 3752 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|       - | 3753 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - | 3754 | `		char zBuf[64];` |
|       4 | 3755 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3756 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|       2 | 3757 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|       - | 3758 | `	}` |
|       - | 3759 | `	/* Extract the string format */` |
|      10 | 3760 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      10 | 3761 | `	if( nLen < 1 ){` |
|       - | 3762 | `		/* Empty string,return zero */` |
|     ! 0 | 3763 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 3764 | `		return PH7_OK;` |
|       - | 3765 | `	}` |
|       - | 3766 | `	/* Point to hashmap */` |
|      10 | 3767 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|       - | 3768 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - | 3769 | `	 * php runs this BEFORE validating the specifiers. */` |
|       - | 3770 | `	{` |
|      10 | 3771 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      10 | 3772 | `		if( rcc != PH7_OK ){` |
|       3 | 3773 | `			return rcc;` |
|       - | 3774 | `		}` |
|       - | 3775 | `	}` |
|       - | 3776 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - | 3777 | `	 * any output; propagate the throw status verbatim. */` |
|       - | 3778 | `	{` |
|       8 | 3779 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|       8 | 3780 | `		if( rcv != PH7_OK ){` |
|     ! 0 | 3781 | `			return rcv;` |
|       - | 3782 | `		}` |
|       - | 3783 | `	}` |
|       - | 3784 | `	/* Extract arguments from the hashmap */` |
|       8 | 3785 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - | 3786 | `	/* Prepare our private data */` |
|       8 | 3787 | `	sFdata.nCount = 0;` |
|       8 | 3788 | `	sFdata.pIO = pDev;` |
|       8 | 3789 | `	sFdata.bIoErr = 0;` |
|       - | 3790 | `	/* A handle with no writer at all is announced HERE -- after every argument` |
|       - | 3791 | `	 * screen, because php's TypeError/ValueError/ArgumentCountError come first` |
|       - | 3792 | `	 * and carry no notice with them, and after the empty format, which writes` |
|       - | 3793 | `	 * nothing and says nothing. */` |
|       8 | 3794 | `	sFdata.bNoWriter = StreamRefuseUnwritable(pCtx,pDev);` |
|       - | 3795 | `	/* Format the string */` |
|       - | 3796 | `	{` |
|       8 | 3797 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|       - | 3798 | `	/* Return total number of bytes written*/` |
|       8 | 3799 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|       8 | 3800 | `	SySetRelease(&sArg);` |
|       - | 3801 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|       - | 3802 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|       - | 3803 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|       8 | 3804 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|       3 | 3805 | `		pCtx->nThrowRc = rcv;` |
|       3 | 3806 | `		return rcv;` |
|       - | 3807 | `	}` |
|       - | 3808 | `	}` |
|       5 | 3809 | `	return PH7_OK;` |
|      12 | 3810 | `}` |
|       - | 3811 | `/*` |
|       - | 3812 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|       - | 3813 | ` * According to the PHP reference manual:` |
|       - | 3814 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|       - | 3815 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|       - | 3816 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|       - | 3817 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|       - | 3818 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|       - | 3819 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|       - | 3820 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|       - | 3821 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|       - | 3822 | ` *         exist, attempt to create it.` |
|       - | 3823 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|       - | 3824 | ` *          not exist, attempt to create it.` |
|       - | 3825 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|       - | 3826 | ` *         already exists,` |
|       - | 3827 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|       - | 3828 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|       - | 3829 | ` *         the underlying open(2) system call.` |
|       - | 3830 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|       - | 3831 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|       - | 3832 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|       - | 3833 | ` *          is positioned on the beginning of the file.` |
|       - | 3834 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|       - | 3835 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|       - | 3836 | ` *          be used after the lock is requested).` |
|       - | 3837 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|       - | 3838 | ` */` |
|       - | 3839 | `/*` |
|       - | 3840 | ` * php's php_stream_parse_fopen_modes, which is narrower than the list above` |
|       - | 3841 | `` * reads: only the FIRST character decides, it must be one of `r w a x c` in`` |
|       - | 3842 | `` * LOWER case, and everything after it is SCANNED -- `+` anywhere makes the`` |
|       - | 3843 | `` * open read-write, `b`/`t` anywhere pick the translation mode, and any other`` |
|       - | 3844 | ` * byte is ignored. Anything else is refused OUTRIGHT, which is what the -1` |
|       - | 3845 | ` * answer is for; the empty mode is one of them.` |
|       - | 3846 | ` *` |
|       - | 3847 | ` * The chunk read only the first TWO characters and had its own idea of both` |
|       - | 3848 | `` * halves, so six ordinary spellings opened the wrong way in silence: `rb+`,`` |
|       - | 3849 | `` * `ab+` and `cb+` -- the `+` is not in position two -- were opened read-only`` |
|       - | 3850 | ``  * or write-only, `rw` was READ-WRITE where php gives read-only, `wr` `` |
|       - | 3851 | ` * likewise, and an unknown or upper-case mode was accepted with a PH7-ism` |
|       - | 3852 | ` * notice and a read-only open where php refuses the call.` |
|       - | 3853 | ` */` |
|    1866 | 3854 | `static int StrModeToFlags(const char *zMode,int nLen,int *piFlags)` |
|       5 | 3855 | `{` |
|       - | 3856 | `	int iFlag,i;` |
|    1871 | 3857 | `	int bPlus = 0,bBin = 0,bText = 0;` |
|    1871 | 3858 | `	if( nLen < 1 ){` |
|       3 | 3859 | `		return -1;` |
|       - | 3860 | `	}` |
|    1869 | 3861 | `	switch( zMode[0] ){` |
|     414 | 3862 | `		case 'r':` |
|       - | 3863 | `			/* Read-only access */` |
|     828 | 3864 | `			iFlag = PH7_IO_OPEN_RDONLY;` |
|     828 | 3865 | `			break;` |
|     177 | 3866 | `		case 'w':` |
|       - | 3867 | `			/* Overwrite mode; create the file if it is not there */` |
|     357 | 3868 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     357 | 3869 | `			break;` |
|      18 | 3870 | `		case 'a':` |
|       - | 3871 | `			/* Append mode; create the file if it is not there */` |
|      38 | 3872 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|      38 | 3873 | `			break;` |
|     297 | 3874 | `		case 'x':` |
|       - | 3875 | `			/* Exclusive create: fails when the file already exists. EXCL is left` |
|       - | 3876 | `			 * to imply the creation on its own -- the device decoders test` |
|       - | 3877 | `			 * CREATE first, so setting both would drop the O_EXCL. */` |
|     599 | 3878 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|     599 | 3879 | `			break;` |
|      10 | 3880 | `		case 'c':` |
|       - | 3881 | `			/* Create if absent, and neither truncate nor fail if present */` |
|      22 | 3882 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|      22 | 3883 | `			break;` |
|      19 | 3884 | `		default:` |
|      40 | 3885 | `			return -1;` |
|       - | 3886 | `	}` |
|    2427 | 3887 | `	for( i = 1 ; i < nLen ; ++i ){` |
|     600 | 3888 | `		if( zMode[i] == '+' ){` |
|     490 | 3889 | `			bPlus = 1;` |
|     355 | 3890 | `		}else if( zMode[i] == 'b' ){` |
|      92 | 3891 | `			bBin = 1;` |
|      67 | 3892 | `		}else if( zMode[i] == 't' ){` |
|      12 | 3893 | `			bText = 1;` |
|       5 | 3894 | `		}` |
|     302 | 3895 | `	}` |
|    1831 | 3896 | `	if( bPlus ){` |
|     488 | 3897 | `		iFlag &= ~(PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_WRONLY);` |
|     488 | 3898 | `		iFlag \|= PH7_IO_OPEN_RDWR;` |
|     242 | 3899 | `	}` |
|       - | 3900 | ``	/* php's `b` wins over `t` when both are named, and binary is its default. */`` |
|    1831 | 3901 | `	if( bText && !bBin ){` |
|       8 | 3902 | `		iFlag \|= PH7_IO_OPEN_TEXT;` |
|       5 | 3903 | `	}else{` |
|    1825 | 3904 | `		iFlag \|= PH7_IO_OPEN_BINARY;` |
|       - | 3905 | `	}` |
|    1831 | 3906 | `	*piFlags = iFlag;` |
|    1831 | 3907 | `	return 0;` |
|     935 | 3908 | `}` |
|       - | 3909 | `/*` |
|       - | 3910 | ` * The same grammar, for a door that has to know whether a mode is php's before` |
|       - | 3911 | ` * deciding whose refusal to raise: gzopen() reports the sentence above for a` |
|       - | 3912 | ` * letter php does not know, and libz's own flat failure for one php takes and` |
|       - | 3913 | ` * libz has no direction for. Answers 1 when php would accept the mode.` |
|       - | 3914 | ` */` |
|       4 | 3915 | `PH7_PRIVATE int PH7_StreamModeIsValid(const char *zMode,int nLen,int *piFlags)` |
|       1 | 3916 | `{` |
|       5 | 3917 | `	return StrModeToFlags(zMode,nLen,piFlags) == 0;` |
|       1 | 3918 | `}` |
|       - | 3919 | `/*` |
|       - | 3920 | ` * Initialize the IO private structure.` |
|       - | 3921 | ` */` |
|   11224 | 3922 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|       5 | 3923 | `{` |
|   11229 | 3924 | `	pOut->pStream = pStream;` |
|   11229 | 3925 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   11229 | 3926 | `	pOut->nOfft = 0;` |
|   11229 | 3927 | `	SyBlobInit(&pOut->sUri,&pVm->sAllocator);` |
|   11229 | 3928 | `	pOut->zMode[0] = 0;` |
|   11229 | 3929 | `	pOut->bEof = 0;` |
|   11229 | 3930 | `	pOut->iLastReadErr = 0;` |
|   11229 | 3931 | `	pOut->bDir = 0;` |
|   11229 | 3932 | `	pOut->bPersist = 0;` |
|   11229 | 3933 | `	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */` |
|   11229 | 3934 | `	pOut->bNonBlock = 0;` |
|   11229 | 3935 | `	pOut->bHasTimeout = 0;` |
|   11229 | 3936 | `	pOut->bTimedOut = 0;` |
|   11229 | 3937 | `	pOut->pReadFilters = 0;` |
|   11229 | 3938 | `	pOut->pWriteFilters = 0;` |
|   11229 | 3939 | `	pOut->bFiltDone = 0;` |
|   11229 | 3940 | `	pOut->bFiltErr = 0;` |
|   11229 | 3941 | `	pOut->iFiltPos = 0;` |
|   11229 | 3942 | `	pOut->pCtxRes = 0;` |
|   11229 | 3943 | `	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);` |
|   11229 | 3944 | `	pOut->nFiltOfft = 0;` |
|       - | 3945 | `	/* Set the magic number */` |
|   11229 | 3946 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   11229 | 3947 | `}` |
|       - | 3948 | `/*` |
|       - | 3949 | `` * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and`` |
|       - | 3950 | `` * `mode` keys. php keeps the URI exactly as written (a relative path stays`` |
|       - | 3951 | ` * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves` |
|       - | 3952 | ` * the key out, which is how a popen() pipe reports no wrapper and no uri.` |
|       - | 3953 | ` */` |
|   10891 | 3954 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)` |
|       5 | 3955 | `{` |
|   10896 | 3956 | `	if( pDev == 0 ){` |
|     ! 0 | 3957 | `		return;` |
|       - | 3958 | `	}` |
|   10896 | 3959 | `	SyBlobReset(&pDev->sUri);` |
|   10896 | 3960 | `	if( zUri && nUriLen > 0 ){` |
|    2077 | 3961 | `		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);` |
|    1033 | 3962 | `	}` |
|   10896 | 3963 | `	if( zMode && nModeLen > 0 ){` |
|   10896 | 3964 | `		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){` |
|     ! 0 | 3965 | `			nModeLen = (int)sizeof(pDev->zMode) - 1;` |
|     ! 0 | 3966 | `		}` |
|   10896 | 3967 | `		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);` |
|   10896 | 3968 | `		pDev->zMode[nModeLen] = 0;` |
|    5443 | 3969 | `	}else{` |
|     ! 0 | 3970 | `		pDev->zMode[0] = 0;` |
|       - | 3971 | `	}` |
|    5443 | 3972 | `}` |
|       - | 3973 | `/*` |
|       - | 3974 | ` * Release the IO private structure.` |
|       - | 3975 | ` */` |
|     295 | 3976 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|       5 | 3977 | `{` |
|     300 | 3978 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     300 | 3979 | `	SyBlobRelease(&pDev->sBuffer);` |
|     300 | 3980 | `	SyBlobRelease(&pDev->sFilt);` |
|     300 | 3981 | `	SyBlobRelease(&pDev->sUri);` |
|     300 | 3982 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|       - | 3983 | `	/* Release the whole structure */` |
|     300 | 3984 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     300 | 3985 | `}` |
|       - | 3986 | `/*` |
|       - | 3987 | ` * Release a handle shell whose open FAILED: it never reached PHP, so nothing can` |
|       - | 3988 | ` * hold a copy and the chunk goes back. For a caller outside this unit (the` |
|       - | 3989 | ` * XMLWriter URI writer builds its own handle the way fopen does).` |
|       - | 3990 | ` */` |
|      24 | 3991 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev)` |
|       3 | 3992 | `{` |
|      27 | 3993 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      27 | 3994 | `}` |
|       - | 3995 | `/*` |
|       - | 3996 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|       - | 3997 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|       - | 3998 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|       - | 3999 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|       - | 4000 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|       - | 4001 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|       - | 4002 | ` * pool-masked UAF) and keep reporting the handle open.` |
|       - | 4003 | ` */` |
|   10533 | 4004 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|       5 | 4005 | `{` |
|       - | 4006 | `	/* A filter outliving its handle would keep answering is_resource() and hold` |
|       - | 4007 | `	 * a pointer to a closed device; every close path releases the chains before` |
|       - | 4008 | `	 * the device goes, and this is the backstop for one that forgets. */` |
|   10538 | 4009 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   10538 | 4010 | `	SyBlobRelease(&pDev->sBuffer);` |
|   10538 | 4011 | `	SyBlobRelease(&pDev->sFilt);` |
|   10538 | 4012 | `	SyBlobRelease(&pDev->sUri);` |
|   10538 | 4013 | `	pDev->pHandle = 0;` |
|   10538 | 4014 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   10538 | 4015 | `}` |
|       - | 4016 | `/*` |
|       - | 4017 | ` * Reset the IO private structure.` |
|       - | 4018 | ` */` |
|     470 | 4019 | `static void ResetIOPrivate(io_private *pDev)` |
|       4 | 4020 | `{` |
|     474 | 4021 | `	SyBlobReset(&pDev->sBuffer);` |
|     474 | 4022 | `	pDev->nOfft = 0;` |
|       - | 4023 | `	/* A seek moves the DEVICE, so whatever the read chain had already produced` |
|       - | 4024 | `	 * from the old position is not what the new one answers. */` |
|     474 | 4025 | `	SyBlobReset(&pDev->sFilt);` |
|     474 | 4026 | `	pDev->nFiltOfft = 0;` |
|     474 | 4027 | `	pDev->bFiltDone = 0;` |
|     474 | 4028 | `	pDev->bFiltErr = 0;` |
|     474 | 4029 | `	PH7_StreamFilterRewound(pDev);` |
|       - | 4030 | `	/* Every caller of this has just MOVED the device (a seek, a rewind, a` |
|       - | 4031 | `	 * truncate), and php clears the end-of-file flag on exactly those. */` |
|     474 | 4032 | `	pDev->bEof = 0;` |
|     474 | 4033 | `}` |
|       - | 4034 | `/* Forward declaration */` |
|       - | 4035 |  |
|       - | 4036 | `/*` |
|       - | 4037 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|       - | 4038 | ` *  Open a file,a URL or any other IO stream.` |
|       - | 4039 | ` * Parameters` |
|       - | 4040 | ` *  $filename` |
|       - | 4041 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|       - | 4042 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|       - | 4043 | ` *   then a regular file is assumed.` |
|       - | 4044 | ` *  $mode` |
|       - | 4045 | ` *   The mode parameter specifies the type of access you require to the stream` |
|       - | 4046 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|       - | 4047 | ` *   modes.` |
|       - | 4048 | ` *  $use_include_path` |
|       - | 4049 | ` *   You can use the optional second parameter and set it to` |
|       - | 4050 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - | 4051 | ` *  $context` |
|       - | 4052 | ` *   A context stream resource.` |
|       - | 4053 | ` * Return` |
|       - | 4054 | ` *  File handle on success or FALSE on failure.` |
|       - | 4055 | ` */` |
|       - | 4056 | `/*` |
|       - | 4057 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|       - | 4058 | ` *                                  int $offset = -1)` |
|       - | 4059 | ` *  Read the remaining contents of a stream into a string.` |
|       - | 4060 | ` */` |
|     640 | 4061 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4062 | `{` |
|       - | 4063 | `	const ph7_io_stream *pStream;` |
|       - | 4064 | `	io_private *pDev;` |
|     644 | 4065 | `	ph7_int64 nMax = -1;` |
|       - | 4066 | `	char zBuf[4096];` |
|       - | 4067 | `	ph7_int64 nRead;` |
|       - | 4068 | `	int rc;` |
|     644 | 4069 | `	if( nArg < 1 ){` |
|     ! 0 | 4070 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 4071 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4072 | `		return PH7_OK;` |
|       - | 4073 | `	}` |
|       - | 4074 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     644 | 4075 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     644 | 4076 | `	if( pDev == 0 ){` |
|       3 | 4077 | `		return rc;` |
|       - | 4078 | `	}` |
|     642 | 4079 | `	pStream = pDev->pStream;` |
|     642 | 4080 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|     ! 0 | 4081 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4082 | `		return PH7_OK;` |
|       - | 4083 | `	}` |
|     642 | 4084 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4085 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|       - | 4086 | `		 * default) means "read until EOF". */` |
|      17 | 4087 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      17 | 4088 | `		if( nMax < -1 ){` |
|       3 | 4089 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4090 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|       - | 4091 | `		}` |
|       7 | 4092 | `	}` |
|     640 | 4093 | `	if( nArg > 2 ){` |
|       9 | 4094 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|       9 | 4095 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|       9 | 4096 | `			if( pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/) == PH7_OK ){` |
|       - | 4097 | `				/* A seek DISCARDS what was buffered ahead — the bytes belong to` |
|       - | 4098 | `				 * the position we just left. Without this the read below served` |
|       - | 4099 | `				 * the old position's leftovers and then continued from the new` |
|       - | 4100 | `				 * one. */` |
|       9 | 4101 | `				ResetIOPrivate(pDev);` |
|       4 | 4102 | `			}` |
|       4 | 4103 | `		}` |
|       4 | 4104 | `	}` |
|     640 | 4105 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|    1206 | 4106 | `	while( nMax != 0 ){` |
|    1200 | 4107 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|    1200 | 4108 | `		if( nMax > 0 && nMax < nAsk ){` |
|       5 | 4109 | `			nAsk = nMax;` |
|       2 | 4110 | `		}` |
|       - | 4111 | `		/* Through PH7_StreamRead, not the device: this is a SCRIPT-level read,` |
|       - | 4112 | `		 * and the line readers buffer AHEAD. Reading the device directly meant` |
|       - | 4113 | ``		 * `stream_get_contents()` after any fgets()/fgetc()/stream_get_line()`` |
|       - | 4114 | `		 * skipped everything still sitting in that buffer — on a file the` |
|       - | 4115 | `		 * line reader had already drained to its end, that is the WHOLE` |
|       - | 4116 | `		 * remainder, so the everyday "read the first line, then take the rest"` |
|       - | 4117 | `		 * idiom answered "" and the position it left behind was wrong too. */` |
|    1200 | 4118 | `		nRead = PH7_StreamRead(pDev,zBuf,nAsk);` |
|    1200 | 4119 | `		if( nRead < 1 ){` |
|     634 | 4120 | `			if( nRead == 0 ){` |
|     628 | 4121 | `				pDev->bEof = 1;` |
|     312 | 4122 | `			}` |
|     634 | 4123 | `			StreamReportReadFailure(pCtx,pDev);` |
|     634 | 4124 | `			break;` |
|       - | 4125 | `		}` |
|     570 | 4126 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     570 | 4127 | `		if( nMax > 0 ){` |
|       5 | 4128 | `			nMax -= nRead;` |
|       2 | 4129 | `		}` |
|       4 | 4130 | `	}` |
|     640 | 4131 | `	return PH7_OK;` |
|     324 | 4132 | `}` |
|       - | 4133 | `/*` |
|       - | 4134 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|       - | 4135 | ` */` |
|      47 | 4136 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4137 | `{` |
|       - | 4138 | `	ph7_value *pArr,*pV;` |
|       - | 4139 | `	ph7_io_stream **apDev;` |
|       - | 4140 | `	sxu32 n;` |
|      23 | 4141 | `	SXUNUSED(nArg);` |
|      23 | 4142 | `	SXUNUSED(apArg);` |
|      51 | 4143 | `	pArr = ph7_context_new_array(pCtx);` |
|      51 | 4144 | `	pV = ph7_context_new_scalar(pCtx);` |
|      51 | 4145 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 | 4146 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4147 | `		return PH7_OK;` |
|       - | 4148 | `	}` |
|      51 | 4149 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     486 | 4150 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|       - | 4151 | `		/* A device a script has unregistered is GONE from php's list -- both a` |
|       - | 4152 | `		 * built-in it switched off and a userland wrapper it withdrew, which` |
|       - | 4153 | `		 * PHL used to keep naming here after neutering the slot behind it. */` |
|     439 | 4154 | `		if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|       9 | 4155 | `			continue;` |
|       - | 4156 | `		}` |
|     431 | 4157 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     431 | 4158 | `		ph7_array_add_elem(pArr,0,pV);` |
|     431 | 4159 | `		ph7_value_reset_string_cursor(pV);` |
|     213 | 4160 | `	}` |
|      51 | 4161 | `	ph7_result_value(pCtx,pArr);` |
|      51 | 4162 | `	return PH7_OK;` |
|      27 | 4163 | `}` |
|       - | 4164 | `/* The userland-wrapper pool is declared further down this file. */` |
|       - | 4165 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream);` |
|       - | 4166 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);` |
|       - | 4167 | `/*` |
|       - | 4168 | ` * php names TWO things in a stream's metadata: the WRAPPER that opened it` |
|       - | 4169 | `` * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for`` |
|       - | 4170 | `` * nearly every device — an ordinary file is opened by `plainfile` and driven by`` |
|       - | 4171 | `` * `STDIO` — and PHL answered its own single device name for both, so neither`` |
|       - | 4172 | ` * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()` |
|       - | 4173 | `` * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.`` |
|       - | 4174 | ` */` |
|     148 | 4175 | `static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)` |
|       5 | 4176 | `{` |
|     153 | 4177 | `	const ph7_io_stream *pS = pDev->pStream;` |
|     153 | 4178 | `	*pzWrapper = 0;` |
|     153 | 4179 | `	*pzStream  = "STDIO";` |
|     153 | 4180 | `	if( pS == 0 ){` |
|     ! 0 | 4181 | `		return;` |
|       - | 4182 | `	}` |
|     153 | 4183 | `	if( pDev->bDir ){` |
|       3 | 4184 | `		*pzWrapper = "plainfile";` |
|       3 | 4185 | `		*pzStream  = "dir";` |
|       3 | 4186 | `		return;` |
|       - | 4187 | `	}` |
|     151 | 4188 | `	if( is_php_stream(pS) ){` |
|      46 | 4189 | `		*pzWrapper = "PHP";` |
|      46 | 4190 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|       - | 4191 | `			/* php://output is the VM's output consumer, not a descriptor. */` |
|       3 | 4192 | `			*pzStream = "Output";` |
|       3 | 4193 | `			return;` |
|       - | 4194 | `		}` |
|      44 | 4195 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_INPUT ){` |
|       - | 4196 | `			/* php's request body, which a command line never has. */` |
|      11 | 4197 | `			*pzStream = "Input";` |
|      11 | 4198 | `			return;` |
|       - | 4199 | `		}` |
|      34 | 4200 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|       - | 4201 | `			/* php://memory and php://temp are ONE device here and two in php,` |
|       - | 4202 | `			 * which labels them apart; the URI is what separates them. */` |
|      26 | 4203 | `			const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|      26 | 4204 | `			sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|      36 | 4205 | `			*pzStream = ( nUri >= sizeof("php://temp")-1` |
|      22 | 4206 | `			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )` |
|      24 | 4207 | `				? "TEMP" : "MEMORY";` |
|      10 | 4208 | `		}` |
|      34 | 4209 | `		return;` |
|       - | 4210 | `	}` |
|     109 | 4211 | `	if( is_data_stream(pS) ){` |
|      15 | 4212 | `		*pzWrapper = *pzStream = "RFC2397";` |
|      15 | 4213 | `		return;` |
|       - | 4214 | `	}` |
|       - | 4215 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|      95 | 4216 | `	if( PH7_ZipStreamIs(pS) ){` |
|       - | 4217 | ``		/* One device, two doors, the same shape ZLIB has below: `zip://` opens`` |
|       - | 4218 | `		 * it through a WRAPPER and ZipArchive::getStream() hands one out` |
|       - | 4219 | ``		 * directly. php reports the ops as `zip` either way and names the`` |
|       - | 4220 | `		 * wrapper only for the first -- and here the URI cannot tell them` |
|       - | 4221 | `		 * apart, because getStream()'s stream carries the ENTRY name as its` |
|       - | 4222 | `		 * uri, so the handle is asked instead. */` |
|       5 | 4223 | `		*pzStream = "zip";` |
|       5 | 4224 | `		if( PH7_ZipStreamViaWrapper(pDev->pHandle) ){` |
|       3 | 4225 | `			*pzWrapper = "zip wrapper";` |
|       1 | 4226 | `		}` |
|       5 | 4227 | `		return;` |
|       - | 4228 | `	}` |
|       - | 4229 | `#endif` |
|       - | 4230 | `#ifdef PH7_ENABLE_ZLIB` |
|      91 | 4231 | `	if( PH7_ZlibStreamIs(pS) ){` |
|       - | 4232 | `		/* One device, two doors: compress.zlib:// goes through a WRAPPER and` |
|       - | 4233 | `		 * gzopen() opens the device directly -- which php reports by leaving` |
|       - | 4234 | ``		 * both `wrapper_type` and `uri` off the second one. The ops are the`` |
|       - | 4235 | `		 * same either way, and php names them ZLIB. */` |
|       5 | 4236 | `		*pzStream = "ZLIB";` |
|       5 | 4237 | `		if( SyBlobLength(&pDev->sUri) > 0 ){` |
|       3 | 4238 | `			*pzWrapper = "ZLIB";` |
|       1 | 4239 | `		}` |
|       5 | 4240 | `		return;` |
|       - | 4241 | `	}` |
|       - | 4242 | `#endif` |
|      87 | 4243 | `	if( IoPrivateIsUwrap(pS) ){` |
|       7 | 4244 | `		*pzWrapper = *pzStream = "user-space";` |
|       7 | 4245 | `		return;` |
|       - | 4246 | `	}` |
|       - | 4247 | `#ifdef PH7_ENABLE_NET` |
|      81 | 4248 | `	if( PH7_HttpStreamIs(pS) ){` |
|       - | 4249 | ``		/* php names the WRAPPER `http` and the ops under it the transport's,`` |
|       - | 4250 | `		 * which is the same socket label a tcp:// handle reports. */` |
|       4 | 4251 | `		*pzWrapper = "http";` |
|       4 | 4252 | `		*pzStream  = "tcp_socket/ssl";` |
|       4 | 4253 | `		return;` |
|       - | 4254 | `	}` |
|       - | 4255 | `#endif` |
|      77 | 4256 | `	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){` |
|       - | 4257 | `		/* php names the socket ops and reports no wrapper for them — and it has` |
|       - | 4258 | `		 * a set of ops PER TRANSPORT, so the label is how a script tells which` |
|       - | 4259 | `		 * one its handle got: a socket with no transport under it (a pair) is` |
|       - | 4260 | ``		 * `generic_socket` and a DATAGRAM one `udp_socket`. */`` |
|       - | 4261 | `#ifdef PH7_ENABLE_NET` |
|      40 | 4262 | `		if( pDev->pHandle && ((sock_private *)pDev->pHandle)->zLabel ){` |
|       - | 4263 | `			/* ext/sockets stated it outright (socket_export_stream). */` |
|       9 | 4264 | `			*pzStream = ((sock_private *)pDev->pHandle)->zLabel;` |
|      36 | 4265 | `		}else if( pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric ){` |
|       3 | 4266 | `			*pzStream = "generic_socket";` |
|      30 | 4267 | `		}else if( pDev->pHandle && ((sock_private *)pDev->pHandle)->bDgram ){` |
|       5 | 4268 | `			*pzStream = "udp_socket";` |
|       3 | 4269 | `		}else{` |
|      24 | 4270 | `			*pzStream = "tcp_socket/ssl";` |
|       - | 4271 | `		}` |
|       - | 4272 | `#else` |
|       - | 4273 | `		*pzStream = "tcp_socket/ssl";` |
|       - | 4274 | `#endif` |
|      40 | 4275 | `		return;` |
|       - | 4276 | `	}` |
|      38 | 4277 | `	if( SyBlobLength(&pDev->sUri) < 1 ){` |
|       - | 4278 | `		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()` |
|       - | 4279 | `		 * or proc_open() pipe end, or a device an extension built by hand like` |
|       - | 4280 | `		 * PDO's blob handle — which is precisely when php reports neither a` |
|       - | 4281 | ``		 * `wrapper_type` nor a `uri`. Such a device names its OWN ops: php's`` |
|       - | 4282 | ``		 * blob stream reports `PDOSQLite`, not the `STDIO` a descriptor gets. */`` |
|      12 | 4283 | `		if( pS->zName && pS->xOpen == 0 && pS->xOpenDir == 0 && pS->xSeek != 0 ){` |
|       - | 4284 | `			/* No opener, no descriptor under it, and it can SEEK: an extension` |
|       - | 4285 | `			 * built this handle itself (PDO's blob and LOB streams), and php` |
|       - | 4286 | `			 * names such a device's own ops. A pipe or a socket end has no seek` |
|       - | 4287 | `			 * and stays php's descriptor label. */` |
|      10 | 4288 | `			*pzStream = pS->zName;` |
|       4 | 4289 | `		}` |
|      12 | 4290 | `		return;` |
|       - | 4291 | `	}` |
|      27 | 4292 | `	*pzWrapper = "plainfile";` |
|      78 | 4293 | `}` |
|       - | 4294 | `/*` |
|       - | 4295 | ` * The WRAPPER label on its own, for a device rather than an open handle: php's` |
|       - | 4296 | `` * path operations name it in their refusals (`unlink(): ZLIB does not allow`` |
|       - | 4297 | `` * unlinking`), and they have no handle to ask. Answers 0 for the plain-file`` |
|       - | 4298 | ` * wrapper -- which implements the operations rather than refusing them -- and` |
|       - | 4299 | ` * for a userland one, whose own door runs before this.` |
|       - | 4300 | ` */` |
|   50360 | 4301 | `PH7_PRIVATE const char * PH7_StreamWrapperLabel(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 | 4302 | `{` |
|   50365 | 4303 | `	if( pStream == 0 \|\| pStream == pVm->pDefStream \|\| IoPrivateIsUwrap(pStream) ){` |
|   50252 | 4304 | `		return 0;` |
|       - | 4305 | `	}` |
|     115 | 4306 | `	if( is_php_stream(pStream) ){` |
|      14 | 4307 | `		return "PHP";` |
|       - | 4308 | `	}` |
|     103 | 4309 | `	if( is_data_stream(pStream) ){` |
|       8 | 4310 | `		return "RFC2397";` |
|       - | 4311 | `	}` |
|       - | 4312 | `#ifdef PH7_ENABLE_ZLIB` |
|      97 | 4313 | `	if( PH7_ZlibStreamIs(pStream) ){` |
|      14 | 4314 | `		return "ZLIB";` |
|       - | 4315 | `	}` |
|       - | 4316 | `#endif` |
|       - | 4317 | `	/* php's remaining built-in wrappers label themselves with their scheme. */` |
|      84 | 4318 | `	return pStream->zName;` |
|   25160 | 4319 | `}` |
|       - | 4320 | `/*` |
|       - | 4321 | `` * The `stream_type` label above, on its own. ext/posix prints it in php's`` |
|       - | 4322 | `` * `Could not use stream of type '%s'` -- the diagnostic a descriptor door`` |
|       - | 4323 | ` * raises for a stream that has no descriptor behind it -- and php reads it from` |
|       - | 4324 | ` * the same place its metadata does.` |
|       - | 4325 | ` */` |
|       4 | 4326 | `PH7_PRIVATE const char * PH7_StreamTypeLabel(io_private *pDev)` |
|       1 | 4327 | `{` |
|       5 | 4328 | `	const char *zWrapper = 0,*zStream = "STDIO";` |
|       5 | 4329 | `	if( pDev == 0 ){` |
|     ! 0 | 4330 | `		return "STDIO";` |
|       - | 4331 | `	}` |
|       5 | 4332 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|       5 | 4333 | `	return zStream;` |
|       2 | 4334 | `}` |
|       - | 4335 |  |
|       - | 4336 | `/*` |
|       - | 4337 | ` * data:// carries its own metadata in php, and all of it comes back out of the` |
|       - | 4338 | ``  * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>` `` |
|       - | 4339 | ` * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A` |
|       - | 4340 | `` * URI naming no media type has no `mediatype` key at all — php does not`` |
|       - | 4341 | ` * substitute the RFC's default — and a repeated parameter keeps its last value.` |
|       - | 4342 | ` */` |
|      12 | 4343 | `static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|       1 | 4344 | `{` |
|      13 | 4345 | `	const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|      13 | 4346 | `	sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|      13 | 4347 | `	sxu32 nStart = 0,nComma,nSeg,i;` |
|      13 | 4348 | `	int bBase64 = 0,bFirst = 1;` |
|      13 | 4349 | `	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){` |
|      13 | 4350 | `		nStart = sizeof("data://")-1;` |
|       6 | 4351 | `	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){` |
|     ! 0 | 4352 | `		nStart = sizeof("data:")-1;` |
|     ! 0 | 4353 | `	}` |
|      13 | 4354 | `	nComma = nStart;` |
|     329 | 4355 | `	while( nComma < nUri && zUri[nComma] != ',' ){` |
|     317 | 4356 | `		nComma++;` |
|       1 | 4357 | `	}` |
|       - | 4358 | `	/* Walk the ';'-separated segments in front of the payload. */` |
|      23 | 4359 | `	for( nSeg = nStart ; nSeg <= nComma ; ){` |
|      23 | 4360 | `		sxu32 nEnd = nSeg;` |
|     329 | 4361 | `		while( nEnd < nComma && zUri[nEnd] != ';' ){` |
|     307 | 4362 | `			nEnd++;` |
|       1 | 4363 | `		}` |
|      23 | 4364 | `		if( bFirst ){` |
|      13 | 4365 | `			if( nEnd > nSeg ){` |
|      11 | 4366 | `				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));` |
|      11 | 4367 | `				ph7_array_add_strkey_elem(pArr,"mediatype",pV);` |
|      11 | 4368 | `				ph7_value_reset_string_cursor(pV);` |
|       5 | 4369 | `			}` |
|      13 | 4370 | `			bFirst = 0;` |
|      17 | 4371 | `		}else if( nEnd - nSeg == sizeof("base64")-1` |
|       8 | 4372 | `		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){` |
|       5 | 4373 | `			bBase64 = 1;` |
|       3 | 4374 | `		}else{` |
|       - | 4375 | ``			/* `name=value`; php keys the array by the name, so a repeat wins. */`` |
|     167 | 4376 | `			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}` |
|       7 | 4377 | `			if( i < nEnd && i > nSeg ){` |
|       - | 4378 | `				/* The name is keyed WHOLE — it has no length limit in the URI,` |
|       - | 4379 | `				 * and a clamped one files the value under a key no script can` |
|       - | 4380 | `				 * look up. */` |
|       7 | 4381 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|       7 | 4382 | `				if( pKey ){` |
|       7 | 4383 | `					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));` |
|       7 | 4384 | `					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));` |
|       7 | 4385 | `					ph7_array_add_elem(pArr,pKey,pV);` |
|       7 | 4386 | `					ph7_value_reset_string_cursor(pV);` |
|       7 | 4387 | `					ph7_context_release_value(pCtx,pKey);` |
|       3 | 4388 | `				}` |
|       3 | 4389 | `			}` |
|       - | 4390 | `		}` |
|      23 | 4391 | `		if( nEnd >= nComma ){` |
|      13 | 4392 | `			break;` |
|       - | 4393 | `		}` |
|      11 | 4394 | `		nSeg = nEnd + 1;` |
|       1 | 4395 | `	}` |
|      13 | 4396 | `	ph7_value_bool(pV,bBase64);` |
|      13 | 4397 | `	ph7_array_add_strkey_elem(pArr,"base64",pV);` |
|      13 | 4398 | `}` |
|       - | 4399 | `/*` |
|       - | 4400 | ` * array stream_get_meta_data(resource $stream)` |
|       - | 4401 | ` *` |
|       - | 4402 | ` * php's own key set, in php's own order. What used to be here answered a` |
|       - | 4403 | `` * best-effort shape: `mode` and `uri` did not exist at all (so the documented`` |
|       - | 4404 | `` * way to ask a handle what FILE it is on was an `Undefined array key` and`` |
|       - | 4405 | `` * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),`` |
|       - | 4406 | `` * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both`` |
|       - | 4407 | ` * PHL's internal device name rather than php's two different labels.` |
|       - | 4408 | ` */` |
|     134 | 4409 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4410 | `{` |
|       - | 4411 | `	const char *zWrapper,*zStream;` |
|       - | 4412 | `	io_private *pDev;` |
|       - | 4413 | `	ph7_value *pArr,*pV;` |
|       - | 4414 | `	sxu32 nUnread;` |
|       - | 4415 | `	int rc;` |
|     139 | 4416 | `	if( nArg < 1 ){` |
|     ! 0 | 4417 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 4418 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4419 | `		return PH7_OK;` |
|       - | 4420 | `	}` |
|       - | 4421 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     139 | 4422 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     139 | 4423 | `	if( pDev == 0 ){` |
|       3 | 4424 | `		return rc;` |
|       - | 4425 | `	}` |
|     137 | 4426 | `	pArr = ph7_context_new_array(pCtx);` |
|     137 | 4427 | `	pV = ph7_context_new_scalar(pCtx);` |
|     137 | 4428 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 | 4429 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4430 | `		return PH7_OK;` |
|       - | 4431 | `	}` |
|     137 | 4432 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|       - | 4433 | `	/* Sample this BEFORE the eof probe below: php answers eof from state it` |
|       - | 4434 | ``	 * already has and never reads ahead for it, so its `unread_bytes` counts`` |
|       - | 4435 | `	 * only what the SCRIPT's own reads left buffered. */` |
|     137 | 4436 | `	nUnread = StreamAheadBytes(pDev);` |
|       - | 4437 | `#ifdef PH7_ENABLE_NET` |
|     137 | 4438 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - | 4439 | `		/* The http wrapper does its own buffering, and what it holds is exactly` |
|       - | 4440 | `		 * what php counts here. */` |
|       4 | 4441 | `		nUnread += PH7_HttpStreamUnread(pDev->pHandle);` |
|       2 | 4442 | `	}` |
|       - | 4443 | `#endif` |
|     137 | 4444 | `	if( is_data_stream(pDev->pStream) ){` |
|       - | 4445 | `		/* A device that answers metadata of its OWN replaces php's three` |
|       - | 4446 | `		 * defaults rather than adding to them: data:// (and php://temp, which` |
|       - | 4447 | `		 * simply has none) report no timed_out/blocked/eof at all. */` |
|      13 | 4448 | `		IoPrivateDataMeta(pCtx,pDev,pArr,pV);` |
|      13 | 4449 | `		ph7_value_reset_string_cursor(pV);` |
|     127 | 4450 | `	}else if( is_php_stream(pDev->pStream)` |
|      76 | 4451 | `	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY` |
|      28 | 4452 | `	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){` |
|       - | 4453 | `		/* php://temp: same rule, no keys of its own. */` |
|       2 | 4454 | `	}else{` |
|     123 | 4455 | `		ph7_value_bool(pV,pDev->bTimedOut != 0);` |
|     123 | 4456 | `		ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|       - | 4457 | `		/* A stream php cannot put in non-blocking mode always reports blocked;` |
|       - | 4458 | `		 * bNonBlock is only ever set for one that CAN. */` |
|     123 | 4459 | `		ph7_value_bool(pV,pDev->bNonBlock == 0);` |
|     123 | 4460 | `		ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|       - | 4461 | `		/* php COPIES the stored flag here and runs no probe of its own, so a` |
|       - | 4462 | `		 * socket feof() has not been called on yet reports false even when its` |
|       - | 4463 | `		 * peer is already gone. */` |
|     123 | 4464 | `		ph7_value_bool(pV,StreamEofCommon(pDev,0) != 0);` |
|     123 | 4465 | `		ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|       - | 4466 | `	}` |
|       - | 4467 | `#ifdef PH7_ENABLE_NET` |
|     137 | 4468 | `	if( PH7_HttpStreamIs(pDev->pStream) ){` |
|       - | 4469 | ``		/* php's `wrapper_data` for an http handle is the response headers of the`` |
|       - | 4470 | `		 * exchange THIS handle made -- the redirect chain's included, in the` |
|       - | 4471 | `		 * order they arrived. */` |
|       4 | 4472 | `		ph7_value *pHdr = PH7_HttpStreamHeaderArray(pCtx->pVm,pDev->pHandle);` |
|       4 | 4473 | `		if( pHdr ){` |
|       4 | 4474 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",pHdr);` |
|       4 | 4475 | `			ph7_release_value(pCtx->pVm,pHdr);` |
|       2 | 4476 | `		}` |
|       2 | 4477 | `	}` |
|       - | 4478 | `#endif` |
|       - | 4479 | `	{` |
|     137 | 4480 | `		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);` |
|     137 | 4481 | `		if( pObj ){` |
|       - | 4482 | `			/* php hands the wrapper INSTANCE back, which is the only way a` |
|       - | 4483 | `			 * script can reach the object serving an open userland stream. */` |
|       - | 4484 | `			ph7_value sObj;` |
|       5 | 4485 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|       5 | 4486 | `			sObj.x.pOther = pObj;` |
|       5 | 4487 | `			sObj.iFlags = MEMOBJ_OBJ;` |
|       5 | 4488 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);` |
|       2 | 4489 | `		}` |
|       - | 4490 | `	}` |
|     137 | 4491 | `	if( zWrapper ){` |
|      86 | 4492 | `		ph7_value_string(pV,zWrapper,-1);` |
|      86 | 4493 | `		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      86 | 4494 | `		ph7_value_reset_string_cursor(pV);` |
|      41 | 4495 | `	}` |
|     137 | 4496 | `	ph7_value_string(pV,zStream,-1);` |
|     137 | 4497 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|     137 | 4498 | `	ph7_value_reset_string_cursor(pV);` |
|     137 | 4499 | `	ph7_value_string(pV,pDev->zMode,-1);` |
|     137 | 4500 | `	ph7_array_add_strkey_elem(pArr,"mode",pV);` |
|     137 | 4501 | `	ph7_value_reset_string_cursor(pV);` |
|       - | 4502 | `	/* Bytes already pulled off the device and not yet handed to the script —` |
|       - | 4503 | `	 * php's own writepos-minus-readpos, which was hardcoded 0. */` |
|     137 | 4504 | `	ph7_value_int64(pV,(ph7_int64)nUnread);` |
|     137 | 4505 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|       - | 4506 | `	{` |
|       - | 4507 | `		/* php answers this from what the handle actually SITS ON, not from what` |
|       - | 4508 | `		 * the device could do: php://stdout is seekable into a file and not` |
|       - | 4509 | `		 * down a pipe, php://output never is, and a pipe is not. Ask the` |
|       - | 4510 | `		 * descriptor first and the device second; a USERLAND wrapper is php's` |
|       - | 4511 | `		 * one exception — its ops always carry a seek, so php always says yes. */` |
|     137 | 4512 | `		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;` |
|     137 | 4513 | `		if( pDev->bDir ){` |
|       - | 4514 | `			/* php's directory ops carry a rewind, so a dir handle is seekable —` |
|       - | 4515 | `			 * and asking the FILE device where it is would hand lseek() the` |
|       - | 4516 | `			 * DIR* this handle stores where a file stores its descriptor. */` |
|       3 | 4517 | `			bSeekable = 1;` |
|     136 | 4518 | `		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){` |
|      83 | 4519 | `			int rcSeek = PH7_StreamHandleCanSeek(pDev);` |
|      83 | 4520 | `			if( rcSeek >= 0 ){` |
|      31 | 4521 | `				bSeekable = rcSeek;` |
|      66 | 4522 | `			}else if( is_php_stream(pDev->pStream)` |
|      41 | 4523 | `			       && PH7_PhpStreamInner(pDev->pHandle) == 0 ){` |
|       - | 4524 | `				/* php://output has no seek AT ALL, and its neighbours on this` |
|       - | 4525 | `				 * one device do. Ask with the seek that moves nothing: an` |
|       - | 4526 | `				 * unsupported one answers SXERR_NOTIMPLEMENTED, which is the` |
|       - | 4527 | `				 * same "no seek here" php's NO_SEEK flag records. The question` |
|       - | 4528 | `				 * is not put to any other device -- a handle an extension built` |
|       - | 4529 | `				 * can have a POSITION riding on its last seek (PDO's blob marks` |
|       - | 4530 | `				 * its own unknown), and asking would move it -- nor to a` |
|       - | 4531 | `				 * php://filter PROXY, whose seek is a real seek of the stream` |
|       - | 4532 | `				 * underneath and would drop what the chain had produced. */` |
|      41 | 4533 | `				bSeekable = pDev->pStream->xSeek(pDev->pHandle,0,1/*SEEK_CUR*/)` |
|      26 | 4534 | `					!= SXERR_NOTIMPLEMENTED;` |
|      39 | 4535 | `			}else if( pDev->pStream->xOpen != 0 && pDev->pStream->xTell != 0 ){` |
|       - | 4536 | `				/* Ask the HANDLE where it is, which is how a descriptor-backed` |
|       - | 4537 | `				 * device says it cannot seek. A device an extension built by` |
|       - | 4538 | `				 * hand (PDO's blob handle) has no opener and answers for itself` |
|       - | 4539 | `				 * -- its xSeek IS the answer, and a position it reports as` |
|       - | 4540 | `				 * unknown after a failed seek must not read as "not seekable". */` |
|      18 | 4541 | `				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;` |
|       8 | 4542 | `			}` |
|      40 | 4543 | `		}` |
|     137 | 4544 | `		ph7_value_bool(pV,bSeekable);` |
|       - | 4545 | `	}` |
|     137 | 4546 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|     137 | 4547 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|       - | 4548 | `		/* php keeps the path exactly as the opener received it — a relative` |
|       - | 4549 | `		 * one stays relative — and omits the key for a stream that has none. */` |
|     113 | 4550 | `		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));` |
|     113 | 4551 | `		ph7_array_add_strkey_elem(pArr,"uri",pV);` |
|     113 | 4552 | `		ph7_value_reset_string_cursor(pV);` |
|      54 | 4553 | `	}` |
|     137 | 4554 | `	ph7_result_value(pCtx,pArr);` |
|     137 | 4555 | `	return PH7_OK;` |
|      72 | 4556 | `}` |
|       - | 4557 | `/*` |
|       - | 4558 | ` * ---------------------------------------------------------------------------` |
|       - | 4559 | ` * Stream contexts (stream_context_create and the accessor family).` |
|       - | 4560 | ` *` |
|       - | 4561 | `` * php's context is a `stream-context` RESOURCE holding two things: a`` |
|       - | 4562 | `` * wrapper => option => value map, and the `notification` parameter. Both`` |
|       - | 4563 | ` * levels keep INSERTION order, which is the order stream_context_get_options()` |
|       - | 4564 | ` * answers in, so the store is a real nested array rather than a flat table.` |
|       - | 4565 | ` *` |
|       - | 4566 | ` * A PHL resource is a bare void*, so the struct opens with an io_private` |
|       - | 4567 | ` * header carrying its own magic (the shape proc_open()'s handle already uses)` |
|       - | 4568 | ` * and the VM owns every one it hands out.` |
|       - | 4569 | ` * ---------------------------------------------------------------------------` |
|       - | 4570 | ` */` |
|       - | 4571 | `/* Allocate one context, chained on the VM registry. */` |
|     620 | 4572 | `static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)` |
|       5 | 4573 | `{` |
|       - | 4574 | `	phl_stream_ctx *pRes;` |
|     625 | 4575 | `	if( pVm == 0 ){` |
|     ! 0 | 4576 | `		return 0;` |
|       - | 4577 | `	}` |
|     625 | 4578 | `	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));` |
|     625 | 4579 | `	if( pRes == 0 ){` |
|     ! 0 | 4580 | `		return 0;` |
|       - | 4581 | `	}` |
|     625 | 4582 | `	SyZero(pRes,sizeof(phl_stream_ctx));` |
|     625 | 4583 | `	pRes->base.iMagic = STREAM_CTX_MAGIC;` |
|     625 | 4584 | `	pRes->pVm = pVm;` |
|     625 | 4585 | `	pRes->pOptions = ph7_new_array(pVm);` |
|     625 | 4586 | `	if( pRes->pOptions == 0 ){` |
|     ! 0 | 4587 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|     ! 0 | 4588 | `		return 0;` |
|       - | 4589 | `	}` |
|     625 | 4590 | `	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;` |
|     625 | 4591 | `	pVm->pStreamCtx = (void *)pRes;` |
|     625 | 4592 | `	return pRes;` |
|     313 | 4593 | `}` |
|       - | 4594 | `/*` |
|       - | 4595 | ` * The context behind a ph7_value, or 0 when the value is not one. The magic` |
|       - | 4596 | ` * probe is the same in-bounds one every resource here answers to.` |
|       - | 4597 | ` */` |
|     350 | 4598 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)` |
|       5 | 4599 | `{` |
|       - | 4600 | `	phl_stream_ctx *pRes;` |
|     355 | 4601 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|      19 | 4602 | `		return 0;` |
|       - | 4603 | `	}` |
|     337 | 4604 | `	pRes = (phl_stream_ctx *)pVal->x.pOther;` |
|     337 | 4605 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_CTX_MAGIC ){` |
|      49 | 4606 | `		return 0;` |
|       - | 4607 | `	}` |
|     290 | 4608 | `	return pRes;` |
|     180 | 4609 | `}` |
|       - | 4610 | `/*` |
|       - | 4611 | ` * The per-VM DEFAULT context. php creates it on demand — the first` |
|       - | 4612 | ` * stream_context_get_default()/set_default() call — and every opener that was` |
|       - | 4613 | ` * handed no context of its own falls back to it.` |
|       - | 4614 | ` */` |
|   83056 | 4615 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)` |
|       5 | 4616 | `{` |
|   83061 | 4617 | `	if( pVm == 0 ){` |
|     ! 0 | 4618 | `		return 0;` |
|       - | 4619 | `	}` |
|   83061 | 4620 | `	if( pVm->pDefaultCtx == 0 ){` |
|     373 | 4621 | `		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);` |
|     182 | 4622 | `	}` |
|   83061 | 4623 | `	return (phl_stream_ctx *)pVm->pDefaultCtx;` |
|   41499 | 4624 | `}` |
|       - | 4625 | `/*` |
|       - | 4626 | ` * Drop every context this VM created. Called from PH7_VmReset, so a reused VM` |
|       - | 4627 | ` * (the -S server's) does not carry one request's default context into the next.` |
|       - | 4628 | ` */` |
|      16 | 4629 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|     ! 0 | 4630 | `{` |
|       - | 4631 | `	phl_stream_ctx *pRes;` |
|      16 | 4632 | `	if( pVm == 0 ){` |
|     ! 0 | 4633 | `		return;` |
|       - | 4634 | `	}` |
|      16 | 4635 | `	pRes = (phl_stream_ctx *)pVm->pStreamCtx;` |
|      24 | 4636 | `	while( pRes ){` |
|       8 | 4637 | `		phl_stream_ctx *pNext = pRes->pNext;` |
|       8 | 4638 | `		if( pRes->pOptions ){` |
|       8 | 4639 | `			ph7_release_value(pVm,pRes->pOptions);` |
|       4 | 4640 | `		}` |
|       8 | 4641 | `		if( pRes->pNotify ){` |
|     ! 0 | 4642 | `			ph7_release_value(pVm,pRes->pNotify);` |
|     ! 0 | 4643 | `		}` |
|       - | 4644 | `		/* Any ph7_value still naming this pointer must stop reporting a live` |
|       - | 4645 | `		 * context, so clear the magic before the memory goes back. */` |
|       8 | 4646 | `		pRes->base.iMagic = 0;` |
|       8 | 4647 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|       8 | 4648 | `		pRes = pNext;` |
|     ! 0 | 4649 | `	}` |
|      16 | 4650 | `	pVm->pStreamCtx = 0;` |
|      16 | 4651 | `	pVm->pDefaultCtx = 0;` |
|       - | 4652 | `	/* Whatever an interrupted open left armed named one of those. */` |
|      16 | 4653 | `	pVm->pOpenCtx = 0;` |
|       8 | 4654 | `}` |
|       - | 4655 | `/* The live element of pArray under pKey, or 0 when there is none. */` |
|     278 | 4656 | `static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)` |
|       4 | 4657 | `{` |
|       - | 4658 | `	ph7_hashmap_node *pNode;` |
|     282 | 4659 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 4660 | `		return 0;` |
|       - | 4661 | `	}` |
|     282 | 4662 | `	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){` |
|     126 | 4663 | `		return 0;` |
|       - | 4664 | `	}` |
|     160 | 4665 | `	return (ph7_value *)PH7_MemObjAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     143 | 4666 | `}` |
|       - | 4667 | `/*` |
|       - | 4668 | ` * Store one option. The wrapper's sub-array is created on first use; an` |
|       - | 4669 | ` * existing one may be SHARED with the script array it was stored from, so it` |
|       - | 4670 | ` * is separated first — otherwise setting an option would write through into` |
|       - | 4671 | ` * the caller's own array.` |
|       - | 4672 | ` */` |
|     156 | 4673 | `static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)` |
|       4 | 4674 | `{` |
|       - | 4675 | `	ph7_value sKey,sName,sVal;` |
|       - | 4676 | `	ph7_value *pSub;` |
|       - | 4677 | `	ph7_hashmap *pMap;` |
|     160 | 4678 | `	if( pRes == 0 \|\| pRes->pOptions == 0 \|\| pWrapper == 0 \|\| pName == 0 \|\| pValue == 0 ){` |
|     ! 0 | 4679 | `		return -1;` |
|       - | 4680 | `	}` |
|       - | 4681 | `	/* Every insertion below can reserve a memory object, which used to GROW (and` |
|       - | 4682 | `	 * therefore move) pVm->aMemObj — and all three arguments may point into it,` |
|       - | 4683 | `	 * so the structs are snapshotted first. Redundant since P1 (fixed segments);` |
|       - | 4684 | `	 * left for the harvest sweep (PERF.md P1). */` |
|     160 | 4685 | `	sKey = *pWrapper; pWrapper = &sKey;` |
|     160 | 4686 | `	sName = *pName;   pName = &sName;` |
|     160 | 4687 | `	sVal = *pValue;   pValue = &sVal;` |
|     160 | 4688 | `	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     160 | 4689 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     126 | 4690 | `		ph7_value *pFresh = ph7_new_array(pRes->pVm);` |
|     126 | 4691 | `		if( pFresh == 0 ){` |
|     ! 0 | 4692 | `			return -1;` |
|       - | 4693 | `		}` |
|     126 | 4694 | `		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){` |
|     ! 0 | 4695 | `			ph7_release_value(pRes->pVm,pFresh);` |
|     ! 0 | 4696 | `			return -1;` |
|       - | 4697 | `		}` |
|     126 | 4698 | `		ph7_release_value(pRes->pVm,pFresh);` |
|     126 | 4699 | `		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     126 | 4700 | `		if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 4701 | `			return -1;` |
|       - | 4702 | `		}` |
|      61 | 4703 | `	}` |
|     160 | 4704 | `	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);` |
|     160 | 4705 | `	if( pMap == 0 ){` |
|     ! 0 | 4706 | `		return -1;` |
|       - | 4707 | `	}` |
|     160 | 4708 | `	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;` |
|      82 | 4709 | `}` |
|       - | 4710 | `/*` |
|       - | 4711 | ` * One wrapper option by name, or 0 when the context does not carry it. This is` |
|       - | 4712 | ` * the read side every consumer (the socket transports) asks through.` |
|       - | 4713 | ` */` |
|    3528 | 4714 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)` |
|       5 | 4715 | `{` |
|       - | 4716 | `	ph7_value *pSub;` |
|    3533 | 4717 | `	if( pRes == 0 \|\| pRes->pOptions == 0 ){` |
|     ! 0 | 4718 | `		return 0;` |
|       - | 4719 | `	}` |
|    3533 | 4720 | `	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);` |
|    3533 | 4721 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    2629 | 4722 | `		return 0;` |
|       - | 4723 | `	}` |
|     906 | 4724 | `	return ph7_array_fetch(pSub,zOption,-1);` |
|    1769 | 4725 | `}` |
|       - | 4726 | `/*` |
|       - | 4727 | ` * php's parse_context_options: every entry must be wrappername => array, and a` |
|       - | 4728 | ` * non-array value — or an INTEGER key, which has no wrapper name at all — is` |
|       - | 4729 | ` * the ValueError below. An integer key one level DOWN has no option name, and` |
|       - | 4730 | ` * php drops that entry in silence rather than refusing the call.` |
|       - | 4731 | ` * Returns 0, or -1 once the exception has been raised.` |
|       - | 4732 | ` */` |
|     252 | 4733 | `static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)` |
|       4 | 4734 | `{` |
|       - | 4735 | `	ph7_hashmap *pMap;` |
|       - | 4736 | `	ph7_hashmap_node *pEntry;` |
|     256 | 4737 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 4738 | `		return 0;` |
|       - | 4739 | `	}` |
|     256 | 4740 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|     256 | 4741 | `	pMap->pCur = pMap->pFirst;` |
|     490 | 4742 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - | 4743 | `		ph7_value sKey;` |
|       - | 4744 | `		ph7_value *pVal;` |
|       - | 4745 | `		int bBad;` |
|     242 | 4746 | `		PH7_MemObjInit(pRes->pVm,&sKey);` |
|     242 | 4747 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     242 | 4748 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     359 | 4749 | `		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 \|\| pVal == 0` |
|     356 | 4750 | `		      \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );` |
|     242 | 4751 | `		if( bBad ){` |
|       5 | 4752 | `			PH7_MemObjRelease(&sKey);` |
|       5 | 4753 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4754 | `				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");` |
|       5 | 4755 | `			return -1;` |
|       - | 4756 | `		}` |
|       - | 4757 | `		{` |
|     238 | 4758 | `			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;` |
|       - | 4759 | `			ph7_hashmap_node *pOpt;` |
|     238 | 4760 | `			pSub->pCur = pSub->pFirst;` |
|     386 | 4761 | `			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){` |
|       - | 4762 | `				ph7_value sName;` |
|       - | 4763 | `				ph7_value *pOptVal;` |
|     152 | 4764 | `				PH7_MemObjInit(pRes->pVm,&sName);` |
|     152 | 4765 | `				PH7_HashmapExtractNodeKey(pOpt,&sName);` |
|     152 | 4766 | `				pOptVal = HashmapExtractNodeValue(pOpt);` |
|     152 | 4767 | `				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){` |
|     150 | 4768 | `					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);` |
|      73 | 4769 | `				}` |
|     152 | 4770 | `				PH7_MemObjRelease(&sName);` |
|       4 | 4771 | `			}` |
|       - | 4772 | `		}` |
|     238 | 4773 | `		PH7_MemObjRelease(&sKey);` |
|       4 | 4774 | `	}` |
|     252 | 4775 | `	return 0;` |
|     130 | 4776 | `}` |
|       - | 4777 | `/*` |
|       - | 4778 | `` * php's parse_context_params: only `notification` and `options` are read, and`` |
|       - | 4779 | ` * anything else in the array is ignored rather than refused. The notification` |
|       - | 4780 | ` * must be callable — php reports the same "must be an array with valid` |
|       - | 4781 | ` * callbacks as values" TypeError the callback taxonomy produces, naming` |
|       - | 4782 | ` * argument #1 whichever function was called.` |
|       - | 4783 | ` * Returns 0, or -1 once a diagnostic has been raised.` |
|       - | 4784 | ` */` |
|      58 | 4785 | `static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)` |
|       1 | 4786 | `{` |
|       - | 4787 | `	ph7_value *pVal;` |
|      59 | 4788 | `	if( pParams == 0 \|\| (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 | 4789 | `		return 0;` |
|       - | 4790 | `	}` |
|      59 | 4791 | `	pVal = ph7_array_fetch(pParams,"notification",-1);` |
|      59 | 4792 | `	if( pVal ){` |
|       - | 4793 | `		char zBuf[128];` |
|      53 | 4794 | `		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|      53 | 4795 | `		if( zReason ){` |
|       9 | 4796 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4797 | `				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",` |
|       6 | 4798 | `				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;` |
|       - | 4799 | `		}` |
|      49 | 4800 | `		if( pRes->pNotify == 0 ){` |
|      49 | 4801 | `			pRes->pNotify = ph7_new_scalar(pRes->pVm);` |
|      24 | 4802 | `		}` |
|      49 | 4803 | `		if( pRes->pNotify ){` |
|      49 | 4804 | `			PH7_MemObjStore(pVal,pRes->pNotify);` |
|      24 | 4805 | `		}` |
|      24 | 4806 | `	}` |
|      55 | 4807 | `	pVal = ph7_array_fetch(pParams,"options",-1);` |
|      55 | 4808 | `	if( pVal ){` |
|       7 | 4809 | `		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 4810 | `			/* php's own wording for a params entry it cannot use. */` |
|     ! 0 | 4811 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     ! 0 | 4812 | `				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;` |
|       - | 4813 | `		}` |
|       7 | 4814 | `		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){` |
|     ! 0 | 4815 | `			return -1;` |
|       - | 4816 | `		}` |
|       3 | 4817 | `	}` |
|      55 | 4818 | `	return 0;` |
|      30 | 4819 | `}` |
|       - | 4820 | `/*` |
|       - | 4821 | `` * Resolve the `$stream_or_context` first argument every accessor takes: a`` |
|       - | 4822 | ` * context resource answers itself, and a STREAM answers the context it` |
|       - | 4823 | ` * carries — created on demand for the setters, the way php's does, since a` |
|       - | 4824 | ` * stream opened without one still accepts stream_context_set_option().` |
|       - | 4825 | ` * Raises php's TypeError and returns 0 for anything else.` |
|       - | 4826 | ` */` |
|      78 | 4827 | `static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,` |
|       - | 4828 | `	const char *zArgName,int *pbThrew)` |
|       4 | 4829 | `{` |
|       - | 4830 | `	char zGiven[64];` |
|       - | 4831 | `	phl_stream_ctx *pRes;` |
|       - | 4832 | `	io_private *pDev;` |
|      82 | 4833 | `	*pbThrew = 1;` |
|      82 | 4834 | `	if( !ph7_value_is_resource(pVal) ){` |
|       8 | 4835 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4836 | `			"%s(): Argument #1 (%s) must be of type resource, %s given",` |
|       2 | 4837 | `			ph7_function_name(pCtx),zArgName,VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|       6 | 4838 | `		return 0;` |
|       - | 4839 | `	}` |
|      77 | 4840 | `	pRes = PH7_StreamCtxFromValue(pVal);` |
|      77 | 4841 | `	if( pRes ){` |
|      63 | 4842 | `		*pbThrew = 0;` |
|      63 | 4843 | `		return pRes;` |
|       - | 4844 | `	}` |
|      16 | 4845 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|      16 | 4846 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 4847 | `		/* A closed handle, a process handle, anything that is neither: php` |
|       - | 4848 | `		 * refuses the call rather than answering an empty option set. */` |
|       4 | 4849 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4850 | `			"%s(): Argument #1 (%s) must be a valid stream/context",` |
|       1 | 4851 | `			ph7_function_name(pCtx),zArgName);` |
|       3 | 4852 | `		return 0;` |
|       - | 4853 | `	}` |
|      14 | 4854 | `	*pbThrew = 0;` |
|      14 | 4855 | `	if( pDev->pCtxRes == 0 && bCreate ){` |
|       3 | 4856 | `		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);` |
|       1 | 4857 | `	}` |
|      14 | 4858 | `	return (phl_stream_ctx *)pDev->pCtxRes;` |
|      43 | 4859 | `}` |
|       - | 4860 | `/*` |
|       - | 4861 | `` * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body`` |
|       - | 4862 | `` * used to read. php's parameter is `?resource $context = null` and its rules`` |
|       - | 4863 | ` * are: a resource that is NOT a stream-context is refused outright, anything` |
|       - | 4864 | ` * else non-null is the ordinary type refusal, and NULL means the DEFAULT` |
|       - | 4865 | ` * context — which php creates on demand, so an opener never runs without one.` |
|       - | 4866 | ` *` |
|       - | 4867 | ` * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/` |
|       - | 4868 | ` * file_put_contents() carry to mean exactly "and do not fall back to it".` |
|       - | 4869 | ` * Returns 0 with *pbThrew set once a diagnostic has been raised.` |
|       - | 4870 | ` */` |
|   83158 | 4871 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 4872 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)` |
|       5 | 4873 | `{` |
|       - | 4874 | `	char zGiven[64];` |
|       - | 4875 | `	phl_stream_ctx *pRes;` |
|   83163 | 4876 | `	*pbThrew = 0;` |
|   83163 | 4877 | `	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){` |
|     266 | 4878 | `		if( !ph7_value_is_resource(apArg[iArg]) ){` |
|      11 | 4879 | `			*pbThrew = 1;` |
|      16 | 4880 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4881 | `				"%s(): Argument #%d (%s) must be of type resource or null, %s given",` |
|      10 | 4882 | `				ph7_function_name(pCtx),iArg + 1,zArgName,VmValueGivenName(apArg[iArg],zGiven,sizeof(zGiven)));` |
|      11 | 4883 | `			return 0;` |
|       - | 4884 | `		}` |
|     256 | 4885 | `		pRes = PH7_StreamCtxFromValue(apArg[iArg]);` |
|     256 | 4886 | `		if( pRes == 0 ){` |
|       - | 4887 | `			/* php names the RESOURCE it wanted rather than the argument here. */` |
|      34 | 4888 | `			*pbThrew = 1;` |
|      50 | 4889 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4890 | `				"%s(): supplied resource is not a valid Stream-Context resource",` |
|      16 | 4891 | `				ph7_function_name(pCtx));` |
|      34 | 4892 | `			return 0;` |
|       - | 4893 | `		}` |
|     223 | 4894 | `		return pRes;` |
|       - | 4895 | `	}` |
|   82901 | 4896 | `	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);` |
|   41550 | 4897 | `}` |
|       - | 4898 | `/*` |
|       - | 4899 | ` * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes` |
|       - | 4900 | ` * and clears it, so the slot describes exactly one open and a caller that never` |
|       - | 4901 | ` * set it finds nothing armed.` |
|       - | 4902 | ` */` |
|   31587 | 4903 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)` |
|       5 | 4904 | `{` |
|   31592 | 4905 | `	if( pVm ){` |
|   31592 | 4906 | `		pVm->pOpenCtx = (void *)pRes;` |
|   15784 | 4907 | `	}` |
|   31592 | 4908 | `}` |
|       - | 4909 | `/*` |
|       - | 4910 | ` * php's notification callback, called with the six arguments its documentation` |
|       - | 4911 | `` * names: `(int $code, int $severity, ?string $message, int $message_code,`` |
|       - | 4912 | `` * int $bytes_transferred, int $bytes_max)`. The message is NULL for every`` |
|       - | 4913 | ` * event that carries no text, which is most of them.` |
|       - | 4914 | ` *` |
|       - | 4915 | ` * The six arguments are STACK values, so the call allocates nothing of its own` |
|       - | 4916 | ` * -- the shape ext/curl's callbacks already use, and the one that matters here` |
|       - | 4917 | ` * because the callers are mid-exchange holding pointers into the handle. A` |
|       - | 4918 | ` * refusal raised by the callback unwinds on its own, the way a throwing` |
|       - | 4919 | ` * userland WRAPPER method's does; what this has to do is stop ASKING.` |
|       - | 4920 | ` */` |
|     826 | 4921 | `PH7_PRIVATE void PH7_StreamCtxNotify(phl_stream_ctx *pCtxRes,int iCode,int iSeverity,` |
|       - | 4922 | `	const char *zMsg,int nMsg,int iMsgCode,sxi64 iBytes,sxi64 iBytesMax)` |
|     ! 0 | 4923 | `{` |
|       - | 4924 | `	ph7_value sArgs[6],sRes,*apArg[6];` |
|       - | 4925 | `	ph7_vm *pVm;` |
|       - | 4926 | `	sxi32 rc;` |
|       - | 4927 | `	int i;` |
|     826 | 4928 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->pVm == 0` |
|     238 | 4929 | `	 \|\| pCtxRes->bNotifyDead ){` |
|     592 | 4930 | `		return;` |
|       - | 4931 | `	}` |
|     234 | 4932 | `	pVm = pCtxRes->pVm;` |
|    1638 | 4933 | `	for( i = 0 ; i < 6 ; ++i ){` |
|    1404 | 4934 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|    1404 | 4935 | `		apArg[i] = &sArgs[i];` |
|     702 | 4936 | `	}` |
|     234 | 4937 | `	ph7_value_int(&sArgs[0],iCode);` |
|     234 | 4938 | `	ph7_value_int(&sArgs[1],iSeverity);` |
|     234 | 4939 | `	if( zMsg ){` |
|      72 | 4940 | `		ph7_value_string(&sArgs[2],zMsg,nMsg);` |
|      36 | 4941 | `	}else{` |
|     162 | 4942 | `		ph7_value_null(&sArgs[2]);` |
|       - | 4943 | `	}` |
|     234 | 4944 | `	ph7_value_int(&sArgs[3],iMsgCode);` |
|     234 | 4945 | `	ph7_value_int64(&sArgs[4],iBytes);` |
|     234 | 4946 | `	ph7_value_int64(&sArgs[5],iBytesMax);` |
|     234 | 4947 | `	PH7_MemObjInit(pVm,&sRes);` |
|       - | 4948 | `	/* The status in a VARIABLE: PH7_CALLBACK_UNWOUND is a macro that reads its` |
|       - | 4949 | `	 * argument twice, so a call written inside it is MADE twice. */` |
|     234 | 4950 | `	rc = PH7_VmCallUserFunction(pVm,pCtxRes->pNotify,6,apArg,&sRes);` |
|     234 | 4951 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 4952 | `		/* php stops asking once the callback has refused: every notification` |
|       - | 4953 | `		 * after the throw is a userland call that would bail on the pending` |
|       - | 4954 | `		 * exception before it ran, so none of them is ever seen. The refusal` |
|       - | 4955 | `		 * itself unwinds on its own, the way a throwing userland WRAPPER` |
|       - | 4956 | `		 * method's already does. */` |
|       2 | 4957 | `		pCtxRes->bNotifyDead = 1;` |
|       1 | 4958 | `	}` |
|     234 | 4959 | `	PH7_MemObjRelease(&sRes);` |
|    1638 | 4960 | `	for( i = 0 ; i < 6 ; ++i ){` |
|    1404 | 4961 | `		PH7_MemObjRelease(&sArgs[i]);` |
|     702 | 4962 | `	}` |
|     413 | 4963 | `}` |
|       - | 4964 | `/*` |
|       - | 4965 | ` * php's php_stream_notify_progress_init: the counter starts again at zero with` |
|       - | 4966 | ` * a new maximum, the notifier is ARMED (it never disarms), and the zero itself` |
|       - | 4967 | ` * is reported. A wrapper calls this once it knows how big the body claims to` |
|       - | 4968 | ` * be -- which is why every PROGRESS before it belongs to the exchange BEFORE it.` |
|       - | 4969 | ` */` |
|     248 | 4970 | `PH7_PRIVATE void PH7_StreamCtxProgressInit(phl_stream_ctx *pCtxRes,sxi64 iMax)` |
|     ! 0 | 4971 | `{` |
|     248 | 4972 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->bNotifyDead ){` |
|     208 | 4973 | `		return;` |
|       - | 4974 | `	}` |
|      40 | 4975 | `	pCtxRes->iProgress = 0;` |
|      40 | 4976 | `	pCtxRes->iProgressMax = iMax;` |
|      40 | 4977 | `	pCtxRes->bProgress = 1;` |
|      60 | 4978 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_PROGRESS,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      20 | 4979 | `		0,0,0,0,iMax);` |
|     124 | 4980 | `}` |
|       - | 4981 | `/*` |
|       - | 4982 | ` * One transfer step: php counts every byte the underlying stream moves, in` |
|       - | 4983 | ` * EITHER direction, and reports the running total each time. Nothing is` |
|       - | 4984 | ` * reported (and nothing counted) before an init has armed the notifier.` |
|       - | 4985 | ` */` |
|     794 | 4986 | `PH7_PRIVATE void PH7_StreamCtxProgressAdd(phl_stream_ctx *pCtxRes,sxi64 nDelta)` |
|     ! 0 | 4987 | `{` |
|     794 | 4988 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->bNotifyDead` |
|     133 | 4989 | `	 \|\| !pCtxRes->bProgress \|\| nDelta <= 0 ){` |
|     752 | 4990 | `		return;` |
|       - | 4991 | `	}` |
|      42 | 4992 | `	pCtxRes->iProgress += nDelta;` |
|      63 | 4993 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_PROGRESS,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      21 | 4994 | `		0,0,0,pCtxRes->iProgress,pCtxRes->iProgressMax);` |
|     397 | 4995 | `}` |
|       - | 4996 | `/*` |
|       - | 4997 | ` * php's php_stream_notify_completed, which is NOT gated on the progress mask:` |
|       - | 4998 | ` * a read that comes back with nothing is the end of the transfer whether or` |
|       - | 4999 | ` * not anything armed the counter, so a context reused for an exchange that` |
|       - | 5000 | ` * ends where a status line was due reports it before the failure.` |
|       - | 5001 | ` */` |
|     172 | 5002 | `PH7_PRIVATE void PH7_StreamCtxCompleted(phl_stream_ctx *pCtxRes)` |
|     ! 0 | 5003 | `{` |
|     172 | 5004 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 ){` |
|     136 | 5005 | `		return;` |
|       - | 5006 | `	}` |
|      54 | 5007 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_COMPLETED,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      18 | 5008 | `		0,0,0,pCtxRes->iProgress,pCtxRes->iProgressMax);` |
|      86 | 5009 | `}` |
|       - | 5010 | `/*` |
|       - | 5011 | ` * resource stream_context_create(?array $options = null, ?array $params = null)` |
|       - | 5012 | ` */` |
|     250 | 5013 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5014 | `{` |
|     254 | 5015 | `	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);` |
|     254 | 5016 | `	if( pRes == 0 ){` |
|     ! 0 | 5017 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5018 | `		return PH7_OK;` |
|       - | 5019 | `	}` |
|     254 | 5020 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     237 | 5021 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|       5 | 5022 | `			return PH7_OK;` |
|       - | 5023 | `		}` |
|     115 | 5024 | `	}` |
|     250 | 5025 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|       - | 5026 | ``		/* php names argument #1 ($options) even for a bad `notification` that`` |
|       - | 5027 | `		 * arrived through $params — the error is raised against a hardcoded` |
|       - | 5028 | `		 * position, and a test that asserts the message would see it. */` |
|      55 | 5029 | `		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){` |
|       5 | 5030 | `			return PH7_OK;` |
|       - | 5031 | `		}` |
|      25 | 5032 | `	}` |
|     246 | 5033 | `	ph7_result_resource(pCtx,pRes);` |
|     246 | 5034 | `	return PH7_OK;` |
|     129 | 5035 | `}` |
|       - | 5036 | `/*` |
|       - | 5037 | ` * array stream_context_get_options(resource $stream_or_context)` |
|       - | 5038 | ` */` |
|      48 | 5039 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 5040 | `{` |
|       - | 5041 | `	phl_stream_ctx *pRes;` |
|       - | 5042 | `	int bThrew;` |
|      51 | 5043 | `	if( nArg < 1 ){` |
|     ! 0 | 5044 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5045 | `		return PH7_OK;` |
|       - | 5046 | `	}` |
|       - | 5047 | `	/* A live stream that was never given a context answers the EMPTY option set` |
|       - | 5048 | `	 * rather than refusing the call, so nothing is created here. */` |
|      51 | 5049 | `	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);` |
|      51 | 5050 | `	if( bThrew ){` |
|       5 | 5051 | `		return PH7_OK;` |
|       - | 5052 | `	}` |
|      47 | 5053 | `	if( pRes == 0 ){` |
|       8 | 5054 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|       8 | 5055 | `		if( pArr == 0 ){` |
|     ! 0 | 5056 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5057 | `			return PH7_OK;` |
|       - | 5058 | `		}` |
|       8 | 5059 | `		ph7_result_value(pCtx,pArr);` |
|       8 | 5060 | `		return PH7_OK;` |
|       - | 5061 | `	}` |
|      41 | 5062 | `	ph7_result_value(pCtx,pRes->pOptions);` |
|      41 | 5063 | `	return PH7_OK;` |
|      27 | 5064 | `}` |
|       - | 5065 | `/*` |
|       - | 5066 | ` * true stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)` |
|       - | 5067 | ` *` |
|       - | 5068 | ` * php also accepts the two-argument (context, options-array) spelling and` |
|       - | 5069 | ` * DEPRECATES it in 8.3 — §10 refuses what php deprecates, so an array in` |
|       - | 5070 | ` * argument #2 is the ordinary string TypeError here and the whole-array form` |
|       - | 5071 | ` * is spelled stream_context_set_options().` |
|       - | 5072 | ` */` |
|      10 | 5073 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5074 | `{` |
|       - | 5075 | `	phl_stream_ctx *pRes;` |
|       - | 5076 | `	int bThrew;` |
|      12 | 5077 | `	if( nArg < 4 ){` |
|     ! 0 | 5078 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5079 | `		return PH7_OK;` |
|       - | 5080 | `	}` |
|      12 | 5081 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      12 | 5082 | `	if( pRes == 0 ){` |
|     ! 0 | 5083 | `		return PH7_OK;` |
|       - | 5084 | `	}` |
|      12 | 5085 | `	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);` |
|      12 | 5086 | `	return PH7_OK;` |
|       7 | 5087 | `}` |
|       - | 5088 | `/*` |
|       - | 5089 | ` * true stream_context_set_options(resource $context, array $options)` |
|       - | 5090 | ` */` |
|       6 | 5091 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5092 | `{` |
|       - | 5093 | `	phl_stream_ctx *pRes;` |
|       - | 5094 | `	int bThrew;` |
|       8 | 5095 | `	if( nArg < 2 ){` |
|     ! 0 | 5096 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5097 | `		return PH7_OK;` |
|       - | 5098 | `	}` |
|       8 | 5099 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|       8 | 5100 | `	if( pRes == 0 ){` |
|     ! 0 | 5101 | `		return PH7_OK;` |
|       - | 5102 | `	}` |
|       8 | 5103 | `	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){` |
|     ! 0 | 5104 | `		return PH7_OK;` |
|       - | 5105 | `	}` |
|       8 | 5106 | `	ph7_result_bool(pCtx,1);` |
|       8 | 5107 | `	return PH7_OK;` |
|       5 | 5108 | `}` |
|       - | 5109 | `/*` |
|       - | 5110 | ` * array stream_context_get_params(resource $stream_or_context)` |
|       - | 5111 | `` *  php answers `notification` (only when one is set) and `options`, in that`` |
|       - | 5112 | ` *  order.` |
|       - | 5113 | ` */` |
|      10 | 5114 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5115 | `{` |
|       - | 5116 | `	phl_stream_ctx *pRes;` |
|       - | 5117 | `	ph7_value *pArr;` |
|       - | 5118 | `	int bThrew;` |
|      12 | 5119 | `	if( nArg < 1 ){` |
|     ! 0 | 5120 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5121 | `		return PH7_OK;` |
|       - | 5122 | `	}` |
|      12 | 5123 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      12 | 5124 | `	if( pRes == 0 ){` |
|       3 | 5125 | `		return PH7_OK;` |
|       - | 5126 | `	}` |
|       9 | 5127 | `	pArr = ph7_context_new_array(pCtx);` |
|       9 | 5128 | `	if( pArr == 0 ){` |
|     ! 0 | 5129 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5130 | `		return PH7_OK;` |
|       - | 5131 | `	}` |
|       9 | 5132 | `	if( pRes->pNotify ){` |
|       5 | 5133 | `		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);` |
|       2 | 5134 | `	}` |
|       9 | 5135 | `	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);` |
|       9 | 5136 | `	ph7_result_value(pCtx,pArr);` |
|       9 | 5137 | `	return PH7_OK;` |
|       7 | 5138 | `}` |
|       - | 5139 | `/*` |
|       - | 5140 | ` * true stream_context_set_params(resource $context, array $params)` |
|       - | 5141 | ` */` |
|       4 | 5142 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5143 | `{` |
|       - | 5144 | `	phl_stream_ctx *pRes;` |
|       - | 5145 | `	int bThrew;` |
|       5 | 5146 | `	if( nArg < 2 ){` |
|     ! 0 | 5147 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5148 | `		return PH7_OK;` |
|       - | 5149 | `	}` |
|       5 | 5150 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|       5 | 5151 | `	if( pRes == 0 ){` |
|     ! 0 | 5152 | `		return PH7_OK;` |
|       - | 5153 | `	}` |
|       5 | 5154 | `	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){` |
|     ! 0 | 5155 | `		return PH7_OK;` |
|       - | 5156 | `	}` |
|       5 | 5157 | `	ph7_result_bool(pCtx,1);` |
|       5 | 5158 | `	return PH7_OK;` |
|       3 | 5159 | `}` |
|       - | 5160 | `/*` |
|       - | 5161 | ` * resource stream_context_get_default(?array $options = null)` |
|       - | 5162 | ` * resource stream_context_set_default(array $options)` |
|       - | 5163 | ` *  Both answer the ONE default context and both MERGE their options into it —` |
|       - | 5164 | ` *  set_default is not a replacement, which is why a second call adds to what` |
|       - | 5165 | ` *  the first left.` |
|       - | 5166 | ` */` |
|      14 | 5167 | `static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5168 | `{` |
|      16 | 5169 | `	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);` |
|      16 | 5170 | `	if( pRes == 0 ){` |
|     ! 0 | 5171 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5172 | `		return PH7_OK;` |
|       - | 5173 | `	}` |
|      16 | 5174 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|       8 | 5175 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|     ! 0 | 5176 | `			return PH7_OK;` |
|       - | 5177 | `		}` |
|       3 | 5178 | `	}` |
|      16 | 5179 | `	ph7_result_resource(pCtx,pRes);` |
|      16 | 5180 | `	return PH7_OK;` |
|       9 | 5181 | `}` |
|      10 | 5182 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5183 | `{` |
|      11 | 5184 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|       1 | 5185 | `}` |
|       4 | 5186 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5187 | `{` |
|       6 | 5188 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|       2 | 5189 | `}` |
|       - | 5190 | `/*` |
|       - | 5191 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|       - | 5192 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|       - | 5193 | ` */` |
|       - | 5194 | `#ifdef PH7_ENABLE_NET` |
|     102 | 5195 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|       4 | 5196 | `{` |
|     106 | 5197 | `	sock_private *pSock = (sock_private *)pHandle;` |
|       - | 5198 | `	int n;` |
|     106 | 5199 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|       - | 5200 | `		/* A server asked for neither BIND nor LISTEN has no socket at all, and` |
|       - | 5201 | `		 * php answers false for a read on it — the shape an ERROR takes. */` |
|       6 | 5202 | `		return -1;` |
|       - | 5203 | `	}` |
|     102 | 5204 | `	if( pSock->bEof ){` |
|     ! 0 | 5205 | `		return 0;` |
|       - | 5206 | `	}` |
|     102 | 5207 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     102 | 5208 | `	if( n == 0 ){` |
|       - | 5209 | `		/* The peer closed: THIS is the end of the stream. */` |
|      23 | 5210 | `		pSock->bEof = 1;` |
|      23 | 5211 | `		return 0;` |
|       - | 5212 | `	}` |
|      80 | 5213 | `	if( n < 0 ){` |
|       - | 5214 | `		/* An error, and since stream_set_blocking()/stream_set_timeout() exist` |
|       - | 5215 | `		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF` |
|       - | 5216 | `		 * here (as this did for every n <= 0, safe only while every socket was` |
|       - | 5217 | `		 * blocking and untimed) made the first empty read close the connection` |
|       - | 5218 | `		 * for good and threw away everything the peer sent afterwards.` |
|       - | 5219 | `		 *` |
|       - | 5220 | `		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a` |
|       - | 5221 | ``		 * Winsock call never touches: without this the `""` a non-blocking read`` |
|       - | 5222 | ``		 * answers and the `timed_out` an expired one reports were both lost on`` |
|       - | 5223 | `		 * Windows, and every such read came back as a plain failure. */` |
|       7 | 5224 | `		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);` |
|       7 | 5225 | `		return -1;` |
|       - | 5226 | `	}` |
|      74 | 5227 | `	return (ph7_int64)n;` |
|      48 | 5228 | `}` |
|     112 | 5229 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|       4 | 5230 | `{` |
|     116 | 5231 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     116 | 5232 | `	const char *zBuf = (const char *)pBuf;` |
|     116 | 5233 | `	ph7_int64 nSent = 0;` |
|     116 | 5234 | `	if( pSock == 0 ){` |
|     ! 0 | 5235 | `		return -1;` |
|       - | 5236 | `	}` |
|     116 | 5237 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|       - | 5238 | `		/* Nothing to send on, and php answers 0 rather than false for it. */` |
|       6 | 5239 | `		return 0;` |
|       - | 5240 | `	}` |
|       - | 5241 | `	/* php answers the number of bytes it MOVED. This used to hand back` |
|       - | 5242 | `	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful` |
|       - | 5243 | ``	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check`` |
|       - | 5244 | `	 * failed on a write that worked, a partial-write retry loop never advanced,` |
|       - | 5245 | `	 * and stream_copy_to_stream() stopped after its first chunk. */` |
|     212 | 5246 | `	while( nSent < nWrite ){` |
|     113 | 5247 | `		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);` |
|     113 | 5248 | `		if( n > 0 ){` |
|     104 | 5249 | `			nSent += n;` |
|     104 | 5250 | `			continue;` |
|       - | 5251 | `		}` |
|       - | 5252 | `		/* Nothing more can go right now. On a non-blocking or timed-out handle` |
|       - | 5253 | `		 * that is php's 0 (or the partial count), and only a write that moved` |
|       - | 5254 | `		 * NO bytes at all for a real error is php's false — which is why the` |
|       - | 5255 | `		 * count is answered here rather than the status. */` |
|      12 | 5256 | `		if( PH7_NetWouldBlock() ){` |
|       4 | 5257 | `			return nSent;` |
|       - | 5258 | `		}` |
|       9 | 5259 | `		pSock->iLastErr = PH7_NetLastError();` |
|       9 | 5260 | `		return nSent > 0 ? nSent : -1;` |
|     ! 0 | 5261 | `	}` |
|     103 | 5262 | `	return nSent;` |
|      58 | 5263 | `}` |
|     197 | 5264 | `static void SockStreamData_Close(void *pHandle)` |
|       4 | 5265 | `{` |
|     201 | 5266 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     201 | 5267 | `	if( pSock == 0 ){` |
|     ! 0 | 5268 | `		return;` |
|       - | 5269 | `	}` |
|     201 | 5270 | `	PH7_NetClose(pSock->sock);` |
|     201 | 5271 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|     103 | 5272 | `}` |
|       - | 5273 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|     ! 0 | 5274 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     ! 0 | 5275 | `{` |
|       - | 5276 | `	sock_private *pSock;` |
|       - | 5277 | `	ph7_socket sock;` |
|       - | 5278 | `	char zHost[256];` |
|       - | 5279 | `	const char *zColon;` |
|     ! 0 | 5280 | `	int iPort = 0,iErrno = 0;` |
|     ! 0 | 5281 | `	const char *zErr = "";` |
|     ! 0 | 5282 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     ! 0 | 5283 | `	SXUNUSED(iMode);` |
|     ! 0 | 5284 | `	if( pVm == 0 ){` |
|     ! 0 | 5285 | `		return -1;` |
|       - | 5286 | `	}` |
|     ! 0 | 5287 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|     ! 0 | 5288 | `	while( zColon > zName && zColon[0] != ':' ){` |
|     ! 0 | 5289 | `		zColon--;` |
|     ! 0 | 5290 | `	}` |
|     ! 0 | 5291 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|     ! 0 | 5292 | `		return -1;` |
|       - | 5293 | `	}` |
|       - | 5294 | `	{` |
|     ! 0 | 5295 | `		sxu32 n = (sxu32)(zColon - zName);` |
|     ! 0 | 5296 | `		if( n >= sizeof(zHost) ){` |
|     ! 0 | 5297 | `			n = sizeof(zHost) - 1;` |
|     ! 0 | 5298 | `		}` |
|     ! 0 | 5299 | `		SyMemcpy(zName,zHost,n);` |
|     ! 0 | 5300 | `		zHost[n] = 0;` |
|       - | 5301 | `	}` |
|       - | 5302 | `	{` |
|     ! 0 | 5303 | `		sxi32 iTmp = 0;` |
|     ! 0 | 5304 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|     ! 0 | 5305 | `		iPort = (int)iTmp;` |
|       - | 5306 | `	}` |
|     ! 0 | 5307 | `	sock = PH7_NetConnect(zHost,iPort,0,0,0,0,&iErrno,&zErr);` |
|     ! 0 | 5308 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 | 5309 | `		return -1;` |
|       - | 5310 | `	}` |
|     ! 0 | 5311 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|     ! 0 | 5312 | `	if( pSock == 0 ){` |
|     ! 0 | 5313 | `		PH7_NetClose(sock);` |
|     ! 0 | 5314 | `		return -1;` |
|       - | 5315 | `	}` |
|     ! 0 | 5316 | `	pSock->pVm = pVm;` |
|     ! 0 | 5317 | `	pSock->sock = sock;` |
|     ! 0 | 5318 | `	pSock->bEof = 0;` |
|     ! 0 | 5319 | `	pSock->iLastErr = 0;` |
|     ! 0 | 5320 | `	pSock->bGeneric = 0;` |
|     ! 0 | 5321 | `	pSock->bDgram = 0;` |
|     ! 0 | 5322 | `	*ppHandle = (void *)pSock;` |
|     ! 0 | 5323 | `	return PH7_OK;` |
|     ! 0 | 5324 | `}` |
|       - | 5325 | `/* php's own listen backlog for a stream server. */` |
|       - | 5326 | `#define SOCK_LISTEN_BACKLOG 128` |
|       - | 5327 | `/*` |
|       - | 5328 | `` * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the`` |
|       - | 5329 | ` * NOTICE its socket ops raise for a send that failed, which is the only` |
|       - | 5330 | ` * diagnostic a write to a departed peer produces (the return value is the same` |
|       - | 5331 | ` * false a closed handle answers). The PLAIN-FILE device has the same notice` |
|       - | 5332 | `` * worded `Write of`, which is what a write to a handle opened read-only`` |
|       - | 5333 | ` * produces: php answers false AND says why, where this engine only answered` |
|       - | 5334 | ` * false. Silent for every other device — nothing else here has an OS error of` |
|       - | 5335 | ` * its own to report, and php's notice lives in those two stream ops alone.` |
|       - | 5336 | ` */` |
|      60 | 5337 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)` |
|       4 | 5338 | `{` |
|      64 | 5339 | `	if( pDev == 0 ){` |
|     ! 0 | 5340 | `		return;` |
|       - | 5341 | `	}` |
|      64 | 5342 | `	if( pDev->bDir \|\| pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|       - | 5343 | `		/* Not a write that failed -- a stream that has no writer, already` |
|       - | 5344 | ``		 * announced as `Stream is not writable`. php has no second sentence`` |
|       - | 5345 | `		 * for it, and this one would name an errno nothing set. */` |
|     ! 0 | 5346 | `		return;` |
|       - | 5347 | `	}` |
|       - | 5348 | `#ifdef PH7_ENABLE_NET` |
|      64 | 5349 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|       9 | 5350 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|       9 | 5351 | `		if( pSock->iLastErr != 0 ){` |
|      12 | 5352 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       - | 5353 | `				"Send of %d bytes failed with errno=%d %s",` |
|       3 | 5354 | `				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));` |
|       9 | 5355 | `			pSock->iLastErr = 0;` |
|       3 | 5356 | `		}` |
|       9 | 5357 | `		return;` |
|       - | 5358 | `	}` |
|       - | 5359 | `#endif` |
|      56 | 5360 | `	if( pDev->pStream == pCtx->pVm->pDefStream ){` |
|      45 | 5361 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      22 | 5362 | `			"Write of %d bytes failed with errno=%d %s",nLen,errno,VfsStrerror(errno));` |
|      11 | 5363 | `	}` |
|      34 | 5364 | `}` |
|       - | 5365 | `/* The settings family below owns both of these; the socket openers here are` |
|       - | 5366 | ` * declared ahead of it so one handle-wrapping routine can serve both halves. */` |
|       - | 5367 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - | 5368 | `	const char *zName,int *pRc);` |
|       - | 5369 | `static ph7_socket * IoPrivateSocket(io_private *pDev);` |
|       - | 5370 | `/*` |
|       - | 5371 | ` * Wrap an open socket in the io_private every f* builtin drives, so a socket a` |
|       - | 5372 | ` * server accepted reads and writes exactly like one a client connected. A NULL` |
|       - | 5373 | ` * zUri is php's "opened by no name at all" — an accepted connection, which` |
|       - | 5374 | `` * reports no `uri` at all from stream_get_meta_data(). `bDgram` is carried`` |
|       - | 5375 | ` * because php's stream ops are chosen per TRANSPORT and a script can see which` |
|       - | 5376 | `` * pair a handle got: a datagram socket reports `udp_socket`.`` |
|       - | 5377 | ` * Answers 0 (and closes the socket) when there is no memory for the handle.` |
|       - | 5378 | ` */` |
|     231 | 5379 | `static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|       - | 5380 | `	const char *zUri,int nUri)` |
|       4 | 5381 | `{` |
|       - | 5382 | `	io_private *pDev;` |
|       - | 5383 | `	sock_private *pSock;` |
|     235 | 5384 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     235 | 5385 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|     235 | 5386 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|     ! 0 | 5387 | `		if( pSock ){` |
|     ! 0 | 5388 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);` |
|     ! 0 | 5389 | `		}` |
|     ! 0 | 5390 | `		if( pDev ){` |
|       - | 5391 | `			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim` |
|       - | 5392 | `			 * this chunk — it is not an io_private yet and has no buffers. */` |
|     ! 0 | 5393 | `			ph7_context_free_chunk(pCtx,pDev);` |
|     ! 0 | 5394 | `		}` |
|     ! 0 | 5395 | `		PH7_NetClose(sock);` |
|     ! 0 | 5396 | `		return 0;` |
|       - | 5397 | `	}` |
|     235 | 5398 | `	pSock->pVm = pCtx->pVm;` |
|     235 | 5399 | `	pSock->sock = sock;` |
|     235 | 5400 | `	pSock->bEof = 0;` |
|     235 | 5401 | `	pSock->iLastErr = 0;` |
|     235 | 5402 | `	pSock->bGeneric = 0;` |
|     235 | 5403 | `	pSock->bDgram = bDgram;` |
|       - | 5404 | `	/* Derived from the two flags above unless ext/sockets states one. */` |
|     235 | 5405 | `	pSock->zLabel = 0;` |
|     235 | 5406 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|       - | 5407 | `	/* php's feof() answers TRUE for a stream whose socket was never created. */` |
|     235 | 5408 | `	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);` |
|     235 | 5409 | `	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);` |
|     235 | 5410 | `	pDev->pHandle = (void *)pSock;` |
|     235 | 5411 | `	return pDev;` |
|     120 | 5412 | `}` |
|       - | 5413 | `/*` |
|       - | 5414 | ` * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own` |
|       - | 5415 | ` * close hook frees the socket handle, and the io_private chunk goes with it.` |
|       - | 5416 | ` * Nothing has handed this out as a resource yet, so there is no ph7_value that` |
|       - | 5417 | ` * could observe it afterwards.` |
|       - | 5418 | ` */` |
|     ! 0 | 5419 | `static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)` |
|     ! 0 | 5420 | `{` |
|     ! 0 | 5421 | `	if( pDev == 0 ){` |
|     ! 0 | 5422 | `		return;` |
|       - | 5423 | `	}` |
|     ! 0 | 5424 | `	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){` |
|     ! 0 | 5425 | `		pDev->pStream->xClose(pDev->pHandle);` |
|     ! 0 | 5426 | `		pDev->pHandle = 0;` |
|     ! 0 | 5427 | `	}` |
|     ! 0 | 5428 | `	ReleaseIOPrivate(pCtx,pDev);` |
|     ! 0 | 5429 | `}` |
|       - | 5430 | `/* Forward: php's port rule, defined with the address parser further down. */` |
|       - | 5431 | `static int SockParsePort(const char *z,int n);` |
|       - | 5432 | `/*` |
|       - | 5433 | `` * php's `socket` context options, read into the shape net.c applies. Only the`` |
|       - | 5434 | `` * ones this transport can honour are read: `bindto`, which is the LOCAL`` |
|       - | 5435 | ``  * address a client connects out from, `backlog`, `so_reuseport`, `tcp_nodelay` `` |
|       - | 5436 | `` * `so_broadcast` -- which had no consumer until udp:// existed, because it is`` |
|       - | 5437 | ` * the permission a DATAGRAM socket needs before the OS will let it address a` |
|       - | 5438 | `` * broadcast address at all -- and `ipv6_v6only`, which had none either while the`` |
|       - | 5439 | ` * socket layer could only open AF_INET.` |
|       - | 5440 | ` *` |
|       - | 5441 | `` * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the`` |
|       - | 5442 | ` * same address rule the server half already uses — and a spelling with no colon` |
|       - | 5443 | ` * at all is not an address, so php performs no bind and says nothing. A value` |
|       - | 5444 | ` * that is not a STRING is php's one hard failure here; everything else is a` |
|       - | 5445 | ` * warning and a connection made from wherever routing would have sent it.` |
|       - | 5446 | ` * Returns 0, or -1 with *pzErr set to php's refusal.` |
|       - | 5447 | ` */` |
|     284 | 5448 | `static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,` |
|       - | 5449 | `	const char **pzErr)` |
|       4 | 5450 | `{` |
|       - | 5451 | `	ph7_value *pVal;` |
|     288 | 5452 | `	SyZero(pOut,sizeof(*pOut));` |
|     288 | 5453 | `	if( pCtxRes == 0 ){` |
|     177 | 5454 | `		return 0;` |
|       - | 5455 | `	}` |
|     112 | 5456 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");` |
|     112 | 5457 | `	if( pVal ){` |
|       5 | 5458 | `		pOut->iBacklog = (int)ph7_value_to_int64(pVal);` |
|       2 | 5459 | `	}` |
|     112 | 5460 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");` |
|     112 | 5461 | `	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);` |
|     112 | 5462 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");` |
|     112 | 5463 | `	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);` |
|     112 | 5464 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_broadcast");` |
|     112 | 5465 | `	pOut->bBroadcast = pVal != 0 && ph7_value_to_bool(pVal);` |
|     112 | 5466 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","ipv6_v6only");` |
|     112 | 5467 | `	pOut->bV6Only = pVal != 0 && ph7_value_to_bool(pVal);` |
|     112 | 5468 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");` |
|     112 | 5469 | `	if( pVal ){` |
|       - | 5470 | `		const char *zSpec;` |
|      15 | 5471 | `		int nSpec = 0,i,nHost = -1;` |
|      15 | 5472 | `		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 5473 | `			*pzErr = "local_addr context option is not a string.";` |
|       3 | 5474 | `			return -1;` |
|       - | 5475 | `		}` |
|      13 | 5476 | `		zSpec = (const char *)SyBlobData(&pVal->sBlob);` |
|      13 | 5477 | `		nSpec = (int)SyBlobLength(&pVal->sBlob);` |
|       - | 5478 | `		/* NOT the bracketed grammar the openers read, deliberately: php's` |
|       - | 5479 | ``		 * `bindto` is applied per RESOLVER CANDIDATE and what it does with one`` |
|       - | 5480 | `		 * it cannot use in that candidate's family is three different things` |
|       - | 5481 | `		 * (skip in silence, warn and connect anyway, abandon the candidate),` |
|       - | 5482 | `		 * split by rules that are not the ones the wordings suggest -- so` |
|       - | 5483 | ``		 * parsing `[::1]:0` here without them would make PHL warn where php`` |
|       - | 5484 | `		 * says nothing. Measured and recorded in §7.4 slice-1 (b)(iii). */` |
|     137 | 5485 | `		for( i = 0 ; i + 1 < nSpec ; i++ ){` |
|     135 | 5486 | `			if( zSpec[i] == ':' ){` |
|      11 | 5487 | `				nHost = i;` |
|      11 | 5488 | `				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);` |
|      11 | 5489 | `				break;` |
|       - | 5490 | `			}` |
|      63 | 5491 | `		}` |
|      13 | 5492 | `		if( nHost >= 0 ){` |
|      11 | 5493 | `			if( nHost >= nHostBuf ){` |
|     ! 0 | 5494 | `				nHost = nHostBuf - 1;` |
|     ! 0 | 5495 | `			}` |
|      11 | 5496 | `			if( nHost > 0 ){` |
|      11 | 5497 | `				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);` |
|       5 | 5498 | `			}` |
|      11 | 5499 | `			zHostBuf[nHost] = 0;` |
|      11 | 5500 | `			pOut->zBindHost = zHostBuf;` |
|       5 | 5501 | `		}` |
|       6 | 5502 | `	}` |
|     110 | 5503 | `	return 0;` |
|     146 | 5504 | `}` |
|       - | 5505 | `/*` |
|       - | 5506 | ` * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask` |
|       - | 5507 | ` * for: a second open of the SAME address hands back the very same resource` |
|       - | 5508 | `` * rather than a second connection — `$a === $b` — and fclose() is what ends it,`` |
|       - | 5509 | ` * after which the next open dials again. The key is the address as the opener` |
|       - | 5510 | ` * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.` |
|       - | 5511 | ` */` |
|      36 | 5512 | `static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)` |
|       2 | 5513 | `{` |
|       - | 5514 | `	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a` |
|       - | 5515 | `	 * persistent stream_socket_client() of one address are two connections. */` |
|      56 | 5516 | `	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",` |
|      18 | 5517 | `		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);` |
|      38 | 5518 | `}` |
|      22 | 5519 | `static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)` |
|       2 | 5520 | `{` |
|      24 | 5521 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - | 5522 | `	sxu32 i;` |
|      44 | 5523 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|      32 | 5524 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|      14 | 5525 | `			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      12 | 5526 | `				return aSlot[i].pDev;` |
|       - | 5527 | `			}` |
|       - | 5528 | `			/* fclose()'d since: the slot is free for the next connection. */` |
|       3 | 5529 | `			aSlot[i].zKey[0] = 0;` |
|       3 | 5530 | `			aSlot[i].pDev = 0;` |
|       1 | 5531 | `		}` |
|      11 | 5532 | `	}` |
|      14 | 5533 | `	return 0;` |
|      13 | 5534 | `}` |
|       - | 5535 | `/*` |
|       - | 5536 | ` * Forget a kept connection whose socket is gone. The io_private itself stays` |
|       - | 5537 | ` * alive and stamped closed (a script may still hold the resource), so this only` |
|       - | 5538 | ` * frees the SLOT for the fresh connection about to take its place.` |
|       - | 5539 | ` */` |
|       2 | 5540 | `static void SockPersistDrop(ph7_vm *pVm,const char *zKey)` |
|       1 | 5541 | `{` |
|       3 | 5542 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - | 5543 | `	sxu32 i;` |
|       3 | 5544 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|       3 | 5545 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|       3 | 5546 | `			aSlot[i].zKey[0] = 0;` |
|       3 | 5547 | `			aSlot[i].pDev = 0;` |
|       3 | 5548 | `			return;` |
|       - | 5549 | `		}` |
|     ! 0 | 5550 | `	}` |
|       2 | 5551 | `}` |
|      14 | 5552 | `static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)` |
|       2 | 5553 | `{` |
|      16 | 5554 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - | 5555 | `	VmPersistSock sSlot;` |
|       - | 5556 | `	sxu32 i;` |
|      28 | 5557 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|      18 | 5558 | `		if( aSlot[i].zKey[0] == 0 \|\| IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|       6 | 5559 | `			SyZero(&aSlot[i],sizeof(VmPersistSock));` |
|       6 | 5560 | `			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);` |
|       6 | 5561 | `			aSlot[i].pDev = pDev;` |
|       6 | 5562 | `			return;` |
|       - | 5563 | `		}` |
|       7 | 5564 | `	}` |
|      12 | 5565 | `	SyZero(&sSlot,sizeof(sSlot));` |
|      12 | 5566 | `	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);` |
|      12 | 5567 | `	sSlot.pDev = pDev;` |
|      12 | 5568 | `	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);` |
|       9 | 5569 | `}` |
|       - | 5570 | `/*` |
|       - | 5571 | `` * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the`` |
|       - | 5572 | ` * moment it is opened — a read from a peer that has gone quiet answers FALSE` |
|       - | 5573 | `` * after it, with `timed_out` set — where this engine armed nothing and waited`` |
|       - | 5574 | ` * forever. That is the difference between a program that reports a dead peer and` |
|       - | 5575 | ` * one that hangs.` |
|       - | 5576 | ` *` |
|       - | 5577 | ` * A LISTENING socket is deliberately left alone: php's accept timeout is its own` |
|       - | 5578 | ` * argument and its own select(), so arming the OS receive timeout here would` |
|       - | 5579 | `` * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be`` |
|       - | 5580 | ` * unbounded — at sixty seconds.` |
|       - | 5581 | ` */` |
|     171 | 5582 | `static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)` |
|       4 | 5583 | `{` |
|       - | 5584 | `	ph7_int64 iSec;` |
|     175 | 5585 | `	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;` |
|     175 | 5586 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 | 5587 | `		return;` |
|       - | 5588 | `	}` |
|     175 | 5589 | `	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);` |
|     175 | 5590 | `	if( iSec > 0 ){` |
|     175 | 5591 | `		PH7_NetSetRwTimeout(*pSock,iSec,0);` |
|     175 | 5592 | `		pDev->bHasTimeout = 1;` |
|      86 | 5593 | `	}` |
|      90 | 5594 | `}` |
|       - | 5595 | `/*` |
|       - | 5596 | ` * The out-params every address-taking opener carries, on the path that WORKED:` |
|       - | 5597 | ` * php writes 0 and "" into them rather than leaving whatever the caller's` |
|       - | 5598 | ` * variables already held.` |
|       - | 5599 | ` */` |
|     189 | 5600 | `static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)` |
|       4 | 5601 | `{` |
|     193 | 5602 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|     193 | 5603 | `	if( pTmp == 0 ){` |
|     ! 0 | 5604 | `		return;` |
|       - | 5605 | `	}` |
|     193 | 5606 | `	if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     167 | 5607 | `		ph7_value_int(pTmp,0);` |
|     167 | 5608 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      82 | 5609 | `	}` |
|     193 | 5610 | `	if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     167 | 5611 | `		ph7_value_string(pTmp,"",0);` |
|     167 | 5612 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      82 | 5613 | `	}` |
|      99 | 5614 | `}` |
|       - | 5615 | `/*` |
|       - | 5616 | ` * The failure shape the whole address-taking family shares: php words the` |
|       - | 5617 | ` * reason into BOTH the by-ref out-params and a warning naming the address as` |
|       - | 5618 | `` * the script wrote it. The `$errno` out-param stays 0 for everything the`` |
|       - | 5619 | ` * ADDRESS itself is refused for — php only ever reports an OS code for a` |
|       - | 5620 | ` * connect() that reached the network.` |
|       - | 5621 | ` */` |
|     141 | 5622 | `static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,` |
|       - | 5623 | `	const char *zAddr,int nAddr,const char *zErr,int iErrno)` |
|       4 | 5624 | `{` |
|       - | 5625 | `	ph7_value *pTmp;` |
|       - | 5626 | `	/* php's two halves do not agree about a failure that logged NO text: the` |
|       - | 5627 | `	 * out-param keeps the empty string php pre-assigned it, and the warning` |
|       - | 5628 | ``	 * says `Unknown error` in its place. A datagram address given the default`` |
|       - | 5629 | `	 * $flags is the one arm that reaches here (php's udp ops refuse LISTEN` |
|       - | 5630 | `	 * silently), so the distinction is user-visible rather than theoretical. */` |
|     145 | 5631 | `	const char *zWarn = zErr ? zErr : "Unknown error";` |
|     145 | 5632 | `	if( zErr == 0 ){` |
|       3 | 5633 | `		zErr = "";` |
|       1 | 5634 | `	}` |
|     145 | 5635 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|     145 | 5636 | `	if( pTmp ){` |
|     145 | 5637 | `		if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     145 | 5638 | `			ph7_value_int(pTmp,iErrno);` |
|     145 | 5639 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      70 | 5640 | `		}` |
|     145 | 5641 | `		if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     145 | 5642 | `			ph7_value_string(pTmp,zErr,-1);` |
|     145 | 5643 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      70 | 5644 | `		}` |
|      70 | 5645 | `	}` |
|       - | 5646 | `	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|     215 | 5647 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",` |
|      70 | 5648 | `		nAddr,zAddr,zWarn);` |
|     145 | 5649 | `}` |
|       - | 5650 | `/*` |
|       - | 5651 | ` * The one failure whose message names the HOST, and the one php reports TWICE:` |
|       - | 5652 | ` * its transport raises the text on its own before the opener that asked repeats` |
|       - | 5653 | ` * it inside "Unable to connect to". Composed here because net.c hands back a` |
|       - | 5654 | ` * static string and only the caller has the name to word in.` |
|       - | 5655 | ` */` |
|       4 | 5656 | `static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)` |
|       1 | 5657 | `{` |
|       7 | 5658 | `	SyBufferFormat(zBuf,(sxu32)nBuf,` |
|       2 | 5659 | `		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);` |
|       5 | 5660 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);` |
|       5 | 5661 | `	return zBuf;` |
|       1 | 5662 | `}` |
|       - | 5663 | `/* int (*xStat)(void *,ph7_value *,ph7_value *)` |
|       - | 5664 | ` *` |
|       - | 5665 | ` * php's socket ops stat the DESCRIPTOR, so fstat() on a socket answers whatever` |
|       - | 5666 | ` * the platform's fstat() says about one -- a full record on both boxes here,` |
|       - | 5667 | ` * with the S_IFSOCK mode. The same call, so the same platform answer, failure` |
|       - | 5668 | ` * included. */` |
|     ! 0 | 5669 | `static int SockStreamData_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|     ! 0 | 5670 | `{` |
|     ! 0 | 5671 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     ! 0 | 5672 | `	if( pSock == 0 ){` |
|     ! 0 | 5673 | `		return -1;` |
|       - | 5674 | `	}` |
|       - | 5675 | `#ifdef __WINNT__` |
|       - | 5676 | `	/* php's socket ops do not stat at all on Windows: they SUCCEED with a` |
|       - | 5677 | `	 * zeroed record, so fstat() there is an array of zeros with the -1` |
|       - | 5678 | `	 * blksize/blocks every Windows stat carries. A SOCKET is not a CRT` |
|       - | 5679 | `	 * descriptor either, so asking one would be a call on a descriptor number` |
|       - | 5680 | `	 * nothing opened. (Read back from php 8.5.8 on the gate guest.) */` |
|       - | 5681 | `	{` |
|       - | 5682 | `		ph7_int64 aVal[13];` |
|       - | 5683 | `		int i;` |
|     ! 0 | 5684 | `		for( i = 0 ; i < 11 ; ++i ){ aVal[i] = 0; }` |
|     ! 0 | 5685 | `		aVal[11] = -1;` |
|     ! 0 | 5686 | `		aVal[12] = -1;` |
|       - | 5687 | `		SXUNUSED(pSock);` |
|     ! 0 | 5688 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|       - | 5689 | `	}` |
|       - | 5690 | `#else` |
|     ! 0 | 5691 | `	return PH7_VfsStatFromFd((int)pSock->sock,pArray,pWorker);` |
|       - | 5692 | `#endif` |
|     ! 0 | 5693 | `}` |
|       - | 5694 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|       - | 5695 | `	"tcp",` |
|       - | 5696 | `	PH7_IO_STREAM_VERSION,` |
|       - | 5697 | `	SockStreamData_Open, /* xOpen */` |
|       - | 5698 | `	0,   /* xOpenDir */` |
|       - | 5699 | `	SockStreamData_Close,/* xClose */` |
|       - | 5700 | `	0,  /* xCloseDir */` |
|       - | 5701 | `	SockStreamData_Read, /* xRead */` |
|       - | 5702 | `	0,  /* xReadDir */` |
|       - | 5703 | `	SockStreamData_Write,/* xWrite */` |
|       - | 5704 | `	0,  /* xSeek (sockets are not seekable) */` |
|       - | 5705 | `	0,  /* xLock */` |
|       - | 5706 | `	0,  /* xRewindDir */` |
|       - | 5707 | `	0,  /* xTell: none, so the stream layer's own counter answers ftell() */` |
|       - | 5708 | ``	0,  /* xTrunc: none at all, which is php's `Can't truncate this stream!` */`` |
|       - | 5709 | `	0,  /* xSync */` |
|       - | 5710 | `	SockStreamData_Stat  /* xStat */` |
|       - | 5711 | `};` |
|       - | 5712 | `#endif /* PH7_ENABLE_NET */` |
|       - | 5713 | `/*` |
|       - | 5714 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|       - | 5715 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|       - | 5716 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|       - | 5717 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|       - | 5718 | ` * The handle carries the userland object, and every stream op dispatches the` |
|       - | 5719 | ` * php streamWrapper protocol method on it.` |
|       - | 5720 | ` */` |
|       - | 5721 | `#define PHL_UWRAP_MAX 8` |
|       - | 5722 | `typedef struct uwrap_slot uwrap_slot;` |
|       - | 5723 | `struct uwrap_slot` |
|       - | 5724 | `{` |
|       - | 5725 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|       - | 5726 | `	char zScheme[32];         /* protocol name */` |
|       - | 5727 | `	char zClass[128];         /* userland wrapper class */` |
|       - | 5728 | `	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated` |
|       - | 5729 | `	                           * by allow_url_fopen, INCLUDING it by` |
|       - | 5730 | `	                           * allow_url_include */` |
|       - | 5731 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|       - | 5732 | `};` |
|       - | 5733 | `typedef struct uwrap_handle uwrap_handle;` |
|       - | 5734 | `struct uwrap_handle` |
|       - | 5735 | `{` |
|       - | 5736 | `	ph7_vm *pVm;` |
|       - | 5737 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|       - | 5738 | `	int iSlot;` |
|       - | 5739 | `	int bEof;` |
|       - | 5740 | `};` |
|       - | 5741 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|       - | 5742 | `/*` |
|       - | 5743 | ` * Was this device registered with STREAM_IS_URL? Only a userland wrapper can` |
|       - | 5744 | ` * carry the flag, so the answer is a scan of the registration slots.` |
|       - | 5745 | ` */` |
|   44046 | 5746 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)` |
|       5 | 5747 | `{` |
|       - | 5748 | `	int i;` |
|       - | 5749 | `	/* php marks its own data:// wrapper a URL, and that is the one that matters` |
|       - | 5750 | ``	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI`` |
|       - | 5751 | `	 * itself, which is why php refuses it unless allow_url_include says` |
|       - | 5752 | `	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */` |
|   44046 | 5753 | `	if( pStream && pStream->zName` |
|   44051 | 5754 | `	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){` |
|      71 | 5755 | `		return 1;` |
|       - | 5756 | `	}` |
|       - | 5757 | `#ifdef PH7_ENABLE_NET` |
|       - | 5758 | `	/* http:// is php's STREAM_IS_URL wrapper proper: allow_url_fopen switches it` |
|       - | 5759 | `	 * off wholesale, and allow_url_include -- off by default -- is what stops an` |
|       - | 5760 | ``	 * `include 'http://…'` from executing whatever answered. */`` |
|   43985 | 5761 | `	if( PH7_HttpStreamIs(pStream) ){` |
|     310 | 5762 | `		return 1;` |
|       - | 5763 | `	}` |
|       - | 5764 | `#endif` |
|  392231 | 5765 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  348663 | 5766 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     108 | 5767 | `			return g_aUwrap[i].bIsUrl;` |
|       - | 5768 | `		}` |
|  173970 | 5769 | `	}` |
|   43573 | 5770 | `	return 0;` |
|   21989 | 5771 | `}` |
|       - | 5772 | `/*` |
|       - | 5773 | ` * Is this device one of the userland wrapper slots? php labels every such` |
|       - | 5774 | `` * stream `user-space` rather than by its protocol.`` |
|       - | 5775 | ` */` |
|   10475 | 5776 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream)` |
|       5 | 5777 | `{` |
|       - | 5778 | `	int i;` |
|   94072 | 5779 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|   83623 | 5780 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|      29 | 5781 | `			return 1;` |
|       - | 5782 | `		}` |
|   41725 | 5783 | `	}` |
|   10454 | 5784 | `	return 0;` |
|    5233 | 5785 | `}` |
|       - | 5786 | `/*` |
|       - | 5787 | ` * Is this device one of the registration slots at all? Unlike IoPrivateIsUwrap()` |
|       - | 5788 | ` * this does NOT ask whether the slot is still live -- restore() has to tell a` |
|       - | 5789 | ` * withdrawn userland wrapper from a built-in, and a withdrawn slot has already` |
|       - | 5790 | ` * had its pVm cleared.` |
|       - | 5791 | ` */` |
|      18 | 5792 | `static int UwrapIsSlotDevice(const ph7_io_stream *pStream)` |
|       1 | 5793 | `{` |
|       - | 5794 | `	int i;` |
|      99 | 5795 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      89 | 5796 | `		if( &g_aUwrap[i].sStream == pStream ){` |
|       9 | 5797 | `			return 1;` |
|       - | 5798 | `		}` |
|      41 | 5799 | `	}` |
|      11 | 5800 | `	return 0;` |
|      10 | 5801 | `}` |
|       - | 5802 | `/* Forward: the protocol dispatcher is defined just below. */` |
|       - | 5803 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|       - | 5804 | `	ph7_value *pResult);` |
|       - | 5805 | `/*` |
|       - | 5806 | ` * Ask a userland wrapper whether it is at end of file — php's own` |
|       - | 5807 | ` * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring` |
|       - | 5808 | ` * the answer from a zero-length read instead. Returns 0 when the handle is not` |
|       - | 5809 | ` * a userland stream (nothing written to *pAnswer).` |
|       - | 5810 | ` */` |
|   10042 | 5811 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)` |
|       5 | 5812 | `{` |
|       - | 5813 | `	uwrap_handle *pH;` |
|       - | 5814 | `	ph7_value sRet;` |
|   10047 | 5815 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|   10041 | 5816 | `		return 0;` |
|       - | 5817 | `	}` |
|       8 | 5818 | `	pH = (uwrap_handle *)pDev->pHandle;` |
|       8 | 5819 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       8 | 5820 | `	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){` |
|       - | 5821 | `		/* php's streamWrapper requires the method; a class without one keeps` |
|       - | 5822 | `		 * the read-derived answer rather than being called into. */` |
|     ! 0 | 5823 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 | 5824 | `		*pAnswer = pH->bEof;` |
|     ! 0 | 5825 | `		return 1;` |
|       - | 5826 | `	}` |
|       8 | 5827 | `	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;` |
|       8 | 5828 | `	PH7_MemObjRelease(&sRet);` |
|       8 | 5829 | `	return 1;` |
|    5022 | 5830 | `}` |
|       - | 5831 | `/*` |
|       - | 5832 | `` * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),`` |
|       - | 5833 | ` * or 0 for any other device.` |
|       - | 5834 | ` */` |
|     132 | 5835 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)` |
|       5 | 5836 | `{` |
|     137 | 5837 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|     133 | 5838 | `		return 0;` |
|       - | 5839 | `	}` |
|       5 | 5840 | `	return ((uwrap_handle *)pDev->pHandle)->pObj;` |
|      71 | 5841 | `}` |
|       - | 5842 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     320 | 5843 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|       - | 5844 | `	ph7_value *pResult)` |
|       4 | 5845 | `{` |
|       - | 5846 | `	ph7_class_method *pMeth;` |
|     324 | 5847 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|     ! 0 | 5848 | `		return -1;` |
|       - | 5849 | `	}` |
|     324 | 5850 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     324 | 5851 | `	if( pMeth == 0 ){` |
|      33 | 5852 | `		return -1;` |
|       - | 5853 | `	}` |
|     294 | 5854 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|     ! 0 | 5855 | `		return -1;` |
|       - | 5856 | `	}` |
|     294 | 5857 | `	return 0;` |
|     164 | 5858 | `}` |
|      70 | 5859 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|       3 | 5860 | `{` |
|      73 | 5861 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - | 5862 | `	ph7_value sArg,sRet;` |
|       - | 5863 | `	const char *zData;` |
|      73 | 5864 | `	int nData = 0;` |
|      73 | 5865 | `	ph7_int64 n = 0;` |
|      73 | 5866 | `	if( pH == 0 \|\| pH->bEof ){` |
|     ! 0 | 5867 | `		return 0;` |
|       - | 5868 | `	}` |
|      73 | 5869 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      73 | 5870 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      73 | 5871 | `	ph7_value_int64(&sArg,nRead);` |
|       - | 5872 | `	{` |
|       - | 5873 | `		ph7_value *apArg[1];` |
|      73 | 5874 | `		apArg[0] = &sArg;` |
|      73 | 5875 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|     ! 0 | 5876 | `			PH7_MemObjRelease(&sArg);` |
|     ! 0 | 5877 | `			PH7_MemObjRelease(&sRet);` |
|     ! 0 | 5878 | `			return -1;` |
|       - | 5879 | `		}` |
|       - | 5880 | `	}` |
|      73 | 5881 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      73 | 5882 | `	if( nData > 0 ){` |
|      37 | 5883 | `		if( (ph7_int64)nData > nRead ){` |
|     ! 0 | 5884 | `			nData = (int)nRead;` |
|     ! 0 | 5885 | `		}` |
|      37 | 5886 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      37 | 5887 | `		n = nData;` |
|      20 | 5888 | `	}else{` |
|      39 | 5889 | `		pH->bEof = 1;` |
|       - | 5890 | `	}` |
|      73 | 5891 | `	PH7_MemObjRelease(&sArg);` |
|      73 | 5892 | `	PH7_MemObjRelease(&sRet);` |
|      73 | 5893 | `	return n;` |
|      38 | 5894 | `}` |
|       4 | 5895 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|       2 | 5896 | `{` |
|       6 | 5897 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - | 5898 | `	ph7_value sArg,sRet;` |
|       - | 5899 | `	ph7_int64 n;` |
|       6 | 5900 | `	if( pH == 0 ){` |
|     ! 0 | 5901 | `		return -1;` |
|       - | 5902 | `	}` |
|       6 | 5903 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|       6 | 5904 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       6 | 5905 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|       - | 5906 | `	{` |
|       - | 5907 | `		ph7_value *apArg[1];` |
|       6 | 5908 | `		apArg[0] = &sArg;` |
|       6 | 5909 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|     ! 0 | 5910 | `			PH7_MemObjRelease(&sArg);` |
|     ! 0 | 5911 | `			PH7_MemObjRelease(&sRet);` |
|     ! 0 | 5912 | `			return -1;` |
|       - | 5913 | `		}` |
|       - | 5914 | `	}` |
|       6 | 5915 | `	n = ph7_value_to_int64(&sRet);` |
|       6 | 5916 | `	PH7_MemObjRelease(&sArg);` |
|       6 | 5917 | `	PH7_MemObjRelease(&sRet);` |
|       6 | 5918 | `	return n;` |
|       4 | 5919 | `}` |
|       2 | 5920 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|       1 | 5921 | `{` |
|       3 | 5922 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - | 5923 | `	ph7_value sOfft,sWhence,sRet;` |
|       - | 5924 | `	ph7_value *apArg[2];` |
|       - | 5925 | `	int rc;` |
|       3 | 5926 | `	if( pH == 0 ){` |
|     ! 0 | 5927 | `		return -1;` |
|       - | 5928 | `	}` |
|       3 | 5929 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|       3 | 5930 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|       3 | 5931 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       3 | 5932 | `	ph7_value_int64(&sOfft,iOfft);` |
|       3 | 5933 | `	ph7_value_int(&sWhence,whence);` |
|       3 | 5934 | `	apArg[0] = &sOfft;` |
|       3 | 5935 | `	apArg[1] = &sWhence;` |
|       3 | 5936 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|       3 | 5937 | `	if( rc == 0 ){` |
|       3 | 5938 | `		pH->bEof = 0;` |
|       3 | 5939 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|       1 | 5940 | `	}` |
|       3 | 5941 | `	PH7_MemObjRelease(&sOfft);` |
|       3 | 5942 | `	PH7_MemObjRelease(&sWhence);` |
|       3 | 5943 | `	PH7_MemObjRelease(&sRet);` |
|       3 | 5944 | `	return rc;` |
|       2 | 5945 | `}` |
|       6 | 5946 | `static ph7_int64 UwrapTell(void *pHandle)` |
|       1 | 5947 | `{` |
|       7 | 5948 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - | 5949 | `	ph7_value sRet;` |
|       - | 5950 | `	ph7_int64 n;` |
|       7 | 5951 | `	if( pH == 0 ){` |
|     ! 0 | 5952 | `		return -1;` |
|       - | 5953 | `	}` |
|       7 | 5954 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       7 | 5955 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|       3 | 5956 | `		PH7_MemObjRelease(&sRet);` |
|       3 | 5957 | `		return -1;` |
|       - | 5958 | `	}` |
|       5 | 5959 | `	n = ph7_value_to_int64(&sRet);` |
|       5 | 5960 | `	PH7_MemObjRelease(&sRet);` |
|       5 | 5961 | `	return n;` |
|       4 | 5962 | `}` |
|      56 | 5963 | `static void UwrapClose(void *pHandle)` |
|       3 | 5964 | `{` |
|      59 | 5965 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      59 | 5966 | `	if( pH == 0 ){` |
|     ! 0 | 5967 | `		return;` |
|       - | 5968 | `	}` |
|      59 | 5969 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      59 | 5970 | `	if( pH->pObj ){` |
|      59 | 5971 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      28 | 5972 | `	}` |
|      59 | 5973 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      31 | 5974 | `}` |
|       - | 5975 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|     100 | 5976 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|       4 | 5977 | `{` |
|     104 | 5978 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|     104 | 5979 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|       - | 5980 | `	ph7_class *pClass;` |
|       - | 5981 | `	uwrap_handle *pH;` |
|       - | 5982 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|       - | 5983 | `	ph7_value *apArg[4];` |
|       - | 5984 | `	int rc;` |
|     104 | 5985 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|     ! 0 | 5986 | `		return -1;` |
|       - | 5987 | `	}` |
|     104 | 5988 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|     104 | 5989 | `	if( pClass == 0 ){` |
|     ! 0 | 5990 | `		return -1;` |
|       - | 5991 | `	}` |
|     104 | 5992 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|     104 | 5993 | `	if( pH == 0 ){` |
|     ! 0 | 5994 | `		return -1;` |
|       - | 5995 | `	}` |
|     104 | 5996 | `	pH->pVm = pVm;` |
|     104 | 5997 | `	pH->iSlot = iSlot;` |
|     104 | 5998 | `	pH->bEof = 0;` |
|     104 | 5999 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|     104 | 6000 | `	if( pH->pObj == 0 ){` |
|     ! 0 | 6001 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 | 6002 | `		return -1;` |
|       - | 6003 | `	}` |
|       - | 6004 | `	{` |
|       - | 6005 | `		/* php's streamWrapper::$context, set on the serving instance BEFORE` |
|       - | 6006 | `		 * stream_open() runs — which is the whole reason a userland wrapper can` |
|       - | 6007 | `		 * be configured per open. It is exactly what the OPENER resolved: the` |
|       - | 6008 | ``		 * default context substitutes for a NULL `$context` argument, so an`` |
|       - | 6009 | `		 * ordinary fopen() hands a resource over; but an opener with no such` |
|       - | 6010 | `		 * argument at all (md5_file(), include) and one that carried` |
|       - | 6011 | `		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default` |
|       - | 6012 | `		 * here would make that flag mean nothing.` |
|       - | 6013 | `		 * The class need not declare the slot; php adds it either way. */` |
|     104 | 6014 | `		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     104 | 6015 | `		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");` |
|     104 | 6016 | `		if( pCtxSlot == 0 ){` |
|     ! 0 | 6017 | `			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);` |
|     ! 0 | 6018 | `		}` |
|     104 | 6019 | `		if( pCtxSlot ){` |
|     104 | 6020 | `			if( pOpenCtx ){` |
|      86 | 6021 | `				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);` |
|      45 | 6022 | `			}else{` |
|      20 | 6023 | `				ph7_value_null(pCtxSlot);` |
|       - | 6024 | `			}` |
|      50 | 6025 | `		}` |
|       - | 6026 | `	}` |
|       - | 6027 | `	/* php hands stream_open the FULL url, scheme included */` |
|     104 | 6028 | `	PH7_MemObjInit(pVm,&sPath);` |
|     104 | 6029 | `	PH7_MemObjInit(pVm,&sMode);` |
|     104 | 6030 | `	PH7_MemObjInit(pVm,&sOpts);` |
|     104 | 6031 | `	PH7_MemObjInit(pVm,&sRet);` |
|       - | 6032 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|       - | 6033 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|       - | 6034 | `	 * it as "could not be passed by reference"). */` |
|       - | 6035 | `	{` |
|     104 | 6036 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|     104 | 6037 | `		if( pRefSlot == 0 ){` |
|     ! 0 | 6038 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|     ! 0 | 6039 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 | 6040 | `			return -1;` |
|       - | 6041 | `		}` |
|     104 | 6042 | `		PH7_MemObjInit(pVm,&sOpened);` |
|     104 | 6043 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|       - | 6044 | `	}` |
|     100 | 6045 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|      71 | 6046 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|       - | 6047 | `		/* The one scheme php does NOT hand back whole. Its locate_url_wrapper` |
|       - | 6048 | `		 * strips "file://" for whoever owns the name, built-in or not, so a` |
|       - | 6049 | `		 * wrapper that replaced file:// sees the plain path -- the same bytes a` |
|       - | 6050 | `		 * bare path would have given it. */` |
|       3 | 6051 | `		ph7_value_string(&sPath,zName,-1);` |
|       2 | 6052 | `	}else{` |
|       - | 6053 | `		SyBlob sUrl;` |
|     102 | 6054 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     102 | 6055 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|     102 | 6056 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     102 | 6057 | `		SyBlobRelease(&sUrl);` |
|       - | 6058 | `	}` |
|     104 | 6059 | `	if( pVm->zOpenMode[0] ){` |
|       - | 6060 | `		/* fopen()/SplFileObject: php hands over the caller's own spelling. */` |
|      54 | 6061 | `		ph7_value_string(&sMode,pVm->zOpenMode,-1);` |
|      29 | 6062 | `	}else{` |
|       - | 6063 | `		/* Every other opener is C with a fixed mode, and php's own are the three` |
|       - | 6064 | `		 * BINARY spellings below -- file_get_contents()/file()/readfile()/copy's` |
|       - | 6065 | `		 * source and include are "rb", file_put_contents() "wb", and its` |
|       - | 6066 | `		 * FILE_APPEND "ab". Spell the flags back rather than guessing: PHL used` |
|       - | 6067 | `		 * to answer "r" for every one of them, so a wrapper was told a WRITE` |
|       - | 6068 | `		 * open was a read. */` |
|       - | 6069 | `		char zSpell[8];` |
|      54 | 6070 | `		int n = 0;` |
|      54 | 6071 | `		if( iMode & PH7_IO_OPEN_APPEND ){` |
|       3 | 6072 | `			zSpell[n++] = 'a';` |
|      53 | 6073 | `		}else if( iMode & PH7_IO_OPEN_EXCL ){` |
|     ! 0 | 6074 | `			zSpell[n++] = 'x';` |
|      52 | 6075 | `		}else if( iMode & PH7_IO_OPEN_TRUNC ){` |
|       6 | 6076 | `			zSpell[n++] = 'w';` |
|      50 | 6077 | `		}else if( iMode & PH7_IO_OPEN_CREATE ){` |
|     ! 0 | 6078 | `			zSpell[n++] = 'c';` |
|     ! 0 | 6079 | `		}else{` |
|      48 | 6080 | `			zSpell[n++] = 'r';` |
|       - | 6081 | `		}` |
|      54 | 6082 | `		if( iMode & PH7_IO_OPEN_RDWR ){` |
|     ! 0 | 6083 | `			zSpell[n++] = '+';` |
|     ! 0 | 6084 | `		}` |
|      54 | 6085 | `		if( (iMode & PH7_IO_OPEN_TEXT) == 0 ){` |
|      54 | 6086 | `			zSpell[n++] = 'b';` |
|      25 | 6087 | `		}` |
|      54 | 6088 | `		ph7_value_string(&sMode,zSpell,n);` |
|       - | 6089 | `	}` |
|     104 | 6090 | `	ph7_value_int(&sOpts,0);` |
|     104 | 6091 | `	apArg[0] = &sPath;` |
|     104 | 6092 | `	apArg[1] = &sMode;` |
|     104 | 6093 | `	apArg[2] = &sOpts;` |
|     104 | 6094 | `	apArg[3] = &sOpened;` |
|     104 | 6095 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|     104 | 6096 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|      42 | 6097 | `		rc = -1;` |
|      20 | 6098 | `	}` |
|     104 | 6099 | `	PH7_MemObjRelease(&sPath);` |
|     104 | 6100 | `	PH7_MemObjRelease(&sMode);` |
|     104 | 6101 | `	PH7_MemObjRelease(&sOpts);` |
|     104 | 6102 | `	PH7_MemObjRelease(&sOpened);` |
|     104 | 6103 | `	PH7_MemObjRelease(&sRet);` |
|     104 | 6104 | `	if( rc != 0 ){` |
|       - | 6105 | `		/* php's own wording for a wrapper that declined: the call it made, not` |
|       - | 6106 | `		 * an errno the wrapper never set. */` |
|      42 | 6107 | `		PH7_StreamSetOpenErrorCall(pVm,pSlot->zClass,"stream_open");` |
|      42 | 6108 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      42 | 6109 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|      42 | 6110 | `		return -1;` |
|       - | 6111 | `	}` |
|      63 | 6112 | `	*ppHandle = (void *)pH;` |
|      63 | 6113 | `	return PH7_OK;` |
|      54 | 6114 | `}` |
|       - | 6115 | `/*` |
|       - | 6116 | `` * Instantiate the wrapper class of a userland slot and set php's `$context` on it,`` |
|       - | 6117 | ` * the way every dispatch of the protocol does. Answers 0 when the class is gone.` |
|       - | 6118 | ` */` |
|     412 | 6119 | `static ph7_class_instance * UwrapNewInstance(ph7_vm *pVm,uwrap_slot *pSlot,void *pStreamCtx)` |
|       2 | 6120 | `{` |
|     414 | 6121 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|       - | 6122 | `	ph7_class_instance *pObj;` |
|       - | 6123 | `	ph7_value *pCtxSlot;` |
|     414 | 6124 | `	if( pClass == 0 ){` |
|     ! 0 | 6125 | `		return 0;` |
|       - | 6126 | `	}` |
|     414 | 6127 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     414 | 6128 | `	if( pObj == 0 ){` |
|     ! 0 | 6129 | `		return 0;` |
|       - | 6130 | `	}` |
|       - | 6131 | `	/* php adds the slot whether or not the class declares it. */` |
|     414 | 6132 | `	pCtxSlot = PH7_NativeAttr(pObj,"context");` |
|     414 | 6133 | `	if( pCtxSlot == 0 ){` |
|     ! 0 | 6134 | `		pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pObj,"context",sizeof("context")-1,0);` |
|     ! 0 | 6135 | `	}` |
|     414 | 6136 | `	if( pCtxSlot ){` |
|     414 | 6137 | `		if( pStreamCtx ){` |
|      29 | 6138 | `			ph7_value_resource(pCtxSlot,pStreamCtx);` |
|      15 | 6139 | `		}else{` |
|     386 | 6140 | `			ph7_value_null(pCtxSlot);` |
|       - | 6141 | `		}` |
|     206 | 6142 | `	}` |
|     414 | 6143 | `	return pObj;` |
|     208 | 6144 | `}` |
|       - | 6145 | `/* The slot a path belongs to, or 0 when no userland wrapper owns it. */` |
|   76165 | 6146 | `static uwrap_slot * UwrapSlotForPath(ph7_vm *pVm,const char *zPath)` |
|       5 | 6147 | `{` |
|   76170 | 6148 | `	const char *zTail = zPath;` |
|       - | 6149 | `	const ph7_io_stream *pDev;` |
|       - | 6150 | `	int i;` |
|   76170 | 6151 | `	if( zPath == 0 ){` |
|     ! 0 | 6152 | `		return 0;` |
|       - | 6153 | `	}` |
|   76170 | 6154 | `	pDev = PH7_VmGetStreamDevice(pVm,&zTail,(int)SyStrlen(zPath));` |
|   76170 | 6155 | `	if( pDev == 0 ){` |
|       6 | 6156 | `		return 0;` |
|       - | 6157 | `	}` |
|  682320 | 6158 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  606555 | 6159 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pDev ){` |
|     398 | 6160 | `			return &g_aUwrap[i];` |
|       - | 6161 | `		}` |
|  302862 | 6162 | `	}` |
|   75770 | 6163 | `	return 0;` |
|   38060 | 6164 | `}` |
|       - | 6165 | `/*` |
|       - | 6166 | ` * php's php_stream_cast() over a USERLAND stream, which is what a caller that` |
|       - | 6167 | ` * wants a real descriptor for an open stream does -- ext/fileinfo asks for one` |
|       - | 6168 | ` * after it has read the bytes, and the answer is visible either way: a wrapper` |
|       - | 6169 | ` * that declares stream_cast() is CALLED (with STREAM_CAST_AS_STREAM, and` |
|       - | 6170 | ` * whatever it answers is discarded here), and one that does not gets php's` |
|       - | 6171 | `` * `%s::stream_cast is not implemented!` warning. Silent for every other kind of`` |
|       - | 6172 | ` * stream, which all have a descriptor of their own.` |
|       - | 6173 | ` */` |
|      22 | 6174 | `PH7_PRIVATE void PH7_StreamUserCast(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle)` |
|       1 | 6175 | `{` |
|       - | 6176 | `	uwrap_handle *pH;` |
|       - | 6177 | `	ph7_class_method *pMeth;` |
|      23 | 6178 | `	if( pCtx == 0 \|\| pHandle == 0 \|\| !IoPrivateIsUwrap(pStream) ){` |
|      17 | 6179 | `		return;` |
|       - | 6180 | `	}` |
|       7 | 6181 | `	pH = (uwrap_handle *)pHandle;` |
|       7 | 6182 | `	if( pH->pObj == 0 ){` |
|     ! 0 | 6183 | `		return;` |
|       - | 6184 | `	}` |
|       7 | 6185 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,"stream_cast",sizeof("stream_cast")-1);` |
|       7 | 6186 | `	if( pMeth == 0 ){` |
|      10 | 6187 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 6188 | `			"%z::stream_cast is not implemented!",&pH->pObj->pClass->sName);` |
|       7 | 6189 | `		return;` |
|       - | 6190 | `	}` |
|       - | 6191 | `	{` |
|       - | 6192 | `		ph7_value sAs,sRet;` |
|       - | 6193 | `		ph7_value *apArg[1];` |
|     ! 0 | 6194 | `		PH7_MemObjInit(pH->pVm,&sAs);` |
|     ! 0 | 6195 | `		PH7_MemObjInit(pH->pVm,&sRet);` |
|     ! 0 | 6196 | `		ph7_value_int(&sAs,PH7_STREAM_CAST_AS_STREAM);` |
|     ! 0 | 6197 | `		apArg[0] = &sAs;` |
|     ! 0 | 6198 | `		PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,&sRet,1,apArg);` |
|     ! 0 | 6199 | `		PH7_MemObjRelease(&sAs);` |
|     ! 0 | 6200 | `		PH7_MemObjRelease(&sRet);` |
|       - | 6201 | `	}` |
|      12 | 6202 | `}` |
|       - | 6203 | `/*` |
|       - | 6204 | ` * php's WRITE door for a userland wrapper: unlink(), rename(), mkdir(), rmdir() and` |
|       - | 6205 | ` * stream_metadata() -- what touch(), chmod(), chown() and chgrp() all become.` |
|       - | 6206 | ` *` |
|       - | 6207 | ``  * PHL sent every one of them to the OS instead, so `unlink('vfs://root/t.txt')` `` |
|       - | 6208 | `` * reported `No such file or directory` about a path the OS had never heard of and`` |
|       - | 6209 | ` * the wrapper was never told. php dispatches the method with the FULL url first and` |
|       - | 6210 | ` * the operation's own extra arguments after it, and answers the bool the wrapper` |
|       - | 6211 | ` * gives back.` |
|       - | 6212 | ` *` |
|       - | 6213 | ` * Same three answers as the stat door -- NOWRAP when nothing owns the path, OK with` |
|       - | 6214 | `` * *pbAnswer set, FAIL after php's `%s::%s is not implemented!` -- plus the raw`` |
|       - | 6215 | ` * unwound status when the wrapper threw.` |
|       - | 6216 | ` */` |
|   50432 | 6217 | `PH7_PRIVATE int PH7_StreamUserPathOp(ph7_context *pCtx,const char *zPath,const char *zMethod,` |
|       - | 6218 | `	void *pStreamCtx,ph7_value **apExtra,int nExtra,int *pbAnswer)` |
|       5 | 6219 | `{` |
|       - | 6220 | `	ph7_vm *pVm;` |
|       - | 6221 | `	uwrap_slot *pSlot;` |
|       - | 6222 | `	ph7_class_instance *pObj;` |
|       - | 6223 | `	ph7_class_method *pMeth;` |
|       - | 6224 | `	ph7_value sUrl,sRet;` |
|       - | 6225 | `	ph7_value *apArg[4];` |
|       - | 6226 | `	int i,rc,iRet;` |
|   50437 | 6227 | `	if( pCtx == 0 \|\| zPath == 0 \|\| nExtra > 3 ){` |
|     ! 0 | 6228 | `		return PHL_URLSTAT_NOWRAP;` |
|       - | 6229 | `	}` |
|   50437 | 6230 | `	pVm = pCtx->pVm;` |
|   50437 | 6231 | `	pSlot = UwrapSlotForPath(pVm,zPath);` |
|   50437 | 6232 | `	if( pSlot == 0 ){` |
|   50365 | 6233 | `		return PHL_URLSTAT_NOWRAP;` |
|       - | 6234 | `	}` |
|      74 | 6235 | `	pObj = UwrapNewInstance(pVm,pSlot,pStreamCtx);` |
|      74 | 6236 | `	if( pObj == 0 ){` |
|     ! 0 | 6237 | `		return PHL_URLSTAT_FAIL;` |
|       - | 6238 | `	}` |
|      74 | 6239 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|      74 | 6240 | `	if( pMeth == 0 ){` |
|      16 | 6241 | `		PH7_ClassInstanceUnref(pObj);` |
|      23 | 6242 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      14 | 6243 | `			"%s::%s is not implemented!",pSlot->zClass,zMethod);` |
|      16 | 6244 | `		return PHL_URLSTAT_FAIL;` |
|       - | 6245 | `	}` |
|      60 | 6246 | `	PH7_MemObjInit(pVm,&sUrl);` |
|      60 | 6247 | `	PH7_MemObjInit(pVm,&sRet);` |
|      60 | 6248 | `	ph7_value_string(&sUrl,zPath,-1);` |
|      60 | 6249 | `	apArg[0] = &sUrl;` |
|     160 | 6250 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|     102 | 6251 | `		apArg[i+1] = apExtra[i];` |
|      52 | 6252 | `	}` |
|      60 | 6253 | `	rc = PH7_VmCallClassMethod(pVm,pObj,pMeth,&sRet,nExtra+1,apArg);` |
|      60 | 6254 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 | 6255 | `		iRet = rc;` |
|      60 | 6256 | `	}else if( rc == SXRET_OK ){` |
|      60 | 6257 | `		*pbAnswer = ph7_value_to_bool(&sRet);` |
|      60 | 6258 | `		iRet = PHL_URLSTAT_OK;` |
|      31 | 6259 | `	}else{` |
|     ! 0 | 6260 | `		iRet = PHL_URLSTAT_FAIL;` |
|       - | 6261 | `	}` |
|      60 | 6262 | `	PH7_MemObjRelease(&sUrl);` |
|      60 | 6263 | `	PH7_MemObjRelease(&sRet);` |
|      60 | 6264 | `	PH7_ClassInstanceUnref(pObj);` |
|      60 | 6265 | `	return iRet;` |
|   25196 | 6266 | `}` |
|       - | 6267 | `/*` |
|       - | 6268 | ` * php's streamWrapper::url_stat(): the STAT door of a userland wrapper.` |
|       - | 6269 | ` *` |
|       - | 6270 | ` * PHL's stat family went straight to the OS VFS for every path, so a path a script's` |
|       - | 6271 | `` * own wrapper owns -- `vfs://root/t.txt`, the shape every test suite that fakes a`` |
|       - | 6272 | ` * filesystem writes -- answered "does not exist" even though fopen() on the same name` |
|       - | 6273 | `` * worked. php asks the wrapper instead: `url_stat($url, $flags)`, with the FULL url`` |
|       - | 6274 | ` * and php's own flag bits, and reads php's thirteen NAMED fields off the array it` |
|       - | 6275 | ` * gets back (the numeric half a wrapper usually merges in is IGNORED, and a field it` |
|       - | 6276 | ` * omits reads 0).` |
|       - | 6277 | ` *` |
|       - | 6278 | ` * Nothing but plain integers crosses the call: the wrapper is PHP code, and every` |
|       - | 6279 | ` * ph7_value the caller holds is invalidated by running some (see the` |
|       - | 6280 | `` * `pointers-die-across-a-user-callback` rule), so the answer leaves here as aVal[13]`` |
|       - | 6281 | ` * and the caller builds its array afterwards.` |
|       - | 6282 | ` *` |
|       - | 6283 | ` * Answers PHL_URLSTAT_NOWRAP when no userland wrapper owns the path -- the caller` |
|       - | 6284 | ` * asks the VFS exactly as before -- PHL_URLSTAT_OK when the wrapper filled aVal, and` |
|       - | 6285 | ` * PHL_URLSTAT_FAIL when it declined. A wrapper with no url_stat at all is php's own` |
|       - | 6286 | `` * `%s::url_stat is not implemented!` warning, raised whatever the QUIET flag says,`` |
|       - | 6287 | ` * and then a failure; one that THREW hands the raw unwound status back.` |
|       - | 6288 | ` */` |
|   25733 | 6289 | `PH7_PRIVATE int PH7_StreamUserUrlStat(ph7_context *pCtx,const char *zPath,int iFlags,` |
|       - | 6290 | `	ph7_int64 *aVal)` |
|       5 | 6291 | `{` |
|       - | 6292 | `	static const char * const azField[] = {` |
|       - | 6293 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|       - | 6294 | `		"atime","mtime","ctime","blksize","blocks"` |
|       - | 6295 | `	};` |
|       - | 6296 | `	ph7_vm *pVm;` |
|       - | 6297 | `	uwrap_slot *pSlot;` |
|       - | 6298 | `	ph7_class_instance *pObj;` |
|       - | 6299 | `	ph7_class_method *pMeth;` |
|       - | 6300 | `	ph7_value sUrl,sFlags,sRet;` |
|       - | 6301 | `	ph7_value *apArg[2];` |
|       - | 6302 | `	int i,rc,iRet;` |
|   25738 | 6303 | `	if( pCtx == 0 \|\| zPath == 0 ){` |
|     ! 0 | 6304 | `		return PHL_URLSTAT_NOWRAP;` |
|       - | 6305 | `	}` |
|   25738 | 6306 | `	pVm = pCtx->pVm;` |
|   25738 | 6307 | `	pSlot = UwrapSlotForPath(pVm,zPath);` |
|   25738 | 6308 | `	if( pSlot == 0 ){` |
|   25414 | 6309 | `		return PHL_URLSTAT_NOWRAP;` |
|       - | 6310 | `	}` |
|       - | 6311 | ``	/* A stat has no opener behind it, so `$context` is php's NULL. */`` |
|     325 | 6312 | `	pObj = UwrapNewInstance(pVm,pSlot,0);` |
|     325 | 6313 | `	if( pObj == 0 ){` |
|     ! 0 | 6314 | `		return PHL_URLSTAT_FAIL;` |
|       - | 6315 | `	}` |
|     325 | 6316 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,"url_stat",sizeof("url_stat")-1);` |
|     325 | 6317 | `	if( pMeth == 0 ){` |
|       - | 6318 | `		/* php's own sentence, and it is raised even for a QUIET ask -- it reports the` |
|       - | 6319 | `		 * wrapper's own incompleteness, not the path's absence. */` |
|       5 | 6320 | `		PH7_ClassInstanceUnref(pObj);` |
|       7 | 6321 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       4 | 6322 | `			"%s::url_stat is not implemented!",pSlot->zClass);` |
|       5 | 6323 | `		return PHL_URLSTAT_FAIL;` |
|       - | 6324 | `	}` |
|     321 | 6325 | `	PH7_MemObjInit(pVm,&sUrl);` |
|     321 | 6326 | `	PH7_MemObjInit(pVm,&sFlags);` |
|     321 | 6327 | `	PH7_MemObjInit(pVm,&sRet);` |
|     321 | 6328 | `	ph7_value_string(&sUrl,zPath,-1);` |
|     321 | 6329 | `	ph7_value_int(&sFlags,iFlags);` |
|     321 | 6330 | `	apArg[0] = &sUrl;` |
|     321 | 6331 | `	apArg[1] = &sFlags;` |
|     321 | 6332 | `	rc = PH7_VmCallClassMethod(pVm,pObj,pMeth,&sRet,2,apArg);` |
|     321 | 6333 | `	iRet = PHL_URLSTAT_FAIL;` |
|     321 | 6334 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - | 6335 | `		/* The wrapper THREW. That is not a "the path is missing" answer: php lets` |
|       - | 6336 | `		 * the exception out of the stat, so the raw status travels back and a` |
|       - | 6337 | `		 * caller that would otherwise raise one of its OWN (SplFileInfo's` |
|       - | 6338 | `		 * RuntimeException) propagates this one instead. PH7_EXCEPTION and` |
|       - | 6339 | `		 * PH7_ABORT are both distinct from the three answers above, so they need` |
|       - | 6340 | `		 * no channel of their own. */` |
|       9 | 6341 | `		iRet = rc;` |
|     317 | 6342 | `	}else if( rc == SXRET_OK && ph7_value_is_array(&sRet) ){` |
|    3837 | 6343 | `		for( i = 0 ; i < (int)SX_ARRAYSIZE(azField) ; ++i ){` |
|    3563 | 6344 | `			ph7_value *pField = ph7_array_fetch(&sRet,azField[i],-1);` |
|    3563 | 6345 | `			aVal[i] = pField ? ph7_value_to_int64(pField) : 0;` |
|    1782 | 6346 | `		}` |
|       - | 6347 | `#ifdef __WINNT__` |
|       - | 6348 | `		/* php's Windows stat record has neither field, so it never reads the` |
|       - | 6349 | `		 * wrapper's two and reports -1 for both, as on every other stream. */` |
|       1 | 6350 | `		aVal[11] = -1;` |
|       1 | 6351 | `		aVal[12] = -1;` |
|       - | 6352 | `#endif` |
|     275 | 6353 | `		iRet = PHL_URLSTAT_OK;` |
|     137 | 6354 | `	}` |
|     321 | 6355 | `	PH7_MemObjRelease(&sUrl);` |
|     321 | 6356 | `	PH7_MemObjRelease(&sFlags);` |
|     321 | 6357 | `	PH7_MemObjRelease(&sRet);` |
|     321 | 6358 | `	PH7_ClassInstanceUnref(pObj);` |
|     321 | 6359 | `	return iRet;` |
|   12869 | 6360 | `}` |
|       - | 6361 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|       - | 6362 | `#define PHL_UWRAP_THUNK(N) \` |
|       - | 6363 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|       - | 6364 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|     100 | 6365 | `PHL_UWRAP_THUNK(0)` |
|       5 | 6366 | `PHL_UWRAP_THUNK(1)` |
|     ! 0 | 6367 | `PHL_UWRAP_THUNK(2)` |
|     ! 0 | 6368 | `PHL_UWRAP_THUNK(3)` |
|     ! 0 | 6369 | `PHL_UWRAP_THUNK(4)` |
|     ! 0 | 6370 | `PHL_UWRAP_THUNK(5)` |
|     ! 0 | 6371 | `PHL_UWRAP_THUNK(6)` |
|     ! 0 | 6372 | `PHL_UWRAP_THUNK(7)` |
|       - | 6373 | `/* Slot index -> its own opener */` |
|       - | 6374 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|       - | 6375 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|       - | 6376 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|       - | 6377 | `};` |
|       - | 6378 | `/*` |
|       - | 6379 | ` * php's DIRECTORY door for a userland wrapper: dir_opendir(), dir_readdir(),` |
|       - | 6380 | ` * dir_rewinddir() and dir_closedir().` |
|       - | 6381 | ` *` |
|       - | 6382 | `` * The slots carried no directory ops at all, so `opendir('vfs://root')` failed with`` |
|       - | 6383 | `` * `Failed to open directory: not implemented` and every reader built on it --`` |
|       - | 6384 | ` * scandir(), dir(), DirectoryIterator, FilesystemIterator -- failed with it. php` |
|       - | 6385 | `` * hands the opener the FULL url and its `$options` (0 for every caller that reaches`` |
|       - | 6386 | ` * here), then reads NAMES one at a time until the wrapper answers false. The list is` |
|       - | 6387 | `` * used exactly as given: php synthesizes no `.` or `..` for a userland wrapper.`` |
|       - | 6388 | ` */` |
|      16 | 6389 | `static int UwrapOpenDirSlot(int iSlot,const char *zName,ph7_value *pResource,void **ppHandle)` |
|       1 | 6390 | `{` |
|      17 | 6391 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      17 | 6392 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|       - | 6393 | `	uwrap_handle *pH;` |
|       - | 6394 | `	ph7_class_method *pMeth;` |
|       - | 6395 | `	ph7_value sPath,sOpts,sRet;` |
|       - | 6396 | `	ph7_value *apArg[2];` |
|       - | 6397 | `	int rc;` |
|      17 | 6398 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|     ! 0 | 6399 | `		return -1;` |
|       - | 6400 | `	}` |
|      17 | 6401 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      17 | 6402 | `	if( pH == 0 ){` |
|     ! 0 | 6403 | `		return -1;` |
|       - | 6404 | `	}` |
|      17 | 6405 | `	pH->pVm = pVm;` |
|      17 | 6406 | `	pH->iSlot = iSlot;` |
|      17 | 6407 | `	pH->bEof = 0;` |
|      17 | 6408 | `	pH->pObj = UwrapNewInstance(pVm,pSlot,pVm->pOpenCtx);` |
|      17 | 6409 | `	if( pH->pObj == 0 ){` |
|     ! 0 | 6410 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 | 6411 | `		return -1;` |
|       - | 6412 | `	}` |
|      17 | 6413 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,"dir_opendir",sizeof("dir_opendir")-1);` |
|      17 | 6414 | `	if( pMeth == 0 ){` |
|       3 | 6415 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       3 | 6416 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|       3 | 6417 | `		return -1;` |
|       - | 6418 | `	}` |
|       - | 6419 | `	/* php hands the opener the FULL url, scheme included -- with the one exception` |
|       - | 6420 | `	 * every other dispatch makes for a wrapper that replaced file://. */` |
|      15 | 6421 | `	PH7_MemObjInit(pVm,&sPath);` |
|      15 | 6422 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      15 | 6423 | `	PH7_MemObjInit(pVm,&sRet);` |
|      14 | 6424 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|       8 | 6425 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|     ! 0 | 6426 | `		ph7_value_string(&sPath,zName,-1);` |
|     ! 0 | 6427 | `	}else{` |
|       - | 6428 | `		SyBlob sUrl;` |
|      15 | 6429 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      15 | 6430 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      15 | 6431 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      15 | 6432 | `		SyBlobRelease(&sUrl);` |
|       - | 6433 | `	}` |
|      15 | 6434 | `	ph7_value_int(&sOpts,0);` |
|      15 | 6435 | `	apArg[0] = &sPath;` |
|      15 | 6436 | `	apArg[1] = &sOpts;` |
|      15 | 6437 | `	rc = UwrapCall(pH,"dir_opendir",2,apArg,&sRet);` |
|      15 | 6438 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|       3 | 6439 | `		rc = -1;` |
|       1 | 6440 | `	}` |
|      15 | 6441 | `	PH7_MemObjRelease(&sPath);` |
|      15 | 6442 | `	PH7_MemObjRelease(&sOpts);` |
|      15 | 6443 | `	PH7_MemObjRelease(&sRet);` |
|      15 | 6444 | `	if( rc != 0 ){` |
|       3 | 6445 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       3 | 6446 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|       3 | 6447 | `		return -1;` |
|       - | 6448 | `	}` |
|      13 | 6449 | `	*ppHandle = (void *)pH;` |
|      13 | 6450 | `	return PH7_OK;` |
|       9 | 6451 | `}` |
|       - | 6452 | `/* One xOpenDir thunk per slot, for the same reason xOpen needs one. */` |
|       - | 6453 | `#define PHL_UWRAP_DIR_THUNK(N) \` |
|       - | 6454 | `	static int UwrapOpenDir##N(const char *zName,ph7_value *pResource,void **ppHandle) \` |
|       - | 6455 | `	{ return UwrapOpenDirSlot(N,zName,pResource,ppHandle); }` |
|      15 | 6456 | `PHL_UWRAP_DIR_THUNK(0)` |
|       3 | 6457 | `PHL_UWRAP_DIR_THUNK(1)` |
|     ! 0 | 6458 | `PHL_UWRAP_DIR_THUNK(2)` |
|     ! 0 | 6459 | `PHL_UWRAP_DIR_THUNK(3)` |
|     ! 0 | 6460 | `PHL_UWRAP_DIR_THUNK(4)` |
|     ! 0 | 6461 | `PHL_UWRAP_DIR_THUNK(5)` |
|     ! 0 | 6462 | `PHL_UWRAP_DIR_THUNK(6)` |
|     ! 0 | 6463 | `PHL_UWRAP_DIR_THUNK(7)` |
|       - | 6464 | `static int (* const g_aUwrapOpenDir[PHL_UWRAP_MAX])(const char *,ph7_value *,void **) = {` |
|       - | 6465 | `	UwrapOpenDir0,UwrapOpenDir1,UwrapOpenDir2,UwrapOpenDir3,` |
|       - | 6466 | `	UwrapOpenDir4,UwrapOpenDir5,UwrapOpenDir6,UwrapOpenDir7` |
|       - | 6467 | `};` |
|       - | 6468 | `/*` |
|       - | 6469 | ` * One entry. The VFS contract reports a name by WRITING the call context's result,` |
|       - | 6470 | ` * and answers anything but PH7_OK to end the walk -- which is what the wrapper's own` |
|       - | 6471 | `` * `false` means.`` |
|       - | 6472 | ` */` |
|      46 | 6473 | `static int UwrapReadDir(void *pHandle,ph7_context *pCtx)` |
|       1 | 6474 | `{` |
|      47 | 6475 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - | 6476 | `	ph7_value sRet;` |
|       - | 6477 | `	const char *zName;` |
|      47 | 6478 | `	int nName = 0;` |
|      47 | 6479 | `	if( pH == 0 ){` |
|     ! 0 | 6480 | `		return -1;` |
|       - | 6481 | `	}` |
|      47 | 6482 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      47 | 6483 | `	if( UwrapCall(pH,"dir_readdir",0,0,&sRet) != 0 ){` |
|     ! 0 | 6484 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 | 6485 | `		return -1;` |
|       - | 6486 | `	}` |
|      47 | 6487 | `	if( (sRet.iFlags & MEMOBJ_BOOL) && sRet.x.iVal == 0 ){` |
|       - | 6488 | `		/* php's end of the walk. */` |
|      11 | 6489 | `		PH7_MemObjRelease(&sRet);` |
|      11 | 6490 | `		return -1;` |
|       - | 6491 | `	}` |
|      37 | 6492 | `	zName = ph7_value_to_string(&sRet,&nName);` |
|      37 | 6493 | `	if( nName < 1 ){` |
|     ! 0 | 6494 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 | 6495 | `		return -1;` |
|       - | 6496 | `	}` |
|      37 | 6497 | `	ph7_result_string(pCtx,zName,nName);` |
|      37 | 6498 | `	PH7_MemObjRelease(&sRet);` |
|      37 | 6499 | `	return PH7_OK;` |
|      24 | 6500 | `}` |
|       4 | 6501 | `static void UwrapRewindDir(void *pHandle)` |
|       1 | 6502 | `{` |
|       5 | 6503 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       5 | 6504 | `	if( pH ){` |
|       5 | 6505 | `		UwrapCall(pH,"dir_rewinddir",0,0,0);` |
|       2 | 6506 | `	}` |
|       5 | 6507 | `}` |
|      12 | 6508 | `static void UwrapCloseDir(void *pHandle)` |
|       1 | 6509 | `{` |
|      13 | 6510 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      13 | 6511 | `	if( pH == 0 ){` |
|     ! 0 | 6512 | `		return;` |
|       - | 6513 | `	}` |
|      13 | 6514 | `	UwrapCall(pH,"dir_closedir",0,0,0);` |
|      13 | 6515 | `	if( pH->pObj ){` |
|      13 | 6516 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       6 | 6517 | `	}` |
|      13 | 6518 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|       7 | 6519 | `}` |
|       - | 6520 | `/*` |
|       - | 6521 | ` * Why did a userland wrapper's directory open fail? php names the METHOD, and says` |
|       - | 6522 | `` * whether the wrapper has one at all -- `"C::dir_opendir" call failed` against`` |
|       - | 6523 | `` * `"C::dir_opendir" is not implemented`. Answers 0 when pStream is not one of ours,`` |
|       - | 6524 | ` * and the caller then keeps the C library's own errno text.` |
|       - | 6525 | ` */` |
|      42 | 6526 | `PH7_PRIVATE int PH7_StreamUserDirReason(ph7_vm *pVm,const ph7_io_stream *pStream,` |
|       - | 6527 | `	char *zBuf,int nBuf)` |
|       5 | 6528 | `{` |
|       - | 6529 | `	int i;` |
|     353 | 6530 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|       - | 6531 | `		ph7_class *pClass;` |
|       - | 6532 | `		int bHas;` |
|     315 | 6533 | `		if( g_aUwrap[i].pVm != pVm \|\| &g_aUwrap[i].sStream != pStream ){` |
|     311 | 6534 | `			continue;` |
|       - | 6535 | `		}` |
|       7 | 6536 | `		pClass = PH7_VmExtractClass(pVm,g_aUwrap[i].zClass,` |
|       4 | 6537 | `			(sxu32)SyStrlen(g_aUwrap[i].zClass),TRUE,0);` |
|       7 | 6538 | `		bHas = pClass != 0` |
|       4 | 6539 | `			&& PH7_ClassExtractMethod(pClass,"dir_opendir",sizeof("dir_opendir")-1) != 0;` |
|       7 | 6540 | `		SyBufferFormat(zBuf,(sxu32)nBuf,"\"%s::dir_opendir\" %s",g_aUwrap[i].zClass,` |
|       2 | 6541 | `			bHas ? "call failed" : "is not implemented");` |
|       5 | 6542 | `		return 1;` |
|     ! 0 | 6543 | `	}` |
|      43 | 6544 | `	return 0;` |
|      26 | 6545 | `}` |
|      44 | 6546 | `static int UwrapDeviceInstalled(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 | 6547 | `{` |
|      49 | 6548 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       - | 6549 | `	sxu32 n;` |
|     459 | 6550 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     425 | 6551 | `		if( apDev[n] == pStream ){` |
|      12 | 6552 | `			return 1;` |
|       - | 6553 | `		}` |
|     210 | 6554 | `	}` |
|      39 | 6555 | `	return 0;` |
|      27 | 6556 | `}` |
|       - | 6557 | `/* Put a device back in service. */` |
|      44 | 6558 | `static void UwrapUnsuppressDevice(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 | 6559 | `{` |
|      49 | 6560 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|      49 | 6561 | `	sxu32 n,nKeep = 0;` |
|      69 | 6562 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      22 | 6563 | `		if( apOff[n] == pStream ){` |
|      12 | 6564 | `			continue;` |
|       - | 6565 | `		}` |
|      12 | 6566 | `		apOff[nKeep++] = apOff[n];` |
|       7 | 6567 | `	}` |
|      49 | 6568 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|      49 | 6569 | `}` |
|       - | 6570 | `/*` |
|       - | 6571 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|       - | 6572 | ` * bool stream_wrapper_unregister(string $protocol)` |
|       - | 6573 | ` */` |
|      44 | 6574 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6575 | `{` |
|       - | 6576 | `	const char *zScheme,*zClass;` |
|      49 | 6577 | `	int nScheme,nClass,i,iFree = -1;` |
|      49 | 6578 | `	if( nArg < 2 ){` |
|     ! 0 | 6579 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6580 | `		return PH7_OK;` |
|       - | 6581 | `	}` |
|      49 | 6582 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      49 | 6583 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      44 | 6584 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      49 | 6585 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|     ! 0 | 6586 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6587 | `		return PH7_OK;` |
|       - | 6588 | `	}` |
|       - | 6589 | `	/* php: registering an already-taken protocol warns and returns false.` |
|       - | 6590 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|       - | 6591 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|       - | 6592 | `	{` |
|      49 | 6593 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|       - | 6594 | `		sxu32 n;` |
|     475 | 6595 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     431 | 6596 | `			if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|      22 | 6597 | `				continue; /* unregistered: the name is free again, which is the` |
|       - | 6598 | `				           * whole point of "replace file:// with my own" */` |
|       - | 6599 | `			}` |
|     406 | 6600 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     243 | 6601 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|     ! 0 | 6602 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 6603 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|     ! 0 | 6604 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 6605 | `				return PH7_OK;` |
|       - | 6606 | `			}` |
|     208 | 6607 | `		}` |
|       - | 6608 | `	}` |
|      63 | 6609 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      63 | 6610 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      49 | 6611 | `			iFree = i;` |
|      49 | 6612 | `			break;` |
|       - | 6613 | `		}` |
|      11 | 6614 | `	}` |
|      49 | 6615 | `	if( iFree < 0 ){` |
|     ! 0 | 6616 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 6617 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|     ! 0 | 6618 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6619 | `		return PH7_OK;` |
|       - | 6620 | `	}` |
|       - | 6621 | `	{` |
|      49 | 6622 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      49 | 6623 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      49 | 6624 | `		pSlot->zScheme[nScheme] = 0;` |
|      49 | 6625 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      49 | 6626 | `		pSlot->zClass[nClass] = 0;` |
|      49 | 6627 | `		pSlot->pVm = pCtx->pVm;` |
|       - | 6628 | `		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the` |
|       - | 6629 | `		 * whole reason the argument exists — a wrapper that says it speaks to the` |
|       - | 6630 | `		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was` |
|       - | 6631 | `		 * declared in the signature and read by nothing, so a wrapper registered as` |
|       - | 6632 | `		 * a URL was opened and INCLUDED like a local file whatever the` |
|       - | 6633 | `		 * configuration said. */` |
|      49 | 6634 | `		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);` |
|      49 | 6635 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      49 | 6636 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      49 | 6637 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      49 | 6638 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      49 | 6639 | `		pSlot->sStream.xOpenDir = g_aUwrapOpenDir[iFree];` |
|      49 | 6640 | `		pSlot->sStream.xCloseDir = UwrapCloseDir;` |
|      49 | 6641 | `		pSlot->sStream.xReadDir = UwrapReadDir;` |
|      49 | 6642 | `		pSlot->sStream.xRewindDir = UwrapRewindDir;` |
|      49 | 6643 | `		pSlot->sStream.xClose = UwrapClose;` |
|      49 | 6644 | `		pSlot->sStream.xRead = UwrapRead;` |
|      49 | 6645 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      49 | 6646 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      49 | 6647 | `		pSlot->sStream.xTell = UwrapTell;` |
|       - | 6648 | `		/* A slot is REUSED once its wrapper has been unregistered, and both the` |
|       - | 6649 | `		 * suppression set and the VM's device list still name it -- so lift the` |
|       - | 6650 | `		 * suppression and install the device only if it is not already there,` |
|       - | 6651 | `		 * or the freshly registered protocol would be born switched off (and` |
|       - | 6652 | `		 * listed twice). */` |
|      49 | 6653 | `		UwrapUnsuppressDevice(pCtx->pVm,&pSlot->sStream);` |
|      49 | 6654 | `		if( !UwrapDeviceInstalled(pCtx->pVm,&pSlot->sStream) ){` |
|      39 | 6655 | `			ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      17 | 6656 | `		}` |
|       - | 6657 | `	}` |
|      49 | 6658 | `	ph7_result_bool(pCtx,1);` |
|      49 | 6659 | `	return PH7_OK;` |
|      27 | 6660 | `}` |
|       - | 6661 | `/*` |
|       - | 6662 | ` * Suppress a live device and, when it is a userland slot, retire the slot with` |
|       - | 6663 | ` * it. Answers 0 when nothing by that name was in service.` |
|       - | 6664 | ` *` |
|       - | 6665 | ` * The match is EXACT and case-SENSITIVE, which php's is too: opening a stream` |
|       - | 6666 | ` * folds the scheme ("FILE://x" reads a file), but unregister() and restore()` |
|       - | 6667 | ` * delete from the wrapper hash by the bytes the script wrote, so` |
|       - | 6668 | ` * stream_wrapper_unregister('FILE') fails where 'file' succeeds.` |
|       - | 6669 | ` */` |
|      28 | 6670 | `static int UwrapSuppressDevice(ph7_vm *pVm,const char *zScheme,int nScheme)` |
|       4 | 6671 | `{` |
|      32 | 6672 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|      32 | 6673 | `	ph7_io_stream *pHit = 0;` |
|       - | 6674 | `	sxu32 n;` |
|       - | 6675 | `	int i;` |
|     330 | 6676 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     298 | 6677 | `		if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     189 | 6678 | `		 && SyMemcmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0` |
|      56 | 6679 | `		 && !PH7_VmStreamDeviceSuppressed(pVm,apDev[n]) ){` |
|      28 | 6680 | `			pHit = apDev[n]; /* the LIVE one is the last match */` |
|      12 | 6681 | `		}` |
|     153 | 6682 | `	}` |
|      32 | 6683 | `	if( pHit == 0 ){` |
|       5 | 6684 | `		return 0;` |
|       - | 6685 | `	}` |
|      28 | 6686 | `	if( SySetPut(&pVm->aSuppressedIo,(const void *)&pHit) != SXRET_OK ){` |
|     ! 0 | 6687 | `		return 0;` |
|       - | 6688 | `	}` |
|      68 | 6689 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      64 | 6690 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pHit ){` |
|      24 | 6691 | `			g_aUwrap[i].pVm = 0;` |
|      24 | 6692 | `			break;` |
|       - | 6693 | `		}` |
|      22 | 6694 | `	}` |
|      28 | 6695 | `	return 1;` |
|      18 | 6696 | `}` |
|       - | 6697 | `/*` |
|       - | 6698 | ` * bool stream_wrapper_unregister(string $protocol)` |
|       - | 6699 | ` *  Take a protocol out of service. It used to handle USERLAND slots only and` |
|       - | 6700 | ` *  answer FALSE for file/php/data/tcp, so the documented "replace file:// with` |
|       - | 6701 | ` *  my own wrapper" idiom failed loudly at the first step. A built-in is now` |
|       - | 6702 | ` *  suppressed per VM: PH7_VmGetStreamDevice() steps over it (including on the` |
|       - | 6703 | ` *  no-scheme default path, which is the same slot), stream_get_wrappers() stops` |
|       - | 6704 | ` *  naming it, and the name becomes free to register again.` |
|       - | 6705 | ` */` |
|      28 | 6706 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6707 | `{` |
|       - | 6708 | `	const char *zScheme;` |
|       - | 6709 | `	int nScheme;` |
|      32 | 6710 | `	if( nArg < 1 ){` |
|     ! 0 | 6711 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6712 | `		return PH7_OK;` |
|       - | 6713 | `	}` |
|      32 | 6714 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      32 | 6715 | `	if( nScheme > 0 && UwrapSuppressDevice(pCtx->pVm,zScheme,nScheme) ){` |
|      28 | 6716 | `		ph7_result_bool(pCtx,1);` |
|      28 | 6717 | `		return PH7_OK;` |
|       - | 6718 | `	}` |
|       7 | 6719 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 6720 | `		"Unable to unregister protocol %.*s://",nScheme,zScheme);` |
|       5 | 6721 | `	ph7_result_bool(pCtx,0);` |
|       5 | 6722 | `	return PH7_OK;` |
|      18 | 6723 | `}` |
|       - | 6724 | `/*` |
|       - | 6725 | ` * bool stream_wrapper_restore(string $protocol)` |
|       - | 6726 | ` *  Put a BUILT-IN protocol back, whether it was unregistered or replaced. The` |
|       - | 6727 | ` *  other half of the override pair, and useless without it -- which is why the` |
|       - | 6728 | ` *  two ship together.` |
|       - | 6729 | ` *` |
|       - | 6730 | ` *  php's three answers: a protocol that was never built in is a warning and` |
|       - | 6731 | ` *  FALSE; one that is built in and was never touched is an E_NOTICE and TRUE` |
|       - | 6732 | ` *  (it is already what it should be); anything else is restored and TRUE.` |
|       - | 6733 | ` */` |
|      12 | 6734 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6735 | `{` |
|      13 | 6736 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 6737 | `	const ph7_io_stream **apOff;` |
|       - | 6738 | `	ph7_io_stream **apDev;` |
|       - | 6739 | `	const char *zScheme;` |
|      13 | 6740 | `	int nScheme,bBuiltin = 0,bChanged = 0,i;` |
|       - | 6741 | `	sxu32 n,nKeep;` |
|      13 | 6742 | `	if( nArg < 1 ){` |
|     ! 0 | 6743 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6744 | `		return PH7_OK;` |
|       - | 6745 | `	}` |
|      13 | 6746 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      13 | 6747 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     133 | 6748 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     121 | 6749 | `		ph7_io_stream *pDev = apDev[n];` |
|     120 | 6750 | `		if( (int)SyStrlen(pDev->zName) != nScheme` |
|      86 | 6751 | `		 \|\| SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) != 0 ){` |
|     107 | 6752 | `			continue;` |
|       - | 6753 | `		}` |
|      15 | 6754 | `		if( UwrapIsSlotDevice(pDev) ){` |
|       - | 6755 | `			/* A userland wrapper standing in its place -- or one already` |
|       - | 6756 | `			 * withdrawn, which is still not a built-in. */` |
|       9 | 6757 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|       5 | 6758 | `				bChanged = 1;` |
|       2 | 6759 | `			}` |
|       9 | 6760 | `			continue;` |
|       - | 6761 | `		}` |
|       7 | 6762 | `		bBuiltin = 1;` |
|       7 | 6763 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|       5 | 6764 | `			bChanged = 1;` |
|       2 | 6765 | `		}` |
|       4 | 6766 | `	}` |
|      13 | 6767 | `	if( !bBuiltin ){` |
|      10 | 6768 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 | 6769 | `			"%.*s:// never existed, nothing to restore",nScheme,zScheme);` |
|       7 | 6770 | `		ph7_result_bool(pCtx,0);` |
|       7 | 6771 | `		return PH7_OK;` |
|       - | 6772 | `	}` |
|       7 | 6773 | `	if( !bChanged ){` |
|       - | 6774 | `		/* php answers TRUE here and says so at NOTICE level: the protocol is` |
|       - | 6775 | `		 * already the one it would restore. */` |
|       4 | 6776 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       1 | 6777 | `			"%.*s:// was never changed, nothing to restore",nScheme,zScheme);` |
|       3 | 6778 | `		ph7_result_bool(pCtx,1);` |
|       3 | 6779 | `		return PH7_OK;` |
|       - | 6780 | `	}` |
|       - | 6781 | `	/* Lift the suppression off the BUILT-IN first, by compacting the set... */` |
|       5 | 6782 | `	apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|       5 | 6783 | `	nKeep = 0;` |
|       9 | 6784 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|       5 | 6785 | `		const ph7_io_stream *pDev = apOff[n];` |
|       4 | 6786 | `		if( (int)SyStrlen(pDev->zName) == nScheme` |
|       4 | 6787 | `		 && SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) == 0` |
|       5 | 6788 | `		 && !UwrapIsSlotDevice(pDev) ){` |
|       5 | 6789 | `			continue; /* the built-in comes back */` |
|       - | 6790 | `		}` |
|     ! 0 | 6791 | `		apOff[nKeep++] = pDev;` |
|     ! 0 | 6792 | `	}` |
|       5 | 6793 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|       - | 6794 | `	/* ...then retire every userland wrapper standing in for the name. */` |
|      37 | 6795 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      32 | 6796 | `		if( g_aUwrap[i].pVm == pVm` |
|      18 | 6797 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|       5 | 6798 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|       5 | 6799 | `			const ph7_io_stream *pDead = &g_aUwrap[i].sStream;` |
|       5 | 6800 | `			g_aUwrap[i].pVm = 0;` |
|       5 | 6801 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDead) ){` |
|       5 | 6802 | `				SySetPut(&pVm->aSuppressedIo,(const void *)&pDead);` |
|       2 | 6803 | `			}` |
|       2 | 6804 | `		}` |
|      17 | 6805 | `	}` |
|       5 | 6806 | `	ph7_result_bool(pCtx,1);` |
|       5 | 6807 | `	return PH7_OK;` |
|       7 | 6808 | `}` |
|       - | 6809 | `#ifdef PH7_ENABLE_NET` |
|       - | 6810 | `/*` |
|       - | 6811 | `` * php's socket address: `[transport://]host:port`. What a re-derivation gets`` |
|       - | 6812 | ` * wrong here is that BOTH halves have a diagnostic of their own, and neither is` |
|       - | 6813 | ` * the other: a transport this build does not carry is not a malformed address,` |
|       - | 6814 | ` * and an address with no port is not an unknown transport.` |
|       - | 6815 | ` */` |
|       - | 6816 | `#define SOCK_ADDR_OK        0` |
|       - | 6817 | `#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */` |
|       - | 6818 | `#define SOCK_ADDR_PARSE     2 /* no port separator at all */` |
|       - | 6819 | ``#define SOCK_ADDR_IPV6      3 /* opened `[` and did not close it with `]:` */`` |
|       - | 6820 | `/*` |
|       - | 6821 | `` * php's port half is `atoi()` of whatever follows the FIRST colon, and the`` |
|       - | 6822 | ` * colon is looked for in every position but the LAST — which is the whole` |
|       - | 6823 | `` * difference between `127.0.0.1:` (php's "Failed to parse address") and`` |
|       - | 6824 | `` * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that`` |
|       - | 6825 | ` * reads digits strictly refuses three addresses php accepts, and one that takes` |
|       - | 6826 | `` * the last colon reads `a:b:c` differently than php does.`` |
|       - | 6827 | ` */` |
|     318 | 6828 | `static int SockParsePort(const char *z,int n)` |
|       4 | 6829 | `{` |
|     322 | 6830 | `	int i = 0,iSign = 1,iVal = 0;` |
|     480 | 6831 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r'` |
|     316 | 6832 | `	              \|\| z[i] == '\v' \|\| z[i] == '\f') ){` |
|     ! 0 | 6833 | `		i++;` |
|     ! 0 | 6834 | `	}` |
|     322 | 6835 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|     ! 0 | 6836 | `		iSign = z[i] == '-' ? -1 : 1;` |
|     ! 0 | 6837 | `		i++;` |
|     ! 0 | 6838 | `	}` |
|    1654 | 6839 | `	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|    1336 | 6840 | `		if( iVal < 1000000000 ){` |
|    1336 | 6841 | `			iVal = iVal * 10 + (z[i] - '0');` |
|     666 | 6842 | `		}` |
|     670 | 6843 | `	}` |
|     322 | 6844 | `	return iSign * iVal;` |
|       4 | 6845 | `}` |
|     330 | 6846 | `static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,` |
|       - | 6847 | `	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest,int *pbDgram)` |
|       4 | 6848 | `{` |
|     334 | 6849 | `	const char *zRest = zAddr;` |
|     334 | 6850 | `	int nRest = nAddr,i,nHost = -1;` |
|     334 | 6851 | `	*pPort = 0;` |
|     334 | 6852 | `	*pzTransport = "tcp";` |
|     334 | 6853 | `	*pnTransport = 3;` |
|     334 | 6854 | `	*pbDgram = 0;` |
|    3130 | 6855 | `	for( i = 0 ; i + 2 < nAddr ; i++ ){` |
|    2940 | 6856 | `		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){` |
|     144 | 6857 | `			*pzTransport = zAddr;` |
|     144 | 6858 | `			*pnTransport = i;` |
|     144 | 6859 | `			zRest = &zAddr[i+3];` |
|     144 | 6860 | `			nRest = nAddr - i - 3;` |
|     144 | 6861 | `			break;` |
|       - | 6862 | `		}` |
|    1402 | 6863 | `	}` |
|     334 | 6864 | `	*pzRest = zRest;` |
|     334 | 6865 | `	*pnRest = nRest;` |
|       - | 6866 | `	/* php looks the transport up in a hash keyed by the name as WRITTEN, so the` |
|       - | 6867 | ``	 * lookup is case-SENSITIVE -- `TCP://127.0.0.1:80` is a transport php has`` |
|       - | 6868 | `	 * not got, where a wrapper SCHEME (file://, PHP://) is folded first. This` |
|       - | 6869 | `	 * used to fold here too, so PHL connected through four spellings php` |
|       - | 6870 | `	 * refuses. */` |
|     334 | 6871 | `	if( *pnTransport == 3 && SyStrncmp(*pzTransport,"udp",3) == 0 ){` |
|      24 | 6872 | `		*pbDgram = 1;` |
|     323 | 6873 | `	}else if( *pnTransport != 3 \|\| SyStrncmp(*pzTransport,"tcp",3) != 0 ){` |
|      12 | 6874 | `		return SOCK_ADDR_TRANSPORT;` |
|       - | 6875 | `	}` |
|     324 | 6876 | `	if( nRest > 1 && zRest[0] == '[' ){` |
|       - | 6877 | `` 		/* php reads the BRACKETED form before it looks for a port at all: a `]` `` |
|       - | 6878 | ``		 * anywhere but the last byte, with a `:` immediately after it, and the`` |
|       - | 6879 | `		 * host is what the brackets hold. Anything else is a refusal of its own` |
|       - | 6880 | `		 * wording -- not the "Failed to parse address" a missing port gets --` |
|       - | 6881 | ``		 * and `[]:9` is an EMPTY host, which the resolver is what refuses.`` |
|       - | 6882 | `		 * (What this build does with the address it parses is §10's IPv4-only` |
|       - | 6883 | ``		 * cut: `::1` reaches the resolver and is refused there.) */`` |
|      69 | 6884 | `		for( i = 1 ; i + 1 < nRest ; i++ ){` |
|      63 | 6885 | `			if( zRest[i] == ']' ){` |
|      13 | 6886 | `				break;` |
|       - | 6887 | `			}` |
|      26 | 6888 | `		}` |
|      19 | 6889 | `		if( i + 1 >= nRest \|\| zRest[i] != ']' \|\| zRest[i+1] != ':' ){` |
|       9 | 6890 | `			return SOCK_ADDR_IPV6;` |
|       - | 6891 | `		}` |
|      11 | 6892 | `		*pPort = SockParsePort(&zRest[i+2],nRest - i - 2);` |
|      11 | 6893 | `		nHost = i - 1;` |
|      11 | 6894 | `		if( nHost >= nHostBuf ){` |
|     ! 0 | 6895 | `			nHost = nHostBuf - 1;` |
|     ! 0 | 6896 | `		}` |
|      11 | 6897 | `		if( nHost > 0 ){` |
|      11 | 6898 | `			SyMemcpy(&zRest[1],zHost,(sxu32)nHost);` |
|       5 | 6899 | `		}` |
|      11 | 6900 | `		zHost[nHost] = 0;` |
|      11 | 6901 | `		return SOCK_ADDR_OK;` |
|       - | 6902 | `	}` |
|    2956 | 6903 | `	for( i = 0 ; i + 1 < nRest ; i++ ){` |
|    2944 | 6904 | `		if( zRest[i] == ':' ){` |
|     294 | 6905 | `			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);` |
|     294 | 6906 | `			nHost = i;` |
|     294 | 6907 | `			break;` |
|       - | 6908 | `		}` |
|    1329 | 6909 | `	}` |
|     306 | 6910 | `	if( nHost < 0 ){` |
|      15 | 6911 | `		return SOCK_ADDR_PARSE;` |
|       - | 6912 | `	}` |
|     294 | 6913 | `	if( nHost >= nHostBuf ){` |
|     ! 0 | 6914 | `		nHost = nHostBuf - 1;` |
|     ! 0 | 6915 | `	}` |
|     294 | 6916 | `	if( nHost > 0 ){` |
|     288 | 6917 | `		SyMemcpy(zRest,zHost,(sxu32)nHost);` |
|     142 | 6918 | `	}` |
|     294 | 6919 | `	zHost[nHost] = 0;` |
|     294 | 6920 | `	return SOCK_ADDR_OK;` |
|     169 | 6921 | `}` |
|       - | 6922 | `/*` |
|       - | 6923 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|       - | 6924 | ` *                          string &$error_message, ?float $timeout = null)` |
|       - | 6925 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|       - | 6926 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|       - | 6927 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|       - | 6928 | ` */` |
|     266 | 6929 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6930 | `{` |
|     270 | 6931 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     270 | 6932 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|     270 | 6933 | `	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";` |
|       - | 6934 | `	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];` |
|       - | 6935 | `	const char *zShow;` |
|     270 | 6936 | `	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;` |
|     270 | 6937 | `	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect,bDgram = 0;` |
|       - | 6938 | `	ph7_socket sock;` |
|       - | 6939 | `	io_private *pDev;` |
|     270 | 6940 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|     270 | 6941 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|     270 | 6942 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|     270 | 6943 | `	phl_stream_ctx *pCtxRes = 0;` |
|       - | 6944 | `	ph7_sockopts sOpt;` |
|       - | 6945 | `	char zBindHost[256];` |
|     270 | 6946 | `	int bThrew = 0;` |
|     270 | 6947 | `	if( nArg < 1 ){` |
|     ! 0 | 6948 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6949 | `		return PH7_OK;` |
|       - | 6950 | `	}` |
|     270 | 6951 | `	if( bClientForm ){` |
|       - | 6952 | ``		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such`` |
|       - | 6953 | `		 * argument, so only the stream_socket_client() spelling takes one. */` |
|      90 | 6954 | `		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);` |
|      90 | 6955 | `		if( bThrew ){` |
|     ! 0 | 6956 | `			return PH7_OK;` |
|       - | 6957 | `		}` |
|      43 | 6958 | `	}` |
|     270 | 6959 | `	zRaw = ph7_value_to_string(apArg[0],&nRaw);` |
|     270 | 6960 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     181 | 6961 | `		iPortArg = ph7_value_to_int(apArg[1]);` |
|      90 | 6962 | `	}` |
|     270 | 6963 | `	if( bClientForm && nArg > 4 ){` |
|       - | 6964 | `		/* Declared in the signature and read by nothing until now, so the` |
|       - | 6965 | `		 * documented spellings did nothing and their constants were undefined` |
|       - | 6966 | `		 * fatals. */` |
|      39 | 6967 | `		iFlags = (int)ph7_value_to_int64(apArg[4]);` |
|      18 | 6968 | `	}` |
|       - | 6969 | `	/* pfsockopen() IS fsockopen() with this flag; php has no other difference` |
|       - | 6970 | `	 * between them. */` |
|     313 | 6971 | `	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0` |
|     223 | 6972 | `		: (zFunc[0] == 'p');` |
|       - | 6973 | `	/* php passes STREAM_XPORT_CONNECT and STREAM_XPORT_CONNECT_ASYNC as two` |
|       - | 6974 | `	 * separate bits and its transport connects for EITHER, so` |
|       - | 6975 | ``	 * `stream_socket_client($a, $e, $es, null, STREAM_CLIENT_ASYNC_CONNECT)` --`` |
|       - | 6976 | `	 * the documented spelling for an asynchronous dial -- is a connected socket` |
|       - | 6977 | `	 * in php and was a socket-less handle here, writing 0 bytes and naming no` |
|       - | 6978 | `	 * peer. */` |
|     270 | 6979 | `	bConnect = bClientForm` |
|     176 | 6980 | `		? (iFlags & (PH7_STREAM_CLIENT_CONNECT\|PH7_STREAM_CLIENT_ASYNC_CONNECT)) != 0 : 1;` |
|       - | 6981 | `	/* php builds ONE address out of fsockopen()'s two arguments — and only when` |
|       - | 6982 | ``	 * the port is a usable one, which is why `fsockopen($h)` reports the address`` |
|       - | 6983 | `	 * it could not parse rather than connecting to port 0. The address it SHOWS` |
|       - | 6984 | `	 * keeps the port either way. */` |
|     270 | 6985 | `	if( bClientForm \|\| iPortArg <= 0 ){` |
|      90 | 6986 | `		zAddr = zRaw;` |
|      90 | 6987 | `		nAddr = nRaw;` |
|      47 | 6988 | `	}else{` |
|     181 | 6989 | `		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     181 | 6990 | `		zAddr = zAddrBuf;` |
|       - | 6991 | `	}` |
|     270 | 6992 | `	if( bClientForm ){` |
|      90 | 6993 | `		zShow = zRaw;` |
|      90 | 6994 | `		nShow = nRaw;` |
|      47 | 6995 | `	}else{` |
|     181 | 6996 | `		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     181 | 6997 | `		zShow = zShowBuf;` |
|       - | 6998 | `	}` |
|     270 | 6999 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|       - | 7000 | `		&zRest,&nRest,&bDgram);` |
|     270 | 7001 | `	if( (rc == SOCK_ADDR_PARSE \|\| rc == SOCK_ADDR_IPV6) && !bConnect ){` |
|       - | 7002 | `		/* php splits the address in TWO places: the transport is looked up when` |
|       - | 7003 | `		 * the stream is created and the host:port half is parsed by the` |
|       - | 7004 | `		 * connect() -- so a $flags without STREAM_CLIENT_CONNECT never looks at` |
|       - | 7005 | ``		 * the address at all, and `stream_socket_client('0.0.0.0', $e, $es,`` |
|       - | 7006 | ``		 * null, 0)` is a resource in php where PHL reported an address it could`` |
|       - | 7007 | `		 * not parse. A transport nothing is registered under still fails. */` |
|       3 | 7008 | `		rc = SOCK_ADDR_OK;` |
|       1 | 7009 | `	}` |
|     270 | 7010 | `	if( rc != SOCK_ADDR_OK ){` |
|      18 | 7011 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|       - | 7012 | `			/* php's own wording for a transport its build does not carry —` |
|       - | 7013 | `			 * which is what this engine's missing ones ARE (§7.4), and what a` |
|       - | 7014 | `			 * script reading $errstr is written against. This used to spell a` |
|       - | 7015 | `			 * message of PHL's own that no php ever answers. */` |
|      10 | 7016 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 7017 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|       3 | 7018 | `				nTransport,zTransport);` |
|      15 | 7019 | `		}else if( rc == SOCK_ADDR_IPV6 ){` |
|       9 | 7020 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse IPv6 address \"%.*s\"",nRest,zRest);` |
|       5 | 7021 | `		}else{` |
|       3 | 7022 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|       - | 7023 | `		}` |
|      18 | 7024 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);` |
|      18 | 7025 | `		ph7_result_bool(pCtx,0);` |
|      18 | 7026 | `		return PH7_OK;` |
|       - | 7027 | `	}` |
|     254 | 7028 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|     211 | 7029 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|     211 | 7030 | `		if( rTimeout > 0 ){` |
|     211 | 7031 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     104 | 7032 | `		}` |
|     104 | 7033 | `	}` |
|     254 | 7034 | `	if( bPersist ){` |
|       - | 7035 | `		/* A live one for this address IS the answer: php hands the same resource` |
|       - | 7036 | `		 * back rather than opening a second connection to the same peer. */` |
|       - | 7037 | `		char zKey[320];` |
|       - | 7038 | `		io_private *pKept;` |
|      24 | 7039 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|      24 | 7040 | `		pKept = SockPersistFind(pCtx->pVm,zKey);` |
|      24 | 7041 | `		if( pKept ){` |
|       - | 7042 | `			/* php does not hand a kept connection back unseen: it runs the same` |
|       - | 7043 | `			 * liveness probe feof() uses, with a zero timeout, and a socket the` |
|       - | 7044 | `			 * far end has finished with is CLOSED and dialled again. Without` |
|       - | 7045 | `			 * this a persistent handle stays broken for the rest of the` |
|       - | 7046 | `			 * request -- every later call gets the same dead socket, and the` |
|       - | 7047 | `			 * script's writes fail on a connection php would have replaced. */` |
|      12 | 7048 | `			ph7_socket *pKeptSock = IoPrivateSocket(pKept);` |
|      12 | 7049 | `			if( pKeptSock == 0 \|\| PH7_NetIsAlive(*pKeptSock) ){` |
|      10 | 7050 | `				SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      10 | 7051 | `				ph7_result_resource(pCtx,pKept);` |
|      10 | 7052 | `				return PH7_OK;` |
|       - | 7053 | `			}` |
|       3 | 7054 | `			PH7_StreamCloseHandle(pKept->pStream,pKept->pHandle);` |
|       3 | 7055 | `			MarkIOPrivateClosed(pKept);` |
|       3 | 7056 | `			SockPersistDrop(pCtx->pVm,zKey);` |
|       1 | 7057 | `		}` |
|       7 | 7058 | `	}` |
|     246 | 7059 | `	if( !bConnect ){` |
|       - | 7060 | `		/* php creates the socket while CONNECTING it, so a $flags without` |
|       - | 7061 | `		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no` |
|       - | 7062 | `		 * name at either end, reads false, writes 0, already at end of file. */` |
|       6 | 7063 | `		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|       6 | 7064 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,bDgram,zAddr,nAddr);` |
|       6 | 7065 | `		if( pDev == 0 ){` |
|     ! 0 | 7066 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 7067 | `			return PH7_OK;` |
|       - | 7068 | `		}` |
|       6 | 7069 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|       6 | 7070 | `		ph7_result_resource(pCtx,pDev);` |
|       6 | 7071 | `		return PH7_OK;` |
|       - | 7072 | `	}` |
|       - | 7073 | `	{` |
|       - | 7074 | ``		/* php reads the `socket` options at the moment it creates the socket:`` |
|       - | 7075 | `		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and` |
|       - | 7076 | `		 * bindto is the LOCAL address it takes before connecting. */` |
|     242 | 7077 | `		const char *zOptErr = 0;` |
|     242 | 7078 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|       - | 7079 | `			/* The one option failure php treats as a failed CONNECT rather than` |
|       - | 7080 | `			 * as a warning it can carry on past. */` |
|       3 | 7081 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);` |
|       3 | 7082 | `			ph7_result_bool(pCtx,0);` |
|       3 | 7083 | `			return PH7_OK;` |
|       - | 7084 | `		}` |
|       - | 7085 | `	}` |
|       - | 7086 | `	/* ASYNC_CONNECT is the one flag that changes the CALL rather than the` |
|       - | 7087 | `	 * socket: php issues a non-blocking connect and answers a resource for a` |
|       - | 7088 | `	 * dial that has not finished (or has already been refused). */` |
|     300 | 7089 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,bDgram,` |
|     148 | 7090 | `		bClientForm && (iFlags & PH7_STREAM_CLIENT_ASYNC_CONNECT) != 0,` |
|       - | 7091 | `		&sOpt,&iErrno,&zErr);` |
|     240 | 7092 | `	if( sOpt.iBindErr ){` |
|       - | 7093 | `		/* php's own wording, and NEITHER shape stops the connection: the socket` |
|       - | 7094 | `		 * goes out from wherever the routing table would have sent it. It tells` |
|       - | 7095 | `		 * the two apart — a local address that is not a numeric literal at all` |
|       - | 7096 | `		 * names the host, one the OS refused to BIND names the address it tried` |
|       - | 7097 | `		 * and the reason. */` |
|       9 | 7098 | `		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){` |
|       5 | 7099 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",` |
|       4 | 7100 | `				sOpt.zBindHost ? sOpt.zBindHost : "");` |
|       3 | 7101 | `		}else{` |
|       - | 7102 | `			/* php RE-COMPOSES the address it tried from the parts it parsed, so` |
|       - | 7103 | ``			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"`` |
|       - | 7104 | `			 * is reported as '192.0.2.1:7'. */` |
|       5 | 7105 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 7106 | `				"Failed to bind to '%s:%d', system said: %s",` |
|       4 | 7107 | `				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,` |
|       2 | 7108 | `				PH7_NetStrError(sOpt.iBindErrno));` |
|       - | 7109 | `		}` |
|       4 | 7110 | `	}` |
|     240 | 7111 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     108 | 7112 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|       3 | 7113 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|       3 | 7114 | `			iErrno = 0;` |
|       1 | 7115 | `		}` |
|     108 | 7116 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);` |
|     108 | 7117 | `		ph7_result_bool(pCtx,0);` |
|     108 | 7118 | `		return PH7_OK;` |
|       - | 7119 | `	}` |
|     133 | 7120 | `	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|       - | 7121 | `	/* Wrap the socket in an io_private so the whole f* family works on it. php` |
|       - | 7122 | ``	 * reports the ADDRESS it opened as the handle's `uri`, which is the same`` |
|       - | 7123 | `	 * one-address-out-of-two-arguments composition it connected through — so an` |
|       - | 7124 | `	 * argument naming only a host still records the port beside it. */` |
|     133 | 7125 | `	pDev = SockWrapSocket(pCtx,sock,bDgram,zAddr,nAddr);` |
|     133 | 7126 | `	if( pDev == 0 ){` |
|     ! 0 | 7127 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7128 | `		return PH7_OK;` |
|       - | 7129 | `	}` |
|       - | 7130 | `	/* php attaches the opener's context to a TRANSPORT stream and to nothing` |
|       - | 7131 | `	 * else — which is why stream_context_get_options() answers for a socket and` |
|       - | 7132 | `	 * answers the empty set for a file opened through the very same call. */` |
|     133 | 7133 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     133 | 7134 | `	SockArmDefaultTimeout(pCtx,pDev);` |
|     133 | 7135 | `	if( bPersist ){` |
|       - | 7136 | `		char zKey[320];` |
|      16 | 7137 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|      16 | 7138 | `		SockPersistKeep(pCtx->pVm,zKey,pDev);` |
|       - | 7139 | `		/* get_resource_type() names it apart, which is how a script can tell it` |
|       - | 7140 | `		 * asked for one at all. */` |
|      16 | 7141 | `		pDev->bPersist = 1;` |
|       7 | 7142 | `	}` |
|     133 | 7143 | `	ph7_result_resource(pCtx,pDev);` |
|     133 | 7144 | `	return PH7_OK;` |
|     137 | 7145 | `}` |
|       - | 7146 | `/*` |
|       - | 7147 | ` * resource\|false stream_socket_server(string $address, int &$error_code,` |
|       - | 7148 | ` *                    string &$error_message, int $flags = STREAM_SERVER_BIND\|STREAM_SERVER_LISTEN,` |
|       - | 7149 | ` *                    ?resource $context = null)` |
|       - | 7150 | ` *` |
|       - | 7151 | ` * The name a php program becomes a SERVER through, and a loud` |
|       - | 7152 | `` * `Call to undefined function` until now — so a script that listens on a port`` |
|       - | 7153 | ` * (a test double, a job runner, a line protocol) could not be spelled at all,` |
|       - | 7154 | ` * even though net.c had bind() and listen() all along.` |
|       - | 7155 | ` *` |
|       - | 7156 | ` * php's two flags are separate for a reason: BIND alone is what a datagram` |
|       - | 7157 | ` * socket wants (there is nothing to listen for), so LISTEN is what makes the` |
|       - | 7158 | ` * socket a stream server. Dropping LISTEN from a tcp:// address is therefore` |
|       - | 7159 | ` * a bound socket nothing can connect to, which is exactly what php answers.` |
|       - | 7160 | ` */` |
|      64 | 7161 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 7162 | `{` |
|      68 | 7163 | `	const char *zAddr,*zTransport,*zRest,*zErr = "";` |
|       - | 7164 | `	char zHost[256];` |
|      68 | 7165 | `	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc,bDgram = 0;` |
|       - | 7166 | `	ph7_socket sock;` |
|       - | 7167 | `	io_private *pDev;` |
|       - | 7168 | `	phl_stream_ctx *pCtxRes;` |
|       - | 7169 | `	ph7_sockopts sOpt;` |
|       - | 7170 | `	char zBindHost[256];` |
|      68 | 7171 | `	int bThrew = 0;` |
|      68 | 7172 | `	if( nArg < 1 ){` |
|     ! 0 | 7173 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7174 | `		return PH7_OK;` |
|       - | 7175 | `	}` |
|      68 | 7176 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);` |
|      68 | 7177 | `	if( bThrew ){` |
|     ! 0 | 7178 | `		return PH7_OK;` |
|       - | 7179 | `	}` |
|       - | 7180 | ``	/* The signature row declares `string $address`, so whatever arrives has`` |
|       - | 7181 | `	 * already been screened; php's own ZPP then CASTS it, and refusing an int` |
|       - | 7182 | `` 	 * here would answer false in silence for `stream_socket_server(8080)` `` |
|       - | 7183 | `	 * where php reports the address it could not parse. */` |
|      68 | 7184 | `	zAddr = ph7_value_to_string(apArg[0],&nAddr);` |
|      49 | 7185 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])` |
|      45 | 7186 | `		: (PH7_STREAM_SERVER_BIND\|PH7_STREAM_SERVER_LISTEN);` |
|      68 | 7187 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|       - | 7188 | `		&zRest,&nRest,&bDgram);` |
|      64 | 7189 | `	if( (rc == SOCK_ADDR_PARSE \|\| rc == SOCK_ADDR_IPV6)` |
|      40 | 7190 | `	 && (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|       - | 7191 | `		/* The server half of the same split: the bind() is what parses` |
|       - | 7192 | `		 * host:port, so a $flags without STREAM_SERVER_BIND answers a socket` |
|       - | 7193 | ``		 * for an address php never reads -- `stream_socket_server('0.0.0.0',`` |
|       - | 7194 | ``		 * $e, $es, STREAM_SERVER_LISTEN)` included, LISTEN being unreachable`` |
|       - | 7195 | `		 * without BIND. The transport is still resolved. */` |
|       3 | 7196 | `		rc = SOCK_ADDR_OK;` |
|       1 | 7197 | `	}` |
|      68 | 7198 | `	if( rc != SOCK_ADDR_OK ){` |
|       - | 7199 | `		char zMsg[512];` |
|      13 | 7200 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|       - | 7201 | `			/* php's own wording for a transport its build has not got, which is` |
|       - | 7202 | `			 * what udp://, unix:// and ssl:// are here (§7.4). */` |
|       8 | 7203 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 7204 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|       2 | 7205 | `				nTransport,zTransport);` |
|      10 | 7206 | `		}else if( rc == SOCK_ADDR_IPV6 ){` |
|     ! 0 | 7207 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse IPv6 address \"%.*s\"",nRest,zRest);` |
|     ! 0 | 7208 | `		}else{` |
|       8 | 7209 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|       - | 7210 | `		}` |
|      13 | 7211 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);` |
|      13 | 7212 | `		ph7_result_bool(pCtx,0);` |
|      13 | 7213 | `		return PH7_OK;` |
|       - | 7214 | `	}` |
|      58 | 7215 | `	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|       - | 7216 | `		/* php creates the socket while BINDING it, so a $flags without` |
|       - | 7217 | `		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:` |
|       - | 7218 | `		 * it has no name, reads false, writes 0 and is already at end of file.` |
|       - | 7219 | ``		 * It does not even resolve the host — `stream_socket_server(':1', $e,`` |
|       - | 7220 | ``		 * $es, 0)` is a resource in php. */`` |
|       8 | 7221 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,bDgram,zAddr,nAddr);` |
|       8 | 7222 | `		if( pDev == 0 ){` |
|     ! 0 | 7223 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 7224 | `			return PH7_OK;` |
|       - | 7225 | `		}` |
|       8 | 7226 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|       8 | 7227 | `		SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|       8 | 7228 | `		ph7_result_resource(pCtx,pDev);` |
|       8 | 7229 | `		return PH7_OK;` |
|       - | 7230 | `	}` |
|      52 | 7231 | `	if( zHost[0] == 0 ){` |
|       - | 7232 | ``		/* An address with no host at all (`:8080`) is a name php asks the`` |
|       - | 7233 | `		 * resolver about and is refused for — NOT a wildcard bind. Answering` |
|       - | 7234 | `		 * 0.0.0.0 for it would put a listener on every interface of the` |
|       - | 7235 | `		 * machine, which is the unsafe direction. */` |
|       - | 7236 | `		char zMsg[512];` |
|       3 | 7237 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,` |
|       1 | 7238 | `			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);` |
|       2 | 7239 | `		ph7_result_bool(pCtx,0);` |
|       2 | 7240 | `		return PH7_OK;` |
|       - | 7241 | `	}` |
|       - | 7242 | `	{` |
|       - | 7243 | ``		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;`` |
|       - | 7244 | ``		 * `bindto` is not one of its options, because the address argument IS`` |
|       - | 7245 | `		 * where a server binds (php ignores it here too). */` |
|      50 | 7246 | `		const char *zOptErr = 0;` |
|      50 | 7247 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|     ! 0 | 7248 | `			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);` |
|     ! 0 | 7249 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 7250 | `			return PH7_OK;` |
|       - | 7251 | `		}` |
|      50 | 7252 | `		sOpt.zBindHost = 0;` |
|       - | 7253 | `	}` |
|      50 | 7254 | `	if( bDgram && (iFlags & PH7_STREAM_SERVER_LISTEN) != 0 ){` |
|       - | 7255 | `		/* php's udp ops answer STREAM_XPORT_OP_LISTEN with a flat -1 -- they do` |
|       - | 7256 | `		 * not call listen() and they log NOTHING -- so the DEFAULT $flags,` |
|       - | 7257 | `		 * BIND\|LISTEN, fails on a datagram address with no reason of its own to` |
|       - | 7258 | `		 * report. That is what makes STREAM_SERVER_BIND the spelling a udp` |
|       - | 7259 | `		 * server is written with, and the failure keeps php's shape: the socket` |
|       - | 7260 | `		 * is created and bound first, then thrown away, so an address that` |
|       - | 7261 | `		 * cannot be bound at all reports THAT instead. */` |
|       3 | 7262 | `		sock = PH7_NetBind(zHost,iPort,1,0,SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|       3 | 7263 | `		if( sock != PH7_NET_INVALID_SOCKET ){` |
|       3 | 7264 | `			PH7_NetClose(sock);` |
|       3 | 7265 | `			sock = PH7_NET_INVALID_SOCKET;` |
|       3 | 7266 | `			iErrno = 0;` |
|       - | 7267 | ``			/* No text at all: php's `errstr` stays the empty string it was`` |
|       - | 7268 | `			 * pre-assigned and only the warning fills the gap, with the words` |
|       - | 7269 | ``			 * `Unknown error`. */`` |
|       3 | 7270 | `			zErr = 0;` |
|       1 | 7271 | `		}` |
|       2 | 7272 | `	}else{` |
|      48 | 7273 | `		sock = PH7_NetBind(zHost,iPort,bDgram,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,` |
|       - | 7274 | `			SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|       - | 7275 | `	}` |
|      50 | 7276 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|       - | 7277 | `		char zMsg[512];` |
|       5 | 7278 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|     ! 0 | 7279 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|     ! 0 | 7280 | `		}` |
|       - | 7281 | `		/* php reports no OS code for a refused ADDRESS — only a connect() that` |
|       - | 7282 | `		 * reached the network carries one — so this stays 0 for every arm. */` |
|       5 | 7283 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);` |
|       5 | 7284 | `		ph7_result_bool(pCtx,0);` |
|       5 | 7285 | `		return PH7_OK;` |
|       - | 7286 | `	}` |
|      46 | 7287 | `	SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|      46 | 7288 | `	pDev = SockWrapSocket(pCtx,sock,bDgram,zAddr,nAddr);` |
|      46 | 7289 | `	if( pDev == 0 ){` |
|     ! 0 | 7290 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7291 | `		return PH7_OK;` |
|       - | 7292 | `	}` |
|      46 | 7293 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|      46 | 7294 | `	ph7_result_resource(pCtx,pDev);` |
|      46 | 7295 | `	return PH7_OK;` |
|      36 | 7296 | `}` |
|       - | 7297 | `/*` |
|       - | 7298 | ` * resource\|false stream_socket_accept(resource $socket, ?float $timeout = null,` |
|       - | 7299 | ` *                                    string &$peer_name = null)` |
|       - | 7300 | ` *` |
|       - | 7301 | ` * The other half of a server, and the one with the timing in it. php waits at` |
|       - | 7302 | `` * most `default_socket_timeout` seconds by default — NOT forever — and reports`` |
|       - | 7303 | ` * an expired wait as a warning plus false, which is what lets a single-threaded` |
|       - | 7304 | ` * server do something else between connections. A negative timeout blocks.` |
|       - | 7305 | ` */` |
|      44 | 7306 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 7307 | `{` |
|       - | 7308 | `	io_private *pDev,*pOut;` |
|       - | 7309 | `	ph7_socket *pSock,sock;` |
|       - | 7310 | `	char zPeer[128];` |
|      48 | 7311 | `	int rc,bTimedOut = 0,iTimeoutMs;` |
|      48 | 7312 | `	if( nArg < 1 ){` |
|     ! 0 | 7313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7314 | `		return PH7_OK;` |
|       - | 7315 | `	}` |
|      48 | 7316 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      48 | 7317 | `	if( pDev == 0 ){` |
|       3 | 7318 | `		return rc;` |
|       - | 7319 | `	}` |
|      46 | 7320 | `	pSock = IoPrivateSocket(pDev);` |
|      46 | 7321 | `	if( pSock == 0 ){` |
|       - | 7322 | `		/* Not a socket at all. php's own answer for it reads oddly and is what` |
|       - | 7323 | `		 * a script sees: the accept never reaches the network, so there is no` |
|       - | 7324 | `		 * OS error to report and php asks its error table for code 0. */` |
|       3 | 7325 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");` |
|       3 | 7326 | `		ph7_result_bool(pCtx,0);` |
|       3 | 7327 | `		return PH7_OK;` |
|       - | 7328 | `	}` |
|      44 | 7329 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      44 | 7330 | `		double rTimeout = ph7_value_to_double(apArg[1]);` |
|      44 | 7331 | `		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);` |
|      24 | 7332 | `	}else{` |
|     ! 0 | 7333 | `		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);` |
|     ! 0 | 7334 | `		if( iTimeoutMs < 0 ){` |
|     ! 0 | 7335 | `			iTimeoutMs = -1;` |
|     ! 0 | 7336 | `		}` |
|       - | 7337 | `	}` |
|      44 | 7338 | `	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));` |
|      44 | 7339 | `	if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - | 7340 | `		/* php waits and then reports the expiry; there is nothing to wait on. */` |
|       3 | 7341 | `		bTimedOut = 1;` |
|       1 | 7342 | `	}` |
|      44 | 7343 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      12 | 7344 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",` |
|       6 | 7345 | `			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));` |
|       9 | 7346 | `		ph7_result_bool(pCtx,0);` |
|       9 | 7347 | `		return PH7_OK;` |
|       - | 7348 | `	}` |
|      38 | 7349 | `	if( nArg > 2 ){` |
|       6 | 7350 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|       6 | 7351 | `		if( pTmp ){` |
|       6 | 7352 | `			ph7_value_string(pTmp,zPeer,-1);` |
|       6 | 7353 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);` |
|       2 | 7354 | `		}` |
|       2 | 7355 | `	}` |
|       - | 7356 | ``	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by`` |
|       - | 7357 | `	 * name, so stream_get_meta_data() has no address to answer with. */` |
|      38 | 7358 | `	pOut = SockWrapSocket(pCtx,sock,0,0,0);` |
|      38 | 7359 | `	if( pOut == 0 ){` |
|     ! 0 | 7360 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7361 | `		return PH7_OK;` |
|       - | 7362 | `	}` |
|      38 | 7363 | `	SockArmDefaultTimeout(pCtx,pOut);` |
|      38 | 7364 | `	ph7_result_resource(pCtx,pOut);` |
|      38 | 7365 | `	return PH7_OK;` |
|      26 | 7366 | `}` |
|       - | 7367 | `/*` |
|       - | 7368 | `` * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected`` |
|       - | 7369 | ` * stream that has none, and NULL for a read that did not happen — php writes it` |
|       - | 7370 | ` * on every call rather than leaving the caller's previous value in place.` |
|       - | 7371 | ` */` |
|      26 | 7372 | `static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)` |
|       2 | 7373 | `{` |
|       - | 7374 | `	ph7_value *pTmp;` |
|      28 | 7375 | `	if( iArg >= nArg ){` |
|       9 | 7376 | `		return;` |
|       - | 7377 | `	}` |
|      20 | 7378 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|      20 | 7379 | `	if( pTmp == 0 ){` |
|     ! 0 | 7380 | `		return;` |
|       - | 7381 | `	}` |
|      20 | 7382 | `	if( zAddr ){` |
|      18 | 7383 | `		ph7_value_string(pTmp,zAddr,-1);` |
|      10 | 7384 | `	}else{` |
|       3 | 7385 | `		ph7_value_null(pTmp);` |
|       - | 7386 | `	}` |
|      20 | 7387 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);` |
|      15 | 7388 | `}` |
|       - | 7389 | `/*` |
|       - | 7390 | ` * bool stream_socket_shutdown(resource $stream, int $mode)` |
|       - | 7391 | ` *` |
|       - | 7392 | ` * The half-close: "I am done SENDING" without closing a handle the program` |
|       - | 7393 | ` * still wants to read from, which is how every request/response protocol tells` |
|       - | 7394 | ` * its peer the request is over. Nothing else can say it — fclose() takes the` |
|       - | 7395 | ` * read side with it.` |
|       - | 7396 | ` */` |
|      10 | 7397 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7398 | `{` |
|       - | 7399 | `	io_private *pDev;` |
|       - | 7400 | `	ph7_socket *pSock;` |
|       - | 7401 | `	ph7_int64 iHow;` |
|       - | 7402 | `	int rc;` |
|      12 | 7403 | `	if( nArg < 2 ){` |
|     ! 0 | 7404 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7405 | `		return PH7_OK;` |
|       - | 7406 | `	}` |
|      12 | 7407 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);` |
|      12 | 7408 | `	if( pDev == 0 ){` |
|     ! 0 | 7409 | `		return rc;` |
|       - | 7410 | `	}` |
|      12 | 7411 | `	iHow = ph7_value_to_int64(apArg[1]);` |
|      12 | 7412 | `	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){` |
|       - | 7413 | `		/* php names the three constants rather than the numbers behind them. */` |
|       4 | 7414 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 7415 | `			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",` |
|       1 | 7416 | `			ph7_function_name(pCtx));` |
|       - | 7417 | `	}` |
|      10 | 7418 | `	pSock = IoPrivateSocket(pDev);` |
|      10 | 7419 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - | 7420 | `		/* Not a socket: php answers false in silence, since there is no` |
|       - | 7421 | `		 * direction to shut down. */` |
|       3 | 7422 | `		ph7_result_bool(pCtx,0);` |
|       3 | 7423 | `		return PH7_OK;` |
|       - | 7424 | `	}` |
|       8 | 7425 | `	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;` |
|       8 | 7426 | `	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){` |
|       - | 7427 | `		/* The read side is gone AND nothing is queued behind it, so this handle` |
|       - | 7428 | `		 * is at its end: php answers feof() for a socket by probing it, and a` |
|       - | 7429 | ``		 * `while (!feof($s))` drain loop after a half-close would otherwise spin`` |
|       - | 7430 | `		 * on a stream that can never answer again. Bytes that HAD arrived are` |
|       - | 7431 | `		 * still handed over — which is why the answer is probed rather than` |
|       - | 7432 | `		 * assumed, and why the device's own latch stays clear. */` |
|       4 | 7433 | `		pDev->bEof = 1;` |
|       1 | 7434 | `	}` |
|       8 | 7435 | `	ph7_result_bool(pCtx,rc);` |
|       8 | 7436 | `	return PH7_OK;` |
|       7 | 7437 | `}` |
|       - | 7438 | `/*` |
|       - | 7439 | ` * string\|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,` |
|       - | 7440 | ` *                                    string &$address = null)` |
|       - | 7441 | ` * int\|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,` |
|       - | 7442 | ` *                               string $address = "")` |
|       - | 7443 | ` *` |
|       - | 7444 | ` * The pair that reaches the socket UNDERNEATH the stream: php's own asks the` |
|       - | 7445 | `` * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can`` |
|       - | 7446 | ` * look at bytes without consuming them (nothing else in the family can) and why` |
|       - | 7447 | ` * a recvfrom() on a handle a line read has already buffered WAITS for more.` |
|       - | 7448 | `` * The `$address` is what a datagram carries and a connected stream does not.`` |
|       - | 7449 | ` */` |
|      28 | 7450 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7451 | `{` |
|       - | 7452 | `	io_private *pDev;` |
|       - | 7453 | `	ph7_socket *pSock;` |
|       - | 7454 | `	ph7_int64 nLen;` |
|       - | 7455 | `	char zAddr[128],*zBuf;` |
|      30 | 7456 | `	int rc,iFlags = 0,n;` |
|      30 | 7457 | `	if( nArg < 2 ){` |
|     ! 0 | 7458 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7459 | `		return PH7_OK;` |
|       - | 7460 | `	}` |
|      30 | 7461 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      30 | 7462 | `	if( pDev == 0 ){` |
|     ! 0 | 7463 | `		return rc;` |
|       - | 7464 | `	}` |
|      30 | 7465 | `	nLen = ph7_value_to_int64(apArg[1]);` |
|      30 | 7466 | `	if( nLen < 1 ){` |
|       4 | 7467 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 7468 | `			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|       - | 7469 | `	}` |
|      28 | 7470 | `	if( nArg > 2 ){` |
|      20 | 7471 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|       9 | 7472 | `	}` |
|      28 | 7473 | `	pSock = IoPrivateSocket(pDev);` |
|      28 | 7474 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       3 | 7475 | `		SockStoreAddress(pCtx,apArg,nArg,3,0);` |
|       3 | 7476 | `		ph7_result_bool(pCtx,0);` |
|       3 | 7477 | `		return PH7_OK;` |
|       - | 7478 | `	}` |
|      26 | 7479 | `	if( nLen > 0x7FFFFFF0 ){` |
|     ! 0 | 7480 | `		nLen = 0x7FFFFFF0;` |
|     ! 0 | 7481 | `	}` |
|      26 | 7482 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      26 | 7483 | `	if( zBuf == 0 ){` |
|     ! 0 | 7484 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 7485 | `	}` |
|      26 | 7486 | `	zAddr[0] = 0;` |
|      26 | 7487 | `	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));` |
|       - | 7488 | `	/* php writes the out-param on every call: the sender's address for a read` |
|       - | 7489 | `	 * that happened (empty for a connected stream, which has none to report) and` |
|       - | 7490 | `	 * NULL for one that did not — never the caller's previous value. */` |
|      26 | 7491 | `	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);` |
|      26 | 7492 | `	if( n < 0 ){` |
|       3 | 7493 | `		ph7_result_bool(pCtx,0);` |
|       2 | 7494 | `	}else{` |
|      24 | 7495 | `		ph7_result_string(pCtx,zBuf,n);` |
|       - | 7496 | `	}` |
|      26 | 7497 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      26 | 7498 | `	return PH7_OK;` |
|      16 | 7499 | `}` |
|       - | 7500 | `/*` |
|       - | 7501 | ` * The address stream_socket_sendto() takes, which is NOT the one an opener` |
|       - | 7502 | ` * reads. php parses this one with network_parse_network_address_with_port(),` |
|       - | 7503 | ` * and the two differ in three places: this one looks for the colon across the` |
|       - | 7504 | `` * WHOLE string (so `1.2.3.4:` is a port of 0 and the send fails with EINVAL,`` |
|       - | 7505 | ` * where an opener answers "Failed to parse address"), it knows no transport at` |
|       - | 7506 | `` * all (so `udp://1.2.3.4:53` names the host `udp`), and an EMPTY host half is a`` |
|       - | 7507 | ` * name like any other, which the resolver is what refuses. The bracketed IPv6` |
|       - | 7508 | ` * form is read here too, and a malformed one has no wording of its own -- php's` |
|       - | 7509 | ` * helper simply fails and the caller prints its own refusal.` |
|       - | 7510 | ` *` |
|       - | 7511 | ` * Answers 1 with zHost and the port filled, or 0 for an address that is none.` |
|       - | 7512 | ` */` |
|      14 | 7513 | `static int SockSendToAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort)` |
|       3 | 7514 | `{` |
|      17 | 7515 | `	int i,nHost = -1,iStart = 0;` |
|      17 | 7516 | `	zHost[0] = 0;` |
|      17 | 7517 | `	if( nAddr > 1 && zAddr[0] == '[' ){` |
|       9 | 7518 | `		for( i = 1 ; i + 1 < nAddr ; i++ ){` |
|       9 | 7519 | `			if( zAddr[i] == ']' ){` |
|       3 | 7520 | `				break;` |
|       - | 7521 | `			}` |
|       4 | 7522 | `		}` |
|       3 | 7523 | `		if( i + 1 >= nAddr \|\| zAddr[i] != ']' \|\| zAddr[i+1] != ':' ){` |
|     ! 0 | 7524 | `			return 0;` |
|       - | 7525 | `		}` |
|       3 | 7526 | `		*pPort = SockParsePort(&zAddr[i+2],nAddr - i - 2);` |
|       3 | 7527 | `		nHost = i - 1;` |
|       3 | 7528 | `		iStart = 1;` |
|       2 | 7529 | `	}else{` |
|     115 | 7530 | `		for( i = 0 ; i < nAddr ; i++ ){` |
|     109 | 7531 | `			if( zAddr[i] == ':' ){` |
|       9 | 7532 | `				*pPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);` |
|       9 | 7533 | `				nHost = i;` |
|       9 | 7534 | `				break;` |
|       - | 7535 | `			}` |
|      53 | 7536 | `		}` |
|      15 | 7537 | `		if( nHost < 0 ){` |
|       8 | 7538 | `			return 0;` |
|       - | 7539 | `		}` |
|       - | 7540 | `	}` |
|      11 | 7541 | `	if( nHost >= nHostBuf ){` |
|     ! 0 | 7542 | `		nHost = nHostBuf - 1;` |
|     ! 0 | 7543 | `	}` |
|      11 | 7544 | `	if( nHost > 0 ){` |
|      11 | 7545 | `		SyMemcpy(&zAddr[iStart],zHost,(sxu32)nHost);` |
|       4 | 7546 | `	}` |
|      11 | 7547 | `	zHost[nHost] = 0;` |
|      11 | 7548 | `	return 1;` |
|      10 | 7549 | `}` |
|      24 | 7550 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 7551 | `{` |
|       - | 7552 | `	io_private *pDev;` |
|       - | 7553 | `	ph7_socket *pSock;` |
|      27 | 7554 | `	const char *zData,*zSentTo = "";` |
|       - | 7555 | `	char zHost[256];` |
|      27 | 7556 | `	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0,bHaveAddr = 0;` |
|      27 | 7557 | `	if( nArg < 2 ){` |
|     ! 0 | 7558 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7559 | `		return PH7_OK;` |
|       - | 7560 | `	}` |
|      27 | 7561 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      27 | 7562 | `	if( pDev == 0 ){` |
|     ! 0 | 7563 | `		return rc;` |
|       - | 7564 | `	}` |
|      27 | 7565 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|      27 | 7566 | `	if( nArg > 2 ){` |
|      17 | 7567 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|       7 | 7568 | `	}` |
|      27 | 7569 | `	zHost[0] = 0;` |
|      27 | 7570 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|       - | 7571 | `		int nAddr;` |
|      17 | 7572 | `		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);` |
|      17 | 7573 | `		if( nAddr > 0 ){` |
|      17 | 7574 | `			if( !SockSendToAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort) ){` |
|      11 | 7575 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 | 7576 | ``					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);`` |
|       8 | 7577 | `				ph7_result_bool(pCtx,0);` |
|       8 | 7578 | `				return PH7_OK;` |
|       - | 7579 | `			}` |
|      11 | 7580 | `			zSentTo = zAddr;` |
|      11 | 7581 | `			nSentTo = nAddr;` |
|      11 | 7582 | `			bHaveAddr = 1;` |
|       4 | 7583 | `		}` |
|       4 | 7584 | `	}` |
|      21 | 7585 | `	pSock = IoPrivateSocket(pDev);` |
|      21 | 7586 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - | 7587 | `		/* php answers -1 here rather than false: this one reports the send()` |
|       - | 7588 | `		 * result, and it never made a call. */` |
|       3 | 7589 | `		ph7_result_int(pCtx,-1);` |
|       3 | 7590 | `		return PH7_OK;` |
|       - | 7591 | `	}` |
|       - | 7592 | `	/* A zHost of 0 is "no $address at all", which is php's plain send() to the` |
|       - | 7593 | `	 * connected peer; an address whose host half is EMPTY is a name, and the` |
|       - | 7594 | `	 * resolver is what refuses it. */` |
|      19 | 7595 | `	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,bHaveAddr ? zHost : 0,iPort,&iErr);` |
|      19 | 7596 | `	if( iErr == PH7_NET_ERR_RESOLVE ){` |
|       - | 7597 | `		/* php says it three times for one failure — the resolver's own text, the` |
|       - | 7598 | `		 * name it could not resolve, and the address it therefore could not` |
|       - | 7599 | `		 * parse — and answers FALSE rather than the -1 a failed send gives. */` |
|       - | 7600 | `		char zMsg[512];` |
|     ! 0 | 7601 | `		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|     ! 0 | 7602 | ``		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);`` |
|     ! 0 | 7603 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 7604 | ``			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);`` |
|     ! 0 | 7605 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7606 | `		return PH7_OK;` |
|       - | 7607 | `	}` |
|      19 | 7608 | `	if( n < 0 ){` |
|       - | 7609 | `		/* php reports the OS text and hands back the -1 send() answered — this` |
|       - | 7610 | `		 * one never answers false, which is why a caller compares it against 0` |
|       - | 7611 | `		 * rather than testing it for truth. The trailing newline is php's own. */` |
|      11 | 7612 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",` |
|       3 | 7613 | `			PH7_NetStrError(PH7_NetLastError()));` |
|       3 | 7614 | `	}` |
|      19 | 7615 | `	ph7_result_int(pCtx,n);` |
|      19 | 7616 | `	return PH7_OK;` |
|      15 | 7617 | `}` |
|       - | 7618 | `/*` |
|       - | 7619 | ` * array\|false stream_socket_pair(int $domain, int $type, int $protocol)` |
|       - | 7620 | ` *` |
|       - | 7621 | ` * Two connected sockets with no address between them — the two-way pipe a` |
|       - | 7622 | ` * program hands a child, or a test double hands the code under test. Which` |
|       - | 7623 | ` * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses` |
|       - | 7624 | ` * AF_INET, and Windows is the other way round (php emulates the pair over the` |
|       - | 7625 | ` * loopback there, and so does this).` |
|       - | 7626 | ` */` |
|       6 | 7627 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7628 | `{` |
|       - | 7629 | `	ph7_socket aSock[2];` |
|       - | 7630 | `	io_private *apDev[2];` |
|       - | 7631 | `	ph7_value *pArr,*pVal;` |
|       8 | 7632 | `	int iErrno = 0,i;` |
|       8 | 7633 | `	if( nArg < 3 ){` |
|     ! 0 | 7634 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7635 | `		return PH7_OK;` |
|       - | 7636 | `	}` |
|       9 | 7637 | `	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),` |
|      11 | 7638 | `		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){` |
|       - | 7639 | `		/* php reports the OS code and its text, in that order and in brackets. */` |
|       5 | 7640 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",` |
|       1 | 7641 | `			iErrno,PH7_NetStrError(iErrno));` |
|       4 | 7642 | `		ph7_result_bool(pCtx,0);` |
|       4 | 7643 | `		return PH7_OK;` |
|       - | 7644 | `	}` |
|       6 | 7645 | `	pArr = ph7_context_new_array(pCtx);` |
|       6 | 7646 | `	pVal = ph7_context_new_scalar(pCtx);` |
|       6 | 7647 | `	apDev[0] = apDev[1] = 0;` |
|       6 | 7648 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|     ! 0 | 7649 | `		PH7_NetClose(aSock[0]);` |
|     ! 0 | 7650 | `		PH7_NetClose(aSock[1]);` |
|     ! 0 | 7651 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 7652 | `	}` |
|      14 | 7653 | `	for( i = 0 ; i < 2 ; i++ ){` |
|       - | 7654 | `		/* No uri: nothing opened these by name, which is what php reports. */` |
|      10 | 7655 | `		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0,0);` |
|      10 | 7656 | `		if( apDev[i] == 0 ){` |
|       - | 7657 | `			/* SockWrapSocket closed the one it could not wrap; the OTHER end is` |
|       - | 7658 | `			 * still ours to close, wrapped or not. */` |
|     ! 0 | 7659 | `			if( i == 0 ){` |
|     ! 0 | 7660 | `				PH7_NetClose(aSock[1]);` |
|     ! 0 | 7661 | `			}else{` |
|     ! 0 | 7662 | `				SockCloseWrapped(pCtx,apDev[0]);` |
|       - | 7663 | `			}` |
|     ! 0 | 7664 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 7665 | `		}` |
|       - | 7666 | `		/* A pair has no transport of its own, and php labels it apart from a` |
|       - | 7667 | `		 * tcp:// stream for exactly that reason. */` |
|      10 | 7668 | `		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;` |
|      10 | 7669 | `		SockArmDefaultTimeout(pCtx,apDev[i]);` |
|       6 | 7670 | `	}` |
|      14 | 7671 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      10 | 7672 | `		ph7_value_resource(pVal,apDev[i]);` |
|      10 | 7673 | `		ph7_array_add_elem(pArr,0,pVal);` |
|       6 | 7674 | `	}` |
|       6 | 7675 | `	ph7_result_value(pCtx,pArr);` |
|       6 | 7676 | `	return PH7_OK;` |
|       5 | 7677 | `}` |
|       - | 7678 | `/*` |
|       - | 7679 | ` * string\|false stream_socket_get_name(resource $socket, bool $remote)` |
|       - | 7680 | ` *` |
|       - | 7681 | `` * Which address this socket sits on (`$remote` false) or is talking to (true).`` |
|       - | 7682 | `` * It is the only way to learn the port a server bound with `:0` actually got,`` |
|       - | 7683 | ` * so a test that needs a free port had to guess one without it.` |
|       - | 7684 | ` */` |
|      80 | 7685 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 7686 | `{` |
|       - | 7687 | `	io_private *pDev;` |
|       - | 7688 | `	ph7_socket *pSock;` |
|       - | 7689 | `	char zName[128];` |
|       - | 7690 | `	int rc;` |
|      84 | 7691 | `	if( nArg < 2 ){` |
|     ! 0 | 7692 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7693 | `		return PH7_OK;` |
|       - | 7694 | `	}` |
|      84 | 7695 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      84 | 7696 | `	if( pDev == 0 ){` |
|       5 | 7697 | `		return rc;` |
|       - | 7698 | `	}` |
|      80 | 7699 | `	pSock = IoPrivateSocket(pDev);` |
|      80 | 7700 | `	if( pSock == 0 \|\| PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){` |
|       - | 7701 | `		/* php answers false for a stream that is not a socket, and for the peer` |
|       - | 7702 | `		 * of a socket that is not connected — an unaccepted server. */` |
|      20 | 7703 | `		ph7_result_bool(pCtx,0);` |
|      20 | 7704 | `		return PH7_OK;` |
|       - | 7705 | `	}` |
|      64 | 7706 | `	ph7_result_string(pCtx,zName,-1);` |
|      64 | 7707 | `	return PH7_OK;` |
|      44 | 7708 | `}` |
|       - | 7709 | `/*` |
|       - | 7710 | `` * The two ADDRESS converters, `inet_pton()` and `inet_ntop()`, and the host`` |
|       - | 7711 | ``  * name beside them -- three names that were a loud `Call to undefined function` `` |
|       - | 7712 | ` * until now, which is what a program handling an IPv6 literal ran into on its` |
|       - | 7713 | ` * first line. php hands each address string straight to the C library, so what` |
|       - | 7714 | ` * is accepted is the SYSTEM resolver's grammar rather than php's; this engine` |
|       - | 7715 | ` * writes that grammar itself so a Windows build answers what a POSIX one does` |
|       - | 7716 | ` * (the same reason the iconv converter is PHL's own).` |
|       - | 7717 | ` *` |
|       - | 7718 | ` * An IPv4 literal is four decimal octets of 0-255, each written with no` |
|       - | 7719 | `` * LEADING ZERO (`01.2.3.4` is refused, which is what keeps a dotted quad from`` |
|       - | 7720 | ` * ever being read as octal), and nothing before or behind them.` |
|       - | 7721 | ` */` |
|      28 | 7722 | `static int NetPton4(const char *zIn,int nLen,unsigned char *aOut)` |
|       1 | 7723 | `{` |
|      29 | 7724 | `	int iOctet = 0,iVal = 0,bDigit = 0,i;` |
|     225 | 7725 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     209 | 7726 | `		int c = (unsigned char)zIn[i];` |
|     209 | 7727 | `		if( c >= '0' && c <= '9' ){` |
|     137 | 7728 | `			if( bDigit && iVal == 0 ){` |
|       3 | 7729 | `				return 0; /* a leading zero */` |
|       - | 7730 | `			}` |
|     135 | 7731 | `			iVal = iVal * 10 + (c - '0');` |
|     135 | 7732 | `			if( iVal > 255 ){` |
|       3 | 7733 | `				return 0;` |
|       - | 7734 | `			}` |
|     133 | 7735 | `			if( !bDigit ){` |
|      91 | 7736 | `				if( ++iOctet > 4 ){` |
|     ! 0 | 7737 | `					return 0;` |
|       - | 7738 | `				}` |
|      91 | 7739 | `				bDigit = 1;` |
|      45 | 7740 | `			}` |
|     133 | 7741 | `			aOut[iOctet-1] = (unsigned char)iVal;` |
|     139 | 7742 | `		}else if( c == '.' && bDigit ){` |
|      69 | 7743 | `			if( iOctet == 4 ){` |
|       5 | 7744 | `				return 0; /* a fifth octet, or a trailing dot */` |
|       - | 7745 | `			}` |
|      65 | 7746 | `			bDigit = 0;` |
|      65 | 7747 | `			iVal = 0;` |
|      33 | 7748 | `		}else{` |
|       5 | 7749 | `			return 0;` |
|       - | 7750 | `		}` |
|      99 | 7751 | `	}` |
|      17 | 7752 | `	return (iOctet == 4 && bDigit) ? 1 : 0;` |
|      15 | 7753 | `}` |
|     330 | 7754 | `static int NetIsHexDigit(int c)` |
|       1 | 7755 | `{` |
|     331 | 7756 | `	c &= 0xFF;` |
|     331 | 7757 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       1 | 7758 | `}` |
|     206 | 7759 | `static int NetHexDigitVal(int c)` |
|       1 | 7760 | `{` |
|     207 | 7761 | `	c &= 0xFF;` |
|     207 | 7762 | `	if( c >= '0' && c <= '9' ){` |
|     163 | 7763 | `		return c - '0';` |
|       - | 7764 | `	}` |
|      45 | 7765 | `	return (c \| 0x20) - 'a' + 10;` |
|     104 | 7766 | `}` |
|       - | 7767 | `/*` |
|       - | 7768 | ` * An IPv6 literal: up to eight groups of at most four hex digits, at most ONE` |
|       - | 7769 | `` * `::` standing for the run of zero groups that makes the count up to eight,`` |
|       - | 7770 | `` * and an optional dotted quad in place of the last two groups. The `::` is`` |
|       - | 7771 | ` * remembered as the POSITION it stood at and whatever was written after it is` |
|       - | 7772 | ` * slid to the end once the string has been read -- which is what lets one` |
|       - | 7773 | ` * spelling mean a different number of groups depending on what follows it.` |
|       - | 7774 | ` */` |
|      36 | 7775 | `static int NetPton6(const char *zIn,int nLen,unsigned char *aOut)` |
|       1 | 7776 | `{` |
|       - | 7777 | `	unsigned char aTmp[16];` |
|      37 | 7778 | `	const char *zEnd = &zIn[nLen];` |
|       - | 7779 | `	const char *zTok;` |
|      37 | 7780 | `	int iOut = 0,iGap = -1,nDigit = 0,iVal = 0;` |
|      37 | 7781 | `	SyZero(aTmp,(sxu32)sizeof(aTmp));` |
|      37 | 7782 | `	if( zIn < zEnd && zIn[0] == ':' ){` |
|       - | 7783 | ``		/* A single leading colon belongs to a `::` and to nothing else. */`` |
|      15 | 7784 | `		if( &zIn[1] >= zEnd \|\| zIn[1] != ':' ){` |
|     ! 0 | 7785 | `			return 0;` |
|       - | 7786 | `		}` |
|      15 | 7787 | `		zIn++;` |
|       7 | 7788 | `	}` |
|      37 | 7789 | `	zTok = zIn;` |
|     351 | 7790 | `	while( zIn < zEnd ){` |
|     331 | 7791 | `		int c = (unsigned char)zIn[0];` |
|     331 | 7792 | `		zIn++;` |
|     331 | 7793 | `		if( NetIsHexDigit(c) ){` |
|     207 | 7794 | `			iVal = (iVal<<4) \| NetHexDigitVal(c);` |
|     207 | 7795 | `			if( ++nDigit > 4 ){` |
|       3 | 7796 | `				return 0;` |
|       - | 7797 | `			}` |
|     205 | 7798 | `			continue;` |
|       - | 7799 | `		}` |
|     125 | 7800 | `		if( c == ':' ){` |
|     115 | 7801 | `			zTok = zIn;` |
|     115 | 7802 | `			if( nDigit < 1 ){` |
|      29 | 7803 | `				if( iGap >= 0 ){` |
|       3 | 7804 | ``					return 0; /* a second `::` */`` |
|       - | 7805 | `				}` |
|      27 | 7806 | `				iGap = iOut;` |
|      27 | 7807 | `				continue;` |
|       - | 7808 | `			}` |
|      87 | 7809 | `			if( zIn >= zEnd ){` |
|       3 | 7810 | `				return 0; /* a group with a colon and nothing behind it */` |
|       - | 7811 | `			}` |
|      85 | 7812 | `			if( iOut + 2 > (int)sizeof(aTmp) ){` |
|     ! 0 | 7813 | `				return 0;` |
|       - | 7814 | `			}` |
|      85 | 7815 | `			aTmp[iOut++] = (unsigned char)(iVal>>8);` |
|      85 | 7816 | `			aTmp[iOut++] = (unsigned char)(iVal & 0xFF);` |
|      85 | 7817 | `			nDigit = 0;` |
|      85 | 7818 | `			iVal = 0;` |
|      85 | 7819 | `			continue;` |
|       - | 7820 | `		}` |
|      10 | 7821 | `		if( c == '.' && iOut + 4 <= (int)sizeof(aTmp)` |
|       7 | 7822 | `		 && NetPton4(zTok,(int)(zEnd - zTok),&aTmp[iOut]) ){` |
|       - | 7823 | `			/* A dotted quad runs to the END of the string by definition, so` |
|       - | 7824 | `			 * reading it is also the end of the walk. */` |
|       7 | 7825 | `			iOut += 4;` |
|       7 | 7826 | `			nDigit = 0;` |
|       7 | 7827 | `			break;` |
|       - | 7828 | `		}` |
|       5 | 7829 | `		return 0;` |
|     ! 0 | 7830 | `	}` |
|      27 | 7831 | `	if( nDigit > 0 ){` |
|      17 | 7832 | `		if( iOut + 2 > (int)sizeof(aTmp) ){` |
|       3 | 7833 | `			return 0;` |
|       - | 7834 | `		}` |
|      15 | 7835 | `		aTmp[iOut++] = (unsigned char)(iVal>>8);` |
|      15 | 7836 | `		aTmp[iOut++] = (unsigned char)(iVal & 0xFF);` |
|       7 | 7837 | `	}` |
|      25 | 7838 | `	if( iGap >= 0 ){` |
|       - | 7839 | ``		/* Slide everything written after the `::` to the end; what it steps`` |
|       - | 7840 | `		 * over is already zero. An address that is already full has no room` |
|       - | 7841 | `		 * for a gap at all. */` |
|      21 | 7842 | `		int nTail = iOut - iGap,i;` |
|      21 | 7843 | `		if( iOut == (int)sizeof(aTmp) ){` |
|     ! 0 | 7844 | `			return 0;` |
|       - | 7845 | `		}` |
|      85 | 7846 | `		for( i = 1 ; i <= nTail ; ++i ){` |
|      65 | 7847 | `			aTmp[sizeof(aTmp)-i] = aTmp[iGap + nTail - i];` |
|      65 | 7848 | `			aTmp[iGap + nTail - i] = 0;` |
|      33 | 7849 | `		}` |
|      21 | 7850 | `		iOut = (int)sizeof(aTmp);` |
|      10 | 7851 | `	}` |
|      25 | 7852 | `	if( iOut != (int)sizeof(aTmp) ){` |
|     ! 0 | 7853 | `		return 0;` |
|       - | 7854 | `	}` |
|      25 | 7855 | `	SyMemcpy(aTmp,aOut,(sxu32)sizeof(aTmp));` |
|      25 | 7856 | `	return 1;` |
|      19 | 7857 | `}` |
|       - | 7858 | `/* The dotted quad an IPv4 address prints as; zOut holds at least 16 bytes. */` |
|      10 | 7859 | `static void NetNtop4(const unsigned char *aIn,char *zOut,int nOut)` |
|       1 | 7860 | `{` |
|      11 | 7861 | `	SyBufferFormat(zOut,(sxu32)nOut,"%d.%d.%d.%d",aIn[0],aIn[1],aIn[2],aIn[3]);` |
|      11 | 7862 | `}` |
|       - | 7863 | `/*` |
|       - | 7864 | ` * The text an IPv6 address prints as: lower-case hex groups with no leading` |
|       - | 7865 | `` * zeros, the LONGEST run of zero groups written as `::` (two groups at least,`` |
|       - | 7866 | ` * and the FIRST of them when two runs are the same length), and the last four` |
|       - | 7867 | ` * bytes written as a dotted quad for the two IPv4-carrying shapes -- an` |
|       - | 7868 | `` * address that is all zeros but for them, and an `::ffff:` one. zOut holds at`` |
|       - | 7869 | ` * least 46 bytes.` |
|       - | 7870 | ` */` |
|      24 | 7871 | `static void NetNtop6(const unsigned char *aIn,char *zOut,int nOut)` |
|       1 | 7872 | `{` |
|       - | 7873 | `	unsigned int aWord[8];` |
|      25 | 7874 | `	int iBest = -1,nBest = 0,iCur = -1,nCur = 0,i,n = 0;` |
|     217 | 7875 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     193 | 7876 | `		aWord[i] = ((unsigned int)aIn[i*2] << 8) \| aIn[i*2+1];` |
|      97 | 7877 | `	}` |
|     217 | 7878 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     193 | 7879 | `		if( aWord[i] == 0 ){` |
|     111 | 7880 | `			if( iCur < 0 ){` |
|      23 | 7881 | `				iCur = i;` |
|      23 | 7882 | `				nCur = 1;` |
|      12 | 7883 | `			}else{` |
|      89 | 7884 | `				nCur++;` |
|       - | 7885 | `			}` |
|     111 | 7886 | `			if( nCur > nBest ){` |
|     107 | 7887 | `				iBest = iCur;` |
|     107 | 7888 | `				nBest = nCur;` |
|      53 | 7889 | `			}` |
|      56 | 7890 | `		}else{` |
|      83 | 7891 | `			iCur = -1;` |
|      83 | 7892 | `			nCur = 0;` |
|       - | 7893 | `		}` |
|      97 | 7894 | `	}` |
|      25 | 7895 | `	if( nBest < 2 ){` |
|       5 | 7896 | `		iBest = -1;` |
|       2 | 7897 | `	}` |
|     205 | 7898 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     187 | 7899 | `		if( iBest >= 0 && i >= iBest && i < iBest + nBest ){` |
|     107 | 7900 | `			if( i == iBest ){` |
|      21 | 7901 | `				zOut[n++] = ':';` |
|      10 | 7902 | `			}` |
|     107 | 7903 | `			continue;` |
|       - | 7904 | `		}` |
|      81 | 7905 | `		if( i != 0 ){` |
|      69 | 7906 | `			zOut[n++] = ':';` |
|      34 | 7907 | `		}` |
|      80 | 7908 | `		if( i == 6 && iBest == 0` |
|      12 | 7909 | `		 && (nBest == 6 \|\| (nBest == 5 && aWord[5] == 0xFFFF)) ){` |
|       7 | 7910 | `			NetNtop4(&aIn[12],&zOut[n],nOut - n);` |
|       7 | 7911 | `			n += (int)SyStrlen(&zOut[n]);` |
|       7 | 7912 | `			break;` |
|       - | 7913 | `		}` |
|      75 | 7914 | `		n += (int)SyBufferFormat(&zOut[n],(sxu32)(nOut - n),"%x",aWord[i]);` |
|      38 | 7915 | `	}` |
|      25 | 7916 | `	if( iBest >= 0 && iBest + nBest == 8 ){` |
|       5 | 7917 | `		zOut[n++] = ':';` |
|       2 | 7918 | `	}` |
|      25 | 7919 | `	zOut[n] = 0;` |
|      25 | 7920 | `}` |
|       - | 7921 | `/*` |
|       - | 7922 | ` * string\|false inet_pton(string $ip)` |
|       - | 7923 | ` *` |
|       - | 7924 | ` * The packed bytes an address string stands for -- four for IPv4, sixteen for` |
|       - | 7925 | ` * IPv6. php picks the family by LOOKING at the string: a colon anywhere makes` |
|       - | 7926 | ` * it IPv6, and a string with no dot in it is not an address at all. Every` |
|       - | 7927 | ` * refusal is a silent FALSE.` |
|       - | 7928 | ` */` |
|      62 | 7929 | `PH7_PRIVATE int PH7_builtin_inet_pton(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7930 | `{` |
|       - | 7931 | `	unsigned char aAddr[16];` |
|       - | 7932 | `	const char *zIn;` |
|      63 | 7933 | `	int nLen,i,bColon = 0,bDot = 0;` |
|      63 | 7934 | `	if( nArg < 1 ){` |
|     ! 0 | 7935 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7936 | `		return PH7_OK;` |
|       - | 7937 | `	}` |
|      63 | 7938 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     657 | 7939 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     595 | 7940 | `		if( zIn[i] == ':' ){` |
|     133 | 7941 | `			bColon = 1;` |
|     529 | 7942 | `		}else if( zIn[i] == '.' ){` |
|      87 | 7943 | `			bDot = 1;` |
|      43 | 7944 | `		}` |
|     298 | 7945 | `	}` |
|      63 | 7946 | `	if( bColon ){` |
|      37 | 7947 | `		if( !NetPton6(zIn,nLen,aAddr) ){` |
|      13 | 7948 | `			ph7_result_bool(pCtx,0);` |
|      13 | 7949 | `			return PH7_OK;` |
|       - | 7950 | `		}` |
|      25 | 7951 | `		ph7_result_string(pCtx,(const char *)aAddr,16);` |
|      25 | 7952 | `		return PH7_OK;` |
|       - | 7953 | `	}` |
|      27 | 7954 | `	if( !bDot \|\| !NetPton4(zIn,nLen,aAddr) ){` |
|      19 | 7955 | `		ph7_result_bool(pCtx,0);` |
|      19 | 7956 | `		return PH7_OK;` |
|       - | 7957 | `	}` |
|       9 | 7958 | `	ph7_result_string(pCtx,(const char *)aAddr,4);` |
|       9 | 7959 | `	return PH7_OK;` |
|      32 | 7960 | `}` |
|       - | 7961 | `/*` |
|       - | 7962 | ` * string\|false inet_ntop(string $ip)` |
|       - | 7963 | ` *` |
|       - | 7964 | ` * The address string a packed address prints as. Its LENGTH is what names the` |
|       - | 7965 | ` * family -- four bytes or sixteen -- and any other length is a silent FALSE` |
|       - | 7966 | ` * rather than a diagnostic.` |
|       - | 7967 | ` */` |
|      36 | 7968 | `PH7_PRIVATE int PH7_builtin_inet_ntop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7969 | `{` |
|       - | 7970 | `	char zOut[64];` |
|       - | 7971 | `	const char *zIn;` |
|       - | 7972 | `	int nLen;` |
|      37 | 7973 | `	if( nArg < 1 ){` |
|     ! 0 | 7974 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 7975 | `		return PH7_OK;` |
|       - | 7976 | `	}` |
|      37 | 7977 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      37 | 7978 | `	zOut[0] = 0;` |
|      37 | 7979 | `	if( nLen == 4 ){` |
|       5 | 7980 | `		NetNtop4((const unsigned char *)zIn,zOut,(int)sizeof(zOut));` |
|      35 | 7981 | `	}else if( nLen == 16 ){` |
|      25 | 7982 | `		NetNtop6((const unsigned char *)zIn,zOut,(int)sizeof(zOut));` |
|      13 | 7983 | `	}else{` |
|       9 | 7984 | `		ph7_result_bool(pCtx,0);` |
|       9 | 7985 | `		return PH7_OK;` |
|       - | 7986 | `	}` |
|      29 | 7987 | `	ph7_result_string(pCtx,zOut,-1);` |
|      29 | 7988 | `	return PH7_OK;` |
|      19 | 7989 | `}` |
|       - | 7990 | `/*` |
|       - | 7991 | ` * string\|false gethostname()` |
|       - | 7992 | ` *` |
|       - | 7993 | ` * The host's own name, as the OS reports it. php warns and answers false when` |
|       - | 7994 | ` * the call fails, naming the OS code and its text.` |
|       - | 7995 | ` */` |
|       6 | 7996 | `PH7_PRIVATE int PH7_builtin_gethostname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7997 | `{` |
|       - | 7998 | `	char zName[256];` |
|       7 | 7999 | `	int iErrno = 0;` |
|       3 | 8000 | `	SXUNUSED(nArg);` |
|       3 | 8001 | `	SXUNUSED(apArg);` |
|       7 | 8002 | `	zName[0] = 0;` |
|       7 | 8003 | `	if( PH7_NetHostName(zName,(int)sizeof(zName),&iErrno) != PH7_OK ){` |
|     ! 0 | 8004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 8005 | `			"unable to fetch host [%d]: %s",iErrno,PH7_NetStrError(iErrno));` |
|     ! 0 | 8006 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8007 | `		return PH7_OK;` |
|       - | 8008 | `	}` |
|       7 | 8009 | `	ph7_result_string(pCtx,zName,-1);` |
|       7 | 8010 | `	return PH7_OK;` |
|       4 | 8011 | `}` |
|       - | 8012 | `#endif /*` |
|       - | 8013 | ` * The stream SETTINGS family. Every one of these was a loud` |
|       - | 8014 | `` * `Call to undefined function` — so a program that puts a socket in`` |
|       - | 8015 | ` * non-blocking mode, bounds a read with a timeout, or asks whether a stream` |
|       - | 8016 | ` * can be locked before calling flock() did not run at all.` |
|       - | 8017 | ` *` |
|       - | 8018 | ` * The shared preamble: php refuses a non-resource with a TypeError naming the` |
|       - | 8019 | ` * parameter, and an already-closed handle the same way.` |
|       - | 8020 | ` */` |
|   95249 | 8021 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - | 8022 | `	const char *zName,int *pRc)` |
|       5 | 8023 | `{` |
|       - | 8024 | `	char zGiven[64];` |
|       - | 8025 | `	io_private *pDev;` |
|   95254 | 8026 | `	*pRc = PH7_OK;` |
|   95254 | 8027 | `	if( !ph7_value_is_resource(pArg) ){` |
|      77 | 8028 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8029 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      25 | 8030 | `			ph7_function_name(pCtx),iPos,zName,VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|      52 | 8031 | `		return 0;` |
|       - | 8032 | `	}` |
|   95204 | 8033 | `	pDev = (io_private *)ph7_value_to_resource(pArg);` |
|   95204 | 8034 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|     106 | 8035 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8036 | `			"%s(): Argument #%d ($%s) must be an open stream resource",` |
|      35 | 8037 | `			ph7_function_name(pCtx),iPos,zName);` |
|      71 | 8038 | `		return 0;` |
|       - | 8039 | `	}` |
|   95134 | 8040 | `	return pDev;` |
|   46976 | 8041 | `}` |
|       - | 8042 | ``/* The whole settings family names its one handle `$stream`; the copy names two. */`` |
|     164 | 8043 | `static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)` |
|       4 | 8044 | `{` |
|     168 | 8045 | `	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);` |
|       4 | 8046 | `}` |
|       - | 8047 | `/* The same screen, for the filter family in vfs_filter.c. */` |
|   94825 | 8048 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - | 8049 | `	const char *zName,int *pRc)` |
|       5 | 8050 | `{` |
|   94830 | 8051 | `	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);` |
|       5 | 8052 | `}` |
|       - | 8053 | `/* The tcp:// socket behind a handle, or 0 for any other device. */` |
|     521 | 8054 | `static ph7_socket * IoPrivateSocket(io_private *pDev)` |
|       5 | 8055 | `{` |
|       - | 8056 | `#ifdef PH7_ENABLE_NET` |
|     526 | 8057 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|     451 | 8058 | `		return &((sock_private *)pDev->pHandle)->sock;` |
|       - | 8059 | `	}` |
|      78 | 8060 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - | 8061 | `		/* An http body is still a socket, and php's stream_select() waits on` |
|       - | 8062 | `		 * that descriptor exactly as it does for a transport stream. */` |
|     ! 0 | 8063 | `		return PH7_HttpStreamSocket(pDev->pHandle);` |
|       - | 8064 | `	}` |
|       - | 8065 | `#endif` |
|      37 | 8066 | `	SXUNUSED(pDev); /* cc warning when NET is off */` |
|      78 | 8067 | `	return 0;` |
|     266 | 8068 | `}` |
|       - | 8069 | `#ifdef PH7_ENABLE_NET` |
|       - | 8070 | `/*` |
|       - | 8071 | ` * ext/sockets' two doors onto this device stack, and the close that goes with` |
|       - | 8072 | ` * them. php ties a Socket and an exported stream together in BOTH directions --` |
|       - | 8073 | ``  * `socket_export_stream()` twice answers the same handle, and `socket_close()` `` |
|       - | 8074 | ` * on an exported socket leaves the stream closed too -- so the extension needs` |
|       - | 8075 | ` * the descriptor behind a stream, a stream around a descriptor, and a close` |
|       - | 8076 | ` * that runs the device's own teardown rather than the raw closesocket().` |
|       - | 8077 | ` */` |
|       4 | 8078 | `PH7_PRIVATE int PH7_StreamSocketHandle(io_private *pDev,ph7_socket *pOut)` |
|       1 | 8079 | `{` |
|       - | 8080 | `	ph7_socket *pSock;` |
|       5 | 8081 | `	if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) ){` |
|     ! 0 | 8082 | `		return 0;` |
|       - | 8083 | `	}` |
|       5 | 8084 | `	pSock = IoPrivateSocket(PH7_StreamUnwrap(pDev));` |
|       5 | 8085 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       3 | 8086 | `		return 0;` |
|       - | 8087 | `	}` |
|       3 | 8088 | `	*pOut = *pSock;` |
|       3 | 8089 | `	return 1;` |
|       3 | 8090 | `}` |
|       8 | 8091 | `PH7_PRIVATE io_private * PH7_StreamWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|       - | 8092 | `	const char *zLabel,const char *zUri)` |
|       1 | 8093 | `{` |
|      17 | 8094 | `	io_private *pDev = SockWrapSocket(pCtx,sock,bDgram,zUri,` |
|       8 | 8095 | `		zUri ? (int)SyStrlen(zUri) : 0);` |
|       9 | 8096 | `	if( pDev ){` |
|       9 | 8097 | `		((sock_private *)pDev->pHandle)->zLabel = zLabel;` |
|       4 | 8098 | `	}` |
|       9 | 8099 | `	return pDev;` |
|       1 | 8100 | `}` |
|       - | 8101 | `/*` |
|       - | 8102 | ` * The close socket_close() runs on a socket it had exported: the device's own` |
|       - | 8103 | ` * teardown (which closes the descriptor) followed by the same closed-marking` |
|       - | 8104 | ` * fclose() does, so every ph7_value still naming the handle reports it shut` |
|       - | 8105 | ` * rather than reading a freed device.` |
|       - | 8106 | ` */` |
|     ! 0 | 8107 | `PH7_PRIVATE void PH7_StreamCloseExported(io_private *pDev)` |
|     ! 0 | 8108 | `{` |
|     ! 0 | 8109 | `	if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) \|\| pDev->pStream == 0 ){` |
|     ! 0 | 8110 | `		return;` |
|       - | 8111 | `	}` |
|     ! 0 | 8112 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     ! 0 | 8113 | `	PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|     ! 0 | 8114 | `	MarkIOPrivateClosed(pDev);` |
|     ! 0 | 8115 | `}` |
|       - | 8116 | `#endif /* PH7_ENABLE_NET */` |
|       - | 8117 | `/*` |
|       - | 8118 | ` * bool stream_set_blocking(resource $stream, bool $enable)` |
|       - | 8119 | ` *` |
|       - | 8120 | ` * php sets the mode AT the descriptor and answers TRUE either way; a stream` |
|       - | 8121 | ` * with no descriptor — a memory buffer, a data:// payload — keeps reporting` |
|       - | 8122 | ` * itself blocked, which is why the flag is only recorded when it took. On` |
|       - | 8123 | ` * Windows php's plain-files device has no O_NONBLOCK to set, so every such` |
|       - | 8124 | ` * stream (a file, a pipe, php://stdin) answers FALSE there.` |
|       - | 8125 | ` */` |
|      40 | 8126 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 8127 | `{` |
|       - | 8128 | `	io_private *pDev;` |
|       - | 8129 | `	int rc,bEnable,fd;` |
|       - | 8130 | `	ph7_socket *pSock;` |
|      44 | 8131 | `	if( nArg < 2 ){` |
|     ! 0 | 8132 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8133 | `		return PH7_OK;` |
|       - | 8134 | `	}` |
|      44 | 8135 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      44 | 8136 | `	if( pDev == 0 ){` |
|       3 | 8137 | `		return rc;` |
|       - | 8138 | `	}` |
|      42 | 8139 | `	bEnable = ph7_value_to_bool(apArg[1]);` |
|      42 | 8140 | `	pSock = IoPrivateSocket(pDev);` |
|      42 | 8141 | `	if( pSock ){` |
|       - | 8142 | `#ifdef PH7_ENABLE_NET` |
|      13 | 8143 | `		if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - | 8144 | `			/* No socket to set the mode on: php's own answer is FALSE, which is` |
|       - | 8145 | `			 * the one place this family reports a setting that did not take. */` |
|       3 | 8146 | `			ph7_result_bool(pCtx,0);` |
|       3 | 8147 | `			return PH7_OK;` |
|       - | 8148 | `		}` |
|      11 | 8149 | `		PH7_NetSetBlocking(*pSock,bEnable);` |
|       - | 8150 | `#endif` |
|      11 | 8151 | `		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|       7 | 8152 | `	}else{` |
|       - | 8153 | `#ifdef __WINNT__` |
|       1 | 8154 | `		if( PH7_StreamIsPlainDevice(pDev) ){` |
|       1 | 8155 | `			ph7_result_bool(pCtx,0);` |
|       1 | 8156 | `			return PH7_OK;` |
|       - | 8157 | `		}` |
|       - | 8158 | `#endif` |
|      29 | 8159 | `		fd = PH7_StreamPosixFd(pDev);` |
|      28 | 8160 | `		if( fd >= 0 ){` |
|       - | 8161 | `#ifndef __WINNT__` |
|      20 | 8162 | `			int iFlags = fcntl(fd,F_GETFL,0);` |
|      20 | 8163 | `			if( iFlags >= 0 ){` |
|      20 | 8164 | `				if( bEnable ){` |
|       4 | 8165 | `					iFlags &= ~O_NONBLOCK;` |
|       2 | 8166 | `				}else{` |
|      16 | 8167 | `					iFlags \|= O_NONBLOCK;` |
|       - | 8168 | `				}` |
|      20 | 8169 | `				if( fcntl(fd,F_SETFL,iFlags) == 0 ){` |
|      20 | 8170 | `					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      10 | 8171 | `				}` |
|      10 | 8172 | `			}` |
|       - | 8173 | `#endif` |
|      10 | 8174 | `		}` |
|       - | 8175 | `	}` |
|      40 | 8176 | `	ph7_result_bool(pCtx,1);` |
|      40 | 8177 | `	return PH7_OK;` |
|      24 | 8178 | `}` |
|       - | 8179 | `/*` |
|       - | 8180 | ` * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)` |
|       - | 8181 | ` *` |
|       - | 8182 | ` * php answers TRUE only for a stream whose transport HAS a timeout — a socket —` |
|       - | 8183 | ` * and FALSE for every file, pipe and memory buffer, because there is nothing` |
|       - | 8184 | ` * to wait on. Silently accepting it for a file would tell a caller its read is` |
|       - | 8185 | ` * bounded when it is not.` |
|       - | 8186 | ` */` |
|      38 | 8187 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8188 | `{` |
|       - | 8189 | `	io_private *pDev;` |
|       - | 8190 | `	ph7_socket *pSock;` |
|       - | 8191 | `	int rc;` |
|      39 | 8192 | `	if( nArg < 2 ){` |
|     ! 0 | 8193 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8194 | `		return PH7_OK;` |
|       - | 8195 | `	}` |
|      39 | 8196 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      39 | 8197 | `	if( pDev == 0 ){` |
|     ! 0 | 8198 | `		return rc;` |
|       - | 8199 | `	}` |
|      39 | 8200 | `	pSock = IoPrivateSocket(pDev);` |
|      39 | 8201 | `	if( pSock == 0 ){` |
|       7 | 8202 | `		ph7_result_bool(pCtx,0);` |
|       7 | 8203 | `		return PH7_OK;` |
|       - | 8204 | `	}` |
|       - | 8205 | `#ifdef PH7_ENABLE_NET` |
|       - | 8206 | `	{` |
|      32 | 8207 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);` |
|      32 | 8208 | `		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|      32 | 8209 | `		if( iSec < 0 ){` |
|     ! 0 | 8210 | `			iSec = 0;` |
|     ! 0 | 8211 | `		}` |
|      32 | 8212 | `		if( iUsec < 0 ){` |
|     ! 0 | 8213 | `			iUsec = 0;` |
|     ! 0 | 8214 | `		}` |
|      32 | 8215 | `		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);` |
|       - | 8216 | `		/* An expired read answers FALSE and says so through the metadata;` |
|       - | 8217 | `		 * without the armed flag it is indistinguishable from a non-blocking` |
|       - | 8218 | `		 * one, which answers "". */` |
|      32 | 8219 | `		pDev->bHasTimeout = 1;` |
|      32 | 8220 | `		pDev->bTimedOut = 0;` |
|       - | 8221 | `	}` |
|       - | 8222 | `#endif` |
|      32 | 8223 | `	ph7_result_bool(pCtx,1);` |
|      32 | 8224 | `	return PH7_OK;` |
|      20 | 8225 | `}` |
|       - | 8226 | `/*` |
|       - | 8227 | ` * int stream_set_chunk_size(resource $stream, int $size)` |
|       - | 8228 | ` *` |
|       - | 8229 | ` * Answers the PREVIOUS size, which is what makes the setting restorable, and` |
|       - | 8230 | ` * refuses a non-positive one the way php does.` |
|       - | 8231 | ` */` |
|      46 | 8232 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8233 | `{` |
|       - | 8234 | `	io_private *pDev;` |
|       - | 8235 | `	ph7_int64 nSize;` |
|       - | 8236 | `	int rc;` |
|      47 | 8237 | `	if( nArg < 2 ){` |
|     ! 0 | 8238 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 | 8239 | `		return PH7_OK;` |
|       - | 8240 | `	}` |
|      47 | 8241 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      47 | 8242 | `	if( pDev == 0 ){` |
|     ! 0 | 8243 | `		return rc;` |
|       - | 8244 | `	}` |
|      47 | 8245 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|      47 | 8246 | `	if( nSize < 1 ){` |
|       5 | 8247 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 8248 | `			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");` |
|       - | 8249 | `	}` |
|      43 | 8250 | `	if( nSize > (ph7_int64)SXI32_HIGH ){` |
|       - | 8251 | `		/* php's own ceiling: the size is an int on its side, and storing a` |
|       - | 8252 | `		 * larger one made the NEXT call report a size no caller ever set. */` |
|       3 | 8253 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 8254 | `			"stream_set_chunk_size(): Argument #2 ($size) is too large");` |
|       - | 8255 | `	}` |
|      41 | 8256 | `	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);` |
|      41 | 8257 | `	pDev->nChunk = (sxu32)nSize;` |
|      41 | 8258 | `	return PH7_OK;` |
|      24 | 8259 | `}` |
|       - | 8260 | `/*` |
|       - | 8261 | ` * int stream_set_read_buffer(resource $stream, int $size)` |
|       - | 8262 | ` * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]` |
|       - | 8263 | ` *` |
|       - | 8264 | ` * php's stream layer has no stdio buffer left to hand these to: the read side` |
|       - | 8265 | ` * answers 0 (accepted) and the write side -1 (unsupported), for every stream` |
|       - | 8266 | ` * and every size. Both are still validated arguments, so a bad handle is the` |
|       - | 8267 | ` * same TypeError the rest of the family raises.` |
|       - | 8268 | ` */` |
|       6 | 8269 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8270 | `{` |
|       7 | 8271 | `	int rc = PH7_OK;` |
|       7 | 8272 | `	if( nArg < 2 ){` |
|     ! 0 | 8273 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 | 8274 | `		return PH7_OK;` |
|       - | 8275 | `	}` |
|       7 | 8276 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|     ! 0 | 8277 | `		return rc;` |
|       - | 8278 | `	}` |
|       7 | 8279 | `	ph7_result_int(pCtx,0);` |
|       7 | 8280 | `	return PH7_OK;` |
|       4 | 8281 | `}` |
|      12 | 8282 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8283 | `{` |
|      13 | 8284 | `	int rc = PH7_OK;` |
|      13 | 8285 | `	if( nArg < 2 ){` |
|     ! 0 | 8286 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 | 8287 | `		return PH7_OK;` |
|       - | 8288 | `	}` |
|      13 | 8289 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|     ! 0 | 8290 | `		return rc;` |
|       - | 8291 | `	}` |
|      13 | 8292 | `	ph7_result_int(pCtx,-1);` |
|      13 | 8293 | `	return PH7_OK;` |
|       7 | 8294 | `}` |
|       - | 8295 | `/*` |
|       - | 8296 | ` * int\|false stream_copy_to_stream(resource $from, resource $to,` |
|       - | 8297 | ` *                                 ?int $length = null, int $offset = 0)` |
|       - | 8298 | ` *` |
|       - | 8299 | ` * The everyday way to move bytes between two open streams, and a loud` |
|       - | 8300 | `` * `Call to undefined function` here until now — so the workaround was`` |
|       - | 8301 | `` * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into`` |
|       - | 8302 | ` * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset` |
|       - | 8303 | ` * seeks the source first and is php's only failure shape short of a broken` |
|       - | 8304 | ` * write.` |
|       - | 8305 | ` */` |
|      38 | 8306 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8307 | `{` |
|       - | 8308 | `	io_private *pFrom,*pTo;` |
|      39 | 8309 | `	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;` |
|       - | 8310 | `	char zBuf[8192];` |
|       - | 8311 | `	int rc;` |
|      39 | 8312 | `	if( nArg < 2 ){` |
|     ! 0 | 8313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8314 | `		return PH7_OK;` |
|       - | 8315 | `	}` |
|      39 | 8316 | `	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);` |
|      39 | 8317 | `	if( pFrom == 0 ){` |
|       3 | 8318 | `		return rc;` |
|       - | 8319 | `	}` |
|      37 | 8320 | `	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);` |
|      37 | 8321 | `	if( pTo == 0 ){` |
|       3 | 8322 | `		return rc;` |
|       - | 8323 | `	}` |
|      34 | 8324 | `	if( pFrom->pStream == 0 \|\| pFrom->pStream->xRead == 0` |
|      35 | 8325 | `	 \|\| pTo->pStream == 0 \|\| pTo->pStream->xWrite == 0 ){` |
|     ! 0 | 8326 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8327 | `		return PH7_OK;` |
|       - | 8328 | `	}` |
|      35 | 8329 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      11 | 8330 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|       5 | 8331 | `	}` |
|      35 | 8332 | `	if( nArg > 3 ){` |
|      19 | 8333 | `		nOfft = ph7_value_to_int64(apArg[3]);` |
|       9 | 8334 | `	}` |
|      35 | 8335 | `	if( nOfft > 0 ){` |
|       - | 8336 | `		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has` |
|       - | 8337 | `		 * no position to move to, and silently copying from wherever it` |
|       - | 8338 | `		 * happens to be would answer for a different slice of the stream. */` |
|       8 | 8339 | `		if( pFrom->pStream->xSeek == 0` |
|       8 | 8340 | `		 \|\| pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){` |
|       2 | 8341 | `			if( pFrom->pStream->xSeek == 0 ){` |
|       2 | 8342 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|       - | 8343 | `					"stream_copy_to_stream(): Stream does not support seeking");` |
|       1 | 8344 | `			}` |
|       3 | 8345 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       1 | 8346 | `				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);` |
|       2 | 8347 | `			ph7_result_bool(pCtx,0);` |
|       2 | 8348 | `			return PH7_OK;` |
|       - | 8349 | `		}` |
|       7 | 8350 | `		ResetIOPrivate(pFrom);` |
|       3 | 8351 | `	}` |
|      33 | 8352 | `	if( nWant == 0 ){` |
|       3 | 8353 | `		ph7_result_int(pCtx,0);` |
|       3 | 8354 | `		return PH7_OK;` |
|       - | 8355 | `	}` |
|       - | 8356 | `#ifdef __WINNT__` |
|       - | 8357 | `	/* An unfiltered plain-file source goes through php's memory-mapped copy,` |
|       - | 8358 | `	 * whose Windows view at the end of the file is a failure: see` |
|       - | 8359 | `	 * PH7_WinFileMapsEmptyView(). */` |
|       - | 8360 | `	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0` |
|       1 | 8361 | `	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft` |
|       - | 8362 | `			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){` |
|       1 | 8363 | `		ph7_result_bool(pCtx,0);` |
|       1 | 8364 | `		return PH7_OK;` |
|       - | 8365 | `	}` |
|       - | 8366 | `#endif` |
|       - | 8367 | `	/* The destination may be sitting past its own read-ahead; the write has to` |
|       - | 8368 | `	 * land where the SCRIPT is, the rule fwrite() follows. */` |
|      31 | 8369 | `	StreamSeekBackForWrite(pTo);` |
|      39 | 8370 | `	for(;;){` |
|      55 | 8371 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|       - | 8372 | `		ph7_int64 nRead,nWr;` |
|      55 | 8373 | `		if( nWant > 0 && nWant - nTotal < nAsk ){` |
|      13 | 8374 | `			nAsk = nWant - nTotal;` |
|       6 | 8375 | `		}` |
|      55 | 8376 | `		if( nAsk < 1 ){` |
|       5 | 8377 | `			break;` |
|       - | 8378 | `		}` |
|      51 | 8379 | `		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);` |
|      51 | 8380 | `		if( nRead < 1 ){` |
|      27 | 8381 | `			break;` |
|       - | 8382 | `		}` |
|      25 | 8383 | `		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);` |
|      25 | 8384 | `		if( nWr < 0 ){` |
|     ! 0 | 8385 | `			SockReportWriteFailure(pCtx,pTo,(int)nRead);` |
|     ! 0 | 8386 | `			break;` |
|       - | 8387 | `		}` |
|      25 | 8388 | `		nTotal += nWr;` |
|      25 | 8389 | `		if( nWr < nRead ){` |
|     ! 0 | 8390 | `			break;` |
|       - | 8391 | `		}` |
|       1 | 8392 | `	}` |
|      31 | 8393 | `	ph7_result_int64(pCtx,nTotal);` |
|      31 | 8394 | `	return PH7_OK;` |
|      20 | 8395 | `}` |
|       - | 8396 | `/*` |
|       - | 8397 | ` * int\|false stream_select(?array &$read, ?array &$write, ?array &$except,` |
|       - | 8398 | ` *                         ?int $seconds, ?int $microseconds = null)` |
|       - | 8399 | ` *` |
|       - | 8400 | ` * The name that makes a program WAIT on several streams at once, and the reason` |
|       - | 8401 | ` * the settings family that shipped beside it had nothing to wait with: a` |
|       - | 8402 | ` * non-blocking read tells you a stream is not ready, and only this tells you` |
|       - | 8403 | ` * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server` |
|       - | 8404 | ` * loop and every event loop written in php is built on, and it was a loud` |
|       - | 8405 | `` * `Call to undefined function`.`` |
|       - | 8406 | ` *` |
|       - | 8407 | ` * php's own shape, and the parts of it a re-derivation misses: the arrays are` |
|       - | 8408 | ` * REWRITTEN in place to hold only the ready entries, under their original keys;` |
|       - | 8409 | ` * a stream that cannot be represented as a descriptor is a warning naming its` |
|       - | 8410 | ` * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and` |
|       - | 8411 | ` * a stream whose own read buffer still holds bytes is answered READY without` |
|       - | 8412 | ` * asking the OS at all — which is the difference between a loop that drains a` |
|       - | 8413 | ` * buffered handle and one that waits forever for data it has already read.` |
|       - | 8414 | ` */` |
|       - | 8415 | `#if !defined(__WINNT__) \|\| defined(PH7_ENABLE_NET)` |
|       - | 8416 | `#define STREAM_SELECT_OK 1` |
|       - | 8417 | `#ifdef __UNIXES__` |
|       - | 8418 | `#include <sys/select.h>` |
|       - | 8419 | `#include <sys/time.h>` |
|       - | 8420 | `#endif` |
|       - | 8421 | `#endif` |
|       - | 8422 | `#define SEL_READ   0` |
|       - | 8423 | `#define SEL_WRITE  1` |
|       - | 8424 | `#define SEL_EXCEPT 2` |
|       - | 8425 | `/* What one walk over an argument is for. The order matters: php COUNTS the` |
|       - | 8426 | ` * already-buffered readable handles before it waits, and only rewrites the` |
|       - | 8427 | ` * arrays once it knows which answer it is giving. */` |
|       - | 8428 | `#define SELM_COLLECT  0 /* put every representable handle in its fd_set */` |
|       - | 8429 | `#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */` |
|       - | 8430 | `#define SELM_READY    2 /* keep the handles select() reported */` |
|       - | 8431 | `#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */` |
|       - | 8432 | `typedef struct stream_select_ctx stream_select_ctx;` |
|       - | 8433 | `struct stream_select_ctx` |
|       - | 8434 | `{` |
|       - | 8435 | `	ph7_context *pCtx;` |
|       - | 8436 | `#ifdef STREAM_SELECT_OK` |
|       - | 8437 | `	fd_set aSet[3];    /* read / write / except, as select() takes them */` |
|       - | 8438 | `#endif` |
|       - | 8439 | `	int iMaxFd;` |
|       - | 8440 | `	int nSelectable;   /* entries that could be represented at all */` |
|       - | 8441 | `	int iWhich;        /* the set being walked (SEL_*) */` |
|       - | 8442 | `	int iMode;         /* SELM_*: what this walk is FOR */` |
|       - | 8443 | `	int nReady;` |
|       - | 8444 | `	int bBadEntry;     /* an entry that is not a stream at all */` |
|       - | 8445 | `	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */` |
|       - | 8446 | `	ph7_value *pOut;   /* the rebuilt array, while harvesting */` |
|       - | 8447 | `};` |
|       - | 8448 | `/*` |
|       - | 8449 | ` * What a select can WAIT on for this handle: the POSIX descriptor, or the` |
|       - | 8450 | ` * SOCKET, which is the only waitable thing a stream carries on Windows (the` |
|       - | 8451 | ` * file devices hold a HANDLE there, and select() cannot take one — a recorded` |
|       - | 8452 | ` * platform difference, §7.4). Answers -1 for a device with neither: a memory` |
|       - | 8453 | ` * buffer, a data:// payload, a userland wrapper.` |
|       - | 8454 | ` */` |
|      90 | 8455 | `static ph7_int64 IoPrivateSelectHandle(io_private *pDev)` |
|       1 | 8456 | `{` |
|       - | 8457 | `	int fd;` |
|       - | 8458 | `#ifdef PH7_ENABLE_NET` |
|      91 | 8459 | `	ph7_socket *pSock = IoPrivateSocket(pDev);` |
|      91 | 8460 | `	if( pSock ){` |
|      65 | 8461 | `		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;` |
|       - | 8462 | `	}` |
|       - | 8463 | `#endif` |
|      27 | 8464 | `	fd = PH7_StreamPosixFd(pDev);` |
|      27 | 8465 | `	return fd < 0 ? -1 : (ph7_int64)fd;` |
|      46 | 8466 | `}` |
|       - | 8467 | `/* Bytes this handle has already pulled off the device and not yet handed over. */` |
|      42 | 8468 | `static sxu32 IoPrivateUnread(io_private *pDev)` |
|       1 | 8469 | `{` |
|      43 | 8470 | `	return StreamAheadBytes(pDev);` |
|       1 | 8471 | `}` |
|      50 | 8472 | `static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)` |
|       1 | 8473 | `{` |
|       - | 8474 | `#ifdef STREAM_SELECT_OK` |
|       - | 8475 | `#ifdef __WINNT__` |
|       - | 8476 | `	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many` |
|       - | 8477 | `	 * are in it already rather than the value of this one. */` |
|       1 | 8478 | `	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){` |
|     ! 0 | 8479 | `		return;` |
|       - | 8480 | `	}` |
|       1 | 8481 | `	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);` |
|       - | 8482 | `#else` |
|      50 | 8483 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|       - | 8484 | `		/* php ignores a descriptor an fd_set cannot hold (its own` |
|       - | 8485 | `		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */` |
|     ! 0 | 8486 | `		return;` |
|       - | 8487 | `	}` |
|      50 | 8488 | `	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);` |
|       - | 8489 | `#endif` |
|      51 | 8490 | `	if( h > (ph7_int64)pSel->iMaxFd ){` |
|      41 | 8491 | `		pSel->iMaxFd = (int)h;` |
|      20 | 8492 | `	}` |
|       - | 8493 | `#else` |
|       - | 8494 | `	SXUNUSED(pSel);` |
|       - | 8495 | `	SXUNUSED(h);` |
|       - | 8496 | `#endif` |
|      26 | 8497 | `}` |
|      32 | 8498 | `static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)` |
|       1 | 8499 | `{` |
|       - | 8500 | `#ifdef STREAM_SELECT_OK` |
|       - | 8501 | `#ifdef __WINNT__` |
|       1 | 8502 | `	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|       - | 8503 | `#else` |
|      32 | 8504 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|     ! 0 | 8505 | `		return 0;` |
|       - | 8506 | `	}` |
|      32 | 8507 | `	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|       - | 8508 | `#endif` |
|       - | 8509 | `#else` |
|       - | 8510 | `	SXUNUSED(pSel);` |
|       - | 8511 | `	SXUNUSED(h);` |
|       - | 8512 | `	return 0;` |
|       - | 8513 | `#endif` |
|      17 | 8514 | `}` |
|       - | 8515 | `/*` |
|       - | 8516 | ` * One entry of one array: collected on the way in, harvested on the way out.` |
|       - | 8517 | ` * php never stops for an entry it cannot use — the diagnostics are remembered` |
|       - | 8518 | ` * and raised once the whole set is known, because whether the array held` |
|       - | 8519 | ` * ANYTHING selectable decides which of them php raises.` |
|       - | 8520 | ` */` |
|     140 | 8521 | `static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       1 | 8522 | `{` |
|     141 | 8523 | `	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;` |
|       - | 8524 | `	io_private *pDev;` |
|       - | 8525 | `	ph7_int64 h;` |
|     141 | 8526 | `	if( !ph7_value_is_resource(pValue) ){` |
|       7 | 8527 | `		pSel->bBadEntry = 1;` |
|       - | 8528 | `		/* php words a value that is not a resource apart from a resource that is` |
|       - | 8529 | `		 * no longer open, and raises one per bad entry — so the LAST one seen is` |
|       - | 8530 | `		 * the message that reaches the caller. */` |
|       7 | 8531 | `		pSel->bBadClosed = 0;` |
|       7 | 8532 | `		return PH7_OK;` |
|       - | 8533 | `	}` |
|     135 | 8534 | `	pDev = (io_private *)ph7_value_to_resource(pValue);` |
|     135 | 8535 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       3 | 8536 | `		pSel->bBadEntry = pSel->bBadClosed = 1;` |
|       3 | 8537 | `		return PH7_OK;` |
|       - | 8538 | `	}` |
|     133 | 8539 | `	if( pSel->iMode == SELM_BUFFERED ){` |
|       - | 8540 | `		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a` |
|       - | 8541 | `		 * readable stream with no descriptor at all take part (a userland` |
|       - | 8542 | `		 * wrapper a line read has filled the buffer of), and answering 0 for one` |
|       - | 8543 | `		 * would sleep out the whole timeout over bytes the script already has. */` |
|      43 | 8544 | `		if( IoPrivateUnread(pDev) > 0 ){` |
|       9 | 8545 | `			if( pSel->pOut ){` |
|       5 | 8546 | `				ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|       2 | 8547 | `			}` |
|       9 | 8548 | `			pSel->nReady++;` |
|       4 | 8549 | `		}` |
|      43 | 8550 | `		return PH7_OK;` |
|       - | 8551 | `	}` |
|      91 | 8552 | `	h = IoPrivateSelectHandle(pDev);` |
|      91 | 8553 | `	if( h < 0 ){` |
|       7 | 8554 | `		if( pSel->iMode == SELM_COLLECT ){` |
|       - | 8555 | `			const char *zWrapper,*zLabel;` |
|       5 | 8556 | `			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|       7 | 8557 | `			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,` |
|       2 | 8558 | `				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);` |
|       2 | 8559 | `		}` |
|       7 | 8560 | `		return PH7_OK;` |
|       - | 8561 | `	}` |
|      85 | 8562 | `	if( pSel->iMode == SELM_COLLECT ){` |
|      51 | 8563 | `		pSel->nSelectable++;` |
|      51 | 8564 | `		StreamSelectAdd(pSel,h);` |
|      51 | 8565 | `		return PH7_OK;` |
|       - | 8566 | `	}` |
|      35 | 8567 | `	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){` |
|      23 | 8568 | `		if( pSel->pOut ){` |
|      23 | 8569 | `			ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|      11 | 8570 | `		}` |
|      23 | 8571 | `		pSel->nReady++;` |
|      11 | 8572 | `	}` |
|      35 | 8573 | `	return PH7_OK;` |
|      71 | 8574 | `}` |
|       - | 8575 | `/* The wait itself, over the pair the caller's numbers were normalised into. */` |
|      24 | 8576 | `static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,` |
|       - | 8577 | `	int *pErrno)` |
|       1 | 8578 | `{` |
|       - | 8579 | `#ifdef STREAM_SELECT_OK` |
|      25 | 8580 | `	struct timeval tv,*pTv = 0;` |
|       - | 8581 | `	int rc;` |
|      25 | 8582 | `	if( !bBlock ){` |
|      25 | 8583 | `		tv.tv_sec = (long)iSec;` |
|      25 | 8584 | `		tv.tv_usec = (long)iUsec;` |
|      25 | 8585 | `		pTv = &tv;` |
|      12 | 8586 | `	}` |
|      37 | 8587 | `	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],` |
|      12 | 8588 | `		&pSel->aSet[SEL_EXCEPT],pTv);` |
|      25 | 8589 | `	if( rc < 0 && pErrno ){` |
|       - | 8590 | `#ifdef __WINNT__` |
|     ! 0 | 8591 | `		*pErrno = WSAGetLastError();` |
|       - | 8592 | `#else` |
|     ! 0 | 8593 | `		*pErrno = errno;` |
|       - | 8594 | `#endif` |
|     ! 0 | 8595 | `	}` |
|      25 | 8596 | `	return rc;` |
|       - | 8597 | `#else` |
|       - | 8598 | `	/* No select() to call: a Windows build with no socket layer. */` |
|       - | 8599 | `	SXUNUSED(pSel);` |
|       - | 8600 | `	SXUNUSED(iSec);` |
|       - | 8601 | `	SXUNUSED(iUsec);` |
|       - | 8602 | `	SXUNUSED(bBlock);` |
|       - | 8603 | `	if( pErrno ){ *pErrno = 0; }` |
|       - | 8604 | `	return -1;` |
|       - | 8605 | `#endif` |
|       1 | 8606 | `}` |
|       - | 8607 | `/* Walk one of the three arguments, if it IS one. */` |
|     200 | 8608 | `static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|       - | 8609 | `	int iMode)` |
|       1 | 8610 | `{` |
|     201 | 8611 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|      89 | 8612 | `		return;` |
|       - | 8613 | `	}` |
|     113 | 8614 | `	pSel->iWhich = iWhich;` |
|     113 | 8615 | `	pSel->iMode = iMode;` |
|     113 | 8616 | `	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);` |
|     101 | 8617 | `}` |
|       - | 8618 | `/* Rebuild one argument from the entries that came back ready. php REPLACES the` |
|       - | 8619 | ` * array either way, so a set with nothing ready comes back empty. */` |
|      84 | 8620 | `static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|       - | 8621 | `	int iMode)` |
|       1 | 8622 | `{` |
|      85 | 8623 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|      51 | 8624 | `		return PH7_OK;` |
|       - | 8625 | `	}` |
|      35 | 8626 | `	pSel->pOut = ph7_context_new_array(pSel->pCtx);` |
|      35 | 8627 | `	if( pSel->pOut == 0 ){` |
|       - | 8628 | `		/* Leaving the caller's array alone would answer that every entry is` |
|       - | 8629 | `		 * ready, which is the one wrong answer this function must not give. */` |
|     ! 0 | 8630 | `		return PH7_ContextMemoryError(pSel->pCtx);` |
|       - | 8631 | `	}` |
|      35 | 8632 | `	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);` |
|      35 | 8633 | `	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);` |
|      35 | 8634 | `	pSel->pOut = 0;` |
|      35 | 8635 | `	return PH7_OK;` |
|      43 | 8636 | `}` |
|      46 | 8637 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8638 | `{` |
|       - | 8639 | `	stream_select_ctx sSel;` |
|      47 | 8640 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|      47 | 8641 | `	int bBlock = 1,iErrno = 0,rc,i;` |
|      47 | 8642 | `	SyZero(&sSel,sizeof(sSel));` |
|      47 | 8643 | `	sSel.pCtx = pCtx;` |
|      47 | 8644 | `	sSel.iMaxFd = -1;` |
|       - | 8645 | `#ifdef STREAM_SELECT_OK` |
|     185 | 8646 | `	for( i = 0 ; i < 3 ; i++ ){` |
|    1243 | 8647 | `		FD_ZERO(&sSel.aSet[i]);` |
|      70 | 8648 | `	}` |
|       - | 8649 | `#endif` |
|      47 | 8650 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);` |
|      47 | 8651 | `	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);` |
|      47 | 8652 | `	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);` |
|      47 | 8653 | `	if( sSel.nSelectable < 1 ){` |
|       - | 8654 | `		/* php's own wording, and it carries no function name. It is the answer` |
|       - | 8655 | `		 * for three NULLs, for empty arrays, and for arrays holding nothing` |
|       - | 8656 | `		 * this engine can wait on — the caller asked to wait for nothing. */` |
|       7 | 8657 | `		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");` |
|       - | 8658 | `	}` |
|      41 | 8659 | `	if( sSel.bBadEntry ){` |
|       - | 8660 | `		/* Raised only once the arrays are known to hold something to wait on —` |
|       - | 8661 | `		 * the empty-arrays Error wins over it — and BEFORE the timeout is` |
|       - | 8662 | `		 * looked at, which is the order php's own pending-exception check` |
|       - | 8663 | `		 * produces. */` |
|      10 | 8664 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8665 | `			"%s(): supplied %s is not a valid stream resource",` |
|       6 | 8666 | `			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");` |
|       - | 8667 | `	}` |
|      35 | 8668 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      33 | 8669 | `		iSec = ph7_value_to_int64(apArg[3]);` |
|      33 | 8670 | `		bBlock = 0;` |
|      33 | 8671 | `		if( iSec < 0 ){` |
|       4 | 8672 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 8673 | `				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",` |
|       1 | 8674 | `				ph7_function_name(pCtx));` |
|       - | 8675 | `		}` |
|      15 | 8676 | `	}` |
|      33 | 8677 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      15 | 8678 | `		iUsec = ph7_value_to_int64(apArg[4]);` |
|      15 | 8679 | `		if( bBlock ){` |
|       - | 8680 | `			/* php refuses the pair rather than guessing which one meant it: a` |
|       - | 8681 | `			 * NULL $seconds is "wait forever", and there is no such thing as` |
|       - | 8682 | `			 * waiting forever for five microseconds. */` |
|       3 | 8683 | `			if( iUsec != 0 ){` |
|       4 | 8684 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 8685 | `					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",` |
|       1 | 8686 | `					ph7_function_name(pCtx));` |
|     ! 0 | 8687 | `			}` |
|      13 | 8688 | `		}else if( iUsec < 0 ){` |
|       4 | 8689 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 8690 | `				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",` |
|       1 | 8691 | `				ph7_function_name(pCtx));` |
|       - | 8692 | `		}` |
|       5 | 8693 | `	}` |
|      29 | 8694 | `	if( iUsec > 999999 ){` |
|       - | 8695 | `		/* php carries the overflow into the seconds, because a tv_usec of a` |
|       - | 8696 | `		 * million or more is what Solaris and the BSDs refuse outright — so` |
|       - | 8697 | ``		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half`` |
|       - | 8698 | `		 * rather than failing. */` |
|       3 | 8699 | `		iSec += iUsec / 1000000;` |
|       3 | 8700 | `		iUsec %= 1000000;` |
|       1 | 8701 | `	}` |
|       - | 8702 | `	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer` |
|       - | 8703 | `	 * still holds bytes the script has not taken is ready NOW, whatever the OS` |
|       - | 8704 | `	 * would say about its descriptor — the device has nothing left to report.` |
|       - | 8705 | `	 * COUNTED first and stored second, because the count is what decides` |
|       - | 8706 | `	 * whether the arrays are rewritten from the buffers or from the wait. */` |
|      29 | 8707 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);` |
|      29 | 8708 | `	if( sSel.nReady > 0 ){` |
|       5 | 8709 | `		sSel.nReady = 0;` |
|       4 | 8710 | `		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK` |
|       - | 8711 | `		/* php answers only the readable ones then, and empties the other two. */` |
|       4 | 8712 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK` |
|       5 | 8713 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){` |
|     ! 0 | 8714 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 8715 | `		}` |
|       5 | 8716 | `		ph7_result_int(pCtx,sSel.nReady);` |
|       5 | 8717 | `		return PH7_OK;` |
|       - | 8718 | `	}` |
|      25 | 8719 | `	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);` |
|      25 | 8720 | `	if( rc < 0 ){` |
|       - | 8721 | `#if defined(__WINNT__) && defined(PH7_ENABLE_NET)` |
|     ! 0 | 8722 | `		const char *zErr = PH7_NetStrError(iErrno);` |
|       - | 8723 | `#else` |
|     ! 0 | 8724 | `		const char *zErr = VfsStrerror(iErrno);` |
|       - | 8725 | `#endif` |
|     ! 0 | 8726 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",` |
|     ! 0 | 8727 | `			iErrno,zErr,sSel.iMaxFd);` |
|     ! 0 | 8728 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8729 | `		return PH7_OK;` |
|       - | 8730 | `	}` |
|      24 | 8731 | `	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK` |
|      24 | 8732 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK` |
|      25 | 8733 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){` |
|     ! 0 | 8734 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 8735 | `	}` |
|       - | 8736 | `	/* The COUNT is select()'s own, not the entries kept: two array members can` |
|       - | 8737 | `	 * name one descriptor, and php answers what the OS said. */` |
|      25 | 8738 | `	ph7_result_int(pCtx,rc);` |
|      25 | 8739 | `	return PH7_OK;` |
|      24 | 8740 | `}` |
|       - | 8741 | `/*` |
|       - | 8742 | ` * array stream_get_transports(void)` |
|       - | 8743 | ` *` |
|       - | 8744 | ` * The transports a stream_socket_client()/fsockopen() address may name. php's` |
|       - | 8745 | ` * own list is what its build registered, so this is what THIS engine can open,` |
|       - | 8746 | ` * in php's own registration order: the ssl/tls/unix set is a recorded scope gap` |
|       - | 8747 | ` * (§7.4), and answering for transports that are not there would tell a script a` |
|       - | 8748 | ` * connection will work when it cannot.` |
|       - | 8749 | ` */` |
|      10 | 8750 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 8751 | `{` |
|       - | 8752 | `	ph7_value *pArr,*pV;` |
|       5 | 8753 | `	SXUNUSED(nArg);` |
|       5 | 8754 | `	SXUNUSED(apArg);` |
|      12 | 8755 | `	pArr = ph7_context_new_array(pCtx);` |
|      12 | 8756 | `	pV = ph7_context_new_scalar(pCtx);` |
|      12 | 8757 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 | 8758 | `		ph7_result_null(pCtx);` |
|     ! 0 | 8759 | `		return PH7_OK;` |
|       - | 8760 | `	}` |
|       - | 8761 | `#ifdef PH7_ENABLE_NET` |
|      12 | 8762 | `	ph7_value_string(pV,"tcp",-1);` |
|      12 | 8763 | `	ph7_array_add_elem(pArr,0,pV);` |
|      12 | 8764 | `	ph7_value_reset_string_cursor(pV);` |
|      12 | 8765 | `	ph7_value_string(pV,"udp",-1);` |
|      12 | 8766 | `	ph7_array_add_elem(pArr,0,pV);` |
|       - | 8767 | `#endif` |
|      12 | 8768 | `	ph7_result_value(pCtx,pArr);` |
|      12 | 8769 | `	return PH7_OK;` |
|       7 | 8770 | `}` |
|       - | 8771 | `/*` |
|       - | 8772 | ` * bool stream_supports_lock(resource $stream)` |
|       - | 8773 | ` *` |
|       - | 8774 | ` * The question flock() answers with a warning if you get it wrong: only a` |
|       - | 8775 | ` * device with a real lock operation can be locked, so a memory buffer and a` |
|       - | 8776 | ` * data:// payload are false.` |
|       - | 8777 | ` */` |
|      14 | 8778 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8779 | `{` |
|       - | 8780 | `	io_private *pDev;` |
|       - | 8781 | `	int rc;` |
|      15 | 8782 | `	if( nArg < 1 ){` |
|     ! 0 | 8783 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8784 | `		return PH7_OK;` |
|       - | 8785 | `	}` |
|      15 | 8786 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      15 | 8787 | `	if( pDev == 0 ){` |
|       3 | 8788 | `		return rc;` |
|       - | 8789 | `	}` |
|       - | 8790 | `	/* php locks at the DESCRIPTOR, so anything with one can be locked even` |
|       - | 8791 | `	 * when the device exposes no lock operation of its own (php://stdout, a` |
|       - | 8792 | `	 * pipe); a memory buffer and a data:// payload have neither and are the` |
|       - | 8793 | `	 * false answers. */` |
|      13 | 8794 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      23 | 8795 | `	ph7_result_bool(pCtx,pDev->bDir == 0` |
|      16 | 8796 | `		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)` |
|       7 | 8797 | `		    \|\| PH7_StreamPosixFd(pDev) >= 0));` |
|      13 | 8798 | `	return PH7_OK;` |
|       8 | 8799 | `}` |
|       - | 8800 | `/*` |
|       - | 8801 | ` * bool stream_is_local(resource\|string $stream)` |
|       - | 8802 | ` *` |
|       - | 8803 | ` * php answers from the WRAPPER, not from the path: a stream opened by a URL` |
|       - | 8804 | ` * wrapper is not local, one opened by no wrapper at all (a pipe) is not local` |
|       - | 8805 | ` * either, and everything else — including php:// and a path naming a scheme` |
|       - | 8806 | ` * nobody registered — is.` |
|       - | 8807 | ` */` |
|      28 | 8808 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8809 | `{` |
|       - | 8810 | `	const ph7_io_stream *pStream;` |
|      29 | 8811 | `	if( nArg < 1 ){` |
|     ! 0 | 8812 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8813 | `		return PH7_OK;` |
|       - | 8814 | `	}` |
|      29 | 8815 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 8816 | `		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };` |
|       - | 8817 | `		int nLen,i;` |
|      21 | 8818 | `		const char *zPath = ph7_value_to_string(apArg[0],&nLen);` |
|      20 | 8819 | `		if( nLen > (int)sizeof("file://")-1` |
|      19 | 8820 | `		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0` |
|      11 | 8821 | `		 && zPath[sizeof("file://")-1] != '/'` |
|       4 | 8822 | `		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){` |
|       - | 8823 | ``			/* `file://host/path` names a REMOTE host, which php refuses rather`` |
|       - | 8824 | `			 * than reading as a local path — so the answer is not local. */` |
|       3 | 8825 | `			ph7_result_bool(pCtx,0);` |
|       3 | 8826 | `			return PH7_OK;` |
|       - | 8827 | `		}` |
|      83 | 8828 | `		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){` |
|      67 | 8829 | `			int nScheme = (int)SyStrlen(azUrlScheme[i]);` |
|      67 | 8830 | `			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){` |
|       - | 8831 | `				/* php registers these as URL wrappers whether or not this` |
|       - | 8832 | `				 * engine can OPEN them (http:// is a recorded gap, §7.4), and` |
|       - | 8833 | `				 * "is this path local?" has to answer for the scheme rather` |
|       - | 8834 | `				 * than for what happens to be implemented — the unsafe` |
|       - | 8835 | `				 * direction is answering TRUE about a remote URL. */` |
|       3 | 8836 | `				ph7_result_bool(pCtx,0);` |
|       3 | 8837 | `				return PH7_OK;` |
|       - | 8838 | `			}` |
|      33 | 8839 | `		}` |
|      17 | 8840 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);` |
|       - | 8841 | `		/* An unregistered scheme has no wrapper to ask, and php answers TRUE` |
|       - | 8842 | `		 * for it — the path is taken at face value. */` |
|      17 | 8843 | `		ph7_result_bool(pCtx,pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream));` |
|      17 | 8844 | `		return PH7_OK;` |
|       - | 8845 | `	}` |
|       - | 8846 | `	{` |
|       - | 8847 | `		int rc;` |
|       9 | 8848 | `		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|       - | 8849 | `		const char *zWrapper,*zLabel;` |
|       9 | 8850 | `		if( pDev == 0 ){` |
|     ! 0 | 8851 | `			return rc;` |
|       - | 8852 | `		}` |
|       9 | 8853 | `		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|       - | 8854 | `		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */` |
|       9 | 8855 | `		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));` |
|       - | 8856 | `	}` |
|       9 | 8857 | `	return PH7_OK;` |
|      15 | 8858 | `}` |
|       - | 8859 | `/* PH7_ENABLE_NET */` |
|       - | 8860 | `/*` |
|       - | 8861 | ` * The open that fopen() is: the device lookup, the io_private, the mode` |
|       - | 8862 | ``  * translation, the handle and the meta-data record `stream_get_meta_data()` `` |
|       - | 8863 | ` * reports back. php's fopen() and its SplFileObject constructor both call` |
|       - | 8864 | ` * php_stream_open_wrapper_ex(), so both doors here share this body rather than` |
|       - | 8865 | ` * spelling the sequence twice.` |
|       - | 8866 | ` *` |
|       - | 8867 | ` * NOTHING is reported from in here. The two failures are handed back through` |
|       - | 8868 | ` * *piErr, because the two callers word them differently: fopen() warns, while` |
|       - | 8869 | ` * SplFileObject's constructor promotes the same warning to a RuntimeException` |
|       - | 8870 | ` * (php's zend_replace_error_handling). *pzErrUri is the name to report -- the` |
|       - | 8871 | ` * scheme-stripped remainder, which is what the warning has always printed.` |
|       - | 8872 | ` */` |
|    1862 | 8873 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|       - | 8874 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|       - | 8875 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri)` |
|       5 | 8876 | `{` |
|       - | 8877 | `	const ph7_io_stream *pStream;` |
|       - | 8878 | `	const char *zUri;` |
|       - | 8879 | `	ph7_value *pResource;` |
|       - | 8880 | `	io_private *pDev;` |
|       - | 8881 | `	int iLen,iOpenFlags;` |
|    1867 | 8882 | `	zUri = ph7_value_to_string(pPath,&iLen);` |
|    1867 | 8883 | `	*piErr = PH7_STREAM_OPEN_OK;` |
|    1867 | 8884 | `	*pzErrUri = zUri;` |
|       - | 8885 | `	/* Try to extract a stream */` |
|    1867 | 8886 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    1867 | 8887 | `	*pzErrUri = zUri;` |
|    1867 | 8888 | `	if( pStream == 0 ){` |
|     ! 0 | 8889 | `		*piErr = PH7_STREAM_OPEN_NODEVICE;` |
|     ! 0 | 8890 | `		return 0;` |
|       - | 8891 | `	}` |
|       - | 8892 | `	/* php's mode grammar belongs to the PLAIN-FILE wrapper and to nothing else:` |
|       - | 8893 | `	 * php://, data:// and a userland wrapper are handed whatever the caller` |
|       - | 8894 | ``	 * wrote and decide for themselves (`fopen('php://memory','zz')` opens`` |
|       - | 8895 | `	 * read-only rather than failing), so only the default device refuses. */` |
|    1867 | 8896 | `	if( StrModeToFlags(zMode,nMode,&iOpenFlags) != 0 ){` |
|      39 | 8897 | `		if( pStream == pCtx->pVm->pDefStream ){` |
|      39 | 8898 | `			*piErr = PH7_STREAM_OPEN_BADMODE;` |
|      39 | 8899 | `			return 0;` |
|       - | 8900 | `		}` |
|     ! 0 | 8901 | `		iOpenFlags = PH7_IO_OPEN_RDONLY;` |
|     ! 0 | 8902 | `	}` |
|       - | 8903 | `	/* Allocate a new IO private instance */` |
|    1829 | 8904 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    1829 | 8905 | `	if( pDev == 0 ){` |
|     ! 0 | 8906 | `		*piErr = PH7_STREAM_OPEN_NOMEM;` |
|     ! 0 | 8907 | `		return 0;` |
|       - | 8908 | `	}` |
|    1829 | 8909 | `	pResource = 0;` |
|    1829 | 8910 | `	if( pCtxArg ){` |
|      10 | 8911 | `		pResource = pCtxArg;` |
|    1825 | 8912 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|       - | 8913 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|       - | 8914 | `		 * access the underlying virtual machine.` |
|       - | 8915 | `		 */` |
|     610 | 8916 | `		pResource = pPath;` |
|     301 | 8917 | `	}` |
|       - | 8918 | `	/* Initialize the structure */` |
|    1829 | 8919 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|    1829 | 8920 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 8921 | `	/* The caller wrote this mode; a userland wrapper is handed it verbatim. */` |
|    1829 | 8922 | `	PH7_StreamArmOpenMode(pCtx->pVm,zMode,nMode);` |
|       - | 8923 | `	/* Try to get a handle */` |
|    2738 | 8924 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     909 | 8925 | `		bUseInclude,pResource,FALSE,0,ph7_function_name(pCtx));` |
|    1829 | 8926 | `	if( pDev->pHandle == 0 ){` |
|     113 | 8927 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     113 | 8928 | `		*piErr = PH7_STREAM_OPEN_FAILED;` |
|     113 | 8929 | `		return 0;` |
|       - | 8930 | `	}` |
|       - | 8931 | `	/* Remember what we were asked for: stream_get_meta_data() reports both.` |
|       - | 8932 | `	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder` |
|       - | 8933 | `	 * PH7_VmGetStreamDevice() advanced zUri past. */` |
|       - | 8934 | `	{` |
|       - | 8935 | `		int nUri;` |
|    1720 | 8936 | `		const char *zOrig = ph7_value_to_string(pPath,&nUri);` |
|    1720 | 8937 | `		const char *zMeta = zMode;` |
|    1720 | 8938 | `		int nMeta = nMode;` |
|    1715 | 8939 | `		if( is_php_stream(pStream)` |
|    1145 | 8940 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|       - | 8941 | `			/* php://output has one mode whatever it was asked for. */` |
|       9 | 8942 | `			zMeta = "wb";` |
|       9 | 8943 | `			nMeta = 2;` |
|    1712 | 8944 | `		}else if( is_php_stream(pStream)` |
|    1137 | 8945 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_INPUT ){` |
|       - | 8946 | `			/* php's input stream is built read-only whatever was asked for. */` |
|      13 | 8947 | `			zMeta = "rb";` |
|      13 | 8948 | `			nMeta = 2;` |
|    1702 | 8949 | `		}else if( is_php_stream(pStream)` |
|    1125 | 8950 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|       - | 8951 | `			/* php's memory streams do not keep the mode they were opened with:` |
|       - | 8952 | `			 * a buffer is readable and writable either way, so php reports the` |
|       - | 8953 | `			 * one it actually built. */` |
|     800 | 8954 | `			int i,bWrite = 0,bAppend = nMode > 0 && (zMode[0] == 'a' \|\| zMode[0] == 'A');` |
|    1503 | 8955 | `			for( i = 0 ; i < nMode ; i++ ){` |
|     967 | 8956 | `				if( zMode[i] == 'w' \|\| zMode[i] == 'W' \|\| zMode[i] == 'a'` |
|     760 | 8957 | `				 \|\| zMode[i] == 'A' \|\| zMode[i] == '+' ){` |
|     630 | 8958 | `					bWrite = 1;` |
|     313 | 8959 | `				}` |
|     487 | 8960 | `			}` |
|     536 | 8961 | `			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";` |
|     536 | 8962 | `			nMeta = (int)SyStrlen(zMeta);` |
|     264 | 8963 | `		}` |
|    1720 | 8964 | `		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);` |
|       - | 8965 | `	}` |
|    1720 | 8966 | `	return pDev;` |
|     933 | 8967 | `}` |
|    1736 | 8968 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 8969 | `{` |
|       - | 8970 | `	const char *zMode,*zErrUri;` |
|       - | 8971 | `	io_private *pDev;` |
|       - | 8972 | `	phl_stream_ctx *pCtxRes;` |
|    1741 | 8973 | `	int imLen,bThrew = 0,iErr;` |
|    1741 | 8974 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 8975 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 8976 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|     ! 0 | 8977 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 8978 | `		return PH7_OK;` |
|       - | 8979 | `	}` |
|       - | 8980 | `	{` |
|    1741 | 8981 | `		int nPath = 0;` |
|    1741 | 8982 | `		ph7_value_to_string(apArg[0],&nPath);` |
|    1741 | 8983 | `		if( PH7_VfsEmptyPathRefused(pCtx,nPath) ){` |
|       2 | 8984 | `			return PH7_OK;` |
|       - | 8985 | `		}` |
|       - | 8986 | `	}` |
|       - | 8987 | `	/* Extract the desired access mode */` |
|    1739 | 8988 | `	if( nArg > 1 ){` |
|    1739 | 8989 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     869 | 8990 | `	}else{` |
|       - | 8991 | `		/* Set a default read-only mode */` |
|     ! 0 | 8992 | `		zMode = "r";` |
|     ! 0 | 8993 | `		imLen = (int)sizeof(char);` |
|       - | 8994 | `	}` |
|       - | 8995 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - | 8996 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - | 8997 | `	 * Resolved before the io_private chunk below, which a throw could not` |
|       - | 8998 | `	 * release. */` |
|    1739 | 8999 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    1739 | 9000 | `	if( bThrew ){` |
|       5 | 9001 | `		return PH7_OK;` |
|       - | 9002 | `	}` |
|    2601 | 9003 | `	pDev = PH7_StreamOpenPath(pCtx,apArg[0],zMode,imLen,` |
|     866 | 9004 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pCtxRes,` |
|     862 | 9005 | `		nArg > 3 ? apArg[3] : 0,&iErr,&zErrUri);` |
|    1735 | 9006 | `	if( pDev == 0 ){` |
|     145 | 9007 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|     ! 0 | 9008 | `			VfsThrowNoDeviceWarning(pCtx,zErrUri,FALSE);` |
|     145 | 9009 | `		}else if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|      55 | 9010 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|       - | 9011 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|      18 | 9012 | `				ph7_function_name(pCtx),zErrUri,imLen,zMode);` |
|     127 | 9013 | `		}else if( iErr == PH7_STREAM_OPEN_FAILED ){` |
|     109 | 9014 | `			VfsThrowOpenWarning(pCtx,zErrUri);` |
|      56 | 9015 | `		}else{` |
|     ! 0 | 9016 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|       - | 9017 | `		}` |
|     145 | 9018 | `		ph7_result_bool(pCtx,0);` |
|     145 | 9019 | `		return PH7_OK;` |
|       - | 9020 | `	}` |
|       - | 9021 | `	/* All done,return the io_private instance as a resource */` |
|    1594 | 9022 | `	ph7_result_resource(pCtx,pDev);` |
|    1594 | 9023 | `	return PH7_OK;` |
|     870 | 9024 | `}` |
|       - | 9025 | `/*` |
|       - | 9026 | ` * bool fclose(resource $handle)` |
|       - | 9027 | ` *  Closes an open file pointer` |
|       - | 9028 | ` * Parameters` |
|       - | 9029 | ` *  $handle` |
|       - | 9030 | ` *   The file pointer.` |
|       - | 9031 | ` * Return` |
|       - | 9032 | ` *  TRUE on success or FALSE on failure.` |
|       - | 9033 | ` */` |
|    2538 | 9034 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 9035 | `{` |
|       - | 9036 | `	const ph7_io_stream *pStream;` |
|       - | 9037 | `	io_private *pDev;` |
|       - | 9038 | `	ph7_vm *pVm;` |
|    2543 | 9039 | `	if( nArg < 1 ){` |
|       - | 9040 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 9041 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 9042 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9043 | `		return PH7_OK;` |
|       - | 9044 | `	}` |
|    2543 | 9045 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - | 9046 | `		char zGiven[64];` |
|      19 | 9047 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 9048 | `			"%s(): Argument #1 ($stream) must be of type resource, %s given",` |
|       6 | 9049 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 9050 | `	}` |
|       - | 9051 | `	/* Extract our private data */` |
|    2531 | 9052 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|       - | 9053 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError,` |
|       - | 9054 | `	 * and the name in it is the one that was CALLED -- gzclose() is this same` |
|       - | 9055 | `	 * body under another name and says gzclose(). */` |
|    2531 | 9056 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      14 | 9057 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 9058 | `			"%s(): Argument #1 ($stream) must be an open stream resource",` |
|       4 | 9059 | `			ph7_function_name(pCtx));` |
|       - | 9060 | `	}` |
|       - | 9061 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    2523 | 9062 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 9063 | `		/*Expecting an IO handle */` |
|     ! 0 | 9064 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 9065 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9066 | `		return PH7_OK;` |
|       - | 9067 | `	}` |
|       - | 9068 | `	/* Point to the target IO stream device */` |
|    2523 | 9069 | `	pStream = pDev->pStream;` |
|    2523 | 9070 | `	if( pStream == 0 ){` |
|       - | 9071 | `		/* Nothing to close. php's fclose() has no diagnostic for it. */` |
|     ! 0 | 9072 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9073 | `		return PH7_OK;` |
|       - | 9074 | `	}` |
|       - | 9075 | `	/* Point to the VM that own this context */` |
|    2523 | 9076 | `	pVm = pCtx->pVm;` |
|       - | 9077 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    2523 | 9078 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|       - | 9079 | `		/* The WRITE chain gets its closing call while the device is still open:` |
|       - | 9080 | `		 * a filter that buffers has nowhere else to put its tail, and php's own` |
|       - | 9081 | `		 * close flushes before it closes. */` |
|    2523 | 9082 | `		PH7_StreamFilterReleaseChains(pDev);` |
|       - | 9083 | `		/* Perform the requested operation */` |
|    2523 | 9084 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|       - | 9085 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    2523 | 9086 | `		MarkIOPrivateClosed(pDev);` |
|    1257 | 9087 | `	}` |
|       - | 9088 | `	/* Return TRUE */` |
|    2523 | 9089 | `	ph7_result_bool(pCtx,1);` |
|    2523 | 9090 | `	return PH7_OK;` |
|    1272 | 9091 | `}` |
|       - | 9092 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|       - | 9093 | `/*` |
|       - | 9094 | ` * MD5/SHA1 digest consumer.` |
|       - | 9095 | ` */` |
|     200 | 9096 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|       3 | 9097 | `{` |
|       - | 9098 | `	/* Append hex chunk verbatim */` |
|     203 | 9099 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     203 | 9100 | `	return SXRET_OK;` |
|       3 | 9101 | `}` |
|       - | 9102 | `/*` |
|       - | 9103 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|       - | 9104 | ` *  Calculates the md5 hash of a given file.` |
|       - | 9105 | ` * Parameters` |
|       - | 9106 | ` *  $uri` |
|       - | 9107 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|       - | 9108 | ` *  $raw_output` |
|       - | 9109 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|       - | 9110 | ` * Return` |
|       - | 9111 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|       - | 9112 | ` */` |
|      14 | 9113 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 9114 | `{` |
|       - | 9115 | `	const ph7_io_stream *pStream;` |
|       - | 9116 | `	unsigned char zDigest[16];` |
|      17 | 9117 | `	int raw_output  = FALSE;` |
|       - | 9118 | `	const char *zFile;` |
|       - | 9119 | `	MD5Context sCtx;` |
|       - | 9120 | `	char zBuf[8192];` |
|       - | 9121 | `	void *pHandle;` |
|       - | 9122 | `	ph7_int64 n;` |
|       - | 9123 | `	int nLen;` |
|      17 | 9124 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 9125 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 9126 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 9127 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9128 | `		return PH7_OK;` |
|       - | 9129 | `	}` |
|       - | 9130 | `	/* Extract the file path */` |
|      17 | 9131 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      17 | 9132 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 9133 | `		return PH7_OK;` |
|       - | 9134 | `	}` |
|       - | 9135 | `	/* Point to the target IO stream device */` |
|      15 | 9136 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      15 | 9137 | `	if( pStream == 0 ){` |
|     ! 0 | 9138 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 9139 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9140 | `		return PH7_OK;` |
|       - | 9141 | `	}` |
|      15 | 9142 | `	if( nArg > 1 ){` |
|     ! 0 | 9143 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     ! 0 | 9144 | `	}` |
|       - | 9145 | `	/* Try to open the file in read-only mode */` |
|      15 | 9146 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      15 | 9147 | `	if( pHandle == 0 ){` |
|       3 | 9148 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 | 9149 | `		ph7_result_bool(pCtx,0);` |
|       3 | 9150 | `		return PH7_OK;` |
|       - | 9151 | `	}` |
|       - | 9152 | `	/* Init the MD5 context */` |
|      13 | 9153 | `	MD5Init(&sCtx);` |
|       - | 9154 | `	/* Perform the requested operation */` |
|       8 | 9155 | `	for(;;){` |
|      19 | 9156 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      19 | 9157 | `		if( n < 1 ){` |
|       - | 9158 | `			/* EOF or IO error,break immediately */` |
|      13 | 9159 | `			break;` |
|       - | 9160 | `		}` |
|       8 | 9161 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|       2 | 9162 | `	}` |
|       - | 9163 | `	/* Close the stream */` |
|      13 | 9164 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 9165 | `	/* Extract the digest */` |
|      13 | 9166 | `	MD5Final(zDigest,&sCtx);` |
|      13 | 9167 | `	if( raw_output ){` |
|       - | 9168 | `		/* Output raw digest */` |
|     ! 0 | 9169 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|     ! 0 | 9170 | `	}else{` |
|       - | 9171 | `		/* Perform a binary to hex conversion */` |
|      13 | 9172 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|       - | 9173 | `	}` |
|      13 | 9174 | `	return PH7_OK;` |
|      10 | 9175 | `}` |
|       - | 9176 | `/*` |
|       - | 9177 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|       - | 9178 | ` *  Calculates the SHA1 hash of a given file.` |
|       - | 9179 | ` * Parameters` |
|       - | 9180 | ` *  $uri` |
|       - | 9181 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|       - | 9182 | ` *  $raw_output` |
|       - | 9183 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|       - | 9184 | ` * Return` |
|       - | 9185 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|       - | 9186 | ` */` |
|       4 | 9187 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 9188 | `{` |
|       - | 9189 | `	const ph7_io_stream *pStream;` |
|       - | 9190 | `	unsigned char zDigest[20];` |
|       5 | 9191 | `	int raw_output  = FALSE;` |
|       - | 9192 | `	const char *zFile;` |
|       - | 9193 | `	SHA1Context sCtx;` |
|       - | 9194 | `	char zBuf[8192];` |
|       - | 9195 | `	void *pHandle;` |
|       - | 9196 | `	ph7_int64 n;` |
|       - | 9197 | `	int nLen;` |
|       5 | 9198 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 9199 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 9200 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 9201 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9202 | `		return PH7_OK;` |
|       - | 9203 | `	}` |
|       - | 9204 | `	/* Extract the file path */` |
|       5 | 9205 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|       5 | 9206 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 9207 | `		return PH7_OK;` |
|       - | 9208 | `	}` |
|       - | 9209 | `	/* Point to the target IO stream device */` |
|       3 | 9210 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|       3 | 9211 | `	if( pStream == 0 ){` |
|     ! 0 | 9212 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 9213 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9214 | `		return PH7_OK;` |
|       - | 9215 | `	}` |
|       3 | 9216 | `	if( nArg > 1 ){` |
|     ! 0 | 9217 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     ! 0 | 9218 | `	}` |
|       - | 9219 | `	/* Try to open the file in read-only mode */` |
|       3 | 9220 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|       3 | 9221 | `	if( pHandle == 0 ){` |
|     ! 0 | 9222 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 9223 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9224 | `		return PH7_OK;` |
|       - | 9225 | `	}` |
|       - | 9226 | `	/* Init the SHA1 context */` |
|       3 | 9227 | `	SHA1Init(&sCtx);` |
|       - | 9228 | `	/* Perform the requested operation */` |
|       2 | 9229 | `	for(;;){` |
|       5 | 9230 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|       5 | 9231 | `		if( n < 1 ){` |
|       - | 9232 | `			/* EOF or IO error,break immediately */` |
|       3 | 9233 | `			break;` |
|       - | 9234 | `		}` |
|       3 | 9235 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|       1 | 9236 | `	}` |
|       - | 9237 | `	/* Close the stream */` |
|       3 | 9238 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 9239 | `	/* Extract the digest */` |
|       3 | 9240 | `	SHA1Final(&sCtx,zDigest);` |
|       3 | 9241 | `	if( raw_output ){` |
|       - | 9242 | `		/* Output raw digest */` |
|     ! 0 | 9243 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|     ! 0 | 9244 | `	}else{` |
|       - | 9245 | `		/* Perform a binary to hex conversion */` |
|       3 | 9246 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|       - | 9247 | `	}` |
|       3 | 9248 | `	return PH7_OK;` |
|       3 | 9249 | `}` |
|       - | 9250 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 9251 | `/*` |
|       - | 9252 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|       - | 9253 | ` *  Parse a configuration file.` |
|       - | 9254 | ` * Parameters` |
|       - | 9255 | ` * $filename` |
|       - | 9256 | ` *  The filename of the ini file being parsed.` |
|       - | 9257 | ` * $process_sections` |
|       - | 9258 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|       - | 9259 | ` *  with the section names and settings included.` |
|       - | 9260 | ` *  The default for process_sections is FALSE.` |
|       - | 9261 | ` * $scanner_mode` |
|       - | 9262 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|       - | 9263 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|       - | 9264 | ` * Return` |
|       - | 9265 | ` *  The settings are returned as an associative array on success.` |
|       - | 9266 | ` *  Otherwise is returned.` |
|       - | 9267 | ` */` |
|      10 | 9268 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 9269 | `{` |
|       - | 9270 | `	const ph7_io_stream *pStream;` |
|       - | 9271 | `	const char *zFile;` |
|       - | 9272 | `	SyBlob sContents;` |
|       - | 9273 | `	void *pHandle;` |
|       - | 9274 | `	int nLen;` |
|      11 | 9275 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      11 | 9276 | `	sxi32 rc = PH7_OK;` |
|      11 | 9277 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 9278 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 9279 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 9280 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9281 | `		return PH7_OK;` |
|       - | 9282 | `	}` |
|      11 | 9283 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|       7 | 9284 | `		iMode = ph7_value_to_int(apArg[2]);` |
|       6 | 9285 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|       6 | 9286 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|       - | 9287 | `			/* php screens the mode BEFORE touching the file */` |
|       - | 9288 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|       3 | 9289 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|       3 | 9290 | `			ph7_result_bool(pCtx,0);` |
|       3 | 9291 | `			return PH7_OK;` |
|       - | 9292 | `		}` |
|       2 | 9293 | `	}` |
|       - | 9294 | `	/* Extract the file path */` |
|       9 | 9295 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|       9 | 9296 | `	if( nLen < 1 ){` |
|       - | 9297 | `		/* One of php's three doors that names its own argument instead of` |
|       - | 9298 | ``		 * raising the stream layer's `Path must not be empty`. */`` |
|       2 | 9299 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 9300 | `			"parse_ini_file(): Argument #1 ($filename) must not be empty");` |
|       - | 9301 | `	}` |
|       - | 9302 | `	/* Point to the target IO stream device */` |
|       7 | 9303 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|       7 | 9304 | `	if( pStream == 0 ){` |
|     ! 0 | 9305 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 9306 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9307 | `		return PH7_OK;` |
|       - | 9308 | `	}` |
|       - | 9309 | `	/* Try to open the file in read-only mode */` |
|       7 | 9310 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|       7 | 9311 | `	if( pHandle == 0 ){` |
|     ! 0 | 9312 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 9313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9314 | `		return PH7_OK;` |
|       - | 9315 | `	}` |
|       7 | 9316 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|       - | 9317 | `	/* Read the whole file */` |
|       7 | 9318 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|       7 | 9319 | `	if( SyBlobLength(&sContents) < 1 ){` |
|       - | 9320 | `		/* Empty buffer,return FALSE */` |
|     ! 0 | 9321 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 9322 | `	}else{` |
|       - | 9323 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      13 | 9324 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|       6 | 9325 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|       - | 9326 | `	}` |
|       - | 9327 | `	/* Close the stream */` |
|       7 | 9328 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 9329 | `	/* Release the working buffer */` |
|       7 | 9330 | `	SyBlobRelease(&sContents);` |
|       - | 9331 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|       7 | 9332 | `	return rc;` |
|       6 | 9333 | `}` |
|       - | 9334 | `/* ZIP archive processing moved to vfs_zip.c */` |
|       - | 9335 | `#else /* PH7_DISABLE_DISK_IO */` |
|       - | 9336 | `/*` |
|       - | 9337 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|       - | 9338 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|       - | 9339 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|       - | 9340 | ` * to any non-VFS resource).` |
|       - | 9341 | ` */` |
|       - | 9342 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|       - | 9343 | `{` |
|       - | 9344 | `	SXUNUSED(pResource);` |
|       - | 9345 | `	return "Unknown";` |
|       - | 9346 | `}` |
|       - | 9347 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|       - | 9348 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|       - | 9349 | `{` |
|       - | 9350 | `	SXUNUSED(pResource);` |
|       - | 9351 | `	return 0;` |
|       - | 9352 | `}` |
|       - | 9353 | `/* No streams means no stream contexts either, but PH7_VmReset still calls this. */` |
|       - | 9354 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|       - | 9355 | `{` |
|       - | 9356 | `	SXUNUSED(pVm);` |
|       - | 9357 | `}` |
|       - | 9358 | `/* Same for the filter registry. */` |
|       - | 9359 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|       - | 9360 | `{` |
|       - | 9361 | `	SXUNUSED(pVm);` |
|       - | 9362 | `}` |
|       - | 9363 | `#endif /* PH7_DISABLE_DISK_IO */` |
|       - | 9364 |  |
