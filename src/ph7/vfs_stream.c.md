# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4962/5742 lines (86.42%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits |  Line | Source |
| ------: | ----: | :--- |
|       - |     1 | `/**` |
|       - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |     5 | ` */` |
|       - |     6 | `#include "ph7int.h"` |
|       - |     7 | `#include <stdio.h>` |
|       - |     8 | `#include <errno.h>` |
|       - |     9 | `#include <string.h>` |
|       - |    10 |  |
|       - |    11 | `#ifdef __UNIXES__` |
|       - |    12 | `#include <unistd.h>` |
|       - |    13 | `#include <sys/wait.h>` |
|       - |    14 | `#include <fcntl.h>` |
|       - |    15 | `#include <signal.h>` |
|       - |    16 | `#endif` |
|       - |    17 | `#if defined(PH7_ENABLE_NET) && defined(PH7_ENABLE_OPENSSL)` |
|       - |    18 | `/* The ssl:// and tls:// transports are libssl DIRECTLY rather than a wrapper` |
|       - |    19 | ` * over ext/openssl's surface: SSL_set_fd() adopts a socket this file already` |
|       - |    20 | `` * connected, and php's own `ssl` context options are OpenSSL's vocabulary`` |
|       - |    21 | ` * spelled in php's words, so most of them are one library call each. This is` |
|       - |    22 | ` * the same libssl ext/openssl links -- and, on Windows, still NOT the TLS` |
|       - |    23 | ` * ext/curl uses, which is Schannel through vcpkg. */` |
|       - |    24 | `#include <openssl/ssl.h>` |
|       - |    25 | `#include <openssl/err.h>` |
|       - |    26 | `#include <openssl/x509v3.h>` |
|       - |    27 | `/* ext/openssl's private header, for the ONE thing this file borrows from it:` |
|       - |    28 | `` * php's `capture_peer_cert` hands a script an OpenSSLCertificate, and that is`` |
|       - |    29 | ` * the extension's handle class rather than anything the transport owns. */` |
|       - |    30 | `#include "openssl_int.h"` |
|       - |    31 | `#endif` |
|       - |    32 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |    33 | `/*` |
|       - |    34 | ` * Section:` |
|       - |    35 | ` *    IO stream implementation.` |
|       - |    36 | ` * Status:` |
|       - |    37 | ` *    Stable.` |
|       - |    38 | ` */` |
|       - |    39 | `/* Forward declaration */` |
|       - |    40 | `static void ResetIOPrivate(io_private *pDev);` |
|       - |    41 | `/*` |
|       - |    42 | ` * How many bytes sit AHEAD of where the script is: the line readers' read-ahead` |
|       - |    43 | ` * plus whatever the read filter chain has already produced and nobody has taken` |
|       - |    44 | ` * yet. Both are past the position a script observes, so ftell(), a SEEK_CUR` |
|       - |    45 | `` * seek and stream_get_meta_data()'s `unread_bytes` all have to discount them.`` |
|       - |    46 | ` */` |
|       - |    47 | `static void ResetIOPrivate(io_private *pDev);` |
|       - |    48 | `static sxu32 StreamAheadBytes(io_private *pDev);` |
|       - |    49 | `static void StreamSeedPosition(io_private *pDev);` |
|       - |    50 | `static void StreamSeekLanded(io_private *pDev,ph7_int64 iOfft,int whence);` |
|       - |    51 | `static void StreamSeekRefused(io_private *pDev);` |
|       - |    52 | `/*` |
|       - |    53 | ` * Ask the device where this handle STARTS, once. php does it with a single` |
|       - |    54 | ` * lseek() as the stream is created and never asks again: from then on its own` |
|       - |    55 | ` * counter is the position, moved by what was read, written or seeked. A` |
|       - |    56 | ` * descriptor that cannot answer seeds the counter at -1, which is why php's` |
|       - |    57 | ` * ftell() on a fresh proc_open() pipe is false and becomes a (short) number` |
|       - |    58 | ` * once bytes have gone past it.` |
|       - |    59 | ` */` |
|   15122 |    60 | `static void StreamSeedPosition(io_private *pDev)` |
|       5 |    61 | `{` |
|   15127 |    62 | `	if( pDev->bPosSeeded ){` |
|    6235 |    63 | `		return;` |
|       - |    64 | `	}` |
|    8897 |    65 | `	pDev->bPosSeeded = 1;` |
|    8897 |    66 | `	if( pDev->pStream && pDev->pStream->xTell && !pDev->bDir ){` |
|    1723 |    67 | `		pDev->iPos = pDev->pStream->xTell(pDev->pHandle);` |
|     857 |    68 | `	}` |
|    7501 |    69 | `}` |
|       - |    70 | `/*` |
|       - |    71 | ` * Where a successful seek LANDED. php's device op hands the resulting absolute` |
|       - |    72 | ` * offset back to the stream layer, which stores it as the new position; the op` |
|       - |    73 | ` * here answers a status code instead, so an absolute seek is its own answer and` |
|       - |    74 | ` * anything else is read back off the device. A device with nothing to read back` |
|       - |    75 | ` * keeps the counter it had rather than inventing one.` |
|       - |    76 | ` */` |
|     518 |    77 | `static void StreamSeekLanded(io_private *pDev,ph7_int64 iOfft,int whence)` |
|       5 |    78 | `{` |
|     523 |    79 | `	pDev->bPosSeeded = 1;` |
|     523 |    80 | `	if( whence == 0 /* SEEK_SET */ ){` |
|     507 |    81 | `		pDev->iPos = iOfft;` |
|     270 |    82 | `	}else if( pDev->pStream && pDev->pStream->xTell ){` |
|      19 |    83 | `		pDev->iPos = pDev->pStream->xTell(pDev->pHandle);` |
|       8 |    84 | `	}` |
|     523 |    85 | `}` |
|       - |    86 | `/*` |
|       - |    87 | ` * A seek the device REFUSED. php hands the device the same slot it writes a` |
|       - |    88 | ` * successful position into, so a device is free to declare, on failure, that it` |
|       - |    89 | ` * no longer knows where it is: PDO's blob handle does exactly that when asked` |
|       - |    90 | ` * for an offset past its own end, and ftell() then answers false until a seek` |
|       - |    91 | ` * succeeds again. A device that keeps its position -- a plain descriptor, whose` |
|       - |    92 | ` * lseek() leaves it alone when it fails -- leaves the counter alone too.` |
|       - |    93 | ` */` |
|      12 |    94 | `static void StreamSeekRefused(io_private *pDev)` |
|       3 |    95 | `{` |
|      15 |    96 | `	pDev->bPosSeeded = 1;` |
|      12 |    97 | `	if( pDev->pStream && pDev->pStream->xTell` |
|      15 |    98 | `	 && pDev->pStream->xTell(pDev->pHandle) < 0 ){` |
|       8 |    99 | `		pDev->iPos = -1;` |
|       3 |   100 | `	}` |
|      15 |   101 | `}` |
|       - |   102 | `/*` |
|       - |   103 | ` * A write lands where the SCRIPT is, not where the device is. Everything the` |
|       - |   104 | ` * readers pulled ahead — the line buffer and the filter chain's output alike —` |
|       - |   105 | ` * sits between the two, so it is stepped over and dropped before the write.` |
|       - |   106 | ` * php does the same by seeking to the logical position it tracks.` |
|       - |   107 | ` */` |
|     693 |   108 | `static void StreamSeekBackForWrite(io_private *pDev)` |
|       5 |   109 | `{` |
|     698 |   110 | `	sxu32 nAhead = StreamAheadBytes(pDev);` |
|     698 |   111 | `	if( nAhead > 0 && pDev->pStream && pDev->pStream->xSeek ){` |
|      19 |   112 | `		pDev->pStream->xSeek(pDev->pHandle,-(ph7_int64)nAhead,1/*SEEK_CUR*/);` |
|      19 |   113 | `		ResetIOPrivate(pDev);` |
|       - |   114 | `		/* The counter counted those bytes as consumed; stepping back over them` |
|       - |   115 | `		 * un-counts them, and the position the script sees does not move. */` |
|      19 |   116 | `		StreamSeedPosition(pDev);` |
|      19 |   117 | `		pDev->iPos -= (ph7_int64)nAhead;` |
|       8 |   118 | `	}` |
|     698 |   119 | `}` |
|     212 |   120 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev)` |
|       5 |   121 | `{` |
|       - |   122 | `	ph7_int64 iOfft;` |
|     217 |   123 | `	if( pDev == 0 ){` |
|     ! 0 |   124 | `		return -1;` |
|       - |   125 | `	}` |
|     217 |   126 | `	if( pDev->pReadFilters ){` |
|       - |   127 | `		/* A read filter breaks the tie between the device's offset and the` |
|       - |   128 | `		 * script's: four base64 characters come out of three bytes, so the two` |
|       - |   129 | `		 * numbers are not even the same magnitude. php counts what it` |
|       - |   130 | `		 * DELIVERED, and so does this — less whatever a line reader is still` |
|       - |   131 | `		 * holding on the script's behalf. */` |
|      21 |   132 | `		iOfft = pDev->iFiltPos;` |
|      21 |   133 | `		if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|     ! 0 |   134 | `			iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);` |
|     ! 0 |   135 | `		}` |
|      21 |   136 | `		return iOfft;` |
|       - |   137 | `	}` |
|     197 |   138 | `	if( pDev->bDir ){` |
|       - |   139 | `		/* A directory handle's position is php's own record counter and` |
|       - |   140 | `		 * nothing the device knows about. */` |
|       7 |   141 | `		return pDev->iPos;` |
|       - |   142 | `	}` |
|       - |   143 | `	/* php's stream layer tracks a position for EVERY stream and asks the device` |
|       - |   144 | `	 * only when it seeks, so this counter -- not the descriptor -- is the` |
|       - |   145 | `	 * answer. On an APPEND handle the descriptor is at the end of the file` |
|       - |   146 | `	 * after every write and the counter is at the bytes written, which is the` |
|       - |   147 | `	 * number php reports; on a pipe or a socket the descriptor cannot be asked` |
|       - |   148 | `	 * at all and the counter still says how far the stream has got. */` |
|     191 |   149 | `	StreamSeedPosition(pDev);` |
|     191 |   150 | `	iOfft = pDev->iPos;` |
|     191 |   151 | `	if( iOfft < 0 ){` |
|       8 |   152 | `		return iOfft;` |
|       - |   153 | `	}` |
|     185 |   154 | `	return iOfft - (ph7_int64)StreamAheadBytes(pDev);` |
|     111 |   155 | `}` |
|       - |   156 | `/*` |
|       - |   157 | ` * Seek the stream a php://filter proxy wraps. Same model as fseek() on a` |
|       - |   158 | ` * filtered handle: a relative move is resolved against the position the SCRIPT` |
|       - |   159 | ` * sees, because the device's own offset is not comparable to it.` |
|       - |   160 | ` */` |
|      44 |   161 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence)` |
|       2 |   162 | `{` |
|       - |   163 | `	int rc;` |
|      46 |   164 | `	if( pDev == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xSeek == 0 ){` |
|     ! 0 |   165 | `		return -1;` |
|       - |   166 | `	}` |
|      46 |   167 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|       9 |   168 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|     ! 0 |   169 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|     ! 0 |   170 | `		}` |
|       9 |   171 | `		whence = 0; /* SEEK_SET */` |
|       4 |   172 | `	}` |
|      46 |   173 | `	rc = pDev->pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      46 |   174 | `	if( rc == PH7_OK ){` |
|      46 |   175 | `		StreamSeekLanded(pDev,iOfft,whence);` |
|      46 |   176 | `		SyBlobReset(&pDev->sBuffer);` |
|      46 |   177 | `		pDev->nOfft = 0;` |
|      46 |   178 | `		SyBlobReset(&pDev->sFilt);` |
|      46 |   179 | `		pDev->nFiltOfft = 0;` |
|      46 |   180 | `		pDev->bFiltDone = 0;` |
|      46 |   181 | `		pDev->bEof = 0;` |
|      46 |   182 | `		PH7_StreamFilterRewound(pDev);` |
|      46 |   183 | `		pDev->iFiltPos = whence == 0 ? iOfft` |
|      22 |   184 | `			: (pDev->pStream->xTell ? pDev->pStream->xTell(pDev->pHandle) : 0);` |
|      22 |   185 | `	}` |
|      46 |   186 | `	return rc;` |
|      24 |   187 | `}` |
|     320 |   188 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev)` |
|       4 |   189 | `{` |
|     324 |   190 | `	if( pDev && pDev->pStream && is_php_stream(pDev->pStream) ){` |
|      96 |   191 | `		io_private *pInner = PH7_PhpStreamInner(pDev->pHandle);` |
|      96 |   192 | `		if( pInner ){` |
|      15 |   193 | `			return pInner;` |
|       - |   194 | `		}` |
|      38 |   195 | `	}` |
|     310 |   196 | `	return pDev;` |
|     161 |   197 | `}` |
|       - |   198 | `/*` |
|       - |   199 | `` * Can this handle take bytes at all? php answers `Stream is not writable` -- an`` |
|       - |   200 | ` * E_NOTICE naming the caller -- for a stream whose ops carry no writer: a` |
|       - |   201 | ` * directory handle, data://, glob://. That is a different event from a write` |
|       - |   202 | `` * that WAS attempted and failed (`Write of N bytes failed with errno=9`), which`` |
|       - |   203 | ` * a read-only descriptor produces and which the device path already reports.` |
|       - |   204 | ` */` |
|     719 |   205 | `static int StreamRefuseUnwritable(ph7_context *pCtx,io_private *pDev)` |
|       5 |   206 | `{` |
|     724 |   207 | `	if( pDev && !pDev->bDir && pDev->pStream && pDev->pStream->xWrite ){` |
|     714 |   208 | `		return 0;` |
|       - |   209 | `	}` |
|      11 |   210 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|      11 |   211 | `	return 1;` |
|     341 |   212 | `}` |
|       - |   213 | `/*` |
|       - |   214 | ` * php's stdio device announces a failed WRITE itself -- an E_NOTICE naming the` |
|       - |   215 | ` * caller, the count and the errno -- and only that device does: a write` |
|       - |   216 | ` * php://input or php://output refuses is silent, which is why the two` |
|       - |   217 | ` * whole-file writers cannot simply report every failure they see. They hold a` |
|       - |   218 | ` * bare handle rather than an io_private, so they ask by DEVICE.` |
|       - |   219 | ` */` |
|       4 |   220 | `static void StreamReportRawWriteFailure(ph7_context *pCtx,const ph7_io_stream *pStream,int nLen,int iErr)` |
|       1 |   221 | `{` |
|       5 |   222 | `	if( pStream == 0 \|\| pStream != pCtx->pVm->pDefStream ){` |
|       5 |   223 | `		return;` |
|       - |   224 | `	}` |
|       1 |   225 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     ! 0 |   226 | `		"Write of %d bytes failed with errno=%d %s",nLen,iErr,VfsStrerror(iErr));` |
|       3 |   227 | `}` |
|       - |   228 | `/*` |
|       - |   229 | ` * ...and can it give any? php's readers answer a silent FALSE on a handle whose` |
|       - |   230 | ` * ops carry no reader, with no diagnostic of any kind. A DIRECTORY handle is` |
|       - |   231 | ` * one of those: its pHandle is a DIR*, so a byte op that reached the file` |
|       - |   232 | ` * device would hand an opendir() pointer to read()/lseek()/ftruncate() as if it` |
|       - |   233 | ` * were a descriptor number.` |
|       - |   234 | ` */` |
|  123460 |   235 | `static int StreamHasReader(io_private *pDev)` |
|       5 |   236 | `{` |
|  123465 |   237 | `	return pDev && !pDev->bDir && pDev->pStream && pDev->pStream->xRead;` |
|       5 |   238 | `}` |
|    1067 |   239 | `static sxu32 StreamAheadBytes(io_private *pDev)` |
|       5 |   240 | `{` |
|    1072 |   241 | `	sxu32 n = 0;` |
|    1072 |   242 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      57 |   243 | `		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      26 |   244 | `	}` |
|    1072 |   245 | `	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|       5 |   246 | `		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|       2 |   247 | `	}` |
|    1072 |   248 | `	return n;` |
|       5 |   249 | `}` |
|       - |   250 | `#ifdef PH7_ENABLE_NET` |
|       - |   251 | `/* The socket handle, declared here because stream_get_meta_data()'s labels ask` |
|       - |   252 | ` * whether a socket has a transport under it. */` |
|       - |   253 | `typedef struct sock_private sock_private;` |
|       - |   254 | `struct sock_private` |
|       - |   255 | `{` |
|       - |   256 | `	ph7_vm *pVm;` |
|       - |   257 | `	ph7_socket sock;` |
|       - |   258 | `	int bEof;` |
|       - |   259 | `	int iLastErr; /* the OS code a failed send left, for php's own notice */` |
|       - |   260 | `	int bGeneric; /* a socketpair: no transport, and php labels it apart */` |
|       - |   261 | `	int bDgram;   /* udp://: a DATAGRAM socket, which php names apart again */` |
|       - |   262 | ``	const char *zLabel; /* an explicit `stream_type`, or 0 to derive it from the two`` |
|       - |   263 | `	                     * flags above. socket_export_stream() states one: php picks` |
|       - |   264 | `	                     * the ops from the DOMAIN as well as the type there, so an` |
|       - |   265 | ``	                     * AF_UNIX datagram socket reports `udg_socket`, a label no`` |
|       - |   266 | `	                     * URI this device stack opens can produce. Static storage. */` |
|       - |   267 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - |   268 | `	/* TLS, once a handshake has settled. A socket carries these from the moment` |
|       - |   269 | `	 * crypto is enabled -- by an ssl:// address, which negotiates during the` |
|       - |   270 | `	 * connect, or by stream_socket_enable_crypto() on a plain one -- and every` |
|       - |   271 | `	 * byte op below goes through libssl instead of the OS socket while they are` |
|       - |   272 | `	 * set. Held as void* so the struct compiles in a build without OpenSSL. */` |
|       - |   273 | `	void *pSsl;    /* SSL * */` |
|       - |   274 | `	void *pSslCtx; /* SSL_CTX *, one per stream: php builds a fresh context per` |
|       - |   275 | `	                * handle because every option below is per-connection. */` |
|       - |   276 | `	int bSslSeen;  /* A session was set up on this handle at some point. php frees` |
|       - |   277 | `	                * nothing until the handle closes, so the SSL object outlives` |
|       - |   278 | `	                * the shutdown -- which is what makes every LATER` |
|       - |   279 | `	                * stream_socket_enable_crypto() on the same handle a refusal,` |
|       - |   280 | `	                * whether or not a session is live right now. */` |
|       - |   281 | `	int iCryptoAccept; /* A LISTENING socket opened through a crypto transport` |
|       - |   282 | `	                    * carries the method its address named, because the` |
|       - |   283 | `	                    * handshake belongs to each connection it accepts and` |
|       - |   284 | `	                    * not to the listener -- which is why php's tls://` |
|       - |   285 | ``	                    * server binds even with a `local_cert` that does not`` |
|       - |   286 | `	                    * exist, and refuses the first accept instead. Zero on` |
|       - |   287 | `	                    * a plain listener and on every connected socket. */` |
|       - |   288 | `#endif` |
|       - |   289 | `};` |
|       - |   290 | `#endif` |
|       - |   291 | `/*` |
|       - |   292 | ``  * The dir trio's argument, which php declares `?resource $dir_handle = null` `` |
|       - |   293 | ` * and which therefore has a THIRD case the byte-stream doors do not: given` |
|       - |   294 | `` * nothing (or null), php raises `Passing null is deprecated, instead the last`` |
|       - |   295 | `` * opened directory stream should be provided` and falls back to whatever`` |
|       - |   296 | ` * opendir() handed out last -- and, when there is none or it was closed,` |
|       - |   297 | ` * refuses with a TypeError that names neither the function nor the argument.` |
|       - |   298 | ` */` |
|   20688 |   299 | `static io_private * StreamDirArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int *pRc)` |
|       5 |   300 | `{` |
|       - |   301 | `	io_private *pDev;` |
|   20693 |   302 | `	*pRc = PH7_OK;` |
|   20693 |   303 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      25 |   304 | `		VmErrorFormat(pCtx->pVm,8192 /* E_DEPRECATED */,` |
|       - |   305 | `			"%s(): Passing null is deprecated, instead the last opened "` |
|       8 |   306 | `			"directory stream should be provided",ph7_function_name(pCtx));` |
|      17 |   307 | `		pDev = (io_private *)pCtx->pVm->pLastDir;` |
|      17 |   308 | `		if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) ){` |
|       9 |   309 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError","No resource supplied");` |
|       9 |   310 | `			return 0;` |
|       - |   311 | `		}` |
|       9 |   312 | `		return pDev;` |
|       - |   313 | `	}` |
|   20677 |   314 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - |   315 | `		char zGiven[64];` |
|      16 |   316 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |   317 | `			"%s(): Argument #1 ($dir_handle) must be of type resource or null, %s given",` |
|       5 |   318 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      11 |   319 | `		return 0;` |
|       - |   320 | `	}` |
|   20667 |   321 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|   20667 |   322 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      10 |   323 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |   324 | `			"%s(): Argument #1 ($dir_handle) must be an open stream resource",` |
|       3 |   325 | `			ph7_function_name(pCtx));` |
|       7 |   326 | `		return 0;` |
|       - |   327 | `	}` |
|   20661 |   328 | `	return pDev;` |
|   10325 |   329 | `}` |
|       - |   330 | `/*` |
|       - |   331 | ` * php's screen for a stream-handle ARGUMENT, forward-declared here because every` |
|       - |   332 | ` * f* door below runs it and it is defined with the settings family further down.` |
|       - |   333 | `` * It answers php's TWO TypeErrors -- `Argument #N ($stream) must be of type`` |
|       - |   334 | `` * resource, X given` for something that was never a handle, and `...must be an`` |
|       - |   335 | `` * open stream resource` for a resource whose device is gone, which is what an`` |
|       - |   336 | ` * already-CLOSED handle is -- and 0, with *pRc carrying the throw.` |
|       - |   337 | ` *` |
|       - |   338 | `` * Both used to be one legacy warning ("Expecting an IO handle") and a `false`,`` |
|       - |   339 | ` * at ~24 doors. The name and position are the DOOR's: php calls the same` |
|       - |   340 | `` * argument `$stream` for the byte-stream verbs, `$dir_handle` for the directory`` |
|       - |   341 | `` * trio and `$handle` for pclose(), and the function in the message is the one`` |
|       - |   342 | ` * that was CALLED -- which is how gzread()'s refusal says gzread() and not` |
|       - |   343 | ` * fread(), the same body under another name.` |
|       - |   344 | ` */` |
|       - |   345 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - |   346 | `	const char *zName,int *pRc);` |
|       - |   347 | `/*` |
|       - |   348 | ` * Return the PHP resource-type name for a raw resource handle.` |
|       - |   349 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|       - |   350 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|       - |   351 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|       - |   352 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|       - |   353 | ` */` |
|      98 |   354 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|       5 |   355 | `{` |
|     103 |   356 | `	io_private *pDev = (io_private *)pResource;` |
|     103 |   357 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|       - |   358 | `		/* php names a persistent stream apart, and that name is the only way a` |
|       - |   359 | `		 * script can see that its handle is one. */` |
|      47 |   360 | `		return pDev->bPersist ? "persistent stream" : "stream";` |
|       - |   361 | `	}` |
|      58 |   362 | `	if( pDev && pDev->iMagic == PROC_PRIVATE_MAGIC ){` |
|       - |   363 | `		/* proc_open()'s handle is not a stream and php does not call it one: it` |
|       - |   364 | `` 		 * answered "Unknown" here, so `get_resource_type($proc) === 'process'` `` |
|       - |   365 | `		 * — the documented way to tell a process handle from a pipe — was` |
|       - |   366 | `		 * false. Its header IS an io_private, magic field included, which is` |
|       - |   367 | `		 * what one probe can tell them apart by. */` |
|       4 |   368 | `		return "process";` |
|       - |   369 | `	}` |
|      54 |   370 | `	if( pDev && pDev->iMagic == STREAM_CTX_MAGIC ){` |
|       - |   371 | `		/* stream_context_create()'s handle, and the name php gives it. */` |
|      29 |   372 | `		return "stream-context";` |
|       - |   373 | `	}` |
|      27 |   374 | `	if( pDev && pDev->iMagic == STREAM_BUCKET_MAGIC ){` |
|       - |   375 | `		/* The handle a StreamBucket carries; php shows one there. */` |
|       5 |   376 | `		return "userfilter.bucket";` |
|       - |   377 | `	}` |
|      23 |   378 | `	if( pDev && pDev->iMagic == STREAM_BRIGADE_MAGIC ){` |
|       - |   379 | ``		/* The `$in` and `$out` a userland filter() is handed. */`` |
|       3 |   380 | `		return "userfilter.bucket brigade";` |
|       - |   381 | `	}` |
|      21 |   382 | `	if( pDev && pDev->iMagic == STREAM_FILTER_MAGIC ){` |
|       - |   383 | `		/* stream_filter_append()'s handle. Note the SPACE: php names the context` |
|       - |   384 | ``		 * `stream-context` and the filter `stream filter`. */`` |
|       5 |   385 | `		return "stream filter";` |
|       - |   386 | `	}` |
|       - |   387 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|       - |   388 | `	{` |
|       - |   389 | `		/* ext/zip's two procedural handles, which are not streams at all:` |
|       - |   390 | ``		 * `zip_open()` hands out a `Zip Directory` and `zip_read()` a`` |
|       - |   391 | ``		 * `Zip Entry`, and get_resource_type() is the only way a script tells`` |
|       - |   392 | `		 * one from the other. */` |
|      17 |   393 | `		const char *zZip = PH7_ZipResourceType(pResource);` |
|      17 |   394 | `		if( zZip ){` |
|      11 |   395 | `			return zZip;` |
|       - |   396 | `		}` |
|       - |   397 | `	}` |
|       - |   398 | `#endif` |
|       6 |   399 | `	return "Unknown";` |
|      54 |   400 | `}` |
|       - |   401 | `/*` |
|       - |   402 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|       - |   403 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|       - |   404 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|       - |   405 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|       - |   406 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|       - |   407 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|       - |   408 | ` */` |
|    4296 |   409 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|       5 |   410 | `{` |
|    4301 |   411 | `	io_private *pDev = (io_private *)pResource;` |
|    8578 |   412 | `	return pDev != 0 && (pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC` |
|    4277 |   413 | `		\|\| pDev->iMagic == STREAM_FILTER_CLOSED_MAGIC);` |
|       5 |   414 | `}` |
|       - |   415 | `/*` |
|       - |   416 | ` * bool ftruncate(resource $handle,int64 $size)` |
|       - |   417 | ` *  Truncates a file to a given length.` |
|       - |   418 | ` * Parameters` |
|       - |   419 | ` *  $handle` |
|       - |   420 | ` *   The file pointer.` |
|       - |   421 | ` *   Note:` |
|       - |   422 | ` *    The handle must be open for writing.` |
|       - |   423 | ` * $size` |
|       - |   424 | ` *   The size to truncate to.` |
|       - |   425 | ` * Return` |
|       - |   426 | ` *  TRUE on success or FALSE on failure.` |
|       - |   427 | ` */` |
|      32 |   428 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |   429 | `{` |
|       - |   430 | `	const ph7_io_stream *pStream;` |
|       - |   431 | `	io_private *pDev;` |
|       - |   432 | `	ph7_int64 nSize;` |
|       - |   433 | `	int rc;` |
|      34 |   434 | `	if( nArg < 2 ){` |
|       - |   435 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   436 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   437 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   438 | `		return PH7_OK;` |
|       - |   439 | `	}` |
|       - |   440 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      34 |   441 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      34 |   442 | `	if( pDev == 0 ){` |
|       3 |   443 | `		return rc;` |
|       - |   444 | `	}` |
|      31 |   445 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|      31 |   446 | `	if( nSize < 0 ){` |
|       - |   447 | `		/* php 8: catchable ValueError, raised BEFORE the unsupported-stream` |
|       - |   448 | `		 * check (php-src orders the size check first). PHL used to truncate. */` |
|       3 |   449 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |   450 | `			"ftruncate(): Argument #2 ($size) must be greater than or equal to 0");` |
|       - |   451 | `	}` |
|       - |   452 | `	/* Point to the target IO stream device */` |
|      29 |   453 | `	pStream = pDev->pStream;` |
|      29 |   454 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xTrunc == 0 ){` |
|       - |   455 | `		/* php asks the stream whether it supports truncation AT ALL and says so` |
|       - |   456 | `		 * when it does not -- a socket, php://output, an http:// body, a` |
|       - |   457 | `		 * directory handle. What it says is this sentence, and it is a` |
|       - |   458 | `		 * different event from a truncation that was tried and refused. */` |
|       3 |   459 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Can\'t truncate this stream!");` |
|       3 |   460 | `		ph7_result_bool(pCtx,0);` |
|       3 |   461 | `		return PH7_OK;` |
|       - |   462 | `	}` |
|       - |   463 | `	/* Perform the requested operation */` |
|      27 |   464 | `	rc = pStream->xTrunc(pDev->pHandle,nSize);` |
|      27 |   465 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       5 |   466 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Can\'t truncate this stream!");` |
|       5 |   467 | `		ph7_result_bool(pCtx,0);` |
|       5 |   468 | `		return PH7_OK;` |
|       - |   469 | `	}` |
|       - |   470 | `	/* php does NOT touch the read buffer here: truncating is not a seek, the` |
|       - |   471 | `	 * position does not move, and what the readers already pulled ahead is` |
|       - |   472 | `	 * still what the next read answers. Dropping it made ftell() jump to the` |
|       - |   473 | `	 * device's own offset and the next read start there — past the new end` |
|       - |   474 | ``	 * (`""` where php answers the buffered line) or, after a truncation that`` |
|       - |   475 | `	 * GREW the file, over the NUL padding no php ever hands back. */` |
|       - |   476 | `	/* IO result */` |
|      23 |   477 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      23 |   478 | `	return PH7_OK;` |
|      18 |   479 | `}` |
|       - |   480 | `/*` |
|       - |   481 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|       - |   482 | ` *  Seeks on a file pointer.` |
|       - |   483 | ` * Parameters` |
|       - |   484 | ` *  $handle` |
|       - |   485 | ` *   A file system pointer resource that is typically created using fopen().` |
|       - |   486 | ` * $offset` |
|       - |   487 | ` *   The offset.` |
|       - |   488 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|       - |   489 | ` *   value in offset and set whence to SEEK_END.` |
|       - |   490 | ` *   whence` |
|       - |   491 | ` *   whence values are:` |
|       - |   492 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|       - |   493 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|       - |   494 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|       - |   495 | ` * Return` |
|       - |   496 | ` *  0 on success,-1 on failure` |
|       - |   497 | ` */` |
|     164 |   498 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   499 | `{` |
|       - |   500 | `	const ph7_io_stream *pStream;` |
|       - |   501 | `	io_private *pDev;` |
|       - |   502 | `	ph7_int64 iOfft;` |
|       - |   503 | `	int whence;` |
|       - |   504 | `	int rc;` |
|     169 |   505 | `	if( nArg < 2 ){` |
|       - |   506 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   507 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   508 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 |   509 | `		return PH7_OK;` |
|       - |   510 | `	}` |
|       - |   511 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     169 |   512 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     169 |   513 | `	if( pDev == 0 ){` |
|       5 |   514 | `		return rc;` |
|       - |   515 | `	}` |
|       - |   516 | `	/* Point to the target IO stream device */` |
|     165 |   517 | `	pStream = pDev->pStream;` |
|     165 |   518 | `	if( !pDev->bDir && (pStream == 0 \|\| pStream->xSeek == 0) ){` |
|       3 |   519 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       3 |   520 | `		ph7_result_int(pCtx,-1);` |
|       3 |   521 | `		return PH7_OK;` |
|       - |   522 | `	}` |
|       - |   523 | `	/* Extract the offset */` |
|     163 |   524 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     163 |   525 | `	whence = 0;/* SEEK_SET */` |
|     163 |   526 | `	if( nArg > 2 ){` |
|       - |   527 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|       - |   528 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|       - |   529 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|       - |   530 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|      68 |   531 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|      32 |   532 | `	}` |
|     163 |   533 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|       - |   534 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|       - |   535 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|       - |   536 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|       - |   537 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|       - |   538 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|      13 |   539 | `		ph7_result_int(pCtx,-1);` |
|      13 |   540 | `		return PH7_OK;` |
|       - |   541 | `	}` |
|     151 |   542 | `	if( pDev->bDir ){` |
|       - |   543 | `		/* php's directory stream seeks by REWINDING, whatever the offset and` |
|       - |   544 | `		 * whatever the whence: it answers 0, the next readdir() is the first` |
|       - |   545 | `		 * entry again, and the position it REPORTS does not go back with it. */` |
|       3 |   546 | `		if( pStream && pStream->xRewindDir ){` |
|       3 |   547 | `			pStream->xRewindDir(pDev->pHandle);` |
|       1 |   548 | `		}` |
|       3 |   549 | `		ph7_result_int(pCtx,0);` |
|       3 |   550 | `		return PH7_OK;` |
|       - |   551 | `	}` |
|     149 |   552 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|       - |   553 | `		/* On a FILTERED stream the two positions are unrelated, so a relative` |
|       - |   554 | `		 * seek is resolved against the one the script sees and the device is` |
|       - |   555 | `		 * then placed at the result — php's own model, and the only one under` |
|       - |   556 | ``		 * which `fseek($f,0,SEEK_CUR)` is the no-op it looks like. */`` |
|       7 |   557 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|       5 |   558 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|       2 |   559 | `		}` |
|       7 |   560 | `		rc = pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|       7 |   561 | `		if( rc == PH7_OK ){` |
|       7 |   562 | `			ResetIOPrivate(pDev);` |
|       7 |   563 | `			StreamSeekLanded(pDev,iOfft,0/*SEEK_SET*/);` |
|       7 |   564 | `			pDev->iFiltPos = iOfft;` |
|       4 |   565 | `		}else{` |
|     ! 0 |   566 | `			StreamSeekRefused(pDev);` |
|     ! 0 |   567 | `			if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 |   568 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|     ! 0 |   569 | `			}` |
|       - |   570 | `		}` |
|       7 |   571 | `		ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|       7 |   572 | `		return PH7_OK;` |
|       - |   573 | `	}` |
|     143 |   574 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|       - |   575 | `		/* A relative seek is resolved against the position the STREAM LAYER` |
|       - |   576 | `		 * holds and handed to the device as an absolute one -- php's own` |
|       - |   577 | `		 * conversion. Passing SEEK_CUR through to the descriptor instead moved` |
|       - |   578 | `		 * from wherever the descriptor happened to be: past the read-ahead the` |
|       - |   579 | `		 * line readers buffer, and on an APPEND handle at the end of the file` |
|       - |   580 | `		 * rather than at the bytes written. */` |
|      18 |   581 | `		iOfft += PH7_StreamLogicalTell(pDev);` |
|      18 |   582 | `		whence = 0; /* SEEK_SET */` |
|       7 |   583 | `	}` |
|       - |   584 | `	/* Perform the requested operation */` |
|     143 |   585 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     143 |   586 | `	if( rc == PH7_OK ){` |
|       - |   587 | `		/* Ignore buffered data */` |
|     133 |   588 | `		ResetIOPrivate(pDev);` |
|     133 |   589 | `		StreamSeekLanded(pDev,iOfft,whence);` |
|     133 |   590 | `		if( pDev->pReadFilters ){` |
|     ! 0 |   591 | `			pDev->iFiltPos = pStream->xTell ? pStream->xTell(pDev->pHandle) : 0;` |
|     ! 0 |   592 | `		}` |
|      69 |   593 | `	}else{` |
|      13 |   594 | `		StreamSeekRefused(pDev);` |
|      13 |   595 | `		if( rc == SXERR_NOTIMPLEMENTED ){` |
|       - |   596 | `			/* The device HAS a seek and this handle cannot use it -- php's` |
|       - |   597 | `			 * php://stdout on a pipe or a terminal. Same sentence as no seek. */` |
|       3 |   598 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       1 |   599 | `		}` |
|       - |   600 | `	}` |
|       - |   601 | `	/* IO result */` |
|     143 |   602 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     143 |   603 | `	return PH7_OK;` |
|      87 |   604 | `}` |
|       - |   605 | `/*` |
|       - |   606 | ` * int64 ftell(resource $handle)` |
|       - |   607 | ` *  Returns the current position of the file read/write pointer.` |
|       - |   608 | ` * Parameters` |
|       - |   609 | ` *  $handle` |
|       - |   610 | ` *   The file pointer.` |
|       - |   611 | ` * Return` |
|       - |   612 | ` *  Returns the position of the file pointer referenced by handle` |
|       - |   613 | ` *  as an integer; i.e., its offset into the file stream.` |
|       - |   614 | ` *  FALSE is returned on failure.` |
|       - |   615 | ` */` |
|     180 |   616 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   617 | `{` |
|       - |   618 | `	io_private *pDev;` |
|       - |   619 | `	ph7_int64 iOfft;` |
|       - |   620 | `	int rc;` |
|     185 |   621 | `	if( nArg < 1 ){` |
|       - |   622 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   623 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   624 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   625 | `		return PH7_OK;` |
|       - |   626 | `	}` |
|       - |   627 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     185 |   628 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     185 |   629 | `	if( pDev == 0 ){` |
|       5 |   630 | `		return rc;` |
|       - |   631 | `	}` |
|       - |   632 | `	/* Point to the target IO stream device */` |
|       - |   633 | `	/* No "unimplemented" arm: php's ftell() never refuses a stream. A device` |
|       - |   634 | `	 * that cannot be asked where it is has the engine's own counter answer` |
|       - |   635 | `	 * for it (PH7_StreamLogicalTell). */` |
|       - |   636 | `	/* Perform the requested operation. The device sits past whatever the line` |
|       - |   637 | `	 * readers buffered ahead, so the SCRIPT's position is the device position` |
|       - |   638 | `	 * less the unconsumed remainder — ftell() after fgets("abcdefghij\nrest")` |
|       - |   639 | `	 * is php's 11, not the 15 the device already read. */` |
|     181 |   640 | `	iOfft = PH7_StreamLogicalTell(pDev);` |
|     181 |   641 | `	if( iOfft < 0 ){` |
|       - |   642 | `		/* The device does not know where it is -- php's answer for a stream` |
|       - |   643 | `		 * whose last seek FAILED (PDO's blob handle refuses one past its own` |
|       - |   644 | `		 * end and leaves the position unknown until a seek succeeds). */` |
|       8 |   645 | `		ph7_result_bool(pCtx,0);` |
|       8 |   646 | `		return PH7_OK;` |
|       - |   647 | `	}` |
|       - |   648 | `	/* IO result */` |
|     175 |   649 | `	ph7_result_int64(pCtx,iOfft);` |
|     175 |   650 | `	return PH7_OK;` |
|      95 |   651 | `}` |
|       - |   652 | `/*` |
|       - |   653 | ` * bool rewind(resource $handle)` |
|       - |   654 | ` *  Rewind the position of a file pointer.` |
|       - |   655 | ` * Parameters` |
|       - |   656 | ` *  $handle` |
|       - |   657 | ` *   The file pointer.` |
|       - |   658 | ` * Return` |
|       - |   659 | ` *  TRUE on success or FALSE on failure.` |
|       - |   660 | ` */` |
|     332 |   661 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   662 | `{` |
|       - |   663 | `	const ph7_io_stream *pStream;` |
|       - |   664 | `	io_private *pDev;` |
|       - |   665 | `	int rc;` |
|     337 |   666 | `	if( nArg < 1 ){` |
|       - |   667 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   668 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   669 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   670 | `		return PH7_OK;` |
|       - |   671 | `	}` |
|       - |   672 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     337 |   673 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     337 |   674 | `	if( pDev == 0 ){` |
|       5 |   675 | `		return rc;` |
|       - |   676 | `	}` |
|       - |   677 | `	/* Point to the target IO stream device */` |
|     333 |   678 | `	pStream = pDev->pStream;` |
|     333 |   679 | `	if( pDev->bDir ){` |
|       - |   680 | `		/* Same rewind fseek() gets, and php's rewind() answers TRUE for it. */` |
|       3 |   681 | `		if( pStream && pStream->xRewindDir ){` |
|       3 |   682 | `			pStream->xRewindDir(pDev->pHandle);` |
|       1 |   683 | `		}` |
|       3 |   684 | `		ph7_result_bool(pCtx,1);` |
|       3 |   685 | `		return PH7_OK;` |
|       - |   686 | `	}` |
|     331 |   687 | `	if( pStream == 0 \|\| pStream->xSeek == 0 ){` |
|     ! 0 |   688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|     ! 0 |   689 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   690 | `		return PH7_OK;` |
|       - |   691 | `	}` |
|       - |   692 | `	/* Perform the requested operation */` |
|     331 |   693 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     331 |   694 | `	if( rc == PH7_OK ){` |
|       - |   695 | `		/* Ignore buffered data */` |
|     329 |   696 | `		ResetIOPrivate(pDev);` |
|     329 |   697 | `		StreamSeekLanded(pDev,0,0/*SEEK_SET*/);` |
|     167 |   698 | `	}else{` |
|       3 |   699 | `		StreamSeekRefused(pDev);` |
|       3 |   700 | `		if( rc == SXERR_NOTIMPLEMENTED ){` |
|       3 |   701 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Stream does not support seeking");` |
|       1 |   702 | `		}` |
|       - |   703 | `	}` |
|       - |   704 | `	/* IO result */` |
|     331 |   705 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     331 |   706 | `	return PH7_OK;` |
|     171 |   707 | `}` |
|       - |   708 | `/*` |
|       - |   709 | ` * bool fflush(resource $handle)` |
|       - |   710 | ` *  Flushes the output to a file.` |
|       - |   711 | ` * Parameters` |
|       - |   712 | ` *  $handle` |
|       - |   713 | ` *   The file pointer.` |
|       - |   714 | ` * Return` |
|       - |   715 | ` *  TRUE on success or FALSE on failure.` |
|       - |   716 | ` */` |
|      18 |   717 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |   718 | `{` |
|       - |   719 | `	const ph7_io_stream *pStream;` |
|       - |   720 | `	io_private *pDev;` |
|       - |   721 | `	int rc;` |
|      20 |   722 | `	if( nArg < 1 ){` |
|       - |   723 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   724 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   725 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   726 | `		return PH7_OK;` |
|       - |   727 | `	}` |
|       - |   728 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      20 |   729 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      20 |   730 | `	if( pDev == 0 ){` |
|       3 |   731 | `		return rc;` |
|       - |   732 | `	}` |
|       - |   733 | `	/* Point to the target IO stream device */` |
|      17 |   734 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      17 |   735 | `	pStream = pDev->pStream;` |
|       - |   736 | `	/* The chain first, and whatever the device can do about it second. */` |
|      17 |   737 | `	PH7_StreamFlushWriteChain(pDev);` |
|      17 |   738 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xSync == 0 ){` |
|       - |   739 | `		/* php's php_stream_flush SUCCEEDS when the device has nothing to flush,` |
|       - |   740 | `		 * silently -- which is why fflush() on php://memory, php://output, a` |
|       - |   741 | `		 * pipe, a socket or a directory handle is simply TRUE. Every` |
|       - |   742 | `		 * symfony/console write ends in one of these. */` |
|       5 |   743 | `		ph7_result_bool(pCtx,1);` |
|       5 |   744 | `		return PH7_OK;` |
|       - |   745 | `	}` |
|       - |   746 | `	/* Perform the requested operation */` |
|      13 |   747 | `	rc = pStream->xSync(pDev->pHandle);` |
|       - |   748 | `	/* IO result */` |
|      13 |   749 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      13 |   750 | `	return PH7_OK;` |
|      11 |   751 | `}` |
|       - |   752 | `/*` |
|       - |   753 | ` * php's end-of-file flag is set AFTER THE FACT: a stream is at EOF once one of` |
|       - |   754 | ` * its OWN reads has come back empty, and asking the question never reads. PHL` |
|       - |   755 | ` * used to probe the device instead — a read-ahead of up to 4 KB from inside` |
|       - |   756 | ` * feof() — which answered TRUE on a handle nothing had read yet (an empty file,` |
|       - |   757 | ` * a fresh php://memory), answered TRUE on a WRITE-only handle because the` |
|       - |   758 | `` * refused read looked like an end, and BLOCKED on `feof(STDIN)` with no input`` |
|       - |   759 | ` * waiting: a question about a stream is not a read of it. bEof is that flag,` |
|       - |   760 | ` * set wherever a read here comes back with nothing and cleared by every seek.` |
|       - |   761 | ` */` |
|       - |   762 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer);` |
|   55105 |   763 | `static int StreamEofCommon(io_private *pDev,int bLive)` |
|       5 |   764 | `{` |
|       - |   765 | `	int bEof;` |
|   55110 |   766 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - |   767 | `		/* Buffered bytes are not an end. */` |
|   42941 |   768 | `		return 0;` |
|       - |   769 | `	}` |
|   12174 |   770 | `	if( IoPrivateUwrapEof(pDev,&bEof) ){` |
|       - |   771 | `		/* A userland wrapper answers the question itself — php calls its` |
|       - |   772 | `		 * streamWrapper::stream_eof() rather than inferring anything. */` |
|       7 |   773 | `		return bEof;` |
|       - |   774 | `	}` |
|       - |   775 | `#ifdef PH7_ENABLE_NET` |
|   12168 |   776 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - |   777 | `		/* The http handle owns the end: the socket may have closed while its` |
|       - |   778 | `		 * dechunker still holds bytes nobody has taken. */` |
|      34 |   779 | `		return PH7_HttpStreamAtEof(pDev->pHandle);` |
|       - |   780 | `	}` |
|   12134 |   781 | `	if( bLive && !pDev->bEof && pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|       - |   782 | `		/* php asks the SOCKET whether it is still alive rather than remembering` |
|       - |   783 | `		 * a read that came back empty, and LATCHES a "no" — which is why the` |
|       - |   784 | ``		 * `eof` key of stream_get_meta_data() reads true only after a feof()`` |
|       - |   785 | `		 * has run, and false before one. */` |
|      90 |   786 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|      90 |   787 | `		if( !PH7_NetIsAlive(pSock->sock) ){` |
|      22 |   788 | `			pSock->bEof = 1;` |
|      22 |   789 | `			pDev->bEof = 1;` |
|      17 |   790 | `		}` |
|      43 |   791 | `	}` |
|       - |   792 | `#else` |
|       - |   793 | `	SXUNUSED(bLive);` |
|       - |   794 | `#endif` |
|   12134 |   795 | `	return pDev->bEof != 0;` |
|   27242 |   796 | `}` |
|       - |   797 | `/*` |
|       - |   798 | ` * php's php_stream_eof(): the question feof() asks, which for a socket PROBES` |
|       - |   799 | ` * the descriptor (see PH7_NetIsAlive) and latches what it finds.` |
|       - |   800 | ` */` |
|   54967 |   801 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev)` |
|       5 |   802 | `{` |
|   54972 |   803 | `	return StreamEofCommon(pDev,1);` |
|       5 |   804 | `}` |
|       - |   805 | `/*` |
|       - |   806 | ` * bool feof(resource $handle)` |
|       - |   807 | ` *  Tests for end-of-file on a file pointer.` |
|       - |   808 | ` * Parameters` |
|       - |   809 | ` *  $handle` |
|       - |   810 | ` *   The file pointer.` |
|       - |   811 | ` * Return` |
|       - |   812 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|       - |   813 | ` */` |
|   54675 |   814 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   815 | `{` |
|       - |   816 | `	io_private *pDev;` |
|       - |   817 | `	int rc;` |
|   54680 |   818 | `	if( nArg < 1 ){` |
|       - |   819 | `		/* Missing/Invalid arguments */` |
|     ! 0 |   820 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |   821 | `		ph7_result_bool(pCtx,1);` |
|     ! 0 |   822 | `		return PH7_OK;` |
|       - |   823 | `	}` |
|       - |   824 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|   54680 |   825 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   54680 |   826 | `	if( pDev == 0 ){` |
|      17 |   827 | `		return rc;` |
|       - |   828 | `	}` |
|   54664 |   829 | `	if( !StreamHasReader(pDev) ){` |
|       - |   830 | `		/* php's end-of-file flag is raised by a READ that came back empty, and` |
|       - |   831 | `		 * a handle nothing can read never had one: feof() is FALSE, silently. */` |
|       3 |   832 | `		ph7_result_bool(pCtx,0);` |
|       3 |   833 | `		return PH7_OK;` |
|       - |   834 | `	}` |
|   54662 |   835 | `	rc = PH7_StreamAtEof(pDev);` |
|       - |   836 | `	/* EOF or not */` |
|   54662 |   837 | `	ph7_result_bool(pCtx,rc != 0);` |
|   54662 |   838 | `	return PH7_OK;` |
|   27027 |   839 | `}` |
|       - |   840 | `/*` |
|       - |   841 | ` * Read n bytes from the underlying IO stream device.` |
|       - |   842 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|       - |   843 | ` * [i.e: IO error ] or EOF.` |
|       - |   844 | ` *` |
|       - |   845 | ` * This is the read every SCRIPT-level reader goes through, because it drains` |
|       - |   846 | ` * the line readers' read-ahead buffer first: a stream that fgets() has already` |
|       - |   847 | ` * pulled a block out of is positioned where the SCRIPT thinks it is, not where` |
|       - |   848 | ` * the device is. Anything reading from a caller's handle has to use this and` |
|       - |   849 | ` * not the device's own xRead.` |
|       - |   850 | ` */` |
|       - |   851 | `/*` |
|       - |   852 | ` * One read from the device, with the timeout bookkeeping php does for EVERY` |
|       - |   853 | `` * reader: `timed_out` describes the last read, so it is cleared on the way in`` |
|       - |   854 | ` * and set only by a wait that expired. Without the clear, one quiet period marks` |
|       - |   855 | ` * a handle timed out for the rest of its life — and now that every socket` |
|       - |   856 | ` * carries default_socket_timeout, that is every socket that ever waited. And` |
|       - |   857 | ` * without the set being here, only fread() would ever report one: fgets(),` |
|       - |   858 | ` * fgetc(), stream_get_line(), stream_get_contents() and fpassthru() all read` |
|       - |   859 | ` * through their own loops.` |
|       - |   860 | ` */` |
|   15423 |   861 | `static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |   862 | `{` |
|       - |   863 | `	ph7_int64 n;` |
|   15428 |   864 | `	pDev->bTimedOut = 0;` |
|   15428 |   865 | `	errno = 0;` |
|   15428 |   866 | `	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);` |
|   15423 |   867 | `	if( n < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK)` |
|      96 |   868 | `	 && pDev->bHasTimeout && !pDev->bNonBlock ){` |
|       5 |   869 | `		pDev->bTimedOut = 1;` |
|       2 |   870 | `	}` |
|   15428 |   871 | `	return n;` |
|       5 |   872 | `}` |
|       - |   873 | `/*` |
|       - |   874 | ` * Serve a read from the FILTERED side of a handle. A filter changes the byte` |
|       - |   875 | ` * count — base64 makes four out of three, dechunk throws whole runs away — so` |
|       - |   876 | ` * what the chain produced cannot go straight into the caller's buffer: it waits` |
|       - |   877 | ` * in sFilt and is handed out from there.` |
|       - |   878 | ` *` |
|       - |   879 | ` * The fill loop runs until sFilt holds what was asked for or the device is` |
|       - |   880 | ` * spent, which is what keeps the caller's invariant intact: a SHORT answer here` |
|       - |   881 | ` * still means end of file, exactly as it does for an unfiltered read.` |
|       - |   882 | ` */` |
|     304 |   883 | `static ph7_int64 IoPrivateFilteredRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       3 |   884 | `{` |
|     307 |   885 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pReadFilters;` |
|       - |   886 | `	sxu32 nAvail;` |
|       - |   887 | `	ph7_int64 n;` |
|       - |   888 |  |
|    2380 |   889 | `	while( pChain != 0 && !pDev->bFiltDone` |
|    2231 |   890 | `	    && (ph7_int64)(SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft) < nLen ){` |
|       - |   891 | `		char zRaw[8192];` |
|    1373 |   892 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zRaw);` |
|       - |   893 | `		ph7_int64 nRaw;` |
|       - |   894 | `		int iStatus;` |
|    1373 |   895 | `		if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|    1099 |   896 | `			nAsk = (ph7_int64)pDev->nChunk;` |
|     549 |   897 | `		}` |
|    1373 |   898 | `		nRaw = IoPrivateRawRead(pDev,zRaw,nAsk);` |
|    1373 |   899 | `		if( nRaw < 0 ){` |
|     ! 0 |   900 | `			if( SyBlobLength(&pDev->sFilt) <= pDev->nFiltOfft ){` |
|       - |   901 | `				/* Nothing was ever produced: the IO error is the answer. */` |
|     ! 0 |   902 | `				return nRaw;` |
|       - |   903 | `			}` |
|     ! 0 |   904 | `			break;` |
|       - |   905 | `		}` |
|       - |   906 | `		{` |
|    1373 |   907 | `			int iF = nRaw > 0 ? PHL_PSFS_FLAG_NORMAL : PHL_PSFS_FLAG_FLUSH_CLOSE;` |
|       - |   908 | `			/* The device's end closes EVERY filter on the stream, not just the` |
|       - |   909 | `			 * head: each one's tail has to travel through the rest. */` |
|    1373 |   910 | `			iStatus = PH7_FilterChainProcess(pChain,zRaw,(sxu32)nRaw,iF,iF,&pDev->sFilt,0);` |
|       - |   911 | `		}` |
|    1373 |   912 | `		if( nRaw == 0 ){` |
|       - |   913 | `			/* The device is spent, and the call above was the chain's CLOSING` |
|       - |   914 | `			 * one: running it again would make a buffering filter emit its tail` |
|       - |   915 | `			 * twice, so the chain is finished for good. */` |
|     143 |   916 | `			pDev->bFiltDone = 1;` |
|     143 |   917 | `			break;` |
|       - |   918 | `		}` |
|    1233 |   919 | `		if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|       - |   920 | `			/* A refusal ends the reading. php reports it to the reader as a` |
|       - |   921 | ``			 * FAILURE — `fread()` answers false, once — and only then as an end`` |
|       - |   922 | `			 * of file; what earlier calls already produced is still the` |
|       - |   923 | `			 * reader's, so the failure waits behind it. */` |
|       5 |   924 | `			pDev->bFiltDone = 1;` |
|       5 |   925 | `			pDev->bFiltErr = 1;` |
|       5 |   926 | `			break;` |
|       - |   927 | `		}` |
|       3 |   928 | `	}` |
|     307 |   929 | `	nAvail = SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|     307 |   930 | `	if( nAvail < 1 ){` |
|     143 |   931 | `		SyBlobReset(&pDev->sFilt);` |
|     143 |   932 | `		pDev->nFiltOfft = 0;` |
|     143 |   933 | `		if( pDev->bFiltErr ){` |
|       5 |   934 | `			pDev->bFiltErr = 0;   /* reported once; the read after it is an end */` |
|       5 |   935 | `			return -1;` |
|       - |   936 | `		}` |
|     139 |   937 | `		return pChain != 0 ? 0 : IoPrivateRawRead(pDev,pBuf,nLen);` |
|       - |   938 | `	}` |
|     167 |   939 | `	n = (ph7_int64)nAvail;` |
|     167 |   940 | `	if( n > nLen ){` |
|      25 |   941 | `		n = nLen;` |
|      12 |   942 | `	}` |
|     167 |   943 | `	SyMemcpy(SyBlobDataAt(&pDev->sFilt,pDev->nFiltOfft),pBuf,(sxu32)n);` |
|     167 |   944 | `	pDev->nFiltOfft += (sxu32)n;` |
|     167 |   945 | `	pDev->iFiltPos += n;` |
|     167 |   946 | `	if( pDev->nFiltOfft >= SyBlobLength(&pDev->sFilt) ){` |
|     143 |   947 | `		SyBlobReset(&pDev->sFilt);` |
|     143 |   948 | `		pDev->nFiltOfft = 0;` |
|      70 |   949 | `	}` |
|     167 |   950 | `	return n;` |
|     155 |   951 | `}` |
|   14357 |   952 | `static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |   953 | `{` |
|       - |   954 | `	ph7_int64 n;` |
|   14362 |   955 | `	if( !StreamHasReader(pDev) ){` |
|       - |   956 | `		/* No reader at all: php's silent false, and never the device's byte op` |
|       - |   957 | `		 * on a handle it does not own. */` |
|     ! 0 |   958 | `		return -1;` |
|       - |   959 | `	}` |
|   14362 |   960 | `	if( pDev->pReadFilters != 0 \|\| SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|       - |   961 | `		/* Bytes can still be waiting after the last read filter was REMOVED:` |
|       - |   962 | `		 * php flushes a filter on its way out and what it emitted belongs to` |
|       - |   963 | `		 * the reader that comes next. */` |
|     307 |   964 | `		return IoPrivateFilteredRead(pDev,pBuf,nLen);` |
|       - |   965 | `	}` |
|       - |   966 | `	/* Ask the device where it started BEFORE moving it: afterwards it answers` |
|       - |   967 | `	 * where the read left it, and the bytes just read would be counted twice. */` |
|   14058 |   968 | `	StreamSeedPosition(pDev);` |
|   14058 |   969 | `	errno = 0;` |
|   14058 |   970 | `	n = IoPrivateRawRead(pDev,pBuf,nLen);` |
|   14053 |   971 | `	if( n > 0 && is_php_stream(pDev->pStream)` |
|    2918 |   972 | `	 && PH7_PhpStreamTempDrained(pDev->pHandle) ){` |
|       - |   973 | `		/* php's php://temp raises its end flag as soon as a read has consumed` |
|       - |   974 | `		 * the buffer, one read before php://memory does. feof() still answers` |
|       - |   975 | `		 * false while the line readers hold bytes -- PH7_StreamAtEof() asks the` |
|       - |   976 | `		 * buffer first, which is php's own rule. */` |
|       5 |   977 | `		pDev->bEof = 1;` |
|       2 |   978 | `	}` |
|   14058 |   979 | `	if( n > 0 ){` |
|       - |   980 | `		/* Where the SCRIPT will be once it has taken these bytes: the counter` |
|       - |   981 | `		 * ftell() reads, on every device. */` |
|    5704 |   982 | `		pDev->iPos += n;` |
|    2818 |   983 | `	}` |
|   14058 |   984 | `	if( n < 0 ){` |
|       - |   985 | `		/* LATCH the failure for the reader to report. php's notice comes from` |
|       - |   986 | `		 * the stream op, which knows the errno but not which builtin is asking;` |
|       - |   987 | `		 * here the builtin knows how to report and the device knows why, so the` |
|       - |   988 | `		 * two meet at the latch -- the same shape the socket write already uses.` |
|       - |   989 | `		 * Cleared by whoever reports it, so one failure is announced once.` |
|       - |   990 | `		 *` |
|       - |   991 | `		 * EAGAIN is not a failure: on a NON-BLOCKING stream it means "nothing to` |
|       - |   992 | `		 * read right now", which php answers with an empty read and no diagnostic` |
|       - |   993 | `		 * at all (its read op only reports when the operation itself failed). An` |
|       - |   994 | `		 * EINTR read is the same "try again". Latching either made` |
|       - |   995 | ``		 * `stream_get_contents()` on a non-blocking proc_open() pipe -- the`` |
|       - |   996 | `		 * everyday shape, and what monolog's ProcessHandler does on every write --` |
|       - |   997 | `		 * raise a notice php never raises. */` |
|     113 |   998 | `		int iRdErr = errno ? errno : EIO;` |
|     108 |   999 | `		if( iRdErr != EAGAIN && iRdErr != EINTR` |
|       - |  1000 | `#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN` |
|       5 |  1001 | `		 && iRdErr != EWOULDBLOCK` |
|       - |  1002 | `#endif` |
|       - |  1003 | `		){` |
|      39 |  1004 | `			pDev->iLastReadErr = iRdErr;` |
|      17 |  1005 | `		}` |
|      54 |  1006 | `	}` |
|   14058 |  1007 | `	return n;` |
|    7140 |  1008 | `}` |
|       - |  1009 | `/*` |
|       - |  1010 | `` * php's `fread(): Read of 8192 bytes failed with errno=9 Bad file descriptor`:`` |
|       - |  1011 | ` * the NOTICE its plain-file read op raises when the device refuses -- a read` |
|       - |  1012 | ` * from a handle opened write-only being the everyday case. The COUNT is not` |
|       - |  1013 | ` * what the caller asked for: php fills its read buffer, so it reports the` |
|       - |  1014 | ` * CHUNK size (8192 by default, whatever stream_set_chunk_size() left` |
|       - |  1015 | ` * otherwise) at every reader. Silent for every other device, as php's is.` |
|       - |  1016 | ` */` |
|    7799 |  1017 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev)` |
|       5 |  1018 | `{` |
|       - |  1019 | `	int iErr;` |
|    7804 |  1020 | `	if( pDev == 0 \|\| pDev->iLastReadErr == 0 ){` |
|    7770 |  1021 | `		return;` |
|       - |  1022 | `	}` |
|      39 |  1023 | `	iErr = pDev->iLastReadErr;` |
|      39 |  1024 | `	pDev->iLastReadErr = 0;` |
|      39 |  1025 | `	if( pDev->pStream != pCtx->pVm->pDefStream ){` |
|      12 |  1026 | `		return;` |
|       - |  1027 | `	}` |
|      27 |  1028 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       - |  1029 | `		"Read of %u bytes failed with errno=%d %s",` |
|      26 |  1030 | `		pDev->nChunk > 0 ? pDev->nChunk : 8192u,iErr,VfsStrerror(iErr));` |
|    3899 |  1031 | `}` |
|    2583 |  1032 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|       5 |  1033 | `{` |
|    2588 |  1034 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|    2588 |  1035 | `	char *zBuf = (char *)pBuf;` |
|       - |  1036 | `	ph7_int64 n,nRead;` |
|    2588 |  1037 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    2588 |  1038 | `	if( n > 0 ){` |
|      17 |  1039 | `		if( n > nLen ){` |
|       9 |  1040 | `			n = nLen;` |
|       3 |  1041 | `		}` |
|       - |  1042 | `		/* Copy the buffered data */` |
|      17 |  1043 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|       - |  1044 | `		/* Update the read offset */` |
|      17 |  1045 | `		pDev->nOfft += (sxu32)n;` |
|      17 |  1046 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - |  1047 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|       9 |  1048 | `			SyBlobReset(&pDev->sBuffer);` |
|       9 |  1049 | `			pDev->nOfft = 0;` |
|       4 |  1050 | `		}` |
|      17 |  1051 | `		nLen -= n;` |
|      17 |  1052 | `		if( nLen < 1 ){` |
|       - |  1053 | `			/* All done */` |
|       9 |  1054 | `			return n;` |
|       - |  1055 | `		}` |
|       - |  1056 | `		/* Advance the cursor */` |
|       9 |  1057 | `		zBuf += n;` |
|       4 |  1058 | `	}` |
|       - |  1059 | `	/* Read without buffering */` |
|    2582 |  1060 | `	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);` |
|    2577 |  1061 | `	if( nRead == 0` |
|    2016 |  1062 | `	 \|\| (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0` |
|    1103 |  1063 | `	     && pStream->xTell(pDev->pHandle) >= 0) ){` |
|       - |  1064 | `		/* A read that came back with nothing IS php's end-of-file event, and` |
|       - |  1065 | `		 * so is a SHORT one on a device that can say where it IS: php fills` |
|       - |  1066 | ``		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs`` |
|       - |  1067 | `		 * the second read that finds the end. The position query is what tells` |
|       - |  1068 | `		 * a regular file from a FIFO — both arrive here through the same file` |
|       - |  1069 | `		 * device, and a short read from a fifo, a pipe or a socket means only` |
|       - |  1070 | `		 * that less had arrived, so latching there would end` |
|       - |  1071 | ``		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A`` |
|       - |  1072 | `		 * NEGATIVE answer is an IO error and never latches. */` |
|    1531 |  1073 | `		pDev->bEof = 1;` |
|     755 |  1074 | `	}` |
|    2582 |  1075 | `	if( nRead > 0 ){` |
|    1356 |  1076 | `		n += nRead;` |
|    1875 |  1077 | `	}else if( n < 1 ){` |
|       - |  1078 | `		/* EOF or IO error */` |
|    1225 |  1079 | `		return nRead;` |
|       - |  1080 | `	}` |
|    1362 |  1081 | `	return n;` |
|    1258 |  1082 | `}` |
|       - |  1083 | `/*` |
|       - |  1084 | ` * Every SCRIPT-level write goes through here, because a handle can carry a` |
|       - |  1085 | ` * WRITE chain: php runs what the script wrote through the filters before the` |
|       - |  1086 | ` * device sees any of it, and a filter changes the byte count — so what reaches` |
|       - |  1087 | ` * the device is not what was handed in, while what fwrite() ANSWERS still is` |
|       - |  1088 | ` * (php reports the bytes it CONSUMED, not the bytes it emitted).` |
|       - |  1089 | ` */` |
|     867 |  1090 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)` |
|       5 |  1091 | `{` |
|     872 |  1092 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|       - |  1093 | `	SyBlob sOut;` |
|       - |  1094 | `	ph7_int64 nWr;` |
|       - |  1095 | `	int iStatus;` |
|     872 |  1096 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 \|\| pDev->bDir ){` |
|     ! 0 |  1097 | `		return -1;` |
|       - |  1098 | `	}` |
|     872 |  1099 | `	StreamSeedPosition(pDev);` |
|     872 |  1100 | `	if( pChain == 0 ){` |
|     830 |  1101 | `		ph7_int64 nRaw = pDev->pStream->xWrite(pDev->pHandle,pData,nLen);` |
|     830 |  1102 | `		if( nRaw > 0 ){` |
|     756 |  1103 | `			pDev->iPos += nRaw;` |
|     354 |  1104 | `		}` |
|     830 |  1105 | `		return nRaw;` |
|       - |  1106 | `	}` |
|      46 |  1107 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|      46 |  1108 | `	iStatus = PH7_FilterChainProcess(pChain,pData,(sxu32)nLen,` |
|       - |  1109 | `		PHL_PSFS_FLAG_NORMAL,PHL_PSFS_FLAG_NORMAL,&sOut,0);` |
|      46 |  1110 | `	if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|       5 |  1111 | `		SyBlobRelease(&sOut);` |
|       5 |  1112 | `		return -1;` |
|       - |  1113 | `	}` |
|      42 |  1114 | `	nWr = 0;` |
|      42 |  1115 | `	if( SyBlobLength(&sOut) > 0 ){` |
|      55 |  1116 | `		nWr = pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|      34 |  1117 | `			(ph7_int64)SyBlobLength(&sOut));` |
|      17 |  1118 | `	}` |
|      42 |  1119 | `	SyBlobRelease(&sOut);` |
|      42 |  1120 | `	if( nWr < 0 ){` |
|     ! 0 |  1121 | `		return -1;` |
|       - |  1122 | `	}` |
|       - |  1123 | `	/* A filter that held its input back (FEED_ME) still consumed it: php's` |
|       - |  1124 | `	 * fwrite() answers the length it was given, and moves the position by it. */` |
|      42 |  1125 | `	pDev->iPos += nLen;` |
|      42 |  1126 | `	return nLen;` |
|     417 |  1127 | `}` |
|       - |  1128 | `/*` |
|       - |  1129 | ` * php's fflush() flushes the WRITE CHAIN before the device: its` |
|       - |  1130 | ` * php_stream_flush runs every filter with a NON-closing flush, so a filter that` |
|       - |  1131 | ` * has been holding bytes back emits what it has and stays open. PHL flushed the` |
|       - |  1132 | ` * device alone, which is invisible for a filter that buffers nothing and very` |
|       - |  1133 | `` * visible for one that does -- a `zlib.deflate` chain wrote NOTHING until the`` |
|       - |  1134 | ` * handle closed, where php's had already emitted the deflate stream up to a` |
|       - |  1135 | ` * sync point.` |
|       - |  1136 | ` */` |
|      16 |  1137 | `PH7_PRIVATE void PH7_StreamFlushWriteChain(io_private *pDev)` |
|       1 |  1138 | `{` |
|      17 |  1139 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|       - |  1140 | `	SyBlob sOut;` |
|      17 |  1141 | `	if( pChain == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|      15 |  1142 | `		return;` |
|       - |  1143 | `	}` |
|       3 |  1144 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|       2 |  1145 | `	if( PH7_FilterChainProcess(pChain,0,0,PHL_PSFS_FLAG_FLUSH_INC,` |
|       1 |  1146 | `			PHL_PSFS_FLAG_FLUSH_INC,&sOut,0) != PHL_PSFS_ERR_FATAL` |
|       3 |  1147 | `	 && SyBlobLength(&sOut) > 0 ){` |
|     ! 0 |  1148 | `		pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|     ! 0 |  1149 | `			(ph7_int64)SyBlobLength(&sOut));` |
|     ! 0 |  1150 | `	}` |
|       3 |  1151 | `	SyBlobRelease(&sOut);` |
|       9 |  1152 | `}` |
|       - |  1153 | `/*` |
|       - |  1154 | ` * Extract a single line from the buffered input.` |
|       - |  1155 | ` */` |
|   47562 |  1156 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|       5 |  1157 | `{` |
|       - |  1158 | `	const char *zIn,*zEnd,*zPtr;` |
|   47567 |  1159 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   47567 |  1160 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   47567 |  1161 | `	zPtr = zIn;` |
| 1923113 |  1162 | `	while( zIn < zEnd ){` |
| 1922718 |  1163 | `		if( zIn[0] == '\n' ){` |
|       - |  1164 | `			/* Line found */` |
|   47172 |  1165 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   47172 |  1166 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   47172 |  1167 | `			*pzLine = zPtr;` |
|   47172 |  1168 | `			return SXRET_OK;` |
|       - |  1169 | `		}` |
| 1875551 |  1170 | `		zIn++;` |
|       5 |  1171 | `	}` |
|       - |  1172 | `	/* No line were found */` |
|     400 |  1173 | `	return SXERR_NOTFOUND;` |
|   23476 |  1174 | `}` |
|       - |  1175 | `/*` |
|       - |  1176 | ` * Read a single line from the underlying IO stream device.` |
|       - |  1177 | ` */` |
|   54457 |  1178 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|       5 |  1179 | `{` |
|       - |  1180 | `	char zBuf[8192];` |
|       - |  1181 | `	ph7_int64 n;` |
|       - |  1182 | `	sxi32 rc;` |
|   54462 |  1183 | `	n = 0;` |
|   54462 |  1184 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - |  1185 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|   11384 |  1186 | `		SyBlobReset(&pDev->sBuffer);` |
|   11384 |  1187 | `		pDev->nOfft = 0;` |
|    5684 |  1188 | `	}` |
|   54462 |  1189 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - |  1190 | `		/* Check if there is a line */` |
|   43083 |  1191 | `		rc = GetLine(pDev,&n,pzData);` |
|   43083 |  1192 | `		if( rc == SXRET_OK ){` |
|       - |  1193 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|       - |  1194 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|   42943 |  1195 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|       3 |  1196 | `				n = nMaxLen;` |
|       1 |  1197 | `			}` |
|   42943 |  1198 | `			pDev->nOfft += (sxu32)n;` |
|   42943 |  1199 | `			return n;` |
|       - |  1200 | `		}` |
|      70 |  1201 | `	}` |
|       - |  1202 | `	/* Perform the read operation until a new line is extracted or length` |
|       - |  1203 | `	 * limit is reached.` |
|       - |  1204 | `	 */` |
|    5863 |  1205 | `	for(;;){` |
|     108 |  1206 | `		{` |
|       - |  1207 | `			/* php fills its read buffer one CHUNK at a time, and` |
|       - |  1208 | `			 * stream_set_chunk_size() is how a script asks for a smaller one. */` |
|   11741 |  1209 | `			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   11741 |  1210 | `			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|      30 |  1211 | `				nAsk = (ph7_int64)pDev->nChunk;` |
|      15 |  1212 | `			}` |
|   11741 |  1213 | `			if( nMaxLen > 0 && nMaxLen < nAsk ){` |
|      75 |  1214 | `				nAsk = nMaxLen;` |
|      37 |  1215 | `			}` |
|   11741 |  1216 | `			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);` |
|       - |  1217 | `		}` |
|   11741 |  1218 | `		if( n == 0 ){` |
|    7253 |  1219 | `			pDev->bEof = 1;` |
|    3619 |  1220 | `		}` |
|   11741 |  1221 | `		if( n < 1 ){` |
|       - |  1222 | `			/* EOF or IO error */` |
|    7257 |  1223 | `			break;` |
|       - |  1224 | `		}` |
|       - |  1225 | `		/* Append the data just read */` |
|    4489 |  1226 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|       - |  1227 | `		/* Try to extract a line */` |
|    4489 |  1228 | `		rc = GetLine(pDev,&n,pzData);` |
|    4489 |  1229 | `		if( rc == SXRET_OK ){` |
|       - |  1230 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|       - |  1231 | `			 * cap, newline included, is left buffered for the next read. */` |
|    4234 |  1232 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|       7 |  1233 | `				n = nMaxLen;` |
|       3 |  1234 | `			}` |
|    4234 |  1235 | `			pDev->nOfft += (sxu32)n;` |
|    4234 |  1236 | `			return n;` |
|       - |  1237 | `		}` |
|     260 |  1238 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|       - |  1239 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|       - |  1240 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|       - |  1241 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|       - |  1242 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|       - |  1243 | `			 * buffer once it is fully consumed. */` |
|      39 |  1244 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|      39 |  1245 | `			n = nMaxLen;` |
|      39 |  1246 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|      39 |  1247 | `			return n;` |
|       - |  1248 | `		}` |
|       5 |  1249 | `	}` |
|    7257 |  1250 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|       - |  1251 | `		/* Read limit reached,return the available data */` |
|     308 |  1252 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     308 |  1253 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|       - |  1254 | `		/* Reset the working buffer */` |
|     308 |  1255 | `		SyBlobReset(&pDev->sBuffer);` |
|     308 |  1256 | `		pDev->nOfft = 0;` |
|     152 |  1257 | `	}` |
|    7257 |  1258 | `	return n;` |
|   26918 |  1259 | `}` |
|       - |  1260 | `/*` |
|       - |  1261 | ` * Open an IO stream handle.` |
|       - |  1262 | ` * Notes on stream:` |
|       - |  1263 | ` * According to the PHP reference manual.` |
|       - |  1264 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|       - |  1265 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|       - |  1266 | ` * to an arbitrary locations within the stream.` |
|       - |  1267 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|       - |  1268 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|       - |  1269 | ` * on a remote server.` |
|       - |  1270 | ` * A stream is referenced as: scheme://target` |
|       - |  1271 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|       - |  1272 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|       - |  1273 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|       - |  1274 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|       - |  1275 | ` *  with a path appended.` |
|       - |  1276 | ` *` |
|       - |  1277 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|       - |  1278 | ` * Please refer to the official documentation for a full discussion.` |
|       - |  1279 | ` * This function return a handle on success. Otherwise null.` |
|       - |  1280 | ` */` |
|   45955 |  1281 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|       - |  1282 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)` |
|       5 |  1283 | `{` |
|   45960 |  1284 | `	void *pHandle = 0; /* cc warning */` |
|       - |  1285 | `	SyString sFile;` |
|       - |  1286 | `	ph7_value sDummy;` |
|       - |  1287 | `	int rc;` |
|   45960 |  1288 | `	if( pStream == 0 ){` |
|       - |  1289 | `		/* No such stream device. The armed context describes THIS open and` |
|       - |  1290 | `		 * nothing else, so it is dropped on every exit — a caller that armed one` |
|       - |  1291 | `		 * and returned early must not leave it for the next open to pick up. */` |
|     ! 0 |  1292 | `		pVm->pOpenCtx = 0;` |
|     ! 0 |  1293 | `		pVm->zOpenMode[0] = 0;` |
|     ! 0 |  1294 | `		if( pVm->nOpenDepth < 1 ){` |
|     ! 0 |  1295 | `			pVm->zOpenErr = 0;` |
|     ! 0 |  1296 | `		}` |
|     ! 0 |  1297 | `		return 0;` |
|       - |  1298 | `	}` |
|       - |  1299 | `	/* Arm the reason THIS open would report. php's default for a wrapper that` |
|       - |  1300 | `	 * logs nothing of its own is a flat "operation failed"; only the plain-file` |
|       - |  1301 | `	 * wrapper reports an errno, which is why every other one used to print` |
|       - |  1302 | ``	 * whatever errno was left over — `Success` for a failure, among others. An`` |
|       - |  1303 | `	 * xOpen body may replace it through PH7_StreamSetOpenError(). */` |
|   45960 |  1304 | `	if( pVm->nOpenDepth < 1 ){` |
|   45858 |  1305 | `		pVm->zOpenErr = pStream == pVm->pDefStream ? 0 : "operation failed";` |
|   45858 |  1306 | `		pVm->zOpenCaller = zCaller;` |
|   22860 |  1307 | `	}` |
|   45960 |  1308 | `	if( pStream->xOpen == 0 ){` |
|       - |  1309 | `		/* A wrapper with a dir_opener and NOTHING else — glob:// is php's one,` |
|       - |  1310 | `		 * and this is php's sentence for it. Reached before the call, because` |
|       - |  1311 | `		 * the call would be through a null pointer. */` |
|       7 |  1312 | `		pVm->pOpenCtx = 0;` |
|       7 |  1313 | `		pVm->zOpenMode[0] = 0;` |
|       7 |  1314 | `		if( pVm->nOpenDepth < 1 ){` |
|       7 |  1315 | `			pVm->zOpenErr = "wrapper does not support stream open";` |
|       3 |  1316 | `		}` |
|       7 |  1317 | `		return 0;` |
|       - |  1318 | `	}` |
|       - |  1319 | `	/* A wrapper registered with STREAM_IS_URL speaks to the network, and php lets` |
|       - |  1320 | `	 * the configuration turn that off: allow_url_fopen for an ordinary open,` |
|       - |  1321 | `	 * allow_url_include for the one that EXECUTES what comes back — which is off` |
|       - |  1322 | `	 * by default, because including a remote file is the classic RFI. */` |
|   45954 |  1323 | `	if( PH7_StreamIsUrlWrapper(pStream) ){` |
|       - |  1324 | `		/* php tests BOTH, in this order, and words them differently. Without` |
|       - |  1325 | `		 * allow_url_fopen the wrapper is not FOUND at all -- php's lookup` |
|       - |  1326 | `		 * declines to hand it over, so the caller reports the missing-wrapper` |
|       - |  1327 | `		 * sentence and names the URI -- while allow_url_include, off by default,` |
|       - |  1328 | `		 * refuses only the open that would EXECUTE what came back, and that one` |
|       - |  1329 | `		 * names the wrapper and the directive. */` |
|       - |  1330 | `		SyString sCaller;` |
|     373 |  1331 | `		SyStringInitFromBuf(&sCaller,zCaller ? zCaller : "",zCaller ? SyStrlen(zCaller) : 0);` |
|     373 |  1332 | `		if( !PH7_VmIniGetBool(pVm,"allow_url_fopen",1) ){` |
|       - |  1333 | `			/* TWO sentences, as php raises them: its own about the directive,` |
|       - |  1334 | `			 * and then the caller's about an open that found no wrapper -- which` |
|       - |  1335 | `			 * is what this open's REASON has to be, since php's lookup is where` |
|       - |  1336 | `			 * the switch lives and a lookup that declines has nothing else to` |
|       - |  1337 | `			 * say. This engine used to raise only the first, leaving the` |
|       - |  1338 | `			 * caller to print whatever reason was armed ("operation failed"). */` |
|       - |  1339 | `			char zMsg[160];` |
|      14 |  1340 | `			pVm->pOpenCtx = 0;` |
|      14 |  1341 | `			pVm->zOpenMode[0] = 0;` |
|      14 |  1342 | `			if( pVm->nOpenDepth < 1 ){` |
|      14 |  1343 | `				pVm->zOpenErr = "no suitable wrapper could be found";` |
|       6 |  1344 | `			}` |
|      20 |  1345 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  1346 | `				"%s:// wrapper is disabled in the server configuration by allow_url_fopen=0",` |
|      12 |  1347 | `				pStream->zName);` |
|      14 |  1348 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|      14 |  1349 | `			return 0;` |
|       - |  1350 | `		}` |
|     361 |  1351 | `		if( bPushInclude && !PH7_VmIniGetBool(pVm,"allow_url_include",0) ){` |
|       - |  1352 | `			char zMsg[160];` |
|       7 |  1353 | `			pVm->pOpenCtx = 0;` |
|       7 |  1354 | `			pVm->zOpenMode[0] = 0;` |
|       - |  1355 | `			/* Same shape as the allow_url_fopen arm above: php's directive check` |
|       - |  1356 | `			 * lives in the WRAPPER LOOKUP, so the caller's own sentence has to` |
|       - |  1357 | `			 * report an open that found no wrapper. Without this the include` |
|       - |  1358 | ``			 * printed the armed default, `operation failed`. */`` |
|       7 |  1359 | `			if( pVm->nOpenDepth < 1 ){` |
|       7 |  1360 | `				pVm->zOpenErr = "no suitable wrapper could be found";` |
|       3 |  1361 | `			}` |
|      10 |  1362 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  1363 | `				"%s:// wrapper is disabled in the server configuration by allow_url_include=0",` |
|       6 |  1364 | `				pStream->zName);` |
|       7 |  1365 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|       7 |  1366 | `			return 0;` |
|       - |  1367 | `		}` |
|     175 |  1368 | `	}` |
|   45936 |  1369 | `	if( pResource == 0 ){` |
|       - |  1370 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|       - |  1371 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|       - |  1372 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|       - |  1373 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|       - |  1374 | `		 * reads it during the call, and file:// ignores it. */` |
|   45051 |  1375 | `		PH7_MemObjInit(pVm,&sDummy);` |
|   45051 |  1376 | `		pResource = &sDummy;` |
|   22458 |  1377 | `	}` |
|   45936 |  1378 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|       - |  1379 | `	/* Everything from here to the matching decrement is INSIDE an open, so a` |
|       - |  1380 | `	 * wrapper that opens something of its own does not get to rename the` |
|       - |  1381 | `	 * failure its caller will report. */` |
|   45936 |  1382 | `	pVm->nOpenDepth++;` |
|   45936 |  1383 | `	if( use_include ){` |
|    5774 |  1384 | `		if(	/* include_path names DIRECTORIES, so it has nothing to say about a` |
|       - |  1385 | `` 			 * URL: walking it for a `php://filter/…` one built `<dir>/filter/…` `` |
|       - |  1386 | `			 * and reported the whole open as an IO error. The direct arm is the` |
|       - |  1387 | `			 * one that also marks the file as included, which is what` |
|       - |  1388 | `			 * include_once needs. */` |
|   11548 |  1389 | `			pStream != pVm->pDefStream \|\|` |
|   11514 |  1390 | `			sFile.zString[0] == '/' \|\|` |
|       - |  1391 | `#ifdef __WINNT__` |
|       - |  1392 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|       - |  1393 | `#endif` |
|   11281 |  1394 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   11274 |  1395 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|       - |  1396 | `				/*  Open the file directly */` |
|     279 |  1397 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     279 |  1398 | `				if( rc == PH7_OK && bPushInclude ){` |
|       - |  1399 | `					/* Mark as included -- under the name the SCRIPT wrote, scheme` |
|       - |  1400 | `					 * included. The lookup handed the wrapper a stripped path, and` |
|       - |  1401 | ``					 * pushing THAT made `__FILE__` inside an included phar entry`` |
|       - |  1402 | ``					 * `x.phar/src/f.php` rather than `phar://x.phar/src/f.php` --`` |
|       - |  1403 | ``					 * so the `__DIR__ . '/../vendor/autoload.php'` every real stub`` |
|       - |  1404 | `					 * writes resolved to a path outside the archive. */` |
|     277 |  1405 | `					const char *zPush = sFile.zString;` |
|     277 |  1406 | `					sxu32 nPush = sFile.nByte;` |
|     277 |  1407 | `					if( zPush == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){` |
|      38 |  1408 | `						zPush = pVm->zOpenUri;` |
|      38 |  1409 | `						nPush = (sxu32)pVm->nOpenUri;` |
|      17 |  1410 | `					}` |
|     277 |  1411 | `					PH7_VmPushFilePath(pVm,zPush,nPush,FALSE,pNew);` |
|     136 |  1412 | `				}` |
|     142 |  1413 | `		}else{` |
|       - |  1414 | `			SyString *pPath;` |
|       - |  1415 | `			SyBlob sWorker;` |
|       - |  1416 | `#ifdef __WINNT__` |
|       - |  1417 | `			static const int c = '\\';` |
|       - |  1418 | `#else` |
|       - |  1419 | `			static const int c = '/';` |
|       - |  1420 | `#endif` |
|       - |  1421 | `			/* Init the path builder working buffer */` |
|   11279 |  1422 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       - |  1423 | `			/* Build a path from the set of include path */` |
|   11279 |  1424 | `			SySetResetCursor(&pVm->aPaths);` |
|   11279 |  1425 | `			rc = SXERR_IO;` |
|   11345 |  1426 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|       - |  1427 | `				/* Build full path */` |
|   11287 |  1428 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|       - |  1429 | `				/* Append null terminator */` |
|   11287 |  1430 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|     ! 0 |  1431 | `					continue;` |
|       - |  1432 | `				}` |
|       - |  1433 | `				/* Try to open the file */` |
|   11287 |  1434 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   11287 |  1435 | `				if( rc == PH7_OK ){` |
|   11220 |  1436 | `					if( bPushInclude ){` |
|       - |  1437 | `						/* Mark as included */` |
|   11220 |  1438 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|    5608 |  1439 | `					}` |
|   11220 |  1440 | `					break;` |
|       - |  1441 | `				}` |
|       - |  1442 | `				/* Reset the working buffer */` |
|      70 |  1443 | `				SyBlobReset(&sWorker);` |
|       - |  1444 | `				/* Check the next path */` |
|       4 |  1445 | `			}` |
|   11279 |  1446 | `			if( rc != PH7_OK ){` |
|       - |  1447 | `				/* php's LAST RESORT, and the one PHL never had: the directory of` |
|       - |  1448 | ``				 * the file that is EXECUTING. `include 'lib.php'` next to the`` |
|       - |  1449 | `				 * script has to work whatever directory the script was started` |
|       - |  1450 | `				 * from -- see PH7_VmExecutingDir(). Tried after the include_path` |
|       - |  1451 | `				 * entries, as php tries it. */` |
|       - |  1452 | `				SyString sDir;` |
|      62 |  1453 | `				if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      62 |  1454 | `					SyBlobReset(&sWorker);` |
|      62 |  1455 | `					SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      62 |  1456 | `					if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      62 |  1457 | `						rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|      62 |  1458 | `						if( rc == PH7_OK && bPushInclude ){` |
|      35 |  1459 | `							PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),` |
|      22 |  1460 | `								SyBlobLength(&sWorker),FALSE,pNew);` |
|      11 |  1461 | `						}` |
|      29 |  1462 | `					}` |
|      29 |  1463 | `				}` |
|      29 |  1464 | `			}` |
|   11279 |  1465 | `			SyBlobRelease(&sWorker);` |
|       - |  1466 | `		}` |
|    5779 |  1467 | `	}else{` |
|       - |  1468 | `		/* Open the URI direcly */` |
|   34388 |  1469 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|       - |  1470 | `	}` |
|   45936 |  1471 | `	pVm->nOpenDepth--;` |
|       - |  1472 | `	/* The armed context describes exactly ONE open — every attempt of the` |
|       - |  1473 | `	 * include-path walk above included — so it is dropped here whether the open` |
|       - |  1474 | `	 * worked or not. A device that wanted it (a userland wrapper) read it while` |
|       - |  1475 | `	 * its xOpen was running. */` |
|   45936 |  1476 | `	pVm->pOpenCtx = 0;` |
|   45936 |  1477 | `	pVm->zOpenMode[0] = 0;` |
|       - |  1478 | ``	/* An http:// exchange publishes `$http_response_header` into the frame that`` |
|       - |  1479 | `	 * called the opener, and it does so whether the open SUCCEEDED or not: a 404` |
|       - |  1480 | `	 * is a failed open with a complete set of headers behind it. Only the` |
|       - |  1481 | `	 * OUTERMOST open publishes -- a php://filter that opened its own resource is` |
|       - |  1482 | `	 * not what the script asked about. */` |
|   45936 |  1483 | `	if( pVm->nOpenDepth < 1 ){` |
|   45834 |  1484 | `		PH7_HttpFlushResponseHeaders(pVm);` |
|   22848 |  1485 | `	}` |
|   45936 |  1486 | `	if( rc != PH7_OK ){` |
|       - |  1487 | `		/* IO error */` |
|     405 |  1488 | `		return 0;` |
|       - |  1489 | `	}` |
|       - |  1490 | `	/* Nothing failed, so nothing is owed a reason: a later warning must not` |
|       - |  1491 | `	 * find this one still armed. An INNER open succeeding says nothing about` |
|       - |  1492 | `	 * the outer one, which may still be on its way to failing. */` |
|   45536 |  1493 | `	if( pVm->nOpenDepth < 1 ){` |
|   45442 |  1494 | `		pVm->zOpenErr = 0;` |
|   22668 |  1495 | `	}` |
|       - |  1496 | `	/* Return the file handle */` |
|   45536 |  1497 | `	return pHandle;` |
|   22916 |  1498 | `}` |
|       - |  1499 | `/* See ph7int.h: the wrapper's own reason for the open in flight. */` |
|     136 |  1500 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason)` |
|       5 |  1501 | `{` |
|       - |  1502 | `	/* Only the OUTERMOST wrapper's own body may name the failure: an inner` |
|       - |  1503 | `	 * open's wrapper is describing something the caller never asked for. */` |
|     141 |  1504 | `	if( pVm->nOpenDepth == 1 ){` |
|     141 |  1505 | `		pVm->zOpenErr = zReason;` |
|      67 |  1506 | `	}` |
|     141 |  1507 | `}` |
|       - |  1508 | `/* See ph7int.h. */` |
|       - |  1509 | `/*` |
|       - |  1510 | ` * Remember the mode STRING an open was asked with. php hands a userland wrapper's` |
|       - |  1511 | ` * stream_open() the caller's own spelling, and only fopen() and SplFileObject have` |
|       - |  1512 | ` * one -- every other opener is C code with a fixed mode, which UwrapOpenSlot spells` |
|       - |  1513 | ` * back from the flag bits. Cleared after each open, exactly like pOpenCtx.` |
|       - |  1514 | ` */` |
|    1988 |  1515 | `PH7_PRIVATE void PH7_StreamArmOpenMode(ph7_vm *pVm,const char *zMode,int nMode)` |
|       5 |  1516 | `{` |
|    1993 |  1517 | `	int n = nMode;` |
|    1993 |  1518 | `	if( zMode == 0 \|\| n < 1 ){` |
|     ! 0 |  1519 | `		pVm->zOpenMode[0] = 0;` |
|     ! 0 |  1520 | `		return;` |
|       - |  1521 | `	}` |
|    1993 |  1522 | `	if( n > (int)sizeof(pVm->zOpenMode) - 1 ){` |
|     ! 0 |  1523 | `		n = (int)sizeof(pVm->zOpenMode) - 1;` |
|     ! 0 |  1524 | `	}` |
|    1993 |  1525 | `	SyMemcpy(zMode,pVm->zOpenMode,(sxu32)n);` |
|    1993 |  1526 | `	pVm->zOpenMode[n] = 0;` |
|     969 |  1527 | `}` |
|      44 |  1528 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod)` |
|       2 |  1529 | `{` |
|      46 |  1530 | `	if( pVm->nOpenDepth != 1 ){` |
|     ! 0 |  1531 | `		return;` |
|       - |  1532 | `	}` |
|      68 |  1533 | `	SyBufferFormat(pVm->zOpenErrBuf,sizeof(pVm->zOpenErrBuf),"\"%s::%s\" call failed",` |
|      22 |  1534 | `		zClass ? zClass : "",zMethod);` |
|      46 |  1535 | `	pVm->zOpenErr = pVm->zOpenErrBuf;` |
|      24 |  1536 | `}` |
|       - |  1537 | `/*` |
|       - |  1538 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|       - |  1539 | ` * Store the read data in the given BLOB (last argument).` |
|       - |  1540 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|       - |  1541 | ` */` |
|   11938 |  1542 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|       5 |  1543 | `{` |
|       - |  1544 | `	ph7_int64 nRead;` |
|       - |  1545 | `	char zBuf[8192]; /* 8K */` |
|       - |  1546 | `	int rc;` |
|       - |  1547 | `	/* Perform the requested operation */` |
|   11942 |  1548 | `	for(;;){` |
|   23929 |  1549 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|   23929 |  1550 | `		if( nRead < 1 ){` |
|       - |  1551 | `			/* EOF or IO error */` |
|   11943 |  1552 | `			break;` |
|       - |  1553 | `		}` |
|       - |  1554 | `		/* Append contents */` |
|   11991 |  1555 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   11991 |  1556 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  1557 | `			break;` |
|       - |  1558 | `		}` |
|       5 |  1559 | `	}` |
|       - |  1560 | `	/* An EMPTY file is read, not failed: php's include of a 0-byte file is a` |
|       - |  1561 | `	 * silent no-op where this answered -1 and the include warned "IO error while` |
|       - |  1562 | `	 * importing". The device's own failure still is one -- it answers a NEGATIVE` |
|       - |  1563 | `	 * count, where end-of-file is 0. */` |
|   11943 |  1564 | `	return (SyBlobLength(pOut) > 0 \|\| nRead == 0) ? SXRET_OK : -1;` |
|       5 |  1565 | `}` |
|       - |  1566 | `/*` |
|       - |  1567 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|       - |  1568 | ` */` |
|   47330 |  1569 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|       5 |  1570 | `{` |
|   47335 |  1571 | `	if( pStream->xClose ){` |
|   47335 |  1572 | `		pStream->xClose(pHandle);` |
|   23615 |  1573 | `	}` |
|   47335 |  1574 | `}` |
|       - |  1575 | `/*` |
|       - |  1576 | ` * string fgetc(resource $handle)` |
|       - |  1577 | ` *  Gets a character from the given file pointer.` |
|       - |  1578 | ` * Parameters` |
|       - |  1579 | ` *  $handle` |
|       - |  1580 | ` *   The file pointer.` |
|       - |  1581 | ` * Return` |
|       - |  1582 | ` *  Returns a string containing a single character read from the file` |
|       - |  1583 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|       - |  1584 | ` * WARNING` |
|       - |  1585 | ` *  This operation is extremely slow.Avoid using it.` |
|       - |  1586 | ` */` |
|      14 |  1587 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  1588 | `{` |
|       - |  1589 | `	io_private *pDev;` |
|       - |  1590 | `	int c,n;` |
|       - |  1591 | `	int rc;` |
|      17 |  1592 | `	if( nArg < 1 ){` |
|       - |  1593 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  1594 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  1595 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1596 | `		return PH7_OK;` |
|       - |  1597 | `	}` |
|       - |  1598 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      17 |  1599 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      17 |  1600 | `	if( pDev == 0 ){` |
|       5 |  1601 | `		return rc;` |
|       - |  1602 | `	}` |
|      12 |  1603 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1604 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 |  1605 | `		ph7_result_bool(pCtx,0);` |
|       3 |  1606 | `		return PH7_OK;` |
|       - |  1607 | `	}` |
|       - |  1608 | `	/* Perform the requested operation */` |
|      10 |  1609 | `	n = (int)PH7_StreamRead(pDev,(void *)&c,sizeof(char));` |
|       - |  1610 | `	/* IO result */` |
|      10 |  1611 | `	if( n < 1 ){` |
|       - |  1612 | `		/* EOF or error,return FALSE */` |
|       3 |  1613 | `		StreamReportReadFailure(pCtx,pDev);` |
|       3 |  1614 | `		ph7_result_bool(pCtx,0);` |
|       2 |  1615 | `	}else{` |
|       - |  1616 | `		/* Return the string holding the character */` |
|       8 |  1617 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|       - |  1618 | `	}` |
|      10 |  1619 | `	return PH7_OK;` |
|      10 |  1620 | `}` |
|       - |  1621 | `/*` |
|       - |  1622 | ` * array\|int\|false\|null fscanf(resource $stream, string $format, mixed &...$vars)` |
|       - |  1623 | ` *  Parse the NEXT LINE of $stream according to $format.` |
|       - |  1624 | ` *` |
|       - |  1625 | ` *  php reads one whole line -- the newline included, which is what makes a` |
|       - |  1626 | `` *  trailing `%s` stop where it does -- and hands it to the same scanner`` |
|       - |  1627 | ` *  sscanf() runs, so every rule of that family (the two-pass format read, the` |
|       - |  1628 | ` *  -1 / NULL "nothing converted" answer) is this function's too. The one` |
|       - |  1629 | ` *  answer of its own is FALSE, and it means the STREAM was at its end: a line` |
|       - |  1630 | ` *  that scans to nothing is still NULL or -1, exactly as sscanf's would be.` |
|       - |  1631 | ` */` |
|      48 |  1632 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  1633 | `{` |
|       - |  1634 | `	const char *zLine,*zFmt;` |
|       - |  1635 | `	io_private *pDev;` |
|       - |  1636 | `	ph7_int64 n;` |
|      51 |  1637 | `	int nFmt = 0;` |
|      51 |  1638 | `	if( nArg < 2 ){` |
|     ! 0 |  1639 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  1640 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1641 | `		return PH7_OK;` |
|       - |  1642 | `	}` |
|      51 |  1643 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - |  1644 | `		char zGiven[64];` |
|      19 |  1645 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  1646 | `			"%s(): Argument #1 ($stream) must be of type resource, %s given",` |
|       6 |  1647 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - |  1648 | `	}` |
|       - |  1649 | `	/* ...but a resource whose DEVICE is gone is the one refusal in this family` |
|       - |  1650 | `	 * php words its own way: it names neither the argument nor its position,` |
|       - |  1651 | `	 * and calls the thing a File-Handle. */` |
|      39 |  1652 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      39 |  1653 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       4 |  1654 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  1655 | `			"%s(): supplied resource is not a valid File-Handle resource",` |
|       1 |  1656 | `			ph7_function_name(pCtx));` |
|       - |  1657 | `	}` |
|      36 |  1658 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1659 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 |  1660 | `		ph7_result_bool(pCtx,0);` |
|       3 |  1661 | `		return PH7_OK;` |
|       - |  1662 | `	}` |
|      33 |  1663 | `	n = StreamReadLine(pDev,&zLine,-1);` |
|      33 |  1664 | `	if( n < 1 ){` |
|       - |  1665 | `		/* Nothing left in the stream at all. */` |
|       5 |  1666 | `		ph7_result_bool(pCtx,0);` |
|       5 |  1667 | `		return PH7_OK;` |
|       - |  1668 | `	}` |
|      29 |  1669 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|      29 |  1670 | `	return (int)PH7_ScanfRun(pCtx,zLine,(int)n,zFmt,nFmt,&apArg[2],nArg - 2);` |
|      27 |  1671 | `}` |
|       - |  1672 | `/*` |
|       - |  1673 | ` * string fgets(resource $handle[,int64 $length ])` |
|       - |  1674 | ` *  Gets line from file pointer.` |
|       - |  1675 | ` * Parameters` |
|       - |  1676 | ` *  $handle` |
|       - |  1677 | ` *   The file pointer.` |
|       - |  1678 | ` * $length` |
|       - |  1679 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - |  1680 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - |  1681 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - |  1682 | ` *  the end of the line.` |
|       - |  1683 | ` * Return` |
|       - |  1684 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|       - |  1685 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|       - |  1686 | ` *  If an error occurs, FALSE is returned.` |
|       - |  1687 | ` */` |
|   53795 |  1688 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  1689 | `{` |
|       - |  1690 | `	const char *zLine;` |
|       - |  1691 | `	io_private *pDev;` |
|       - |  1692 | `	ph7_int64 n,nLen;` |
|       - |  1693 | `	int rc;` |
|   53800 |  1694 | `	if( nArg < 1 ){` |
|       - |  1695 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  1696 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  1697 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1698 | `		return PH7_OK;` |
|       - |  1699 | `	}` |
|       - |  1700 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|   53800 |  1701 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   53800 |  1702 | `	if( pDev == 0 ){` |
|       5 |  1703 | `		return rc;` |
|       - |  1704 | `	}` |
|   53796 |  1705 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1706 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 |  1707 | `		ph7_result_bool(pCtx,0);` |
|       3 |  1708 | `		return PH7_OK;` |
|       - |  1709 | `	}` |
|   53794 |  1710 | `	nLen = -1;` |
|   53794 |  1711 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - |  1712 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|       - |  1713 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|      66 |  1714 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      66 |  1715 | `		if( nLen < 1 ){` |
|       7 |  1716 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  1717 | `				"%s(): Argument #2 ($length) must be greater than 0",` |
|       2 |  1718 | `				ph7_function_name(pCtx));` |
|       - |  1719 | `		}` |
|       - |  1720 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|       - |  1721 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|       - |  1722 | `		 * false at any position, exactly like EOF. */` |
|      62 |  1723 | `		nLen -= 1;` |
|      62 |  1724 | `		if( nLen == 0 ){` |
|       6 |  1725 | `			ph7_result_bool(pCtx,0);` |
|       6 |  1726 | `			return PH7_OK;` |
|       - |  1727 | `		}` |
|      28 |  1728 | `	}` |
|       - |  1729 | `	/* Perform the requested operation */` |
|   53786 |  1730 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   53786 |  1731 | `	if( n < 1 ){` |
|       - |  1732 | `		/* EOF or IO error,return FALSE */` |
|    6764 |  1733 | `		StreamReportReadFailure(pCtx,pDev);` |
|    6764 |  1734 | `		ph7_result_bool(pCtx,0);` |
|    3379 |  1735 | `	}else{` |
|       - |  1736 | `		/* Return the freshly extracted line */` |
|   47027 |  1737 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|       - |  1738 | `	}` |
|   53786 |  1739 | `	return PH7_OK;` |
|   26587 |  1740 | `}` |
|       - |  1741 | `/*` |
|       - |  1742 | ` * string\|false stream_get_line(resource $stream, int $length, string $ending = "")` |
|       - |  1743 | ` *  Read a line from a stream, up to $length bytes or the FIRST occurrence of` |
|       - |  1744 | ` *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but` |
|       - |  1745 | ` *  never returned, and it may be any string.` |
|       - |  1746 | ` *  php's window rule (php_stream_get_record), pinned by probe: the ending` |
|       - |  1747 | ` *  counts only when it fits ENTIRELY inside the first $length bytes —` |
|       - |  1748 | ` *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,` |
|       - |  1749 | ` *  because the ending straddles its edge — and a capped read consumes no` |
|       - |  1750 | ` *  ending that starts at the boundary. $length 0 means php's 8192 default; at` |
|       - |  1751 | ` *  EOF the remainder is returned as-is, and false only when nothing is left.` |
|       - |  1752 | ` */` |
|      66 |  1753 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  1754 | `{` |
|       - |  1755 | `	char zGiven[64];` |
|      69 |  1756 | `	const char *zEnding = "";` |
|       - |  1757 | `	io_private *pDev;` |
|       - |  1758 | `	ph7_int64 nMaxLen;` |
|      69 |  1759 | `	int nEndLen = 0;` |
|      69 |  1760 | `	sxu32 iScanFrom = 0;` |
|      69 |  1761 | `	int bEof = 0;` |
|      69 |  1762 | `	if( nArg < 2 ){` |
|       - |  1763 | `		/* The central arity screen reports this; keep a refusal for a direct call. */` |
|     ! 0 |  1764 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1765 | `		return PH7_OK;` |
|       - |  1766 | `	}` |
|      69 |  1767 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       4 |  1768 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  1769 | `			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",` |
|       1 |  1770 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - |  1771 | `	}` |
|      67 |  1772 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      67 |  1773 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - |  1774 | `		/* A closed or foreign resource is php's own TypeError, not a warning. */` |
|       3 |  1775 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  1776 | `			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");` |
|       - |  1777 | `	}` |
|      65 |  1778 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1779 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 |  1780 | `		ph7_result_bool(pCtx,0);` |
|       3 |  1781 | `		return PH7_OK;` |
|       - |  1782 | `	}` |
|      63 |  1783 | `	nMaxLen = ph7_value_to_int64(apArg[1]);` |
|      63 |  1784 | `	if( nMaxLen < 0 ){` |
|       3 |  1785 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  1786 | `			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");` |
|       - |  1787 | `	}` |
|      61 |  1788 | `	if( nMaxLen == 0 ){` |
|       - |  1789 | `		/* php's documented default window */` |
|       3 |  1790 | `		nMaxLen = 8192;` |
|       1 |  1791 | `	}` |
|      61 |  1792 | `	if( nArg > 2 ){` |
|      57 |  1793 | `		zEnding = ph7_value_to_string(apArg[2],&nEndLen);` |
|      27 |  1794 | `	}` |
|      61 |  1795 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|       - |  1796 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      33 |  1797 | `		SyBlobReset(&pDev->sBuffer);` |
|      33 |  1798 | `		pDev->nOfft = 0;` |
|      15 |  1799 | `	}` |
|       - |  1800 | `	/* Fill-and-scan: buffer chunks until the ending fits inside the window,` |
|       - |  1801 | `	 * the window itself fills, or the stream dries up. */` |
|      65 |  1802 | `	for(;;){` |
|     105 |  1803 | `		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     105 |  1804 | `		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     105 |  1805 | `		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;` |
|       - |  1806 | `		ph7_int64 n;` |
|       - |  1807 | `		char zBuf[8192];` |
|     105 |  1808 | `		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){` |
|       - |  1809 | `			/* The ending must END inside the window to count. Resume the scan` |
|       - |  1810 | `			 * where the previous fill left off — a candidate can straddle two` |
|       - |  1811 | `			 * fills, so back up by the ending's length less one. */` |
|       - |  1812 | `			sxu32 i;` |
|   40183 |  1813 | `			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){` |
|   40151 |  1814 | `				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){` |
|      29 |  1815 | `					pDev->nOfft += i + (sxu32)nEndLen;` |
|      29 |  1816 | `					ph7_result_string(pCtx,zData,(int)i);` |
|      45 |  1817 | `					return PH7_OK;` |
|       - |  1818 | `				}` |
|   20064 |  1819 | `			}` |
|      34 |  1820 | `			iScanFrom = i;` |
|      16 |  1821 | `		}` |
|      79 |  1822 | `		if( (ph7_int64)nAvail >= nMaxLen ){` |
|       - |  1823 | `			/* Window full with no ending inside it: hand the window back raw,` |
|       - |  1824 | `			 * anything past it (an ending included) stays buffered. */` |
|      18 |  1825 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|      18 |  1826 | `			ph7_result_string(pCtx,zData,(int)nMaxLen);` |
|      18 |  1827 | `			return PH7_OK;` |
|       - |  1828 | `		}` |
|      63 |  1829 | `		if( bEof ){` |
|       - |  1830 | `			/* EOF: the remainder as-is, false when nothing is left. */` |
|      18 |  1831 | `			if( nAvail > 0 ){` |
|      12 |  1832 | `				pDev->nOfft += nAvail;` |
|      12 |  1833 | `				ph7_result_string(pCtx,zData,(int)nAvail);` |
|       7 |  1834 | `			}else{` |
|       8 |  1835 | `				ph7_result_bool(pCtx,0);` |
|       - |  1836 | `			}` |
|      18 |  1837 | `			return PH7_OK;` |
|       - |  1838 | `		}` |
|      47 |  1839 | `		n = IoPrivateDeviceRead(pDev,zBuf,(ph7_int64)sizeof(zBuf));` |
|      47 |  1840 | `		if( n < 1 ){` |
|      18 |  1841 | `			bEof = 1;` |
|      18 |  1842 | `			if( n == 0 ){` |
|      18 |  1843 | `				pDev->bEof = 1;` |
|       8 |  1844 | `			}` |
|      18 |  1845 | `			continue;` |
|       - |  1846 | `		}` |
|      31 |  1847 | `		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){` |
|     ! 0 |  1848 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  1849 | `		}` |
|       3 |  1850 | `	}` |
|      36 |  1851 | `}` |
|       - |  1852 | `/*` |
|       - |  1853 | ` * string fread(resource $handle,int64 $length)` |
|       - |  1854 | ` *  Binary-safe file read.` |
|       - |  1855 | ` * Parameters` |
|       - |  1856 | ` *  $handle` |
|       - |  1857 | ` *   The file pointer.` |
|       - |  1858 | ` * $length` |
|       - |  1859 | ` *  Up to length number of bytes read.` |
|       - |  1860 | ` * Return` |
|       - |  1861 | ` *  The data readen on success or FALSE on failure.` |
|       - |  1862 | ` */` |
|     481 |  1863 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  1864 | `{` |
|       - |  1865 | `	io_private *pDev;` |
|       - |  1866 | `	ph7_int64 nRead;` |
|       - |  1867 | `	void *pBuf;` |
|       - |  1868 | `	int nLen;` |
|       - |  1869 | `	int rc;` |
|     486 |  1870 | `	if( nArg < 1 ){` |
|       - |  1871 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  1872 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  1873 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1874 | `		return PH7_OK;` |
|       - |  1875 | `	}` |
|       - |  1876 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     486 |  1877 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     486 |  1878 | `	if( pDev == 0 ){` |
|      17 |  1879 | `		return rc;` |
|       - |  1880 | `	}` |
|     470 |  1881 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1882 | `		/* No reader: php answers FALSE and says nothing. */` |
|       3 |  1883 | `		ph7_result_bool(pCtx,0);` |
|       3 |  1884 | `		return PH7_OK;` |
|       - |  1885 | `	}` |
|     468 |  1886 | `        nLen = 4096;` |
|     468 |  1887 | `	if( nArg > 1 ){` |
|       - |  1888 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|       - |  1889 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|       - |  1890 | `	   * the central type screen (the recorded null-policy divergence). */` |
|     468 |  1891 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|     468 |  1892 | `	  if( nWant < 1 ){` |
|       7 |  1893 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  1894 | `			"%s(): Argument #2 ($length) must be greater than 0",` |
|       2 |  1895 | `			ph7_function_name(pCtx));` |
|       - |  1896 | `	  }` |
|     464 |  1897 | `	  nLen = (int)nWant;` |
|     464 |  1898 | `	  if( nLen < 1 ){` |
|       - |  1899 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|       - |  1900 | `		 * (StreamRead returns only what the stream holds) instead of` |
|       - |  1901 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|       - |  1902 | `		 * that do not fit an int. */` |
|     ! 0 |  1903 | `		nLen = 4096;` |
|     ! 0 |  1904 | `	  }` |
|     220 |  1905 | `        }` |
|       - |  1906 | `	/* Allocate enough buffer */` |
|     464 |  1907 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     464 |  1908 | `	if( pBuf == 0 ){` |
|     ! 0 |  1909 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  1910 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1911 | `		return PH7_OK;` |
|       - |  1912 | `	}` |
|       - |  1913 | `	/* Perform the requested operation */` |
|     464 |  1914 | `	errno = 0;` |
|     464 |  1915 | `	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     464 |  1916 | `	if( nRead < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK) ){` |
|       - |  1917 | `		/* Nothing had ARRIVED yet, which is not a failure: php answers "" for a` |
|       - |  1918 | ``		 * read that could not proceed and reserves `false` for one that broke.`` |
|       - |  1919 | `		 * The question is answered by errno rather than by a per-handle flag —` |
|       - |  1920 | `		 * two handles can share one descriptor (every php://stdin is fd 0), so` |
|       - |  1921 | `		 * a flag on the handle that set the mode answers wrongly for its` |
|       - |  1922 | `		 * siblings, and a genuine EBADF on a non-blocking write-only handle` |
|       - |  1923 | `		 * would come back as "" rather than false. When a TIMEOUT is what` |
|       - |  1924 | ``		 * expired, php reports false and sets the metadata's `timed_out`. */`` |
|      71 |  1925 | `		if( pDev->bHasTimeout && !pDev->bNonBlock ){` |
|       - |  1926 | `			/* A handle in NON-BLOCKING mode is the other case: it answers "" for` |
|       - |  1927 | `			 * a read that found nothing whether or not a timeout is armed, and` |
|       - |  1928 | ``			 * every socket now carries `default_socket_timeout`. */`` |
|       3 |  1929 | `			pDev->bTimedOut = 1;` |
|       3 |  1930 | `			ph7_result_bool(pCtx,0);` |
|       2 |  1931 | `		}else{` |
|      69 |  1932 | `			ph7_result_string(pCtx,"",0);` |
|       1 |  1933 | `		}` |
|     429 |  1934 | `	}else if( nRead < 0 ){` |
|       - |  1935 | `		/* A real IO error, which is php's other false here. */` |
|      33 |  1936 | `		StreamReportReadFailure(pCtx,pDev);` |
|      33 |  1937 | `		ph7_result_bool(pCtx,0);` |
|      19 |  1938 | `	}else{` |
|       - |  1939 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|       - |  1940 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|       - |  1941 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|       - |  1942 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|       - |  1943 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|     366 |  1944 | `		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);` |
|       - |  1945 | `	}` |
|       - |  1946 | `	/* Release the buffer */` |
|     464 |  1947 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     464 |  1948 | `	return PH7_OK;` |
|     236 |  1949 | `}` |
|       - |  1950 | `/*` |
|       - |  1951 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|       - |  1952 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|       - |  1953 | ` * Gets line from file pointer and parse for CSV fields.` |
|       - |  1954 | ` * Parameters` |
|       - |  1955 | ` * $handle` |
|       - |  1956 | ` *   The file pointer.` |
|       - |  1957 | ` * $length` |
|       - |  1958 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - |  1959 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - |  1960 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - |  1961 | ` *  the end of the line.` |
|       - |  1962 | ` * $delimiter` |
|       - |  1963 | ` *   Set the field delimiter (one character only).` |
|       - |  1964 | ` * $enclosure` |
|       - |  1965 | ` *   Set the field enclosure character (one character only).` |
|       - |  1966 | ` * $escape` |
|       - |  1967 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|       - |  1968 | ` * Return` |
|       - |  1969 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|       - |  1970 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|       - |  1971 | ` *  If an error occurs, FALSE is returned.` |
|       - |  1972 | ` */` |
|      70 |  1973 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  1974 | `{` |
|       - |  1975 | `	const char *zLine;` |
|       - |  1976 | `	io_private *pDev;` |
|       - |  1977 | `	ph7_int64 n,nLen;` |
|      72 |  1978 | `	int delim  = ',';   /* Delimiter */` |
|      72 |  1979 | `	int encl   = '"' ;  /* Enclosure */` |
|      72 |  1980 | `	int escape = '\\';  /* Escape character */` |
|       - |  1981 | ``	int rcArg;   /* the handle screen's; the CSV screens below shadow `rc` */`` |
|      72 |  1982 | `	if( nArg < 1 ){` |
|       - |  1983 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  1984 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  1985 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1986 | `		return PH7_OK;` |
|       - |  1987 | `	}` |
|       - |  1988 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      72 |  1989 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcArg);` |
|      72 |  1990 | `	if( pDev == 0 ){` |
|       3 |  1991 | `		return rcArg;` |
|       - |  1992 | `	}` |
|      69 |  1993 | `	if( !StreamHasReader(pDev) ){` |
|       - |  1994 | `		/* No reader: php answers FALSE and says nothing. */` |
|     ! 0 |  1995 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  1996 | `		return PH7_OK;` |
|       - |  1997 | `	}` |
|      69 |  1998 | `	if( nArg > 2 ){` |
|       - |  1999 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|       - |  2000 | `		 * ordering) and even when the stream is already at EOF. */` |
|      67 |  2001 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|      67 |  2002 | `		if( rc != PH7_OK ){` |
|       7 |  2003 | `			return rc;` |
|       - |  2004 | `		}` |
|      61 |  2005 | `		if( nArg > 3 ){` |
|      61 |  2006 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|      61 |  2007 | `			if( rc != PH7_OK ){` |
|       3 |  2008 | `				return rc;` |
|       - |  2009 | `			}` |
|      59 |  2010 | `			if( nArg > 4 ){` |
|      59 |  2011 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|      59 |  2012 | `				if( rc != PH7_OK ){` |
|       3 |  2013 | `					return rc;` |
|       - |  2014 | `				}` |
|      28 |  2015 | `			}` |
|      28 |  2016 | `		}` |
|      28 |  2017 | `	}` |
|      59 |  2018 | `	nLen = -1;` |
|      59 |  2019 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - |  2020 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|       - |  2021 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|       - |  2022 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|      49 |  2023 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      49 |  2024 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|       3 |  2025 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  2026 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|       - |  2027 | `		}` |
|       - |  2028 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|      23 |  2029 | `	}` |
|       - |  2030 | `	/* Perform the requested operation */` |
|      57 |  2031 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      57 |  2032 | `	if( n < 1 ){` |
|       - |  2033 | `		/* EOF or IO error,return FALSE */` |
|      13 |  2034 | `		StreamReportReadFailure(pCtx,pDev);` |
|      13 |  2035 | `		ph7_result_bool(pCtx,0);` |
|       7 |  2036 | `	}else{` |
|       - |  2037 | `		ph7_value *pArray;` |
|       - |  2038 | `		SyBlob sRec;` |
|       - |  2039 | `		PH7_CsvScan sScan;` |
|       - |  2040 | `		/* Create our array */` |
|      45 |  2041 | `		pArray = ph7_context_new_array(pCtx);` |
|      45 |  2042 | `		if( pArray == 0 ){` |
|     ! 0 |  2043 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  2044 | `			ph7_result_null(pCtx);` |
|     ! 0 |  2045 | `			return PH7_OK;` |
|       - |  2046 | `		}` |
|       - |  2047 | `		/* A RECORD is not a line: an enclosure that is still open when the line` |
|       - |  2048 | `		 * ends means the value contains the newline and the record continues on` |
|       - |  2049 | `		 * the next one. Parsing a single line and stopping split such a value` |
|       - |  2050 | `		 * across two rows, with the halves quoted wrong. The whole record is` |
|       - |  2051 | `		 * gathered FIRST and parsed once -- the scan below carries its position` |
|       - |  2052 | `		 * across the appends, so a stray quote costs one pass over the file` |
|       - |  2053 | `		 * rather than one per line. */` |
|      45 |  2054 | `		SyBlobInit(&sRec,&pCtx->pVm->sAllocator);` |
|      45 |  2055 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|      45 |  2056 | `		PH7_CsvScanInit(&sScan);` |
|      55 |  2057 | `		while( PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|      27 |  2058 | `				SyBlobLength(&sRec),delim,encl,escape) ){` |
|      13 |  2059 | `			if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|       - |  2060 | `				/* The parser measures in int; stop rather than wrap negative. */` |
|     ! 0 |  2061 | `				break;` |
|       - |  2062 | `			}` |
|       - |  2063 | `			/* Continuation reads are NOT capped by $length: php's limit applies` |
|       - |  2064 | `			 * to the first read of the record, and reusing it here ended the` |
|       - |  2065 | `			 * record on a chunk boundary in the middle of a quoted value. */` |
|      13 |  2066 | `			n = StreamReadLine(pDev,&zLine,0);` |
|      13 |  2067 | `			if( n < 1 ){` |
|       - |  2068 | `				/* EOF inside the enclosure: php answers what it has. */` |
|       3 |  2069 | `				break;` |
|       - |  2070 | `			}` |
|      11 |  2071 | `			SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|       1 |  2072 | `		}` |
|      67 |  2073 | `		PH7_ProcessCsv(pArray,(const char *)SyBlobData(&sRec),` |
|      44 |  2074 | `			(int)SyBlobLength(&sRec),delim,encl,escape,0);` |
|      45 |  2075 | `		SyBlobRelease(&sRec);` |
|       - |  2076 | `		/* Return the freshly created array  */` |
|      45 |  2077 | `		ph7_result_value(pCtx,pArray);` |
|       - |  2078 | `	}` |
|      57 |  2079 | `	return PH7_OK;` |
|      37 |  2080 | `}` |
|       - |  2081 | `/*` |
|       - |  2082 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|       - |  2083 | ` *  Gets line from file pointer and strip HTML tags.` |
|       - |  2084 | ` * Parameters` |
|       - |  2085 | ` * $handle` |
|       - |  2086 | ` *   The file pointer.` |
|       - |  2087 | ` * $length` |
|       - |  2088 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|       - |  2089 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|       - |  2090 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|       - |  2091 | ` *  the end of the line.` |
|       - |  2092 | ` * $allowable_tags` |
|       - |  2093 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|       - |  2094 | ` * Return` |
|       - |  2095 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|       - |  2096 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|       - |  2097 | ` */` |
|       2 |  2098 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  2099 | `{` |
|       - |  2100 | `	const char *zLine;` |
|       - |  2101 | `	io_private *pDev;` |
|       - |  2102 | `	ph7_int64 n,nLen;` |
|       3 |  2103 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|       - |  2104 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2105 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  2106 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2107 | `		return PH7_OK;` |
|       - |  2108 | `	}` |
|       - |  2109 | `	/* Extract our private data */` |
|       3 |  2110 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|       - |  2111 | `	/* Make sure we are dealing with a valid io_private instance */` |
|       3 |  2112 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - |  2113 | `		/*Expecting an IO handle */` |
|     ! 0 |  2114 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  2115 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2116 | `		return PH7_OK;` |
|       - |  2117 | `	}` |
|       3 |  2118 | `	if( !StreamHasReader(pDev) ){` |
|       - |  2119 | `		/* No reader: php answers FALSE and says nothing. */` |
|     ! 0 |  2120 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2121 | `		return PH7_OK;` |
|       - |  2122 | `	}` |
|       3 |  2123 | `	nLen = -1;` |
|       3 |  2124 | `	if( nArg > 1 ){` |
|       - |  2125 | `		/* Maximum data to read */` |
|     ! 0 |  2126 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     ! 0 |  2127 | `	}` |
|       - |  2128 | `	/* Perform the requested operation */` |
|       3 |  2129 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|       3 |  2130 | `	if( n < 1 ){` |
|       - |  2131 | `		/* EOF or IO error,return FALSE */` |
|     ! 0 |  2132 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2133 | `	}else{` |
|       3 |  2134 | `		const char *zTaglist = 0;` |
|       3 |  2135 | `		int nTaglen = 0;` |
|       3 |  2136 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|       - |  2137 | `			/* Allowed tag */` |
|     ! 0 |  2138 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|     ! 0 |  2139 | `		}` |
|       - |  2140 | `		/* Process data just read */` |
|       3 |  2141 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen,0);` |
|       - |  2142 | `	}` |
|       3 |  2143 | `	return PH7_OK;` |
|       2 |  2144 | `}` |
|       - |  2145 | `/*` |
|       - |  2146 | ` * string readdir(resource $dir_handle)` |
|       - |  2147 | ` *   Read entry from directory handle.` |
|       - |  2148 | ` * Parameter` |
|       - |  2149 | ` *  $dir_handle` |
|       - |  2150 | ` *   The directory handle resource previously opened with opendir().` |
|       - |  2151 | ` * Return` |
|       - |  2152 | ` *  Returns the filename on success or FALSE on failure.` |
|       - |  2153 | ` */` |
|   18831 |  2154 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  2155 | `{` |
|       - |  2156 | `	const ph7_io_stream *pStream;` |
|       - |  2157 | `	io_private *pDev;` |
|       - |  2158 | `	int rc;` |
|   18836 |  2159 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|   18836 |  2160 | `	if( pDev == 0 ){` |
|      19 |  2161 | `		return rc;` |
|       - |  2162 | `	}` |
|       - |  2163 | `	/* Point to the target IO stream device */` |
|   18818 |  2164 | `	pStream = pDev->pStream;` |
|   18818 |  2165 | `	if( pStream == 0 \|\| pStream->xReadDir == 0 ){` |
|       - |  2166 | `		/* No entries to give: php's readdir() answers FALSE in silence. */` |
|     ! 0 |  2167 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2168 | `		return PH7_OK;` |
|       - |  2169 | `	}` |
|   18818 |  2170 | `	ph7_result_bool(pCtx,0);` |
|       - |  2171 | `	/* Perform the requested operation */` |
|   18818 |  2172 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   18818 |  2173 | `	if( rc != PH7_OK ){` |
|       - |  2174 | `		/* Return FALSE */` |
|    1830 |  2175 | `		ph7_result_bool(pCtx,0);` |
|     915 |  2176 | `	}else{` |
|       - |  2177 | `		/* php's directory stream moves by one record per entry it PRODUCED --` |
|       - |  2178 | `		 * the read that finds the end moves nothing -- and that product is the` |
|       - |  2179 | `		 * only thing ftell() on a directory handle reports. */` |
|   16993 |  2180 | `		pDev->iPos += PHL_DIR_RECORD;` |
|       - |  2181 | `	}` |
|   18818 |  2182 | `	return PH7_OK;` |
|    9399 |  2183 | `}` |
|       - |  2184 | `/*` |
|       - |  2185 | ` * void rewinddir(resource $dir_handle)` |
|       - |  2186 | ` *   Rewind directory handle.` |
|       - |  2187 | ` * Parameter` |
|       - |  2188 | ` *  $dir_handle` |
|       - |  2189 | ` *   The directory handle resource previously opened with opendir().` |
|       - |  2190 | ` * Return` |
|       - |  2191 | ` *  FALSE on failure.` |
|       - |  2192 | ` */` |
|      12 |  2193 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  2194 | `{` |
|       - |  2195 | `	const ph7_io_stream *pStream;` |
|       - |  2196 | `	io_private *pDev;` |
|       - |  2197 | `	int rc;` |
|      14 |  2198 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|      14 |  2199 | `	if( pDev == 0 ){` |
|       3 |  2200 | `		return rc;` |
|       - |  2201 | `	}` |
|       - |  2202 | `	/* Point to the target IO stream device */` |
|      12 |  2203 | `	pStream = pDev->pStream;` |
|      12 |  2204 | `	if( pStream == 0 \|\| pStream->xRewindDir == 0 ){` |
|       - |  2205 | `		/* Nothing to rewind, and php says nothing about it. */` |
|     ! 0 |  2206 | `		return PH7_OK;` |
|       - |  2207 | `	}` |
|       - |  2208 | `	/* Perform the requested operation */` |
|      12 |  2209 | `	pStream->xRewindDir(pDev->pHandle);` |
|      12 |  2210 | `	return PH7_OK;` |
|       8 |  2211 | ` }` |
|       - |  2212 | `/* Forward declaration */` |
|       - |  2213 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|       - |  2214 | `/*` |
|       - |  2215 | ` * void closedir(resource $dir_handle)` |
|       - |  2216 | ` *   Close directory handle.` |
|       - |  2217 | ` * Parameter` |
|       - |  2218 | ` *  $dir_handle` |
|       - |  2219 | ` *   The directory handle resource previously opened with opendir().` |
|       - |  2220 | ` * Return` |
|       - |  2221 | ` *  FALSE on failure.` |
|       - |  2222 | ` */` |
|    1845 |  2223 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  2224 | `{` |
|       - |  2225 | `	const ph7_io_stream *pStream;` |
|       - |  2226 | `	io_private *pDev;` |
|       - |  2227 | `	int rc;` |
|    1850 |  2228 | `	pDev = StreamDirArg(pCtx,nArg,apArg,&rc);` |
|    1850 |  2229 | `	if( pDev == 0 ){` |
|       5 |  2230 | `		return rc;` |
|       - |  2231 | `	}` |
|       - |  2232 | `	/* Point to the target IO stream device */` |
|    1846 |  2233 | `	pStream = pDev->pStream;` |
|    1846 |  2234 | `	if( pStream == 0 \|\| pStream->xCloseDir == 0 ){` |
|       - |  2235 | `		/* Nothing to close, and php says nothing about it. */` |
|     ! 0 |  2236 | `		return PH7_OK;` |
|       - |  2237 | `	}` |
|       - |  2238 | `	/* Perform the requested operation */` |
|    1846 |  2239 | `	PH7_StreamFilterReleaseChains(pDev);` |
|    1846 |  2240 | `	pStream->xCloseDir(pDev->pHandle);` |
|       - |  2241 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|    1846 |  2242 | `	MarkIOPrivateClosed(pDev);` |
|    1846 |  2243 | `	if( pCtx->pVm->pLastDir == (void *)pDev ){` |
|       - |  2244 | `		/* php's fallback is the last opened stream, not the last LIVE one: once` |
|       - |  2245 | `		 * it is closed, readdir() with no argument is "No resource supplied". */` |
|    1844 |  2246 | `		pCtx->pVm->pLastDir = 0;` |
|     917 |  2247 | `	}` |
|    1846 |  2248 | `	return PH7_OK;` |
|     925 |  2249 | ` }` |
|       - |  2250 | `/*` |
|       - |  2251 | ` * php's scandir() body reports a failed open TWICE: the opener's own warning, and` |
|       - |  2252 | `` * then `scandir(): (errno 2): No such file or directory` from the errno it left`` |
|       - |  2253 | ` * behind -- whatever that errno is, so a wrapper's refusal reads a stale one. The` |
|       - |  2254 | ` * prelude scandir() drives opendir() for its open, so the line is raised here,` |
|       - |  2255 | ` * for the scandir frame only; opendir() and dir() say it once.` |
|       - |  2256 | ` */` |
|      34 |  2257 | `static void OpenDirScandirErrno(ph7_context *pCtx,int iErr)` |
|       4 |  2258 | `{` |
|       - |  2259 | `	char zFn[64];` |
|      38 |  2260 | `	const char *zName = PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn));` |
|      34 |  2261 | `	if( zName == 0 \|\| SyStrncmp(zName,"scandir",sizeof("scandir")-1) != 0` |
|      26 |  2262 | `	 \|\| zName[sizeof("scandir")-1] != 0 ){` |
|      28 |  2263 | `		return;` |
|       - |  2264 | `	}` |
|      12 |  2265 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"scandir(): (errno %d): %s",iErr,VfsStrerror(iErr));` |
|      21 |  2266 | `}` |
|       - |  2267 | `/*` |
|       - |  2268 | ` * resource opendir(string $path[,resource $context])` |
|       - |  2269 | ` *  Open directory handle.` |
|       - |  2270 | ` * Parameters` |
|       - |  2271 | ` * $path` |
|       - |  2272 | ` *   The directory path that is to be opened.` |
|       - |  2273 | ` * $context` |
|       - |  2274 | ` *   A context stream resource.` |
|       - |  2275 | ` * Return` |
|       - |  2276 | ` *  A directory handle resource on success,or FALSE on failure.` |
|       - |  2277 | ` */` |
|    1899 |  2278 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  2279 | `{` |
|       - |  2280 | `	const ph7_io_stream *pStream;` |
|       - |  2281 | `	const char *zPath,*zAsked;` |
|       - |  2282 | `	io_private *pDev;` |
|    1904 |  2283 | `	int iLen,rc,iErr = 0,bThrew = 0;` |
|    1904 |  2284 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - |  2285 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2286 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|     ! 0 |  2287 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2288 | `		return PH7_OK;` |
|       - |  2289 | `	}` |
|       - |  2290 | `	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it` |
|       - |  2291 | `	 * here — dir_opendir() over a userland wrapper is not dispatched (recorded` |
|       - |  2292 | `	 * slice-2 (e)) — but the refusal is the argument's contract. */` |
|    1904 |  2293 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    1904 |  2294 | `	if( bThrew ){` |
|       3 |  2295 | `		return PH7_OK;` |
|       - |  2296 | `	}` |
|       - |  2297 | `	/* Extract the target path */` |
|    1902 |  2298 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|    1902 |  2299 | `	if( iLen < 1 ){` |
|       - |  2300 | `		/* php answers FALSE for an empty directory name and says nothing about` |
|       - |  2301 | `		 * it -- the ValueError its file openers raise is not this door's. */` |
|       2 |  2302 | `		ph7_result_bool(pCtx,0);` |
|       2 |  2303 | `		return PH7_OK;` |
|       - |  2304 | `	}` |
|       - |  2305 | `	/* php names the path AS WRITTEN in every diagnostic below, scheme included,` |
|       - |  2306 | `	 * and the device lookup advances zPath past that scheme. */` |
|    1900 |  2307 | `	zAsked = zPath;` |
|       - |  2308 | `	/* Try to extract a stream */` |
|    1900 |  2309 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zPath,iLen);` |
|    1900 |  2310 | `	if( pStream == 0 ){` |
|     ! 0 |  2311 | `		VfsThrowNoDeviceWarning(pCtx,zPath,TRUE);` |
|     ! 0 |  2312 | `		OpenDirScandirErrno(pCtx,errno);` |
|     ! 0 |  2313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2314 | `		return PH7_OK;` |
|       - |  2315 | `	}` |
|    1900 |  2316 | `	if( pStream->xOpenDir == 0 ){` |
|       - |  2317 | `		/* php words a wrapper with no directory opener as an ordinary failed` |
|       - |  2318 | ``		 * open whose reason is `not implemented` -- the same sentence a missing`` |
|       - |  2319 | `		 * directory gets, so a caller's error handling does not have to know` |
|       - |  2320 | `		 * that this one is about the WRAPPER. */` |
|       - |  2321 | `		char zFn[64];` |
|      18 |  2322 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: not implemented",` |
|       5 |  2323 | `			PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn)),zAsked);` |
|      13 |  2324 | `		OpenDirScandirErrno(pCtx,errno);` |
|      13 |  2325 | `		ph7_result_bool(pCtx,0);` |
|      13 |  2326 | `		return PH7_OK;` |
|       - |  2327 | `	}` |
|       - |  2328 | `	/* Allocate a new IO private instance */` |
|    1890 |  2329 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    1890 |  2330 | `	if( pDev == 0 ){` |
|     ! 0 |  2331 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  2332 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2333 | `		return PH7_OK;` |
|       - |  2334 | `	}` |
|       - |  2335 | `	/* Initialize the structure */` |
|    1890 |  2336 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|       - |  2337 | `	/* Open the target directory. A device reaches the VM only through this` |
|       - |  2338 | `	 * argument -- its xOpenDir has no vm parameter -- and glob:// needs one to` |
|       - |  2339 | `	 * walk the filesystem and hold what it finds, so a caller that passed no` |
|       - |  2340 | `	 * context hands over a synthesized stack value carrying the VM, exactly as` |
|       - |  2341 | `	 * PH7_StreamOpenHandle() does for the byte-stream openers. */` |
|       - |  2342 | `	{` |
|       - |  2343 | `		ph7_value sDummy;` |
|    1890 |  2344 | `		ph7_value *pRes = nArg > 1 ? apArg[1] : 0;` |
|    1890 |  2345 | `		if( pRes == 0 ){` |
|     432 |  2346 | `			PH7_MemObjInit(pCtx->pVm,&sDummy);` |
|     432 |  2347 | `			pRes = &sDummy;` |
|     203 |  2348 | `		}` |
|    1890 |  2349 | `		rc = pStream->xOpenDir(zPath,pRes,&pDev->pHandle);` |
|    1890 |  2350 | `		iErr = errno;` |
|    1890 |  2351 | `		if( pRes == &sDummy ){` |
|     432 |  2352 | `			PH7_MemObjRelease(&sDummy);` |
|     203 |  2353 | `		}` |
|       - |  2354 | `	}` |
|    1890 |  2355 | `	if( rc != PH7_OK ){` |
|       - |  2356 | ``		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No`` |
|       - |  2357 | ``		 * such file or directory` — and PHL returned FALSE in silence. The message`` |
|       - |  2358 | `` 		 * names the ACTIVE function, which is how dir() gets php's `dir(...)` `` |
|       - |  2359 | `		 * wording out of the same call.` |
|       - |  2360 | `		 *` |
|       - |  2361 | `		 * A userland wrapper's refusal is not an errno: php names the METHOD it` |
|       - |  2362 | `		 * called and whether the wrapper has one at all, the same way a failed` |
|       - |  2363 | `		 * stream_open() is reported. */` |
|       - |  2364 | `		char zWhy[160];` |
|       - |  2365 | `		char zFn[64];` |
|      28 |  2366 | `		const char *zReason = PH7_StreamUserDirReason(pCtx->pVm,pStream,zWhy,(int)sizeof(zWhy))` |
|      24 |  2367 | `			? zWhy : VfsStrerror(iErr);` |
|       - |  2368 | `#ifdef __WINNT__` |
|       4 |  2369 | `		if( pStream == &sWinFileStream ){` |
|       - |  2370 | `			/* php's plain-files opener on Windows warns with the system's own` |
|       - |  2371 | `			 * reason first, and only then fails the way every platform does. */` |
|       - |  2372 | `			char zSys[256];` |
|       3 |  2373 | `			unsigned long nCode = PH7_WinOpenDirReason(zSys,(int)sizeof(zSys));` |
|       3 |  2374 | `			if( nCode ){` |
|       3 |  2375 | `				PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s (code: %lu)",` |
|       - |  2376 | `					PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn)),zAsked,zSys,nCode);` |
|       - |  2377 | `			}` |
|       - |  2378 | `		}` |
|       - |  2379 | `#endif` |
|       - |  2380 | `		/* php names scandir() here, not the opendir() this engine writes its body` |
|       - |  2381 | `		 * in terms of -- see PH7_CtxDiagFuncName. dir() reaches this door as a host` |
|       - |  2382 | `		 * builtin of its own and keeps its own name either way. */` |
|      40 |  2383 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",` |
|      12 |  2384 | `			PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn)),zAsked,zReason);` |
|      28 |  2385 | `		OpenDirScandirErrno(pCtx,iErr);` |
|      28 |  2386 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      28 |  2387 | `		ph7_result_bool(pCtx,0);` |
|      16 |  2388 | `	}else{` |
|       - |  2389 | `		/* php's directory handles carry a mode and NO uri, and name their own` |
|       - |  2390 | ``		 * ops `dir` rather than the byte-stream STDIO. */`` |
|    1866 |  2391 | `		SetIOPrivateOpenedAs(pDev,0,0,"r",1);` |
|    1866 |  2392 | `		pDev->bDir = 1;` |
|       - |  2393 | `		/* php remembers this one as the "last opened directory stream", which is` |
|       - |  2394 | `		 * what readdir()/rewinddir()/closedir() reach for when they are given` |
|       - |  2395 | `		 * null. Only opendir() sets it; dir() goes through here too. */` |
|    1866 |  2396 | `		pCtx->pVm->pLastDir = (void *)pDev;` |
|       - |  2397 | `		/* Return the handle as a resource */` |
|    1866 |  2398 | `		ph7_result_resource(pCtx,pDev);` |
|       - |  2399 | `	}` |
|    1890 |  2400 | `	return PH7_OK;` |
|     942 |  2401 | `}` |
|       - |  2402 | `/*` |
|       - |  2403 | ``  * `dir(string $directory, $context = null): Directory\|false` `` |
|       - |  2404 | ` *` |
|       - |  2405 | ` * php's own dir() opens the stream and fills the object itself, which is why its` |
|       - |  2406 | ` * class needs no constructor. The open goes through the engine's opendir builtin` |
|       - |  2407 | `` * with THIS context, so the failure warning names `dir(...)` exactly as php's`` |
|       - |  2408 | ` * does; a failed open is FALSE, where the chunk's version handed back a Directory` |
|       - |  2409 | `` * whose handle was `false`.`` |
|       - |  2410 | ` */` |
|      12 |  2411 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  2412 | `{` |
|       - |  2413 | `	ph7_class_instance *pObj;` |
|       - |  2414 | `	ph7_class *pClass;` |
|       - |  2415 | `	ph7_value *pRet;` |
|       - |  2416 | `	int rc;` |
|      15 |  2417 | `	rc = PH7_builtin_opendir(pCtx,nArg,apArg);` |
|      15 |  2418 | `	if( rc != PH7_OK ){` |
|     ! 0 |  2419 | `		return rc;` |
|       - |  2420 | `	}` |
|      15 |  2421 | `	pRet = pCtx->pRet;` |
|      15 |  2422 | `	if( pRet == 0 \|\| (pRet->iFlags & MEMOBJ_RES) == 0 ){` |
|       8 |  2423 | `		ph7_result_bool(pCtx,0);` |
|       8 |  2424 | `		return PH7_OK;` |
|       - |  2425 | `	}` |
|       8 |  2426 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);` |
|       8 |  2427 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|       8 |  2428 | `	if( pObj == 0 ){` |
|     ! 0 |  2429 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2430 | `		return PH7_OK;` |
|       - |  2431 | `	}` |
|       - |  2432 | `	/* php's order: the path first, then the handle (var_dump shows both). */` |
|       - |  2433 | `	{` |
|       8 |  2434 | `		int nPath = 0;` |
|       8 |  2435 | `		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";` |
|       8 |  2436 | `		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);` |
|       - |  2437 | `	}` |
|       8 |  2438 | `	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);` |
|       8 |  2439 | `	PH7_NativeResultObject(pCtx,pObj);` |
|       8 |  2440 | `	return PH7_OK;` |
|       9 |  2441 | `}` |
|       - |  2442 | `/*` |
|       - |  2443 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|       - |  2444 | ` *  Reads a file and writes it to the output buffer.` |
|       - |  2445 | ` * Parameters` |
|       - |  2446 | ` *  $filename` |
|       - |  2447 | ` *   The filename being read.` |
|       - |  2448 | ` *  $use_include_path` |
|       - |  2449 | ` *   You can use the optional second parameter and set it to` |
|       - |  2450 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - |  2451 | ` *  $context` |
|       - |  2452 | ` *   A context stream resource.` |
|       - |  2453 | ` * Return` |
|       - |  2454 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|       - |  2455 | ` */` |
|       - |  2456 | `/*` |
|       - |  2457 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|       - |  2458 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|       - |  2459 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|       - |  2460 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|       - |  2461 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|       - |  2462 | ` * trailing reason comes from.` |
|       - |  2463 | ` */` |
|      18 |  2464 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  2465 | `{` |
|      21 |  2466 | `	int use_include  = FALSE;` |
|       - |  2467 | `	const ph7_io_stream *pStream;` |
|       - |  2468 | `	ph7_int64 n,nRead;` |
|       - |  2469 | `	const char *zFile;` |
|       - |  2470 | `	char zBuf[8192];` |
|       - |  2471 | `	void *pHandle;` |
|       - |  2472 | `	phl_stream_ctx *pCtxRes;` |
|      21 |  2473 | `	int rc,nLen,bThrew = 0;` |
|      21 |  2474 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - |  2475 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2476 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 |  2477 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2478 | `		return PH7_OK;` |
|       - |  2479 | `	}` |
|       - |  2480 | `	/* Extract the file path */` |
|      21 |  2481 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      21 |  2482 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 |  2483 | `		return PH7_OK;` |
|       - |  2484 | `	}` |
|       - |  2485 | `	/* Point to the target IO stream device */` |
|      19 |  2486 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      19 |  2487 | `	if( pStream == 0 ){` |
|     ! 0 |  2488 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 |  2489 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2490 | `		return PH7_OK;` |
|       - |  2491 | `	}` |
|      19 |  2492 | `	if( nArg > 1 ){` |
|       6 |  2493 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|       2 |  2494 | `	}` |
|       - |  2495 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - |  2496 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - |  2497 | `	 * The armed one describes exactly this open. */` |
|      19 |  2498 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      19 |  2499 | `	if( bThrew ){` |
|       6 |  2500 | `		return PH7_OK;` |
|       - |  2501 | `	}` |
|      15 |  2502 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - |  2503 | `	/* Try to open the file in read-only mode */` |
|      21 |  2504 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|       6 |  2505 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      15 |  2506 | `	if( pHandle == 0 ){` |
|      11 |  2507 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      11 |  2508 | `		ph7_result_bool(pCtx,0);` |
|      11 |  2509 | `		return PH7_OK;` |
|       - |  2510 | `	}` |
|       - |  2511 | `	/* Perform the requested operation */` |
|       5 |  2512 | `	nRead = 0;` |
|       4 |  2513 | `	for(;;){` |
|       9 |  2514 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|       9 |  2515 | `		if( n < 1 ){` |
|       - |  2516 | `			/* EOF or IO error,break immediately */` |
|       5 |  2517 | `			break;` |
|       - |  2518 | `		}` |
|       - |  2519 | `		/* Output data */` |
|       5 |  2520 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|       5 |  2521 | `		if( rc == PH7_ABORT ){` |
|     ! 0 |  2522 | `			break;` |
|       - |  2523 | `		}` |
|       - |  2524 | `		/* Increment counter */` |
|       5 |  2525 | `		nRead += n;` |
|       1 |  2526 | `	}` |
|       - |  2527 | `	/* Close the stream */` |
|       5 |  2528 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - |  2529 | `	/* Total number of bytes readen */` |
|       5 |  2530 | `	ph7_result_int64(pCtx,nRead);` |
|       5 |  2531 | `	return PH7_OK;` |
|      12 |  2532 | `}` |
|       - |  2533 | `/*` |
|       - |  2534 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|       - |  2535 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|       - |  2536 | ` *  Reads entire file into a string.` |
|       - |  2537 | ` * Parameters` |
|       - |  2538 | ` *  $filename` |
|       - |  2539 | ` *   The filename being read.` |
|       - |  2540 | ` *  $use_include_path` |
|       - |  2541 | ` *   You can use the optional second parameter and set it to` |
|       - |  2542 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - |  2543 | ` *  $context` |
|       - |  2544 | ` *   A context stream resource.` |
|       - |  2545 | ` *  $offset` |
|       - |  2546 | ` *   The offset where the reading starts on the original stream.` |
|       - |  2547 | ` *  $maxlen` |
|       - |  2548 | ` *    Maximum length of data read. The default is to read until end of file` |
|       - |  2549 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|       - |  2550 | ` * Return` |
|       - |  2551 | ` *   The function returns the read data or FALSE on failure.` |
|       - |  2552 | ` */` |
|   10634 |  2553 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  2554 | `{` |
|       - |  2555 | `	const ph7_io_stream *pStream;` |
|       - |  2556 | `	ph7_int64 n,nRead,nMaxlen;` |
|   10639 |  2557 | `	int use_include  = FALSE;` |
|       - |  2558 | `	const char *zFile;` |
|       - |  2559 | `	char zBuf[8192];` |
|       - |  2560 | `	void *pHandle;` |
|       - |  2561 | `	phl_stream_ctx *pCtxRes;` |
|   10639 |  2562 | `	int nLen,bThrew = 0;` |
|       - |  2563 |  |
|   10639 |  2564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - |  2565 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 |  2567 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2568 | `		return PH7_OK;` |
|       - |  2569 | `	}` |
|       - |  2570 | `	/* Extract the file path */` |
|   10639 |  2571 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|   10639 |  2572 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 |  2573 | `		return PH7_OK;` |
|       - |  2574 | `	}` |
|       - |  2575 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|       - |  2576 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|       - |  2577 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|       - |  2578 | `	 * the whole file. */` |
|   10637 |  2579 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      27 |  2580 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|       5 |  2581 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  2582 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|       - |  2583 | `		}` |
|      11 |  2584 | `	}` |
|       - |  2585 | `	/* Point to the target IO stream device */` |
|   10633 |  2586 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|   10633 |  2587 | `	if( pStream == 0 ){` |
|      18 |  2588 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|      18 |  2589 | `		ph7_result_bool(pCtx,0);` |
|      18 |  2590 | `		return PH7_OK;` |
|       - |  2591 | `	}` |
|   10617 |  2592 | `	nMaxlen = -1;` |
|   10617 |  2593 | `	if( nArg > 1 ){` |
|     241 |  2594 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     119 |  2595 | `	}` |
|       - |  2596 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - |  2597 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - |  2598 | `	 * The armed one describes exactly this open. */` |
|   10617 |  2599 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|   10617 |  2600 | `	if( bThrew ){` |
|       5 |  2601 | `		return PH7_OK;` |
|       - |  2602 | `	}` |
|   10613 |  2603 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - |  2604 | `	/* Try to open the file in read-only mode */` |
|   10613 |  2605 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|   10613 |  2606 | `	if( pHandle == 0 ){` |
|      97 |  2607 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      97 |  2608 | `		ph7_result_bool(pCtx,0);` |
|      97 |  2609 | `		return PH7_OK;` |
|       - |  2610 | `	}` |
|   10520 |  2611 | `	if( nArg > 3 ){` |
|       - |  2612 | `		/* Extract the offset */` |
|      25 |  2613 | `		n = ph7_value_to_int64(apArg[3]);` |
|      25 |  2614 | `		if( n > 0 ){` |
|       7 |  2615 | `			if( pStream->xSeek ){` |
|       - |  2616 | `				/* Seek to the desired offset */` |
|       7 |  2617 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|       3 |  2618 | `			}` |
|       3 |  2619 | `		}` |
|      25 |  2620 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       - |  2621 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|       - |  2622 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|       - |  2623 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|      23 |  2624 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      11 |  2625 | `		}` |
|      12 |  2626 | `	}` |
|       - |  2627 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|       - |  2628 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|       - |  2629 | `	 * through to the empty-string result below. */` |
|   10520 |  2630 | `	nRead = 0;` |
|   21211 |  2631 | `	while( nMaxlen != 0 ){` |
|       - |  2632 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|       - |  2633 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|       - |  2634 | `		 * otherwise overshoot and append past $length. */` |
|   21207 |  2635 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   21207 |  2636 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|      17 |  2637 | `			nAsk = nMaxlen - nRead;` |
|       8 |  2638 | `		}` |
|   21207 |  2639 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|   21207 |  2640 | `		if( n < 1 ){` |
|       - |  2641 | `			/* EOF or IO error,break immediately */` |
|   10502 |  2642 | `			break;` |
|       - |  2643 | `		}` |
|       - |  2644 | `		/* Append data */` |
|   10710 |  2645 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|       - |  2646 | `		/* Increment read counter */` |
|   10710 |  2647 | `		nRead += n;` |
|   10710 |  2648 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|       - |  2649 | `			/* Read limit reached */` |
|      15 |  2650 | `			break;` |
|       - |  2651 | `		}` |
|       5 |  2652 | `	}` |
|       - |  2653 | `	/* Close the stream */` |
|   10520 |  2654 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - |  2655 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|       - |  2656 | `	 * open failure, handled above); the read loop never set a string result, so` |
|       - |  2657 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   10520 |  2658 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     123 |  2659 | `		ph7_result_string(pCtx,"",0);` |
|      59 |  2660 | `	}` |
|   10520 |  2661 | `	return PH7_OK;` |
|    5320 |  2662 | `}` |
|       - |  2663 | `/*` |
|       - |  2664 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|       - |  2665 | ` *  Write a string to a file.` |
|       - |  2666 | ` * Parameters` |
|       - |  2667 | ` *  $filename` |
|       - |  2668 | ` *  Path to the file where to write the data.` |
|       - |  2669 | ` * $data` |
|       - |  2670 | ` *  The data to write(Must be a string).` |
|       - |  2671 | ` * $flags` |
|       - |  2672 | ` *  The value of flags can be any combination of the following` |
|       - |  2673 | ` * flags, joined with the binary OR (\|) operator.` |
|       - |  2674 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|       - |  2675 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|       - |  2676 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|       - |  2677 | ` * context` |
|       - |  2678 | ` *  A context stream resource.` |
|       - |  2679 | ` * Return` |
|       - |  2680 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|       - |  2681 | ` */` |
|       - |  2682 | `/*` |
|       - |  2683 | ` * Append a buffer to a file, creating it when absent, and raise php's open` |
|       - |  2684 | `` * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's`` |
|       - |  2685 | ` * name when it cannot be opened. Returns PH7_OK or -1.` |
|       - |  2686 | ` *` |
|       - |  2687 | ` * This is error_log()'s message_type 3, factored here because that is where the` |
|       - |  2688 | ` * stream device, the open flags and the warning shape already live.` |
|       - |  2689 | ` */` |
|      10 |  2690 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)` |
|       2 |  2691 | `{` |
|       - |  2692 | `	const ph7_io_stream *pStream;` |
|       - |  2693 | `	void *pHandle;` |
|       - |  2694 | `	int nPath;` |
|      12 |  2695 | `	if( zFile == 0 \|\| zFile[0] == 0 ){` |
|     ! 0 |  2696 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 |  2697 | `		return -1;` |
|       - |  2698 | `	}` |
|      12 |  2699 | `	nPath = (int)SyStrlen(zFile);` |
|      12 |  2700 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);` |
|      12 |  2701 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|     ! 0 |  2702 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 |  2703 | `		return -1;` |
|       - |  2704 | `	}` |
|       - |  2705 | `	/* php opens for WRITING only here ("ab"), and nothing reads back through the` |
|       - |  2706 | `	 * handle -- which is also the mode string a userland wrapper is handed. */` |
|      17 |  2707 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|       5 |  2708 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      12 |  2709 | `	if( pHandle == 0 ){` |
|       3 |  2710 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 |  2711 | `		return -1;` |
|       - |  2712 | `	}` |
|      10 |  2713 | `	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){` |
|     ! 0 |  2714 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     ! 0 |  2715 | `		return -1;` |
|       - |  2716 | `	}` |
|      10 |  2717 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      10 |  2718 | `	return PH7_OK;` |
|       7 |  2719 | `}` |
|       - |  2720 | `/*` |
|       - |  2721 | ` * file_put_contents()'s array $data: php walks the array and writes the` |
|       - |  2722 | ` * elements one after another with nothing between them, casting each one` |
|       - |  2723 | `` * user-visibly -- so a nested array warns `Array to string conversion` and`` |
|       - |  2724 | ` * contributes "Array", and an element that is an object with no __toString()` |
|       - |  2725 | ` * is php's catchable Error and the file is left as the open truncated it.` |
|       - |  2726 | ` */` |
|       - |  2727 | `typedef struct VfsPutContentsJoin VfsPutContentsJoin;` |
|       - |  2728 | `struct VfsPutContentsJoin` |
|       - |  2729 | `{` |
|       - |  2730 | `	ph7_context *pCtx;` |
|       - |  2731 | `	SyBlob *pOut;` |
|       - |  2732 | `	sxi32 rcThrow;      /* Allocation failure only */` |
|       - |  2733 | `	ph7_class *pOwed;   /* First class that could not be coerced; raised after the write */` |
|       - |  2734 | `};` |
|      18 |  2735 | `static int VfsPutContentsWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 |  2736 | `{` |
|      19 |  2737 | `	VfsPutContentsJoin *pJn = (VfsPutContentsJoin *)pUserData;` |
|       - |  2738 | `	const char *zElem;` |
|       - |  2739 | `	int nElem;` |
|       - |  2740 | `	ph7_class *pBad;` |
|       9 |  2741 | `	SXUNUSED(pKey);` |
|       - |  2742 | `	/* php does not stop for the coercion's throw: the failing element contributes` |
|       - |  2743 | `	 * nothing and the ELEMENTS AFTER IT are still written, so` |
|       - |  2744 | ``	 * `file_put_contents($f,['A',$obj,'B'])` leaves "AB" in the file and reports`` |
|       - |  2745 | `	 * the Error afterwards -- which is why the raise is deferred to the caller. */` |
|      19 |  2746 | `	pBad = PH7_ValueToStringUVDefer(pJn->pCtx,pData,&zElem,&nElem);` |
|      19 |  2747 | `	if( pBad && pJn->pOwed == 0 ){` |
|       3 |  2748 | `		pJn->pOwed = pBad;` |
|       1 |  2749 | `	}` |
|      19 |  2750 | `	if( nElem > 0 && SyBlobAppend(pJn->pOut,(const void *)zElem,(sxu32)nElem) != SXRET_OK ){` |
|     ! 0 |  2751 | `		pJn->rcThrow = PH7_ContextMemoryError(pJn->pCtx);` |
|     ! 0 |  2752 | `		return PH7_ABORT;` |
|       - |  2753 | `	}` |
|      19 |  2754 | `	return PH7_OK;` |
|      10 |  2755 | `}` |
|   20524 |  2756 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  2757 | `{` |
|   20529 |  2758 | `	int use_include  = FALSE;` |
|       - |  2759 | `	const ph7_io_stream *pStream;` |
|       - |  2760 | `	const char *zFile;` |
|       - |  2761 | `	const char *zData;` |
|       - |  2762 | `	int iOpenFlags;` |
|       - |  2763 | `	void *pHandle;` |
|       - |  2764 | `	phl_stream_ctx *pCtxRes;` |
|       - |  2765 | `	int iFlags;` |
|   20529 |  2766 | `	int nLen,bThrew = 0;` |
|       - |  2767 | `	SyBlob sJoin;        /* array/stream $data, joined (see below) */` |
|   20529 |  2768 | `	ph7_class *pOwed = 0;/* A $data element php stringifies to nothing and throws for */` |
|   20529 |  2769 | `	int bScalarBad = 0;  /* Scalar $data php cannot stringify: FALSE, and no throw */` |
|       - |  2770 |  |
|   20529 |  2771 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - |  2772 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2773 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 |  2774 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2775 | `		return PH7_OK;` |
|       - |  2776 | `	}` |
|       - |  2777 | `	/* Extract the file path */` |
|   20529 |  2778 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|   20529 |  2779 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 |  2780 | `		return PH7_OK;` |
|       - |  2781 | `	}` |
|       - |  2782 | `	/* Point to the target IO stream device */` |
|   20527 |  2783 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|   20527 |  2784 | `	if( pStream == 0 ){` |
|     ! 0 |  2785 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 |  2786 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2787 | `		return PH7_OK;` |
|       - |  2788 | `	}` |
|       - |  2789 | ``	/* The bytes to write, which php reads from `mixed $data` in THREE shapes and`` |
|       - |  2790 | `	 * PHL string-cast wholesale -- so an ARRAY wrote the five bytes "Array" and a` |
|       - |  2791 | `	 * STREAM wrote "Resource id #N", both instead of the content the program` |
|       - |  2792 | `	 * meant. php JOINS an array's elements with nothing between them (its own` |
|       - |  2793 | `	 * comment calls it "an array-to-string with no glue"), casting each element` |
|       - |  2794 | `	 * user-visibly, and COPIES a stream handle's remaining contents. Anything` |
|       - |  2795 | `	 * else is the ordinary user-visible cast: an object hands over its` |
|       - |  2796 | `	 * __toString(), one without it is php's catchable Error.` |
|       - |  2797 | `	 *` |
|       - |  2798 | `	 * The joined bytes have to outlive this block, so they live in sJoin until` |
|       - |  2799 | `	 * the write below; the scalar path keeps pointing straight at the value. */` |
|   20527 |  2800 | `	SyBlobInit(&sJoin,&pCtx->pVm->sAllocator);` |
|   20527 |  2801 | `	if( ph7_value_is_array(apArg[1]) ){` |
|       - |  2802 | `		VfsPutContentsJoin sJn;` |
|       5 |  2803 | `		sJn.pCtx = pCtx;` |
|       5 |  2804 | `		sJn.pOut = &sJoin;` |
|       5 |  2805 | `		sJn.rcThrow = SXRET_OK;` |
|       5 |  2806 | `		sJn.pOwed = 0;` |
|       5 |  2807 | `		ph7_array_walk(apArg[1],VfsPutContentsWalker,&sJn);` |
|       5 |  2808 | `		if( sJn.rcThrow != SXRET_OK ){` |
|       - |  2809 | `			/* Allocation failure only -- a coercion carries on above. */` |
|     ! 0 |  2810 | `			SyBlobRelease(&sJoin);` |
|     ! 0 |  2811 | `			return sJn.rcThrow;` |
|       - |  2812 | `		}` |
|       5 |  2813 | `		pOwed = sJn.pOwed;` |
|       5 |  2814 | `		zData = (const char *)SyBlobData(&sJoin);` |
|       5 |  2815 | `		nLen = (int)SyBlobLength(&sJoin);` |
|   20525 |  2816 | `	}else if( ph7_value_is_resource(apArg[1]) ){` |
|       3 |  2817 | `		io_private *pSrc = (io_private *)ph7_value_to_resource(apArg[1]);` |
|       3 |  2818 | `		if( !IO_PRIVATE_INVALID(pSrc) && pSrc->pStream && pSrc->pStream->xRead ){` |
|       3 |  2819 | `			PH7_StreamReadWholeFile(pSrc->pHandle,pSrc->pStream,&sJoin);` |
|       1 |  2820 | `		}` |
|       3 |  2821 | `		zData = (const char *)SyBlobData(&sJoin);` |
|       3 |  2822 | `		nLen = (int)SyBlobLength(&sJoin);` |
|       2 |  2823 | `	}else{` |
|   20521 |  2824 | `		if( PH7_ValueToStringUVDefer(pCtx,apArg[1],&zData,&nLen) != 0 ){` |
|       - |  2825 | `			/* php's SCALAR $data path is not its array one: an object it cannot` |
|       - |  2826 | `			 * stringify raises NOTHING here -- the file is still opened and` |
|       - |  2827 | `			 * TRUNCATED, nothing is written, and the call answers FALSE. The` |
|       - |  2828 | `			 * asymmetry with the array branch above (which does throw) is php's. */` |
|       3 |  2829 | `			bScalarBad = 1;` |
|       1 |  2830 | `		}` |
|       - |  2831 | `	}` |
|       - |  2832 | `	/* php opens for WRITING only -- "wb", or "ab" once FILE_APPEND turns up below --` |
|       - |  2833 | `	 * and that is the mode string a userland wrapper is handed. */` |
|   20527 |  2834 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC;` |
|       - |  2835 | `	/* Extract the flags */` |
|   20527 |  2836 | `	iFlags = 0;` |
|   20527 |  2837 | `	if( nArg > 2 ){` |
|      11 |  2838 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|      11 |  2839 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|     ! 0 |  2840 | `			use_include = TRUE;` |
|     ! 0 |  2841 | `		}` |
|      11 |  2842 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|       - |  2843 | `			/* If the file already exists, append the data to the file` |
|       - |  2844 | `			 * instead of overwriting it.` |
|       - |  2845 | `			 */` |
|       3 |  2846 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|       - |  2847 | `			/* Append mode */` |
|       3 |  2848 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|       1 |  2849 | `		}` |
|       4 |  2850 | `	}` |
|       - |  2851 | `	/* FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|       - |  2852 | `	 * back to the default context" — which is why it needed one to exist. */` |
|   30783 |  2853 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",` |
|   20522 |  2854 | `		(iFlags & 0x10) != 0,&bThrew);` |
|   20527 |  2855 | `	if( bThrew ){` |
|       6 |  2856 | `		SyBlobRelease(&sJoin);` |
|       6 |  2857 | `		return PH7_OK;` |
|       - |  2858 | `	}` |
|   20523 |  2859 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|   30777 |  2860 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   10254 |  2861 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));` |
|   20523 |  2862 | `	if( pHandle == 0 ){` |
|      12 |  2863 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      12 |  2864 | `		ph7_result_bool(pCtx,0);` |
|      12 |  2865 | `		SyBlobRelease(&sJoin);` |
|      12 |  2866 | `		return PH7_OK;` |
|       - |  2867 | `	}` |
|   20513 |  2868 | `	if( bScalarBad ){` |
|       - |  2869 | `		/* Opened and truncated, then refused — php's answer, and its order. */` |
|       3 |  2870 | `		ph7_result_bool(pCtx,0);` |
|       3 |  2871 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|       3 |  2872 | `		SyBlobRelease(&sJoin);` |
|       3 |  2873 | `		return PH7_OK;` |
|       - |  2874 | `	}` |
|   20511 |  2875 | `	if( nLen < 1 ){` |
|       - |  2876 | `		/* Empty data, file is created/truncated */` |
|     221 |  2877 | `		ph7_result_int64(pCtx,0);` |
|     221 |  2878 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     221 |  2879 | `		SyBlobRelease(&sJoin);` |
|     221 |  2880 | `		if( pOwed ){` |
|     ! 0 |  2881 | `			return PH7_VmThrowException(pCtx,"Error",` |
|       - |  2882 | `				"Object of class %.*s could not be converted to string",` |
|     ! 0 |  2883 | `				(int)pOwed->sDisp.nByte,pOwed->sDisp.zString);` |
|       - |  2884 | `		}` |
|     221 |  2885 | `		return PH7_OK;` |
|       - |  2886 | `	}` |
|   20295 |  2887 | `	if( pStream->xWrite ){` |
|       - |  2888 | `		ph7_int64 n;` |
|   20293 |  2889 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|       - |  2890 | `			/* Try to acquire an exclusive lock */` |
|     ! 0 |  2891 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|     ! 0 |  2892 | `		}` |
|       - |  2893 | `		/* Perform the write operation */` |
|   20293 |  2894 | `		errno = 0;` |
|   20293 |  2895 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|   20293 |  2896 | `		if( n < 0 ){` |
|       - |  2897 | `			/* IO error,return FALSE — with php's write-failure diagnostic,` |
|       - |  2898 | `			 * which is a NOTICE and comes from the device rather than from` |
|       - |  2899 | ``			 * here: `file_put_contents('php://input','q')` is a silent false. */`` |
|       3 |  2900 | `			StreamReportRawWriteFailure(pCtx,pStream,(int)nLen,errno);` |
|       3 |  2901 | `			ph7_result_bool(pCtx,0);` |
|       2 |  2902 | `		}else{` |
|       - |  2903 | `			/* Total number of bytes written */` |
|   20291 |  2904 | `			ph7_result_int64(pCtx,n);` |
|       - |  2905 | `		}` |
|   10144 |  2906 | `	}else{` |
|       - |  2907 | ``		/* A wrapper with no writer at all: php's `Stream is not writable`,`` |
|       - |  2908 | `		 * the same notice fwrite() gives, and not a fatal of our own. */` |
|       3 |  2909 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|       3 |  2910 | `		ph7_result_bool(pCtx,0);` |
|       - |  2911 | `	}` |
|       - |  2912 | `	/* Close the handle */` |
|   20295 |  2913 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   20295 |  2914 | `	SyBlobRelease(&sJoin);` |
|   20295 |  2915 | `	if( pOwed ){` |
|       4 |  2916 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  2917 | `			"Object of class %.*s could not be converted to string",` |
|       2 |  2918 | `			(int)pOwed->sDisp.nByte,pOwed->sDisp.zString);` |
|       - |  2919 | `	}` |
|   20293 |  2920 | `	return PH7_OK;` |
|   10262 |  2921 | `}` |
|       - |  2922 | `/*` |
|       - |  2923 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|       - |  2924 | ` *  Reads entire file into an array.` |
|       - |  2925 | ` * Parameters` |
|       - |  2926 | ` *  $filename` |
|       - |  2927 | ` *   The filename being read.` |
|       - |  2928 | ` *  $flags` |
|       - |  2929 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|       - |  2930 | ` *   FILE_USE_INCLUDE_PATH` |
|       - |  2931 | ` *       Search for the file in the include_path.` |
|       - |  2932 | ` *   FILE_IGNORE_NEW_LINES` |
|       - |  2933 | ` *       Do not add newline at the end of each array element` |
|       - |  2934 | ` *   FILE_SKIP_EMPTY_LINES` |
|       - |  2935 | ` *       Skip empty lines` |
|       - |  2936 | ` *  $context` |
|       - |  2937 | ` *   A context stream resource.` |
|       - |  2938 | ` * Return` |
|       - |  2939 | ` *   The function returns the read data or FALSE on failure.` |
|       - |  2940 | ` */` |
|     172 |  2941 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  2942 | `{` |
|       - |  2943 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|       - |  2944 | `	ph7_value *pArray,*pLine;` |
|       - |  2945 | `	const ph7_io_stream *pStream;` |
|     176 |  2946 | `	int use_include = 0;` |
|       - |  2947 | `	io_private *pDev;` |
|       - |  2948 | `	phl_stream_ctx *pCtxRes;` |
|       - |  2949 | `	ph7_int64 n;` |
|       - |  2950 | `	int iFlags;` |
|     176 |  2951 | `	int nLen,bThrew = 0;` |
|       - |  2952 |  |
|     176 |  2953 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - |  2954 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  2955 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 |  2956 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2957 | `		return PH7_OK;` |
|       - |  2958 | `	}` |
|     176 |  2959 | `	iFlags = 0;` |
|     176 |  2960 | `	if( nArg > 1 ){` |
|       - |  2961 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|       - |  2962 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|       - |  2963 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|       - |  2964 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|       - |  2965 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|       - |  2966 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|       - |  2967 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|       - |  2968 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|       - |  2969 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|       - |  2970 | `		 * mask. */` |
|     154 |  2971 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     154 |  2972 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|      13 |  2973 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  2974 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|       - |  2975 | `		}` |
|     142 |  2976 | `		iFlags = (int)nFlags;` |
|      70 |  2977 | `	}` |
|       - |  2978 | `	/* Resolved here for the same reason the flag mask is: a refused $context` |
|       - |  2979 | `	 * must not strand the io_private chunk allocated below.` |
|       - |  2980 | `	 * FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|       - |  2981 | `	 * back to the default context" — which is why it needed one to exist. */` |
|     244 |  2982 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",` |
|     160 |  2983 | `		(iFlags & 0x10) != 0,&bThrew);` |
|     164 |  2984 | `	if( bThrew ){` |
|       3 |  2985 | `		return PH7_OK;` |
|       - |  2986 | `	}` |
|       - |  2987 | `	/* Extract the file path */` |
|     162 |  2988 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|     162 |  2989 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 |  2990 | `		return PH7_OK;` |
|       - |  2991 | `	}` |
|       - |  2992 | `	/* Point to the target IO stream device */` |
|     160 |  2993 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|     160 |  2994 | `	if( pStream == 0 ){` |
|     ! 0 |  2995 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 |  2996 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  2997 | `		return PH7_OK;` |
|       - |  2998 | `	}` |
|       - |  2999 | `	/* Allocate a new IO private instance */` |
|     160 |  3000 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     160 |  3001 | `	if( pDev == 0 ){` |
|     ! 0 |  3002 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  3003 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3004 | `		return PH7_OK;` |
|       - |  3005 | `	}` |
|       - |  3006 | `	/* Initialize the structure */` |
|     160 |  3007 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     160 |  3008 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|       3 |  3009 | `		use_include = TRUE;` |
|       1 |  3010 | `	}` |
|       - |  3011 | `	/* Create the array and the working value */` |
|     160 |  3012 | `	pArray = ph7_context_new_array(pCtx);` |
|     160 |  3013 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     160 |  3014 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|     ! 0 |  3015 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  3016 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3017 | `		return PH7_OK;` |
|       - |  3018 | `	}` |
|     160 |  3019 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - |  3020 | `	/* Try to open the file in read-only mode */` |
|     160 |  3021 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     160 |  3022 | `	if( pDev->pHandle == 0 ){` |
|      13 |  3023 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      13 |  3024 | `		ph7_result_bool(pCtx,0);` |
|       - |  3025 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|       - |  3026 | `		 * as soon we return from this function.` |
|       - |  3027 | `		 */` |
|      13 |  3028 | `		return PH7_OK;` |
|       - |  3029 | `	}` |
|       - |  3030 | `	/* Perform the requested operation */` |
|     205 |  3031 | `	for(;;){` |
|       - |  3032 | `		/* Try to extract a line */` |
|     416 |  3033 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|     416 |  3034 | `		if( n < 1 ){` |
|       - |  3035 | `			/* EOF or IO error */` |
|     148 |  3036 | `			break;` |
|       - |  3037 | `		}` |
|       - |  3038 | `		/* Reset the cursor */` |
|     270 |  3039 | `		ph7_value_reset_string_cursor(pLine);` |
|       - |  3040 | `		/* Remove line ending if requested by the caller */` |
|     270 |  3041 | `		zPtr = zBuf;` |
|     270 |  3042 | `		zEnd = &zBuf[n];` |
|     270 |  3043 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|       - |  3044 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|       - |  3045 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|       - |  3046 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|       - |  3047 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|       - |  3048 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     211 |  3049 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     199 |  3050 | `				n--;` |
|     199 |  3051 | `				zEnd--;` |
|     199 |  3052 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|      13 |  3053 | `					n--;` |
|      13 |  3054 | `					zEnd--;` |
|       6 |  3055 | `				}` |
|      99 |  3056 | `			}` |
|     105 |  3057 | `		}` |
|     270 |  3058 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|       - |  3059 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|       - |  3060 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|       - |  3061 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|       - |  3062 | `			 * line (never zero-length, since the newline is still attached) was` |
|       - |  3063 | `			 * dropped too. Both are silent data loss from a read. */` |
|     187 |  3064 | `			if( zEnd <= zPtr ){` |
|       5 |  3065 | `				continue;` |
|       - |  3066 | `			}` |
|      91 |  3067 | `		}` |
|     266 |  3068 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|       - |  3069 | `		/* Insert line */` |
|     266 |  3070 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|       2 |  3071 | `	}` |
|       - |  3072 | `	/* Close the stream */` |
|     148 |  3073 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|       - |  3074 | `	/* Release the io_private instance */` |
|     148 |  3075 | `	ReleaseIOPrivate(pCtx,pDev);` |
|       - |  3076 | `	/* Return the created array */` |
|     148 |  3077 | `	ph7_result_value(pCtx,pArray);` |
|     148 |  3078 | `	return PH7_OK;` |
|      90 |  3079 | `}` |
|       - |  3080 | `/*` |
|       - |  3081 | ` * php's copy() stats BOTH ends before it opens either one, and the three` |
|       - |  3082 | `` * refusals that screen comes to are the reason `copy($f,$f)` does not destroy`` |
|       - |  3083 | ` * $f: the destination is opened "wb", so reaching the open at all truncates the` |
|       - |  3084 | ` * source to nothing and copies the empty result over itself.` |
|       - |  3085 | ` *` |
|       - |  3086 | ` * The screen is php_copy_file_ctx()'s own, in its order: a source that stats as` |
|       - |  3087 | ` * a DIRECTORY, then a destination that does, then the two naming ONE file --` |
|       - |  3088 | ` * which php decides on st_dev/st_ino, not on the spelling, so a hard link, a` |
|       - |  3089 | `` * symlink to the source and `./f` against `f` are all refused. The identity`` |
|       - |  3090 | ` * refusal is SILENT; only the two directory arms say anything.` |
|       - |  3091 | ` *` |
|       - |  3092 | `` * A stat that fails is php's `safe_to_copy` -- a destination that does not exist`` |
|       - |  3093 | ` * yet is the ordinary case -- and so is a wrapper with no url_stat behind it` |
|       - |  3094 | ` * (php://, data://, http://), which is what restricting the screen to the` |
|       - |  3095 | ` * platform file device answers here.` |
|       - |  3096 | ` */` |
|      72 |  3097 | `static int CopyIsPlainFileDevice(const ph7_io_stream *pStream)` |
|       3 |  3098 | `{` |
|       - |  3099 | `#ifdef __WINNT__` |
|       3 |  3100 | `	return pStream == &sWinFileStream;` |
|       - |  3101 | `#elif defined(__UNIXES__)` |
|      72 |  3102 | `	return pStream == &sUnixFileStream;` |
|       - |  3103 | `#else` |
|       - |  3104 | `	SXUNUSED(pStream);` |
|       - |  3105 | `	return 0;` |
|       - |  3106 | `#endif` |
|       3 |  3107 | `}` |
|       - |  3108 | `/*` |
|       - |  3109 | ` * The three fields the screen above asks for, as php's php_stream_stat_path_ex()` |
|       - |  3110 | ` * answers them to copy(): the wrapper that owns the name answers (a userland` |
|       - |  3111 | `` * url_stat -- which warns `not implemented` even for the QUIET destination ask --`` |
|       - |  3112 | ` * or the phar archive), the plain-files device stats the path, and anything else is` |
|       - |  3113 | ` * php's "non-statable stream". A stat that FAILS is that too: php does not refuse` |
|       - |  3114 | ` * a missing source here, it goes on to the open, whose warning is what the script` |
|       - |  3115 | ` * reads. So 0 means "no record", never "no file"; 1 fills aOut; -1 is a wrapper` |
|       - |  3116 | ` * that THREW, with the exception pending.` |
|       - |  3117 | ` */` |
|      76 |  3118 | `static int CopyStatPath(ph7_context *pCtx,const ph7_io_stream *pStream,` |
|       - |  3119 | `	const char *zAsked,const char *zPath,int bQuiet,ph7_int64 *aOut)` |
|       3 |  3120 | `{` |
|       - |  3121 | `	static const char * const azField[] = { "dev","ino","mode" };` |
|      79 |  3122 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|       - |  3123 | `	ph7_value *pArray,*pWorker;` |
|       - |  3124 | `	ph7_int64 aVal[13];` |
|       - |  3125 | `	sxu32 i;` |
|       - |  3126 | `	int rc;` |
|      79 |  3127 | `	if( zAsked == 0 \|\| zAsked[0] == 0 ){` |
|     ! 0 |  3128 | `		return 0;` |
|       - |  3129 | `	}` |
|      79 |  3130 | `	rc = PH7_VfsUserStatFields(pCtx,zAsked,bQuiet ? PH7_STAT_ASK_EXISTS : PH7_STAT_ASK_STAT,aVal);` |
|      79 |  3131 | `	if( rc != PHL_URLSTAT_NOWRAP ){` |
|       4 |  3132 | `		if( rc == PHL_URLSTAT_FAIL ){` |
|       4 |  3133 | `			return 0;` |
|       - |  3134 | `		}` |
|     ! 0 |  3135 | `		if( rc != PHL_URLSTAT_OK ){` |
|     ! 0 |  3136 | `			return -1;` |
|       - |  3137 | `		}` |
|     ! 0 |  3138 | `		for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|     ! 0 |  3139 | `			aOut[i] = aVal[i];` |
|     ! 0 |  3140 | `		}` |
|     ! 0 |  3141 | `		return 1;` |
|       - |  3142 | `	}` |
|      72 |  3143 | `	if( zPath == 0 \|\| zPath[0] == 0 \|\| !CopyIsPlainFileDevice(pStream)` |
|      74 |  3144 | `	 \|\| pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|       3 |  3145 | `		return 0;` |
|       - |  3146 | `	}` |
|      73 |  3147 | `	pArray = ph7_context_new_array(pCtx);` |
|      73 |  3148 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|      73 |  3149 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|     ! 0 |  3150 | `		return 0;` |
|       - |  3151 | `	}` |
|      73 |  3152 | `	if( pVfs->xStat(zPath,pArray,pWorker) != PH7_OK ){` |
|      21 |  3153 | `		return 0;` |
|       - |  3154 | `	}` |
|     210 |  3155 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|     158 |  3156 | `		ph7_value *pField = ph7_array_fetch(pArray,azField[i],-1);` |
|     158 |  3157 | `		if( pField == 0 ){` |
|     ! 0 |  3158 | `			return 0;` |
|       - |  3159 | `		}` |
|     158 |  3160 | `		aOut[i] = ph7_value_to_int64(pField);` |
|      80 |  3161 | `	}` |
|      54 |  3162 | `	return 1;` |
|      41 |  3163 | `}` |
|       - |  3164 | `/*` |
|       - |  3165 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|       - |  3166 | ` *  Makes a copy of the file source to dest.` |
|       - |  3167 | ` * Parameters` |
|       - |  3168 | ` *  $source` |
|       - |  3169 | ` *   Path to the source file.` |
|       - |  3170 | ` *  $dest` |
|       - |  3171 | ` *   The destination path. If dest is a URL, the copy operation` |
|       - |  3172 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|       - |  3173 | ` *  $context` |
|       - |  3174 | ` *   A context stream resource.` |
|       - |  3175 | ` * Return` |
|       - |  3176 | ` *  TRUE on success or FALSE on failure.` |
|       - |  3177 | ` */` |
|      48 |  3178 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  3179 | `{` |
|      51 |  3180 | `	const ph7_io_stream *pSin,*pSout,*pDest = 0;` |
|      51 |  3181 | `	const char *zFile,*zSrc,*zDest = 0;` |
|       - |  3182 | `	char zBuf[8192];` |
|       - |  3183 | `	void *pIn,*pOut;` |
|       - |  3184 | `	phl_stream_ctx *pCtxRes;` |
|       - |  3185 | `	ph7_int64 n;` |
|      51 |  3186 | `	int nLen,nDest = 0,bThrew = 0;` |
|      51 |  3187 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|       - |  3188 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3189 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|     ! 0 |  3190 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3191 | `		return PH7_OK;` |
|       - |  3192 | `	}` |
|       - |  3193 | `	/* Extract the source name */` |
|      51 |  3194 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      51 |  3195 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 |  3196 | `		return PH7_OK;` |
|       - |  3197 | `	}` |
|       - |  3198 | `	/* php resolves a name's wrapper EVERY time it touches the name -- the source` |
|       - |  3199 | `	 * for its open_basedir screen, again to stat it, again to open it; the` |
|       - |  3200 | `	 * destination to stat it and again to open it -- and a scheme nobody is` |
|       - |  3201 | `	 * registered under warns on each resolution, so a script counts five` |
|       - |  3202 | ``	 * `Unable to find the wrapper` lines for two unknown schemes, three for an`` |
|       - |  3203 | `	 * unknown source alone, two for an unknown destination alone. Resolving once` |
|       - |  3204 | `	 * per name counted one. The steps below are php's, in php's order; the` |
|       - |  3205 | `	 * source's LAST resolution sits right before its open because resolving a` |
|       - |  3206 | `	 * name records the URI a failed open names. */` |
|      49 |  3207 | `	zSrc = zFile;` |
|      49 |  3208 | `	zDest = ph7_value_to_string(apArg[1],&nDest);` |
|      49 |  3209 | `	pSin = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      49 |  3210 | `	if( pSin == 0 ){` |
|     ! 0 |  3211 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 |  3212 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3213 | `		return PH7_OK;` |
|       - |  3214 | `	}` |
|       - |  3215 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - |  3216 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - |  3217 | `	 * The armed one describes exactly this open. */` |
|      49 |  3218 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      49 |  3219 | `	if( bThrew ){` |
|       3 |  3220 | `		return PH7_OK;` |
|       - |  3221 | `	}` |
|       - |  3222 | `	/* php's screen over the two paths, before either is opened. A source whose` |
|       - |  3223 | `	 * stat FAILS is not refused here: php goes straight to the open, so a missing` |
|       - |  3224 | ``	 * source is the open's `Failed to open stream` and a directory DESTINATION`` |
|       - |  3225 | `	 * behind it is never looked at -- this engine refused the destination first. */` |
|       - |  3226 | `	{` |
|       - |  3227 | `	ph7_int64 aSrc[3],aDst[3];` |
|      47 |  3228 | `	int bSrc,bDst = 0;` |
|      47 |  3229 | `	zFile = zSrc;` |
|      47 |  3230 | `	pSin = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      47 |  3231 | `	bSrc = pSin ? CopyStatPath(pCtx,pSin,zSrc,zFile,0,aSrc) : 0;` |
|      47 |  3232 | `	if( bSrc < 0 ){` |
|     ! 0 |  3233 | `		return PH7_OK;` |
|       - |  3234 | `	}` |
|      47 |  3235 | `	if( bSrc && (aSrc[2] & PH7_S_IFMT) == PH7_S_IFDIR ){` |
|       3 |  3236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|       - |  3237 | `			"The first argument to copy() function cannot be a directory");` |
|       3 |  3238 | `		ph7_result_bool(pCtx,0);` |
|       3 |  3239 | `		return PH7_OK;` |
|       - |  3240 | `	}` |
|      45 |  3241 | `	if( bSrc && nDest > 0 ){` |
|       - |  3242 | `		/* php's QUIET stat of the destination: a wrapper that declines the name` |
|       - |  3243 | `		 * (file://host/) is a non-statable stream here, and only its OPEN says why. */` |
|      34 |  3244 | `		const char *zDestPath = zDest;` |
|      34 |  3245 | `		pDest = PH7_VfsStreamDeviceOrFile(pCtx,&zDestPath,nDest);` |
|      34 |  3246 | `		bDst = pDest ? CopyStatPath(pCtx,pDest,zDest,zDestPath,1,aDst) : 0;` |
|      34 |  3247 | `		if( bDst < 0 ){` |
|     ! 0 |  3248 | `			return PH7_OK;` |
|       - |  3249 | `		}` |
|      16 |  3250 | `	}` |
|      45 |  3251 | `	if( bDst && (aDst[2] & PH7_S_IFMT) == PH7_S_IFDIR ){` |
|       5 |  3252 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|       - |  3253 | `			"The second argument to copy() function cannot be a directory");` |
|       5 |  3254 | `		ph7_result_bool(pCtx,0);` |
|       5 |  3255 | `		return PH7_OK;` |
|       - |  3256 | `	}` |
|      38 |  3257 | `	if( bSrc && bDst && aSrc[1] != 0 && aDst[1] != 0` |
|      15 |  3258 | `	 && aSrc[0] == aDst[0] && aSrc[1] == aDst[1] ){` |
|       - |  3259 | `		/* One file under two names. php says nothing and answers FALSE. */` |
|      11 |  3260 | `		ph7_result_bool(pCtx,0);` |
|      11 |  3261 | `		return PH7_OK;` |
|       - |  3262 | `	}` |
|       - |  3263 | `	}` |
|       - |  3264 | `	/* php's third resolution of the source: the open's own. */` |
|      31 |  3265 | `	zFile = zSrc;` |
|      31 |  3266 | `	pSin = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      31 |  3267 | `	if( pSin == 0 ){` |
|     ! 0 |  3268 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3269 | `		return PH7_OK;` |
|       - |  3270 | `	}` |
|      31 |  3271 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - |  3272 | `	/* Try to open the source file in a read-only mode */` |
|      31 |  3273 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      31 |  3274 | `	if( pIn == 0 ){` |
|       9 |  3275 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       9 |  3276 | `		ph7_result_bool(pCtx,0);` |
|       9 |  3277 | `		return PH7_OK;` |
|       - |  3278 | `	}` |
|       - |  3279 | `	/* Extract the destination name */` |
|      22 |  3280 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      22 |  3281 | `	if( nLen < 1 ){` |
|       - |  3282 | `		/* php's stream layer refuses this end of the copy the same way it` |
|       - |  3283 | `		 * refused the other -- with the source already open, which is what the` |
|       - |  3284 | `		 * close here is for. */` |
|       2 |  3285 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|       2 |  3286 | `		PH7_VfsEmptyPathRefused(pCtx,nLen);` |
|       2 |  3287 | `		return PH7_OK;` |
|       - |  3288 | `	}` |
|       - |  3289 | `	/* Point to the target IO stream device */` |
|      20 |  3290 | `	pSout = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      20 |  3291 | `	if( pSout == 0 ){` |
|     ! 0 |  3292 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 |  3293 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3294 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|     ! 0 |  3295 | `		return PH7_OK;` |
|       - |  3296 | `	}` |
|      20 |  3297 | `	if( pSout->xOpen != 0 && pSout->xWrite == 0 ){` |
|       - |  3298 | ``		/* php's copy() reaches the same `Stream is not writable` notice the`` |
|       - |  3299 | `		 * write doors do -- a destination wrapper with a stream opener and no` |
|       - |  3300 | `		 * writer behind it. A wrapper with no OPENER either (glob://) is` |
|       - |  3301 | `		 * refused one step earlier, by the open below, and says so. */` |
|     ! 0 |  3302 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Stream is not writable");` |
|     ! 0 |  3303 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3304 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|     ! 0 |  3305 | `		return PH7_OK;` |
|       - |  3306 | `	}` |
|       - |  3307 | `	/* php hands the ONE context to both halves of the copy. */` |
|      20 |  3308 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - |  3309 | `	/* php opens the destination for WRITING only ("wb"). */` |
|      29 |  3310 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|       9 |  3311 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_WRONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      20 |  3312 | `	if( pOut == 0 ){` |
|       2 |  3313 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       2 |  3314 | `		ph7_result_bool(pCtx,0);` |
|       2 |  3315 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|       2 |  3316 | `		return PH7_OK;` |
|       - |  3317 | `	}` |
|       - |  3318 | `	/* Perform the requested operation */` |
|       - |  3319 | `	{` |
|      18 |  3320 | `	int bFailed = 0,iErr = 0,nAsked = 0;` |
|      15 |  3321 | `	for(;;){` |
|       - |  3322 | `		/* Read from source */` |
|      32 |  3323 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      32 |  3324 | `		if( n < 1 ){` |
|       - |  3325 | `			/* EOF or IO error,break immediately */` |
|      16 |  3326 | `			break;` |
|       - |  3327 | `		}` |
|       - |  3328 | `		/* Write to dest */` |
|      18 |  3329 | `		nAsked = (int)n;` |
|      18 |  3330 | `		errno = 0;` |
|      18 |  3331 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      18 |  3332 | `		if( n < 1 ){` |
|       - |  3333 | `			/* php's copy() is FALSE when the destination would not take the` |
|       - |  3334 | `			 * bytes -- it answered TRUE here whatever the write did, so a copy` |
|       - |  3335 | `			 * onto a full filesystem reported success. The device's own notice` |
|       - |  3336 | `			 * goes with it, for the device that raises one. */` |
|       3 |  3337 | `			bFailed = 1;` |
|       3 |  3338 | `			iErr = errno;` |
|       3 |  3339 | `			break;` |
|       - |  3340 | `		}` |
|       2 |  3341 | `	}` |
|       - |  3342 | `	/* Close the streams */` |
|      18 |  3343 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      18 |  3344 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      18 |  3345 | `	if( bFailed ){` |
|       3 |  3346 | `		StreamReportRawWriteFailure(pCtx,pSout,nAsked,iErr);` |
|       1 |  3347 | `	}` |
|      18 |  3348 | `	ph7_result_bool(pCtx,!bFailed);` |
|       - |  3349 | `	}` |
|      18 |  3350 | `	return PH7_OK;` |
|      27 |  3351 | `}` |
|       - |  3352 | `/*` |
|       - |  3353 | ` * array fstat(resource $handle)` |
|       - |  3354 | ` *  Gets information about a file using an open file pointer.` |
|       - |  3355 | ` * Parameters` |
|       - |  3356 | ` *  $handle` |
|       - |  3357 | ` *   The file pointer.` |
|       - |  3358 | ` * Return` |
|       - |  3359 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|       - |  3360 | ` */` |
|      48 |  3361 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  3362 | `{` |
|       - |  3363 | `	ph7_value *pArray,*pValue;` |
|       - |  3364 | `	const ph7_io_stream *pStream;` |
|       - |  3365 | `	io_private *pDev;` |
|       - |  3366 | `	int rc;` |
|      52 |  3367 | `	if( nArg < 1 ){` |
|       - |  3368 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3369 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  3370 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3371 | `		return PH7_OK;` |
|       - |  3372 | `	}` |
|       - |  3373 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      52 |  3374 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      52 |  3375 | `	if( pDev == 0 ){` |
|       3 |  3376 | `		return rc;` |
|       - |  3377 | `	}` |
|       - |  3378 | `	/* A php://filter handle is the stream underneath it, and that is the one` |
|       - |  3379 | `	 * with a stat to answer. */` |
|      49 |  3380 | `	pDev = PH7_StreamUnwrap(pDev);` |
|       - |  3381 | `	/* Point to the target IO stream device */` |
|      49 |  3382 | `	pStream = pDev->pStream;` |
|      49 |  3383 | `	if( pDev->bDir \|\| pStream == 0 \|\| pStream->xStat == 0 ){` |
|       - |  3384 | `		/* php's fstat() on a stream with no stat -- a directory handle, an` |
|       - |  3385 | `		 * http:// body, php://output -- is FALSE and no diagnostic. */` |
|       3 |  3386 | `		ph7_result_bool(pCtx,0);` |
|       3 |  3387 | `		return PH7_OK;` |
|       - |  3388 | `	}` |
|       - |  3389 | `	/* Create the array and the working value */` |
|      47 |  3390 | `	pArray = ph7_context_new_array(pCtx);` |
|      47 |  3391 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      47 |  3392 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 |  3393 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|     ! 0 |  3394 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3395 | `		return PH7_OK;` |
|       - |  3396 | `	}` |
|       - |  3397 | `	/* Perform the requested operation */` |
|      47 |  3398 | `	if( pStream->xStat(pDev->pHandle,pArray,pValue) != PH7_OK ){` |
|       - |  3399 | `		/* php's fstat() is FALSE when the device could not answer, and says` |
|       - |  3400 | `		 * nothing about it -- php://output has no descriptor to stat. */` |
|       8 |  3401 | `		ph7_result_bool(pCtx,0);` |
|       8 |  3402 | `		return PH7_OK;` |
|       - |  3403 | `	}` |
|       - |  3404 | `	/* php answers the same thirteen fields twice -- numeric 0..12, then named` |
|       - |  3405 | `	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and` |
|       - |  3406 | `	 * had the same missing half. */` |
|       - |  3407 | `	{` |
|      40 |  3408 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      40 |  3409 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      36 |  3410 | `			ph7_result_value(pCtx,pFull);` |
|      36 |  3411 | `			return PH7_OK;` |
|       - |  3412 | `		}` |
|       - |  3413 | `	}` |
|       - |  3414 | `	/* Return the freshly created array */` |
|       5 |  3415 | `	ph7_result_value(pCtx,pArray);` |
|       - |  3416 | `	/* Don't worry about freeing memory here,everything will be` |
|       - |  3417 | `	 * released automatically as soon we return from this function.` |
|       - |  3418 | `	 */` |
|       5 |  3419 | `	return PH7_OK;` |
|      28 |  3420 | `}` |
|       - |  3421 | `/*` |
|       - |  3422 | ` * php's socket ops report a failed send THEMSELVES, as an E_NOTICE naming the` |
|       - |  3423 | ` * count, the errno and its text, before the caller ever sees the false — so a` |
|       - |  3424 | ` * write to a peer that has gone is diagnosed rather than silent. Defined with` |
|       - |  3425 | ` * the socket device further down; the write paths that can reach a socket call` |
|       - |  3426 | ` * it where php's own do.` |
|       - |  3427 | ` */` |
|       - |  3428 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen);` |
|       - |  3429 | `/*` |
|       - |  3430 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|       - |  3431 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|       - |  3432 | ` * Parameters` |
|       - |  3433 | ` *  $handle` |
|       - |  3434 | ` *   The file pointer.` |
|       - |  3435 | ` *  $string` |
|       - |  3436 | ` *   The string that is to be written.` |
|       - |  3437 | ` *  $length` |
|       - |  3438 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|       - |  3439 | ` *   or the end of string is reached, whichever comes first.` |
|       - |  3440 | ` * Return` |
|       - |  3441 | ` *  Returns the number of bytes written, or FALSE on error.` |
|       - |  3442 | ` */` |
|     595 |  3443 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  3444 | `{` |
|       - |  3445 | `	const char *zString;` |
|       - |  3446 | `	io_private *pDev;` |
|       - |  3447 | `	int nLen,n;` |
|       - |  3448 | `	int rc;` |
|     600 |  3449 | `	if( nArg < 2 ){` |
|       - |  3450 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3451 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  3452 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3453 | `		return PH7_OK;` |
|       - |  3454 | `	}` |
|       - |  3455 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     600 |  3456 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     600 |  3457 | `	if( pDev == 0 ){` |
|       7 |  3458 | `		return rc;` |
|       - |  3459 | `	}` |
|       - |  3460 | `	/* Point to the target IO stream device */` |
|     594 |  3461 | `	if( StreamRefuseUnwritable(pCtx,pDev) ){` |
|       5 |  3462 | `		ph7_result_bool(pCtx,0);` |
|       5 |  3463 | `		return PH7_OK;` |
|       - |  3464 | `	}` |
|       - |  3465 | `	/* Extract the data to write */` |
|     590 |  3466 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     590 |  3467 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       - |  3468 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|       - |  3469 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|       - |  3470 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|       - |  3471 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|       - |  3472 | `		 * and write the whole string). */` |
|      20 |  3473 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|      20 |  3474 | `		if( nMax < 0 ){` |
|       5 |  3475 | `			nLen = 0;` |
|      18 |  3476 | `		}else if( nMax < (sxi64)nLen ){` |
|      10 |  3477 | `			nLen = (int)nMax;` |
|       4 |  3478 | `		}` |
|       9 |  3479 | `	}` |
|     590 |  3480 | `	if( nLen < 1 ){` |
|       - |  3481 | `		/* Nothing to write */` |
|      12 |  3482 | `		ph7_result_int(pCtx,0);` |
|      12 |  3483 | `		return PH7_OK;` |
|       - |  3484 | `	}` |
|       - |  3485 | `	/* The device sits PAST what the readers pulled ahead: php writes at the` |
|       - |  3486 | `	 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|       - |  3487 | `	 * unread) — the ftell()/SEEK_CUR rule, applied to the write. */` |
|     580 |  3488 | `	StreamSeekBackForWrite(pDev);` |
|       - |  3489 | `	/* Perform the requested operation */` |
|     580 |  3490 | `	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);` |
|     580 |  3491 | `	if( n <  0 ){` |
|       - |  3492 | `		/* IO error,return FALSE */` |
|      60 |  3493 | `		SockReportWriteFailure(pCtx,pDev,nLen);` |
|      60 |  3494 | `		ph7_result_bool(pCtx,0);` |
|      32 |  3495 | `	}else{` |
|       - |  3496 | `		/* #Bytes written */` |
|     524 |  3497 | `		ph7_result_int(pCtx,n);` |
|       - |  3498 | `	}` |
|     580 |  3499 | `	return PH7_OK;` |
|     279 |  3500 | `}` |
|       - |  3501 | `/*` |
|       - |  3502 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|       - |  3503 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|       - |  3504 | ` * script can tell contention (1) from a plain failure (0).` |
|       - |  3505 | ` */` |
|      38 |  3506 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|       1 |  3507 | `{` |
|       - |  3508 | `	ph7_value sVal;` |
|      39 |  3509 | `	if( nArg < 3 ){` |
|      25 |  3510 | `		return;` |
|       - |  3511 | `	}` |
|      15 |  3512 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|      15 |  3513 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|      15 |  3514 | `	PH7_MemObjRelease(&sVal);` |
|      20 |  3515 | `}` |
|       - |  3516 | `/*` |
|       - |  3517 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|       - |  3518 | ` *  Portable advisory file locking.` |
|       - |  3519 | ` * Parameters` |
|       - |  3520 | ` *  $handle` |
|       - |  3521 | ` *   The file pointer.` |
|       - |  3522 | ` *  $operation` |
|       - |  3523 | ` *   operation is one of the following:` |
|       - |  3524 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|       - |  3525 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|       - |  3526 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|       - |  3527 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|       - |  3528 | ` *  &$would_block` |
|       - |  3529 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|       - |  3530 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|       - |  3531 | ` * Return` |
|       - |  3532 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - |  3533 | ` */` |
|      48 |  3534 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  3535 | `{` |
|       - |  3536 | `	const ph7_io_stream *pStream;` |
|       - |  3537 | `	io_private *pDev;` |
|       - |  3538 | `	int nLock;` |
|       - |  3539 | `	int rc;` |
|      50 |  3540 | `	if( nArg < 2 ){` |
|       - |  3541 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3542 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  3543 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3544 | `		return PH7_OK;` |
|       - |  3545 | `	}` |
|       - |  3546 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      50 |  3547 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      50 |  3548 | `	if( pDev == 0 ){` |
|       3 |  3549 | `		return rc;` |
|       - |  3550 | `	}` |
|       - |  3551 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|       - |  3552 | `	 * support is considered: the low two bits select the action (its bison` |
|       - |  3553 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|       - |  3554 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|      47 |  3555 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      47 |  3556 | `	if( (nLock & 3) == 0 ){` |
|       9 |  3557 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  3558 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|       - |  3559 | `	}` |
|      39 |  3560 | `	pDev = PH7_StreamUnwrap(pDev);` |
|       - |  3561 | `	/* Point to the target IO stream device */` |
|      39 |  3562 | `	pStream = pDev->pStream;` |
|      39 |  3563 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|       - |  3564 | `		/* php returns FALSE silently when the stream does not support locking` |
|       - |  3565 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|       7 |  3566 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|       7 |  3567 | `		ph7_result_bool(pCtx,0);` |
|       7 |  3568 | `		return PH7_OK;` |
|       - |  3569 | `	}` |
|       - |  3570 | `	/*` |
|       - |  3571 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|       - |  3572 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|       - |  3573 | `	 */` |
|       - |  3574 | `	{` |
|      33 |  3575 | `		int iOp = nLock & 3;` |
|      33 |  3576 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|      33 |  3577 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      13 |  3578 | `			nLock = -1;` |
|      27 |  3579 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      11 |  3580 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|       6 |  3581 | `		}else{` |
|      11 |  3582 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|       - |  3583 | `		}` |
|       - |  3584 | `	}` |
|       - |  3585 | `	/* Lock operation */` |
|      33 |  3586 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|       - |  3587 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|       - |  3588 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|      33 |  3589 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|       - |  3590 | `	/* IO result */` |
|      33 |  3591 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      33 |  3592 | `	return PH7_OK;` |
|      26 |  3593 | `}` |
|       - |  3594 | `/*` |
|       - |  3595 | ` * int fpassthru(resource $handle)` |
|       - |  3596 | ` *  Output all remaining data on a file pointer.` |
|       - |  3597 | ` * Parameters` |
|       - |  3598 | ` *  $handle` |
|       - |  3599 | ` *   The file pointer.` |
|       - |  3600 | ` * Return` |
|       - |  3601 | ` *  Total number of characters read from handle and passed through` |
|       - |  3602 | ` *  to the output on success or FALSE on failure.` |
|       - |  3603 | ` */` |
|      16 |  3604 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  3605 | `{` |
|       - |  3606 | `	io_private *pDev;` |
|       - |  3607 | `	ph7_int64 n,nRead;` |
|       - |  3608 | `	char zBuf[8192];` |
|       - |  3609 | `	int rc;` |
|      19 |  3610 | `	if( nArg < 1 ){` |
|       - |  3611 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3612 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  3613 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3614 | `		return PH7_OK;` |
|       - |  3615 | `	}` |
|       - |  3616 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|      19 |  3617 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|      19 |  3618 | `	if( pDev == 0 ){` |
|       5 |  3619 | `		return rc;` |
|       - |  3620 | `	}` |
|      14 |  3621 | `	if( !StreamHasReader(pDev) ){` |
|       - |  3622 | `		/* php's fpassthru() reports the FAILING READ's own -1 when nothing was` |
|       - |  3623 | `		 * passed through, which is what a handle with no reader gives. */` |
|       3 |  3624 | `		ph7_result_int(pCtx,-1);` |
|       3 |  3625 | `		return PH7_OK;` |
|       - |  3626 | `	}` |
|       - |  3627 | `	/* Perform the requested operation */` |
|      12 |  3628 | `	nRead = 0;` |
|      10 |  3629 | `	for(;;){` |
|      22 |  3630 | `		n = PH7_StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      22 |  3631 | `		if( n < 1 ){` |
|       - |  3632 | `			/* Error or EOF */` |
|      12 |  3633 | `			StreamReportReadFailure(pCtx,pDev);` |
|      12 |  3634 | `			if( n < 0 && nRead == 0 ){` |
|       - |  3635 | `				/* php answers the failing read's own -1 when NOTHING was passed` |
|       - |  3636 | `				 * through; a failure after some bytes reports those bytes. */` |
|       3 |  3637 | `				ph7_result_int64(pCtx,-1);` |
|       3 |  3638 | `				return PH7_OK;` |
|       - |  3639 | `			}` |
|      10 |  3640 | `			break;` |
|       - |  3641 | `		}` |
|       - |  3642 | `		/* Increment the read counter */` |
|      12 |  3643 | `		nRead += n;` |
|       - |  3644 | `		/* Output the bytes THIS read produced. Handing the running total to` |
|       - |  3645 | `		 * ph7_context_output() instead read past the end of zBuf from the second` |
|       - |  3646 | `		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:` |
|       - |  3647 | `		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */` |
|      12 |  3648 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      12 |  3649 | `		if( rc == PH7_ABORT ){` |
|       - |  3650 | `			/* Consumer callback request an operation abort */` |
|     ! 0 |  3651 | `			break;` |
|       - |  3652 | `		}` |
|       2 |  3653 | `	}` |
|       - |  3654 | `	/* Total number of bytes readen */` |
|      10 |  3655 | `	ph7_result_int64(pCtx,nRead);` |
|      10 |  3656 | `	return PH7_OK;` |
|      11 |  3657 | `}` |
|       - |  3658 | `/* CSV writer private data */` |
|       - |  3659 | `struct csv_data` |
|       - |  3660 | `{` |
|       - |  3661 | `	int delimiter;     /* Delimiter. Default ',' */` |
|       - |  3662 | `	int enclosure;     /* Enclosure. Default '"' */` |
|       - |  3663 | `	int escape;        /* Escape, or PH7_CSV_NO_ESCAPE when "" disabled it */` |
|       - |  3664 | `	SyBlob *pLine;     /* The line being built */` |
|       - |  3665 | `	sxu32 nCount;      /* Fields still to write after this one */` |
|       - |  3666 | `	ph7_context *pCtx; /* Call context (the field cast's diagnostics) */` |
|       - |  3667 | `	ph7_class *pOwed;  /* First field class that could not be coerced */` |
|       - |  3668 | `};` |
|       - |  3669 | `/*` |
|       - |  3670 | ` * The following callback is used by fputcsv() to walk the $fields array and` |
|       - |  3671 | ` * append each entry to the line under construction. It is a port of php's own` |
|       - |  3672 | ` * php_fputcsv (ext/standard/file.c), and the parts a re-derivation gets wrong` |
|       - |  3673 | ` * are all here:` |
|       - |  3674 | ` *  - WHICH fields are enclosed. php quotes a field containing the delimiter,` |
|       - |  3675 | ` *    the enclosure, the escape (when one is enabled) or any of \n, \r, \t and` |
|       - |  3676 | ` *    SPACE. PH7 tested the first two only, so a field with an embedded newline` |
|       - |  3677 | ` *    was written raw and became two CSV ROWS on the way back in.` |
|       - |  3678 | ` *  - HOW an embedded enclosure is written: doubled, unless the escape character` |
|       - |  3679 | ` *    came immediately before it (then the pair is passed through as-is and the` |
|       - |  3680 | ` *    escape does NOT arm again for the byte after).` |
|       - |  3681 | ` *  - that an EMPTY field is still a field. PH7 returned early for a zero-length` |
|       - |  3682 | `` *    value and skipped its delimiter with it, so `['', 'a']` wrote "a" -- one`` |
|       - |  3683 | ` *    column where the caller wrote two, silently shifting every later column.` |
|       - |  3684 | ` * The delimiter goes BETWEEN fields, so it is written from the remaining count` |
|       - |  3685 | ` * rather than from a "not the first" flag: php appends it after every field but` |
|       - |  3686 | ` * the last, and an empty first field must still be followed by one.` |
|       - |  3687 | ` */` |
|     140 |  3688 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       1 |  3689 | `{` |
|     141 |  3690 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|       - |  3691 | `	const char *zData;` |
|       - |  3692 | `	int nLen,i;` |
|     141 |  3693 | `	int bEnclose = 0;` |
|      70 |  3694 | `	SXUNUSED(pKey); /* cc warning */` |
|       - |  3695 | `	/* php casts each field USER-VISIBLY: a field that is itself an array warns` |
|       - |  3696 | ``	 * `Array to string conversion` and is written as "Array", and one that is an`` |
|       - |  3697 | `	 * object with no __toString() is php's catchable Error -- where PHL wrote the` |
|       - |  3698 | `	 * placeholder "Object" into the file, in silence. */` |
|       - |  3699 | `	{` |
|     141 |  3700 | `		ph7_class *pBad = PH7_ValueToStringUVDefer(pData->pCtx,pValue,&zData,&nLen);` |
|     141 |  3701 | `		if( pBad && pData->pOwed == 0 ){` |
|       3 |  3702 | `			pData->pOwed = pBad;` |
|       1 |  3703 | `		}` |
|       - |  3704 | `	}` |
|     275 |  3705 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     183 |  3706 | `		int c = (unsigned char)zData[i];` |
|     182 |  3707 | `		if( c == pData->delimiter \|\| c == pData->enclosure` |
|     163 |  3708 | `		 \|\| (pData->escape != PH7_CSV_NO_ESCAPE && c == pData->escape)` |
|     153 |  3709 | `		 \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ' ' ){` |
|      49 |  3710 | `			bEnclose = 1;` |
|      49 |  3711 | `			break;` |
|       - |  3712 | `		}` |
|      68 |  3713 | `	}` |
|     141 |  3714 | `	if( bEnclose ){` |
|      49 |  3715 | `		char cEnc = (char)pData->enclosure;` |
|      49 |  3716 | `		int bEscaped = 0;` |
|      49 |  3717 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     197 |  3718 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     149 |  3719 | `			char c = zData[i];` |
|     149 |  3720 | `			if( pData->escape != PH7_CSV_NO_ESCAPE && (unsigned char)c == pData->escape ){` |
|       9 |  3721 | `				bEscaped = 1;` |
|     145 |  3722 | `			}else if( !bEscaped && (unsigned char)c == pData->enclosure ){` |
|      21 |  3723 | `				SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|      11 |  3724 | `			}else{` |
|     121 |  3725 | `				bEscaped = 0;` |
|       - |  3726 | `			}` |
|     149 |  3727 | `			SyBlobAppend(pData->pLine,(const void *)&c,sizeof(char));` |
|      75 |  3728 | `		}` |
|      49 |  3729 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     117 |  3730 | `	}else if( nLen > 0 ){` |
|      71 |  3731 | `		SyBlobAppend(pData->pLine,(const void *)zData,(sxu32)nLen);` |
|      35 |  3732 | `	}` |
|     141 |  3733 | `	if( pData->nCount > 0 ){` |
|     141 |  3734 | `		pData->nCount--;` |
|      70 |  3735 | `	}` |
|     141 |  3736 | `	if( pData->nCount > 0 ){` |
|      55 |  3737 | `		char cDel = (char)pData->delimiter;` |
|      55 |  3738 | `		SyBlobAppend(pData->pLine,(const void *)&cDel,sizeof(char));` |
|      27 |  3739 | `	}` |
|     141 |  3740 | `	return PH7_OK;` |
|       1 |  3741 | `}` |
|       - |  3742 | `/*` |
|       - |  3743 | ` * int\|false fputcsv(resource $stream, array $fields, string $separator = ',',` |
|       - |  3744 | ` *                   string $enclosure = '"', string $escape = '\\',` |
|       - |  3745 | ` *                   string $eol = "\n")` |
|       - |  3746 | ` *  Format line as CSV and write to file pointer.` |
|       - |  3747 | ` * Parameters` |
|       - |  3748 | ` *  $stream` |
|       - |  3749 | ` *   Open file handle.` |
|       - |  3750 | ` *  $fields` |
|       - |  3751 | ` *   An array of values.` |
|       - |  3752 | ` *  $separator` |
|       - |  3753 | ` *   The optional separator parameter sets the field delimiter (one character only).` |
|       - |  3754 | ` *  $enclosure` |
|       - |  3755 | ` *   The optional enclosure parameter sets the field enclosure (one character only).` |
|       - |  3756 | ` *  $escape` |
|       - |  3757 | ` *   The escape character (one character), or "" to disable escaping entirely.` |
|       - |  3758 | ` *  $eol` |
|       - |  3759 | ` *   php 8.1's line ending. It is "\n" on EVERY platform -- php does not follow` |
|       - |  3760 | ` *   the host's convention here, and PHL used to write CRLF on Windows, so the` |
|       - |  3761 | ` *   same program produced a different FILE depending on where it ran.` |
|       - |  3762 | ` * Return` |
|       - |  3763 | ` *  The number of bytes written, or FALSE when the write fails. The count was` |
|       - |  3764 | ` *  missing entirely (the call answered NULL), so the documented` |
|       - |  3765 | `` *  `if (fputcsv(...) === false)` check never fired and a caller totalling the`` |
|       - |  3766 | ` *  bytes it wrote added nothing.` |
|       - |  3767 | ` */` |
|     102 |  3768 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  3769 | `{` |
|       - |  3770 | `	struct csv_data sCsv;` |
|       - |  3771 | `	io_private *pDev;` |
|       - |  3772 | `	SyBlob sLine;` |
|     104 |  3773 | `	const char *zEol = "\n";` |
|     104 |  3774 | `	int nEol = 1;` |
|       - |  3775 | `	ph7_int64 nWr;` |
|       - |  3776 | ``	int rcArg;   /* the handle screen's; the CSV screens below shadow `rc` */`` |
|     104 |  3777 | `	if( nArg < 2 ){` |
|       - |  3778 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3779 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|     ! 0 |  3780 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3781 | `		return PH7_OK;` |
|       - |  3782 | `	}` |
|       - |  3783 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     104 |  3784 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcArg);` |
|     104 |  3785 | `	if( pDev == 0 ){` |
|       3 |  3786 | `		return rcArg;` |
|       - |  3787 | `	}` |
|     101 |  3788 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  3789 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  3790 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|     ! 0 |  3791 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3792 | `		return PH7_OK;` |
|       - |  3793 | `	}` |
|       - |  3794 | `	/* Point to the target IO stream device */` |
|     101 |  3795 | `	if( StreamRefuseUnwritable(pCtx,pDev) ){` |
|     ! 0 |  3796 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  3797 | `		return PH7_OK;` |
|       - |  3798 | `	}` |
|       - |  3799 | `	/* Set default csv separator */` |
|     101 |  3800 | `	sCsv.delimiter = ',';` |
|     101 |  3801 | `	sCsv.enclosure = '"';` |
|     101 |  3802 | `	sCsv.escape = '\\';` |
|     101 |  3803 | `	if( nArg > 2 ){` |
|     101 |  3804 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     101 |  3805 | `		if( rc != PH7_OK ){` |
|       5 |  3806 | `			return rc;` |
|       - |  3807 | `		}` |
|      97 |  3808 | `		if( nArg > 3 ){` |
|      97 |  3809 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|      97 |  3810 | `			if( rc != PH7_OK ){` |
|       5 |  3811 | `				return rc;` |
|       - |  3812 | `			}` |
|      93 |  3813 | `			if( nArg > 4 ){` |
|      93 |  3814 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&sCsv.escape);` |
|      93 |  3815 | `				if( rc != PH7_OK ){` |
|       5 |  3816 | `					return rc;` |
|       - |  3817 | `				}` |
|      89 |  3818 | `				if( nArg > 5 ){` |
|       - |  3819 | `					/* $eol takes ANY string, the empty one included -- it is not` |
|       - |  3820 | `					 * a single-character argument like the three above. */` |
|      75 |  3821 | `					zEol = ph7_value_to_string(apArg[5],&nEol);` |
|      37 |  3822 | `				}` |
|      44 |  3823 | `			}` |
|      44 |  3824 | `		}` |
|      44 |  3825 | `	}` |
|       - |  3826 | `	/* php builds the whole line first and writes it ONCE, which is what makes the` |
|       - |  3827 | `	 * byte count meaningful and keeps a partly-written row off the stream. */` |
|      89 |  3828 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|      89 |  3829 | `	sCsv.pLine = &sLine;` |
|      89 |  3830 | `	sCsv.nCount = (sxu32)ph7_array_count(apArg[1]);` |
|      89 |  3831 | `	sCsv.pCtx = pCtx;` |
|      89 |  3832 | `	sCsv.pOwed = 0;` |
|      89 |  3833 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      89 |  3834 | `	if( nEol > 0 ){` |
|      87 |  3835 | `		SyBlobAppend(&sLine,(const void *)zEol,(sxu32)nEol);` |
|      43 |  3836 | `	}` |
|       - |  3837 | `	/* Write at the LOGICAL position, not the device one -- the same rule` |
|       - |  3838 | `	 * PH7_builtin_fwrite applies after a buffered read (fgets() then` |
|       - |  3839 | `	 * fputcsv() overwrites what fgets left unread). */` |
|      89 |  3840 | `	StreamSeekBackForWrite(pDev);` |
|     133 |  3841 | `	nWr = PH7_StreamWrite(pDev,(const void *)SyBlobData(&sLine),` |
|      88 |  3842 | `		(ph7_int64)SyBlobLength(&sLine));` |
|      89 |  3843 | `	if( nWr < 0 ){` |
|       3 |  3844 | `		SockReportWriteFailure(pCtx,pDev,(int)SyBlobLength(&sLine));` |
|       1 |  3845 | `	}` |
|      89 |  3846 | `	SyBlobRelease(&sLine);` |
|      89 |  3847 | `	if( nWr < 0 ){` |
|       3 |  3848 | `		ph7_result_bool(pCtx,0);` |
|       2 |  3849 | `	}else{` |
|      87 |  3850 | `		ph7_result_int64(pCtx,nWr);` |
|       - |  3851 | `	}` |
|      89 |  3852 | `	if( sCsv.pOwed ){` |
|       - |  3853 | `		/* php writes the line first and reports the field it could not stringify` |
|       - |  3854 | `		 * afterwards; raising it during the build would put the catch's own` |
|       - |  3855 | `		 * output in front of the row. */` |
|       4 |  3856 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - |  3857 | `			"Object of class %.*s could not be converted to string",` |
|       2 |  3858 | `			(int)sCsv.pOwed->sDisp.nByte,sCsv.pOwed->sDisp.zString);` |
|       - |  3859 | `	}` |
|      87 |  3860 | `	return PH7_OK;` |
|      53 |  3861 | `}` |
|       - |  3862 | `/*` |
|       - |  3863 | ` * fprintf,vfprintf private data.` |
|       - |  3864 | ` * An instance of the following structure is passed to the formatted` |
|       - |  3865 | ` * input consumer callback defined below.` |
|       - |  3866 | ` */` |
|       - |  3867 | `typedef struct fprintf_data fprintf_data;` |
|       - |  3868 | `struct fprintf_data` |
|       - |  3869 | `{` |
|       - |  3870 | `	io_private *pIO;        /* IO stream */` |
|       - |  3871 | `	ph7_int64 nCount;       /* Total bytes FORMATTED (php's answer, not the bytes` |
|       - |  3872 | `	                         * the device took: php builds the whole string, writes` |
|       - |  3873 | `	                         * it once and returns its length whatever the write` |
|       - |  3874 | `	                         * did) */` |
|       - |  3875 | `	int bNoWriter;          /* the stream has no writer AT ALL: announced once,` |
|       - |  3876 | `	                         * before the format runs, and then every chunk is` |
|       - |  3877 | `	                         * counted and dropped rather than offered */` |
|       - |  3878 | `	int bIoErr;             /* the device refused, so stop feeding it -- but this` |
|       - |  3879 | `	                         * is an IO failure and not a mid-format THROW, and the` |
|       - |  3880 | `	                         * caller must not confuse the two */` |
|       - |  3881 | `};` |
|       - |  3882 | `/*` |
|       - |  3883 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|       - |  3884 | ` */` |
|      70 |  3885 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       3 |  3886 | `{` |
|      73 |  3887 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|       - |  3888 | `	ph7_int64 n;` |
|      73 |  3889 | `	if( pFdata->bNoWriter ){` |
|       - |  3890 | `		/* Nowhere to put it, and php builds the string anyway: its fprintf()` |
|       - |  3891 | `		 * formats first and writes once, so the LENGTH it answers is the same` |
|       - |  3892 | `		 * whether the stream took the bytes or refused them all. Counting` |
|       - |  3893 | ``		 * without writing is what keeps `fprintf($dir, "a%sb", "q")` at 3`` |
|       - |  3894 | `		 * rather than the first chunk's 1. */` |
|      19 |  3895 | `		pFdata->nCount += nLen;` |
|      19 |  3896 | `		return PH7_OK;` |
|       - |  3897 | `	}` |
|       - |  3898 | `	/* Write the formatted data */` |
|      55 |  3899 | `	n = PH7_StreamWrite(pFdata->pIO,(const void *)zInput,nLen);` |
|      55 |  3900 | `	pFdata->nCount += nLen;` |
|      55 |  3901 | `	if( n < 0 ){` |
|       3 |  3902 | `		SockReportWriteFailure(pCtx,pFdata->pIO,nLen);` |
|       - |  3903 | `		/* Nothing more can reach the device; stop, and let the caller answer.` |
|       - |  3904 | `		 * Propagating this as a THROW status aborted the whole script -- a` |
|       - |  3905 | `		 * failed fprintf() ended the program where php returns a number. */` |
|       3 |  3906 | `		pFdata->bIoErr = 1;` |
|       3 |  3907 | `		return SXERR_ABORT;` |
|       - |  3908 | `	}` |
|      53 |  3909 | `	return PH7_OK;` |
|      38 |  3910 | `}` |
|       - |  3911 | `/*` |
|       - |  3912 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|       - |  3913 | ` *  Write a formatted string to a stream.` |
|       - |  3914 | ` * Parameters` |
|       - |  3915 | ` *  $handle` |
|       - |  3916 | ` *   The file pointer.` |
|       - |  3917 | ` *  $format` |
|       - |  3918 | ` *   String format (see sprintf()).` |
|       - |  3919 | ` * Return` |
|       - |  3920 | ` *  The length of the written string.` |
|       - |  3921 | ` */` |
|      36 |  3922 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  3923 | `{` |
|       - |  3924 | `	fprintf_data sFdata;` |
|       - |  3925 | `	const char *zFormat;` |
|       - |  3926 | `	io_private *pDev;` |
|       - |  3927 | `	int nLen;` |
|      40 |  3928 | `	if( nArg < 2 ){` |
|     ! 0 |  3929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|     ! 0 |  3930 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  3931 | `		return PH7_OK;` |
|       - |  3932 | `	}` |
|       - |  3933 | `	{` |
|       - |  3934 | `		/* php: the $stream argument is refused BEFORE $format and $values -- a` |
|       - |  3935 | `		 * non-resource names the type it got, and one whose device is gone is` |
|       - |  3936 | `		 * "must be an open stream resource". Both are TypeErrors, not the` |
|       - |  3937 | `		 * warn-and-return-0 this door used to give. */` |
|       - |  3938 | `		int rcs;` |
|      40 |  3939 | `		pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcs);` |
|      40 |  3940 | `		if( pDev == 0 ){` |
|       5 |  3941 | `			return rcs;` |
|       - |  3942 | `		}` |
|       - |  3943 | `	}` |
|       - |  3944 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|       - |  3945 | `	{` |
|      35 |  3946 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      35 |  3947 | `		if( rcf != PH7_OK ){` |
|     ! 0 |  3948 | `			return rcf;` |
|       - |  3949 | `		}` |
|       - |  3950 | `	}` |
|       - |  3951 | `	/* Extract the string format (scalars/null coerce). */` |
|      35 |  3952 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      35 |  3953 | `	if( nLen < 1 ){` |
|       - |  3954 | `		/* Empty string,return zero */` |
|       3 |  3955 | `		ph7_result_int(pCtx,0);` |
|       3 |  3956 | `		return PH7_OK;` |
|       - |  3957 | `	}` |
|       - |  3958 | `	{` |
|       - |  3959 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|       - |  3960 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|       - |  3961 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|      33 |  3962 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|      33 |  3963 | `		if( rcv != PH7_OK ){` |
|       3 |  3964 | `			return rcv;` |
|       - |  3965 | `		}` |
|       - |  3966 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|       - |  3967 | `		 * before any output; propagate the throw status verbatim. */` |
|      31 |  3968 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      31 |  3969 | `		if( rcv != PH7_OK ){` |
|       7 |  3970 | `			return rcv;` |
|       - |  3971 | `		}` |
|       - |  3972 | `	}` |
|       - |  3973 | `	/* Prepare our private data */` |
|      25 |  3974 | `	sFdata.nCount = 0;` |
|      25 |  3975 | `	sFdata.pIO = pDev;` |
|      25 |  3976 | `	sFdata.bIoErr = 0;` |
|       - |  3977 | `	/* A handle with no writer at all is announced HERE -- after every argument` |
|       - |  3978 | `	 * screen, because php's TypeError/ValueError/ArgumentCountError come first` |
|       - |  3979 | `	 * and carry no notice with them, and after the empty format, which writes` |
|       - |  3980 | `	 * nothing and says nothing. */` |
|      25 |  3981 | `	sFdata.bNoWriter = StreamRefuseUnwritable(pCtx,pDev);` |
|       - |  3982 | `	/* Format the string */` |
|       - |  3983 | `	{` |
|      25 |  3984 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|       - |  3985 | `	/* Return total number of bytes written */` |
|      25 |  3986 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|       - |  3987 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|       - |  3988 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|       - |  3989 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|      25 |  3990 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|       3 |  3991 | `		pCtx->nThrowRc = rcv;` |
|       3 |  3992 | `		return rcv;` |
|       - |  3993 | `	}` |
|       - |  3994 | `	}` |
|      22 |  3995 | `	return PH7_OK;` |
|      22 |  3996 | `}` |
|       - |  3997 | `/*` |
|       - |  3998 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|       - |  3999 | ` *  Write a formatted string to a stream.` |
|       - |  4000 | ` * Parameters` |
|       - |  4001 | ` *  $handle` |
|       - |  4002 | ` *   The file pointer.` |
|       - |  4003 | ` *  $format` |
|       - |  4004 | ` *   String format (see sprintf()).` |
|       - |  4005 | ` * $args` |
|       - |  4006 | ` *   User arguments.` |
|       - |  4007 | ` * Return` |
|       - |  4008 | ` *  The length of the written string.` |
|       - |  4009 | ` */` |
|      20 |  4010 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  4011 | `{` |
|       - |  4012 | `	fprintf_data sFdata;` |
|       - |  4013 | `	const char *zFormat;` |
|       - |  4014 | `	ph7_hashmap *pMap;` |
|       - |  4015 | `	io_private *pDev;` |
|       - |  4016 | `	SySet sArg;` |
|       - |  4017 | `	int n,nLen;` |
|      24 |  4018 | `	if( nArg < 3 ){` |
|     ! 0 |  4019 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|     ! 0 |  4020 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  4021 | `		return PH7_OK;` |
|       - |  4022 | `	}` |
|       - |  4023 | `	{` |
|       - |  4024 | `		/* php: the $stream argument is refused BEFORE $format and $values -- a` |
|       - |  4025 | `		 * non-resource names the type it got, and one whose device is gone is` |
|       - |  4026 | `		 * "must be an open stream resource". Both are TypeErrors, not the` |
|       - |  4027 | `		 * warn-and-return-0 this door used to give. */` |
|       - |  4028 | `		int rcs;` |
|      24 |  4029 | `		pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rcs);` |
|      24 |  4030 | `		if( pDev == 0 ){` |
|      10 |  4031 | `			return rcs;` |
|       - |  4032 | `		}` |
|       - |  4033 | `	}` |
|       - |  4034 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|       - |  4035 | `	{` |
|      16 |  4036 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      16 |  4037 | `		if( rcf != PH7_OK ){` |
|     ! 0 |  4038 | `			return rcf;` |
|       - |  4039 | `		}` |
|       - |  4040 | `	}` |
|      16 |  4041 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|       - |  4042 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - |  4043 | `		char zBuf[64];` |
|       4 |  4044 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  4045 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|       2 |  4046 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|       - |  4047 | `	}` |
|       - |  4048 | `	/* Extract the string format */` |
|      13 |  4049 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      13 |  4050 | `	if( nLen < 1 ){` |
|       - |  4051 | `		/* Empty string,return zero */` |
|     ! 0 |  4052 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  4053 | `		return PH7_OK;` |
|       - |  4054 | `	}` |
|       - |  4055 | `	/* Point to hashmap */` |
|      13 |  4056 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|       - |  4057 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - |  4058 | `	 * php runs this BEFORE validating the specifiers. */` |
|       - |  4059 | `	{` |
|      13 |  4060 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      13 |  4061 | `		if( rcc != PH7_OK ){` |
|       3 |  4062 | `			return rcc;` |
|       - |  4063 | `		}` |
|       - |  4064 | `	}` |
|       - |  4065 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - |  4066 | `	 * any output; propagate the throw status verbatim. */` |
|       - |  4067 | `	{` |
|      11 |  4068 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      11 |  4069 | `		if( rcv != PH7_OK ){` |
|     ! 0 |  4070 | `			return rcv;` |
|       - |  4071 | `		}` |
|       - |  4072 | `	}` |
|       - |  4073 | `	/* Extract arguments from the hashmap */` |
|      11 |  4074 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - |  4075 | `	/* Prepare our private data */` |
|      11 |  4076 | `	sFdata.nCount = 0;` |
|      11 |  4077 | `	sFdata.pIO = pDev;` |
|      11 |  4078 | `	sFdata.bIoErr = 0;` |
|       - |  4079 | `	/* A handle with no writer at all is announced HERE -- after every argument` |
|       - |  4080 | `	 * screen, because php's TypeError/ValueError/ArgumentCountError come first` |
|       - |  4081 | `	 * and carry no notice with them, and after the empty format, which writes` |
|       - |  4082 | `	 * nothing and says nothing. */` |
|      11 |  4083 | `	sFdata.bNoWriter = StreamRefuseUnwritable(pCtx,pDev);` |
|       - |  4084 | `	/* Format the string */` |
|       - |  4085 | `	{` |
|      11 |  4086 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|       - |  4087 | `	/* Return total number of bytes written*/` |
|      11 |  4088 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      11 |  4089 | `	SySetRelease(&sArg);` |
|       - |  4090 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|       - |  4091 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|       - |  4092 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|      11 |  4093 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|       3 |  4094 | `		pCtx->nThrowRc = rcv;` |
|       3 |  4095 | `		return rcv;` |
|       - |  4096 | `	}` |
|       - |  4097 | `	}` |
|       8 |  4098 | `	return PH7_OK;` |
|      14 |  4099 | `}` |
|       - |  4100 | `/*` |
|       - |  4101 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|       - |  4102 | ` * According to the PHP reference manual:` |
|       - |  4103 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|       - |  4104 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|       - |  4105 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|       - |  4106 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|       - |  4107 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|       - |  4108 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|       - |  4109 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|       - |  4110 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|       - |  4111 | ` *         exist, attempt to create it.` |
|       - |  4112 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|       - |  4113 | ` *          not exist, attempt to create it.` |
|       - |  4114 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|       - |  4115 | ` *         already exists,` |
|       - |  4116 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|       - |  4117 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|       - |  4118 | ` *         the underlying open(2) system call.` |
|       - |  4119 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|       - |  4120 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|       - |  4121 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|       - |  4122 | ` *          is positioned on the beginning of the file.` |
|       - |  4123 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|       - |  4124 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|       - |  4125 | ` *          be used after the lock is requested).` |
|       - |  4126 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|       - |  4127 | ` */` |
|       - |  4128 | `/*` |
|       - |  4129 | ` * php's php_stream_parse_fopen_modes, which is narrower than the list above` |
|       - |  4130 | `` * reads: only the FIRST character decides, it must be one of `r w a x c` in`` |
|       - |  4131 | `` * LOWER case, and everything after it is SCANNED -- `+` anywhere makes the`` |
|       - |  4132 | `` * open read-write, `b`/`t` anywhere pick the translation mode, and any other`` |
|       - |  4133 | ` * byte is ignored. Anything else is refused OUTRIGHT, which is what the -1` |
|       - |  4134 | ` * answer is for; the empty mode is one of them.` |
|       - |  4135 | ` *` |
|       - |  4136 | ` * The chunk read only the first TWO characters and had its own idea of both` |
|       - |  4137 | `` * halves, so six ordinary spellings opened the wrong way in silence: `rb+`,`` |
|       - |  4138 | `` * `ab+` and `cb+` -- the `+` is not in position two -- were opened read-only`` |
|       - |  4139 | ``  * or write-only, `rw` was READ-WRITE where php gives read-only, `wr` `` |
|       - |  4140 | ` * likewise, and an unknown or upper-case mode was accepted with a PH7-ism` |
|       - |  4141 | ` * notice and a read-only open where php refuses the call.` |
|       - |  4142 | ` */` |
|    1982 |  4143 | `static int StrModeToFlags(const char *zMode,int nLen,int *piFlags)` |
|       5 |  4144 | `{` |
|       - |  4145 | `	int iFlag,i;` |
|    1987 |  4146 | `	int bPlus = 0,bBin = 0,bText = 0;` |
|    1987 |  4147 | `	if( nLen < 1 ){` |
|       3 |  4148 | `		return -1;` |
|       - |  4149 | `	}` |
|    1985 |  4150 | `	switch( zMode[0] ){` |
|     476 |  4151 | `		case 'r':` |
|       - |  4152 | `			/* Read-only access */` |
|     899 |  4153 | `			iFlag = PH7_IO_OPEN_RDONLY;` |
|     899 |  4154 | `			break;` |
|     179 |  4155 | `		case 'w':` |
|       - |  4156 | `			/* Overwrite mode; create the file if it is not there */` |
|     362 |  4157 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     362 |  4158 | `			break;` |
|      25 |  4159 | `		case 'a':` |
|       - |  4160 | `			/* Append mode; create the file if it is not there */` |
|      53 |  4161 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|      53 |  4162 | `			break;` |
|     311 |  4163 | `		case 'x':` |
|       - |  4164 | `			/* Exclusive create: fails when the file already exists. EXCL is left` |
|       - |  4165 | `			 * to imply the creation on its own -- the device decoders test` |
|       - |  4166 | `			 * CREATE first, so setting both would drop the O_EXCL. */` |
|     626 |  4167 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|     626 |  4168 | `			break;` |
|      10 |  4169 | `		case 'c':` |
|       - |  4170 | `			/* Create if absent, and neither truncate nor fail if present */` |
|      22 |  4171 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|      22 |  4172 | `			break;` |
|      19 |  4173 | `		default:` |
|      40 |  4174 | `			return -1;` |
|       - |  4175 | `	}` |
|    2559 |  4176 | `	for( i = 1 ; i < nLen ; ++i ){` |
|     617 |  4177 | `		if( zMode[i] == '+' ){` |
|     505 |  4178 | `			bPlus = 1;` |
|     364 |  4179 | `		}else if( zMode[i] == 'b' ){` |
|      94 |  4180 | `			bBin = 1;` |
|      68 |  4181 | `		}else if( zMode[i] == 't' ){` |
|      12 |  4182 | `			bText = 1;` |
|       5 |  4183 | `		}` |
|     311 |  4184 | `	}` |
|    1947 |  4185 | `	if( bPlus ){` |
|     503 |  4186 | `		iFlag &= ~(PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_WRONLY);` |
|     503 |  4187 | `		iFlag \|= PH7_IO_OPEN_RDWR;` |
|     249 |  4188 | `	}` |
|       - |  4189 | ``	/* php's `b` wins over `t` when both are named, and binary is its default. */`` |
|    1947 |  4190 | `	if( bText && !bBin ){` |
|       8 |  4191 | `		iFlag \|= PH7_IO_OPEN_TEXT;` |
|       5 |  4192 | `	}else{` |
|    1941 |  4193 | `		iFlag \|= PH7_IO_OPEN_BINARY;` |
|       - |  4194 | `	}` |
|    1947 |  4195 | `	*piFlags = iFlag;` |
|    1947 |  4196 | `	return 0;` |
|     966 |  4197 | `}` |
|       - |  4198 | `/*` |
|       - |  4199 | ` * The same grammar, for a door that has to know whether a mode is php's before` |
|       - |  4200 | ` * deciding whose refusal to raise: gzopen() reports the sentence above for a` |
|       - |  4201 | ` * letter php does not know, and libz's own flat failure for one php takes and` |
|       - |  4202 | ` * libz has no direction for. Answers 1 when php would accept the mode.` |
|       - |  4203 | ` */` |
|       4 |  4204 | `PH7_PRIVATE int PH7_StreamModeIsValid(const char *zMode,int nLen,int *piFlags)` |
|       1 |  4205 | `{` |
|       5 |  4206 | `	return StrModeToFlags(zMode,nLen,piFlags) == 0;` |
|       1 |  4207 | `}` |
|       - |  4208 | `/*` |
|       - |  4209 | ` * Initialize the IO private structure.` |
|       - |  4210 | ` */` |
|   13131 |  4211 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|       5 |  4212 | `{` |
|   13136 |  4213 | `	pOut->iHead = IO_PRIVATE_HEAD_MAGIC;` |
|   13136 |  4214 | `	pOut->pStream = pStream;` |
|   13136 |  4215 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   13136 |  4216 | `	pOut->nOfft = 0;` |
|   13136 |  4217 | `	SyBlobInit(&pOut->sUri,&pVm->sAllocator);` |
|   13136 |  4218 | `	pOut->zMode[0] = 0;` |
|   13136 |  4219 | `	pOut->bEof = 0;` |
|   13136 |  4220 | `	pOut->iLastReadErr = 0;` |
|   13136 |  4221 | `	pOut->bDir = 0;` |
|   13136 |  4222 | `	pOut->bPersist = 0;` |
|   13136 |  4223 | `	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */` |
|   13136 |  4224 | `	pOut->bNonBlock = 0;` |
|   13136 |  4225 | `	pOut->bHasTimeout = 0;` |
|   13136 |  4226 | `	pOut->bTimedOut = 0;` |
|   13136 |  4227 | `	pOut->pReadFilters = 0;` |
|   13136 |  4228 | `	pOut->pWriteFilters = 0;` |
|   13136 |  4229 | `	pOut->bFiltDone = 0;` |
|   13136 |  4230 | `	pOut->bFiltErr = 0;` |
|   13136 |  4231 | `	pOut->iFiltPos = 0;` |
|   13136 |  4232 | `	pOut->iPos = 0;` |
|   13136 |  4233 | `	pOut->bPosSeeded = 0;` |
|   13136 |  4234 | `	pOut->pCtxRes = 0;` |
|   13136 |  4235 | `	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);` |
|   13136 |  4236 | `	pOut->nFiltOfft = 0;` |
|       - |  4237 | `	/* No ph7_value names it yet: the opener's own result value takes the first` |
|       - |  4238 | `	 * count, and a handle a NON-value holder keeps takes one of its own. */` |
|   13136 |  4239 | `	pOut->nValRef = 0;` |
|       - |  4240 | `	/* Set the magic number */` |
|   13136 |  4241 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   13136 |  4242 | `}` |
|       - |  4243 | `/*` |
|       - |  4244 | ` * The counted-resource header behind a raw resource pointer, or 0 when there is` |
|       - |  4245 | ` * none. Every resource this engine hands out opens with an io_private header,` |
|       - |  4246 | ` * so iHead is in bounds for all of them and iMagic then says WHICH kind; only` |
|       - |  4247 | ` * the kinds whose LAST holder has something to release answer here: a live` |
|       - |  4248 | ` * stream, a stream context, a stream filter -- before or after it left its` |
|       - |  4249 | ` * chain -- and the two brigade handles that live inside a filter. A closed` |
|       - |  4250 | ` * stream, a process handle and a bucket token all fall through and stay` |
|       - |  4251 | ` * uncounted.` |
|       - |  4252 | ` */` |
|       - |  4253 | `static void StreamCtxDestroy(phl_stream_ctx *pRes);` |
|  888677 |  4254 | `static io_private * ResCountedHead(void *pResource)` |
|       5 |  4255 | `{` |
|  888682 |  4256 | `	io_private *pDev = (io_private *)pResource;` |
|  888682 |  4257 | `	if( pDev == 0 \|\| pDev->iHead != IO_PRIVATE_HEAD_MAGIC ){` |
|     558 |  4258 | `		return 0;` |
|       - |  4259 | `	}` |
|  888121 |  4260 | `	if( pDev->iMagic == IO_PRIVATE_MAGIC \|\| pDev->iMagic == STREAM_CTX_MAGIC` |
|  309359 |  4261 | `	 \|\| pDev->iMagic == STREAM_FILTER_MAGIC \|\| pDev->iMagic == STREAM_FILTER_CLOSED_MAGIC` |
|   36531 |  4262 | `	 \|\| pDev->iMagic == STREAM_BRIGADE_MAGIC ){` |
|  862423 |  4263 | `		return pDev;` |
|       - |  4264 | `	}` |
|   25708 |  4265 | `	return 0;` |
|  442643 |  4266 | `}` |
|       - |  4267 | `/*` |
|       - |  4268 | ` * Take one reference on a counted resource for a ph7_value that now names it.` |
|       - |  4269 | ` * Accepts any resource pointer; anything that is not counted falls through and` |
|       - |  4270 | ` * answers 0, which is how the value learns not to carry the mark.` |
|       - |  4271 | ` */` |
|  445961 |  4272 | `PH7_PRIVATE int PH7_StreamValueRef(void *pResource)` |
|       5 |  4273 | `{` |
|  445966 |  4274 | `	io_private *pDev = ResCountedHead(pResource);` |
|  445966 |  4275 | `	if( pDev == 0 ){` |
|    1387 |  4276 | `		return 0;` |
|       - |  4277 | `	}` |
|  444583 |  4278 | `	pDev->nValRef++;` |
|  444583 |  4279 | `	return 1;` |
|  222133 |  4280 | `}` |
|       - |  4281 | `/*` |
|       - |  4282 | ` * Drop the reference a ph7_value held, and CLOSE the handle when it was the` |
|       - |  4283 | ` * last: php's stream layer closes a stream whose refcount reaches zero, so` |
|       - |  4284 | `` * `$h = fopen(...); $h = null;` releases the descriptor and a dropped`` |
|       - |  4285 | ` * stream_socket_server() frees its port. The struct itself stays alive and` |
|       - |  4286 | ` * stamped closed, so a copy that outlives the close still reads` |
|       - |  4287 | ` * "resource (closed)" rather than freed memory -- the same contract fclose()` |
|       - |  4288 | ` * has always had here.` |
|       - |  4289 | ` *` |
|       - |  4290 | ` * A PERSISTENT stream is exempt: php keeps one open past its last holder, which` |
|       - |  4291 | ` * is the whole point of pfsockopen(). So are the three standard handles and any` |
|       - |  4292 | ` * device with no close entry point.` |
|       - |  4293 | ` */` |
|  442716 |  4294 | `PH7_PRIVATE void PH7_StreamValueUnref(void *pResource)` |
|       5 |  4295 | `{` |
|  442721 |  4296 | `	io_private *pDev = ResCountedHead(pResource);` |
|  442721 |  4297 | `	if( pDev == 0 ){` |
|   24881 |  4298 | `		return;` |
|       - |  4299 | `	}` |
|  417845 |  4300 | `	if( pDev->nValRef < 1 ){` |
|       - |  4301 | `		/* Nobody counted this handle IN, so no count can say it is the last one` |
|       - |  4302 | `		 * out: a holder that writes a resource slot straight rather than through` |
|       - |  4303 | `		 * ph7_value_resource() owns its handle and closes it itself. Leaving it` |
|       - |  4304 | `		 * open is the answer this engine has always given. */` |
|     ! 0 |  4305 | `		return;` |
|       - |  4306 | `	}` |
|  417845 |  4307 | `	pDev->nValRef--;` |
|  417845 |  4308 | `	if( pDev->nValRef > 0 ){` |
|  327118 |  4309 | `		return;` |
|       - |  4310 | `	}` |
|   90732 |  4311 | `	if( pDev->iMagic == STREAM_CTX_MAGIC ){` |
|       - |  4312 | `		/* A CONTEXT has no descriptor to close: what its last holder releases` |
|       - |  4313 | `		 * is the context itself, which php frees the moment its refcount hits` |
|       - |  4314 | `		 * zero. Nothing can still see it -- every holder there is takes a` |
|       - |  4315 | `		 * count -- so the struct goes back rather than being stamped closed` |
|       - |  4316 | `		 * the way a stream's is. */` |
|   80271 |  4317 | `		StreamCtxDestroy((phl_stream_ctx *)pDev);` |
|   80271 |  4318 | `		return;` |
|       - |  4319 | `	}` |
|   10459 |  4320 | `	if( pDev->iMagic == STREAM_FILTER_MAGIC \|\| pDev->iMagic == STREAM_FILTER_CLOSED_MAGIC` |
|    5363 |  4321 | `	 \|\| pDev->iMagic == STREAM_BRIGADE_MAGIC ){` |
|       - |  4322 | `		/* A filter has two owners -- the chain it sits on and the values naming` |
|       - |  4323 | `		 * it -- so losing the last value is only half the question; the filter` |
|       - |  4324 | `		 * half answers the other. */` |
|   10245 |  4325 | `		PH7_StreamFilterValueGone(pDev);` |
|   10245 |  4326 | `		return;` |
|       - |  4327 | `	}` |
|     222 |  4328 | `	if( pDev->bPersist \|\| pDev->pStream == 0 ){` |
|     ! 0 |  4329 | `		return;` |
|       - |  4330 | `	}` |
|       - |  4331 | `	/* The write chain gets its closing call while the device is still open, the` |
|       - |  4332 | `	 * way fclose() does it. */` |
|     222 |  4333 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     222 |  4334 | `	if( pDev->bDir ){` |
|      20 |  4335 | `		if( pDev->pStream->xCloseDir ){` |
|      20 |  4336 | `			pDev->pStream->xCloseDir(pDev->pHandle);` |
|     ! 0 |  4337 | `		}` |
|     ! 0 |  4338 | `	}else{` |
|     202 |  4339 | `		PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|       - |  4340 | `	}` |
|     222 |  4341 | `	MarkIOPrivateClosed(pDev);` |
|  220515 |  4342 | `}` |
|       - |  4343 | `/*` |
|       - |  4344 | `` * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and`` |
|       - |  4345 | `` * `mode` keys. php keeps the URI exactly as written (a relative path stays`` |
|       - |  4346 | ` * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves` |
|       - |  4347 | ` * the key out, which is how a popen() pipe reports no wrapper and no uri.` |
|       - |  4348 | ` */` |
|   12790 |  4349 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)` |
|       5 |  4350 | `{` |
|   12795 |  4351 | `	if( pDev == 0 ){` |
|     ! 0 |  4352 | `		return;` |
|       - |  4353 | `	}` |
|   12795 |  4354 | `	SyBlobReset(&pDev->sUri);` |
|   12795 |  4355 | `	if( zUri && nUriLen > 0 ){` |
|    2336 |  4356 | `		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);` |
|    1135 |  4357 | `	}` |
|   12795 |  4358 | `	if( zMode && nModeLen > 0 ){` |
|   12795 |  4359 | `		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){` |
|     ! 0 |  4360 | `			nModeLen = (int)sizeof(pDev->zMode) - 1;` |
|     ! 0 |  4361 | `		}` |
|   12795 |  4362 | `		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);` |
|   12795 |  4363 | `		pDev->zMode[nModeLen] = 0;` |
|    6352 |  4364 | `	}else{` |
|     ! 0 |  4365 | `		pDev->zMode[0] = 0;` |
|       - |  4366 | `	}` |
|    6352 |  4367 | `}` |
|       - |  4368 | `/*` |
|       - |  4369 | ` * Release the IO private structure.` |
|       - |  4370 | ` */` |
|     341 |  4371 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|       5 |  4372 | `{` |
|     346 |  4373 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     346 |  4374 | `	SyBlobRelease(&pDev->sBuffer);` |
|     346 |  4375 | `	SyBlobRelease(&pDev->sFilt);` |
|     346 |  4376 | `	SyBlobRelease(&pDev->sUri);` |
|     346 |  4377 | `	pDev->iHead = 0;  /* the chunk goes back to the pool: no probe may trust it */` |
|     346 |  4378 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|       - |  4379 | `	/* Release the whole structure */` |
|     346 |  4380 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     346 |  4381 | `}` |
|       - |  4382 | `/*` |
|       - |  4383 | ` * Release a handle shell whose open FAILED: it never reached PHP, so nothing can` |
|       - |  4384 | ` * hold a copy and the chunk goes back. For a caller outside this unit (the` |
|       - |  4385 | ` * XMLWriter URI writer builds its own handle the way fopen does).` |
|       - |  4386 | ` */` |
|      24 |  4387 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev)` |
|       3 |  4388 | `{` |
|      27 |  4389 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      27 |  4390 | `}` |
|       - |  4391 | `/*` |
|       - |  4392 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|       - |  4393 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|       - |  4394 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|       - |  4395 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|       - |  4396 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|       - |  4397 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|       - |  4398 | ` * pool-masked UAF) and keep reporting the handle open.` |
|       - |  4399 | ` */` |
|   12539 |  4400 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|       5 |  4401 | `{` |
|       - |  4402 | `	/* A filter outliving its handle would keep answering is_resource() and hold` |
|       - |  4403 | `	 * a pointer to a closed device; every close path releases the chains before` |
|       - |  4404 | `	 * the device goes, and this is the backstop for one that forgets. */` |
|   12544 |  4405 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   12544 |  4406 | `	SyBlobRelease(&pDev->sBuffer);` |
|   12544 |  4407 | `	SyBlobRelease(&pDev->sFilt);` |
|   12544 |  4408 | `	SyBlobRelease(&pDev->sUri);` |
|   12544 |  4409 | `	pDev->pHandle = 0;` |
|   12544 |  4410 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   12544 |  4411 | `}` |
|       - |  4412 | `/*` |
|       - |  4413 | ` * Reset the IO private structure.` |
|       - |  4414 | ` */` |
|     490 |  4415 | `static void ResetIOPrivate(io_private *pDev)` |
|       5 |  4416 | `{` |
|     495 |  4417 | `	SyBlobReset(&pDev->sBuffer);` |
|     495 |  4418 | `	pDev->nOfft = 0;` |
|       - |  4419 | `	/* A seek moves the DEVICE, so whatever the read chain had already produced` |
|       - |  4420 | `	 * from the old position is not what the new one answers. */` |
|     495 |  4421 | `	SyBlobReset(&pDev->sFilt);` |
|     495 |  4422 | `	pDev->nFiltOfft = 0;` |
|     495 |  4423 | `	pDev->bFiltDone = 0;` |
|     495 |  4424 | `	pDev->bFiltErr = 0;` |
|     495 |  4425 | `	PH7_StreamFilterRewound(pDev);` |
|       - |  4426 | `	/* Every caller of this has just MOVED the device (a seek, a rewind, a` |
|       - |  4427 | `	 * truncate), and php clears the end-of-file flag on exactly those. */` |
|     495 |  4428 | `	pDev->bEof = 0;` |
|     495 |  4429 | `}` |
|       - |  4430 | `/* Forward declaration */` |
|       - |  4431 |  |
|       - |  4432 | `/*` |
|       - |  4433 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|       - |  4434 | ` *  Open a file,a URL or any other IO stream.` |
|       - |  4435 | ` * Parameters` |
|       - |  4436 | ` *  $filename` |
|       - |  4437 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|       - |  4438 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|       - |  4439 | ` *   then a regular file is assumed.` |
|       - |  4440 | ` *  $mode` |
|       - |  4441 | ` *   The mode parameter specifies the type of access you require to the stream` |
|       - |  4442 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|       - |  4443 | ` *   modes.` |
|       - |  4444 | ` *  $use_include_path` |
|       - |  4445 | ` *   You can use the optional second parameter and set it to` |
|       - |  4446 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|       - |  4447 | ` *  $context` |
|       - |  4448 | ` *   A context stream resource.` |
|       - |  4449 | ` * Return` |
|       - |  4450 | ` *  File handle on success or FALSE on failure.` |
|       - |  4451 | ` */` |
|       - |  4452 | `/*` |
|       - |  4453 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|       - |  4454 | ` *                                  int $offset = -1)` |
|       - |  4455 | ` *  Read the remaining contents of a stream into a string.` |
|       - |  4456 | ` */` |
|     972 |  4457 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  4458 | `{` |
|       - |  4459 | `	const ph7_io_stream *pStream;` |
|       - |  4460 | `	io_private *pDev;` |
|     977 |  4461 | `	ph7_int64 nMax = -1;` |
|       - |  4462 | `	char zBuf[4096];` |
|       - |  4463 | `	ph7_int64 nRead;` |
|       - |  4464 | `	int rc;` |
|     977 |  4465 | `	if( nArg < 1 ){` |
|     ! 0 |  4466 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  4467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  4468 | `		return PH7_OK;` |
|       - |  4469 | `	}` |
|       - |  4470 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     977 |  4471 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     977 |  4472 | `	if( pDev == 0 ){` |
|       3 |  4473 | `		return rc;` |
|       - |  4474 | `	}` |
|     975 |  4475 | `	pStream = pDev->pStream;` |
|     975 |  4476 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|     ! 0 |  4477 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  4478 | `		return PH7_OK;` |
|       - |  4479 | `	}` |
|     975 |  4480 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - |  4481 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|       - |  4482 | `		 * default) means "read until EOF". */` |
|      20 |  4483 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      20 |  4484 | `		if( nMax < -1 ){` |
|       3 |  4485 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  4486 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|       - |  4487 | `		}` |
|       8 |  4488 | `	}` |
|     973 |  4489 | `	if( nArg > 2 ){` |
|      12 |  4490 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      12 |  4491 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      12 |  4492 | `			if( pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/) == PH7_OK ){` |
|       - |  4493 | `				/* A seek DISCARDS what was buffered ahead — the bytes belong to` |
|       - |  4494 | `				 * the position we just left. Without this the read below served` |
|       - |  4495 | `				 * the old position's leftovers and then continued from the new` |
|       - |  4496 | `				 * one. */` |
|      12 |  4497 | `				ResetIOPrivate(pDev);` |
|      12 |  4498 | `				StreamSeekLanded(pDev,iOfft,0/*SEEK_SET*/);` |
|       5 |  4499 | `			}` |
|       5 |  4500 | `		}` |
|       5 |  4501 | `	}` |
|     973 |  4502 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|    1865 |  4503 | `	while( nMax != 0 ){` |
|    1859 |  4504 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|    1859 |  4505 | `		if( nMax > 0 && nMax < nAsk ){` |
|       5 |  4506 | `			nAsk = nMax;` |
|       2 |  4507 | `		}` |
|       - |  4508 | `		/* Through PH7_StreamRead, not the device: this is a SCRIPT-level read,` |
|       - |  4509 | `		 * and the line readers buffer AHEAD. Reading the device directly meant` |
|       - |  4510 | ``		 * `stream_get_contents()` after any fgets()/fgetc()/stream_get_line()`` |
|       - |  4511 | `		 * skipped everything still sitting in that buffer — on a file the` |
|       - |  4512 | `		 * line reader had already drained to its end, that is the WHOLE` |
|       - |  4513 | `		 * remainder, so the everyday "read the first line, then take the rest"` |
|       - |  4514 | `		 * idiom answered "" and the position it left behind was wrong too. */` |
|    1859 |  4515 | `		nRead = PH7_StreamRead(pDev,zBuf,nAsk);` |
|    1859 |  4516 | `		if( nRead < 1 ){` |
|     967 |  4517 | `			if( nRead == 0 ){` |
|     961 |  4518 | `				pDev->bEof = 1;` |
|     478 |  4519 | `			}` |
|     967 |  4520 | `			StreamReportReadFailure(pCtx,pDev);` |
|     967 |  4521 | `			break;` |
|       - |  4522 | `		}` |
|     897 |  4523 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     897 |  4524 | `		if( nMax > 0 ){` |
|       5 |  4525 | `			nMax -= nRead;` |
|       2 |  4526 | `		}` |
|       5 |  4527 | `	}` |
|     973 |  4528 | `	return PH7_OK;` |
|     491 |  4529 | `}` |
|       - |  4530 | `/*` |
|       - |  4531 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|       - |  4532 | ` */` |
|      53 |  4533 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  4534 | `{` |
|       - |  4535 | `	ph7_value *pArr,*pV;` |
|       - |  4536 | `	ph7_io_stream **apDev;` |
|       - |  4537 | `	sxu32 n;` |
|      26 |  4538 | `	SXUNUSED(nArg);` |
|      26 |  4539 | `	SXUNUSED(apArg);` |
|      57 |  4540 | `	pArr = ph7_context_new_array(pCtx);` |
|      57 |  4541 | `	pV = ph7_context_new_scalar(pCtx);` |
|      57 |  4542 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 |  4543 | `		ph7_result_null(pCtx);` |
|     ! 0 |  4544 | `		return PH7_OK;` |
|       - |  4545 | `	}` |
|      57 |  4546 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     599 |  4547 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|       - |  4548 | `		/* A device a script has unregistered is GONE from php's list -- both a` |
|       - |  4549 | `		 * built-in it switched off and a userland wrapper it withdrew, which` |
|       - |  4550 | `		 * PHL used to keep naming here after neutering the slot behind it. */` |
|     546 |  4551 | `		if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|       9 |  4552 | `			continue;` |
|       - |  4553 | `		}` |
|     538 |  4554 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     538 |  4555 | `		ph7_array_add_elem(pArr,0,pV);` |
|     538 |  4556 | `		ph7_value_reset_string_cursor(pV);` |
|     266 |  4557 | `	}` |
|      57 |  4558 | `	ph7_result_value(pCtx,pArr);` |
|      57 |  4559 | `	return PH7_OK;` |
|      30 |  4560 | `}` |
|       - |  4561 | `/* The userland-wrapper pool is declared further down this file. */` |
|       - |  4562 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream);` |
|       - |  4563 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);` |
|       - |  4564 | `/*` |
|       - |  4565 | ` * php names TWO things in a stream's metadata: the WRAPPER that opened it` |
|       - |  4566 | `` * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for`` |
|       - |  4567 | `` * nearly every device — an ordinary file is opened by `plainfile` and driven by`` |
|       - |  4568 | `` * `STDIO` — and PHL answered its own single device name for both, so neither`` |
|       - |  4569 | ` * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()` |
|       - |  4570 | `` * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.`` |
|       - |  4571 | ` */` |
|     168 |  4572 | `static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)` |
|       5 |  4573 | `{` |
|     173 |  4574 | `	const ph7_io_stream *pS = pDev->pStream;` |
|     173 |  4575 | `	*pzWrapper = 0;` |
|     173 |  4576 | `	*pzStream  = "STDIO";` |
|     173 |  4577 | `	if( pS == 0 ){` |
|     ! 0 |  4578 | `		return;` |
|       - |  4579 | `	}` |
|     173 |  4580 | `	if( pDev->bDir ){` |
|       3 |  4581 | `		*pzWrapper = "plainfile";` |
|       3 |  4582 | `		*pzStream  = "dir";` |
|       3 |  4583 | `		return;` |
|       - |  4584 | `	}` |
|     171 |  4585 | `	if( is_php_stream(pS) ){` |
|      45 |  4586 | `		*pzWrapper = "PHP";` |
|      45 |  4587 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|       - |  4588 | `			/* php://output is the VM's output consumer, not a descriptor. */` |
|       3 |  4589 | `			*pzStream = "Output";` |
|       3 |  4590 | `			return;` |
|       - |  4591 | `		}` |
|      43 |  4592 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_INPUT ){` |
|       - |  4593 | `			/* php's request body, which a command line never has. */` |
|      11 |  4594 | `			*pzStream = "Input";` |
|      11 |  4595 | `			return;` |
|       - |  4596 | `		}` |
|      33 |  4597 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|       - |  4598 | `			/* php://memory and php://temp are ONE device here and two in php,` |
|       - |  4599 | `			 * which labels them apart; the URI is what separates them. */` |
|      25 |  4600 | `			const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|      25 |  4601 | `			sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|      35 |  4602 | `			*pzStream = ( nUri >= sizeof("php://temp")-1` |
|      22 |  4603 | `			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )` |
|      24 |  4604 | `				? "TEMP" : "MEMORY";` |
|      10 |  4605 | `		}` |
|      33 |  4606 | `		return;` |
|       - |  4607 | `	}` |
|     129 |  4608 | `	if( is_data_stream(pS) ){` |
|      15 |  4609 | `		*pzWrapper = *pzStream = "RFC2397";` |
|      15 |  4610 | `		return;` |
|       - |  4611 | `	}` |
|       - |  4612 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|     115 |  4613 | `	if( PH7_ZipStreamIs(pS) ){` |
|       - |  4614 | ``		/* One device, two doors, the same shape ZLIB has below: `zip://` opens`` |
|       - |  4615 | `		 * it through a WRAPPER and ZipArchive::getStream() hands one out` |
|       - |  4616 | ``		 * directly. php reports the ops as `zip` either way and names the`` |
|       - |  4617 | `		 * wrapper only for the first -- and here the URI cannot tell them` |
|       - |  4618 | `		 * apart, because getStream()'s stream carries the ENTRY name as its` |
|       - |  4619 | `		 * uri, so the handle is asked instead. */` |
|       5 |  4620 | `		*pzStream = "zip";` |
|       5 |  4621 | `		if( PH7_ZipStreamViaWrapper(pDev->pHandle) ){` |
|       3 |  4622 | `			*pzWrapper = "zip wrapper";` |
|       1 |  4623 | `		}` |
|       5 |  4624 | `		return;` |
|       - |  4625 | `	}` |
|       - |  4626 | `#endif` |
|       - |  4627 | `#ifdef PH7_ENABLE_ZLIB` |
|     111 |  4628 | `	if( PH7_ZlibStreamIs(pS) ){` |
|       - |  4629 | `		/* One device, two doors: compress.zlib:// goes through a WRAPPER and` |
|       - |  4630 | `		 * gzopen() opens the device directly -- which php reports by leaving` |
|       - |  4631 | ``		 * both `wrapper_type` and `uri` off the second one. The ops are the`` |
|       - |  4632 | `		 * same either way, and php names them ZLIB. */` |
|       5 |  4633 | `		*pzStream = "ZLIB";` |
|       5 |  4634 | `		if( SyBlobLength(&pDev->sUri) > 0 ){` |
|       3 |  4635 | `			*pzWrapper = "ZLIB";` |
|       1 |  4636 | `		}` |
|       5 |  4637 | `		return;` |
|       - |  4638 | `	}` |
|       - |  4639 | `#endif` |
|     107 |  4640 | `	if( IoPrivateIsUwrap(pS) ){` |
|       7 |  4641 | `		*pzWrapper = *pzStream = "user-space";` |
|       7 |  4642 | `		return;` |
|       - |  4643 | `	}` |
|       - |  4644 | `#ifdef PH7_ENABLE_NET` |
|     101 |  4645 | `	if( PH7_HttpStreamIs(pS) ){` |
|       - |  4646 | ``		/* php names the WRAPPER `http` and the ops under it the transport's,`` |
|       - |  4647 | `		 * which is the same socket label a tcp:// handle reports. */` |
|       6 |  4648 | `		*pzWrapper = "http";` |
|       6 |  4649 | `		*pzStream  = "tcp_socket/ssl";` |
|       6 |  4650 | `		return;` |
|       - |  4651 | `	}` |
|       - |  4652 | `#endif` |
|      95 |  4653 | `	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){` |
|       - |  4654 | `		/* php names the socket ops and reports no wrapper for them — and it has` |
|       - |  4655 | `		 * a set of ops PER TRANSPORT, so the label is how a script tells which` |
|       - |  4656 | `		 * one its handle got: a socket with no transport under it (a pair) is` |
|       - |  4657 | ``		 * `generic_socket` and a DATAGRAM one `udp_socket`. */`` |
|       - |  4658 | `#ifdef PH7_ENABLE_NET` |
|      55 |  4659 | `		if( pDev->pHandle && ((sock_private *)pDev->pHandle)->zLabel ){` |
|       - |  4660 | `			/* ext/sockets stated it outright (socket_export_stream). */` |
|       9 |  4661 | `			*pzStream = ((sock_private *)pDev->pHandle)->zLabel;` |
|      51 |  4662 | `		}else if( pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric ){` |
|       3 |  4663 | `			*pzStream = "generic_socket";` |
|      46 |  4664 | `		}else if( pDev->pHandle && ((sock_private *)pDev->pHandle)->bDgram ){` |
|       5 |  4665 | `			*pzStream = "udp_socket";` |
|       3 |  4666 | `		}else{` |
|      40 |  4667 | `			*pzStream = "tcp_socket/ssl";` |
|       - |  4668 | `		}` |
|       - |  4669 | `#else` |
|       - |  4670 | `		*pzStream = "tcp_socket/ssl";` |
|       - |  4671 | `#endif` |
|      55 |  4672 | `		return;` |
|       - |  4673 | `	}` |
|      42 |  4674 | `	if( SyBlobLength(&pDev->sUri) < 1 ){` |
|       - |  4675 | `		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()` |
|       - |  4676 | `		 * or proc_open() pipe end, or a device an extension built by hand like` |
|       - |  4677 | `		 * PDO's blob handle — which is precisely when php reports neither a` |
|       - |  4678 | ``		 * `wrapper_type` nor a `uri`. Such a device names its OWN ops: php's`` |
|       - |  4679 | ``		 * blob stream reports `PDOSQLite`, not the `STDIO` a descriptor gets. */`` |
|      13 |  4680 | `		if( pS->zName && pS->xOpen == 0 && pS->xOpenDir == 0 && pS->xSeek != 0 ){` |
|       - |  4681 | `			/* No opener, no descriptor under it, and it can SEEK: an extension` |
|       - |  4682 | `			 * built this handle itself (PDO's blob and LOB streams), and php` |
|       - |  4683 | `			 * names such a device's own ops. A pipe or a socket end has no seek` |
|       - |  4684 | `			 * and stays php's descriptor label. */` |
|      10 |  4685 | `			*pzStream = pS->zName;` |
|       4 |  4686 | `		}` |
|      13 |  4687 | `		return;` |
|       - |  4688 | `	}` |
|      30 |  4689 | `	*pzWrapper = "plainfile";` |
|      88 |  4690 | `}` |
|       - |  4691 | `/*` |
|       - |  4692 | ` * The WRAPPER label on its own, for a device rather than an open handle: php's` |
|       - |  4693 | `` * path operations name it in their refusals (`unlink(): ZLIB does not allow`` |
|       - |  4694 | `` * unlinking`), and they have no handle to ask. Answers 0 for the plain-file`` |
|       - |  4695 | ` * wrapper -- which implements the operations rather than refusing them -- and` |
|       - |  4696 | ` * for a userland one, whose own door runs before this.` |
|       - |  4697 | ` */` |
|   52412 |  4698 | `PH7_PRIVATE const char * PH7_StreamWrapperLabel(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 |  4699 | `{` |
|   52417 |  4700 | `	if( pStream == 0 \|\| pStream == pVm->pDefStream \|\| IoPrivateIsUwrap(pStream) ){` |
|   52304 |  4701 | `		return 0;` |
|       - |  4702 | `	}` |
|     115 |  4703 | `	if( is_php_stream(pStream) ){` |
|      14 |  4704 | `		return "PHP";` |
|       - |  4705 | `	}` |
|     103 |  4706 | `	if( is_data_stream(pStream) ){` |
|       8 |  4707 | `		return "RFC2397";` |
|       - |  4708 | `	}` |
|       - |  4709 | `#ifdef PH7_ENABLE_ZLIB` |
|      97 |  4710 | `	if( PH7_ZlibStreamIs(pStream) ){` |
|      14 |  4711 | `		return "ZLIB";` |
|       - |  4712 | `	}` |
|       - |  4713 | `#endif` |
|       - |  4714 | `	/* php's remaining built-in wrappers label themselves with their scheme. */` |
|      84 |  4715 | `	return pStream->zName;` |
|   26185 |  4716 | `}` |
|       - |  4717 | `/*` |
|       - |  4718 | `` * The `stream_type` label above, on its own. ext/posix prints it in php's`` |
|       - |  4719 | `` * `Could not use stream of type '%s'` -- the diagnostic a descriptor door`` |
|       - |  4720 | ` * raises for a stream that has no descriptor behind it -- and php reads it from` |
|       - |  4721 | ` * the same place its metadata does.` |
|       - |  4722 | ` */` |
|       4 |  4723 | `PH7_PRIVATE const char * PH7_StreamTypeLabel(io_private *pDev)` |
|       1 |  4724 | `{` |
|       5 |  4725 | `	const char *zWrapper = 0,*zStream = "STDIO";` |
|       5 |  4726 | `	if( pDev == 0 ){` |
|     ! 0 |  4727 | `		return "STDIO";` |
|       - |  4728 | `	}` |
|       5 |  4729 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|       5 |  4730 | `	return zStream;` |
|       2 |  4731 | `}` |
|       - |  4732 |  |
|       - |  4733 | `/*` |
|       - |  4734 | ` * data:// carries its own metadata in php, and all of it comes back out of the` |
|       - |  4735 | ``  * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>` `` |
|       - |  4736 | ` * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A` |
|       - |  4737 | `` * URI naming no media type has no `mediatype` key at all — php does not`` |
|       - |  4738 | ` * substitute the RFC's default — and a repeated parameter keeps its last value.` |
|       - |  4739 | ` */` |
|      12 |  4740 | `static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|       1 |  4741 | `{` |
|      13 |  4742 | `	const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|      13 |  4743 | `	sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|      13 |  4744 | `	sxu32 nStart = 0,nComma,nSeg,i;` |
|      13 |  4745 | `	int bBase64 = 0,bFirst = 1;` |
|      13 |  4746 | `	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){` |
|      13 |  4747 | `		nStart = sizeof("data://")-1;` |
|       6 |  4748 | `	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){` |
|     ! 0 |  4749 | `		nStart = sizeof("data:")-1;` |
|     ! 0 |  4750 | `	}` |
|      13 |  4751 | `	nComma = nStart;` |
|     329 |  4752 | `	while( nComma < nUri && zUri[nComma] != ',' ){` |
|     317 |  4753 | `		nComma++;` |
|       1 |  4754 | `	}` |
|       - |  4755 | `	/* Walk the ';'-separated segments in front of the payload. */` |
|      23 |  4756 | `	for( nSeg = nStart ; nSeg <= nComma ; ){` |
|      23 |  4757 | `		sxu32 nEnd = nSeg;` |
|     329 |  4758 | `		while( nEnd < nComma && zUri[nEnd] != ';' ){` |
|     307 |  4759 | `			nEnd++;` |
|       1 |  4760 | `		}` |
|      23 |  4761 | `		if( bFirst ){` |
|      13 |  4762 | `			if( nEnd > nSeg ){` |
|      11 |  4763 | `				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));` |
|      11 |  4764 | `				ph7_array_add_strkey_elem(pArr,"mediatype",pV);` |
|      11 |  4765 | `				ph7_value_reset_string_cursor(pV);` |
|       5 |  4766 | `			}` |
|      13 |  4767 | `			bFirst = 0;` |
|      17 |  4768 | `		}else if( nEnd - nSeg == sizeof("base64")-1` |
|       8 |  4769 | `		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){` |
|       5 |  4770 | `			bBase64 = 1;` |
|       3 |  4771 | `		}else{` |
|       - |  4772 | ``			/* `name=value`; php keys the array by the name, so a repeat wins. */`` |
|     167 |  4773 | `			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}` |
|       7 |  4774 | `			if( i < nEnd && i > nSeg ){` |
|       - |  4775 | `				/* The name is keyed WHOLE — it has no length limit in the URI,` |
|       - |  4776 | `				 * and a clamped one files the value under a key no script can` |
|       - |  4777 | `				 * look up. */` |
|       7 |  4778 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|       7 |  4779 | `				if( pKey ){` |
|       7 |  4780 | `					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));` |
|       7 |  4781 | `					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));` |
|       7 |  4782 | `					ph7_array_add_elem(pArr,pKey,pV);` |
|       7 |  4783 | `					ph7_value_reset_string_cursor(pV);` |
|       7 |  4784 | `					ph7_context_release_value(pCtx,pKey);` |
|       3 |  4785 | `				}` |
|       3 |  4786 | `			}` |
|       - |  4787 | `		}` |
|      23 |  4788 | `		if( nEnd >= nComma ){` |
|      13 |  4789 | `			break;` |
|       - |  4790 | `		}` |
|      11 |  4791 | `		nSeg = nEnd + 1;` |
|       1 |  4792 | `	}` |
|      13 |  4793 | `	ph7_value_bool(pV,bBase64);` |
|      13 |  4794 | `	ph7_array_add_strkey_elem(pArr,"base64",pV);` |
|      13 |  4795 | `}` |
|       - |  4796 | `#if defined(PH7_ENABLE_NET) && defined(PH7_ENABLE_OPENSSL)` |
|       - |  4797 | `/*` |
|       - |  4798 | `` * php's `crypto` sub-array: what the handle's LIVE session settled on, and the`` |
|       - |  4799 | ` * only door a script has to it. It is present exactly while a session is` |
|       - |  4800 | ` * active -- never on a plain socket, never on a listener, and gone again after` |
|       - |  4801 | ` * stream_socket_enable_crypto($h, false) -- and php builds it FIRST, so it` |
|       - |  4802 | `` * leads the key order that every other device starts with `timed_out`.`` |
|       - |  4803 | ` *` |
|       - |  4804 | `` * `protocol` is php's spelling of the negotiated version rather than OpenSSL's,`` |
|       - |  4805 | `` * and anything php's build has no name for reads `UNKNOWN`; the other three are`` |
|       - |  4806 | ` * the cipher's own answers. php never checks that a cipher was settled at all,` |
|       - |  4807 | `` * and neither does this -- SSL_CIPHER_get_name(0) is `(NONE)` and the two`` |
|       - |  4808 | ` * numbers are zero, which is what a script sees on a session that has none.` |
|       - |  4809 | ` */` |
|     152 |  4810 | `static void SockSslMetaData(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|       5 |  4811 | `{` |
|       - |  4812 | `	sock_private *pSock;` |
|       - |  4813 | `	const SSL_CIPHER *pCipher;` |
|       - |  4814 | `	ph7_value *pCrypto;` |
|       - |  4815 | `	const char *zProto;` |
|     157 |  4816 | `	if( pDev->pStream != &sTCP_Stream \|\| pDev->pHandle == 0 ){` |
|     104 |  4817 | `		return;` |
|       - |  4818 | `	}` |
|      55 |  4819 | `	pSock = (sock_private *)pDev->pHandle;` |
|      55 |  4820 | `	if( pSock->pSsl == 0 ){` |
|      51 |  4821 | `		return;` |
|       - |  4822 | `	}` |
|       4 |  4823 | `	pCrypto = ph7_context_new_array(pCtx);` |
|       4 |  4824 | `	if( pCrypto == 0 ){` |
|     ! 0 |  4825 | `		return;` |
|       - |  4826 | `	}` |
|       4 |  4827 | `	switch( SSL_version((SSL *)pSock->pSsl) ){` |
|       4 |  4828 | `		case TLS1_3_VERSION: zProto = "TLSv1.3"; break;` |
|     ! 0 |  4829 | `		case TLS1_2_VERSION: zProto = "TLSv1.2"; break;` |
|     ! 0 |  4830 | `		case TLS1_1_VERSION: zProto = "TLSv1.1"; break;` |
|     ! 0 |  4831 | `		case TLS1_VERSION:   zProto = "TLSv1";   break;` |
|     ! 0 |  4832 | `		default:             zProto = "UNKNOWN"; break;` |
|       - |  4833 | `	}` |
|       4 |  4834 | `	pCipher = SSL_get_current_cipher((SSL *)pSock->pSsl);` |
|       4 |  4835 | `	ph7_value_string(pV,zProto,-1);` |
|       4 |  4836 | `	ph7_array_add_strkey_elem(pCrypto,"protocol",pV);` |
|       4 |  4837 | `	ph7_value_reset_string_cursor(pV);` |
|       4 |  4838 | `	ph7_value_string(pV,SSL_CIPHER_get_name(pCipher),-1);` |
|       4 |  4839 | `	ph7_array_add_strkey_elem(pCrypto,"cipher_name",pV);` |
|       4 |  4840 | `	ph7_value_reset_string_cursor(pV);` |
|       4 |  4841 | `	ph7_value_int64(pV,(ph7_int64)SSL_CIPHER_get_bits(pCipher,0));` |
|       4 |  4842 | `	ph7_array_add_strkey_elem(pCrypto,"cipher_bits",pV);` |
|       4 |  4843 | `	ph7_value_string(pV,SSL_CIPHER_get_version(pCipher),-1);` |
|       4 |  4844 | `	ph7_array_add_strkey_elem(pCrypto,"cipher_version",pV);` |
|       4 |  4845 | `	ph7_value_reset_string_cursor(pV);` |
|       4 |  4846 | `	ph7_array_add_strkey_elem(pArr,"crypto",pCrypto);` |
|      81 |  4847 | `}` |
|       - |  4848 | `#endif /* PH7_ENABLE_NET && PH7_ENABLE_OPENSSL */` |
|       - |  4849 | `/*` |
|       - |  4850 | ` * array stream_get_meta_data(resource $stream)` |
|       - |  4851 | ` *` |
|       - |  4852 | ` * php's own key set, in php's own order. What used to be here answered a` |
|       - |  4853 | `` * best-effort shape: `mode` and `uri` did not exist at all (so the documented`` |
|       - |  4854 | `` * way to ask a handle what FILE it is on was an `Undefined array key` and`` |
|       - |  4855 | `` * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),`` |
|       - |  4856 | `` * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both`` |
|       - |  4857 | ` * PHL's internal device name rather than php's two different labels.` |
|       - |  4858 | ` */` |
|     154 |  4859 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  4860 | `{` |
|       - |  4861 | `	const char *zWrapper,*zStream;` |
|       - |  4862 | `	io_private *pDev;` |
|       - |  4863 | `	ph7_value *pArr,*pV;` |
|       - |  4864 | `	sxu32 nUnread;` |
|       - |  4865 | `	int rc;` |
|     159 |  4866 | `	if( nArg < 1 ){` |
|     ! 0 |  4867 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 |  4868 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  4869 | `		return PH7_OK;` |
|       - |  4870 | `	}` |
|       - |  4871 | `	/* php's two TypeErrors, in place of the legacy warn-and-false. */` |
|     159 |  4872 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|     159 |  4873 | `	if( pDev == 0 ){` |
|       3 |  4874 | `		return rc;` |
|       - |  4875 | `	}` |
|     157 |  4876 | `	pArr = ph7_context_new_array(pCtx);` |
|     157 |  4877 | `	pV = ph7_context_new_scalar(pCtx);` |
|     157 |  4878 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 |  4879 | `		ph7_result_null(pCtx);` |
|     ! 0 |  4880 | `		return PH7_OK;` |
|       - |  4881 | `	}` |
|     157 |  4882 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|       - |  4883 | `	/* Sample this BEFORE the eof probe below: php answers eof from state it` |
|       - |  4884 | ``	 * already has and never reads ahead for it, so its `unread_bytes` counts`` |
|       - |  4885 | `	 * only what the SCRIPT's own reads left buffered. */` |
|     157 |  4886 | `	nUnread = StreamAheadBytes(pDev);` |
|       - |  4887 | `#ifdef PH7_ENABLE_NET` |
|     157 |  4888 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - |  4889 | `		/* The http wrapper does its own buffering, and what it holds is exactly` |
|       - |  4890 | `		 * what php counts here. */` |
|       6 |  4891 | `		nUnread += PH7_HttpStreamUnread(pDev->pHandle);` |
|       3 |  4892 | `	}` |
|       - |  4893 | `#endif` |
|       - |  4894 | `#if defined(PH7_ENABLE_NET) && defined(PH7_ENABLE_OPENSSL)` |
|       - |  4895 | `	/* Before the three defaults below, because php's crypto layer adds its own` |
|       - |  4896 | `	 * key first and then replaces them. */` |
|     157 |  4897 | `	SockSslMetaData(pCtx,pDev,pArr,pV);` |
|     157 |  4898 | `	ph7_value_reset_string_cursor(pV);` |
|       - |  4899 | `#endif` |
|     157 |  4900 | `	if( is_data_stream(pDev->pStream) ){` |
|       - |  4901 | `		/* A device that answers metadata of its OWN replaces php's three` |
|       - |  4902 | `		 * defaults rather than adding to them: data:// (and php://temp, which` |
|       - |  4903 | `		 * simply has none) report no timed_out/blocked/eof at all. */` |
|      13 |  4904 | `		IoPrivateDataMeta(pCtx,pDev,pArr,pV);` |
|      13 |  4905 | `		ph7_value_reset_string_cursor(pV);` |
|     147 |  4906 | `	}else if( is_php_stream(pDev->pStream)` |
|      86 |  4907 | `	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY` |
|      28 |  4908 | `	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){` |
|       - |  4909 | `		/* php://temp: same rule, no keys of its own. */` |
|       2 |  4910 | `	}else{` |
|     143 |  4911 | `		ph7_value_bool(pV,pDev->bTimedOut != 0);` |
|     143 |  4912 | `		ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|       - |  4913 | `		/* A stream php cannot put in non-blocking mode always reports blocked;` |
|       - |  4914 | `		 * bNonBlock is only ever set for one that CAN. */` |
|     143 |  4915 | `		ph7_value_bool(pV,pDev->bNonBlock == 0);` |
|     143 |  4916 | `		ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|       - |  4917 | `		/* php COPIES the stored flag here and runs no probe of its own, so a` |
|       - |  4918 | `		 * socket feof() has not been called on yet reports false even when its` |
|       - |  4919 | `		 * peer is already gone. */` |
|     143 |  4920 | `		ph7_value_bool(pV,StreamEofCommon(pDev,0) != 0);` |
|     143 |  4921 | `		ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|       - |  4922 | `	}` |
|       - |  4923 | `#ifdef PH7_ENABLE_NET` |
|     157 |  4924 | `	if( PH7_HttpStreamIs(pDev->pStream) ){` |
|       - |  4925 | ``		/* php's `wrapper_data` for an http handle is the response headers of the`` |
|       - |  4926 | `		 * exchange THIS handle made -- the redirect chain's included, in the` |
|       - |  4927 | `		 * order they arrived. */` |
|       6 |  4928 | `		ph7_value *pHdr = PH7_HttpStreamHeaderArray(pCtx->pVm,pDev->pHandle);` |
|       6 |  4929 | `		if( pHdr ){` |
|       6 |  4930 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",pHdr);` |
|       6 |  4931 | `			ph7_release_value(pCtx->pVm,pHdr);` |
|       3 |  4932 | `		}` |
|       3 |  4933 | `	}` |
|       - |  4934 | `#endif` |
|       - |  4935 | `	{` |
|     157 |  4936 | `		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);` |
|     157 |  4937 | `		if( pObj ){` |
|       - |  4938 | `			/* php hands the wrapper INSTANCE back, which is the only way a` |
|       - |  4939 | `			 * script can reach the object serving an open userland stream. */` |
|       - |  4940 | `			ph7_value sObj;` |
|       5 |  4941 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|       5 |  4942 | `			sObj.x.pOther = pObj;` |
|       5 |  4943 | `			sObj.iFlags = MEMOBJ_OBJ;` |
|       5 |  4944 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);` |
|       2 |  4945 | `		}` |
|       - |  4946 | `	}` |
|     157 |  4947 | `	if( zWrapper ){` |
|      90 |  4948 | `		ph7_value_string(pV,zWrapper,-1);` |
|      90 |  4949 | `		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      90 |  4950 | `		ph7_value_reset_string_cursor(pV);` |
|      43 |  4951 | `	}` |
|     157 |  4952 | `	ph7_value_string(pV,zStream,-1);` |
|     157 |  4953 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|     157 |  4954 | `	ph7_value_reset_string_cursor(pV);` |
|     157 |  4955 | `	ph7_value_string(pV,pDev->zMode,-1);` |
|     157 |  4956 | `	ph7_array_add_strkey_elem(pArr,"mode",pV);` |
|     157 |  4957 | `	ph7_value_reset_string_cursor(pV);` |
|       - |  4958 | `	/* Bytes already pulled off the device and not yet handed to the script —` |
|       - |  4959 | `	 * php's own writepos-minus-readpos, which was hardcoded 0. */` |
|     157 |  4960 | `	ph7_value_int64(pV,(ph7_int64)nUnread);` |
|     157 |  4961 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|       - |  4962 | `	{` |
|       - |  4963 | `		/* php answers this from what the handle actually SITS ON, not from what` |
|       - |  4964 | `		 * the device could do: php://stdout is seekable into a file and not` |
|       - |  4965 | `		 * down a pipe, php://output never is, and a pipe is not. Ask the` |
|       - |  4966 | `		 * descriptor first and the device second; a USERLAND wrapper is php's` |
|       - |  4967 | `		 * one exception — its ops always carry a seek, so php always says yes. */` |
|     157 |  4968 | `		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;` |
|     157 |  4969 | `		if( pDev->bDir ){` |
|       - |  4970 | `			/* php's directory ops carry a rewind, so a dir handle is seekable —` |
|       - |  4971 | `			 * and asking the FILE device where it is would hand lseek() the` |
|       - |  4972 | `			 * DIR* this handle stores where a file stores its descriptor. */` |
|       3 |  4973 | `			bSeekable = 1;` |
|     156 |  4974 | `		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){` |
|      86 |  4975 | `			int rcSeek = PH7_StreamHandleCanSeek(pDev);` |
|      86 |  4976 | `			if( rcSeek >= 0 ){` |
|      33 |  4977 | `				bSeekable = rcSeek;` |
|      67 |  4978 | `			}else if( is_php_stream(pDev->pStream)` |
|      42 |  4979 | `			       && PH7_PhpStreamInner(pDev->pHandle) == 0 ){` |
|       - |  4980 | `				/* php://output has no seek AT ALL, and its neighbours on this` |
|       - |  4981 | `				 * one device do. Ask with the seek that moves nothing: an` |
|       - |  4982 | `				 * unsupported one answers SXERR_NOTIMPLEMENTED, which is the` |
|       - |  4983 | `				 * same "no seek here" php's NO_SEEK flag records. The question` |
|       - |  4984 | `				 * is not put to any other device -- a handle an extension built` |
|       - |  4985 | `				 * can have a POSITION riding on its last seek (PDO's blob marks` |
|       - |  4986 | `				 * its own unknown), and asking would move it -- nor to a` |
|       - |  4987 | `				 * php://filter PROXY, whose seek is a real seek of the stream` |
|       - |  4988 | `				 * underneath and would drop what the chain had produced. */` |
|      41 |  4989 | `				bSeekable = pDev->pStream->xSeek(pDev->pHandle,0,1/*SEEK_CUR*/)` |
|      26 |  4990 | `					!= SXERR_NOTIMPLEMENTED;` |
|      41 |  4991 | `			}else if( pDev->pStream->xOpen != 0 && pDev->pStream->xTell != 0 ){` |
|       - |  4992 | `				/* Ask the HANDLE where it is, which is how a descriptor-backed` |
|       - |  4993 | `				 * device says it cannot seek. A device an extension built by` |
|       - |  4994 | `				 * hand (PDO's blob handle) has no opener and answers for itself` |
|       - |  4995 | `				 * -- its xSeek IS the answer, and a position it reports as` |
|       - |  4996 | `				 * unknown after a failed seek must not read as "not seekable". */` |
|      19 |  4997 | `				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;` |
|       8 |  4998 | `			}` |
|      41 |  4999 | `		}` |
|     157 |  5000 | `		ph7_value_bool(pV,bSeekable);` |
|       - |  5001 | `	}` |
|     157 |  5002 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|     157 |  5003 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|       - |  5004 | `		/* php keeps the path exactly as the opener received it — a relative` |
|       - |  5005 | `		 * one stays relative — and omits the key for a stream that has none. */` |
|     133 |  5006 | `		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));` |
|     133 |  5007 | `		ph7_array_add_strkey_elem(pArr,"uri",pV);` |
|     133 |  5008 | `		ph7_value_reset_string_cursor(pV);` |
|      64 |  5009 | `	}` |
|     157 |  5010 | `	ph7_result_value(pCtx,pArr);` |
|     157 |  5011 | `	return PH7_OK;` |
|      82 |  5012 | `}` |
|       - |  5013 | `/*` |
|       - |  5014 | ` * ---------------------------------------------------------------------------` |
|       - |  5015 | ` * Stream contexts (stream_context_create and the accessor family).` |
|       - |  5016 | ` *` |
|       - |  5017 | `` * php's context is a `stream-context` RESOURCE holding two things: a`` |
|       - |  5018 | `` * wrapper => option => value map, and the `notification` parameter. Both`` |
|       - |  5019 | ` * levels keep INSERTION order, which is the order stream_context_get_options()` |
|       - |  5020 | ` * answers in, so the store is a real nested array rather than a flat table.` |
|       - |  5021 | ` *` |
|       - |  5022 | ` * A PHL resource is a bare void*, so the struct opens with an io_private` |
|       - |  5023 | ` * header carrying its own magic (the shape proc_open()'s handle already uses)` |
|       - |  5024 | ` * and the VM owns every one it hands out.` |
|       - |  5025 | ` * ---------------------------------------------------------------------------` |
|       - |  5026 | ` */` |
|       - |  5027 | `/* Allocate one context, chained on the VM registry. */` |
|   80805 |  5028 | `static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)` |
|       5 |  5029 | `{` |
|       - |  5030 | `	phl_stream_ctx *pRes;` |
|   80810 |  5031 | `	if( pVm == 0 ){` |
|     ! 0 |  5032 | `		return 0;` |
|       - |  5033 | `	}` |
|   80810 |  5034 | `	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));` |
|   80810 |  5035 | `	if( pRes == 0 ){` |
|     ! 0 |  5036 | `		return 0;` |
|       - |  5037 | `	}` |
|   80810 |  5038 | `	SyZero(pRes,sizeof(phl_stream_ctx));` |
|       - |  5039 | `	/* The header word every resource probe reads, and what tells the value` |
|       - |  5040 | `	 * doors this pointer carries a count they may take. */` |
|   80810 |  5041 | `	pRes->base.iHead = IO_PRIVATE_HEAD_MAGIC;` |
|   80810 |  5042 | `	pRes->base.iMagic = STREAM_CTX_MAGIC;` |
|   80810 |  5043 | `	pRes->base.nValRef = 0;` |
|   80810 |  5044 | `	pRes->pVm = pVm;` |
|   80810 |  5045 | `	pRes->pOptions = ph7_new_array(pVm);` |
|   80810 |  5046 | `	if( pRes->pOptions == 0 ){` |
|     ! 0 |  5047 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|     ! 0 |  5048 | `		return 0;` |
|       - |  5049 | `	}` |
|   80810 |  5050 | `	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;` |
|   80810 |  5051 | `	if( pRes->pNext ){` |
|     348 |  5052 | `		pRes->pNext->pPrev = pRes;` |
|     172 |  5053 | `	}` |
|   80810 |  5054 | `	pRes->pPrev = 0;` |
|   80810 |  5055 | `	pVm->pStreamCtx = (void *)pRes;` |
|   80810 |  5056 | `	return pRes;` |
|   40405 |  5057 | `}` |
|       - |  5058 | `/*` |
|       - |  5059 | ` * Take one count for a holder that is NOT a ph7_value: the VM's default` |
|       - |  5060 | ` * context and a stream that carries one in io_private.pCtxRes both outlive` |
|       - |  5061 | ` * every value the script has, and an uncounted holder cannot be told from the` |
|       - |  5062 | ` * last one out.` |
|       - |  5063 | ` */` |
|     786 |  5064 | `static void StreamCtxHold(phl_stream_ctx *pRes)` |
|       5 |  5065 | `{` |
|     791 |  5066 | `	if( pRes ){` |
|     717 |  5067 | `		pRes->base.nValRef++;` |
|     354 |  5068 | `	}` |
|     791 |  5069 | `}` |
|       - |  5070 | `/*` |
|       - |  5071 | ` * Free one context: unlink it from the VM registry, drop the two values it` |
|       - |  5072 | ` * owns, and clear the magic so a pointer that somehow outlived it cannot read` |
|       - |  5073 | ` * a live context out of freed memory. Called when the last holder goes.` |
|       - |  5074 | ` */` |
|   80268 |  5075 | `static void StreamCtxDestroy(phl_stream_ctx *pRes)` |
|       3 |  5076 | `{` |
|       - |  5077 | `	ph7_vm *pVm;` |
|   80271 |  5078 | `	if( pRes == 0 \|\| pRes->pVm == 0 ){` |
|     ! 0 |  5079 | `		return;` |
|       - |  5080 | `	}` |
|   80271 |  5081 | `	pVm = pRes->pVm;` |
|   80271 |  5082 | `	if( pRes->pPrev ){` |
|       3 |  5083 | `		pRes->pPrev->pNext = pRes->pNext;` |
|   80269 |  5084 | `	}else if( pVm->pStreamCtx == (void *)pRes ){` |
|   80268 |  5085 | `		pVm->pStreamCtx = (void *)pRes->pNext;` |
|   40133 |  5086 | `	}` |
|   80271 |  5087 | `	if( pRes->pNext ){` |
|     271 |  5088 | `		pRes->pNext->pPrev = pRes->pPrev;` |
|     134 |  5089 | `	}` |
|   80271 |  5090 | `	if( pVm->pOpenCtx == (void *)pRes ){` |
|     ! 0 |  5091 | `		pVm->pOpenCtx = 0;` |
|     ! 0 |  5092 | `	}` |
|   80271 |  5093 | `	if( pRes->pOptions ){` |
|   80271 |  5094 | `		ph7_release_value(pVm,pRes->pOptions);` |
|   40134 |  5095 | `	}` |
|   80271 |  5096 | `	if( pRes->pNotify ){` |
|      65 |  5097 | `		ph7_release_value(pVm,pRes->pNotify);` |
|      32 |  5098 | `	}` |
|   80271 |  5099 | `	pRes->base.iMagic = 0;` |
|   80271 |  5100 | `	pRes->base.iHead = 0;` |
|   80271 |  5101 | `	SyMemBackendFree(&pVm->sAllocator,pRes);` |
|   40137 |  5102 | `}` |
|       - |  5103 | `/*` |
|       - |  5104 | ` * The context behind a ph7_value, or 0 when the value is not one. The magic` |
|       - |  5105 | ` * probe is the same in-bounds one every resource here answers to.` |
|       - |  5106 | ` */` |
|     486 |  5107 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)` |
|       5 |  5108 | `{` |
|       - |  5109 | `	phl_stream_ctx *pRes;` |
|     491 |  5110 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|      19 |  5111 | `		return 0;` |
|       - |  5112 | `	}` |
|     473 |  5113 | `	pRes = (phl_stream_ctx *)pVal->x.pOther;` |
|     473 |  5114 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_CTX_MAGIC ){` |
|      57 |  5115 | `		return 0;` |
|       - |  5116 | `	}` |
|     419 |  5117 | `	return pRes;` |
|     248 |  5118 | `}` |
|       - |  5119 | `/*` |
|       - |  5120 | ` * The per-VM DEFAULT context. php creates it on demand — the first` |
|       - |  5121 | ` * stream_context_get_default()/set_default() call — and every opener that was` |
|       - |  5122 | ` * handed no context of its own falls back to it.` |
|       - |  5123 | ` */` |
|   87008 |  5124 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)` |
|       5 |  5125 | `{` |
|   87013 |  5126 | `	if( pVm == 0 ){` |
|     ! 0 |  5127 | `		return 0;` |
|       - |  5128 | `	}` |
|   87013 |  5129 | `	if( pVm->pDefaultCtx == 0 ){` |
|     464 |  5130 | `		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);` |
|       - |  5131 | `		/* The VM itself is a holder: the default context outlives every value` |
|       - |  5132 | `		 * an opener was handed, and lives until the VM is reset. */` |
|     464 |  5133 | `		StreamCtxHold((phl_stream_ctx *)pVm->pDefaultCtx);` |
|     227 |  5134 | `	}` |
|   87013 |  5135 | `	return (phl_stream_ctx *)pVm->pDefaultCtx;` |
|   43435 |  5136 | `}` |
|       - |  5137 | `/*` |
|       - |  5138 | ` * Drop every context this VM created. Called from PH7_VmReset, so a reused VM` |
|       - |  5139 | ` * (the -S server's) does not carry one request's default context into the next.` |
|       - |  5140 | ` */` |
|      16 |  5141 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|     ! 0 |  5142 | `{` |
|       - |  5143 | `	phl_stream_ctx *pRes;` |
|      16 |  5144 | `	if( pVm == 0 ){` |
|     ! 0 |  5145 | `		return;` |
|       - |  5146 | `	}` |
|      16 |  5147 | `	pRes = (phl_stream_ctx *)pVm->pStreamCtx;` |
|      24 |  5148 | `	while( pRes ){` |
|       8 |  5149 | `		phl_stream_ctx *pNext = pRes->pNext;` |
|       8 |  5150 | `		if( pRes->pOptions ){` |
|       8 |  5151 | `			ph7_release_value(pVm,pRes->pOptions);` |
|       4 |  5152 | `		}` |
|       8 |  5153 | `		if( pRes->pNotify ){` |
|     ! 0 |  5154 | `			ph7_release_value(pVm,pRes->pNotify);` |
|     ! 0 |  5155 | `		}` |
|       - |  5156 | `		/* Any ph7_value still naming this pointer must stop reporting a live` |
|       - |  5157 | `		 * context, so clear the magic before the memory goes back. */` |
|       8 |  5158 | `		pRes->base.iMagic = 0;` |
|       8 |  5159 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|       8 |  5160 | `		pRes = pNext;` |
|     ! 0 |  5161 | `	}` |
|      16 |  5162 | `	pVm->pStreamCtx = 0;` |
|      16 |  5163 | `	pVm->pDefaultCtx = 0;` |
|       - |  5164 | `	/* Whatever an interrupted open left armed named one of those. */` |
|      16 |  5165 | `	pVm->pOpenCtx = 0;` |
|       8 |  5166 | `}` |
|       - |  5167 | `/* The live element of pArray under pKey, or 0 when there is none. */` |
|  160666 |  5168 | `static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)` |
|       5 |  5169 | `{` |
|       - |  5170 | `	ph7_hashmap_node *pNode;` |
|  160671 |  5171 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  5172 | `		return 0;` |
|       - |  5173 | `	}` |
|  160671 |  5174 | `	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){` |
|   80243 |  5175 | `		return 0;` |
|       - |  5176 | `	}` |
|   80433 |  5177 | `	return (ph7_value *)PH7_MemObjAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|   80338 |  5178 | `}` |
|       - |  5179 | `/*` |
|       - |  5180 | ` * Store one option. The wrapper's sub-array is created on first use; an` |
|       - |  5181 | ` * existing one may be SHARED with the script array it was stored from, so it` |
|       - |  5182 | ` * is separated first — otherwise setting an option would write through into` |
|       - |  5183 | ` * the caller's own array.` |
|       - |  5184 | ` */` |
|   80428 |  5185 | `static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)` |
|       5 |  5186 | `{` |
|       - |  5187 | `	ph7_value sKey,sName,sVal;` |
|       - |  5188 | `	ph7_value *pSub;` |
|       - |  5189 | `	ph7_hashmap *pMap;` |
|   80433 |  5190 | `	if( pRes == 0 \|\| pRes->pOptions == 0 \|\| pWrapper == 0 \|\| pName == 0 \|\| pValue == 0 ){` |
|     ! 0 |  5191 | `		return -1;` |
|       - |  5192 | `	}` |
|       - |  5193 | `	/* Every insertion below can reserve a memory object, which used to GROW (and` |
|       - |  5194 | `	 * therefore move) pVm->aMemObj — and all three arguments may point into it,` |
|       - |  5195 | `	 * so the structs are snapshotted first. Redundant since P1 (fixed segments);` |
|       - |  5196 | `	 * left for the harvest sweep. */` |
|   80433 |  5197 | `	sKey = *pWrapper; pWrapper = &sKey;` |
|   80433 |  5198 | `	sName = *pName;   pName = &sName;` |
|   80433 |  5199 | `	sVal = *pValue;   pValue = &sVal;` |
|   80433 |  5200 | `	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|   80433 |  5201 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   80243 |  5202 | `		ph7_value *pFresh = ph7_new_array(pRes->pVm);` |
|   80243 |  5203 | `		if( pFresh == 0 ){` |
|     ! 0 |  5204 | `			return -1;` |
|       - |  5205 | `		}` |
|   80243 |  5206 | `		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){` |
|     ! 0 |  5207 | `			ph7_release_value(pRes->pVm,pFresh);` |
|     ! 0 |  5208 | `			return -1;` |
|       - |  5209 | `		}` |
|   80243 |  5210 | `		ph7_release_value(pRes->pVm,pFresh);` |
|   80243 |  5211 | `		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|   80243 |  5212 | `		if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  5213 | `			return -1;` |
|       - |  5214 | `		}` |
|   40119 |  5215 | `	}` |
|   80433 |  5216 | `	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);` |
|   80433 |  5217 | `	if( pMap == 0 ){` |
|     ! 0 |  5218 | `		return -1;` |
|       - |  5219 | `	}` |
|   80433 |  5220 | `	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;` |
|   40219 |  5221 | `}` |
|       - |  5222 | `/*` |
|       - |  5223 | ` * One wrapper option by name, or 0 when the context does not carry it. This is` |
|       - |  5224 | ` * the read side every consumer (the socket transports) asks through.` |
|       - |  5225 | ` */` |
|    5614 |  5226 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)` |
|       5 |  5227 | `{` |
|       - |  5228 | `	ph7_value *pSub;` |
|    5619 |  5229 | `	if( pRes == 0 \|\| pRes->pOptions == 0 ){` |
|     ! 0 |  5230 | `		return 0;` |
|       - |  5231 | `	}` |
|    5619 |  5232 | `	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);` |
|    5619 |  5233 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    3597 |  5234 | `		return 0;` |
|       - |  5235 | `	}` |
|    2024 |  5236 | `	return ph7_array_fetch(pSub,zOption,-1);` |
|    2812 |  5237 | `}` |
|       - |  5238 | `/*` |
|       - |  5239 | ` * php's parse_context_options: every entry must be wrappername => array, and a` |
|       - |  5240 | ` * non-array value — or an INTEGER key, which has no wrapper name at all — is` |
|       - |  5241 | ` * the ValueError below. An integer key one level DOWN has no option name, and` |
|       - |  5242 | ` * php drops that entry in silence rather than refusing the call.` |
|       - |  5243 | ` * Returns 0, or -1 once the exception has been raised.` |
|       - |  5244 | ` */` |
|   80346 |  5245 | `static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)` |
|       5 |  5246 | `{` |
|       - |  5247 | `	ph7_hashmap *pMap;` |
|       - |  5248 | `	ph7_hashmap_node *pEntry;` |
|   80351 |  5249 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  5250 | `		return 0;` |
|       - |  5251 | `	}` |
|   80351 |  5252 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|   80351 |  5253 | `	pMap->pCur = pMap->pFirst;` |
|  160701 |  5254 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - |  5255 | `		ph7_value sKey;` |
|       - |  5256 | `		ph7_value *pVal;` |
|       - |  5257 | `		int bBad;` |
|   80359 |  5258 | `		PH7_MemObjInit(pRes->pVm,&sKey);` |
|   80359 |  5259 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   80359 |  5260 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  120534 |  5261 | `		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 \|\| pVal == 0` |
|  120530 |  5262 | `		      \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );` |
|   80359 |  5263 | `		if( bBad ){` |
|       5 |  5264 | `			PH7_MemObjRelease(&sKey);` |
|       5 |  5265 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  5266 | `				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");` |
|       5 |  5267 | `			return -1;` |
|       - |  5268 | `		}` |
|       - |  5269 | `		{` |
|   80355 |  5270 | `			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;` |
|       - |  5271 | `			ph7_hashmap_node *pOpt;` |
|   80355 |  5272 | `			pSub->pCur = pSub->pFirst;` |
|  160767 |  5273 | `			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){` |
|       - |  5274 | `				ph7_value sName;` |
|       - |  5275 | `				ph7_value *pOptVal;` |
|   80417 |  5276 | `				PH7_MemObjInit(pRes->pVm,&sName);` |
|   80417 |  5277 | `				PH7_HashmapExtractNodeKey(pOpt,&sName);` |
|   80417 |  5278 | `				pOptVal = HashmapExtractNodeValue(pOpt);` |
|   80417 |  5279 | `				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){` |
|   80415 |  5280 | `					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);` |
|   40205 |  5281 | `				}` |
|   80417 |  5282 | `				PH7_MemObjRelease(&sName);` |
|       5 |  5283 | `			}` |
|       - |  5284 | `		}` |
|   80355 |  5285 | `		PH7_MemObjRelease(&sKey);` |
|       5 |  5286 | `	}` |
|   80347 |  5287 | `	return 0;` |
|   40178 |  5288 | `}` |
|       - |  5289 | `/*` |
|       - |  5290 | `` * php's parse_context_params: only `notification` and `options` are read, and`` |
|       - |  5291 | ` * anything else in the array is ignored rather than refused. The notification` |
|       - |  5292 | ` * must be callable — php reports the same "must be an array with valid` |
|       - |  5293 | ` * callbacks as values" TypeError the callback taxonomy produces, naming` |
|       - |  5294 | ` * argument #1 whichever function was called.` |
|       - |  5295 | ` * Returns 0, or -1 once a diagnostic has been raised.` |
|       - |  5296 | ` */` |
|      76 |  5297 | `static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)` |
|       1 |  5298 | `{` |
|       - |  5299 | `	ph7_value *pVal;` |
|      77 |  5300 | `	if( pParams == 0 \|\| (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  5301 | `		return 0;` |
|       - |  5302 | `	}` |
|      77 |  5303 | `	pVal = ph7_array_fetch(pParams,"notification",-1);` |
|      77 |  5304 | `	if( pVal ){` |
|       - |  5305 | `		char zBuf[128];` |
|      71 |  5306 | `		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|      71 |  5307 | `		if( zReason ){` |
|       9 |  5308 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  5309 | `				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",` |
|       6 |  5310 | `				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;` |
|       - |  5311 | `		}` |
|      67 |  5312 | `		if( pRes->pNotify == 0 ){` |
|      67 |  5313 | `			pRes->pNotify = ph7_new_scalar(pRes->pVm);` |
|      33 |  5314 | `		}` |
|      67 |  5315 | `		if( pRes->pNotify ){` |
|      67 |  5316 | `			PH7_MemObjStore(pVal,pRes->pNotify);` |
|      33 |  5317 | `		}` |
|      33 |  5318 | `	}` |
|      73 |  5319 | `	pVal = ph7_array_fetch(pParams,"options",-1);` |
|      73 |  5320 | `	if( pVal ){` |
|       7 |  5321 | `		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  5322 | `			/* php's own wording for a params entry it cannot use. */` |
|     ! 0 |  5323 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     ! 0 |  5324 | `				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;` |
|       - |  5325 | `		}` |
|       7 |  5326 | `		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){` |
|     ! 0 |  5327 | `			return -1;` |
|       - |  5328 | `		}` |
|       3 |  5329 | `	}` |
|      73 |  5330 | `	return 0;` |
|      39 |  5331 | `}` |
|       - |  5332 | `/*` |
|       - |  5333 | `` * Resolve the `$stream_or_context` first argument every accessor takes: a`` |
|       - |  5334 | ` * context resource answers itself, and a STREAM answers the context it` |
|       - |  5335 | ` * carries — created on demand for the setters, the way php's does, since a` |
|       - |  5336 | ` * stream opened without one still accepts stream_context_set_option().` |
|       - |  5337 | ` * Raises php's TypeError and returns 0 for anything else.` |
|       - |  5338 | ` */` |
|     120 |  5339 | `static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,` |
|       - |  5340 | `	const char *zArgName,int *pbThrew)` |
|       4 |  5341 | `{` |
|       - |  5342 | `	char zGiven[64];` |
|       - |  5343 | `	phl_stream_ctx *pRes;` |
|       - |  5344 | `	io_private *pDev;` |
|     124 |  5345 | `	*pbThrew = 1;` |
|     124 |  5346 | `	if( !ph7_value_is_resource(pVal) ){` |
|       8 |  5347 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  5348 | `			"%s(): Argument #1 (%s) must be of type resource, %s given",` |
|       2 |  5349 | `			ph7_function_name(pCtx),zArgName,VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|       6 |  5350 | `		return 0;` |
|       - |  5351 | `	}` |
|     120 |  5352 | `	pRes = PH7_StreamCtxFromValue(pVal);` |
|     120 |  5353 | `	if( pRes ){` |
|      98 |  5354 | `		*pbThrew = 0;` |
|      98 |  5355 | `		return pRes;` |
|       - |  5356 | `	}` |
|      25 |  5357 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|      25 |  5358 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - |  5359 | `		/* A closed handle, a process handle, anything that is neither: php` |
|       - |  5360 | `		 * refuses the call rather than answering an empty option set. */` |
|       4 |  5361 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  5362 | `			"%s(): Argument #1 (%s) must be a valid stream/context",` |
|       1 |  5363 | `			ph7_function_name(pCtx),zArgName);` |
|       3 |  5364 | `		return 0;` |
|       - |  5365 | `	}` |
|      23 |  5366 | `	*pbThrew = 0;` |
|      23 |  5367 | `	if( pDev->pCtxRes == 0 && bCreate ){` |
|       3 |  5368 | `		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);` |
|       - |  5369 | `		/* The HANDLE holds it, not any value the script has. */` |
|       3 |  5370 | `		StreamCtxHold((phl_stream_ctx *)pDev->pCtxRes);` |
|       1 |  5371 | `	}` |
|      23 |  5372 | `	return (phl_stream_ctx *)pDev->pCtxRes;` |
|      64 |  5373 | `}` |
|       - |  5374 | `/*` |
|       - |  5375 | `` * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body`` |
|       - |  5376 | `` * used to read. php's parameter is `?resource $context = null` and its rules`` |
|       - |  5377 | ` * are: a resource that is NOT a stream-context is refused outright, anything` |
|       - |  5378 | ` * else non-null is the ordinary type refusal, and NULL means the DEFAULT` |
|       - |  5379 | ` * context — which php creates on demand, so an opener never runs without one.` |
|       - |  5380 | ` *` |
|       - |  5381 | ` * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/` |
|       - |  5382 | ` * file_put_contents() carry to mean exactly "and do not fall back to it".` |
|       - |  5383 | ` * Returns 0 with *pbThrew set once a diagnostic has been raised.` |
|       - |  5384 | ` */` |
|   87196 |  5385 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - |  5386 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)` |
|       5 |  5387 | `{` |
|       - |  5388 | `	char zGiven[64];` |
|       - |  5389 | `	phl_stream_ctx *pRes;` |
|   87201 |  5390 | `	*pbThrew = 0;` |
|   87201 |  5391 | `	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){` |
|     360 |  5392 | `		if( !ph7_value_is_resource(apArg[iArg]) ){` |
|      11 |  5393 | `			*pbThrew = 1;` |
|      16 |  5394 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  5395 | `				"%s(): Argument #%d (%s) must be of type resource or null, %s given",` |
|      10 |  5396 | `				ph7_function_name(pCtx),iArg + 1,zArgName,VmValueGivenName(apArg[iArg],zGiven,sizeof(zGiven)));` |
|      11 |  5397 | `			return 0;` |
|       - |  5398 | `		}` |
|     350 |  5399 | `		pRes = PH7_StreamCtxFromValue(apArg[iArg]);` |
|     350 |  5400 | `		if( pRes == 0 ){` |
|       - |  5401 | `			/* php names the RESOURCE it wanted rather than the argument here. */` |
|      34 |  5402 | `			*pbThrew = 1;` |
|      50 |  5403 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  5404 | `				"%s(): supplied resource is not a valid Stream-Context resource",` |
|      16 |  5405 | `				ph7_function_name(pCtx));` |
|      34 |  5406 | `			return 0;` |
|       - |  5407 | `		}` |
|     318 |  5408 | `		return pRes;` |
|       - |  5409 | `	}` |
|   86845 |  5410 | `	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);` |
|   43529 |  5411 | `}` |
|       - |  5412 | `/*` |
|       - |  5413 | ` * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes` |
|       - |  5414 | ` * and clears it, so the slot describes exactly one open and a caller that never` |
|       - |  5415 | ` * set it finds nothing armed.` |
|       - |  5416 | ` */` |
|   33374 |  5417 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)` |
|       5 |  5418 | `{` |
|   33379 |  5419 | `	if( pVm ){` |
|   33379 |  5420 | `		pVm->pOpenCtx = (void *)pRes;` |
|   16650 |  5421 | `	}` |
|   33379 |  5422 | `}` |
|       - |  5423 | `/*` |
|       - |  5424 | ` * php's notification callback, called with the six arguments its documentation` |
|       - |  5425 | `` * names: `(int $code, int $severity, ?string $message, int $message_code,`` |
|       - |  5426 | `` * int $bytes_transferred, int $bytes_max)`. The message is NULL for every`` |
|       - |  5427 | ` * event that carries no text, which is most of them.` |
|       - |  5428 | ` *` |
|       - |  5429 | ` * The six arguments are STACK values, so the call allocates nothing of its own` |
|       - |  5430 | ` * -- the shape ext/curl's callbacks already use, and the one that matters here` |
|       - |  5431 | ` * because the callers are mid-exchange holding pointers into the handle. A` |
|       - |  5432 | ` * refusal raised by the callback unwinds on its own, the way a throwing` |
|       - |  5433 | ` * userland WRAPPER method's does; what this has to do is stop ASKING.` |
|       - |  5434 | ` */` |
|     944 |  5435 | `PH7_PRIVATE void PH7_StreamCtxNotify(phl_stream_ctx *pCtxRes,int iCode,int iSeverity,` |
|       - |  5436 | `	const char *zMsg,int nMsg,int iMsgCode,sxi64 iBytes,sxi64 iBytesMax)` |
|     ! 0 |  5437 | `{` |
|       - |  5438 | `	ph7_value sArgs[6],sRes,*apArg[6];` |
|       - |  5439 | `	ph7_vm *pVm;` |
|       - |  5440 | `	sxi32 rc;` |
|       - |  5441 | `	int i;` |
|     944 |  5442 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->pVm == 0` |
|     334 |  5443 | `	 \|\| pCtxRes->bNotifyDead ){` |
|     614 |  5444 | `		return;` |
|       - |  5445 | `	}` |
|     330 |  5446 | `	pVm = pCtxRes->pVm;` |
|    2310 |  5447 | `	for( i = 0 ; i < 6 ; ++i ){` |
|    1980 |  5448 | `		PH7_MemObjInit(pVm,&sArgs[i]);` |
|    1980 |  5449 | `		apArg[i] = &sArgs[i];` |
|     990 |  5450 | `	}` |
|     330 |  5451 | `	ph7_value_int(&sArgs[0],iCode);` |
|     330 |  5452 | `	ph7_value_int(&sArgs[1],iSeverity);` |
|     330 |  5453 | `	if( zMsg ){` |
|     104 |  5454 | `		ph7_value_string(&sArgs[2],zMsg,nMsg);` |
|      52 |  5455 | `	}else{` |
|     226 |  5456 | `		ph7_value_null(&sArgs[2]);` |
|       - |  5457 | `	}` |
|     330 |  5458 | `	ph7_value_int(&sArgs[3],iMsgCode);` |
|     330 |  5459 | `	ph7_value_int64(&sArgs[4],iBytes);` |
|     330 |  5460 | `	ph7_value_int64(&sArgs[5],iBytesMax);` |
|     330 |  5461 | `	PH7_MemObjInit(pVm,&sRes);` |
|       - |  5462 | `	/* The status in a VARIABLE: PH7_CALLBACK_UNWOUND is a macro that reads its` |
|       - |  5463 | `	 * argument twice, so a call written inside it is MADE twice. */` |
|     330 |  5464 | `	rc = PH7_VmCallUserFunction(pVm,pCtxRes->pNotify,6,apArg,&sRes);` |
|     330 |  5465 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  5466 | `		/* php stops asking once the callback has refused: every notification` |
|       - |  5467 | `		 * after the throw is a userland call that would bail on the pending` |
|       - |  5468 | `		 * exception before it ran, so none of them is ever seen. The refusal` |
|       - |  5469 | `		 * itself unwinds on its own, the way a throwing userland WRAPPER` |
|       - |  5470 | `		 * method's already does. */` |
|       2 |  5471 | `		pCtxRes->bNotifyDead = 1;` |
|       1 |  5472 | `	}` |
|     330 |  5473 | `	PH7_MemObjRelease(&sRes);` |
|    2310 |  5474 | `	for( i = 0 ; i < 6 ; ++i ){` |
|    1980 |  5475 | `		PH7_MemObjRelease(&sArgs[i]);` |
|     990 |  5476 | `	}` |
|     472 |  5477 | `}` |
|       - |  5478 | `/*` |
|       - |  5479 | ` * php's php_stream_notify_progress_init: the counter starts again at zero with` |
|       - |  5480 | ` * a new maximum, the notifier is ARMED (it never disarms), and the zero itself` |
|       - |  5481 | ` * is reported. A wrapper calls this once it knows how big the body claims to` |
|       - |  5482 | ` * be -- which is why every PROGRESS before it belongs to the exchange BEFORE it.` |
|       - |  5483 | ` */` |
|     274 |  5484 | `PH7_PRIVATE void PH7_StreamCtxProgressInit(phl_stream_ctx *pCtxRes,sxi64 iMax)` |
|     ! 0 |  5485 | `{` |
|     274 |  5486 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->bNotifyDead ){` |
|     218 |  5487 | `		return;` |
|       - |  5488 | `	}` |
|      56 |  5489 | `	pCtxRes->iProgress = 0;` |
|      56 |  5490 | `	pCtxRes->iProgressMax = iMax;` |
|      56 |  5491 | `	pCtxRes->bProgress = 1;` |
|      84 |  5492 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_PROGRESS,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      28 |  5493 | `		0,0,0,0,iMax);` |
|     137 |  5494 | `}` |
|       - |  5495 | `/*` |
|       - |  5496 | ` * One transfer step: php counts every byte the underlying stream moves, in` |
|       - |  5497 | ` * EITHER direction, and reports the running total each time. Nothing is` |
|       - |  5498 | ` * reported (and nothing counted) before an init has armed the notifier.` |
|       - |  5499 | ` */` |
|     888 |  5500 | `PH7_PRIVATE void PH7_StreamCtxProgressAdd(phl_stream_ctx *pCtxRes,sxi64 nDelta)` |
|     ! 0 |  5501 | `{` |
|     888 |  5502 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 \|\| pCtxRes->bNotifyDead` |
|     197 |  5503 | `	 \|\| !pCtxRes->bProgress \|\| nDelta <= 0 ){` |
|     830 |  5504 | `		return;` |
|       - |  5505 | `	}` |
|      58 |  5506 | `	pCtxRes->iProgress += nDelta;` |
|      87 |  5507 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_PROGRESS,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      29 |  5508 | `		0,0,0,pCtxRes->iProgress,pCtxRes->iProgressMax);` |
|     444 |  5509 | `}` |
|       - |  5510 | `/*` |
|       - |  5511 | ` * php's php_stream_notify_completed, which is NOT gated on the progress mask:` |
|       - |  5512 | ` * a read that comes back with nothing is the end of the transfer whether or` |
|       - |  5513 | ` * not anything armed the counter, so a context reused for an exchange that` |
|       - |  5514 | ` * ends where a status line was due reports it before the failure.` |
|       - |  5515 | ` */` |
|     196 |  5516 | `PH7_PRIVATE void PH7_StreamCtxCompleted(phl_stream_ctx *pCtxRes)` |
|     ! 0 |  5517 | `{` |
|     196 |  5518 | `	if( pCtxRes == 0 \|\| pCtxRes->pNotify == 0 ){` |
|     144 |  5519 | `		return;` |
|       - |  5520 | `	}` |
|      78 |  5521 | `	PH7_StreamCtxNotify(pCtxRes,PHL_STREAM_NOTIFY_COMPLETED,PHL_STREAM_NOTIFY_SEVERITY_INFO,` |
|      26 |  5522 | `		0,0,0,pCtxRes->iProgress,pCtxRes->iProgressMax);` |
|      98 |  5523 | `}` |
|       - |  5524 | `/*` |
|       - |  5525 | ` * resource stream_context_create(?array $options = null, ?array $params = null)` |
|       - |  5526 | ` */` |
|   80344 |  5527 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  5528 | `{` |
|   80349 |  5529 | `	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);` |
|   80349 |  5530 | `	if( pRes == 0 ){` |
|     ! 0 |  5531 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5532 | `		return PH7_OK;` |
|       - |  5533 | `	}` |
|   80349 |  5534 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|   80332 |  5535 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|       5 |  5536 | `			return PH7_OK;` |
|       - |  5537 | `		}` |
|   40162 |  5538 | `	}` |
|   80345 |  5539 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|       - |  5540 | ``		/* php names argument #1 ($options) even for a bad `notification` that`` |
|       - |  5541 | `		 * arrived through $params — the error is raised against a hardcoded` |
|       - |  5542 | `		 * position, and a test that asserts the message would see it. */` |
|      55 |  5543 | `		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){` |
|       5 |  5544 | `			return PH7_OK;` |
|       - |  5545 | `		}` |
|      25 |  5546 | `	}` |
|   80341 |  5547 | `	ph7_result_resource(pCtx,pRes);` |
|   80341 |  5548 | `	return PH7_OK;` |
|   40177 |  5549 | `}` |
|       - |  5550 | `/*` |
|       - |  5551 | ` * array stream_context_get_options(resource $stream_or_context)` |
|       - |  5552 | ` */` |
|      70 |  5553 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  5554 | `{` |
|       - |  5555 | `	phl_stream_ctx *pRes;` |
|       - |  5556 | `	int bThrew;` |
|      74 |  5557 | `	if( nArg < 1 ){` |
|     ! 0 |  5558 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5559 | `		return PH7_OK;` |
|       - |  5560 | `	}` |
|       - |  5561 | `	/* A live stream that was never given a context answers the EMPTY option set` |
|       - |  5562 | `	 * rather than refusing the call, so nothing is created here. */` |
|      74 |  5563 | `	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);` |
|      74 |  5564 | `	if( bThrew ){` |
|       5 |  5565 | `		return PH7_OK;` |
|       - |  5566 | `	}` |
|      70 |  5567 | `	if( pRes == 0 ){` |
|      11 |  5568 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|      11 |  5569 | `		if( pArr == 0 ){` |
|     ! 0 |  5570 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  5571 | `			return PH7_OK;` |
|       - |  5572 | `		}` |
|      11 |  5573 | `		ph7_result_value(pCtx,pArr);` |
|      11 |  5574 | `		return PH7_OK;` |
|       - |  5575 | `	}` |
|      62 |  5576 | `	ph7_result_value(pCtx,pRes->pOptions);` |
|      62 |  5577 | `	return PH7_OK;` |
|      39 |  5578 | `}` |
|       - |  5579 | `/*` |
|       - |  5580 | ` * true stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)` |
|       - |  5581 | ` *` |
|       - |  5582 | ` * php also accepts the two-argument (context, options-array) spelling and` |
|       - |  5583 | ` * DEPRECATES it in 8.3 — the scope policy refuses what php deprecates, so an array in` |
|       - |  5584 | ` * argument #2 is the ordinary string TypeError here and the whole-array form` |
|       - |  5585 | ` * is spelled stream_context_set_options().` |
|       - |  5586 | ` */` |
|      12 |  5587 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  5588 | `{` |
|       - |  5589 | `	phl_stream_ctx *pRes;` |
|       - |  5590 | `	int bThrew;` |
|      15 |  5591 | `	if( nArg < 4 ){` |
|     ! 0 |  5592 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5593 | `		return PH7_OK;` |
|       - |  5594 | `	}` |
|      15 |  5595 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      15 |  5596 | `	if( pRes == 0 ){` |
|     ! 0 |  5597 | `		return PH7_OK;` |
|       - |  5598 | `	}` |
|      15 |  5599 | `	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);` |
|      15 |  5600 | `	return PH7_OK;` |
|       9 |  5601 | `}` |
|       - |  5602 | `/*` |
|       - |  5603 | ` * true stream_context_set_options(resource $context, array $options)` |
|       - |  5604 | ` */` |
|       6 |  5605 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  5606 | `{` |
|       - |  5607 | `	phl_stream_ctx *pRes;` |
|       - |  5608 | `	int bThrew;` |
|       8 |  5609 | `	if( nArg < 2 ){` |
|     ! 0 |  5610 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5611 | `		return PH7_OK;` |
|       - |  5612 | `	}` |
|       8 |  5613 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|       8 |  5614 | `	if( pRes == 0 ){` |
|     ! 0 |  5615 | `		return PH7_OK;` |
|       - |  5616 | `	}` |
|       8 |  5617 | `	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){` |
|     ! 0 |  5618 | `		return PH7_OK;` |
|       - |  5619 | `	}` |
|       8 |  5620 | `	ph7_result_bool(pCtx,1);` |
|       8 |  5621 | `	return PH7_OK;` |
|       5 |  5622 | `}` |
|       - |  5623 | `/*` |
|       - |  5624 | ` * array stream_context_get_params(resource $stream_or_context)` |
|       - |  5625 | `` *  php answers `notification` (only when one is set) and `options`, in that`` |
|       - |  5626 | ` *  order.` |
|       - |  5627 | ` */` |
|      10 |  5628 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  5629 | `{` |
|       - |  5630 | `	phl_stream_ctx *pRes;` |
|       - |  5631 | `	ph7_value *pArr;` |
|       - |  5632 | `	int bThrew;` |
|      12 |  5633 | `	if( nArg < 1 ){` |
|     ! 0 |  5634 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5635 | `		return PH7_OK;` |
|       - |  5636 | `	}` |
|      12 |  5637 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      12 |  5638 | `	if( pRes == 0 ){` |
|       3 |  5639 | `		return PH7_OK;` |
|       - |  5640 | `	}` |
|       9 |  5641 | `	pArr = ph7_context_new_array(pCtx);` |
|       9 |  5642 | `	if( pArr == 0 ){` |
|     ! 0 |  5643 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5644 | `		return PH7_OK;` |
|       - |  5645 | `	}` |
|       9 |  5646 | `	if( pRes->pNotify ){` |
|       5 |  5647 | `		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);` |
|       2 |  5648 | `	}` |
|       9 |  5649 | `	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);` |
|       9 |  5650 | `	ph7_result_value(pCtx,pArr);` |
|       9 |  5651 | `	return PH7_OK;` |
|       7 |  5652 | `}` |
|       - |  5653 | `/*` |
|       - |  5654 | ` * true stream_context_set_params(resource $context, array $params)` |
|       - |  5655 | ` */` |
|      22 |  5656 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  5657 | `{` |
|       - |  5658 | `	phl_stream_ctx *pRes;` |
|       - |  5659 | `	int bThrew;` |
|      23 |  5660 | `	if( nArg < 2 ){` |
|     ! 0 |  5661 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5662 | `		return PH7_OK;` |
|       - |  5663 | `	}` |
|      23 |  5664 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      23 |  5665 | `	if( pRes == 0 ){` |
|     ! 0 |  5666 | `		return PH7_OK;` |
|       - |  5667 | `	}` |
|      23 |  5668 | `	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){` |
|     ! 0 |  5669 | `		return PH7_OK;` |
|       - |  5670 | `	}` |
|      23 |  5671 | `	ph7_result_bool(pCtx,1);` |
|      23 |  5672 | `	return PH7_OK;` |
|      12 |  5673 | `}` |
|       - |  5674 | `/*` |
|       - |  5675 | ` * resource stream_context_get_default(?array $options = null)` |
|       - |  5676 | ` * resource stream_context_set_default(array $options)` |
|       - |  5677 | ` *  Both answer the ONE default context and both MERGE their options into it —` |
|       - |  5678 | ` *  set_default is not a replacement, which is why a second call adds to what` |
|       - |  5679 | ` *  the first left.` |
|       - |  5680 | ` */` |
|      18 |  5681 | `static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  5682 | `{` |
|      21 |  5683 | `	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);` |
|      21 |  5684 | `	if( pRes == 0 ){` |
|     ! 0 |  5685 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  5686 | `		return PH7_OK;` |
|       - |  5687 | `	}` |
|      21 |  5688 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|       8 |  5689 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|     ! 0 |  5690 | `			return PH7_OK;` |
|       - |  5691 | `		}` |
|       3 |  5692 | `	}` |
|      21 |  5693 | `	ph7_result_resource(pCtx,pRes);` |
|      21 |  5694 | `	return PH7_OK;` |
|      12 |  5695 | `}` |
|      14 |  5696 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  5697 | `{` |
|      16 |  5698 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|       2 |  5699 | `}` |
|       4 |  5700 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  5701 | `{` |
|       6 |  5702 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|       2 |  5703 | `}` |
|       - |  5704 | `/*` |
|       - |  5705 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|       - |  5706 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|       - |  5707 | ` */` |
|       - |  5708 | `#ifdef PH7_ENABLE_NET` |
|       - |  5709 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - |  5710 | `/*` |
|       - |  5711 | ` * ------------------------------------------------------------------------` |
|       - |  5712 | ` * The ssl:// and tls:// transports` |
|       - |  5713 | ` * ------------------------------------------------------------------------` |
|       - |  5714 | ` *` |
|       - |  5715 | ` * php's crypto METHOD is a bitmask of its own (see the STREAM_CRYPTO_METHOD_*` |
|       - |  5716 | ` * constants): bit 0 says client, and one bit per protocol above it. OpenSSL` |
|       - |  5717 | ` * has no such mask -- it takes a MINIMUM and a MAXIMUM version on the context` |
|       - |  5718 | ` * -- so the set is turned into that pair here. A method naming no protocol` |
|       - |  5719 | ` * OpenSSL 3 will negotiate (the SSLv2 and SSLv3 bits alone) leaves the pair` |
|       - |  5720 | ` * unset, and the handshake refuses on its own terms rather than silently` |
|       - |  5721 | ` * speaking TLS the caller did not ask for.` |
|       - |  5722 | ` */` |
|       - |  5723 | `#define SOCK_CRYPTO_CLIENT   0x01` |
|       - |  5724 | `#define SOCK_CRYPTO_SSLv2    0x02` |
|       - |  5725 | `#define SOCK_CRYPTO_SSLv3    0x04` |
|       - |  5726 | `#define SOCK_CRYPTO_TLSv1_0  0x08` |
|       - |  5727 | `#define SOCK_CRYPTO_TLSv1_1  0x10` |
|       - |  5728 | `#define SOCK_CRYPTO_TLSv1_2  0x20` |
|       - |  5729 | `#define SOCK_CRYPTO_TLSv1_3  0x40` |
|       - |  5730 | `/* php's STREAM_CRYPTO_METHOD_TLS_CLIENT: every TLS version, client side. */` |
|       - |  5731 | `#define SOCK_CRYPTO_TLS_CLIENT \` |
|       - |  5732 | `	(SOCK_CRYPTO_CLIENT\|SOCK_CRYPTO_TLSv1_0\|SOCK_CRYPTO_TLSv1_1 \` |
|       - |  5733 | `	 \|SOCK_CRYPTO_TLSv1_2\|SOCK_CRYPTO_TLSv1_3)` |
|       - |  5734 | `/*` |
|       - |  5735 | ` * The method an ADDRESS names. php registers one transport per protocol` |
|       - |  5736 | `` * pin, so `tlsv1.2://` is not `tls://` with an option -- it is a different`` |
|       - |  5737 | `` * transport whose method is that one version. `ssl://` and `tls://` are the`` |
|       - |  5738 | `` * same any-TLS method in php 8: the `ssl` name kept its spelling and lost its`` |
|       - |  5739 | ` * protocol long ago. Answers 0 for a name that is not a crypto transport at` |
|       - |  5740 | ` * all, so the caller can tell "plain tcp" from "TLS pinned to 1.0".` |
|       - |  5741 | ` */` |
|      88 |  5742 | `static int SockCryptoTransport(const char *zName,int nName)` |
|       3 |  5743 | `{` |
|      91 |  5744 | `	struct { const char *zName; int nName; int iMethod; } aXport[] = {` |
|       - |  5745 | `		{ "ssl",     3, SOCK_CRYPTO_TLS_CLIENT },` |
|       - |  5746 | `		{ "tls",     3, SOCK_CRYPTO_TLS_CLIENT },` |
|       - |  5747 | `		{ "tlsv1.0", 7, SOCK_CRYPTO_CLIENT\|SOCK_CRYPTO_TLSv1_0 },` |
|       - |  5748 | `		{ "tlsv1.1", 7, SOCK_CRYPTO_CLIENT\|SOCK_CRYPTO_TLSv1_1 },` |
|       - |  5749 | `		{ "tlsv1.2", 7, SOCK_CRYPTO_CLIENT\|SOCK_CRYPTO_TLSv1_2 },` |
|       - |  5750 | `		{ "tlsv1.3", 7, SOCK_CRYPTO_CLIENT\|SOCK_CRYPTO_TLSv1_3 }` |
|       - |  5751 | `	};` |
|       - |  5752 | `	int i;` |
|     245 |  5753 | `	for( i = 0 ; i < (int)(sizeof(aXport)/sizeof(aXport[0])) ; i++ ){` |
|     232 |  5754 | `		if( nName == aXport[i].nName` |
|     205 |  5755 | `		 && SyStrncmp(zName,aXport[i].zName,(sxu32)nName) == 0 ){` |
|      79 |  5756 | `			return aXport[i].iMethod;` |
|       - |  5757 | `		}` |
|      80 |  5758 | `	}` |
|      12 |  5759 | `	return 0;` |
|      47 |  5760 | `}` |
|       - |  5761 | ``/* The context's `ssl` options, each answered as php's own default when the`` |
|       - |  5762 | ` * script named none. php's defaults are not OpenSSL's: verify_peer and` |
|       - |  5763 | ` * verify_peer_name are ON, which is the whole reason a self-signed peer needs` |
|       - |  5764 | ` * a context at all. */` |
|     468 |  5765 | `static int SockSslOptBool(phl_stream_ctx *pCtxRes,const char *zName,int bDefault)` |
|     ! 0 |  5766 | `{` |
|     468 |  5767 | `	ph7_value *pVal = pCtxRes ? PH7_StreamCtxOption(pCtxRes,"ssl",zName) : 0;` |
|     468 |  5768 | `	if( pVal == 0 ){` |
|     224 |  5769 | `		return bDefault;` |
|       - |  5770 | `	}` |
|     244 |  5771 | `	return ph7_value_to_bool(pVal) ? 1 : 0;` |
|     234 |  5772 | `}` |
|     344 |  5773 | `static const char * SockSslOptStr(phl_stream_ctx *pCtxRes,const char *zName,SyBlob *pOut)` |
|     ! 0 |  5774 | `{` |
|     344 |  5775 | `	ph7_value *pVal = pCtxRes ? PH7_StreamCtxOption(pCtxRes,"ssl",zName) : 0;` |
|       - |  5776 | `	const char *z;` |
|     344 |  5777 | `	int n = 0;` |
|     344 |  5778 | `	if( pVal == 0 \|\| ph7_value_is_null(pVal) ){` |
|     336 |  5779 | `		return 0;` |
|       - |  5780 | `	}` |
|       8 |  5781 | `	z = ph7_value_to_string(pVal,&n);` |
|       8 |  5782 | `	if( n < 1 ){` |
|     ! 0 |  5783 | `		return 0;` |
|       - |  5784 | `	}` |
|       - |  5785 | `	/* Every libssl door below takes a C string, and a VM blob is not one --` |
|       - |  5786 | `	 * the same NUL rule the plain-file opener records. */` |
|       8 |  5787 | `	SyBlobReset(pOut);` |
|       8 |  5788 | `	if( SyBlobAppend(pOut,z,(sxu32)n) != SXRET_OK` |
|       8 |  5789 | `	 \|\| SyBlobNullAppend(pOut) != SXRET_OK ){` |
|     ! 0 |  5790 | `		return 0;` |
|       - |  5791 | `	}` |
|       8 |  5792 | `	return (const char *)SyBlobData(pOut);` |
|     172 |  5793 | `}` |
|       - |  5794 | `/*` |
|       - |  5795 | ` * php's error text for a failed TLS HANDSHAKE: the sentence, then -- only when` |
|       - |  5796 | `` * OpenSSL queued anything at all -- a single `OpenSSL Error messages:` heading`` |
|       - |  5797 | ` * with the queued lines under it, newline-separated. php drains the whole queue` |
|       - |  5798 | ` * under ONE heading and puts nothing between the sentence and it; a handshake` |
|       - |  5799 | ` * that failed for a reason OpenSSL did not queue (the peer simply closed)` |
|       - |  5800 | ` * carries the sentence alone.` |
|       - |  5801 | ` *` |
|       - |  5802 | ` * A context-SETUP failure is not this shape at all. php names the file it could` |
|       - |  5803 | ` * not read in a sentence of its own and drops the queue unread, so the callers` |
|       - |  5804 | ` * below format their own text and clear the queue rather than coming here --` |
|       - |  5805 | ` * otherwise the next handshake's message inherits a stale error.` |
|       - |  5806 | ` */` |
|       6 |  5807 | `static void SockSslErrorText(const char *zWhat,char *zBuf,int nBuf)` |
|     ! 0 |  5808 | `{` |
|       - |  5809 | `	char zErr[256];` |
|       - |  5810 | `	unsigned long uErr;` |
|       6 |  5811 | `	int n,bHead = 0;` |
|       6 |  5812 | `	n = (int)SyBufferFormat(zBuf,(sxu32)nBuf,"%s",zWhat);` |
|       9 |  5813 | `	while( (uErr = ERR_get_error()) != 0 ){` |
|       3 |  5814 | `		ERR_error_string_n(uErr,zErr,sizeof(zErr));` |
|       3 |  5815 | `		if( n + (int)SyStrlen(zErr) + 32 >= nBuf ){` |
|     ! 0 |  5816 | `			break;` |
|       - |  5817 | `		}` |
|       4 |  5818 | `		n += (int)SyBufferFormat(&zBuf[n],(sxu32)(nBuf - n),"%s%s",` |
|       1 |  5819 | `			bHead ? "\n" : "OpenSSL Error messages:\n",zErr);` |
|       3 |  5820 | `		bHead = 1;` |
|     ! 0 |  5821 | `	}` |
|       6 |  5822 | `}` |
|       - |  5823 | `/*` |
|       - |  5824 | `` * php's `capture_peer_cert` and `capture_peer_cert_chain`: the peer's`` |
|       - |  5825 | ``  * certificate -- and the chain it arrived in -- written BACK into the `ssl` `` |
|       - |  5826 | ` * options of the CONTEXT the handshake read, as the OpenSSLCertificate objects` |
|       - |  5827 | ` * openssl_x509_parse() and openssl_x509_fingerprint() already take.` |
|       - |  5828 | ` *` |
|       - |  5829 | ` * That the store is the context and not the handle is observable twice over: a` |
|       - |  5830 | ` * context dialled a second time answers the SECOND peer's certificate, and the` |
|       - |  5831 | ` * keys are there even after the fingerprint check below refused the very peer` |
|       - |  5832 | ` * they describe. Nothing is written when the option is absent or false, and` |
|       - |  5833 | ` * nothing is written when the peer sent no certificate at all -- a tls://` |
|       - |  5834 | `` * listener that asked its client for none carries `capture_peer_cert` on its`` |
|       - |  5835 | `` * context with no `peer_certificate` beside it.`` |
|       - |  5836 | ` */` |
|       6 |  5837 | `static void SockSslStoreOption(phl_stream_ctx *pCtxRes,const char *zName,ph7_value *pVal)` |
|     ! 0 |  5838 | `{` |
|       - |  5839 | `	ph7_value sWrap,sName;` |
|       6 |  5840 | `	if( pCtxRes == 0 \|\| pCtxRes->pOptions == 0 \|\| pVal == 0 ){` |
|     ! 0 |  5841 | `		return;` |
|       - |  5842 | `	}` |
|       6 |  5843 | `	PH7_MemObjInit(pCtxRes->pVm,&sWrap);` |
|       6 |  5844 | `	PH7_MemObjInit(pCtxRes->pVm,&sName);` |
|       6 |  5845 | `	ph7_value_string(&sWrap,"ssl",sizeof("ssl")-1);` |
|       6 |  5846 | `	ph7_value_string(&sName,zName,-1);` |
|       6 |  5847 | `	StreamCtxSetOption(pCtxRes,&sWrap,&sName,pVal);` |
|       6 |  5848 | `	PH7_MemObjRelease(&sName);` |
|       6 |  5849 | `	PH7_MemObjRelease(&sWrap);` |
|       3 |  5850 | `}` |
|       - |  5851 | `/*` |
|       - |  5852 | ` * One X509 parked in pOut as php's OpenSSLCertificate. The certificate is` |
|       - |  5853 | ` * CONSUMED -- freed here when the object cannot be built -- which is the same` |
|       - |  5854 | ` * ownership rule ext/openssl's own result door states. Answers 0 on success.` |
|       - |  5855 | ` */` |
|       6 |  5856 | `static int SockSslCertValue(ph7_vm *pVm,X509 *pCert,ph7_value *pOut)` |
|     ! 0 |  5857 | `{` |
|       6 |  5858 | `	ph7_class_instance *pInst = 0;` |
|       6 |  5859 | `	if( pCert == 0 ){` |
|     ! 0 |  5860 | `		return -1;` |
|       - |  5861 | `	}` |
|       6 |  5862 | `	if( PH7_SslNewObject(pVm,PHL_SSL_KIND_CERT,(void *)pCert,&pInst) == 0 \|\| pInst == 0 ){` |
|     ! 0 |  5863 | `		X509_free(pCert);` |
|     ! 0 |  5864 | `		return -1;` |
|       - |  5865 | `	}` |
|       6 |  5866 | `	PH7_MemObjRelease(pOut);` |
|       6 |  5867 | `	pOut->x.pOther = (void *)pInst;` |
|       6 |  5868 | `	MemObjSetType(pOut,MEMOBJ_OBJ);` |
|       6 |  5869 | `	return 0;` |
|       3 |  5870 | `}` |
|       - |  5871 | `/*` |
|       - |  5872 | ` * Drop the value above once the array or the option table it was handed to has` |
|       - |  5873 | ` * taken its own reference. The temporary BORROWS the instance -- it never took` |
|       - |  5874 | ` * a reference of its own -- so it is blanked rather than released, and the` |
|       - |  5875 | ` * CREATION reference is the one dropped here.` |
|       - |  5876 | ` */` |
|       6 |  5877 | `static void SockSslCertValueDrop(ph7_value *pVal)` |
|     ! 0 |  5878 | `{` |
|       6 |  5879 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       6 |  5880 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pVal->x.pOther);` |
|       6 |  5881 | `		pVal->x.pOther = 0;` |
|       6 |  5882 | `		pVal->iFlags = MEMOBJ_NULL;` |
|       3 |  5883 | `	}` |
|       6 |  5884 | `}` |
|      76 |  5885 | `static void SockSslCapture(ph7_vm *pVm,SSL *pSsl,phl_stream_ctx *pCtxRes)` |
|     ! 0 |  5886 | `{` |
|       - |  5887 | `	ph7_value sVal;` |
|      76 |  5888 | `	if( pCtxRes == 0 ){` |
|     ! 0 |  5889 | `		return;` |
|       - |  5890 | `	}` |
|      76 |  5891 | `	PH7_MemObjInit(pVm,&sVal);` |
|      76 |  5892 | `	if( SockSslOptBool(pCtxRes,"capture_peer_cert",0) ){` |
|       4 |  5893 | `		X509 *pPeer = SSL_get1_peer_certificate(pSsl);` |
|       4 |  5894 | `		if( pPeer && SockSslCertValue(pVm,pPeer,&sVal) == 0 ){` |
|       4 |  5895 | `			SockSslStoreOption(pCtxRes,"peer_certificate",&sVal);` |
|       4 |  5896 | `			SockSslCertValueDrop(&sVal);` |
|       2 |  5897 | `		}` |
|       2 |  5898 | `	}` |
|      76 |  5899 | `	if( SockSslOptBool(pCtxRes,"capture_peer_cert_chain",0) ){` |
|       2 |  5900 | `		STACK_OF(X509) *pChain = SSL_get_peer_cert_chain(pSsl);` |
|       2 |  5901 | `		int i,nCert = pChain ? sk_X509_num(pChain) : 0;` |
|       2 |  5902 | `		if( nCert > 0 ){` |
|       2 |  5903 | `			ph7_value *pArr = ph7_new_array(pVm);` |
|       2 |  5904 | `			if( pArr ){` |
|       4 |  5905 | `				for( i = 0 ; i < nCert ; ++i ){` |
|       2 |  5906 | `					X509 *pOne = sk_X509_value(pChain,i);` |
|       - |  5907 | `					/* The stack belongs to the SESSION, and the script's` |
|       - |  5908 | `					 * certificates outlive it, so each entry is duplicated` |
|       - |  5909 | `					 * rather than parked. */` |
|       2 |  5910 | `					if( pOne == 0 \|\| SockSslCertValue(pVm,X509_dup(pOne),&sVal) != 0 ){` |
|     ! 0 |  5911 | `						continue;` |
|       - |  5912 | `					}` |
|       2 |  5913 | `					ph7_array_add_elem(pArr,0,&sVal);` |
|       2 |  5914 | `					SockSslCertValueDrop(&sVal);` |
|       1 |  5915 | `				}` |
|       2 |  5916 | `				SockSslStoreOption(pCtxRes,"peer_certificate_chain",pArr);` |
|       2 |  5917 | `				ph7_release_value(pVm,pArr);` |
|       1 |  5918 | `			}` |
|       1 |  5919 | `		}` |
|       1 |  5920 | `	}` |
|      76 |  5921 | `	PH7_MemObjRelease(&sVal);` |
|      38 |  5922 | `}` |
|       - |  5923 | `/* Compare a raw digest with the hex a script pinned. php's comparison is` |
|       - |  5924 | ` * case-INSENSITIVE: an uppercase fingerprint matches. */` |
|      26 |  5925 | `static int SockSslHexEq(const unsigned char *aMd,unsigned int nMd,const char *zWant,int nWant)` |
|     ! 0 |  5926 | `{` |
|       - |  5927 | `	static const char zDigit[] = "0123456789abcdef";` |
|       - |  5928 | `	unsigned int i;` |
|      26 |  5929 | `	if( zWant == 0 \|\| (int)(nMd * 2) != nWant ){` |
|     ! 0 |  5930 | `		return 0;` |
|       - |  5931 | `	}` |
|     450 |  5932 | `	for( i = 0 ; i < nMd ; ++i ){` |
|     432 |  5933 | `		char c0 = zWant[i * 2],c1 = zWant[i * 2 + 1];` |
|     432 |  5934 | `		if( c0 >= 'A' && c0 <= 'F' ){ c0 = (char)(c0 - 'A' + 'a'); }` |
|     432 |  5935 | `		if( c1 >= 'A' && c1 <= 'F' ){ c1 = (char)(c1 - 'A' + 'a'); }` |
|     432 |  5936 | `		if( c0 != zDigit[(aMd[i] >> 4) & 0x0F] \|\| c1 != zDigit[aMd[i] & 0x0F] ){` |
|       8 |  5937 | `			return 0;` |
|       - |  5938 | `		}` |
|     212 |  5939 | `	}` |
|      18 |  5940 | `	return 1;` |
|      13 |  5941 | `}` |
|       - |  5942 | `/* Digest pCert under the named algorithm and compare. 1 on a match, 0 on a` |
|       - |  5943 | ` * mismatch, and -1 when the name is not a digest at all -- php's own` |
|       - |  5944 | `` * `Unknown digest algorithm`, which the caller raises. The lookup is OpenSSL's`` |
|       - |  5945 | `` * legacy name table, the one php asks, so `SHA1` and `sha1` are one name. */`` |
|      34 |  5946 | `static int SockSslFpOne(X509 *pCert,const char *zAlgo,const char *zWant,int nWant)` |
|     ! 0 |  5947 | `{` |
|       - |  5948 | `	const EVP_MD *pMd;` |
|       - |  5949 | `	unsigned char aMd[EVP_MAX_MD_SIZE];` |
|      34 |  5950 | `	unsigned int nMd = 0;` |
|      34 |  5951 | `	if( zAlgo == 0 \|\| (pMd = EVP_get_digestbyname(zAlgo)) == 0 ){` |
|       8 |  5952 | `		return -1;` |
|       - |  5953 | `	}` |
|      26 |  5954 | `	if( X509_digest(pCert,pMd,aMd,&nMd) != 1 ){` |
|     ! 0 |  5955 | `		return 0;` |
|       - |  5956 | `	}` |
|      26 |  5957 | `	return SockSslHexEq(aMd,nMd,zWant,nWant);` |
|      17 |  5958 | `}` |
|       - |  5959 | `/* The bytes of a value that already IS a string, without converting it. */` |
|      62 |  5960 | `static const char * SockSslStrOf(ph7_value *pVal,int *pnByte)` |
|     ! 0 |  5961 | `{` |
|      62 |  5962 | `	*pnByte = 0;` |
|      62 |  5963 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       4 |  5964 | `		return 0;` |
|       - |  5965 | `	}` |
|      58 |  5966 | `	return ph7_value_to_string(pVal,pnByte);` |
|      31 |  5967 | `}` |
|       - |  5968 | `/*` |
|       - |  5969 | `` * php's `peer_fingerprint`. It is a verification step of php's own rather than`` |
|       - |  5970 | ` * an OpenSSL setting: the peer's certificate is digested here and compared` |
|       - |  5971 | ` * with what the script pinned, and a mismatch is php's refusal.` |
|       - |  5972 | ` *` |
|       - |  5973 | ` * The option's SHAPE picks the algorithm:` |
|       - |  5974 | ` *` |
|       - |  5975 | ` *   a STRING is the digest alone, and its LENGTH names the algorithm -- 32 hex` |
|       - |  5976 | ` *     characters is md5, 40 is sha1, and no other length names anything. A` |
|       - |  5977 | ` *     sha256 hex string is 64 characters, so it is a plain MISMATCH and never` |
|       - |  5978 | `` *     an `Unknown digest algorithm`; php's string form cannot express sha256.`` |
|       - |  5979 | ` *   an ARRAY is [algo => hex] and EVERY entry must match -- one wrong digest` |
|       - |  5980 | ` *     beside a right one refuses, whichever order the two are written in. A` |
|       - |  5981 | ` *     non-string key or value, and an array with no entries at all, are the` |
|       - |  5982 | ` *     shape complaint instead.` |
|       - |  5983 | ` *   anything ELSE is refused before the session is asked for anything, and` |
|       - |  5984 | ` *     that refusal is the only sentence: no match failure follows it.` |
|       - |  5985 | ` *` |
|       - |  5986 | `` * Answers 0 when the peer passed (`nothing was pinned` included), or -1 with`` |
|       - |  5987 | ` * php's sentence in zErr. The two SHAPE complaints are warnings of their own,` |
|       - |  5988 | ` * raised here ahead of the sentence exactly as php raises them.` |
|       - |  5989 | ` */` |
|      76 |  5990 | `static int SockSslCheckFingerprint(ph7_context *pCtx,SSL *pSsl,phl_stream_ctx *pCtxRes,` |
|       - |  5991 | `	char *zErr,int nErr)` |
|     ! 0 |  5992 | `{` |
|      76 |  5993 | `	ph7_value *pWant = pCtxRes ? PH7_StreamCtxOption(pCtxRes,"ssl","peer_fingerprint") : 0;` |
|       - |  5994 | `	X509 *pCert;` |
|      76 |  5995 | `	int bOk = 0;` |
|      76 |  5996 | `	if( pWant == 0 ){` |
|      34 |  5997 | `		return 0;` |
|       - |  5998 | `	}` |
|      42 |  5999 | `	if( (pWant->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|       6 |  6000 | `		SyBufferFormat(zErr,(sxu32)nErr,` |
|       - |  6001 | `			"Expected peer fingerprint must be a string or an array");` |
|       6 |  6002 | `		return -1;` |
|       - |  6003 | `	}` |
|       - |  6004 | `	/* Every shape below needs the peer's certificate, and a session that` |
|       - |  6005 | `	 * carries none -- a listener whose client was never asked for one -- is` |
|       - |  6006 | ``	 * php's `Could not get peer certificate` rather than a mismatch. */`` |
|      36 |  6007 | `	pCert = SSL_get1_peer_certificate(pSsl);` |
|      36 |  6008 | `	if( pCert == 0 ){` |
|       2 |  6009 | `		SyBufferFormat(zErr,(sxu32)nErr,"Could not get peer certificate");` |
|       2 |  6010 | `		return -1;` |
|       - |  6011 | `	}` |
|      34 |  6012 | `	if( pWant->iFlags & MEMOBJ_HASHMAP ){` |
|      20 |  6013 | `		ph7_hashmap *pMap = (ph7_hashmap *)pWant->x.pOther;` |
|       - |  6014 | `		ph7_hashmap_node *pEntry;` |
|      20 |  6015 | `		const char *zBad = "Invalid peer_fingerprint array; [algo => fingerprint] form required";` |
|      20 |  6016 | `		const char *zWarn = 0;` |
|      20 |  6017 | `		int bAny = 0;` |
|      20 |  6018 | `		bOk = 1;` |
|      20 |  6019 | `		pMap->pCur = pMap->pFirst;` |
|      32 |  6020 | `		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|       - |  6021 | `			ph7_value sKey;` |
|       - |  6022 | `			ph7_value *pVal;` |
|       - |  6023 | `			const char *zAlgo,*zHex;` |
|       - |  6024 | `			char zName[64];` |
|       - |  6025 | `			int nAlgo,nHex,iOne;` |
|      24 |  6026 | `			bAny = 1;` |
|      24 |  6027 | `			PH7_MemObjInit(pCtxRes->pVm,&sKey);` |
|      24 |  6028 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      24 |  6029 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      24 |  6030 | `			zAlgo = SockSslStrOf(&sKey,&nAlgo);` |
|      24 |  6031 | `			zHex = SockSslStrOf(pVal,&nHex);` |
|      24 |  6032 | `			if( zAlgo == 0 \|\| zHex == 0 ){` |
|       - |  6033 | `				/* An INTEGER key has no algorithm name and a non-string value` |
|       - |  6034 | `				 * is not a digest; php complains about the array's shape and` |
|       - |  6035 | `				 * stops there. */` |
|       4 |  6036 | `				PH7_MemObjRelease(&sKey);` |
|       4 |  6037 | `				zWarn = zBad;` |
|       4 |  6038 | `				bOk = 0;` |
|       8 |  6039 | `				break;` |
|       - |  6040 | `			}` |
|      20 |  6041 | `			if( nAlgo >= (int)sizeof(zName) ){` |
|     ! 0 |  6042 | `				iOne = -1;` |
|     ! 0 |  6043 | `			}else{` |
|      20 |  6044 | `				SyMemcpy((const void *)zAlgo,(void *)zName,(sxu32)nAlgo);` |
|      20 |  6045 | `				zName[nAlgo] = 0;` |
|      20 |  6046 | `				iOne = SockSslFpOne(pCert,zName,zHex,nHex);` |
|       - |  6047 | `			}` |
|      20 |  6048 | `			PH7_MemObjRelease(&sKey);` |
|      20 |  6049 | `			if( iOne != 1 ){` |
|       8 |  6050 | `				if( iOne < 0 ){` |
|       4 |  6051 | `					zWarn = "Unknown digest algorithm";` |
|       2 |  6052 | `				}` |
|       8 |  6053 | `				bOk = 0;` |
|       8 |  6054 | `				break;` |
|       - |  6055 | `			}` |
|     ! 0 |  6056 | `		}` |
|      20 |  6057 | `		if( !bAny ){` |
|       - |  6058 | `			/* php refuses an EMPTY array on its shape rather than treating` |
|       - |  6059 | ``			 * `nothing to check` as a pass. */`` |
|       2 |  6060 | `			zWarn = zBad;` |
|       2 |  6061 | `			bOk = 0;` |
|       1 |  6062 | `		}` |
|      20 |  6063 | `		if( zWarn ){` |
|       - |  6064 | `			/* No calling context when the negotiation belongs to the http` |
|       - |  6065 | `			 * wrapper's own socket: the sentence still belongs to whatever` |
|       - |  6066 | `			 * function is doing the opening, which is the name the VM has` |
|       - |  6067 | `			 * armed for it. */` |
|      10 |  6068 | `			if( pCtx ){` |
|      10 |  6069 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,zWarn);` |
|       5 |  6070 | `			}else if( pCtxRes && pCtxRes->pVm ){` |
|     ! 0 |  6071 | `				ph7_vm *pVm = pCtxRes->pVm;` |
|       - |  6072 | `				SyString sCaller;` |
|     ! 0 |  6073 | `				SyStringInitFromBuf(&sCaller,pVm->zOpenCaller ? pVm->zOpenCaller : "",` |
|       - |  6074 | `					pVm->zOpenCaller ? SyStrlen(pVm->zOpenCaller) : 0);` |
|     ! 0 |  6075 | `				PH7_VmThrowError(pVm,pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_WARNING,zWarn);` |
|     ! 0 |  6076 | `			}` |
|       5 |  6077 | `		}` |
|      10 |  6078 | `	}else{` |
|       - |  6079 | `		int nHex;` |
|      14 |  6080 | `		const char *zHex = SockSslStrOf(pWant,&nHex);` |
|      14 |  6081 | `		const char *zAlgo = nHex == 32 ? "md5" : (nHex == 40 ? "sha1" : 0);` |
|      14 |  6082 | `		bOk = SockSslFpOne(pCert,zAlgo,zHex,nHex) == 1;` |
|       - |  6083 | `	}` |
|      34 |  6084 | `	X509_free(pCert);` |
|      34 |  6085 | `	if( bOk ){` |
|      12 |  6086 | `		return 0;` |
|       - |  6087 | `	}` |
|      22 |  6088 | `	SyBufferFormat(zErr,(sxu32)nErr,"peer_fingerprint match failure");` |
|      22 |  6089 | `	return -1;` |
|      38 |  6090 | `}` |
|       - |  6091 | `/*` |
|       - |  6092 | ``  * Turn php's method mask into the context OpenSSL wants, apply the `ssl` `` |
|       - |  6093 | ` * context options, adopt the socket and run the handshake. Answers PH7_OK, or` |
|       - |  6094 | ` * -1 with *zErr carrying php's sentence for the failure.` |
|       - |  6095 | ` *` |
|       - |  6096 | ` * The socket is left BLOCKING for the duration whatever the handle's own mode` |
|       - |  6097 | ` * is: php's handshake drives its own retry loop over a non-blocking socket and` |
|       - |  6098 | ` * the observable end of both is the same completed (or refused) handshake, so` |
|       - |  6099 | ` * the loop is not reproduced -- the read/write ops below are what the handle's` |
|       - |  6100 | ` * blocking mode is really about.` |
|       - |  6101 | ` */` |
|      88 |  6102 | `static int SockSslHandshakeOn(ph7_vm *pVm,ph7_context *pCtx,ph7_socket sock,int iMethod,` |
|       - |  6103 | `	phl_stream_ctx *pCtxRes,const char *zPeerName,void **ppSsl,void **ppSslCtx,` |
|       - |  6104 | `	char *zErr,int nErr)` |
|     ! 0 |  6105 | `{` |
|       - |  6106 | `	SSL_CTX *pSslCtx;` |
|       - |  6107 | `	SSL *pSsl;` |
|       - |  6108 | `	SyBlob sTmp;` |
|       - |  6109 | `	const char *z;` |
|      88 |  6110 | `	int bServer = (iMethod & SOCK_CRYPTO_CLIENT) == 0;` |
|      88 |  6111 | `	int iMin = 0,iMax = 0,rc;` |
|      88 |  6112 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 |  6113 | `		SyBufferFormat(zErr,(sxu32)nErr,"This stream does not support SSL/crypto");` |
|     ! 0 |  6114 | `		return -1;` |
|       - |  6115 | `	}` |
|      88 |  6116 | `	if( iMethod & SOCK_CRYPTO_TLSv1_0 ){ iMin = iMin ? iMin : TLS1_VERSION; iMax = TLS1_VERSION; }` |
|      88 |  6117 | `	if( iMethod & SOCK_CRYPTO_TLSv1_1 ){ iMin = iMin ? iMin : TLS1_1_VERSION; iMax = TLS1_1_VERSION; }` |
|      88 |  6118 | `	if( iMethod & SOCK_CRYPTO_TLSv1_2 ){ iMin = iMin ? iMin : TLS1_2_VERSION; iMax = TLS1_2_VERSION; }` |
|      88 |  6119 | `	if( iMethod & SOCK_CRYPTO_TLSv1_3 ){ iMin = iMin ? iMin : TLS1_3_VERSION; iMax = TLS1_3_VERSION; }` |
|      88 |  6120 | `	if( iMin == 0 ){` |
|       - |  6121 | `		/* SSLv2 or SSLv3 alone: php hands the mask to a method OpenSSL 3 has` |
|       - |  6122 | `		 * removed, and the refusal is the handshake's. Said here instead,` |
|       - |  6123 | `		 * because there is no context to build. */` |
|     ! 0 |  6124 | `		SyBufferFormat(zErr,(sxu32)nErr,` |
|       - |  6125 | `			"No SSL protocols are enabled and the client cannot continue");` |
|     ! 0 |  6126 | `		return -1;` |
|       - |  6127 | `	}` |
|      88 |  6128 | `	pSslCtx = SSL_CTX_new(bServer ? TLS_server_method() : TLS_client_method());` |
|      88 |  6129 | `	if( pSslCtx == 0 ){` |
|     ! 0 |  6130 | `		SockSslErrorText("Failed to create an SSL context",zErr,nErr);` |
|     ! 0 |  6131 | `		return -1;` |
|       - |  6132 | `	}` |
|      88 |  6133 | `	SSL_CTX_set_min_proto_version(pSslCtx,iMin);` |
|      88 |  6134 | `	SSL_CTX_set_max_proto_version(pSslCtx,iMax);` |
|      88 |  6135 | `	SyBlobInit(&sTmp,&pVm->sAllocator);` |
|      88 |  6136 | `	if( (z = SockSslOptStr(pCtxRes,"ciphers",&sTmp)) != 0 ){` |
|     ! 0 |  6137 | `		SSL_CTX_set_cipher_list(pSslCtx,z);` |
|     ! 0 |  6138 | `	}` |
|      88 |  6139 | `	if( (z = SockSslOptStr(pCtxRes,"local_cert",&sTmp)) != 0 ){` |
|       - |  6140 | ``		/* php reads a PEM holding the certificate AND, unless `local_pk` names`` |
|       - |  6141 | `		 * one separately, the private key beside it -- which is why a single` |
|       - |  6142 | `		 * combined PEM is the documented shape. */` |
|       8 |  6143 | `		if( SSL_CTX_use_certificate_chain_file(pSslCtx,z) != 1 ){` |
|       - |  6144 | `			/* php's own sentence, naming the file and pointing at the two` |
|       - |  6145 | `			 * options that would have supplied its issuer -- and NOT OpenSSL's` |
|       - |  6146 | `			 * queue, which php reads only for a handshake. */` |
|       9 |  6147 | `			SyBufferFormat(zErr,(sxu32)nErr,` |
|       - |  6148 | ``				"Unable to set local cert chain file `%s'; Check that your "`` |
|       - |  6149 | `				"cafile/capath settings include details of your certificate "` |
|       3 |  6150 | `				"and its issuer",z);` |
|       6 |  6151 | `			ERR_clear_error();` |
|       6 |  6152 | `			goto fail_ctx;` |
|       - |  6153 | `		}` |
|       2 |  6154 | `		if( SockSslOptStr(pCtxRes,"local_pk",&sTmp) == 0` |
|       2 |  6155 | `		 && SSL_CTX_use_PrivateKey_file(pSslCtx,z,SSL_FILETYPE_PEM) != 1 ){` |
|     ! 0 |  6156 | ``			SyBufferFormat(zErr,(sxu32)nErr,"Unable to set private key file `%s'",z);`` |
|     ! 0 |  6157 | `			ERR_clear_error();` |
|     ! 0 |  6158 | `			goto fail_ctx;` |
|       - |  6159 | `		}` |
|       1 |  6160 | `	}` |
|      82 |  6161 | `	if( (z = SockSslOptStr(pCtxRes,"local_pk",&sTmp)) != 0 ){` |
|     ! 0 |  6162 | `		if( SSL_CTX_use_PrivateKey_file(pSslCtx,z,SSL_FILETYPE_PEM) != 1 ){` |
|     ! 0 |  6163 | ``			SyBufferFormat(zErr,(sxu32)nErr,"Unable to set private key file `%s'",z);`` |
|     ! 0 |  6164 | `			ERR_clear_error();` |
|     ! 0 |  6165 | `			goto fail_ctx;` |
|       - |  6166 | `		}` |
|     ! 0 |  6167 | `	}` |
|      82 |  6168 | `	if( SockSslOptBool(pCtxRes,"verify_peer",1) ){` |
|       - |  6169 | `		SyBlob sCa;` |
|       - |  6170 | `		const char *zCaFile,*zCaPath;` |
|       2 |  6171 | `		SyBlobInit(&sCa,&pVm->sAllocator);` |
|       2 |  6172 | `		zCaFile = SockSslOptStr(pCtxRes,"cafile",&sTmp);` |
|       2 |  6173 | `		zCaPath = SockSslOptStr(pCtxRes,"capath",&sCa);` |
|       2 |  6174 | `		if( zCaFile \|\| zCaPath ){` |
|     ! 0 |  6175 | `			if( SSL_CTX_load_verify_locations(pSslCtx,zCaFile,zCaPath) != 1 ){` |
|     ! 0 |  6176 | `				SockSslErrorText("Unable to load the CA bundle",zErr,nErr);` |
|     ! 0 |  6177 | `				SyBlobRelease(&sCa);` |
|     ! 0 |  6178 | `				goto fail_ctx;` |
|       - |  6179 | `			}` |
|     ! 0 |  6180 | `		}else{` |
|       2 |  6181 | `			SSL_CTX_set_default_verify_paths(pSslCtx);` |
|       - |  6182 | `		}` |
|       2 |  6183 | `		SyBlobRelease(&sCa);` |
|       - |  6184 | ``		/* `allow_self_signed` does not turn verification off -- it whitelists`` |
|       - |  6185 | `		 * the two verdicts a self-signed chain produces, so an EXPIRED` |
|       - |  6186 | `		 * self-signed certificate is still refused. Reproduced by verifying and` |
|       - |  6187 | `		 * reading the verdict after the handshake rather than by asking` |
|       - |  6188 | `		 * OpenSSL to fail here. */` |
|       2 |  6189 | `		SSL_CTX_set_verify(pSslCtx,SSL_VERIFY_NONE,0);` |
|       1 |  6190 | `	}else{` |
|      80 |  6191 | `		SSL_CTX_set_verify(pSslCtx,SSL_VERIFY_NONE,0);` |
|       - |  6192 | `	}` |
|      82 |  6193 | `	pSsl = SSL_new(pSslCtx);` |
|      82 |  6194 | `	if( pSsl == 0 ){` |
|     ! 0 |  6195 | `		SockSslErrorText("Failed to create an SSL handle",zErr,nErr);` |
|     ! 0 |  6196 | `		goto fail_ctx;` |
|       - |  6197 | `	}` |
|      82 |  6198 | `	if( !bServer ){` |
|      80 |  6199 | `		const char *zHost = SockSslOptStr(pCtxRes,"peer_name",&sTmp);` |
|      80 |  6200 | `		if( zHost == 0 ){` |
|      80 |  6201 | `			zHost = zPeerName;` |
|      40 |  6202 | `		}` |
|      80 |  6203 | `		if( zHost && zHost[0] ){` |
|      80 |  6204 | `			if( SockSslOptBool(pCtxRes,"SNI_enabled",1) ){` |
|       - |  6205 | `				/* An IP literal is not a server NAME, and OpenSSL refuses to` |
|       - |  6206 | `				 * put one in the extension -- php sends SNI for a hostname` |
|       - |  6207 | `				 * only, so the return value is deliberately not checked. */` |
|      80 |  6208 | `				SSL_set_tlsext_host_name(pSsl,zHost);` |
|      40 |  6209 | `			}` |
|      80 |  6210 | `			if( SockSslOptBool(pCtxRes,"verify_peer_name",1) ){` |
|     ! 0 |  6211 | `				X509_VERIFY_PARAM *pParam = SSL_get0_param(pSsl);` |
|     ! 0 |  6212 | `				X509_VERIFY_PARAM_set_hostflags(pParam,` |
|       - |  6213 | `					X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);` |
|     ! 0 |  6214 | `				X509_VERIFY_PARAM_set1_host(pParam,zHost,0);` |
|     ! 0 |  6215 | `			}` |
|      40 |  6216 | `		}` |
|      40 |  6217 | `	}` |
|      82 |  6218 | `	if( SSL_set_fd(pSsl,(int)sock) != 1 ){` |
|     ! 0 |  6219 | `		SockSslErrorText("Failed to attach the socket to the SSL handle",zErr,nErr);` |
|     ! 0 |  6220 | `		SSL_free(pSsl);` |
|     ! 0 |  6221 | `		goto fail_ctx;` |
|       - |  6222 | `	}` |
|      82 |  6223 | `	ERR_clear_error();` |
|      82 |  6224 | `	rc = bServer ? SSL_accept(pSsl) : SSL_connect(pSsl);` |
|      82 |  6225 | `	if( rc != 1 ){` |
|       - |  6226 | `		{` |
|       - |  6227 | `			char zHead[64];` |
|       9 |  6228 | `			SyBufferFormat(zHead,sizeof(zHead),"SSL operation failed with code %d. ",` |
|       3 |  6229 | `				SSL_get_error(pSsl,rc));` |
|       6 |  6230 | `			SockSslErrorText(zHead,zErr,nErr);` |
|       - |  6231 | `		}` |
|       6 |  6232 | `		SSL_free(pSsl);` |
|       6 |  6233 | `		goto fail_ctx;` |
|       - |  6234 | `	}` |
|      76 |  6235 | `	if( !bServer && SockSslOptBool(pCtxRes,"verify_peer",1) ){` |
|     ! 0 |  6236 | `		long iVerdict = SSL_get_verify_result(pSsl);` |
|     ! 0 |  6237 | `		int bSelf = (iVerdict == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT` |
|     ! 0 |  6238 | `		          \|\| iVerdict == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN);` |
|     ! 0 |  6239 | `		if( iVerdict != X509_V_OK` |
|     ! 0 |  6240 | `		 && !(bSelf && SockSslOptBool(pCtxRes,"allow_self_signed",0)) ){` |
|     ! 0 |  6241 | `			SyBufferFormat(zErr,(sxu32)nErr,` |
|       - |  6242 | `				"Peer certificate did not match expected peer: %s",` |
|     ! 0 |  6243 | `				X509_verify_cert_error_string(iVerdict));` |
|     ! 0 |  6244 | `			SSL_free(pSsl);` |
|     ! 0 |  6245 | `			goto fail_ctx;` |
|       - |  6246 | `		}` |
|     ! 0 |  6247 | `	}` |
|       - |  6248 | `	/* php CAPTURES before it checks the fingerprint, which is why a session` |
|       - |  6249 | `	 * the pin below refuses still leaves the certificate it refused on the` |
|       - |  6250 | `	 * context -- and why nothing is captured from a handshake the chain` |
|       - |  6251 | `	 * verdict above already ended. */` |
|      76 |  6252 | `	SockSslCapture(pVm,pSsl,pCtxRes);` |
|      76 |  6253 | `	if( SockSslCheckFingerprint(pCtx,pSsl,pCtxRes,zErr,nErr) != 0 ){` |
|      30 |  6254 | `		SSL_free(pSsl);` |
|      30 |  6255 | `		goto fail_ctx;` |
|       - |  6256 | `	}` |
|      46 |  6257 | `	SyBlobRelease(&sTmp);` |
|      46 |  6258 | `	*ppSsl = (void *)pSsl;` |
|      46 |  6259 | `	*ppSslCtx = (void *)pSslCtx;` |
|      46 |  6260 | `	return PH7_OK;` |
|      21 |  6261 | `fail_ctx:` |
|      42 |  6262 | `	SyBlobRelease(&sTmp);` |
|      42 |  6263 | `	SSL_CTX_free(pSslCtx);` |
|      42 |  6264 | `	return -1;` |
|      44 |  6265 | `}` |
|       - |  6266 | `/*` |
|       - |  6267 | ` * The socket handle's door onto the negotiation above: the session and the` |
|       - |  6268 | `` * context it was built from are stored ON the handle, and `bSslSeen` is what`` |
|       - |  6269 | ` * makes a second stream_socket_enable_crypto() answer for a session that has` |
|       - |  6270 | ` * already been negotiated.` |
|       - |  6271 | ` */` |
|      62 |  6272 | `static int SockSslHandshake(ph7_context *pCtx,sock_private *pSock,int iMethod,` |
|       - |  6273 | `	phl_stream_ctx *pCtxRes,const char *zPeerName,char *zErr,int nErr)` |
|     ! 0 |  6274 | `{` |
|       - |  6275 | `	int rc;` |
|      62 |  6276 | `	if( pSock == 0 ){` |
|     ! 0 |  6277 | `		SyBufferFormat(zErr,(sxu32)nErr,"This stream does not support SSL/crypto");` |
|     ! 0 |  6278 | `		return -1;` |
|       - |  6279 | `	}` |
|      93 |  6280 | `	rc = SockSslHandshakeOn(pSock->pVm,pCtx,pSock->sock,iMethod,pCtxRes,zPeerName,` |
|      31 |  6281 | `		&pSock->pSsl,&pSock->pSslCtx,zErr,nErr);` |
|      62 |  6282 | `	if( rc == PH7_OK ){` |
|      22 |  6283 | `		pSock->bSslSeen = 1;` |
|      11 |  6284 | `	}` |
|      62 |  6285 | `	return rc;` |
|      31 |  6286 | `}` |
|       - |  6287 | `/*` |
|       - |  6288 | ` * The same negotiation for a socket that is NOT a stream handle: the http` |
|       - |  6289 | ` * wrapper dials one of its own and speaks TLS over it, exactly as php's does` |
|       - |  6290 | `` * by opening `ssl://host:port` rather than `tcp://` for an https:// URL. The`` |
|       - |  6291 | ` * method is that transport's -- any TLS version, client side.` |
|       - |  6292 | ` */` |
|      26 |  6293 | `PH7_PRIVATE int PH7_SslClientHandshake(ph7_vm *pVm,ph7_socket sock,` |
|       - |  6294 | `	phl_stream_ctx *pCtxRes,const char *zPeerName,void **ppSsl,void **ppSslCtx,` |
|       - |  6295 | `	char *zErr,int nErr)` |
|     ! 0 |  6296 | `{` |
|      39 |  6297 | `	return SockSslHandshakeOn(pVm,0,sock,SOCK_CRYPTO_TLS_CLIENT,pCtxRes,zPeerName,` |
|      13 |  6298 | `		ppSsl,ppSslCtx,zErr,nErr);` |
|     ! 0 |  6299 | `}` |
|       - |  6300 | `/* Drop a session negotiated by either door, leaving the socket alone. */` |
|     669 |  6301 | `PH7_PRIVATE void PH7_SslDropSession(void **ppSsl,void **ppSslCtx)` |
|       5 |  6302 | `{` |
|     674 |  6303 | `	if( ppSsl == 0 \|\| *ppSsl == 0 ){` |
|     628 |  6304 | `		return;` |
|       - |  6305 | `	}` |
|       - |  6306 | `	/* One shutdown, not the two-step wait for the peer's own close_notify: the` |
|       - |  6307 | `	 * peer may be gone and php does not block a close on it either. */` |
|      46 |  6308 | `	SSL_shutdown((SSL *)*ppSsl);` |
|      46 |  6309 | `	SSL_free((SSL *)*ppSsl);` |
|      46 |  6310 | `	*ppSsl = 0;` |
|      46 |  6311 | `	if( ppSslCtx && *ppSslCtx ){` |
|      46 |  6312 | `		SSL_CTX_free((SSL_CTX *)*ppSslCtx);` |
|      46 |  6313 | `		*ppSslCtx = 0;` |
|      23 |  6314 | `	}` |
|     340 |  6315 | `}` |
|       - |  6316 | `/* Tear the TLS session down without touching the socket: php's` |
|       - |  6317 | ` * stream_socket_enable_crypto($h, false) leaves a usable plain handle behind,` |
|       - |  6318 | ` * and the close path below runs the same teardown before closing the socket. */` |
|     355 |  6319 | `static void SockSslDrop(sock_private *pSock)` |
|       4 |  6320 | `{` |
|     359 |  6321 | `	if( pSock == 0 ){` |
|     ! 0 |  6322 | `		return;` |
|       - |  6323 | `	}` |
|     359 |  6324 | `	PH7_SslDropSession(&pSock->pSsl,&pSock->pSslCtx);` |
|     182 |  6325 | `}` |
|       - |  6326 | `#endif /* PH7_ENABLE_OPENSSL */` |
|     110 |  6327 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|       4 |  6328 | `{` |
|     114 |  6329 | `	sock_private *pSock = (sock_private *)pHandle;` |
|       - |  6330 | `	int n;` |
|     114 |  6331 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|       - |  6332 | `		/* A server asked for neither BIND nor LISTEN has no socket at all, and` |
|       - |  6333 | `		 * php answers false for a read on it — the shape an ERROR takes. */` |
|       6 |  6334 | `		return -1;` |
|       - |  6335 | `	}` |
|     110 |  6336 | `	if( pSock->bEof ){` |
|     ! 0 |  6337 | `		return 0;` |
|       - |  6338 | `	}` |
|       - |  6339 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     110 |  6340 | `	if( pSock->pSsl ){` |
|       - |  6341 | `		/* Under TLS the RECORD layer is the stream: libssl owns the socket, and` |
|       - |  6342 | `		 * a read that answers nothing is told from a closed session by` |
|       - |  6343 | `		 * SSL_get_error() rather than by errno -- WANT_READ/WANT_WRITE is the` |
|       - |  6344 | `		 * EAGAIN a non-blocking handle sees, and a clean ZERO_RETURN is the` |
|       - |  6345 | `		 * peer's close_notify, which IS this stream's end of file. */` |
|       4 |  6346 | `		n = SSL_read((SSL *)pSock->pSsl,pBuffer,(int)nRead);` |
|       4 |  6347 | `		if( n > 0 ){` |
|       2 |  6348 | `			return (ph7_int64)n;` |
|       - |  6349 | `		}` |
|       2 |  6350 | `		switch( SSL_get_error((SSL *)pSock->pSsl,n) ){` |
|       1 |  6351 | `			case SSL_ERROR_ZERO_RETURN:` |
|       2 |  6352 | `				pSock->bEof = 1;` |
|       2 |  6353 | `				return 0;` |
|     ! 0 |  6354 | `			case SSL_ERROR_WANT_READ:` |
|       - |  6355 | `			case SSL_ERROR_WANT_WRITE:` |
|     ! 0 |  6356 | `				errno = EAGAIN;` |
|     ! 0 |  6357 | `				return -1;` |
|     ! 0 |  6358 | `			case SSL_ERROR_SYSCALL:` |
|       - |  6359 | `				/* The peer vanished without a close_notify. php reports the end` |
|       - |  6360 | `				 * of the stream for it rather than an error, because the bytes` |
|       - |  6361 | `				 * already handed over are the whole of what there was. */` |
|     ! 0 |  6362 | `				if( n == 0 ){` |
|     ! 0 |  6363 | `					pSock->bEof = 1;` |
|     ! 0 |  6364 | `					return 0;` |
|       - |  6365 | `				}` |
|     ! 0 |  6366 | `				errno = errno != 0 ? errno : EIO;` |
|     ! 0 |  6367 | `				return -1;` |
|     ! 0 |  6368 | `			default:` |
|     ! 0 |  6369 | `				errno = EIO;` |
|     ! 0 |  6370 | `				return -1;` |
|       - |  6371 | `		}` |
|       - |  6372 | `	}` |
|       - |  6373 | `#endif` |
|     106 |  6374 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     106 |  6375 | `	if( n == 0 ){` |
|       - |  6376 | `		/* The peer closed: THIS is the end of the stream. */` |
|      25 |  6377 | `		pSock->bEof = 1;` |
|      25 |  6378 | `		return 0;` |
|       - |  6379 | `	}` |
|      82 |  6380 | `	if( n < 0 ){` |
|       - |  6381 | `		/* An error, and since stream_set_blocking()/stream_set_timeout() exist` |
|       - |  6382 | `		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF` |
|       - |  6383 | `		 * here (as this did for every n <= 0, safe only while every socket was` |
|       - |  6384 | `		 * blocking and untimed) made the first empty read close the connection` |
|       - |  6385 | `		 * for good and threw away everything the peer sent afterwards.` |
|       - |  6386 | `		 *` |
|       - |  6387 | `		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a` |
|       - |  6388 | ``		 * Winsock call never touches: without this the `""` a non-blocking read`` |
|       - |  6389 | ``		 * answers and the `timed_out` an expired one reports were both lost on`` |
|       - |  6390 | `		 * Windows, and every such read came back as a plain failure. */` |
|       7 |  6391 | `		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);` |
|       7 |  6392 | `		return -1;` |
|       - |  6393 | `	}` |
|      76 |  6394 | `	return (ph7_int64)n;` |
|      51 |  6395 | `}` |
|     114 |  6396 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|       4 |  6397 | `{` |
|     118 |  6398 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     118 |  6399 | `	const char *zBuf = (const char *)pBuf;` |
|     118 |  6400 | `	ph7_int64 nSent = 0;` |
|     118 |  6401 | `	if( pSock == 0 ){` |
|     ! 0 |  6402 | `		return -1;` |
|       - |  6403 | `	}` |
|     118 |  6404 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|       - |  6405 | `		/* Nothing to send on, and php answers 0 rather than false for it. */` |
|       6 |  6406 | `		return 0;` |
|       - |  6407 | `	}` |
|       - |  6408 | `	/* php answers the number of bytes it MOVED. This used to hand back` |
|       - |  6409 | `	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful` |
|       - |  6410 | ``	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check`` |
|       - |  6411 | `	 * failed on a write that worked, a partial-write retry loop never advanced,` |
|       - |  6412 | `	 * and stream_copy_to_stream() stopped after its first chunk. */` |
|       - |  6413 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     114 |  6414 | `	if( pSock->pSsl ){` |
|       4 |  6415 | `		while( nSent < nWrite ){` |
|       2 |  6416 | `			int n = SSL_write((SSL *)pSock->pSsl,&zBuf[nSent],(int)(nWrite - nSent));` |
|       2 |  6417 | `			if( n > 0 ){` |
|       2 |  6418 | `				nSent += n;` |
|       2 |  6419 | `				continue;` |
|       - |  6420 | `			}` |
|       - |  6421 | `			{` |
|     ! 0 |  6422 | `				int iErr = SSL_get_error((SSL *)pSock->pSsl,n);` |
|     ! 0 |  6423 | `				if( iErr == SSL_ERROR_WANT_READ \|\| iErr == SSL_ERROR_WANT_WRITE ){` |
|     ! 0 |  6424 | `					return nSent;` |
|       - |  6425 | `				}` |
|       - |  6426 | `				/* Same shape the plain socket answers: the COUNT moved, and` |
|       - |  6427 | `				 * false only for a write that moved nothing at all. The errno` |
|       - |  6428 | `				 * is what the socket notice reads, and a TLS failure has none` |
|       - |  6429 | ``				 * of its own -- libssl's queue is not php's `errno=`. */`` |
|     ! 0 |  6430 | `				pSock->iLastErr = errno != 0 ? errno : EIO;` |
|       - |  6431 | `			}` |
|     ! 0 |  6432 | `			return nSent > 0 ? nSent : -1;` |
|     ! 0 |  6433 | `		}` |
|       2 |  6434 | `		return nSent;` |
|       - |  6435 | `	}` |
|       - |  6436 | `#endif` |
|     212 |  6437 | `	while( nSent < nWrite ){` |
|     113 |  6438 | `		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);` |
|     113 |  6439 | `		if( n > 0 ){` |
|     104 |  6440 | `			nSent += n;` |
|     104 |  6441 | `			continue;` |
|       - |  6442 | `		}` |
|       - |  6443 | `		/* Nothing more can go right now. On a non-blocking or timed-out handle` |
|       - |  6444 | `		 * that is php's 0 (or the partial count), and only a write that moved` |
|       - |  6445 | `		 * NO bytes at all for a real error is php's false — which is why the` |
|       - |  6446 | `		 * count is answered here rather than the status. */` |
|      11 |  6447 | `		if( PH7_NetWouldBlock() ){` |
|       4 |  6448 | `			return nSent;` |
|       - |  6449 | `		}` |
|       8 |  6450 | `		pSock->iLastErr = PH7_NetLastError();` |
|       8 |  6451 | `		return nSent > 0 ? nSent : -1;` |
|     ! 0 |  6452 | `	}` |
|     103 |  6453 | `	return nSent;` |
|      59 |  6454 | `}` |
|     353 |  6455 | `static void SockStreamData_Close(void *pHandle)` |
|       4 |  6456 | `{` |
|     357 |  6457 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     357 |  6458 | `	if( pSock == 0 ){` |
|     ! 0 |  6459 | `		return;` |
|       - |  6460 | `	}` |
|       - |  6461 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - |  6462 | `	/* The session goes down BEFORE the socket does: a close_notify has to reach` |
|       - |  6463 | `	 * the peer over a descriptor that is still open, and libssl would otherwise` |
|       - |  6464 | `	 * write it to a closed (or, worse, recycled) one. */` |
|     357 |  6465 | `	SockSslDrop(pSock);` |
|       - |  6466 | `#endif` |
|     357 |  6467 | `	PH7_NetClose(pSock->sock);` |
|     357 |  6468 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|     181 |  6469 | `}` |
|       - |  6470 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|     ! 0 |  6471 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     ! 0 |  6472 | `{` |
|       - |  6473 | `	sock_private *pSock;` |
|       - |  6474 | `	ph7_socket sock;` |
|       - |  6475 | `	char zHost[256];` |
|       - |  6476 | `	const char *zColon;` |
|     ! 0 |  6477 | `	int iPort = 0,iErrno = 0;` |
|     ! 0 |  6478 | `	const char *zErr = "";` |
|     ! 0 |  6479 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     ! 0 |  6480 | `	SXUNUSED(iMode);` |
|     ! 0 |  6481 | `	if( pVm == 0 ){` |
|     ! 0 |  6482 | `		return -1;` |
|       - |  6483 | `	}` |
|     ! 0 |  6484 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|     ! 0 |  6485 | `	while( zColon > zName && zColon[0] != ':' ){` |
|     ! 0 |  6486 | `		zColon--;` |
|     ! 0 |  6487 | `	}` |
|     ! 0 |  6488 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|     ! 0 |  6489 | `		return -1;` |
|       - |  6490 | `	}` |
|       - |  6491 | `	{` |
|     ! 0 |  6492 | `		sxu32 n = (sxu32)(zColon - zName);` |
|     ! 0 |  6493 | `		if( n >= sizeof(zHost) ){` |
|     ! 0 |  6494 | `			n = sizeof(zHost) - 1;` |
|     ! 0 |  6495 | `		}` |
|     ! 0 |  6496 | `		SyMemcpy(zName,zHost,n);` |
|     ! 0 |  6497 | `		zHost[n] = 0;` |
|       - |  6498 | `	}` |
|       - |  6499 | `	{` |
|     ! 0 |  6500 | `		sxi32 iTmp = 0;` |
|     ! 0 |  6501 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|     ! 0 |  6502 | `		iPort = (int)iTmp;` |
|       - |  6503 | `	}` |
|     ! 0 |  6504 | `	sock = PH7_NetConnect(zHost,iPort,0,0,0,0,&iErrno,&zErr);` |
|     ! 0 |  6505 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 |  6506 | `		return -1;` |
|       - |  6507 | `	}` |
|     ! 0 |  6508 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|     ! 0 |  6509 | `	if( pSock == 0 ){` |
|     ! 0 |  6510 | `		PH7_NetClose(sock);` |
|     ! 0 |  6511 | `		return -1;` |
|       - |  6512 | `	}` |
|     ! 0 |  6513 | `	pSock->pVm = pVm;` |
|     ! 0 |  6514 | `	pSock->sock = sock;` |
|     ! 0 |  6515 | `	pSock->bEof = 0;` |
|     ! 0 |  6516 | `	pSock->iLastErr = 0;` |
|     ! 0 |  6517 | `	pSock->bGeneric = 0;` |
|     ! 0 |  6518 | `	pSock->bDgram = 0;` |
|       - |  6519 | ``	/* Left unset until now, so a `tcp://host:port` opened through fopen() --`` |
|       - |  6520 | `	 * the one door that reaches this opener -- read an uninitialised pointer` |
|       - |  6521 | ``	 * the moment stream_get_meta_data() asked for its `stream_type`. */`` |
|     ! 0 |  6522 | `	pSock->zLabel = 0;` |
|       - |  6523 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     ! 0 |  6524 | `	pSock->pSsl = 0;` |
|     ! 0 |  6525 | `	pSock->pSslCtx = 0;` |
|     ! 0 |  6526 | `	pSock->bSslSeen = 0;` |
|     ! 0 |  6527 | `	pSock->iCryptoAccept = 0;` |
|       - |  6528 | `#endif` |
|     ! 0 |  6529 | `	*ppHandle = (void *)pSock;` |
|     ! 0 |  6530 | `	return PH7_OK;` |
|     ! 0 |  6531 | `}` |
|       - |  6532 | `/* php's own listen backlog for a stream server. */` |
|       - |  6533 | `#define SOCK_LISTEN_BACKLOG 128` |
|       - |  6534 | `/*` |
|       - |  6535 | `` * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the`` |
|       - |  6536 | ` * NOTICE its socket ops raise for a send that failed, which is the only` |
|       - |  6537 | ` * diagnostic a write to a departed peer produces (the return value is the same` |
|       - |  6538 | ` * false a closed handle answers). The PLAIN-FILE device has the same notice` |
|       - |  6539 | `` * worded `Write of`, which is what a write to a handle opened read-only`` |
|       - |  6540 | ` * produces: php answers false AND says why, where this engine only answered` |
|       - |  6541 | ` * false. Silent for every other device — nothing else here has an OS error of` |
|       - |  6542 | ` * its own to report, and php's notice lives in those two stream ops alone.` |
|       - |  6543 | ` */` |
|      60 |  6544 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)` |
|       4 |  6545 | `{` |
|      64 |  6546 | `	if( pDev == 0 ){` |
|     ! 0 |  6547 | `		return;` |
|       - |  6548 | `	}` |
|      64 |  6549 | `	if( pDev->bDir \|\| pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|       - |  6550 | `		/* Not a write that failed -- a stream that has no writer, already` |
|       - |  6551 | ``		 * announced as `Stream is not writable`. php has no second sentence`` |
|       - |  6552 | `		 * for it, and this one would name an errno nothing set. */` |
|     ! 0 |  6553 | `		return;` |
|       - |  6554 | `	}` |
|       - |  6555 | `#ifdef PH7_ENABLE_NET` |
|      64 |  6556 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|       8 |  6557 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|       8 |  6558 | `		if( pSock->iLastErr != 0 ){` |
|      11 |  6559 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       - |  6560 | `				"Send of %d bytes failed with errno=%d %s",` |
|       3 |  6561 | `				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));` |
|       8 |  6562 | `			pSock->iLastErr = 0;` |
|       3 |  6563 | `		}` |
|       8 |  6564 | `		return;` |
|       - |  6565 | `	}` |
|       - |  6566 | `#endif` |
|      56 |  6567 | `	if( pDev->pStream == pCtx->pVm->pDefStream ){` |
|      45 |  6568 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      22 |  6569 | `			"Write of %d bytes failed with errno=%d %s",nLen,errno,VfsStrerror(errno));` |
|      11 |  6570 | `	}` |
|      34 |  6571 | `}` |
|       - |  6572 | `/* The settings family below owns both of these; the socket openers here are` |
|       - |  6573 | ` * declared ahead of it so one handle-wrapping routine can serve both halves. */` |
|       - |  6574 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - |  6575 | `	const char *zName,int *pRc);` |
|       - |  6576 | `static ph7_socket * IoPrivateSocket(io_private *pDev);` |
|       - |  6577 | `/*` |
|       - |  6578 | ` * Wrap an open socket in the io_private every f* builtin drives, so a socket a` |
|       - |  6579 | ` * server accepted reads and writes exactly like one a client connected. A NULL` |
|       - |  6580 | ` * zUri is php's "opened by no name at all" — an accepted connection, which` |
|       - |  6581 | `` * reports no `uri` at all from stream_get_meta_data(). `bDgram` is carried`` |
|       - |  6582 | ` * because php's stream ops are chosen per TRANSPORT and a script can see which` |
|       - |  6583 | `` * pair a handle got: a datagram socket reports `udp_socket`.`` |
|       - |  6584 | ` * Answers 0 (and closes the socket) when there is no memory for the handle.` |
|       - |  6585 | ` */` |
|     379 |  6586 | `static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|       - |  6587 | `	const char *zUri,int nUri)` |
|       4 |  6588 | `{` |
|       - |  6589 | `	io_private *pDev;` |
|       - |  6590 | `	sock_private *pSock;` |
|     383 |  6591 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     383 |  6592 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|     383 |  6593 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|     ! 0 |  6594 | `		if( pSock ){` |
|     ! 0 |  6595 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);` |
|     ! 0 |  6596 | `		}` |
|     ! 0 |  6597 | `		if( pDev ){` |
|       - |  6598 | `			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim` |
|       - |  6599 | `			 * this chunk — it is not an io_private yet and has no buffers. */` |
|     ! 0 |  6600 | `			ph7_context_free_chunk(pCtx,pDev);` |
|     ! 0 |  6601 | `		}` |
|     ! 0 |  6602 | `		PH7_NetClose(sock);` |
|     ! 0 |  6603 | `		return 0;` |
|       - |  6604 | `	}` |
|     383 |  6605 | `	pSock->pVm = pCtx->pVm;` |
|     383 |  6606 | `	pSock->sock = sock;` |
|     383 |  6607 | `	pSock->bEof = 0;` |
|     383 |  6608 | `	pSock->iLastErr = 0;` |
|     383 |  6609 | `	pSock->bGeneric = 0;` |
|     383 |  6610 | `	pSock->bDgram = bDgram;` |
|       - |  6611 | `	/* Derived from the two flags above unless ext/sockets states one. */` |
|     383 |  6612 | `	pSock->zLabel = 0;` |
|       - |  6613 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     383 |  6614 | `	pSock->pSsl = 0;` |
|     383 |  6615 | `	pSock->pSslCtx = 0;` |
|     383 |  6616 | `	pSock->bSslSeen = 0;` |
|     383 |  6617 | `	pSock->iCryptoAccept = 0;` |
|       - |  6618 | `#endif` |
|     383 |  6619 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|       - |  6620 | `	/* php's feof() answers TRUE for a stream whose socket was never created. */` |
|     383 |  6621 | `	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);` |
|     383 |  6622 | `	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);` |
|     383 |  6623 | `	pDev->pHandle = (void *)pSock;` |
|     383 |  6624 | `	return pDev;` |
|     194 |  6625 | `}` |
|       - |  6626 | `/*` |
|       - |  6627 | ` * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own` |
|       - |  6628 | ` * close hook frees the socket handle, and the io_private chunk goes with it.` |
|       - |  6629 | ` * Nothing has handed this out as a resource yet, so there is no ph7_value that` |
|       - |  6630 | ` * could observe it afterwards.` |
|       - |  6631 | ` */` |
|      38 |  6632 | `static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)` |
|     ! 0 |  6633 | `{` |
|      38 |  6634 | `	if( pDev == 0 ){` |
|     ! 0 |  6635 | `		return;` |
|       - |  6636 | `	}` |
|      38 |  6637 | `	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){` |
|      38 |  6638 | `		pDev->pStream->xClose(pDev->pHandle);` |
|      38 |  6639 | `		pDev->pHandle = 0;` |
|      19 |  6640 | `	}` |
|      38 |  6641 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      19 |  6642 | `}` |
|       - |  6643 | `/* Forward: php's port rule, defined with the address parser further down. */` |
|       - |  6644 | `static int SockParsePort(const char *z,int n);` |
|       - |  6645 | `/*` |
|       - |  6646 | `` * php's `socket` context options, read into the shape net.c applies. Only the`` |
|       - |  6647 | `` * ones this transport can honour are read: `bindto`, which is the LOCAL`` |
|       - |  6648 | ``  * address a client connects out from, `backlog`, `so_reuseport`, `tcp_nodelay` `` |
|       - |  6649 | `` * `so_broadcast` -- which had no consumer until udp:// existed, because it is`` |
|       - |  6650 | ` * the permission a DATAGRAM socket needs before the OS will let it address a` |
|       - |  6651 | `` * broadcast address at all -- and `ipv6_v6only`, which had none either while the`` |
|       - |  6652 | ` * socket layer could only open AF_INET.` |
|       - |  6653 | ` *` |
|       - |  6654 | `` * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the`` |
|       - |  6655 | ` * same address rule the server half already uses — and a spelling with no colon` |
|       - |  6656 | ` * at all is not an address, so php performs no bind and says nothing. A value` |
|       - |  6657 | ` * that is not a STRING is php's one hard failure here; everything else is a` |
|       - |  6658 | ` * warning and a connection made from wherever routing would have sent it.` |
|       - |  6659 | ` * Returns 0, or -1 with *pzErr set to php's refusal.` |
|       - |  6660 | ` */` |
|     439 |  6661 | `static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,` |
|       - |  6662 | `	const char **pzErr)` |
|       4 |  6663 | `{` |
|       - |  6664 | `	ph7_value *pVal;` |
|     443 |  6665 | `	SyZero(pOut,sizeof(*pOut));` |
|     443 |  6666 | `	if( pCtxRes == 0 ){` |
|     184 |  6667 | `		return 0;` |
|       - |  6668 | `	}` |
|     260 |  6669 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");` |
|     260 |  6670 | `	if( pVal ){` |
|       5 |  6671 | `		pOut->iBacklog = (int)ph7_value_to_int64(pVal);` |
|       2 |  6672 | `	}` |
|     260 |  6673 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");` |
|     260 |  6674 | `	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);` |
|     260 |  6675 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");` |
|     260 |  6676 | `	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);` |
|     260 |  6677 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_broadcast");` |
|     260 |  6678 | `	pOut->bBroadcast = pVal != 0 && ph7_value_to_bool(pVal);` |
|     260 |  6679 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","ipv6_v6only");` |
|     260 |  6680 | `	pOut->bV6Only = pVal != 0 && ph7_value_to_bool(pVal);` |
|     260 |  6681 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");` |
|     260 |  6682 | `	if( pVal ){` |
|       - |  6683 | `		const char *zSpec;` |
|      15 |  6684 | `		int nSpec = 0,i,nHost = -1;` |
|      15 |  6685 | `		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 |  6686 | `			*pzErr = "local_addr context option is not a string.";` |
|       3 |  6687 | `			return -1;` |
|       - |  6688 | `		}` |
|      13 |  6689 | `		zSpec = (const char *)SyBlobData(&pVal->sBlob);` |
|      13 |  6690 | `		nSpec = (int)SyBlobLength(&pVal->sBlob);` |
|       - |  6691 | `		/* NOT the bracketed grammar the openers read, deliberately: php's` |
|       - |  6692 | ``		 * `bindto` is applied per RESOLVER CANDIDATE and what it does with one`` |
|       - |  6693 | `		 * it cannot use in that candidate's family is three different things` |
|       - |  6694 | `		 * (skip in silence, warn and connect anyway, abandon the candidate),` |
|       - |  6695 | `		 * split by rules that are not the ones the wordings suggest -- so` |
|       - |  6696 | ``		 * parsing `[::1]:0` here without them would make PHL warn where php`` |
|       - |  6697 | `		 * says nothing. Measured and recorded. */` |
|     137 |  6698 | `		for( i = 0 ; i + 1 < nSpec ; i++ ){` |
|     135 |  6699 | `			if( zSpec[i] == ':' ){` |
|      11 |  6700 | `				nHost = i;` |
|      11 |  6701 | `				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);` |
|      11 |  6702 | `				break;` |
|       - |  6703 | `			}` |
|      63 |  6704 | `		}` |
|      13 |  6705 | `		if( nHost >= 0 ){` |
|      11 |  6706 | `			if( nHost >= nHostBuf ){` |
|     ! 0 |  6707 | `				nHost = nHostBuf - 1;` |
|     ! 0 |  6708 | `			}` |
|      11 |  6709 | `			if( nHost > 0 ){` |
|      11 |  6710 | `				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);` |
|       5 |  6711 | `			}` |
|      11 |  6712 | `			zHostBuf[nHost] = 0;` |
|      11 |  6713 | `			pOut->zBindHost = zHostBuf;` |
|       5 |  6714 | `		}` |
|       6 |  6715 | `	}` |
|     258 |  6716 | `	return 0;` |
|     225 |  6717 | `}` |
|       - |  6718 | `/*` |
|       - |  6719 | ` * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask` |
|       - |  6720 | ` * for: a second open of the SAME address hands back the very same resource` |
|       - |  6721 | `` * rather than a second connection — `$a === $b` — and fclose() is what ends it,`` |
|       - |  6722 | ` * after which the next open dials again. The key is the address as the opener` |
|       - |  6723 | ` * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.` |
|       - |  6724 | ` */` |
|      36 |  6725 | `static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)` |
|       2 |  6726 | `{` |
|       - |  6727 | `	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a` |
|       - |  6728 | `	 * persistent stream_socket_client() of one address are two connections. */` |
|      56 |  6729 | `	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",` |
|      18 |  6730 | `		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);` |
|      38 |  6731 | `}` |
|      22 |  6732 | `static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)` |
|       2 |  6733 | `{` |
|      24 |  6734 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - |  6735 | `	sxu32 i;` |
|      44 |  6736 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|      32 |  6737 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|      14 |  6738 | `			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      12 |  6739 | `				return aSlot[i].pDev;` |
|       - |  6740 | `			}` |
|       - |  6741 | `			/* fclose()'d since: the slot is free for the next connection. */` |
|       3 |  6742 | `			aSlot[i].zKey[0] = 0;` |
|       3 |  6743 | `			aSlot[i].pDev = 0;` |
|       1 |  6744 | `		}` |
|      11 |  6745 | `	}` |
|      14 |  6746 | `	return 0;` |
|      13 |  6747 | `}` |
|       - |  6748 | `/*` |
|       - |  6749 | ` * Forget a kept connection whose socket is gone. The io_private itself stays` |
|       - |  6750 | ` * alive and stamped closed (a script may still hold the resource), so this only` |
|       - |  6751 | ` * frees the SLOT for the fresh connection about to take its place.` |
|       - |  6752 | ` */` |
|       2 |  6753 | `static void SockPersistDrop(ph7_vm *pVm,const char *zKey)` |
|       1 |  6754 | `{` |
|       3 |  6755 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - |  6756 | `	sxu32 i;` |
|       3 |  6757 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|       3 |  6758 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|       3 |  6759 | `			aSlot[i].zKey[0] = 0;` |
|       3 |  6760 | `			aSlot[i].pDev = 0;` |
|       3 |  6761 | `			return;` |
|       - |  6762 | `		}` |
|     ! 0 |  6763 | `	}` |
|       2 |  6764 | `}` |
|      14 |  6765 | `static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)` |
|       2 |  6766 | `{` |
|      16 |  6767 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|       - |  6768 | `	VmPersistSock sSlot;` |
|       - |  6769 | `	sxu32 i;` |
|      28 |  6770 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|      18 |  6771 | `		if( aSlot[i].zKey[0] == 0 \|\| IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|       6 |  6772 | `			SyZero(&aSlot[i],sizeof(VmPersistSock));` |
|       6 |  6773 | `			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);` |
|       6 |  6774 | `			aSlot[i].pDev = pDev;` |
|       6 |  6775 | `			return;` |
|       - |  6776 | `		}` |
|       7 |  6777 | `	}` |
|      12 |  6778 | `	SyZero(&sSlot,sizeof(sSlot));` |
|      12 |  6779 | `	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);` |
|      12 |  6780 | `	sSlot.pDev = pDev;` |
|      12 |  6781 | `	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);` |
|       9 |  6782 | `}` |
|       - |  6783 | `/*` |
|       - |  6784 | `` * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the`` |
|       - |  6785 | ` * moment it is opened — a read from a peer that has gone quiet answers FALSE` |
|       - |  6786 | `` * after it, with `timed_out` set — where this engine armed nothing and waited`` |
|       - |  6787 | ` * forever. That is the difference between a program that reports a dead peer and` |
|       - |  6788 | ` * one that hangs.` |
|       - |  6789 | ` *` |
|       - |  6790 | ` * A LISTENING socket is deliberately left alone: php's accept timeout is its own` |
|       - |  6791 | ` * argument and its own select(), so arming the OS receive timeout here would` |
|       - |  6792 | `` * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be`` |
|       - |  6793 | ` * unbounded — at sixty seconds.` |
|       - |  6794 | ` */` |
|     203 |  6795 | `static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)` |
|       4 |  6796 | `{` |
|       - |  6797 | `	ph7_int64 iSec;` |
|     207 |  6798 | `	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;` |
|     207 |  6799 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|     ! 0 |  6800 | `		return;` |
|       - |  6801 | `	}` |
|     207 |  6802 | `	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);` |
|     207 |  6803 | `	if( iSec > 0 ){` |
|     207 |  6804 | `		PH7_NetSetRwTimeout(*pSock,iSec,0);` |
|     207 |  6805 | `		pDev->bHasTimeout = 1;` |
|     102 |  6806 | `	}` |
|     106 |  6807 | `}` |
|       - |  6808 | `/*` |
|       - |  6809 | ` * The out-params every address-taking opener carries, on the path that WORKED:` |
|       - |  6810 | ` * php writes 0 and "" into them rather than leaving whatever the caller's` |
|       - |  6811 | ` * variables already held.` |
|       - |  6812 | ` */` |
|     331 |  6813 | `static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)` |
|       4 |  6814 | `{` |
|     335 |  6815 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|     335 |  6816 | `	if( pTmp == 0 ){` |
|     ! 0 |  6817 | `		return;` |
|       - |  6818 | `	}` |
|     335 |  6819 | `	if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     297 |  6820 | `		ph7_value_int(pTmp,0);` |
|     297 |  6821 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     147 |  6822 | `	}` |
|     335 |  6823 | `	if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     297 |  6824 | `		ph7_value_string(pTmp,"",0);` |
|     297 |  6825 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     147 |  6826 | `	}` |
|     170 |  6827 | `}` |
|       - |  6828 | `/*` |
|       - |  6829 | ` * The failure shape the whole address-taking family shares: php words the` |
|       - |  6830 | ` * reason into BOTH the by-ref out-params and a warning naming the address as` |
|       - |  6831 | `` * the script wrote it. The `$errno` out-param stays 0 for everything the`` |
|       - |  6832 | ` * ADDRESS itself is refused for — php only ever reports an OS code for a` |
|       - |  6833 | ` * connect() that reached the network.` |
|       - |  6834 | ` */` |
|     188 |  6835 | `static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,` |
|       - |  6836 | `	const char *zAddr,int nAddr,const char *zErr,int iErrno)` |
|       4 |  6837 | `{` |
|       - |  6838 | `	ph7_value *pTmp;` |
|       - |  6839 | `	/* php's two halves do not agree about a failure that logged NO text: the` |
|       - |  6840 | `	 * out-param keeps the empty string php pre-assigned it, and the warning` |
|       - |  6841 | ``	 * says `Unknown error` in its place. A datagram address given the default`` |
|       - |  6842 | `	 * $flags is the one arm that reaches here (php's udp ops refuse LISTEN` |
|       - |  6843 | `	 * silently), so the distinction is user-visible rather than theoretical. */` |
|     192 |  6844 | `	const char *zWarn = zErr ? zErr : "Unknown error";` |
|     192 |  6845 | `	if( zErr == 0 ){` |
|      37 |  6846 | `		zErr = "";` |
|      18 |  6847 | `	}` |
|     192 |  6848 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|     192 |  6849 | `	if( pTmp ){` |
|     192 |  6850 | `		if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     192 |  6851 | `			ph7_value_int(pTmp,iErrno);` |
|     192 |  6852 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      95 |  6853 | `		}` |
|     192 |  6854 | `		if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     192 |  6855 | `			ph7_value_string(pTmp,zErr,-1);` |
|     192 |  6856 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      95 |  6857 | `		}` |
|      95 |  6858 | `	}` |
|       - |  6859 | `	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|     287 |  6860 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",` |
|      95 |  6861 | `		nAddr,zAddr,zWarn);` |
|     192 |  6862 | `}` |
|       - |  6863 | `/*` |
|       - |  6864 | ` * The one failure whose message names the HOST, and the one php reports TWICE:` |
|       - |  6865 | ` * its transport raises the text on its own before the opener that asked repeats` |
|       - |  6866 | ` * it inside "Unable to connect to". Composed here because net.c hands back a` |
|       - |  6867 | ` * static string and only the caller has the name to word in.` |
|       - |  6868 | ` */` |
|       4 |  6869 | `static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)` |
|       1 |  6870 | `{` |
|       7 |  6871 | `	SyBufferFormat(zBuf,(sxu32)nBuf,` |
|       2 |  6872 | `		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);` |
|       5 |  6873 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);` |
|       5 |  6874 | `	return zBuf;` |
|       1 |  6875 | `}` |
|       - |  6876 | `/* int (*xStat)(void *,ph7_value *,ph7_value *)` |
|       - |  6877 | ` *` |
|       - |  6878 | ` * php's socket ops stat the DESCRIPTOR, so fstat() on a socket answers whatever` |
|       - |  6879 | ` * the platform's fstat() says about one -- a full record on both boxes here,` |
|       - |  6880 | ` * with the S_IFSOCK mode. The same call, so the same platform answer, failure` |
|       - |  6881 | ` * included. */` |
|     ! 0 |  6882 | `static int SockStreamData_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|     ! 0 |  6883 | `{` |
|     ! 0 |  6884 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     ! 0 |  6885 | `	if( pSock == 0 ){` |
|     ! 0 |  6886 | `		return -1;` |
|       - |  6887 | `	}` |
|       - |  6888 | `#ifdef __WINNT__` |
|       - |  6889 | `	/* php's socket ops do not stat at all on Windows: they SUCCEED with a` |
|       - |  6890 | `	 * zeroed record, so fstat() there is an array of zeros with the -1` |
|       - |  6891 | `	 * blksize/blocks every Windows stat carries. A SOCKET is not a CRT` |
|       - |  6892 | `	 * descriptor either, so asking one would be a call on a descriptor number` |
|       - |  6893 | `	 * nothing opened. (Read back from php 8.5.8 on the gate guest.) */` |
|       - |  6894 | `	{` |
|       - |  6895 | `		ph7_int64 aVal[13];` |
|       - |  6896 | `		int i;` |
|     ! 0 |  6897 | `		for( i = 0 ; i < 11 ; ++i ){ aVal[i] = 0; }` |
|     ! 0 |  6898 | `		aVal[11] = -1;` |
|     ! 0 |  6899 | `		aVal[12] = -1;` |
|       - |  6900 | `		SXUNUSED(pSock);` |
|     ! 0 |  6901 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|       - |  6902 | `	}` |
|       - |  6903 | `#else` |
|     ! 0 |  6904 | `	return PH7_VfsStatFromFd((int)pSock->sock,pArray,pWorker);` |
|       - |  6905 | `#endif` |
|     ! 0 |  6906 | `}` |
|       - |  6907 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|       - |  6908 | `	"tcp",` |
|       - |  6909 | `	PH7_IO_STREAM_VERSION,` |
|       - |  6910 | `	SockStreamData_Open, /* xOpen */` |
|       - |  6911 | `	0,   /* xOpenDir */` |
|       - |  6912 | `	SockStreamData_Close,/* xClose */` |
|       - |  6913 | `	0,  /* xCloseDir */` |
|       - |  6914 | `	SockStreamData_Read, /* xRead */` |
|       - |  6915 | `	0,  /* xReadDir */` |
|       - |  6916 | `	SockStreamData_Write,/* xWrite */` |
|       - |  6917 | `	0,  /* xSeek (sockets are not seekable) */` |
|       - |  6918 | `	0,  /* xLock */` |
|       - |  6919 | `	0,  /* xRewindDir */` |
|       - |  6920 | `	0,  /* xTell: none, so the stream layer's own counter answers ftell() */` |
|       - |  6921 | ``	0,  /* xTrunc: none at all, which is php's `Can't truncate this stream!` */`` |
|       - |  6922 | `	0,  /* xSync */` |
|       - |  6923 | `	SockStreamData_Stat  /* xStat */` |
|       - |  6924 | `};` |
|       - |  6925 | `#endif /* PH7_ENABLE_NET */` |
|       - |  6926 | `/*` |
|       - |  6927 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|       - |  6928 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|       - |  6929 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|       - |  6930 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|       - |  6931 | ` * The handle carries the userland object, and every stream op dispatches the` |
|       - |  6932 | ` * php streamWrapper protocol method on it.` |
|       - |  6933 | ` */` |
|       - |  6934 | `#define PHL_UWRAP_MAX 8` |
|       - |  6935 | `typedef struct uwrap_slot uwrap_slot;` |
|       - |  6936 | `struct uwrap_slot` |
|       - |  6937 | `{` |
|       - |  6938 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|       - |  6939 | `	char zScheme[32];         /* protocol name */` |
|       - |  6940 | `` 	char zClass[512];         /* userland wrapper class, NUL-terminated for the `%s` `` |
|       - |  6941 | `	                           * diagnostics -- but an ANONYMOUS class's name has a` |
|       - |  6942 | `	                           * NUL of its own inside it and a file path behind that,` |
|       - |  6943 | `	                           * which is why the length is kept beside the buffer and` |
|       - |  6944 | `	                           * why the buffer is not 128 bytes any more. Terminator` |
|       - |  6945 | `	                           * and length together are php's two faces of the name:` |
|       - |  6946 | ``	                           * `%s` shows `class@anonymous`, the lookup wants all`` |
|       - |  6947 | `	                           * of it. */` |
|       - |  6948 | `	int nClass;               /* zClass length, NUL bytes included */` |
|       - |  6949 | `	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated` |
|       - |  6950 | `	                           * by allow_url_fopen, INCLUDING it by` |
|       - |  6951 | `	                           * allow_url_include */` |
|       - |  6952 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|       - |  6953 | `};` |
|       - |  6954 | `typedef struct uwrap_handle uwrap_handle;` |
|       - |  6955 | `struct uwrap_handle` |
|       - |  6956 | `{` |
|       - |  6957 | `	ph7_vm *pVm;` |
|       - |  6958 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|       - |  6959 | `	int iSlot;` |
|       - |  6960 | `	int bEof;` |
|       - |  6961 | `};` |
|       - |  6962 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|       - |  6963 | `/*` |
|       - |  6964 | ` * Was this device registered with STREAM_IS_URL? Only a userland wrapper can` |
|       - |  6965 | ` * carry the flag, so the answer is a scan of the registration slots.` |
|       - |  6966 | ` */` |
|   46013 |  6967 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)` |
|       5 |  6968 | `{` |
|       - |  6969 | `	int i;` |
|       - |  6970 | `	/* php marks its own data:// wrapper a URL, and that is the one that matters` |
|       - |  6971 | ``	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI`` |
|       - |  6972 | `	 * itself, which is why php refuses it unless allow_url_include says` |
|       - |  6973 | `	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */` |
|   46013 |  6974 | `	if( pStream && pStream->zName` |
|   46018 |  6975 | `	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){` |
|      71 |  6976 | `		return 1;` |
|       - |  6977 | `	}` |
|       - |  6978 | `#ifdef PH7_ENABLE_NET` |
|       - |  6979 | `	/* http:// is php's STREAM_IS_URL wrapper proper: allow_url_fopen switches it` |
|       - |  6980 | `	 * off wholesale, and allow_url_include -- off by default -- is what stops an` |
|       - |  6981 | ``	 * `include 'http://…'` from executing whatever answered. */`` |
|   45952 |  6982 | `	if( PH7_HttpStreamIs(pStream) ){` |
|     336 |  6983 | `		return 1;` |
|       - |  6984 | `	}` |
|       - |  6985 | `#endif` |
|  409668 |  6986 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  364163 |  6987 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     113 |  6988 | `			return g_aUwrap[i].bIsUrl;` |
|       - |  6989 | `		}` |
|  181498 |  6990 | `	}` |
|   45510 |  6991 | `	return 0;` |
|   22945 |  6992 | `}` |
|       - |  6993 | `/*` |
|       - |  6994 | ` * Is this device one of the userland wrapper slots? php labels every such` |
|       - |  6995 | `` * stream `user-space` rather than by its protocol.`` |
|       - |  6996 | ` */` |
|   12644 |  6997 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream)` |
|       5 |  6998 | `{` |
|       - |  6999 | `	int i;` |
|  113593 |  7000 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  100975 |  7001 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|      27 |  7002 | `			return 1;` |
|       - |  7003 | `		}` |
|   50389 |  7004 | `	}` |
|   12623 |  7005 | `	return 0;` |
|    6316 |  7006 | `}` |
|       - |  7007 | `/*` |
|       - |  7008 | ` * Is this device one of the registration slots at all? Unlike IoPrivateIsUwrap()` |
|       - |  7009 | ` * this does NOT ask whether the slot is still live -- restore() has to tell a` |
|       - |  7010 | ` * withdrawn userland wrapper from a built-in, and a withdrawn slot has already` |
|       - |  7011 | ` * had its pVm cleared.` |
|       - |  7012 | ` */` |
|      18 |  7013 | `static int UwrapIsSlotDevice(const ph7_io_stream *pStream)` |
|       1 |  7014 | `{` |
|       - |  7015 | `	int i;` |
|      99 |  7016 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      89 |  7017 | `		if( &g_aUwrap[i].sStream == pStream ){` |
|       9 |  7018 | `			return 1;` |
|       - |  7019 | `		}` |
|      41 |  7020 | `	}` |
|      11 |  7021 | `	return 0;` |
|      10 |  7022 | `}` |
|       - |  7023 | `/* Forward: the protocol dispatcher is defined just below. */` |
|       - |  7024 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|       - |  7025 | `	ph7_value *pResult);` |
|       - |  7026 | `/*` |
|       - |  7027 | ` * Ask a userland wrapper whether it is at end of file — php's own` |
|       - |  7028 | ` * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring` |
|       - |  7029 | ` * the answer from a zero-length read instead. Returns 0 when the handle is not` |
|       - |  7030 | ` * a userland stream (nothing written to *pAnswer).` |
|       - |  7031 | ` */` |
|   12169 |  7032 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)` |
|       5 |  7033 | `{` |
|       - |  7034 | `	uwrap_handle *pH;` |
|       - |  7035 | `	ph7_value sRet;` |
|   12174 |  7036 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|   12168 |  7037 | `		return 0;` |
|       - |  7038 | `	}` |
|       7 |  7039 | `	pH = (uwrap_handle *)pDev->pHandle;` |
|       7 |  7040 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       7 |  7041 | `	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){` |
|       - |  7042 | `		/* php's streamWrapper requires the method; a class without one keeps` |
|       - |  7043 | `		 * the read-derived answer rather than being called into. */` |
|     ! 0 |  7044 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 |  7045 | `		*pAnswer = pH->bEof;` |
|     ! 0 |  7046 | `		return 1;` |
|       - |  7047 | `	}` |
|       7 |  7048 | `	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;` |
|       7 |  7049 | `	PH7_MemObjRelease(&sRet);` |
|       7 |  7050 | `	return 1;` |
|    6084 |  7051 | `}` |
|       - |  7052 | `/*` |
|       - |  7053 | `` * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),`` |
|       - |  7054 | ` * or 0 for any other device.` |
|       - |  7055 | ` */` |
|     152 |  7056 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)` |
|       5 |  7057 | `{` |
|     157 |  7058 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|     153 |  7059 | `		return 0;` |
|       - |  7060 | `	}` |
|       5 |  7061 | `	return ((uwrap_handle *)pDev->pHandle)->pObj;` |
|      81 |  7062 | `}` |
|       - |  7063 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     336 |  7064 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|       - |  7065 | `	ph7_value *pResult)` |
|       5 |  7066 | `{` |
|       - |  7067 | `	ph7_class_method *pMeth;` |
|     341 |  7068 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|     ! 0 |  7069 | `		return -1;` |
|       - |  7070 | `	}` |
|     341 |  7071 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     341 |  7072 | `	if( pMeth == 0 ){` |
|      42 |  7073 | `		return -1;` |
|       - |  7074 | `	}` |
|     303 |  7075 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|     ! 0 |  7076 | `		return -1;` |
|       - |  7077 | `	}` |
|     303 |  7078 | `	return 0;` |
|     173 |  7079 | `}` |
|      70 |  7080 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|       4 |  7081 | `{` |
|      74 |  7082 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - |  7083 | `	ph7_value sArg,sRet;` |
|       - |  7084 | `	const char *zData;` |
|      74 |  7085 | `	int nData = 0;` |
|      74 |  7086 | `	ph7_int64 n = 0;` |
|      74 |  7087 | `	if( pH == 0 \|\| pH->bEof ){` |
|     ! 0 |  7088 | `		return 0;` |
|       - |  7089 | `	}` |
|      74 |  7090 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      74 |  7091 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      74 |  7092 | `	ph7_value_int64(&sArg,nRead);` |
|       - |  7093 | `	{` |
|       - |  7094 | `		ph7_value *apArg[1];` |
|      74 |  7095 | `		apArg[0] = &sArg;` |
|      74 |  7096 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|     ! 0 |  7097 | `			PH7_MemObjRelease(&sArg);` |
|     ! 0 |  7098 | `			PH7_MemObjRelease(&sRet);` |
|     ! 0 |  7099 | `			return -1;` |
|       - |  7100 | `		}` |
|       - |  7101 | `	}` |
|      74 |  7102 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      74 |  7103 | `	if( nData > 0 ){` |
|      38 |  7104 | `		if( (ph7_int64)nData > nRead ){` |
|     ! 0 |  7105 | `			nData = (int)nRead;` |
|     ! 0 |  7106 | `		}` |
|      38 |  7107 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      38 |  7108 | `		n = nData;` |
|      21 |  7109 | `	}else{` |
|      40 |  7110 | `		pH->bEof = 1;` |
|       - |  7111 | `	}` |
|      74 |  7112 | `	PH7_MemObjRelease(&sArg);` |
|      74 |  7113 | `	PH7_MemObjRelease(&sRet);` |
|      74 |  7114 | `	return n;` |
|      39 |  7115 | `}` |
|       4 |  7116 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|       2 |  7117 | `{` |
|       6 |  7118 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - |  7119 | `	ph7_value sArg,sRet;` |
|       - |  7120 | `	ph7_int64 n;` |
|       6 |  7121 | `	if( pH == 0 ){` |
|     ! 0 |  7122 | `		return -1;` |
|       - |  7123 | `	}` |
|       6 |  7124 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|       6 |  7125 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       6 |  7126 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|       - |  7127 | `	{` |
|       - |  7128 | `		ph7_value *apArg[1];` |
|       6 |  7129 | `		apArg[0] = &sArg;` |
|       6 |  7130 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|     ! 0 |  7131 | `			PH7_MemObjRelease(&sArg);` |
|     ! 0 |  7132 | `			PH7_MemObjRelease(&sRet);` |
|     ! 0 |  7133 | `			return -1;` |
|       - |  7134 | `		}` |
|       - |  7135 | `	}` |
|       6 |  7136 | `	n = ph7_value_to_int64(&sRet);` |
|       6 |  7137 | `	PH7_MemObjRelease(&sArg);` |
|       6 |  7138 | `	PH7_MemObjRelease(&sRet);` |
|       6 |  7139 | `	return n;` |
|       4 |  7140 | `}` |
|       2 |  7141 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|       1 |  7142 | `{` |
|       3 |  7143 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - |  7144 | `	ph7_value sOfft,sWhence,sRet;` |
|       - |  7145 | `	ph7_value *apArg[2];` |
|       - |  7146 | `	int rc;` |
|       3 |  7147 | `	if( pH == 0 ){` |
|     ! 0 |  7148 | `		return -1;` |
|       - |  7149 | `	}` |
|       3 |  7150 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|       3 |  7151 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|       3 |  7152 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|       3 |  7153 | `	ph7_value_int64(&sOfft,iOfft);` |
|       3 |  7154 | `	ph7_value_int(&sWhence,whence);` |
|       3 |  7155 | `	apArg[0] = &sOfft;` |
|       3 |  7156 | `	apArg[1] = &sWhence;` |
|       3 |  7157 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|       3 |  7158 | `	if( rc == 0 ){` |
|       3 |  7159 | `		pH->bEof = 0;` |
|       3 |  7160 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|       1 |  7161 | `	}` |
|       3 |  7162 | `	PH7_MemObjRelease(&sOfft);` |
|       3 |  7163 | `	PH7_MemObjRelease(&sWhence);` |
|       3 |  7164 | `	PH7_MemObjRelease(&sRet);` |
|       3 |  7165 | `	return rc;` |
|       2 |  7166 | `}` |
|      16 |  7167 | `static ph7_int64 UwrapTell(void *pHandle)` |
|       2 |  7168 | `{` |
|      18 |  7169 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - |  7170 | `	ph7_value sRet;` |
|       - |  7171 | `	ph7_int64 n;` |
|      18 |  7172 | `	if( pH == 0 ){` |
|     ! 0 |  7173 | `		return -1;` |
|       - |  7174 | `	}` |
|      18 |  7175 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      18 |  7176 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|      10 |  7177 | `		PH7_MemObjRelease(&sRet);` |
|      10 |  7178 | `		return -1;` |
|       - |  7179 | `	}` |
|      10 |  7180 | `	n = ph7_value_to_int64(&sRet);` |
|      10 |  7181 | `	PH7_MemObjRelease(&sRet);` |
|      10 |  7182 | `	return n;` |
|      10 |  7183 | `}` |
|      58 |  7184 | `static void UwrapClose(void *pHandle)` |
|       4 |  7185 | `{` |
|      62 |  7186 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      62 |  7187 | `	if( pH == 0 ){` |
|     ! 0 |  7188 | `		return;` |
|       - |  7189 | `	}` |
|      62 |  7190 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      62 |  7191 | `	if( pH->pObj ){` |
|      62 |  7192 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      29 |  7193 | `	}` |
|      62 |  7194 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      33 |  7195 | `}` |
|       - |  7196 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|     104 |  7197 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|       5 |  7198 | `{` |
|     109 |  7199 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|     109 |  7200 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|       - |  7201 | `	ph7_class *pClass;` |
|       - |  7202 | `	uwrap_handle *pH;` |
|       - |  7203 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|       - |  7204 | `	ph7_value *apArg[4];` |
|       - |  7205 | `	int rc;` |
|     109 |  7206 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|     ! 0 |  7207 | `		return -1;` |
|       - |  7208 | `	}` |
|     109 |  7209 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)pSlot->nClass,TRUE,0);` |
|     109 |  7210 | `	if( pClass == 0 ){` |
|     ! 0 |  7211 | `		return -1;` |
|       - |  7212 | `	}` |
|     109 |  7213 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|     109 |  7214 | `	if( pH == 0 ){` |
|     ! 0 |  7215 | `		return -1;` |
|       - |  7216 | `	}` |
|     109 |  7217 | `	pH->pVm = pVm;` |
|     109 |  7218 | `	pH->iSlot = iSlot;` |
|     109 |  7219 | `	pH->bEof = 0;` |
|     109 |  7220 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|     109 |  7221 | `	if( pH->pObj == 0 ){` |
|     ! 0 |  7222 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 |  7223 | `		return -1;` |
|       - |  7224 | `	}` |
|       - |  7225 | `	{` |
|       - |  7226 | `		/* php's streamWrapper::$context, set on the serving instance BEFORE` |
|       - |  7227 | `		 * stream_open() runs — which is the whole reason a userland wrapper can` |
|       - |  7228 | `		 * be configured per open. It is exactly what the OPENER resolved: the` |
|       - |  7229 | ``		 * default context substitutes for a NULL `$context` argument, so an`` |
|       - |  7230 | `		 * ordinary fopen() hands a resource over; but an opener with no such` |
|       - |  7231 | `		 * argument at all (md5_file(), include) and one that carried` |
|       - |  7232 | `		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default` |
|       - |  7233 | `		 * here would make that flag mean nothing.` |
|       - |  7234 | `		 * The class need not declare the slot; php adds it either way. */` |
|     109 |  7235 | `		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     109 |  7236 | `		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");` |
|     109 |  7237 | `		if( pCtxSlot == 0 ){` |
|     ! 0 |  7238 | `			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);` |
|     ! 0 |  7239 | `		}` |
|     109 |  7240 | `		if( pCtxSlot ){` |
|     109 |  7241 | `			if( pOpenCtx ){` |
|      91 |  7242 | `				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);` |
|      48 |  7243 | `			}else{` |
|      20 |  7244 | `				ph7_value_null(pCtxSlot);` |
|       - |  7245 | `			}` |
|      52 |  7246 | `		}` |
|       - |  7247 | `	}` |
|       - |  7248 | `	/* php hands stream_open the FULL url, scheme included */` |
|     109 |  7249 | `	PH7_MemObjInit(pVm,&sPath);` |
|     109 |  7250 | `	PH7_MemObjInit(pVm,&sMode);` |
|     109 |  7251 | `	PH7_MemObjInit(pVm,&sOpts);` |
|     109 |  7252 | `	PH7_MemObjInit(pVm,&sRet);` |
|       - |  7253 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|       - |  7254 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|       - |  7255 | `	 * it as "could not be passed by reference"). */` |
|       - |  7256 | `	{` |
|     109 |  7257 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|     109 |  7258 | `		if( pRefSlot == 0 ){` |
|     ! 0 |  7259 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|     ! 0 |  7260 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 |  7261 | `			return -1;` |
|       - |  7262 | `		}` |
|     109 |  7263 | `		PH7_MemObjInit(pVm,&sOpened);` |
|     109 |  7264 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|       - |  7265 | `	}` |
|     104 |  7266 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|      74 |  7267 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|       - |  7268 | `		/* The one scheme php does NOT hand back whole. Its locate_url_wrapper` |
|       - |  7269 | `		 * strips "file://" for whoever owns the name, built-in or not, so a` |
|       - |  7270 | `		 * wrapper that replaced file:// sees the plain path -- the same bytes a` |
|       - |  7271 | `		 * bare path would have given it. */` |
|       3 |  7272 | `		ph7_value_string(&sPath,zName,-1);` |
|       2 |  7273 | `	}else{` |
|       - |  7274 | `		SyBlob sUrl;` |
|     107 |  7275 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     107 |  7276 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|     107 |  7277 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     107 |  7278 | `		SyBlobRelease(&sUrl);` |
|       - |  7279 | `	}` |
|     109 |  7280 | `	if( pVm->zOpenMode[0] ){` |
|       - |  7281 | `		/* fopen()/SplFileObject: php hands over the caller's own spelling. */` |
|      54 |  7282 | `		ph7_value_string(&sMode,pVm->zOpenMode,-1);` |
|      29 |  7283 | `	}else{` |
|       - |  7284 | `		/* Every other opener is C with a fixed mode, and php's own are the three` |
|       - |  7285 | `		 * BINARY spellings below -- file_get_contents()/file()/readfile()/copy's` |
|       - |  7286 | `		 * source and include are "rb", file_put_contents() "wb", and its` |
|       - |  7287 | `		 * FILE_APPEND "ab". Spell the flags back rather than guessing: PHL used` |
|       - |  7288 | `		 * to answer "r" for every one of them, so a wrapper was told a WRITE` |
|       - |  7289 | `		 * open was a read. */` |
|       - |  7290 | `		char zSpell[8];` |
|      59 |  7291 | `		int n = 0;` |
|      59 |  7292 | `		if( iMode & PH7_IO_OPEN_APPEND ){` |
|       3 |  7293 | `			zSpell[n++] = 'a';` |
|      58 |  7294 | `		}else if( iMode & PH7_IO_OPEN_EXCL ){` |
|     ! 0 |  7295 | `			zSpell[n++] = 'x';` |
|      57 |  7296 | `		}else if( iMode & PH7_IO_OPEN_TRUNC ){` |
|       8 |  7297 | `			zSpell[n++] = 'w';` |
|      54 |  7298 | `		}else if( iMode & PH7_IO_OPEN_CREATE ){` |
|     ! 0 |  7299 | `			zSpell[n++] = 'c';` |
|     ! 0 |  7300 | `		}else{` |
|      51 |  7301 | `			zSpell[n++] = 'r';` |
|       - |  7302 | `		}` |
|      59 |  7303 | `		if( iMode & PH7_IO_OPEN_RDWR ){` |
|     ! 0 |  7304 | `			zSpell[n++] = '+';` |
|     ! 0 |  7305 | `		}` |
|      59 |  7306 | `		if( (iMode & PH7_IO_OPEN_TEXT) == 0 ){` |
|      59 |  7307 | `			zSpell[n++] = 'b';` |
|      27 |  7308 | `		}` |
|      59 |  7309 | `		ph7_value_string(&sMode,zSpell,n);` |
|       - |  7310 | `	}` |
|     109 |  7311 | `	ph7_value_int(&sOpts,0);` |
|     109 |  7312 | `	apArg[0] = &sPath;` |
|     109 |  7313 | `	apArg[1] = &sMode;` |
|     109 |  7314 | `	apArg[2] = &sOpts;` |
|     109 |  7315 | `	apArg[3] = &sOpened;` |
|     109 |  7316 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|     109 |  7317 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|      46 |  7318 | `		rc = -1;` |
|      22 |  7319 | `	}` |
|     109 |  7320 | `	PH7_MemObjRelease(&sPath);` |
|     109 |  7321 | `	PH7_MemObjRelease(&sMode);` |
|     109 |  7322 | `	PH7_MemObjRelease(&sOpts);` |
|     109 |  7323 | `	PH7_MemObjRelease(&sOpened);` |
|     109 |  7324 | `	PH7_MemObjRelease(&sRet);` |
|     109 |  7325 | `	if( rc != 0 ){` |
|       - |  7326 | `		/* php's own wording for a wrapper that declined: the call it made, not` |
|       - |  7327 | `		 * an errno the wrapper never set. */` |
|      46 |  7328 | `		PH7_StreamSetOpenErrorCall(pVm,pSlot->zClass,"stream_open");` |
|      46 |  7329 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      46 |  7330 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|      46 |  7331 | `		return -1;` |
|       - |  7332 | `	}` |
|      64 |  7333 | `	*ppHandle = (void *)pH;` |
|      64 |  7334 | `	return PH7_OK;` |
|      57 |  7335 | `}` |
|       - |  7336 | `/*` |
|       - |  7337 | `` * Instantiate the wrapper class of a userland slot and set php's `$context` on it,`` |
|       - |  7338 | ` * the way every dispatch of the protocol does. Answers 0 when the class is gone.` |
|       - |  7339 | ` */` |
|     416 |  7340 | `static ph7_class_instance * UwrapNewInstance(ph7_vm *pVm,uwrap_slot *pSlot,void *pStreamCtx)` |
|       2 |  7341 | `{` |
|     418 |  7342 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)pSlot->nClass,TRUE,0);` |
|       - |  7343 | `	ph7_class_instance *pObj;` |
|       - |  7344 | `	ph7_value *pCtxSlot;` |
|     418 |  7345 | `	if( pClass == 0 ){` |
|     ! 0 |  7346 | `		return 0;` |
|       - |  7347 | `	}` |
|     418 |  7348 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     418 |  7349 | `	if( pObj == 0 ){` |
|     ! 0 |  7350 | `		return 0;` |
|       - |  7351 | `	}` |
|       - |  7352 | `	/* php adds the slot whether or not the class declares it. */` |
|     418 |  7353 | `	pCtxSlot = PH7_NativeAttr(pObj,"context");` |
|     418 |  7354 | `	if( pCtxSlot == 0 ){` |
|     ! 0 |  7355 | `		pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pObj,"context",sizeof("context")-1,0);` |
|     ! 0 |  7356 | `	}` |
|     418 |  7357 | `	if( pCtxSlot ){` |
|     418 |  7358 | `		if( pStreamCtx ){` |
|      29 |  7359 | `			ph7_value_resource(pCtxSlot,pStreamCtx);` |
|      15 |  7360 | `		}else{` |
|     390 |  7361 | `			ph7_value_null(pCtxSlot);` |
|       - |  7362 | `		}` |
|     208 |  7363 | `	}` |
|     418 |  7364 | `	return pObj;` |
|     210 |  7365 | `}` |
|       - |  7366 | `/* The slot a path belongs to, or 0 when no userland wrapper owns it. */` |
|   79601 |  7367 | `static uwrap_slot * UwrapSlotForPath(ph7_vm *pVm,const char *zPath)` |
|       5 |  7368 | `{` |
|   79606 |  7369 | `	const char *zTail = zPath;` |
|       - |  7370 | `	const ph7_io_stream *pDev;` |
|       - |  7371 | `	int i;` |
|   79606 |  7372 | `	if( zPath == 0 ){` |
|     ! 0 |  7373 | `		return 0;` |
|       - |  7374 | `	}` |
|   79606 |  7375 | `	pDev = PH7_VmGetStreamDevice(pVm,&zTail,(int)SyStrlen(zPath));` |
|   79606 |  7376 | `	if( pDev == 0 ){` |
|      51 |  7377 | `		return 0;` |
|       - |  7378 | `	}` |
|  712798 |  7379 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  633647 |  7380 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pDev ){` |
|     402 |  7381 | `			return &g_aUwrap[i];` |
|       - |  7382 | `		}` |
|  316382 |  7383 | `	}` |
|   79156 |  7384 | `	return 0;` |
|   39775 |  7385 | `}` |
|       - |  7386 | `/*` |
|       - |  7387 | ` * php's php_stream_cast() over a USERLAND stream, which is what a caller that` |
|       - |  7388 | ` * wants a real descriptor for an open stream does -- ext/fileinfo asks for one` |
|       - |  7389 | ` * after it has read the bytes, and the answer is visible either way: a wrapper` |
|       - |  7390 | ` * that declares stream_cast() is CALLED (with STREAM_CAST_AS_STREAM, and` |
|       - |  7391 | ` * whatever it answers is discarded here), and one that does not gets php's` |
|       - |  7392 | `` * `%s::stream_cast is not implemented!` warning. Silent for every other kind of`` |
|       - |  7393 | ` * stream, which all have a descriptor of their own.` |
|       - |  7394 | ` */` |
|      22 |  7395 | `PH7_PRIVATE void PH7_StreamUserCast(ph7_context *pCtx,const ph7_io_stream *pStream,void *pHandle)` |
|       1 |  7396 | `{` |
|       - |  7397 | `	uwrap_handle *pH;` |
|       - |  7398 | `	ph7_class_method *pMeth;` |
|      23 |  7399 | `	if( pCtx == 0 \|\| pHandle == 0 \|\| !IoPrivateIsUwrap(pStream) ){` |
|      17 |  7400 | `		return;` |
|       - |  7401 | `	}` |
|       7 |  7402 | `	pH = (uwrap_handle *)pHandle;` |
|       7 |  7403 | `	if( pH->pObj == 0 ){` |
|     ! 0 |  7404 | `		return;` |
|       - |  7405 | `	}` |
|       7 |  7406 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,"stream_cast",sizeof("stream_cast")-1);` |
|       7 |  7407 | `	if( pMeth == 0 ){` |
|      10 |  7408 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 |  7409 | `			"%z::stream_cast is not implemented!",&pH->pObj->pClass->sDisp);` |
|       7 |  7410 | `		return;` |
|       - |  7411 | `	}` |
|       - |  7412 | `	{` |
|       - |  7413 | `		ph7_value sAs,sRet;` |
|       - |  7414 | `		ph7_value *apArg[1];` |
|     ! 0 |  7415 | `		PH7_MemObjInit(pH->pVm,&sAs);` |
|     ! 0 |  7416 | `		PH7_MemObjInit(pH->pVm,&sRet);` |
|     ! 0 |  7417 | `		ph7_value_int(&sAs,PH7_STREAM_CAST_AS_STREAM);` |
|     ! 0 |  7418 | `		apArg[0] = &sAs;` |
|     ! 0 |  7419 | `		PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,&sRet,1,apArg);` |
|     ! 0 |  7420 | `		PH7_MemObjRelease(&sAs);` |
|     ! 0 |  7421 | `		PH7_MemObjRelease(&sRet);` |
|       - |  7422 | `	}` |
|      12 |  7423 | `}` |
|       - |  7424 | `/*` |
|       - |  7425 | ` * php's WRITE door for a userland wrapper: unlink(), rename(), mkdir(), rmdir() and` |
|       - |  7426 | ` * stream_metadata() -- what touch(), chmod(), chown() and chgrp() all become.` |
|       - |  7427 | ` *` |
|       - |  7428 | ``  * PHL sent every one of them to the OS instead, so `unlink('vfs://root/t.txt')` `` |
|       - |  7429 | `` * reported `No such file or directory` about a path the OS had never heard of and`` |
|       - |  7430 | ` * the wrapper was never told. php dispatches the method with the FULL url first and` |
|       - |  7431 | ` * the operation's own extra arguments after it, and answers the bool the wrapper` |
|       - |  7432 | ` * gives back.` |
|       - |  7433 | ` *` |
|       - |  7434 | ` * Same three answers as the stat door -- NOWRAP when nothing owns the path, OK with` |
|       - |  7435 | `` * *pbAnswer set, FAIL after php's `%s::%s is not implemented!` -- plus the raw`` |
|       - |  7436 | ` * unwound status when the wrapper threw.` |
|       - |  7437 | ` */` |
|   52506 |  7438 | `PH7_PRIVATE int PH7_StreamUserPathOp(ph7_context *pCtx,const char *zPath,const char *zMethod,` |
|       - |  7439 | `	void *pStreamCtx,ph7_value **apExtra,int nExtra,int *pbAnswer)` |
|       5 |  7440 | `{` |
|       - |  7441 | `	ph7_vm *pVm;` |
|       - |  7442 | `	uwrap_slot *pSlot;` |
|       - |  7443 | `	ph7_class_instance *pObj;` |
|       - |  7444 | `	ph7_class_method *pMeth;` |
|       - |  7445 | `	ph7_value sUrl,sRet;` |
|       - |  7446 | `	ph7_value *apArg[4];` |
|       - |  7447 | `	int i,rc,iRet;` |
|   52511 |  7448 | `	if( pCtx == 0 \|\| zPath == 0 \|\| nExtra > 3 ){` |
|     ! 0 |  7449 | `		return PHL_URLSTAT_NOWRAP;` |
|       - |  7450 | `	}` |
|   52511 |  7451 | `	pVm = pCtx->pVm;` |
|   52511 |  7452 | `	pSlot = UwrapSlotForPath(pVm,zPath);` |
|   52511 |  7453 | `	if( pSlot == 0 ){` |
|   52439 |  7454 | `		return PHL_URLSTAT_NOWRAP;` |
|       - |  7455 | `	}` |
|      74 |  7456 | `	pObj = UwrapNewInstance(pVm,pSlot,pStreamCtx);` |
|      74 |  7457 | `	if( pObj == 0 ){` |
|     ! 0 |  7458 | `		return PHL_URLSTAT_FAIL;` |
|       - |  7459 | `	}` |
|      74 |  7460 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|      74 |  7461 | `	if( pMeth == 0 ){` |
|      16 |  7462 | `		PH7_ClassInstanceUnref(pObj);` |
|      23 |  7463 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      14 |  7464 | `			"%s::%s is not implemented!",pSlot->zClass,zMethod);` |
|      16 |  7465 | `		return PHL_URLSTAT_FAIL;` |
|       - |  7466 | `	}` |
|      60 |  7467 | `	PH7_MemObjInit(pVm,&sUrl);` |
|      60 |  7468 | `	PH7_MemObjInit(pVm,&sRet);` |
|      60 |  7469 | `	ph7_value_string(&sUrl,zPath,-1);` |
|      60 |  7470 | `	apArg[0] = &sUrl;` |
|     160 |  7471 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|     102 |  7472 | `		apArg[i+1] = apExtra[i];` |
|      52 |  7473 | `	}` |
|      60 |  7474 | `	rc = PH7_VmCallClassMethod(pVm,pObj,pMeth,&sRet,nExtra+1,apArg);` |
|      60 |  7475 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 |  7476 | `		iRet = rc;` |
|      60 |  7477 | `	}else if( rc == SXRET_OK ){` |
|      60 |  7478 | `		*pbAnswer = ph7_value_to_bool(&sRet);` |
|      60 |  7479 | `		iRet = PHL_URLSTAT_OK;` |
|      31 |  7480 | `	}else{` |
|     ! 0 |  7481 | `		iRet = PHL_URLSTAT_FAIL;` |
|       - |  7482 | `	}` |
|      60 |  7483 | `	PH7_MemObjRelease(&sUrl);` |
|      60 |  7484 | `	PH7_MemObjRelease(&sRet);` |
|      60 |  7485 | `	PH7_ClassInstanceUnref(pObj);` |
|      60 |  7486 | `	return iRet;` |
|   26232 |  7487 | `}` |
|       - |  7488 | `/*` |
|       - |  7489 | ` * php's streamWrapper::url_stat(): the STAT door of a userland wrapper.` |
|       - |  7490 | ` *` |
|       - |  7491 | ` * PHL's stat family went straight to the OS VFS for every path, so a path a script's` |
|       - |  7492 | `` * own wrapper owns -- `vfs://root/t.txt`, the shape every test suite that fakes a`` |
|       - |  7493 | ` * filesystem writes -- answered "does not exist" even though fopen() on the same name` |
|       - |  7494 | `` * worked. php asks the wrapper instead: `url_stat($url, $flags)`, with the FULL url`` |
|       - |  7495 | ` * and php's own flag bits, and reads php's thirteen NAMED fields off the array it` |
|       - |  7496 | ` * gets back (the numeric half a wrapper usually merges in is IGNORED, and a field it` |
|       - |  7497 | ` * omits reads 0).` |
|       - |  7498 | ` *` |
|       - |  7499 | ` * Nothing but plain integers crosses the call: the wrapper is PHP code, and every` |
|       - |  7500 | ` * ph7_value the caller holds is invalidated by running some (see the` |
|       - |  7501 | `` * `pointers-die-across-a-user-callback` rule), so the answer leaves here as aVal[13]`` |
|       - |  7502 | ` * and the caller builds its array afterwards.` |
|       - |  7503 | ` *` |
|       - |  7504 | ` * Answers PHL_URLSTAT_NOWRAP when no userland wrapper owns the path -- the caller` |
|       - |  7505 | ` * asks the VFS exactly as before -- PHL_URLSTAT_OK when the wrapper filled aVal, and` |
|       - |  7506 | ` * PHL_URLSTAT_FAIL when it declined. A wrapper with no url_stat at all is php's own` |
|       - |  7507 | `` * `%s::url_stat is not implemented!` warning, raised whatever the QUIET flag says,`` |
|       - |  7508 | ` * and then a failure; one that THREW hands the raw unwound status back.` |
|       - |  7509 | ` */` |
|   27095 |  7510 | `PH7_PRIVATE int PH7_StreamUserUrlStat(ph7_context *pCtx,const char *zPath,int iFlags,` |
|       - |  7511 | `	ph7_int64 *aVal)` |
|       5 |  7512 | `{` |
|       - |  7513 | `	static const char * const azField[] = {` |
|       - |  7514 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|       - |  7515 | `		"atime","mtime","ctime","blksize","blocks"` |
|       - |  7516 | `	};` |
|       - |  7517 | `	ph7_vm *pVm;` |
|       - |  7518 | `	uwrap_slot *pSlot;` |
|       - |  7519 | `	ph7_class_instance *pObj;` |
|       - |  7520 | `	ph7_class_method *pMeth;` |
|       - |  7521 | `	ph7_value sUrl,sFlags,sRet;` |
|       - |  7522 | `	ph7_value *apArg[2];` |
|       - |  7523 | `	int i,rc,iRet;` |
|   27100 |  7524 | `	if( pCtx == 0 \|\| zPath == 0 ){` |
|     ! 0 |  7525 | `		return PHL_URLSTAT_NOWRAP;` |
|       - |  7526 | `	}` |
|   27100 |  7527 | `	pVm = pCtx->pVm;` |
|   27100 |  7528 | `	pSlot = UwrapSlotForPath(pVm,zPath);` |
|   27100 |  7529 | `	if( pSlot == 0 ){` |
|   26772 |  7530 | `		return PHL_URLSTAT_NOWRAP;` |
|       - |  7531 | `	}` |
|       - |  7532 | ``	/* A stat has no opener behind it, so `$context` is php's NULL. */`` |
|     329 |  7533 | `	pObj = UwrapNewInstance(pVm,pSlot,0);` |
|     329 |  7534 | `	if( pObj == 0 ){` |
|     ! 0 |  7535 | `		return PHL_URLSTAT_FAIL;` |
|       - |  7536 | `	}` |
|     329 |  7537 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,"url_stat",sizeof("url_stat")-1);` |
|     329 |  7538 | `	if( pMeth == 0 ){` |
|       - |  7539 | `		/* php's own sentence, and it is raised even for a QUIET ask -- it reports the` |
|       - |  7540 | `		 * wrapper's own incompleteness, not the path's absence. */` |
|       9 |  7541 | `		PH7_ClassInstanceUnref(pObj);` |
|      13 |  7542 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       8 |  7543 | `			"%s::url_stat is not implemented!",pSlot->zClass);` |
|       9 |  7544 | `		return PHL_URLSTAT_FAIL;` |
|       - |  7545 | `	}` |
|     321 |  7546 | `	PH7_MemObjInit(pVm,&sUrl);` |
|     321 |  7547 | `	PH7_MemObjInit(pVm,&sFlags);` |
|     321 |  7548 | `	PH7_MemObjInit(pVm,&sRet);` |
|     321 |  7549 | `	ph7_value_string(&sUrl,zPath,-1);` |
|     321 |  7550 | `	ph7_value_int(&sFlags,iFlags);` |
|     321 |  7551 | `	apArg[0] = &sUrl;` |
|     321 |  7552 | `	apArg[1] = &sFlags;` |
|     321 |  7553 | `	rc = PH7_VmCallClassMethod(pVm,pObj,pMeth,&sRet,2,apArg);` |
|     321 |  7554 | `	iRet = PHL_URLSTAT_FAIL;` |
|     321 |  7555 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  7556 | `		/* The wrapper THREW. That is not a "the path is missing" answer: php lets` |
|       - |  7557 | `		 * the exception out of the stat, so the raw status travels back and a` |
|       - |  7558 | `		 * caller that would otherwise raise one of its OWN (SplFileInfo's` |
|       - |  7559 | `		 * RuntimeException) propagates this one instead. PH7_EXCEPTION and` |
|       - |  7560 | `		 * PH7_ABORT are both distinct from the three answers above, so they need` |
|       - |  7561 | `		 * no channel of their own. */` |
|       9 |  7562 | `		iRet = rc;` |
|     317 |  7563 | `	}else if( rc == SXRET_OK && ph7_value_is_array(&sRet) ){` |
|    3837 |  7564 | `		for( i = 0 ; i < (int)SX_ARRAYSIZE(azField) ; ++i ){` |
|    3563 |  7565 | `			ph7_value *pField = ph7_array_fetch(&sRet,azField[i],-1);` |
|    3563 |  7566 | `			aVal[i] = pField ? ph7_value_to_int64(pField) : 0;` |
|    1782 |  7567 | `		}` |
|       - |  7568 | `#ifdef __WINNT__` |
|       - |  7569 | `		/* php's Windows stat record has neither field, so it never reads the` |
|       - |  7570 | `		 * wrapper's two and reports -1 for both, as on every other stream. */` |
|       1 |  7571 | `		aVal[11] = -1;` |
|       1 |  7572 | `		aVal[12] = -1;` |
|       - |  7573 | `#endif` |
|     275 |  7574 | `		iRet = PHL_URLSTAT_OK;` |
|     137 |  7575 | `	}` |
|     321 |  7576 | `	PH7_MemObjRelease(&sUrl);` |
|     321 |  7577 | `	PH7_MemObjRelease(&sFlags);` |
|     321 |  7578 | `	PH7_MemObjRelease(&sRet);` |
|     321 |  7579 | `	PH7_ClassInstanceUnref(pObj);` |
|     321 |  7580 | `	return iRet;` |
|   13548 |  7581 | `}` |
|       - |  7582 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|       - |  7583 | `#define PHL_UWRAP_THUNK(N) \` |
|       - |  7584 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|       - |  7585 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|     105 |  7586 | `PHL_UWRAP_THUNK(0)` |
|       5 |  7587 | `PHL_UWRAP_THUNK(1)` |
|     ! 0 |  7588 | `PHL_UWRAP_THUNK(2)` |
|     ! 0 |  7589 | `PHL_UWRAP_THUNK(3)` |
|     ! 0 |  7590 | `PHL_UWRAP_THUNK(4)` |
|     ! 0 |  7591 | `PHL_UWRAP_THUNK(5)` |
|     ! 0 |  7592 | `PHL_UWRAP_THUNK(6)` |
|     ! 0 |  7593 | `PHL_UWRAP_THUNK(7)` |
|       - |  7594 | `/* Slot index -> its own opener */` |
|       - |  7595 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|       - |  7596 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|       - |  7597 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|       - |  7598 | `};` |
|       - |  7599 | `/*` |
|       - |  7600 | ` * php's DIRECTORY door for a userland wrapper: dir_opendir(), dir_readdir(),` |
|       - |  7601 | ` * dir_rewinddir() and dir_closedir().` |
|       - |  7602 | ` *` |
|       - |  7603 | `` * The slots carried no directory ops at all, so `opendir('vfs://root')` failed with`` |
|       - |  7604 | `` * `Failed to open directory: not implemented` and every reader built on it --`` |
|       - |  7605 | ` * scandir(), dir(), DirectoryIterator, FilesystemIterator -- failed with it. php` |
|       - |  7606 | `` * hands the opener the FULL url and its `$options` (0 for every caller that reaches`` |
|       - |  7607 | ` * here), then reads NAMES one at a time until the wrapper answers false. The list is` |
|       - |  7608 | `` * used exactly as given: php synthesizes no `.` or `..` for a userland wrapper.`` |
|       - |  7609 | ` */` |
|      16 |  7610 | `static int UwrapOpenDirSlot(int iSlot,const char *zName,ph7_value *pResource,void **ppHandle)` |
|       1 |  7611 | `{` |
|      17 |  7612 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      17 |  7613 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|       - |  7614 | `	uwrap_handle *pH;` |
|       - |  7615 | `	ph7_class_method *pMeth;` |
|       - |  7616 | `	ph7_value sPath,sOpts,sRet;` |
|       - |  7617 | `	ph7_value *apArg[2];` |
|       - |  7618 | `	int rc;` |
|      17 |  7619 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|     ! 0 |  7620 | `		return -1;` |
|       - |  7621 | `	}` |
|      17 |  7622 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      17 |  7623 | `	if( pH == 0 ){` |
|     ! 0 |  7624 | `		return -1;` |
|       - |  7625 | `	}` |
|      17 |  7626 | `	pH->pVm = pVm;` |
|      17 |  7627 | `	pH->iSlot = iSlot;` |
|      17 |  7628 | `	pH->bEof = 0;` |
|      17 |  7629 | `	pH->pObj = UwrapNewInstance(pVm,pSlot,pVm->pOpenCtx);` |
|      17 |  7630 | `	if( pH->pObj == 0 ){` |
|     ! 0 |  7631 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|     ! 0 |  7632 | `		return -1;` |
|       - |  7633 | `	}` |
|      17 |  7634 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,"dir_opendir",sizeof("dir_opendir")-1);` |
|      17 |  7635 | `	if( pMeth == 0 ){` |
|       3 |  7636 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       3 |  7637 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|       3 |  7638 | `		return -1;` |
|       - |  7639 | `	}` |
|       - |  7640 | `	/* php hands the opener the FULL url, scheme included -- with the one exception` |
|       - |  7641 | `	 * every other dispatch makes for a wrapper that replaced file://. */` |
|      15 |  7642 | `	PH7_MemObjInit(pVm,&sPath);` |
|      15 |  7643 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      15 |  7644 | `	PH7_MemObjInit(pVm,&sRet);` |
|      14 |  7645 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|       8 |  7646 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|     ! 0 |  7647 | `		ph7_value_string(&sPath,zName,-1);` |
|     ! 0 |  7648 | `	}else{` |
|       - |  7649 | `		SyBlob sUrl;` |
|      15 |  7650 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      15 |  7651 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      15 |  7652 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      15 |  7653 | `		SyBlobRelease(&sUrl);` |
|       - |  7654 | `	}` |
|      15 |  7655 | `	ph7_value_int(&sOpts,0);` |
|      15 |  7656 | `	apArg[0] = &sPath;` |
|      15 |  7657 | `	apArg[1] = &sOpts;` |
|      15 |  7658 | `	rc = UwrapCall(pH,"dir_opendir",2,apArg,&sRet);` |
|      15 |  7659 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|       3 |  7660 | `		rc = -1;` |
|       1 |  7661 | `	}` |
|      15 |  7662 | `	PH7_MemObjRelease(&sPath);` |
|      15 |  7663 | `	PH7_MemObjRelease(&sOpts);` |
|      15 |  7664 | `	PH7_MemObjRelease(&sRet);` |
|      15 |  7665 | `	if( rc != 0 ){` |
|       3 |  7666 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       3 |  7667 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|       3 |  7668 | `		return -1;` |
|       - |  7669 | `	}` |
|      13 |  7670 | `	*ppHandle = (void *)pH;` |
|      13 |  7671 | `	return PH7_OK;` |
|       9 |  7672 | `}` |
|       - |  7673 | `/* One xOpenDir thunk per slot, for the same reason xOpen needs one. */` |
|       - |  7674 | `#define PHL_UWRAP_DIR_THUNK(N) \` |
|       - |  7675 | `	static int UwrapOpenDir##N(const char *zName,ph7_value *pResource,void **ppHandle) \` |
|       - |  7676 | `	{ return UwrapOpenDirSlot(N,zName,pResource,ppHandle); }` |
|      15 |  7677 | `PHL_UWRAP_DIR_THUNK(0)` |
|       3 |  7678 | `PHL_UWRAP_DIR_THUNK(1)` |
|     ! 0 |  7679 | `PHL_UWRAP_DIR_THUNK(2)` |
|     ! 0 |  7680 | `PHL_UWRAP_DIR_THUNK(3)` |
|     ! 0 |  7681 | `PHL_UWRAP_DIR_THUNK(4)` |
|     ! 0 |  7682 | `PHL_UWRAP_DIR_THUNK(5)` |
|     ! 0 |  7683 | `PHL_UWRAP_DIR_THUNK(6)` |
|     ! 0 |  7684 | `PHL_UWRAP_DIR_THUNK(7)` |
|       - |  7685 | `static int (* const g_aUwrapOpenDir[PHL_UWRAP_MAX])(const char *,ph7_value *,void **) = {` |
|       - |  7686 | `	UwrapOpenDir0,UwrapOpenDir1,UwrapOpenDir2,UwrapOpenDir3,` |
|       - |  7687 | `	UwrapOpenDir4,UwrapOpenDir5,UwrapOpenDir6,UwrapOpenDir7` |
|       - |  7688 | `};` |
|       - |  7689 | `/*` |
|       - |  7690 | ` * One entry. The VFS contract reports a name by WRITING the call context's result,` |
|       - |  7691 | ` * and answers anything but PH7_OK to end the walk -- which is what the wrapper's own` |
|       - |  7692 | `` * `false` means.`` |
|       - |  7693 | ` */` |
|      46 |  7694 | `static int UwrapReadDir(void *pHandle,ph7_context *pCtx)` |
|       1 |  7695 | `{` |
|      47 |  7696 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       - |  7697 | `	ph7_value sRet;` |
|       - |  7698 | `	const char *zName;` |
|      47 |  7699 | `	int nName = 0;` |
|      47 |  7700 | `	if( pH == 0 ){` |
|     ! 0 |  7701 | `		return -1;` |
|       - |  7702 | `	}` |
|      47 |  7703 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      47 |  7704 | `	if( UwrapCall(pH,"dir_readdir",0,0,&sRet) != 0 ){` |
|     ! 0 |  7705 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 |  7706 | `		return -1;` |
|       - |  7707 | `	}` |
|      47 |  7708 | `	if( (sRet.iFlags & MEMOBJ_BOOL) && sRet.x.iVal == 0 ){` |
|       - |  7709 | `		/* php's end of the walk. */` |
|      11 |  7710 | `		PH7_MemObjRelease(&sRet);` |
|      11 |  7711 | `		return -1;` |
|       - |  7712 | `	}` |
|      37 |  7713 | `	zName = ph7_value_to_string(&sRet,&nName);` |
|      37 |  7714 | `	if( nName < 1 ){` |
|     ! 0 |  7715 | `		PH7_MemObjRelease(&sRet);` |
|     ! 0 |  7716 | `		return -1;` |
|       - |  7717 | `	}` |
|      37 |  7718 | `	ph7_result_string(pCtx,zName,nName);` |
|      37 |  7719 | `	PH7_MemObjRelease(&sRet);` |
|      37 |  7720 | `	return PH7_OK;` |
|      24 |  7721 | `}` |
|       4 |  7722 | `static void UwrapRewindDir(void *pHandle)` |
|       1 |  7723 | `{` |
|       5 |  7724 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|       5 |  7725 | `	if( pH ){` |
|       5 |  7726 | `		UwrapCall(pH,"dir_rewinddir",0,0,0);` |
|       2 |  7727 | `	}` |
|       5 |  7728 | `}` |
|      12 |  7729 | `static void UwrapCloseDir(void *pHandle)` |
|       1 |  7730 | `{` |
|      13 |  7731 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      13 |  7732 | `	if( pH == 0 ){` |
|     ! 0 |  7733 | `		return;` |
|       - |  7734 | `	}` |
|      13 |  7735 | `	UwrapCall(pH,"dir_closedir",0,0,0);` |
|      13 |  7736 | `	if( pH->pObj ){` |
|      13 |  7737 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|       6 |  7738 | `	}` |
|      13 |  7739 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|       7 |  7740 | `}` |
|       - |  7741 | `/*` |
|       - |  7742 | ` * Why did a userland wrapper's directory open fail? php names the METHOD, and says` |
|       - |  7743 | `` * whether the wrapper has one at all -- `"C::dir_opendir" call failed` against`` |
|       - |  7744 | `` * `"C::dir_opendir" is not implemented`. Answers 0 when pStream is not one of ours,`` |
|       - |  7745 | ` * and the caller then keeps the C library's own errno text.` |
|       - |  7746 | ` */` |
|      24 |  7747 | `PH7_PRIVATE int PH7_StreamUserDirReason(ph7_vm *pVm,const ph7_io_stream *pStream,` |
|       - |  7748 | `	char *zBuf,int nBuf)` |
|       4 |  7749 | `{` |
|       - |  7750 | `	int i;` |
|     190 |  7751 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|       - |  7752 | `		ph7_class *pClass;` |
|       - |  7753 | `		int bHas;` |
|     170 |  7754 | `		if( g_aUwrap[i].pVm != pVm \|\| &g_aUwrap[i].sStream != pStream ){` |
|     166 |  7755 | `			continue;` |
|       - |  7756 | `		}` |
|       7 |  7757 | `		pClass = PH7_VmExtractClass(pVm,g_aUwrap[i].zClass,` |
|       4 |  7758 | `			(sxu32)g_aUwrap[i].nClass,TRUE,0);` |
|       7 |  7759 | `		bHas = pClass != 0` |
|       4 |  7760 | `			&& PH7_ClassExtractMethod(pClass,"dir_opendir",sizeof("dir_opendir")-1) != 0;` |
|       7 |  7761 | `		SyBufferFormat(zBuf,(sxu32)nBuf,"\"%s::dir_opendir\" %s",g_aUwrap[i].zClass,` |
|       2 |  7762 | `			bHas ? "call failed" : "is not implemented");` |
|       5 |  7763 | `		return 1;` |
|     ! 0 |  7764 | `	}` |
|      23 |  7765 | `	return 0;` |
|      16 |  7766 | `}` |
|      46 |  7767 | `static int UwrapDeviceInstalled(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 |  7768 | `{` |
|      51 |  7769 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|       - |  7770 | `	sxu32 n;` |
|     525 |  7771 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     489 |  7772 | `		if( apDev[n] == pStream ){` |
|      12 |  7773 | `			return 1;` |
|       - |  7774 | `		}` |
|     242 |  7775 | `	}` |
|      41 |  7776 | `	return 0;` |
|      28 |  7777 | `}` |
|       - |  7778 | `/* Put a device back in service. */` |
|      46 |  7779 | `static void UwrapUnsuppressDevice(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|       5 |  7780 | `{` |
|      51 |  7781 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|      51 |  7782 | `	sxu32 n,nKeep = 0;` |
|      71 |  7783 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      22 |  7784 | `		if( apOff[n] == pStream ){` |
|      12 |  7785 | `			continue;` |
|       - |  7786 | `		}` |
|      12 |  7787 | `		apOff[nKeep++] = apOff[n];` |
|       7 |  7788 | `	}` |
|      51 |  7789 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|      51 |  7790 | `}` |
|       - |  7791 | `/*` |
|       - |  7792 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|       - |  7793 | ` * bool stream_wrapper_unregister(string $protocol)` |
|       - |  7794 | ` */` |
|      46 |  7795 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  7796 | `{` |
|       - |  7797 | `	const char *zScheme,*zClass;` |
|      51 |  7798 | `	int nScheme,nClass,i,iFree = -1;` |
|      51 |  7799 | `	if( nArg < 2 ){` |
|     ! 0 |  7800 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  7801 | `		return PH7_OK;` |
|       - |  7802 | `	}` |
|      51 |  7803 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      51 |  7804 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      46 |  7805 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      51 |  7806 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|     ! 0 |  7807 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  7808 | `		return PH7_OK;` |
|       - |  7809 | `	}` |
|       - |  7810 | `	/* php: registering an already-taken protocol warns and returns false.` |
|       - |  7811 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|       - |  7812 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|       - |  7813 | `	{` |
|      51 |  7814 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|       - |  7815 | `		sxu32 n;` |
|     541 |  7816 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     495 |  7817 | `			if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|      22 |  7818 | `				continue; /* unregistered: the name is free again, which is the` |
|       - |  7819 | `				           * whole point of "replace file:// with my own" */` |
|       - |  7820 | `			}` |
|     470 |  7821 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     278 |  7822 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|     ! 0 |  7823 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 |  7824 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|     ! 0 |  7825 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 |  7826 | `				return PH7_OK;` |
|       - |  7827 | `			}` |
|     240 |  7828 | `		}` |
|       - |  7829 | `	}` |
|      65 |  7830 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      65 |  7831 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      51 |  7832 | `			iFree = i;` |
|      51 |  7833 | `			break;` |
|       - |  7834 | `		}` |
|      11 |  7835 | `	}` |
|      51 |  7836 | `	if( iFree < 0 ){` |
|     ! 0 |  7837 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - |  7838 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|     ! 0 |  7839 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  7840 | `		return PH7_OK;` |
|       - |  7841 | `	}` |
|       - |  7842 | `	{` |
|      51 |  7843 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      51 |  7844 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      51 |  7845 | `		pSlot->zScheme[nScheme] = 0;` |
|      51 |  7846 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      51 |  7847 | `		pSlot->zClass[nClass] = 0;` |
|      51 |  7848 | `		pSlot->nClass = nClass;` |
|      51 |  7849 | `		pSlot->pVm = pCtx->pVm;` |
|       - |  7850 | `		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the` |
|       - |  7851 | `		 * whole reason the argument exists — a wrapper that says it speaks to the` |
|       - |  7852 | `		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was` |
|       - |  7853 | `		 * declared in the signature and read by nothing, so a wrapper registered as` |
|       - |  7854 | `		 * a URL was opened and INCLUDED like a local file whatever the` |
|       - |  7855 | `		 * configuration said. */` |
|      51 |  7856 | `		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);` |
|      51 |  7857 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      51 |  7858 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      51 |  7859 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      51 |  7860 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      51 |  7861 | `		pSlot->sStream.xOpenDir = g_aUwrapOpenDir[iFree];` |
|      51 |  7862 | `		pSlot->sStream.xCloseDir = UwrapCloseDir;` |
|      51 |  7863 | `		pSlot->sStream.xReadDir = UwrapReadDir;` |
|      51 |  7864 | `		pSlot->sStream.xRewindDir = UwrapRewindDir;` |
|      51 |  7865 | `		pSlot->sStream.xClose = UwrapClose;` |
|      51 |  7866 | `		pSlot->sStream.xRead = UwrapRead;` |
|      51 |  7867 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      51 |  7868 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      51 |  7869 | `		pSlot->sStream.xTell = UwrapTell;` |
|       - |  7870 | `		/* A slot is REUSED once its wrapper has been unregistered, and both the` |
|       - |  7871 | `		 * suppression set and the VM's device list still name it -- so lift the` |
|       - |  7872 | `		 * suppression and install the device only if it is not already there,` |
|       - |  7873 | `		 * or the freshly registered protocol would be born switched off (and` |
|       - |  7874 | `		 * listed twice). */` |
|      51 |  7875 | `		UwrapUnsuppressDevice(pCtx->pVm,&pSlot->sStream);` |
|      51 |  7876 | `		if( !UwrapDeviceInstalled(pCtx->pVm,&pSlot->sStream) ){` |
|      41 |  7877 | `			ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      18 |  7878 | `		}` |
|       - |  7879 | `	}` |
|      51 |  7880 | `	ph7_result_bool(pCtx,1);` |
|      51 |  7881 | `	return PH7_OK;` |
|      28 |  7882 | `}` |
|       - |  7883 | `/*` |
|       - |  7884 | ` * Suppress a live device and, when it is a userland slot, retire the slot with` |
|       - |  7885 | ` * it. Answers 0 when nothing by that name was in service.` |
|       - |  7886 | ` *` |
|       - |  7887 | ` * The match is EXACT and case-SENSITIVE, which php's is too: opening a stream` |
|       - |  7888 | ` * folds the scheme ("FILE://x" reads a file), but unregister() and restore()` |
|       - |  7889 | ` * delete from the wrapper hash by the bytes the script wrote, so` |
|       - |  7890 | ` * stream_wrapper_unregister('FILE') fails where 'file' succeeds.` |
|       - |  7891 | ` */` |
|      28 |  7892 | `static int UwrapSuppressDevice(ph7_vm *pVm,const char *zScheme,int nScheme)` |
|       4 |  7893 | `{` |
|      32 |  7894 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|      32 |  7895 | `	ph7_io_stream *pHit = 0;` |
|       - |  7896 | `	sxu32 n;` |
|       - |  7897 | `	int i;` |
|     358 |  7898 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     326 |  7899 | `		if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     205 |  7900 | `		 && SyMemcmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0` |
|      58 |  7901 | `		 && !PH7_VmStreamDeviceSuppressed(pVm,apDev[n]) ){` |
|      28 |  7902 | `			pHit = apDev[n]; /* the LIVE one is the last match */` |
|      12 |  7903 | `		}` |
|     167 |  7904 | `	}` |
|      32 |  7905 | `	if( pHit == 0 ){` |
|       5 |  7906 | `		return 0;` |
|       - |  7907 | `	}` |
|      28 |  7908 | `	if( SySetPut(&pVm->aSuppressedIo,(const void *)&pHit) != SXRET_OK ){` |
|     ! 0 |  7909 | `		return 0;` |
|       - |  7910 | `	}` |
|      68 |  7911 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      64 |  7912 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pHit ){` |
|      24 |  7913 | `			g_aUwrap[i].pVm = 0;` |
|      24 |  7914 | `			break;` |
|       - |  7915 | `		}` |
|      22 |  7916 | `	}` |
|      28 |  7917 | `	return 1;` |
|      18 |  7918 | `}` |
|       - |  7919 | `/*` |
|       - |  7920 | ` * bool stream_wrapper_unregister(string $protocol)` |
|       - |  7921 | ` *  Take a protocol out of service. It used to handle USERLAND slots only and` |
|       - |  7922 | ` *  answer FALSE for file/php/data/tcp, so the documented "replace file:// with` |
|       - |  7923 | ` *  my own wrapper" idiom failed loudly at the first step. A built-in is now` |
|       - |  7924 | ` *  suppressed per VM: PH7_VmGetStreamDevice() steps over it (including on the` |
|       - |  7925 | ` *  no-scheme default path, which is the same slot), stream_get_wrappers() stops` |
|       - |  7926 | ` *  naming it, and the name becomes free to register again.` |
|       - |  7927 | ` */` |
|      28 |  7928 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  7929 | `{` |
|       - |  7930 | `	const char *zScheme;` |
|       - |  7931 | `	int nScheme;` |
|      32 |  7932 | `	if( nArg < 1 ){` |
|     ! 0 |  7933 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  7934 | `		return PH7_OK;` |
|       - |  7935 | `	}` |
|      32 |  7936 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      32 |  7937 | `	if( nScheme > 0 && UwrapSuppressDevice(pCtx->pVm,zScheme,nScheme) ){` |
|      28 |  7938 | `		ph7_result_bool(pCtx,1);` |
|      28 |  7939 | `		return PH7_OK;` |
|       - |  7940 | `	}` |
|       7 |  7941 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 |  7942 | `		"Unable to unregister protocol %.*s://",nScheme,zScheme);` |
|       5 |  7943 | `	ph7_result_bool(pCtx,0);` |
|       5 |  7944 | `	return PH7_OK;` |
|      18 |  7945 | `}` |
|       - |  7946 | `/*` |
|       - |  7947 | ` * bool stream_wrapper_restore(string $protocol)` |
|       - |  7948 | ` *  Put a BUILT-IN protocol back, whether it was unregistered or replaced. The` |
|       - |  7949 | ` *  other half of the override pair, and useless without it -- which is why the` |
|       - |  7950 | ` *  two ship together.` |
|       - |  7951 | ` *` |
|       - |  7952 | ` *  php's three answers: a protocol that was never built in is a warning and` |
|       - |  7953 | ` *  FALSE; one that is built in and was never touched is an E_NOTICE and TRUE` |
|       - |  7954 | ` *  (it is already what it should be); anything else is restored and TRUE.` |
|       - |  7955 | ` */` |
|      12 |  7956 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  7957 | `{` |
|      13 |  7958 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  7959 | `	const ph7_io_stream **apOff;` |
|       - |  7960 | `	ph7_io_stream **apDev;` |
|       - |  7961 | `	const char *zScheme;` |
|      13 |  7962 | `	int nScheme,bBuiltin = 0,bChanged = 0,i;` |
|       - |  7963 | `	sxu32 n,nKeep;` |
|      13 |  7964 | `	if( nArg < 1 ){` |
|     ! 0 |  7965 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  7966 | `		return PH7_OK;` |
|       - |  7967 | `	}` |
|      13 |  7968 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      13 |  7969 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     145 |  7970 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     133 |  7971 | `		ph7_io_stream *pDev = apDev[n];` |
|     132 |  7972 | `		if( (int)SyStrlen(pDev->zName) != nScheme` |
|      93 |  7973 | `		 \|\| SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) != 0 ){` |
|     119 |  7974 | `			continue;` |
|       - |  7975 | `		}` |
|      15 |  7976 | `		if( UwrapIsSlotDevice(pDev) ){` |
|       - |  7977 | `			/* A userland wrapper standing in its place -- or one already` |
|       - |  7978 | `			 * withdrawn, which is still not a built-in. */` |
|       9 |  7979 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|       5 |  7980 | `				bChanged = 1;` |
|       2 |  7981 | `			}` |
|       9 |  7982 | `			continue;` |
|       - |  7983 | `		}` |
|       7 |  7984 | `		bBuiltin = 1;` |
|       7 |  7985 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|       5 |  7986 | `			bChanged = 1;` |
|       2 |  7987 | `		}` |
|       4 |  7988 | `	}` |
|      13 |  7989 | `	if( !bBuiltin ){` |
|      10 |  7990 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 |  7991 | `			"%.*s:// never existed, nothing to restore",nScheme,zScheme);` |
|       7 |  7992 | `		ph7_result_bool(pCtx,0);` |
|       7 |  7993 | `		return PH7_OK;` |
|       - |  7994 | `	}` |
|       7 |  7995 | `	if( !bChanged ){` |
|       - |  7996 | `		/* php answers TRUE here and says so at NOTICE level: the protocol is` |
|       - |  7997 | `		 * already the one it would restore. */` |
|       4 |  7998 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|       1 |  7999 | `			"%.*s:// was never changed, nothing to restore",nScheme,zScheme);` |
|       3 |  8000 | `		ph7_result_bool(pCtx,1);` |
|       3 |  8001 | `		return PH7_OK;` |
|       - |  8002 | `	}` |
|       - |  8003 | `	/* Lift the suppression off the BUILT-IN first, by compacting the set... */` |
|       5 |  8004 | `	apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|       5 |  8005 | `	nKeep = 0;` |
|       9 |  8006 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|       5 |  8007 | `		const ph7_io_stream *pDev = apOff[n];` |
|       4 |  8008 | `		if( (int)SyStrlen(pDev->zName) == nScheme` |
|       4 |  8009 | `		 && SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) == 0` |
|       5 |  8010 | `		 && !UwrapIsSlotDevice(pDev) ){` |
|       5 |  8011 | `			continue; /* the built-in comes back */` |
|       - |  8012 | `		}` |
|     ! 0 |  8013 | `		apOff[nKeep++] = pDev;` |
|     ! 0 |  8014 | `	}` |
|       5 |  8015 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|       - |  8016 | `	/* ...then retire every userland wrapper standing in for the name. */` |
|      37 |  8017 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      32 |  8018 | `		if( g_aUwrap[i].pVm == pVm` |
|      18 |  8019 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|       5 |  8020 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|       5 |  8021 | `			const ph7_io_stream *pDead = &g_aUwrap[i].sStream;` |
|       5 |  8022 | `			g_aUwrap[i].pVm = 0;` |
|       5 |  8023 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDead) ){` |
|       5 |  8024 | `				SySetPut(&pVm->aSuppressedIo,(const void *)&pDead);` |
|       2 |  8025 | `			}` |
|       2 |  8026 | `		}` |
|      17 |  8027 | `	}` |
|       5 |  8028 | `	ph7_result_bool(pCtx,1);` |
|       5 |  8029 | `	return PH7_OK;` |
|       7 |  8030 | `}` |
|       - |  8031 | `#ifdef PH7_ENABLE_NET` |
|       - |  8032 | `/*` |
|       - |  8033 | `` * php's socket address: `[transport://]host:port`. What a re-derivation gets`` |
|       - |  8034 | ` * wrong here is that BOTH halves have a diagnostic of their own, and neither is` |
|       - |  8035 | ` * the other: a transport this build does not carry is not a malformed address,` |
|       - |  8036 | ` * and an address with no port is not an unknown transport.` |
|       - |  8037 | ` */` |
|       - |  8038 | `#define SOCK_ADDR_OK        0` |
|       - |  8039 | `#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */` |
|       - |  8040 | `#define SOCK_ADDR_PARSE     2 /* no port separator at all */` |
|       - |  8041 | ``#define SOCK_ADDR_IPV6      3 /* opened `[` and did not close it with `]:` */`` |
|       - |  8042 | `/*` |
|       - |  8043 | `` * php's port half is `atoi()` of whatever follows the FIRST colon, and the`` |
|       - |  8044 | ` * colon is looked for in every position but the LAST — which is the whole` |
|       - |  8045 | `` * difference between `127.0.0.1:` (php's "Failed to parse address") and`` |
|       - |  8046 | `` * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that`` |
|       - |  8047 | ` * reads digits strictly refuses three addresses php accepts, and one that takes` |
|       - |  8048 | `` * the last colon reads `a:b:c` differently than php does.`` |
|       - |  8049 | ` */` |
|     475 |  8050 | `static int SockParsePort(const char *z,int n)` |
|       4 |  8051 | `{` |
|     479 |  8052 | `	int i = 0,iSign = 1,iVal = 0;` |
|     717 |  8053 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r'` |
|     473 |  8054 | `	              \|\| z[i] == '\v' \|\| z[i] == '\f') ){` |
|     ! 0 |  8055 | `		i++;` |
|     ! 0 |  8056 | `	}` |
|     479 |  8057 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|     ! 0 |  8058 | `		iSign = z[i] == '-' ? -1 : 1;` |
|     ! 0 |  8059 | `		i++;` |
|     ! 0 |  8060 | `	}` |
|    2476 |  8061 | `	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|    2001 |  8062 | `		if( iVal < 1000000000 ){` |
|    2001 |  8063 | `			iVal = iVal * 10 + (z[i] - '0');` |
|    1006 |  8064 | `		}` |
|    1010 |  8065 | `	}` |
|     479 |  8066 | `	return iSign * iVal;` |
|       4 |  8067 | `}` |
|     487 |  8068 | `static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,` |
|       - |  8069 | `	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest,int *pbDgram,` |
|       - |  8070 | `	int *piCrypto)` |
|       4 |  8071 | `{` |
|     491 |  8072 | `	const char *zRest = zAddr;` |
|     491 |  8073 | `	int nRest = nAddr,i,nHost = -1;` |
|     491 |  8074 | `	*pPort = 0;` |
|     491 |  8075 | `	*pzTransport = "tcp";` |
|     491 |  8076 | `	*pnTransport = 3;` |
|     491 |  8077 | `	*pbDgram = 0;` |
|     491 |  8078 | `	*piCrypto = 0;` |
|    3852 |  8079 | `	for( i = 0 ; i + 2 < nAddr ; i++ ){` |
|    3655 |  8080 | `		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){` |
|     294 |  8081 | `			*pzTransport = zAddr;` |
|     294 |  8082 | `			*pnTransport = i;` |
|     294 |  8083 | `			zRest = &zAddr[i+3];` |
|     294 |  8084 | `			nRest = nAddr - i - 3;` |
|     294 |  8085 | `			break;` |
|       - |  8086 | `		}` |
|    1704 |  8087 | `	}` |
|     491 |  8088 | `	*pzRest = zRest;` |
|     491 |  8089 | `	*pnRest = nRest;` |
|       - |  8090 | `	/* php looks the transport up in a hash keyed by the name as WRITTEN, so the` |
|       - |  8091 | ``	 * lookup is case-SENSITIVE -- `TCP://127.0.0.1:80` is a transport php has`` |
|       - |  8092 | `	 * not got, where a wrapper SCHEME (file://, PHP://) is folded first. This` |
|       - |  8093 | `	 * used to fold here too, so PHL connected through four spellings php` |
|       - |  8094 | `	 * refuses. */` |
|     491 |  8095 | `	if( *pnTransport == 3 && SyStrncmp(*pzTransport,"udp",3) == 0 ){` |
|      25 |  8096 | `		*pbDgram = 1;` |
|     480 |  8097 | `	}else if( *pnTransport != 3 \|\| SyStrncmp(*pzTransport,"tcp",3) != 0 ){` |
|       - |  8098 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - |  8099 | `		/* A crypto transport is a STREAM one that negotiates as part of the` |
|       - |  8100 | `		 * connect, so everything below -- the host:port split, the bind` |
|       - |  8101 | `		 * options, the persistence key -- is the tcp:// path unchanged, and` |
|       - |  8102 | `		 * only the handshake is added on top of the connected socket. */` |
|      91 |  8103 | `		*piCrypto = SockCryptoTransport(*pzTransport,*pnTransport);` |
|      91 |  8104 | `		if( *piCrypto == 0 ){` |
|      12 |  8105 | `			return SOCK_ADDR_TRANSPORT;` |
|       - |  8106 | `		}` |
|       - |  8107 | `#else` |
|       - |  8108 | `		return SOCK_ADDR_TRANSPORT;` |
|       - |  8109 | `#endif` |
|      39 |  8110 | `	}` |
|     481 |  8111 | `	if( nRest > 1 && zRest[0] == '[' ){` |
|       - |  8112 | `` 		/* php reads the BRACKETED form before it looks for a port at all: a `]` `` |
|       - |  8113 | ``		 * anywhere but the last byte, with a `:` immediately after it, and the`` |
|       - |  8114 | `		 * host is what the brackets hold. Anything else is a refusal of its own` |
|       - |  8115 | `		 * wording -- not the "Failed to parse address" a missing port gets --` |
|       - |  8116 | ``		 * and `[]:9` is an EMPTY host, which the resolver is what refuses.`` |
|       - |  8117 | `		 * (What this build does with the address it parses is the scope policy's IPv4-only` |
|       - |  8118 | ``		 * cut: `::1` reaches the resolver and is refused there.) */`` |
|      69 |  8119 | `		for( i = 1 ; i + 1 < nRest ; i++ ){` |
|      63 |  8120 | `			if( zRest[i] == ']' ){` |
|      13 |  8121 | `				break;` |
|       - |  8122 | `			}` |
|      26 |  8123 | `		}` |
|      19 |  8124 | `		if( i + 1 >= nRest \|\| zRest[i] != ']' \|\| zRest[i+1] != ':' ){` |
|       9 |  8125 | `			return SOCK_ADDR_IPV6;` |
|       - |  8126 | `		}` |
|      11 |  8127 | `		*pPort = SockParsePort(&zRest[i+2],nRest - i - 2);` |
|      11 |  8128 | `		nHost = i - 1;` |
|      11 |  8129 | `		if( nHost >= nHostBuf ){` |
|     ! 0 |  8130 | `			nHost = nHostBuf - 1;` |
|     ! 0 |  8131 | `		}` |
|      11 |  8132 | `		if( nHost > 0 ){` |
|      11 |  8133 | `			SyMemcpy(&zRest[1],zHost,(sxu32)nHost);` |
|       5 |  8134 | `		}` |
|      11 |  8135 | `		zHost[nHost] = 0;` |
|      11 |  8136 | `		return SOCK_ADDR_OK;` |
|       - |  8137 | `	}` |
|    4525 |  8138 | `	for( i = 0 ; i + 1 < nRest ; i++ ){` |
|    4513 |  8139 | `		if( zRest[i] == ':' ){` |
|     450 |  8140 | `			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);` |
|     450 |  8141 | `			nHost = i;` |
|     450 |  8142 | `			break;` |
|       - |  8143 | `		}` |
|    2048 |  8144 | `	}` |
|     462 |  8145 | `	if( nHost < 0 ){` |
|      15 |  8146 | `		return SOCK_ADDR_PARSE;` |
|       - |  8147 | `	}` |
|     450 |  8148 | `	if( nHost >= nHostBuf ){` |
|     ! 0 |  8149 | `		nHost = nHostBuf - 1;` |
|     ! 0 |  8150 | `	}` |
|     450 |  8151 | `	if( nHost > 0 ){` |
|     444 |  8152 | `		SyMemcpy(zRest,zHost,(sxu32)nHost);` |
|     222 |  8153 | `	}` |
|     450 |  8154 | `	zHost[nHost] = 0;` |
|     450 |  8155 | `	return SOCK_ADDR_OK;` |
|     249 |  8156 | `}` |
|       - |  8157 | `/*` |
|       - |  8158 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|       - |  8159 | ` *                          string &$error_message, ?float $timeout = null)` |
|       - |  8160 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|       - |  8161 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|       - |  8162 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|       - |  8163 | ` */` |
|     343 |  8164 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  8165 | `{` |
|     347 |  8166 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     347 |  8167 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|     347 |  8168 | `	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";` |
|       - |  8169 | `	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];` |
|       - |  8170 | `	const char *zShow;` |
|     347 |  8171 | `	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;` |
|     347 |  8172 | `	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect,bDgram = 0,iCrypto = 0;` |
|       - |  8173 | `	ph7_socket sock;` |
|       - |  8174 | `	io_private *pDev;` |
|     347 |  8175 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|     347 |  8176 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|     347 |  8177 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|     347 |  8178 | `	phl_stream_ctx *pCtxRes = 0;` |
|       - |  8179 | `	ph7_sockopts sOpt;` |
|       - |  8180 | `	char zBindHost[256];` |
|     347 |  8181 | `	int bThrew = 0;` |
|     347 |  8182 | `	if( nArg < 1 ){` |
|     ! 0 |  8183 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8184 | `		return PH7_OK;` |
|       - |  8185 | `	}` |
|     347 |  8186 | `	if( bClientForm ){` |
|       - |  8187 | ``		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such`` |
|       - |  8188 | `		 * argument, so only the stream_socket_client() spelling takes one. */` |
|     160 |  8189 | `		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);` |
|     160 |  8190 | `		if( bThrew ){` |
|     ! 0 |  8191 | `			return PH7_OK;` |
|       - |  8192 | `		}` |
|      78 |  8193 | `	}` |
|     347 |  8194 | `	zRaw = ph7_value_to_string(apArg[0],&nRaw);` |
|     347 |  8195 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     188 |  8196 | `		iPortArg = ph7_value_to_int(apArg[1]);` |
|      95 |  8197 | `	}` |
|     347 |  8198 | `	if( bClientForm && nArg > 4 ){` |
|       - |  8199 | `		/* Declared in the signature and read by nothing until now, so the` |
|       - |  8200 | `		 * documented spellings did nothing and their constants were undefined` |
|       - |  8201 | `		 * fatals. */` |
|      97 |  8202 | `		iFlags = (int)ph7_value_to_int64(apArg[4]);` |
|      47 |  8203 | `	}` |
|       - |  8204 | `	/* pfsockopen() IS fsockopen() with this flag; php has no other difference` |
|       - |  8205 | `	 * between them. */` |
|     425 |  8206 | `	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0` |
|     265 |  8207 | `		: (zFunc[0] == 'p');` |
|       - |  8208 | `	/* php passes STREAM_XPORT_CONNECT and STREAM_XPORT_CONNECT_ASYNC as two` |
|       - |  8209 | `	 * separate bits and its transport connects for EITHER, so` |
|       - |  8210 | ``	 * `stream_socket_client($a, $e, $es, null, STREAM_CLIENT_ASYNC_CONNECT)` --`` |
|       - |  8211 | `	 * the documented spelling for an asynchronous dial -- is a connected socket` |
|       - |  8212 | `	 * in php and was a socket-less handle here, writing 0 bytes and naming no` |
|       - |  8213 | `	 * peer. */` |
|     347 |  8214 | `	bConnect = bClientForm` |
|     248 |  8215 | `		? (iFlags & (PH7_STREAM_CLIENT_CONNECT\|PH7_STREAM_CLIENT_ASYNC_CONNECT)) != 0 : 1;` |
|       - |  8216 | `	/* php builds ONE address out of fsockopen()'s two arguments — and only when` |
|       - |  8217 | ``	 * the port is a usable one, which is why `fsockopen($h)` reports the address`` |
|       - |  8218 | `	 * it could not parse rather than connecting to port 0. The address it SHOWS` |
|       - |  8219 | `	 * keeps the port either way. */` |
|     347 |  8220 | `	if( bClientForm \|\| iPortArg <= 0 ){` |
|     160 |  8221 | `		zAddr = zRaw;` |
|     160 |  8222 | `		nAddr = nRaw;` |
|      82 |  8223 | `	}else{` |
|     188 |  8224 | `		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     188 |  8225 | `		zAddr = zAddrBuf;` |
|       - |  8226 | `	}` |
|     347 |  8227 | `	if( bClientForm ){` |
|     160 |  8228 | `		zShow = zRaw;` |
|     160 |  8229 | `		nShow = nRaw;` |
|      82 |  8230 | `	}else{` |
|     188 |  8231 | `		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     188 |  8232 | `		zShow = zShowBuf;` |
|       - |  8233 | `	}` |
|     347 |  8234 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|       - |  8235 | `		&zRest,&nRest,&bDgram,&iCrypto);` |
|     347 |  8236 | `	if( (rc == SOCK_ADDR_PARSE \|\| rc == SOCK_ADDR_IPV6) && !bConnect ){` |
|       - |  8237 | `		/* php splits the address in TWO places: the transport is looked up when` |
|       - |  8238 | `		 * the stream is created and the host:port half is parsed by the` |
|       - |  8239 | `		 * connect() -- so a $flags without STREAM_CLIENT_CONNECT never looks at` |
|       - |  8240 | ``		 * the address at all, and `stream_socket_client('0.0.0.0', $e, $es,`` |
|       - |  8241 | ``		 * null, 0)` is a resource in php where PHL reported an address it could`` |
|       - |  8242 | `		 * not parse. A transport nothing is registered under still fails. */` |
|       3 |  8243 | `		rc = SOCK_ADDR_OK;` |
|       1 |  8244 | `	}` |
|     347 |  8245 | `	if( rc != SOCK_ADDR_OK ){` |
|      18 |  8246 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|       - |  8247 | `			/* php's own wording for a transport its build does not carry —` |
|       - |  8248 | `			 * which is what this engine's missing ones ARE (recorded), and what a` |
|       - |  8249 | `			 * script reading $errstr is written against. This used to spell a` |
|       - |  8250 | `			 * message of PHL's own that no php ever answers. */` |
|      10 |  8251 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  8252 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|       3 |  8253 | `				nTransport,zTransport);` |
|      15 |  8254 | `		}else if( rc == SOCK_ADDR_IPV6 ){` |
|       9 |  8255 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse IPv6 address \"%.*s\"",nRest,zRest);` |
|       5 |  8256 | `		}else{` |
|       3 |  8257 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|       - |  8258 | `		}` |
|      18 |  8259 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);` |
|      18 |  8260 | `		ph7_result_bool(pCtx,0);` |
|      18 |  8261 | `		return PH7_OK;` |
|       - |  8262 | `	}` |
|     331 |  8263 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|     282 |  8264 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|     282 |  8265 | `		if( rTimeout > 0 ){` |
|     282 |  8266 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     141 |  8267 | `		}` |
|     141 |  8268 | `	}` |
|     331 |  8269 | `	if( bPersist ){` |
|       - |  8270 | `		/* A live one for this address IS the answer: php hands the same resource` |
|       - |  8271 | `		 * back rather than opening a second connection to the same peer. */` |
|       - |  8272 | `		char zKey[320];` |
|       - |  8273 | `		io_private *pKept;` |
|      24 |  8274 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|      24 |  8275 | `		pKept = SockPersistFind(pCtx->pVm,zKey);` |
|      24 |  8276 | `		if( pKept ){` |
|       - |  8277 | `			/* php does not hand a kept connection back unseen: it runs the same` |
|       - |  8278 | `			 * liveness probe feof() uses, with a zero timeout, and a socket the` |
|       - |  8279 | `			 * far end has finished with is CLOSED and dialled again. Without` |
|       - |  8280 | `			 * this a persistent handle stays broken for the rest of the` |
|       - |  8281 | `			 * request -- every later call gets the same dead socket, and the` |
|       - |  8282 | `			 * script's writes fail on a connection php would have replaced. */` |
|      12 |  8283 | `			ph7_socket *pKeptSock = IoPrivateSocket(pKept);` |
|      12 |  8284 | `			if( pKeptSock == 0 \|\| PH7_NetIsAlive(*pKeptSock) ){` |
|      10 |  8285 | `				SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      10 |  8286 | `				ph7_result_resource(pCtx,pKept);` |
|      10 |  8287 | `				return PH7_OK;` |
|       - |  8288 | `			}` |
|       3 |  8289 | `			PH7_StreamCloseHandle(pKept->pStream,pKept->pHandle);` |
|       3 |  8290 | `			MarkIOPrivateClosed(pKept);` |
|       3 |  8291 | `			SockPersistDrop(pCtx->pVm,zKey);` |
|       1 |  8292 | `		}` |
|       7 |  8293 | `	}` |
|     323 |  8294 | `	if( !bConnect ){` |
|       - |  8295 | `		/* php creates the socket while CONNECTING it, so a $flags without` |
|       - |  8296 | `		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no` |
|       - |  8297 | `		 * name at either end, reads false, writes 0, already at end of file. */` |
|       6 |  8298 | `		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|       6 |  8299 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,bDgram,zAddr,nAddr);` |
|       6 |  8300 | `		if( pDev == 0 ){` |
|     ! 0 |  8301 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  8302 | `			return PH7_OK;` |
|       - |  8303 | `		}` |
|       6 |  8304 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|       6 |  8305 | `		StreamCtxHold(pCtxRes);` |
|       6 |  8306 | `		ph7_result_resource(pCtx,pDev);` |
|       6 |  8307 | `		return PH7_OK;` |
|       - |  8308 | `	}` |
|       - |  8309 | `	{` |
|       - |  8310 | ``		/* php reads the `socket` options at the moment it creates the socket:`` |
|       - |  8311 | `		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and` |
|       - |  8312 | `		 * bindto is the LOCAL address it takes before connecting. */` |
|     319 |  8313 | `		const char *zOptErr = 0;` |
|     319 |  8314 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|       - |  8315 | `			/* The one option failure php treats as a failed CONNECT rather than` |
|       - |  8316 | `			 * as a warning it can carry on past. */` |
|       3 |  8317 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);` |
|       3 |  8318 | `			ph7_result_bool(pCtx,0);` |
|       3 |  8319 | `			return PH7_OK;` |
|       - |  8320 | `		}` |
|       - |  8321 | `	}` |
|       - |  8322 | `	/* ASYNC_CONNECT is the one flag that changes the CALL rather than the` |
|       - |  8323 | `	 * socket: php issues a non-blocking connect and answers a resource for a` |
|       - |  8324 | `	 * dial that has not finished (or has already been refused). */` |
|     447 |  8325 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,bDgram,` |
|     223 |  8326 | `		bClientForm && (iFlags & PH7_STREAM_CLIENT_ASYNC_CONNECT) != 0,` |
|       - |  8327 | `		&sOpt,&iErrno,&zErr);` |
|     317 |  8328 | `	if( sOpt.iBindErr ){` |
|       - |  8329 | `		/* php's own wording, and NEITHER shape stops the connection: the socket` |
|       - |  8330 | `		 * goes out from wherever the routing table would have sent it. It tells` |
|       - |  8331 | `		 * the two apart — a local address that is not a numeric literal at all` |
|       - |  8332 | `		 * names the host, one the OS refused to BIND names the address it tried` |
|       - |  8333 | `		 * and the reason. */` |
|       9 |  8334 | `		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){` |
|       5 |  8335 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",` |
|       4 |  8336 | `				sOpt.zBindHost ? sOpt.zBindHost : "");` |
|       3 |  8337 | `		}else{` |
|       - |  8338 | `			/* php RE-COMPOSES the address it tried from the parts it parsed, so` |
|       - |  8339 | ``			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"`` |
|       - |  8340 | `			 * is reported as '192.0.2.1:7'. */` |
|       5 |  8341 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - |  8342 | `				"Failed to bind to '%s:%d', system said: %s",` |
|       4 |  8343 | `				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,` |
|       2 |  8344 | `				PH7_NetStrError(sOpt.iBindErrno));` |
|       - |  8345 | `		}` |
|       4 |  8346 | `	}` |
|     317 |  8347 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     122 |  8348 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|       3 |  8349 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|       3 |  8350 | `			iErrno = 0;` |
|       1 |  8351 | `		}` |
|     122 |  8352 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);` |
|     122 |  8353 | `		ph7_result_bool(pCtx,0);` |
|     122 |  8354 | `		return PH7_OK;` |
|       - |  8355 | `	}` |
|     197 |  8356 | `	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|       - |  8357 | `	/* Wrap the socket in an io_private so the whole f* family works on it. php` |
|       - |  8358 | ``	 * reports the ADDRESS it opened as the handle's `uri`, which is the same`` |
|       - |  8359 | `	 * one-address-out-of-two-arguments composition it connected through — so an` |
|       - |  8360 | `	 * argument naming only a host still records the port beside it. */` |
|     197 |  8361 | `	pDev = SockWrapSocket(pCtx,sock,bDgram,zAddr,nAddr);` |
|     197 |  8362 | `	if( pDev == 0 ){` |
|     ! 0 |  8363 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8364 | `		return PH7_OK;` |
|       - |  8365 | `	}` |
|       - |  8366 | `	/* php attaches the opener's context to a TRANSPORT stream and to nothing` |
|       - |  8367 | `	 * else — which is why stream_context_get_options() answers for a socket and` |
|       - |  8368 | `	 * answers the empty set for a file opened through the very same call. */` |
|       - |  8369 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     197 |  8370 | `	if( iCrypto != 0 ){` |
|       - |  8371 | `		/* php negotiates inside the connect for a crypto transport, so a` |
|       - |  8372 | `		 * handshake that fails is a failed CONNECT: no handle at all rather` |
|       - |  8373 | `		 * than a plaintext one. It reports the failure THREE times and` |
|       - |  8374 | `		 * asymmetrically -- the crypto layer raises what went wrong, the connect` |
|       - |  8375 | `		 * that asked for crypto says it could not get it, and the opener then` |
|       - |  8376 | ``		 * says `Unable to connect to ... (Unknown error)` and leaves $errstr`` |
|       - |  8377 | `		 * EMPTY, because the text belonged to the layer below and php never` |
|       - |  8378 | `		 * carried it up. The resolve failure above has the same shape` |
|       - |  8379 | `		 * with the text carried. Run before the context is attached, so the` |
|       - |  8380 | `		 * abandoned handle owes it no reference. */` |
|       - |  8381 | `		char zSslErr[512];` |
|      84 |  8382 | `		if( SockSslHandshake(pCtx,(sock_private *)pDev->pHandle,iCrypto,pCtxRes,zHost,` |
|      56 |  8383 | `				zSslErr,(int)sizeof(zSslErr)) != PH7_OK ){` |
|      34 |  8384 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zSslErr);` |
|      34 |  8385 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Failed to enable crypto");` |
|      34 |  8386 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,0,0);` |
|      34 |  8387 | `			SockCloseWrapped(pCtx,pDev);` |
|      34 |  8388 | `			ph7_result_bool(pCtx,0);` |
|      34 |  8389 | `			return PH7_OK;` |
|       - |  8390 | `		}` |
|      11 |  8391 | `	}` |
|       - |  8392 | `#endif` |
|     163 |  8393 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     163 |  8394 | `	StreamCtxHold(pCtxRes);` |
|     163 |  8395 | `	SockArmDefaultTimeout(pCtx,pDev);` |
|     163 |  8396 | `	if( bPersist ){` |
|       - |  8397 | `		char zKey[320];` |
|      16 |  8398 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|      16 |  8399 | `		SockPersistKeep(pCtx->pVm,zKey,pDev);` |
|       - |  8400 | `		/* get_resource_type() names it apart, which is how a script can tell it` |
|       - |  8401 | `		 * asked for one at all. */` |
|      16 |  8402 | `		pDev->bPersist = 1;` |
|       7 |  8403 | `	}` |
|     163 |  8404 | `	ph7_result_resource(pCtx,pDev);` |
|     163 |  8405 | `	return PH7_OK;` |
|     177 |  8406 | `}` |
|       - |  8407 | `/*` |
|       - |  8408 | ` * resource\|false stream_socket_server(string $address, int &$error_code,` |
|       - |  8409 | ` *                    string &$error_message, int $flags = STREAM_SERVER_BIND\|STREAM_SERVER_LISTEN,` |
|       - |  8410 | ` *                    ?resource $context = null)` |
|       - |  8411 | ` *` |
|       - |  8412 | ` * The name a php program becomes a SERVER through, and a loud` |
|       - |  8413 | `` * `Call to undefined function` until now — so a script that listens on a port`` |
|       - |  8414 | ` * (a test double, a job runner, a line protocol) could not be spelled at all,` |
|       - |  8415 | ` * even though net.c had bind() and listen() all along.` |
|       - |  8416 | ` *` |
|       - |  8417 | ` * php's two flags are separate for a reason: BIND alone is what a datagram` |
|       - |  8418 | ` * socket wants (there is nothing to listen for), so LISTEN is what makes the` |
|       - |  8419 | ` * socket a stream server. Dropping LISTEN from a tcp:// address is therefore` |
|       - |  8420 | ` * a bound socket nothing can connect to, which is exactly what php answers.` |
|       - |  8421 | ` */` |
|     142 |  8422 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  8423 | `{` |
|     146 |  8424 | `	const char *zAddr,*zTransport,*zRest,*zErr = "";` |
|       - |  8425 | `	char zHost[256];` |
|     146 |  8426 | `	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc,bDgram = 0,iCrypto = 0;` |
|       - |  8427 | `	ph7_socket sock;` |
|       - |  8428 | `	io_private *pDev;` |
|       - |  8429 | `	phl_stream_ctx *pCtxRes;` |
|       - |  8430 | `	ph7_sockopts sOpt;` |
|       - |  8431 | `	char zBindHost[256];` |
|     146 |  8432 | `	int bThrew = 0;` |
|     146 |  8433 | `	if( nArg < 1 ){` |
|     ! 0 |  8434 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8435 | `		return PH7_OK;` |
|       - |  8436 | `	}` |
|     146 |  8437 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);` |
|     146 |  8438 | `	if( bThrew ){` |
|     ! 0 |  8439 | `		return PH7_OK;` |
|       - |  8440 | `	}` |
|       - |  8441 | ``	/* The signature row declares `string $address`, so whatever arrives has`` |
|       - |  8442 | `	 * already been screened; php's own ZPP then CASTS it, and refusing an int` |
|       - |  8443 | `` 	 * here would answer false in silence for `stream_socket_server(8080)` `` |
|       - |  8444 | `	 * where php reports the address it could not parse. */` |
|     146 |  8445 | `	zAddr = ph7_value_to_string(apArg[0],&nAddr);` |
|      97 |  8446 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])` |
|      93 |  8447 | `		: (PH7_STREAM_SERVER_BIND\|PH7_STREAM_SERVER_LISTEN);` |
|     146 |  8448 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|       - |  8449 | `		&zRest,&nRest,&bDgram,&iCrypto);` |
|       - |  8450 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     146 |  8451 | `	if( iCrypto != 0 ){` |
|       - |  8452 | `		/* The listener itself is an ordinary bound TCP socket — php negotiates` |
|       - |  8453 | `		 * per ACCEPTED connection, not here — so the transport only decides` |
|       - |  8454 | `		 * which method each accept will hand the handshake. Dropping the client` |
|       - |  8455 | `		 * bit is what turns the address's mask into the server one: php numbers` |
|       - |  8456 | `		 * the two sides of a protocol as the same bits with and without it. */` |
|      16 |  8457 | `		iCrypto &= ~SOCK_CRYPTO_CLIENT;` |
|       8 |  8458 | `	}` |
|       - |  8459 | `#else` |
|       - |  8460 | `	if( iCrypto != 0 ){` |
|       - |  8461 | `		/* No libssl under this build, so the address names a transport that is` |
|       - |  8462 | `		 * genuinely not here. */` |
|       - |  8463 | `		rc = SOCK_ADDR_TRANSPORT;` |
|       - |  8464 | `	}` |
|       - |  8465 | `#endif` |
|     142 |  8466 | `	if( (rc == SOCK_ADDR_PARSE \|\| rc == SOCK_ADDR_IPV6)` |
|      79 |  8467 | `	 && (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|       - |  8468 | `		/* The server half of the same split: the bind() is what parses` |
|       - |  8469 | `		 * host:port, so a $flags without STREAM_SERVER_BIND answers a socket` |
|       - |  8470 | ``		 * for an address php never reads -- `stream_socket_server('0.0.0.0',`` |
|       - |  8471 | ``		 * $e, $es, STREAM_SERVER_LISTEN)` included, LISTEN being unreachable`` |
|       - |  8472 | `		 * without BIND. The transport is still resolved. */` |
|       3 |  8473 | `		rc = SOCK_ADDR_OK;` |
|       1 |  8474 | `	}` |
|     146 |  8475 | `	if( rc != SOCK_ADDR_OK ){` |
|       - |  8476 | `		char zMsg[512];` |
|      13 |  8477 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|       - |  8478 | `			/* php's own wording for a transport its build has not got, which is` |
|       - |  8479 | `			 * what unix:// and udg:// are here (recorded). */` |
|       8 |  8480 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  8481 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|       2 |  8482 | `				nTransport,zTransport);` |
|      10 |  8483 | `		}else if( rc == SOCK_ADDR_IPV6 ){` |
|     ! 0 |  8484 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse IPv6 address \"%.*s\"",nRest,zRest);` |
|     ! 0 |  8485 | `		}else{` |
|       8 |  8486 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|       - |  8487 | `		}` |
|      13 |  8488 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);` |
|      13 |  8489 | `		ph7_result_bool(pCtx,0);` |
|      13 |  8490 | `		return PH7_OK;` |
|       - |  8491 | `	}` |
|     136 |  8492 | `	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|       - |  8493 | `		/* php creates the socket while BINDING it, so a $flags without` |
|       - |  8494 | `		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:` |
|       - |  8495 | `		 * it has no name, reads false, writes 0 and is already at end of file.` |
|       - |  8496 | ``		 * It does not even resolve the host — `stream_socket_server(':1', $e,`` |
|       - |  8497 | ``		 * $es, 0)` is a resource in php. */`` |
|       8 |  8498 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,bDgram,zAddr,nAddr);` |
|       8 |  8499 | `		if( pDev == 0 ){` |
|     ! 0 |  8500 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  8501 | `			return PH7_OK;` |
|       - |  8502 | `		}` |
|       8 |  8503 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|       8 |  8504 | `		StreamCtxHold(pCtxRes);` |
|       8 |  8505 | `		SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|       8 |  8506 | `		ph7_result_resource(pCtx,pDev);` |
|       8 |  8507 | `		return PH7_OK;` |
|       - |  8508 | `	}` |
|     130 |  8509 | `	if( zHost[0] == 0 ){` |
|       - |  8510 | ``		/* An address with no host at all (`:8080`) is a name php asks the`` |
|       - |  8511 | `		 * resolver about and is refused for — NOT a wildcard bind. Answering` |
|       - |  8512 | `		 * 0.0.0.0 for it would put a listener on every interface of the` |
|       - |  8513 | `		 * machine, which is the unsafe direction. */` |
|       - |  8514 | `		char zMsg[512];` |
|       3 |  8515 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,` |
|       1 |  8516 | `			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);` |
|       2 |  8517 | `		ph7_result_bool(pCtx,0);` |
|       2 |  8518 | `		return PH7_OK;` |
|       - |  8519 | `	}` |
|       - |  8520 | `	{` |
|       - |  8521 | ``		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;`` |
|       - |  8522 | ``		 * `bindto` is not one of its options, because the address argument IS`` |
|       - |  8523 | `		 * where a server binds (php ignores it here too). */` |
|     128 |  8524 | `		const char *zOptErr = 0;` |
|     128 |  8525 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|     ! 0 |  8526 | `			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);` |
|     ! 0 |  8527 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 |  8528 | `			return PH7_OK;` |
|       - |  8529 | `		}` |
|     128 |  8530 | `		sOpt.zBindHost = 0;` |
|       - |  8531 | `	}` |
|     128 |  8532 | `	if( bDgram && (iFlags & PH7_STREAM_SERVER_LISTEN) != 0 ){` |
|       - |  8533 | `		/* php's udp ops answer STREAM_XPORT_OP_LISTEN with a flat -1 -- they do` |
|       - |  8534 | `		 * not call listen() and they log NOTHING -- so the DEFAULT $flags,` |
|       - |  8535 | `		 * BIND\|LISTEN, fails on a datagram address with no reason of its own to` |
|       - |  8536 | `		 * report. That is what makes STREAM_SERVER_BIND the spelling a udp` |
|       - |  8537 | `		 * server is written with, and the failure keeps php's shape: the socket` |
|       - |  8538 | `		 * is created and bound first, then thrown away, so an address that` |
|       - |  8539 | `		 * cannot be bound at all reports THAT instead. */` |
|       3 |  8540 | `		sock = PH7_NetBind(zHost,iPort,1,0,SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|       3 |  8541 | `		if( sock != PH7_NET_INVALID_SOCKET ){` |
|       3 |  8542 | `			PH7_NetClose(sock);` |
|       3 |  8543 | `			sock = PH7_NET_INVALID_SOCKET;` |
|       3 |  8544 | `			iErrno = 0;` |
|       - |  8545 | ``			/* No text at all: php's `errstr` stays the empty string it was`` |
|       - |  8546 | `			 * pre-assigned and only the warning fills the gap, with the words` |
|       - |  8547 | ``			 * `Unknown error`. */`` |
|       3 |  8548 | `			zErr = 0;` |
|       1 |  8549 | `		}` |
|       2 |  8550 | `	}else{` |
|     126 |  8551 | `		sock = PH7_NetBind(zHost,iPort,bDgram,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,` |
|       - |  8552 | `			SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|       - |  8553 | `	}` |
|     128 |  8554 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|       - |  8555 | `		char zMsg[512];` |
|       5 |  8556 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|     ! 0 |  8557 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|     ! 0 |  8558 | `		}` |
|       - |  8559 | `		/* php reports no OS code for a refused ADDRESS — only a connect() that` |
|       - |  8560 | `		 * reached the network carries one — so this stays 0 for every arm. */` |
|       5 |  8561 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);` |
|       5 |  8562 | `		ph7_result_bool(pCtx,0);` |
|       5 |  8563 | `		return PH7_OK;` |
|       - |  8564 | `	}` |
|     124 |  8565 | `	SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|     124 |  8566 | `	pDev = SockWrapSocket(pCtx,sock,bDgram,zAddr,nAddr);` |
|     124 |  8567 | `	if( pDev == 0 ){` |
|     ! 0 |  8568 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8569 | `		return PH7_OK;` |
|       - |  8570 | `	}` |
|       - |  8571 | `#if defined(PH7_ENABLE_OPENSSL)` |
|     124 |  8572 | `	((sock_private *)pDev->pHandle)->iCryptoAccept = iCrypto;` |
|       - |  8573 | `#endif` |
|     124 |  8574 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     124 |  8575 | `	StreamCtxHold(pCtxRes);` |
|     124 |  8576 | `	ph7_result_resource(pCtx,pDev);` |
|     124 |  8577 | `	return PH7_OK;` |
|      75 |  8578 | `}` |
|       - |  8579 | `/*` |
|       - |  8580 | ` * resource\|false stream_socket_accept(resource $socket, ?float $timeout = null,` |
|       - |  8581 | ` *                                    string &$peer_name = null)` |
|       - |  8582 | ` *` |
|       - |  8583 | ` * The other half of a server, and the one with the timing in it. php waits at` |
|       - |  8584 | `` * most `default_socket_timeout` seconds by default — NOT forever — and reports`` |
|       - |  8585 | ` * an expired wait as a warning plus false, which is what lets a single-threaded` |
|       - |  8586 | ` * server do something else between connections. A negative timeout blocks.` |
|       - |  8587 | ` */` |
|      50 |  8588 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  8589 | `{` |
|       - |  8590 | `	io_private *pDev,*pOut;` |
|       - |  8591 | `	ph7_socket *pSock,sock;` |
|       - |  8592 | `	char zPeer[128];` |
|      54 |  8593 | `	int rc,bTimedOut = 0,iTimeoutMs;` |
|      54 |  8594 | `	if( nArg < 1 ){` |
|     ! 0 |  8595 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8596 | `		return PH7_OK;` |
|       - |  8597 | `	}` |
|      54 |  8598 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      54 |  8599 | `	if( pDev == 0 ){` |
|       3 |  8600 | `		return rc;` |
|       - |  8601 | `	}` |
|      52 |  8602 | `	pSock = IoPrivateSocket(pDev);` |
|      52 |  8603 | `	if( pSock == 0 ){` |
|       - |  8604 | `		/* Not a socket at all. php's own answer for it reads oddly and is what` |
|       - |  8605 | `		 * a script sees: the accept never reaches the network, so there is no` |
|       - |  8606 | `		 * OS error to report and php asks its error table for code 0. */` |
|       3 |  8607 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");` |
|       3 |  8608 | `		ph7_result_bool(pCtx,0);` |
|       3 |  8609 | `		return PH7_OK;` |
|       - |  8610 | `	}` |
|      50 |  8611 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      50 |  8612 | `		double rTimeout = ph7_value_to_double(apArg[1]);` |
|      50 |  8613 | `		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);` |
|      27 |  8614 | `	}else{` |
|     ! 0 |  8615 | `		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);` |
|     ! 0 |  8616 | `		if( iTimeoutMs < 0 ){` |
|     ! 0 |  8617 | `			iTimeoutMs = -1;` |
|     ! 0 |  8618 | `		}` |
|       - |  8619 | `	}` |
|      50 |  8620 | `	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));` |
|      50 |  8621 | `	if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - |  8622 | `		/* php waits and then reports the expiry; there is nothing to wait on. */` |
|       3 |  8623 | `		bTimedOut = 1;` |
|       1 |  8624 | `	}` |
|      50 |  8625 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      11 |  8626 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",` |
|       6 |  8627 | `			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));` |
|       8 |  8628 | `		ph7_result_bool(pCtx,0);` |
|       8 |  8629 | `		return PH7_OK;` |
|       - |  8630 | `	}` |
|       - |  8631 | ``	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by`` |
|       - |  8632 | `	 * name, so stream_get_meta_data() has no address to answer with. */` |
|      44 |  8633 | `	pOut = SockWrapSocket(pCtx,sock,0,0,0);` |
|      44 |  8634 | `	if( pOut == 0 ){` |
|     ! 0 |  8635 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8636 | `		return PH7_OK;` |
|       - |  8637 | `	}` |
|       - |  8638 | `#if defined(PH7_ENABLE_OPENSSL)` |
|      44 |  8639 | `	if( ((sock_private *)pDev->pHandle)->iCryptoAccept != 0 ){` |
|       - |  8640 | `		/* A crypto listener negotiates HERE, once per connection, with the` |
|       - |  8641 | `		 * method its address named — which is why php's tls:// server binds` |
|       - |  8642 | ``		 * happily on a `local_cert` that does not exist and refuses the first`` |
|       - |  8643 | `		 * accept instead. A handshake that fails is a failed ACCEPT: php answers` |
|       - |  8644 | ``		 * false and leaves `$peer_name` alone, so the connection is dropped`` |
|       - |  8645 | `		 * rather than handed back speaking plaintext. Run before the context is` |
|       - |  8646 | `		 * attached below, so the abandoned handle owes it no reference. */` |
|       - |  8647 | `		char zSslErr[512];` |
|       6 |  8648 | `		if( SockSslHandshake(pCtx,(sock_private *)pOut->pHandle,` |
|       4 |  8649 | `				((sock_private *)pDev->pHandle)->iCryptoAccept,` |
|       6 |  8650 | `				(phl_stream_ctx *)pDev->pCtxRes,0,zSslErr,(int)sizeof(zSslErr)) != PH7_OK ){` |
|       - |  8651 | `			/* Three warnings, like php: the crypto layer says what went wrong,` |
|       - |  8652 | `			 * the crypto step reports that it could not be switched on, and the` |
|       - |  8653 | `			 * accept that asked then reports what it could not do. */` |
|       4 |  8654 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zSslErr);` |
|       4 |  8655 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Failed to enable crypto");` |
|       4 |  8656 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Cannot enable crypto");` |
|       4 |  8657 | `			SockCloseWrapped(pCtx,pOut);` |
|       4 |  8658 | `			ph7_result_bool(pCtx,0);` |
|       4 |  8659 | `			return PH7_OK;` |
|       - |  8660 | `		}` |
|     ! 0 |  8661 | `	}` |
|       - |  8662 | `#endif` |
|       - |  8663 | `	/* The listener's context is the accepted connection's context, for a plain` |
|       - |  8664 | `	 * tcp:// server as much as for a crypto one: php clones the ops AND the` |
|       - |  8665 | `	 * context onto the new handle, which is why stream_context_get_options()` |
|       - |  8666 | ``	 * answers the same set on both ends of an accept, and why the `ssl` options`` |
|       - |  8667 | `	 * a tls:// listener was created with are the ones the handshake above` |
|       - |  8668 | `	 * read. */` |
|      40 |  8669 | `	if( pDev->pCtxRes ){` |
|      40 |  8670 | `		pOut->pCtxRes = pDev->pCtxRes;` |
|      40 |  8671 | `		StreamCtxHold((phl_stream_ctx *)pOut->pCtxRes);` |
|      18 |  8672 | `	}` |
|      40 |  8673 | `	if( nArg > 2 ){` |
|       6 |  8674 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|       6 |  8675 | `		if( pTmp ){` |
|       6 |  8676 | `			ph7_value_string(pTmp,zPeer,-1);` |
|       6 |  8677 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);` |
|       2 |  8678 | `		}` |
|       2 |  8679 | `	}` |
|      40 |  8680 | `	SockArmDefaultTimeout(pCtx,pOut);` |
|      40 |  8681 | `	ph7_result_resource(pCtx,pOut);` |
|      40 |  8682 | `	return PH7_OK;` |
|      29 |  8683 | `}` |
|       - |  8684 | `/*` |
|       - |  8685 | `` * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected`` |
|       - |  8686 | ` * stream that has none, and NULL for a read that did not happen — php writes it` |
|       - |  8687 | ` * on every call rather than leaving the caller's previous value in place.` |
|       - |  8688 | ` */` |
|      26 |  8689 | `static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)` |
|       2 |  8690 | `{` |
|       - |  8691 | `	ph7_value *pTmp;` |
|      28 |  8692 | `	if( iArg >= nArg ){` |
|       9 |  8693 | `		return;` |
|       - |  8694 | `	}` |
|      20 |  8695 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|      20 |  8696 | `	if( pTmp == 0 ){` |
|     ! 0 |  8697 | `		return;` |
|       - |  8698 | `	}` |
|      20 |  8699 | `	if( zAddr ){` |
|      18 |  8700 | `		ph7_value_string(pTmp,zAddr,-1);` |
|      10 |  8701 | `	}else{` |
|       3 |  8702 | `		ph7_value_null(pTmp);` |
|       - |  8703 | `	}` |
|      20 |  8704 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);` |
|      15 |  8705 | `}` |
|       - |  8706 | `/*` |
|       - |  8707 | ` * bool stream_socket_shutdown(resource $stream, int $mode)` |
|       - |  8708 | ` *` |
|       - |  8709 | ` * The half-close: "I am done SENDING" without closing a handle the program` |
|       - |  8710 | ` * still wants to read from, which is how every request/response protocol tells` |
|       - |  8711 | ` * its peer the request is over. Nothing else can say it — fclose() takes the` |
|       - |  8712 | ` * read side with it.` |
|       - |  8713 | ` */` |
|      10 |  8714 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  8715 | `{` |
|       - |  8716 | `	io_private *pDev;` |
|       - |  8717 | `	ph7_socket *pSock;` |
|       - |  8718 | `	ph7_int64 iHow;` |
|       - |  8719 | `	int rc;` |
|      11 |  8720 | `	if( nArg < 2 ){` |
|     ! 0 |  8721 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8722 | `		return PH7_OK;` |
|       - |  8723 | `	}` |
|      11 |  8724 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);` |
|      11 |  8725 | `	if( pDev == 0 ){` |
|     ! 0 |  8726 | `		return rc;` |
|       - |  8727 | `	}` |
|      11 |  8728 | `	iHow = ph7_value_to_int64(apArg[1]);` |
|      11 |  8729 | `	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){` |
|       - |  8730 | `		/* php names the three constants rather than the numbers behind them. */` |
|       4 |  8731 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  8732 | `			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",` |
|       1 |  8733 | `			ph7_function_name(pCtx));` |
|       - |  8734 | `	}` |
|       9 |  8735 | `	pSock = IoPrivateSocket(pDev);` |
|       9 |  8736 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - |  8737 | `		/* Not a socket: php answers false in silence, since there is no` |
|       - |  8738 | `		 * direction to shut down. */` |
|       3 |  8739 | `		ph7_result_bool(pCtx,0);` |
|       3 |  8740 | `		return PH7_OK;` |
|       - |  8741 | `	}` |
|       7 |  8742 | `	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;` |
|       7 |  8743 | `	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){` |
|       - |  8744 | `		/* The read side is gone AND nothing is queued behind it, so this handle` |
|       - |  8745 | `		 * is at its end: php answers feof() for a socket by probing it, and a` |
|       - |  8746 | ``		 * `while (!feof($s))` drain loop after a half-close would otherwise spin`` |
|       - |  8747 | `		 * on a stream that can never answer again. Bytes that HAD arrived are` |
|       - |  8748 | `		 * still handed over — which is why the answer is probed rather than` |
|       - |  8749 | `		 * assumed, and why the device's own latch stays clear. */` |
|       3 |  8750 | `		pDev->bEof = 1;` |
|       1 |  8751 | `	}` |
|       7 |  8752 | `	ph7_result_bool(pCtx,rc);` |
|       7 |  8753 | `	return PH7_OK;` |
|       6 |  8754 | `}` |
|       - |  8755 | `/*` |
|       - |  8756 | ` * string\|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,` |
|       - |  8757 | ` *                                    string &$address = null)` |
|       - |  8758 | ` * int\|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,` |
|       - |  8759 | ` *                               string $address = "")` |
|       - |  8760 | ` *` |
|       - |  8761 | ` * The pair that reaches the socket UNDERNEATH the stream: php's own asks the` |
|       - |  8762 | `` * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can`` |
|       - |  8763 | ` * look at bytes without consuming them (nothing else in the family can) and why` |
|       - |  8764 | ` * a recvfrom() on a handle a line read has already buffered WAITS for more.` |
|       - |  8765 | `` * The `$address` is what a datagram carries and a connected stream does not.`` |
|       - |  8766 | ` */` |
|      28 |  8767 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  8768 | `{` |
|       - |  8769 | `	io_private *pDev;` |
|       - |  8770 | `	ph7_socket *pSock;` |
|       - |  8771 | `	ph7_int64 nLen;` |
|       - |  8772 | `	char zAddr[128],*zBuf;` |
|      30 |  8773 | `	int rc,iFlags = 0,n;` |
|      30 |  8774 | `	if( nArg < 2 ){` |
|     ! 0 |  8775 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8776 | `		return PH7_OK;` |
|       - |  8777 | `	}` |
|      30 |  8778 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      30 |  8779 | `	if( pDev == 0 ){` |
|     ! 0 |  8780 | `		return rc;` |
|       - |  8781 | `	}` |
|      30 |  8782 | `	nLen = ph7_value_to_int64(apArg[1]);` |
|      30 |  8783 | `	if( nLen < 1 ){` |
|       4 |  8784 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 |  8785 | `			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|       - |  8786 | `	}` |
|      28 |  8787 | `	if( nArg > 2 ){` |
|      20 |  8788 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|       9 |  8789 | `	}` |
|      28 |  8790 | `	pSock = IoPrivateSocket(pDev);` |
|      28 |  8791 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       3 |  8792 | `		SockStoreAddress(pCtx,apArg,nArg,3,0);` |
|       3 |  8793 | `		ph7_result_bool(pCtx,0);` |
|       3 |  8794 | `		return PH7_OK;` |
|       - |  8795 | `	}` |
|      26 |  8796 | `	if( nLen > 0x7FFFFFF0 ){` |
|     ! 0 |  8797 | `		nLen = 0x7FFFFFF0;` |
|     ! 0 |  8798 | `	}` |
|      26 |  8799 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      26 |  8800 | `	if( zBuf == 0 ){` |
|     ! 0 |  8801 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  8802 | `	}` |
|      26 |  8803 | `	zAddr[0] = 0;` |
|      26 |  8804 | `	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));` |
|       - |  8805 | `	/* php writes the out-param on every call: the sender's address for a read` |
|       - |  8806 | `	 * that happened (empty for a connected stream, which has none to report) and` |
|       - |  8807 | `	 * NULL for one that did not — never the caller's previous value. */` |
|      26 |  8808 | `	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);` |
|      26 |  8809 | `	if( n < 0 ){` |
|       3 |  8810 | `		ph7_result_bool(pCtx,0);` |
|       2 |  8811 | `	}else{` |
|      24 |  8812 | `		ph7_result_string(pCtx,zBuf,n);` |
|       - |  8813 | `	}` |
|      26 |  8814 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      26 |  8815 | `	return PH7_OK;` |
|      16 |  8816 | `}` |
|       - |  8817 | `/*` |
|       - |  8818 | ` * The address stream_socket_sendto() takes, which is NOT the one an opener` |
|       - |  8819 | ` * reads. php parses this one with network_parse_network_address_with_port(),` |
|       - |  8820 | ` * and the two differ in three places: this one looks for the colon across the` |
|       - |  8821 | `` * WHOLE string (so `1.2.3.4:` is a port of 0 and the send fails with EINVAL,`` |
|       - |  8822 | ` * where an opener answers "Failed to parse address"), it knows no transport at` |
|       - |  8823 | `` * all (so `udp://1.2.3.4:53` names the host `udp`), and an EMPTY host half is a`` |
|       - |  8824 | ` * name like any other, which the resolver is what refuses. The bracketed IPv6` |
|       - |  8825 | ` * form is read here too, and a malformed one has no wording of its own -- php's` |
|       - |  8826 | ` * helper simply fails and the caller prints its own refusal.` |
|       - |  8827 | ` *` |
|       - |  8828 | ` * Answers 1 with zHost and the port filled, or 0 for an address that is none.` |
|       - |  8829 | ` */` |
|      14 |  8830 | `static int SockSendToAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort)` |
|       3 |  8831 | `{` |
|      17 |  8832 | `	int i,nHost = -1,iStart = 0;` |
|      17 |  8833 | `	zHost[0] = 0;` |
|      17 |  8834 | `	if( nAddr > 1 && zAddr[0] == '[' ){` |
|       9 |  8835 | `		for( i = 1 ; i + 1 < nAddr ; i++ ){` |
|       9 |  8836 | `			if( zAddr[i] == ']' ){` |
|       3 |  8837 | `				break;` |
|       - |  8838 | `			}` |
|       4 |  8839 | `		}` |
|       3 |  8840 | `		if( i + 1 >= nAddr \|\| zAddr[i] != ']' \|\| zAddr[i+1] != ':' ){` |
|     ! 0 |  8841 | `			return 0;` |
|       - |  8842 | `		}` |
|       3 |  8843 | `		*pPort = SockParsePort(&zAddr[i+2],nAddr - i - 2);` |
|       3 |  8844 | `		nHost = i - 1;` |
|       3 |  8845 | `		iStart = 1;` |
|       2 |  8846 | `	}else{` |
|     114 |  8847 | `		for( i = 0 ; i < nAddr ; i++ ){` |
|     108 |  8848 | `			if( zAddr[i] == ':' ){` |
|       8 |  8849 | `				*pPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);` |
|       8 |  8850 | `				nHost = i;` |
|       8 |  8851 | `				break;` |
|       - |  8852 | `			}` |
|      52 |  8853 | `		}` |
|      14 |  8854 | `		if( nHost < 0 ){` |
|       8 |  8855 | `			return 0;` |
|       - |  8856 | `		}` |
|       - |  8857 | `	}` |
|      11 |  8858 | `	if( nHost >= nHostBuf ){` |
|     ! 0 |  8859 | `		nHost = nHostBuf - 1;` |
|     ! 0 |  8860 | `	}` |
|      11 |  8861 | `	if( nHost > 0 ){` |
|      11 |  8862 | `		SyMemcpy(&zAddr[iStart],zHost,(sxu32)nHost);` |
|       4 |  8863 | `	}` |
|      11 |  8864 | `	zHost[nHost] = 0;` |
|      11 |  8865 | `	return 1;` |
|      10 |  8866 | `}` |
|      24 |  8867 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  8868 | `{` |
|       - |  8869 | `	io_private *pDev;` |
|       - |  8870 | `	ph7_socket *pSock;` |
|      27 |  8871 | `	const char *zData,*zSentTo = "";` |
|       - |  8872 | `	char zHost[256];` |
|      27 |  8873 | `	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0,bHaveAddr = 0;` |
|      27 |  8874 | `	if( nArg < 2 ){` |
|     ! 0 |  8875 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8876 | `		return PH7_OK;` |
|       - |  8877 | `	}` |
|      27 |  8878 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      27 |  8879 | `	if( pDev == 0 ){` |
|     ! 0 |  8880 | `		return rc;` |
|       - |  8881 | `	}` |
|      27 |  8882 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|      27 |  8883 | `	if( nArg > 2 ){` |
|      17 |  8884 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|       7 |  8885 | `	}` |
|      27 |  8886 | `	zHost[0] = 0;` |
|      27 |  8887 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|       - |  8888 | `		int nAddr;` |
|      17 |  8889 | `		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);` |
|      17 |  8890 | `		if( nAddr > 0 ){` |
|      17 |  8891 | `			if( !SockSendToAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort) ){` |
|      11 |  8892 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       3 |  8893 | ``					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);`` |
|       8 |  8894 | `				ph7_result_bool(pCtx,0);` |
|       8 |  8895 | `				return PH7_OK;` |
|       - |  8896 | `			}` |
|      11 |  8897 | `			zSentTo = zAddr;` |
|      11 |  8898 | `			nSentTo = nAddr;` |
|      11 |  8899 | `			bHaveAddr = 1;` |
|       4 |  8900 | `		}` |
|       4 |  8901 | `	}` |
|      21 |  8902 | `	pSock = IoPrivateSocket(pDev);` |
|      21 |  8903 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - |  8904 | `		/* php answers -1 here rather than false: this one reports the send()` |
|       - |  8905 | `		 * result, and it never made a call. */` |
|       3 |  8906 | `		ph7_result_int(pCtx,-1);` |
|       3 |  8907 | `		return PH7_OK;` |
|       - |  8908 | `	}` |
|       - |  8909 | `	/* A zHost of 0 is "no $address at all", which is php's plain send() to the` |
|       - |  8910 | `	 * connected peer; an address whose host half is EMPTY is a name, and the` |
|       - |  8911 | `	 * resolver is what refuses it. */` |
|      19 |  8912 | `	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,bHaveAddr ? zHost : 0,iPort,&iErr);` |
|      19 |  8913 | `	if( iErr == PH7_NET_ERR_RESOLVE ){` |
|       - |  8914 | `		/* php says it three times for one failure — the resolver's own text, the` |
|       - |  8915 | `		 * name it could not resolve, and the address it therefore could not` |
|       - |  8916 | `		 * parse — and answers FALSE rather than the -1 a failed send gives. */` |
|       - |  8917 | `		char zMsg[512];` |
|     ! 0 |  8918 | `		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|     ! 0 |  8919 | ``		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);`` |
|     ! 0 |  8920 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 |  8921 | ``			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);`` |
|     ! 0 |  8922 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8923 | `		return PH7_OK;` |
|       - |  8924 | `	}` |
|      19 |  8925 | `	if( n < 0 ){` |
|       - |  8926 | `		/* php reports the OS text and hands back the -1 send() answered — this` |
|       - |  8927 | `		 * one never answers false, which is why a caller compares it against 0` |
|       - |  8928 | `		 * rather than testing it for truth. The trailing newline is php's own. */` |
|      10 |  8929 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",` |
|       3 |  8930 | `			PH7_NetStrError(PH7_NetLastError()));` |
|       3 |  8931 | `	}` |
|      19 |  8932 | `	ph7_result_int(pCtx,n);` |
|      19 |  8933 | `	return PH7_OK;` |
|      15 |  8934 | `}` |
|       - |  8935 | `/*` |
|       - |  8936 | ` * array\|false stream_socket_pair(int $domain, int $type, int $protocol)` |
|       - |  8937 | ` *` |
|       - |  8938 | ` * Two connected sockets with no address between them — the two-way pipe a` |
|       - |  8939 | ` * program hands a child, or a test double hands the code under test. Which` |
|       - |  8940 | ` * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses` |
|       - |  8941 | ` * AF_INET, and Windows is the other way round (php emulates the pair over the` |
|       - |  8942 | ` * loopback there, and so does this).` |
|       - |  8943 | ` */` |
|       6 |  8944 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  8945 | `{` |
|       - |  8946 | `	ph7_socket aSock[2];` |
|       - |  8947 | `	io_private *apDev[2];` |
|       - |  8948 | `	ph7_value *pArr,*pVal;` |
|       8 |  8949 | `	int iErrno = 0,i;` |
|       8 |  8950 | `	if( nArg < 3 ){` |
|     ! 0 |  8951 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  8952 | `		return PH7_OK;` |
|       - |  8953 | `	}` |
|       9 |  8954 | `	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),` |
|      11 |  8955 | `		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){` |
|       - |  8956 | `		/* php reports the OS code and its text, in that order and in brackets. */` |
|       5 |  8957 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",` |
|       1 |  8958 | `			iErrno,PH7_NetStrError(iErrno));` |
|       4 |  8959 | `		ph7_result_bool(pCtx,0);` |
|       4 |  8960 | `		return PH7_OK;` |
|       - |  8961 | `	}` |
|       6 |  8962 | `	pArr = ph7_context_new_array(pCtx);` |
|       6 |  8963 | `	pVal = ph7_context_new_scalar(pCtx);` |
|       6 |  8964 | `	apDev[0] = apDev[1] = 0;` |
|       6 |  8965 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|     ! 0 |  8966 | `		PH7_NetClose(aSock[0]);` |
|     ! 0 |  8967 | `		PH7_NetClose(aSock[1]);` |
|     ! 0 |  8968 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  8969 | `	}` |
|      14 |  8970 | `	for( i = 0 ; i < 2 ; i++ ){` |
|       - |  8971 | `		/* No uri: nothing opened these by name, which is what php reports. */` |
|      10 |  8972 | `		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0,0);` |
|      10 |  8973 | `		if( apDev[i] == 0 ){` |
|       - |  8974 | `			/* SockWrapSocket closed the one it could not wrap; the OTHER end is` |
|       - |  8975 | `			 * still ours to close, wrapped or not. */` |
|     ! 0 |  8976 | `			if( i == 0 ){` |
|     ! 0 |  8977 | `				PH7_NetClose(aSock[1]);` |
|     ! 0 |  8978 | `			}else{` |
|     ! 0 |  8979 | `				SockCloseWrapped(pCtx,apDev[0]);` |
|       - |  8980 | `			}` |
|     ! 0 |  8981 | `			return PH7_ContextMemoryError(pCtx);` |
|       - |  8982 | `		}` |
|       - |  8983 | `		/* A pair has no transport of its own, and php labels it apart from a` |
|       - |  8984 | `		 * tcp:// stream for exactly that reason. */` |
|      10 |  8985 | `		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;` |
|      10 |  8986 | `		SockArmDefaultTimeout(pCtx,apDev[i]);` |
|       6 |  8987 | `	}` |
|      14 |  8988 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      10 |  8989 | `		ph7_value_resource(pVal,apDev[i]);` |
|      10 |  8990 | `		ph7_array_add_elem(pArr,0,pVal);` |
|       6 |  8991 | `	}` |
|       6 |  8992 | `	ph7_result_value(pCtx,pArr);` |
|       6 |  8993 | `	return PH7_OK;` |
|       5 |  8994 | `}` |
|       - |  8995 | `/*` |
|       - |  8996 | ` * string\|false stream_socket_get_name(resource $socket, bool $remote)` |
|       - |  8997 | ` *` |
|       - |  8998 | `` * Which address this socket sits on (`$remote` false) or is talking to (true).`` |
|       - |  8999 | `` * It is the only way to learn the port a server bound with `:0` actually got,`` |
|       - |  9000 | ` * so a test that needs a free port had to guess one without it.` |
|       - |  9001 | ` */` |
|      94 |  9002 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  9003 | `{` |
|       - |  9004 | `	io_private *pDev;` |
|       - |  9005 | `	ph7_socket *pSock;` |
|       - |  9006 | `	char zName[128];` |
|       - |  9007 | `	int rc;` |
|      98 |  9008 | `	if( nArg < 2 ){` |
|     ! 0 |  9009 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9010 | `		return PH7_OK;` |
|       - |  9011 | `	}` |
|      98 |  9012 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|      98 |  9013 | `	if( pDev == 0 ){` |
|       5 |  9014 | `		return rc;` |
|       - |  9015 | `	}` |
|      94 |  9016 | `	pSock = IoPrivateSocket(pDev);` |
|      94 |  9017 | `	if( pSock == 0 \|\| PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){` |
|       - |  9018 | `		/* php answers false for a stream that is not a socket, and for the peer` |
|       - |  9019 | `		 * of a socket that is not connected — an unaccepted server. */` |
|      20 |  9020 | `		ph7_result_bool(pCtx,0);` |
|      20 |  9021 | `		return PH7_OK;` |
|       - |  9022 | `	}` |
|      77 |  9023 | `	ph7_result_string(pCtx,zName,-1);` |
|      77 |  9024 | `	return PH7_OK;` |
|      51 |  9025 | `}` |
|       - |  9026 | `/*` |
|       - |  9027 | `` * The two ADDRESS converters, `inet_pton()` and `inet_ntop()`, and the host`` |
|       - |  9028 | ``  * name beside them -- three names that were a loud `Call to undefined function` `` |
|       - |  9029 | ` * until now, which is what a program handling an IPv6 literal ran into on its` |
|       - |  9030 | ` * first line. php hands each address string straight to the C library, so what` |
|       - |  9031 | ` * is accepted is the SYSTEM resolver's grammar rather than php's; this engine` |
|       - |  9032 | ` * writes that grammar itself so a Windows build answers what a POSIX one does` |
|       - |  9033 | ` * (the same reason the iconv converter is PHL's own).` |
|       - |  9034 | ` *` |
|       - |  9035 | ` * An IPv4 literal is four decimal octets of 0-255, each written with no` |
|       - |  9036 | `` * LEADING ZERO (`01.2.3.4` is refused, which is what keeps a dotted quad from`` |
|       - |  9037 | ` * ever being read as octal), and nothing before or behind them.` |
|       - |  9038 | ` */` |
|      28 |  9039 | `static int NetPton4(const char *zIn,int nLen,unsigned char *aOut)` |
|       1 |  9040 | `{` |
|      29 |  9041 | `	int iOctet = 0,iVal = 0,bDigit = 0,i;` |
|     225 |  9042 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     209 |  9043 | `		int c = (unsigned char)zIn[i];` |
|     209 |  9044 | `		if( c >= '0' && c <= '9' ){` |
|     137 |  9045 | `			if( bDigit && iVal == 0 ){` |
|       3 |  9046 | `				return 0; /* a leading zero */` |
|       - |  9047 | `			}` |
|     135 |  9048 | `			iVal = iVal * 10 + (c - '0');` |
|     135 |  9049 | `			if( iVal > 255 ){` |
|       3 |  9050 | `				return 0;` |
|       - |  9051 | `			}` |
|     133 |  9052 | `			if( !bDigit ){` |
|      91 |  9053 | `				if( ++iOctet > 4 ){` |
|     ! 0 |  9054 | `					return 0;` |
|       - |  9055 | `				}` |
|      91 |  9056 | `				bDigit = 1;` |
|      45 |  9057 | `			}` |
|     133 |  9058 | `			aOut[iOctet-1] = (unsigned char)iVal;` |
|     139 |  9059 | `		}else if( c == '.' && bDigit ){` |
|      69 |  9060 | `			if( iOctet == 4 ){` |
|       5 |  9061 | `				return 0; /* a fifth octet, or a trailing dot */` |
|       - |  9062 | `			}` |
|      65 |  9063 | `			bDigit = 0;` |
|      65 |  9064 | `			iVal = 0;` |
|      33 |  9065 | `		}else{` |
|       5 |  9066 | `			return 0;` |
|       - |  9067 | `		}` |
|      99 |  9068 | `	}` |
|      17 |  9069 | `	return (iOctet == 4 && bDigit) ? 1 : 0;` |
|      15 |  9070 | `}` |
|     330 |  9071 | `static int NetIsHexDigit(int c)` |
|       1 |  9072 | `{` |
|     331 |  9073 | `	c &= 0xFF;` |
|     331 |  9074 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       1 |  9075 | `}` |
|     206 |  9076 | `static int NetHexDigitVal(int c)` |
|       1 |  9077 | `{` |
|     207 |  9078 | `	c &= 0xFF;` |
|     207 |  9079 | `	if( c >= '0' && c <= '9' ){` |
|     163 |  9080 | `		return c - '0';` |
|       - |  9081 | `	}` |
|      45 |  9082 | `	return (c \| 0x20) - 'a' + 10;` |
|     104 |  9083 | `}` |
|       - |  9084 | `/*` |
|       - |  9085 | ` * An IPv6 literal: up to eight groups of at most four hex digits, at most ONE` |
|       - |  9086 | `` * `::` standing for the run of zero groups that makes the count up to eight,`` |
|       - |  9087 | `` * and an optional dotted quad in place of the last two groups. The `::` is`` |
|       - |  9088 | ` * remembered as the POSITION it stood at and whatever was written after it is` |
|       - |  9089 | ` * slid to the end once the string has been read -- which is what lets one` |
|       - |  9090 | ` * spelling mean a different number of groups depending on what follows it.` |
|       - |  9091 | ` */` |
|      36 |  9092 | `static int NetPton6(const char *zIn,int nLen,unsigned char *aOut)` |
|       1 |  9093 | `{` |
|       - |  9094 | `	unsigned char aTmp[16];` |
|      37 |  9095 | `	const char *zEnd = &zIn[nLen];` |
|       - |  9096 | `	const char *zTok;` |
|      37 |  9097 | `	int iOut = 0,iGap = -1,nDigit = 0,iVal = 0;` |
|      37 |  9098 | `	SyZero(aTmp,(sxu32)sizeof(aTmp));` |
|      37 |  9099 | `	if( zIn < zEnd && zIn[0] == ':' ){` |
|       - |  9100 | ``		/* A single leading colon belongs to a `::` and to nothing else. */`` |
|      15 |  9101 | `		if( &zIn[1] >= zEnd \|\| zIn[1] != ':' ){` |
|     ! 0 |  9102 | `			return 0;` |
|       - |  9103 | `		}` |
|      15 |  9104 | `		zIn++;` |
|       7 |  9105 | `	}` |
|      37 |  9106 | `	zTok = zIn;` |
|     351 |  9107 | `	while( zIn < zEnd ){` |
|     331 |  9108 | `		int c = (unsigned char)zIn[0];` |
|     331 |  9109 | `		zIn++;` |
|     331 |  9110 | `		if( NetIsHexDigit(c) ){` |
|     207 |  9111 | `			iVal = (iVal<<4) \| NetHexDigitVal(c);` |
|     207 |  9112 | `			if( ++nDigit > 4 ){` |
|       3 |  9113 | `				return 0;` |
|       - |  9114 | `			}` |
|     205 |  9115 | `			continue;` |
|       - |  9116 | `		}` |
|     125 |  9117 | `		if( c == ':' ){` |
|     115 |  9118 | `			zTok = zIn;` |
|     115 |  9119 | `			if( nDigit < 1 ){` |
|      29 |  9120 | `				if( iGap >= 0 ){` |
|       3 |  9121 | ``					return 0; /* a second `::` */`` |
|       - |  9122 | `				}` |
|      27 |  9123 | `				iGap = iOut;` |
|      27 |  9124 | `				continue;` |
|       - |  9125 | `			}` |
|      87 |  9126 | `			if( zIn >= zEnd ){` |
|       3 |  9127 | `				return 0; /* a group with a colon and nothing behind it */` |
|       - |  9128 | `			}` |
|      85 |  9129 | `			if( iOut + 2 > (int)sizeof(aTmp) ){` |
|     ! 0 |  9130 | `				return 0;` |
|       - |  9131 | `			}` |
|      85 |  9132 | `			aTmp[iOut++] = (unsigned char)(iVal>>8);` |
|      85 |  9133 | `			aTmp[iOut++] = (unsigned char)(iVal & 0xFF);` |
|      85 |  9134 | `			nDigit = 0;` |
|      85 |  9135 | `			iVal = 0;` |
|      85 |  9136 | `			continue;` |
|       - |  9137 | `		}` |
|      10 |  9138 | `		if( c == '.' && iOut + 4 <= (int)sizeof(aTmp)` |
|       7 |  9139 | `		 && NetPton4(zTok,(int)(zEnd - zTok),&aTmp[iOut]) ){` |
|       - |  9140 | `			/* A dotted quad runs to the END of the string by definition, so` |
|       - |  9141 | `			 * reading it is also the end of the walk. */` |
|       7 |  9142 | `			iOut += 4;` |
|       7 |  9143 | `			nDigit = 0;` |
|       7 |  9144 | `			break;` |
|       - |  9145 | `		}` |
|       5 |  9146 | `		return 0;` |
|     ! 0 |  9147 | `	}` |
|      27 |  9148 | `	if( nDigit > 0 ){` |
|      17 |  9149 | `		if( iOut + 2 > (int)sizeof(aTmp) ){` |
|       3 |  9150 | `			return 0;` |
|       - |  9151 | `		}` |
|      15 |  9152 | `		aTmp[iOut++] = (unsigned char)(iVal>>8);` |
|      15 |  9153 | `		aTmp[iOut++] = (unsigned char)(iVal & 0xFF);` |
|       7 |  9154 | `	}` |
|      25 |  9155 | `	if( iGap >= 0 ){` |
|       - |  9156 | ``		/* Slide everything written after the `::` to the end; what it steps`` |
|       - |  9157 | `		 * over is already zero. An address that is already full has no room` |
|       - |  9158 | `		 * for a gap at all. */` |
|      21 |  9159 | `		int nTail = iOut - iGap,i;` |
|      21 |  9160 | `		if( iOut == (int)sizeof(aTmp) ){` |
|     ! 0 |  9161 | `			return 0;` |
|       - |  9162 | `		}` |
|      85 |  9163 | `		for( i = 1 ; i <= nTail ; ++i ){` |
|      65 |  9164 | `			aTmp[sizeof(aTmp)-i] = aTmp[iGap + nTail - i];` |
|      65 |  9165 | `			aTmp[iGap + nTail - i] = 0;` |
|      33 |  9166 | `		}` |
|      21 |  9167 | `		iOut = (int)sizeof(aTmp);` |
|      10 |  9168 | `	}` |
|      25 |  9169 | `	if( iOut != (int)sizeof(aTmp) ){` |
|     ! 0 |  9170 | `		return 0;` |
|       - |  9171 | `	}` |
|      25 |  9172 | `	SyMemcpy(aTmp,aOut,(sxu32)sizeof(aTmp));` |
|      25 |  9173 | `	return 1;` |
|      19 |  9174 | `}` |
|       - |  9175 | `/* The dotted quad an IPv4 address prints as; zOut holds at least 16 bytes. */` |
|      10 |  9176 | `static void NetNtop4(const unsigned char *aIn,char *zOut,int nOut)` |
|       1 |  9177 | `{` |
|      11 |  9178 | `	SyBufferFormat(zOut,(sxu32)nOut,"%d.%d.%d.%d",aIn[0],aIn[1],aIn[2],aIn[3]);` |
|      11 |  9179 | `}` |
|       - |  9180 | `/*` |
|       - |  9181 | ` * The text an IPv6 address prints as: lower-case hex groups with no leading` |
|       - |  9182 | `` * zeros, the LONGEST run of zero groups written as `::` (two groups at least,`` |
|       - |  9183 | ` * and the FIRST of them when two runs are the same length), and the last four` |
|       - |  9184 | ` * bytes written as a dotted quad for the two IPv4-carrying shapes -- an` |
|       - |  9185 | `` * address that is all zeros but for them, and an `::ffff:` one. zOut holds at`` |
|       - |  9186 | ` * least 46 bytes.` |
|       - |  9187 | ` */` |
|      24 |  9188 | `static void NetNtop6(const unsigned char *aIn,char *zOut,int nOut)` |
|       1 |  9189 | `{` |
|       - |  9190 | `	unsigned int aWord[8];` |
|      25 |  9191 | `	int iBest = -1,nBest = 0,iCur = -1,nCur = 0,i,n = 0;` |
|     217 |  9192 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     193 |  9193 | `		aWord[i] = ((unsigned int)aIn[i*2] << 8) \| aIn[i*2+1];` |
|      97 |  9194 | `	}` |
|     217 |  9195 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     193 |  9196 | `		if( aWord[i] == 0 ){` |
|     111 |  9197 | `			if( iCur < 0 ){` |
|      23 |  9198 | `				iCur = i;` |
|      23 |  9199 | `				nCur = 1;` |
|      12 |  9200 | `			}else{` |
|      89 |  9201 | `				nCur++;` |
|       - |  9202 | `			}` |
|     111 |  9203 | `			if( nCur > nBest ){` |
|     107 |  9204 | `				iBest = iCur;` |
|     107 |  9205 | `				nBest = nCur;` |
|      53 |  9206 | `			}` |
|      56 |  9207 | `		}else{` |
|      83 |  9208 | `			iCur = -1;` |
|      83 |  9209 | `			nCur = 0;` |
|       - |  9210 | `		}` |
|      97 |  9211 | `	}` |
|      25 |  9212 | `	if( nBest < 2 ){` |
|       5 |  9213 | `		iBest = -1;` |
|       2 |  9214 | `	}` |
|     205 |  9215 | `	for( i = 0 ; i < 8 ; ++i ){` |
|     187 |  9216 | `		if( iBest >= 0 && i >= iBest && i < iBest + nBest ){` |
|     107 |  9217 | `			if( i == iBest ){` |
|      21 |  9218 | `				zOut[n++] = ':';` |
|      10 |  9219 | `			}` |
|     107 |  9220 | `			continue;` |
|       - |  9221 | `		}` |
|      81 |  9222 | `		if( i != 0 ){` |
|      69 |  9223 | `			zOut[n++] = ':';` |
|      34 |  9224 | `		}` |
|      80 |  9225 | `		if( i == 6 && iBest == 0` |
|      12 |  9226 | `		 && (nBest == 6 \|\| (nBest == 5 && aWord[5] == 0xFFFF)) ){` |
|       7 |  9227 | `			NetNtop4(&aIn[12],&zOut[n],nOut - n);` |
|       7 |  9228 | `			n += (int)SyStrlen(&zOut[n]);` |
|       7 |  9229 | `			break;` |
|       - |  9230 | `		}` |
|      75 |  9231 | `		n += (int)SyBufferFormat(&zOut[n],(sxu32)(nOut - n),"%x",aWord[i]);` |
|      38 |  9232 | `	}` |
|      25 |  9233 | `	if( iBest >= 0 && iBest + nBest == 8 ){` |
|       5 |  9234 | `		zOut[n++] = ':';` |
|       2 |  9235 | `	}` |
|      25 |  9236 | `	zOut[n] = 0;` |
|      25 |  9237 | `}` |
|       - |  9238 | `/*` |
|       - |  9239 | ` * string\|false inet_pton(string $ip)` |
|       - |  9240 | ` *` |
|       - |  9241 | ` * The packed bytes an address string stands for -- four for IPv4, sixteen for` |
|       - |  9242 | ` * IPv6. php picks the family by LOOKING at the string: a colon anywhere makes` |
|       - |  9243 | ` * it IPv6, and a string with no dot in it is not an address at all. Every` |
|       - |  9244 | ` * refusal is a silent FALSE.` |
|       - |  9245 | ` */` |
|      62 |  9246 | `PH7_PRIVATE int PH7_builtin_inet_pton(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9247 | `{` |
|       - |  9248 | `	unsigned char aAddr[16];` |
|       - |  9249 | `	const char *zIn;` |
|      63 |  9250 | `	int nLen,i,bColon = 0,bDot = 0;` |
|      63 |  9251 | `	if( nArg < 1 ){` |
|     ! 0 |  9252 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9253 | `		return PH7_OK;` |
|       - |  9254 | `	}` |
|      63 |  9255 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     657 |  9256 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     595 |  9257 | `		if( zIn[i] == ':' ){` |
|     133 |  9258 | `			bColon = 1;` |
|     529 |  9259 | `		}else if( zIn[i] == '.' ){` |
|      87 |  9260 | `			bDot = 1;` |
|      43 |  9261 | `		}` |
|     298 |  9262 | `	}` |
|      63 |  9263 | `	if( bColon ){` |
|      37 |  9264 | `		if( !NetPton6(zIn,nLen,aAddr) ){` |
|      13 |  9265 | `			ph7_result_bool(pCtx,0);` |
|      13 |  9266 | `			return PH7_OK;` |
|       - |  9267 | `		}` |
|      25 |  9268 | `		ph7_result_string(pCtx,(const char *)aAddr,16);` |
|      25 |  9269 | `		return PH7_OK;` |
|       - |  9270 | `	}` |
|      27 |  9271 | `	if( !bDot \|\| !NetPton4(zIn,nLen,aAddr) ){` |
|      19 |  9272 | `		ph7_result_bool(pCtx,0);` |
|      19 |  9273 | `		return PH7_OK;` |
|       - |  9274 | `	}` |
|       9 |  9275 | `	ph7_result_string(pCtx,(const char *)aAddr,4);` |
|       9 |  9276 | `	return PH7_OK;` |
|      32 |  9277 | `}` |
|       - |  9278 | `/*` |
|       - |  9279 | ` * string\|false inet_ntop(string $ip)` |
|       - |  9280 | ` *` |
|       - |  9281 | ` * The address string a packed address prints as. Its LENGTH is what names the` |
|       - |  9282 | ` * family -- four bytes or sixteen -- and any other length is a silent FALSE` |
|       - |  9283 | ` * rather than a diagnostic.` |
|       - |  9284 | ` */` |
|      36 |  9285 | `PH7_PRIVATE int PH7_builtin_inet_ntop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9286 | `{` |
|       - |  9287 | `	char zOut[64];` |
|       - |  9288 | `	const char *zIn;` |
|       - |  9289 | `	int nLen;` |
|      37 |  9290 | `	if( nArg < 1 ){` |
|     ! 0 |  9291 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9292 | `		return PH7_OK;` |
|       - |  9293 | `	}` |
|      37 |  9294 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      37 |  9295 | `	zOut[0] = 0;` |
|      37 |  9296 | `	if( nLen == 4 ){` |
|       5 |  9297 | `		NetNtop4((const unsigned char *)zIn,zOut,(int)sizeof(zOut));` |
|      35 |  9298 | `	}else if( nLen == 16 ){` |
|      25 |  9299 | `		NetNtop6((const unsigned char *)zIn,zOut,(int)sizeof(zOut));` |
|      13 |  9300 | `	}else{` |
|       9 |  9301 | `		ph7_result_bool(pCtx,0);` |
|       9 |  9302 | `		return PH7_OK;` |
|       - |  9303 | `	}` |
|      29 |  9304 | `	ph7_result_string(pCtx,zOut,-1);` |
|      29 |  9305 | `	return PH7_OK;` |
|      19 |  9306 | `}` |
|       - |  9307 | `/*` |
|       - |  9308 | ` * string\|false gethostname()` |
|       - |  9309 | ` *` |
|       - |  9310 | ` * The host's own name, as the OS reports it. php warns and answers false when` |
|       - |  9311 | ` * the call fails, naming the OS code and its text.` |
|       - |  9312 | ` */` |
|       6 |  9313 | `PH7_PRIVATE int PH7_builtin_gethostname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9314 | `{` |
|       - |  9315 | `	char zName[256];` |
|       7 |  9316 | `	int iErrno = 0;` |
|       3 |  9317 | `	SXUNUSED(nArg);` |
|       3 |  9318 | `	SXUNUSED(apArg);` |
|       7 |  9319 | `	zName[0] = 0;` |
|       7 |  9320 | `	if( PH7_NetHostName(zName,(int)sizeof(zName),&iErrno) != PH7_OK ){` |
|     ! 0 |  9321 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 |  9322 | `			"unable to fetch host [%d]: %s",iErrno,PH7_NetStrError(iErrno));` |
|     ! 0 |  9323 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9324 | `		return PH7_OK;` |
|       - |  9325 | `	}` |
|       7 |  9326 | `	ph7_result_string(pCtx,zName,-1);` |
|       7 |  9327 | `	return PH7_OK;` |
|       4 |  9328 | `}` |
|       - |  9329 | `#endif /*` |
|       - |  9330 | ` * The stream SETTINGS family. Every one of these was a loud` |
|       - |  9331 | `` * `Call to undefined function` — so a program that puts a socket in`` |
|       - |  9332 | ` * non-blocking mode, bounds a read with a timeout, or asks whether a stream` |
|       - |  9333 | ` * can be locked before calling flock() did not run at all.` |
|       - |  9334 | ` *` |
|       - |  9335 | ` * The shared preamble: php refuses a non-resource with a TypeError naming the` |
|       - |  9336 | ` * parameter, and an already-closed handle the same way.` |
|       - |  9337 | ` */` |
|  129488 |  9338 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - |  9339 | `	const char *zName,int *pRc)` |
|       5 |  9340 | `{` |
|       - |  9341 | `	char zGiven[64];` |
|       - |  9342 | `	io_private *pDev;` |
|  129493 |  9343 | `	*pRc = PH7_OK;` |
|  129493 |  9344 | `	if( !ph7_value_is_resource(pArg) ){` |
|      77 |  9345 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  9346 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      25 |  9347 | `			ph7_function_name(pCtx),iPos,zName,VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|      52 |  9348 | `		return 0;` |
|       - |  9349 | `	}` |
|  129443 |  9350 | `	pDev = (io_private *)ph7_value_to_resource(pArg);` |
|  129443 |  9351 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|     106 |  9352 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  9353 | `			"%s(): Argument #%d ($%s) must be an open stream resource",` |
|      35 |  9354 | `			ph7_function_name(pCtx),iPos,zName);` |
|      71 |  9355 | `		return 0;` |
|       - |  9356 | `	}` |
|  129373 |  9357 | `	return pDev;` |
|   64080 |  9358 | `}` |
|       - |  9359 | ``/* The whole settings family names its one handle `$stream`; the copy names two. */`` |
|     226 |  9360 | `static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)` |
|       4 |  9361 | `{` |
|     230 |  9362 | `	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);` |
|       4 |  9363 | `}` |
|       - |  9364 | `/* The same screen, for the filter family in vfs_filter.c. */` |
|  128962 |  9365 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|       - |  9366 | `	const char *zName,int *pRc)` |
|       5 |  9367 | `{` |
|  128967 |  9368 | `	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);` |
|       5 |  9369 | `}` |
|       - |  9370 | `/* The tcp:// socket behind a handle, or 0 for any other device. */` |
|     635 |  9371 | `static ph7_socket * IoPrivateSocket(io_private *pDev)` |
|       5 |  9372 | `{` |
|       - |  9373 | `#ifdef PH7_ENABLE_NET` |
|     640 |  9374 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|     507 |  9375 | `		return &((sock_private *)pDev->pHandle)->sock;` |
|       - |  9376 | `	}` |
|     136 |  9377 | `	if( PH7_HttpStreamIs(pDev->pStream) && pDev->pHandle ){` |
|       - |  9378 | `		/* An http body is still a socket, and php's stream_select() waits on` |
|       - |  9379 | `		 * that descriptor exactly as it does for a transport stream. */` |
|     ! 0 |  9380 | `		return PH7_HttpStreamSocket(pDev->pHandle);` |
|       - |  9381 | `	}` |
|       - |  9382 | `#endif` |
|      66 |  9383 | `	SXUNUSED(pDev); /* cc warning when NET is off */` |
|     136 |  9384 | `	return 0;` |
|     323 |  9385 | `}` |
|       - |  9386 | `#ifdef PH7_ENABLE_NET` |
|       - |  9387 | `/*` |
|       - |  9388 | ` * ext/sockets' two doors onto this device stack, and the close that goes with` |
|       - |  9389 | ` * them. php ties a Socket and an exported stream together in BOTH directions --` |
|       - |  9390 | ``  * `socket_export_stream()` twice answers the same handle, and `socket_close()` `` |
|       - |  9391 | ` * on an exported socket leaves the stream closed too -- so the extension needs` |
|       - |  9392 | ` * the descriptor behind a stream, a stream around a descriptor, and a close` |
|       - |  9393 | ` * that runs the device's own teardown rather than the raw closesocket().` |
|       - |  9394 | ` */` |
|       4 |  9395 | `PH7_PRIVATE int PH7_StreamSocketHandle(io_private *pDev,ph7_socket *pOut)` |
|       1 |  9396 | `{` |
|       - |  9397 | `	ph7_socket *pSock;` |
|       5 |  9398 | `	if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) ){` |
|     ! 0 |  9399 | `		return 0;` |
|       - |  9400 | `	}` |
|       5 |  9401 | `	pSock = IoPrivateSocket(PH7_StreamUnwrap(pDev));` |
|       5 |  9402 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|       3 |  9403 | `		return 0;` |
|       - |  9404 | `	}` |
|       3 |  9405 | `	*pOut = *pSock;` |
|       3 |  9406 | `	return 1;` |
|       3 |  9407 | `}` |
|       8 |  9408 | `PH7_PRIVATE io_private * PH7_StreamWrapSocket(ph7_context *pCtx,ph7_socket sock,int bDgram,` |
|       - |  9409 | `	const char *zLabel,const char *zUri)` |
|       1 |  9410 | `{` |
|      17 |  9411 | `	io_private *pDev = SockWrapSocket(pCtx,sock,bDgram,zUri,` |
|       8 |  9412 | `		zUri ? (int)SyStrlen(zUri) : 0);` |
|       9 |  9413 | `	if( pDev ){` |
|       9 |  9414 | `		((sock_private *)pDev->pHandle)->zLabel = zLabel;` |
|       4 |  9415 | `	}` |
|       9 |  9416 | `	return pDev;` |
|       1 |  9417 | `}` |
|       - |  9418 | `/*` |
|       - |  9419 | ` * The close socket_close() runs on a socket it had exported: the device's own` |
|       - |  9420 | ` * teardown (which closes the descriptor) followed by the same closed-marking` |
|       - |  9421 | ` * fclose() does, so every ph7_value still naming the handle reports it shut` |
|       - |  9422 | ` * rather than reading a freed device.` |
|       - |  9423 | ` */` |
|     ! 0 |  9424 | `PH7_PRIVATE void PH7_StreamCloseExported(io_private *pDev)` |
|     ! 0 |  9425 | `{` |
|     ! 0 |  9426 | `	if( pDev == 0 \|\| IO_PRIVATE_INVALID(pDev) \|\| pDev->pStream == 0 ){` |
|     ! 0 |  9427 | `		return;` |
|       - |  9428 | `	}` |
|     ! 0 |  9429 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     ! 0 |  9430 | `	PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|     ! 0 |  9431 | `	MarkIOPrivateClosed(pDev);` |
|     ! 0 |  9432 | `}` |
|       - |  9433 | `#endif /* PH7_ENABLE_NET */` |
|       - |  9434 | `/*` |
|       - |  9435 | ` * bool stream_set_blocking(resource $stream, bool $enable)` |
|       - |  9436 | ` *` |
|       - |  9437 | ` * php sets the mode AT the descriptor and answers TRUE either way; a stream` |
|       - |  9438 | ` * with no descriptor — a memory buffer, a data:// payload — keeps reporting` |
|       - |  9439 | ` * itself blocked, which is why the flag is only recorded when it took. On` |
|       - |  9440 | ` * Windows php's plain-files device has no O_NONBLOCK to set, so every such` |
|       - |  9441 | ` * stream (a file, a pipe, php://stdin) answers FALSE there.` |
|       - |  9442 | ` */` |
|     102 |  9443 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  9444 | `{` |
|       - |  9445 | `	io_private *pDev;` |
|       - |  9446 | `	int rc,bEnable,fd;` |
|       - |  9447 | `	ph7_socket *pSock;` |
|     106 |  9448 | `	if( nArg < 2 ){` |
|     ! 0 |  9449 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9450 | `		return PH7_OK;` |
|       - |  9451 | `	}` |
|     106 |  9452 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     106 |  9453 | `	if( pDev == 0 ){` |
|       3 |  9454 | `		return rc;` |
|       - |  9455 | `	}` |
|     104 |  9456 | `	bEnable = ph7_value_to_bool(apArg[1]);` |
|     104 |  9457 | `	pSock = IoPrivateSocket(pDev);` |
|     104 |  9458 | `	if( pSock ){` |
|       - |  9459 | `#ifdef PH7_ENABLE_NET` |
|      17 |  9460 | `		if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|       - |  9461 | `			/* No socket to set the mode on: php's own answer is FALSE, which is` |
|       - |  9462 | `			 * the one place this family reports a setting that did not take. */` |
|       3 |  9463 | `			ph7_result_bool(pCtx,0);` |
|       3 |  9464 | `			return PH7_OK;` |
|       - |  9465 | `		}` |
|      15 |  9466 | `		PH7_NetSetBlocking(*pSock,bEnable);` |
|       - |  9467 | `#endif` |
|      15 |  9468 | `		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|       9 |  9469 | `	}else{` |
|       - |  9470 | `#ifdef __WINNT__` |
|       1 |  9471 | `		if( PH7_StreamIsPlainDevice(pDev) ){` |
|       1 |  9472 | `			ph7_result_bool(pCtx,0);` |
|       1 |  9473 | `			return PH7_OK;` |
|       - |  9474 | `		}` |
|       - |  9475 | `#endif` |
|      87 |  9476 | `		fd = PH7_StreamPosixFd(pDev);` |
|      86 |  9477 | `		if( fd >= 0 ){` |
|       - |  9478 | `#ifndef __WINNT__` |
|      78 |  9479 | `			int iFlags = fcntl(fd,F_GETFL,0);` |
|      78 |  9480 | `			if( iFlags >= 0 ){` |
|      78 |  9481 | `				if( bEnable ){` |
|       4 |  9482 | `					iFlags &= ~O_NONBLOCK;` |
|       2 |  9483 | `				}else{` |
|      74 |  9484 | `					iFlags \|= O_NONBLOCK;` |
|       - |  9485 | `				}` |
|      78 |  9486 | `				if( fcntl(fd,F_SETFL,iFlags) == 0 ){` |
|      78 |  9487 | `					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      39 |  9488 | `				}` |
|      39 |  9489 | `			}` |
|       - |  9490 | `#endif` |
|      39 |  9491 | `		}` |
|       - |  9492 | `	}` |
|     102 |  9493 | `	ph7_result_bool(pCtx,1);` |
|     102 |  9494 | `	return PH7_OK;` |
|      55 |  9495 | `}` |
|       - |  9496 | `/*` |
|       - |  9497 | ` * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)` |
|       - |  9498 | ` *` |
|       - |  9499 | ` * php answers TRUE only for a stream whose transport HAS a timeout — a socket —` |
|       - |  9500 | ` * and FALSE for every file, pipe and memory buffer, because there is nothing` |
|       - |  9501 | ` * to wait on. Silently accepting it for a file would tell a caller its read is` |
|       - |  9502 | ` * bounded when it is not.` |
|       - |  9503 | ` */` |
|      38 |  9504 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9505 | `{` |
|       - |  9506 | `	io_private *pDev;` |
|       - |  9507 | `	ph7_socket *pSock;` |
|       - |  9508 | `	int rc;` |
|      39 |  9509 | `	if( nArg < 2 ){` |
|     ! 0 |  9510 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9511 | `		return PH7_OK;` |
|       - |  9512 | `	}` |
|      39 |  9513 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      39 |  9514 | `	if( pDev == 0 ){` |
|     ! 0 |  9515 | `		return rc;` |
|       - |  9516 | `	}` |
|      39 |  9517 | `	pSock = IoPrivateSocket(pDev);` |
|      39 |  9518 | `	if( pSock == 0 ){` |
|       7 |  9519 | `		ph7_result_bool(pCtx,0);` |
|       7 |  9520 | `		return PH7_OK;` |
|       - |  9521 | `	}` |
|       - |  9522 | `#ifdef PH7_ENABLE_NET` |
|       - |  9523 | `	{` |
|      32 |  9524 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);` |
|      32 |  9525 | `		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|      32 |  9526 | `		if( iSec < 0 ){` |
|     ! 0 |  9527 | `			iSec = 0;` |
|     ! 0 |  9528 | `		}` |
|      32 |  9529 | `		if( iUsec < 0 ){` |
|     ! 0 |  9530 | `			iUsec = 0;` |
|     ! 0 |  9531 | `		}` |
|      32 |  9532 | `		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);` |
|       - |  9533 | `		/* An expired read answers FALSE and says so through the metadata;` |
|       - |  9534 | `		 * without the armed flag it is indistinguishable from a non-blocking` |
|       - |  9535 | `		 * one, which answers "". */` |
|      32 |  9536 | `		pDev->bHasTimeout = 1;` |
|      32 |  9537 | `		pDev->bTimedOut = 0;` |
|       - |  9538 | `	}` |
|       - |  9539 | `#endif` |
|      32 |  9540 | `	ph7_result_bool(pCtx,1);` |
|      32 |  9541 | `	return PH7_OK;` |
|      20 |  9542 | `}` |
|       - |  9543 | `/*` |
|       - |  9544 | ` * int stream_set_chunk_size(resource $stream, int $size)` |
|       - |  9545 | ` *` |
|       - |  9546 | ` * Answers the PREVIOUS size, which is what makes the setting restorable, and` |
|       - |  9547 | ` * refuses a non-positive one the way php does.` |
|       - |  9548 | ` */` |
|      46 |  9549 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9550 | `{` |
|       - |  9551 | `	io_private *pDev;` |
|       - |  9552 | `	ph7_int64 nSize;` |
|       - |  9553 | `	int rc;` |
|      47 |  9554 | `	if( nArg < 2 ){` |
|     ! 0 |  9555 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 |  9556 | `		return PH7_OK;` |
|       - |  9557 | `	}` |
|      47 |  9558 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      47 |  9559 | `	if( pDev == 0 ){` |
|     ! 0 |  9560 | `		return rc;` |
|       - |  9561 | `	}` |
|      47 |  9562 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|      47 |  9563 | `	if( nSize < 1 ){` |
|       5 |  9564 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  9565 | `			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");` |
|       - |  9566 | `	}` |
|      43 |  9567 | `	if( nSize > (ph7_int64)SXI32_HIGH ){` |
|       - |  9568 | `		/* php's own ceiling: the size is an int on its side, and storing a` |
|       - |  9569 | `		 * larger one made the NEXT call report a size no caller ever set. */` |
|       3 |  9570 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  9571 | `			"stream_set_chunk_size(): Argument #2 ($size) is too large");` |
|       - |  9572 | `	}` |
|      41 |  9573 | `	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);` |
|      41 |  9574 | `	pDev->nChunk = (sxu32)nSize;` |
|      41 |  9575 | `	return PH7_OK;` |
|      24 |  9576 | `}` |
|       - |  9577 | `/*` |
|       - |  9578 | ` * int stream_set_read_buffer(resource $stream, int $size)` |
|       - |  9579 | ` * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]` |
|       - |  9580 | ` *` |
|       - |  9581 | ` * php's stream layer has no stdio buffer left to hand these to: the read side` |
|       - |  9582 | ` * answers 0 (accepted) and the write side -1 (unsupported), for every stream` |
|       - |  9583 | ` * and every size. Both are still validated arguments, so a bad handle is the` |
|       - |  9584 | ` * same TypeError the rest of the family raises.` |
|       - |  9585 | ` */` |
|       6 |  9586 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9587 | `{` |
|       7 |  9588 | `	int rc = PH7_OK;` |
|       7 |  9589 | `	if( nArg < 2 ){` |
|     ! 0 |  9590 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 |  9591 | `		return PH7_OK;` |
|       - |  9592 | `	}` |
|       7 |  9593 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|     ! 0 |  9594 | `		return rc;` |
|       - |  9595 | `	}` |
|       7 |  9596 | `	ph7_result_int(pCtx,0);` |
|       7 |  9597 | `	return PH7_OK;` |
|       4 |  9598 | `}` |
|      12 |  9599 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9600 | `{` |
|      13 |  9601 | `	int rc = PH7_OK;` |
|      13 |  9602 | `	if( nArg < 2 ){` |
|     ! 0 |  9603 | `		ph7_result_int(pCtx,-1);` |
|     ! 0 |  9604 | `		return PH7_OK;` |
|       - |  9605 | `	}` |
|      13 |  9606 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|     ! 0 |  9607 | `		return rc;` |
|       - |  9608 | `	}` |
|      13 |  9609 | `	ph7_result_int(pCtx,-1);` |
|      13 |  9610 | `	return PH7_OK;` |
|       7 |  9611 | `}` |
|       - |  9612 | `/*` |
|       - |  9613 | ` * int\|false stream_copy_to_stream(resource $from, resource $to,` |
|       - |  9614 | ` *                                 ?int $length = null, int $offset = 0)` |
|       - |  9615 | ` *` |
|       - |  9616 | ` * The everyday way to move bytes between two open streams, and a loud` |
|       - |  9617 | `` * `Call to undefined function` here until now — so the workaround was`` |
|       - |  9618 | `` * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into`` |
|       - |  9619 | ` * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset` |
|       - |  9620 | ` * seeks the source first and is php's only failure shape short of a broken` |
|       - |  9621 | ` * write.` |
|       - |  9622 | ` */` |
|      38 |  9623 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9624 | `{` |
|       - |  9625 | `	io_private *pFrom,*pTo;` |
|      39 |  9626 | `	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;` |
|       - |  9627 | `	char zBuf[8192];` |
|       - |  9628 | `	int rc;` |
|      39 |  9629 | `	if( nArg < 2 ){` |
|     ! 0 |  9630 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9631 | `		return PH7_OK;` |
|       - |  9632 | `	}` |
|      39 |  9633 | `	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);` |
|      39 |  9634 | `	if( pFrom == 0 ){` |
|       3 |  9635 | `		return rc;` |
|       - |  9636 | `	}` |
|      37 |  9637 | `	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);` |
|      37 |  9638 | `	if( pTo == 0 ){` |
|       3 |  9639 | `		return rc;` |
|       - |  9640 | `	}` |
|      34 |  9641 | `	if( pFrom->pStream == 0 \|\| pFrom->pStream->xRead == 0` |
|      35 |  9642 | `	 \|\| pTo->pStream == 0 \|\| pTo->pStream->xWrite == 0 ){` |
|     ! 0 |  9643 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  9644 | `		return PH7_OK;` |
|       - |  9645 | `	}` |
|      35 |  9646 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      11 |  9647 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|       5 |  9648 | `	}` |
|      35 |  9649 | `	if( nArg > 3 ){` |
|      19 |  9650 | `		nOfft = ph7_value_to_int64(apArg[3]);` |
|       9 |  9651 | `	}` |
|      35 |  9652 | `	if( nOfft > 0 ){` |
|       - |  9653 | `		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has` |
|       - |  9654 | `		 * no position to move to, and silently copying from wherever it` |
|       - |  9655 | `		 * happens to be would answer for a different slice of the stream. */` |
|       8 |  9656 | `		if( pFrom->pStream->xSeek == 0` |
|       8 |  9657 | `		 \|\| pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){` |
|       2 |  9658 | `			if( pFrom->pStream->xSeek == 0 ){` |
|       2 |  9659 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|       - |  9660 | `					"stream_copy_to_stream(): Stream does not support seeking");` |
|       1 |  9661 | `			}` |
|       3 |  9662 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       1 |  9663 | `				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);` |
|       2 |  9664 | `			ph7_result_bool(pCtx,0);` |
|       2 |  9665 | `			return PH7_OK;` |
|       - |  9666 | `		}` |
|       7 |  9667 | `		ResetIOPrivate(pFrom);` |
|       7 |  9668 | `		StreamSeekLanded(pFrom,nOfft,0/*SEEK_SET*/);` |
|       3 |  9669 | `	}` |
|      33 |  9670 | `	if( nWant == 0 ){` |
|       3 |  9671 | `		ph7_result_int(pCtx,0);` |
|       3 |  9672 | `		return PH7_OK;` |
|       - |  9673 | `	}` |
|       - |  9674 | `#ifdef __WINNT__` |
|       - |  9675 | `	/* An unfiltered plain-file source goes through php's memory-mapped copy,` |
|       - |  9676 | `	 * whose Windows view at the end of the file is a failure: see` |
|       - |  9677 | `	 * PH7_WinFileMapsEmptyView(). */` |
|       - |  9678 | `	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0` |
|       1 |  9679 | `	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft` |
|       - |  9680 | `			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){` |
|       1 |  9681 | `		ph7_result_bool(pCtx,0);` |
|       1 |  9682 | `		return PH7_OK;` |
|       - |  9683 | `	}` |
|       - |  9684 | `#endif` |
|       - |  9685 | `	/* The destination may be sitting past its own read-ahead; the write has to` |
|       - |  9686 | `	 * land where the SCRIPT is, the rule fwrite() follows. */` |
|      31 |  9687 | `	StreamSeekBackForWrite(pTo);` |
|      39 |  9688 | `	for(;;){` |
|      55 |  9689 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|       - |  9690 | `		ph7_int64 nRead,nWr;` |
|      55 |  9691 | `		if( nWant > 0 && nWant - nTotal < nAsk ){` |
|      13 |  9692 | `			nAsk = nWant - nTotal;` |
|       6 |  9693 | `		}` |
|      55 |  9694 | `		if( nAsk < 1 ){` |
|       5 |  9695 | `			break;` |
|       - |  9696 | `		}` |
|      51 |  9697 | `		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);` |
|      51 |  9698 | `		if( nRead < 1 ){` |
|      27 |  9699 | `			break;` |
|       - |  9700 | `		}` |
|      25 |  9701 | `		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);` |
|      25 |  9702 | `		if( nWr < 0 ){` |
|     ! 0 |  9703 | `			SockReportWriteFailure(pCtx,pTo,(int)nRead);` |
|     ! 0 |  9704 | `			break;` |
|       - |  9705 | `		}` |
|      25 |  9706 | `		nTotal += nWr;` |
|      25 |  9707 | `		if( nWr < nRead ){` |
|     ! 0 |  9708 | `			break;` |
|       - |  9709 | `		}` |
|       1 |  9710 | `	}` |
|      31 |  9711 | `	ph7_result_int64(pCtx,nTotal);` |
|      31 |  9712 | `	return PH7_OK;` |
|      20 |  9713 | `}` |
|       - |  9714 | `/*` |
|       - |  9715 | ` * int\|false stream_select(?array &$read, ?array &$write, ?array &$except,` |
|       - |  9716 | ` *                         ?int $seconds, ?int $microseconds = null)` |
|       - |  9717 | ` *` |
|       - |  9718 | ` * The name that makes a program WAIT on several streams at once, and the reason` |
|       - |  9719 | ` * the settings family that shipped beside it had nothing to wait with: a` |
|       - |  9720 | ` * non-blocking read tells you a stream is not ready, and only this tells you` |
|       - |  9721 | ` * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server` |
|       - |  9722 | ` * loop and every event loop written in php is built on, and it was a loud` |
|       - |  9723 | `` * `Call to undefined function`.`` |
|       - |  9724 | ` *` |
|       - |  9725 | ` * php's own shape, and the parts of it a re-derivation misses: the arrays are` |
|       - |  9726 | ` * REWRITTEN in place to hold only the ready entries, under their original keys;` |
|       - |  9727 | ` * a stream that cannot be represented as a descriptor is a warning naming its` |
|       - |  9728 | ` * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and` |
|       - |  9729 | ` * a stream whose own read buffer still holds bytes is answered READY without` |
|       - |  9730 | ` * asking the OS at all — which is the difference between a loop that drains a` |
|       - |  9731 | ` * buffered handle and one that waits forever for data it has already read.` |
|       - |  9732 | ` */` |
|       - |  9733 | `#if !defined(__WINNT__) \|\| defined(PH7_ENABLE_NET)` |
|       - |  9734 | `#define STREAM_SELECT_OK 1` |
|       - |  9735 | `#ifdef __UNIXES__` |
|       - |  9736 | `#include <sys/select.h>` |
|       - |  9737 | `#include <sys/time.h>` |
|       - |  9738 | `#endif` |
|       - |  9739 | `#endif` |
|       - |  9740 | `#define SEL_READ   0` |
|       - |  9741 | `#define SEL_WRITE  1` |
|       - |  9742 | `#define SEL_EXCEPT 2` |
|       - |  9743 | `/* What one walk over an argument is for. The order matters: php COUNTS the` |
|       - |  9744 | ` * already-buffered readable handles before it waits, and only rewrites the` |
|       - |  9745 | ` * arrays once it knows which answer it is giving. */` |
|       - |  9746 | `#define SELM_COLLECT  0 /* put every representable handle in its fd_set */` |
|       - |  9747 | `#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */` |
|       - |  9748 | `#define SELM_READY    2 /* keep the handles select() reported */` |
|       - |  9749 | `#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */` |
|       - |  9750 | `typedef struct stream_select_ctx stream_select_ctx;` |
|       - |  9751 | `struct stream_select_ctx` |
|       - |  9752 | `{` |
|       - |  9753 | `	ph7_context *pCtx;` |
|       - |  9754 | `#ifdef STREAM_SELECT_OK` |
|       - |  9755 | `	fd_set aSet[3];    /* read / write / except, as select() takes them */` |
|       - |  9756 | `#endif` |
|       - |  9757 | `	int iMaxFd;` |
|       - |  9758 | `	int nSelectable;   /* entries that could be represented at all */` |
|       - |  9759 | `	int iWhich;        /* the set being walked (SEL_*) */` |
|       - |  9760 | `	int iMode;         /* SELM_*: what this walk is FOR */` |
|       - |  9761 | `	int nReady;` |
|       - |  9762 | `	int bBadEntry;     /* an entry that is not a stream at all */` |
|       - |  9763 | `	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */` |
|       - |  9764 | `	ph7_value *pOut;   /* the rebuilt array, while harvesting */` |
|       - |  9765 | `};` |
|       - |  9766 | `/*` |
|       - |  9767 | ` * What a select can WAIT on for this handle: the POSIX descriptor, or the` |
|       - |  9768 | ` * SOCKET, which is the only waitable thing a stream carries on Windows (the` |
|       - |  9769 | ` * file devices hold a HANDLE there, and select() cannot take one — a recorded` |
|       - |  9770 | ` * platform difference). Answers -1 for a device with neither: a memory` |
|       - |  9771 | ` * buffer, a data:// payload, a userland wrapper.` |
|       - |  9772 | ` */` |
|      90 |  9773 | `static ph7_int64 IoPrivateSelectHandle(io_private *pDev)` |
|       1 |  9774 | `{` |
|       - |  9775 | `	int fd;` |
|       - |  9776 | `#ifdef PH7_ENABLE_NET` |
|      91 |  9777 | `	ph7_socket *pSock = IoPrivateSocket(pDev);` |
|      91 |  9778 | `	if( pSock ){` |
|      65 |  9779 | `		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;` |
|       - |  9780 | `	}` |
|       - |  9781 | `#endif` |
|      27 |  9782 | `	fd = PH7_StreamPosixFd(pDev);` |
|      27 |  9783 | `	return fd < 0 ? -1 : (ph7_int64)fd;` |
|      46 |  9784 | `}` |
|       - |  9785 | `/* Bytes this handle has already pulled off the device and not yet handed over. */` |
|      42 |  9786 | `static sxu32 IoPrivateUnread(io_private *pDev)` |
|       1 |  9787 | `{` |
|      43 |  9788 | `	return StreamAheadBytes(pDev);` |
|       1 |  9789 | `}` |
|      50 |  9790 | `static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)` |
|       1 |  9791 | `{` |
|       - |  9792 | `#ifdef STREAM_SELECT_OK` |
|       - |  9793 | `#ifdef __WINNT__` |
|       - |  9794 | `	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many` |
|       - |  9795 | `	 * are in it already rather than the value of this one. */` |
|       1 |  9796 | `	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){` |
|     ! 0 |  9797 | `		return;` |
|       - |  9798 | `	}` |
|       1 |  9799 | `	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);` |
|       - |  9800 | `#else` |
|      50 |  9801 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|       - |  9802 | `		/* php ignores a descriptor an fd_set cannot hold (its own` |
|       - |  9803 | `		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */` |
|     ! 0 |  9804 | `		return;` |
|       - |  9805 | `	}` |
|      50 |  9806 | `	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);` |
|       - |  9807 | `#endif` |
|      51 |  9808 | `	if( h > (ph7_int64)pSel->iMaxFd ){` |
|      41 |  9809 | `		pSel->iMaxFd = (int)h;` |
|      20 |  9810 | `	}` |
|       - |  9811 | `#else` |
|       - |  9812 | `	SXUNUSED(pSel);` |
|       - |  9813 | `	SXUNUSED(h);` |
|       - |  9814 | `#endif` |
|      26 |  9815 | `}` |
|      32 |  9816 | `static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)` |
|       1 |  9817 | `{` |
|       - |  9818 | `#ifdef STREAM_SELECT_OK` |
|       - |  9819 | `#ifdef __WINNT__` |
|       1 |  9820 | `	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|       - |  9821 | `#else` |
|      32 |  9822 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|     ! 0 |  9823 | `		return 0;` |
|       - |  9824 | `	}` |
|      32 |  9825 | `	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|       - |  9826 | `#endif` |
|       - |  9827 | `#else` |
|       - |  9828 | `	SXUNUSED(pSel);` |
|       - |  9829 | `	SXUNUSED(h);` |
|       - |  9830 | `	return 0;` |
|       - |  9831 | `#endif` |
|      17 |  9832 | `}` |
|       - |  9833 | `/*` |
|       - |  9834 | ` * One entry of one array: collected on the way in, harvested on the way out.` |
|       - |  9835 | ` * php never stops for an entry it cannot use — the diagnostics are remembered` |
|       - |  9836 | ` * and raised once the whole set is known, because whether the array held` |
|       - |  9837 | ` * ANYTHING selectable decides which of them php raises.` |
|       - |  9838 | ` */` |
|     140 |  9839 | `static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       1 |  9840 | `{` |
|     141 |  9841 | `	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;` |
|       - |  9842 | `	io_private *pDev;` |
|       - |  9843 | `	ph7_int64 h;` |
|     141 |  9844 | `	if( !ph7_value_is_resource(pValue) ){` |
|       7 |  9845 | `		pSel->bBadEntry = 1;` |
|       - |  9846 | `		/* php words a value that is not a resource apart from a resource that is` |
|       - |  9847 | `		 * no longer open, and raises one per bad entry — so the LAST one seen is` |
|       - |  9848 | `		 * the message that reaches the caller. */` |
|       7 |  9849 | `		pSel->bBadClosed = 0;` |
|       7 |  9850 | `		return PH7_OK;` |
|       - |  9851 | `	}` |
|     135 |  9852 | `	pDev = (io_private *)ph7_value_to_resource(pValue);` |
|     135 |  9853 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       3 |  9854 | `		pSel->bBadEntry = pSel->bBadClosed = 1;` |
|       3 |  9855 | `		return PH7_OK;` |
|       - |  9856 | `	}` |
|     133 |  9857 | `	if( pSel->iMode == SELM_BUFFERED ){` |
|       - |  9858 | `		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a` |
|       - |  9859 | `		 * readable stream with no descriptor at all take part (a userland` |
|       - |  9860 | `		 * wrapper a line read has filled the buffer of), and answering 0 for one` |
|       - |  9861 | `		 * would sleep out the whole timeout over bytes the script already has. */` |
|      43 |  9862 | `		if( IoPrivateUnread(pDev) > 0 ){` |
|       9 |  9863 | `			if( pSel->pOut ){` |
|       5 |  9864 | `				ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|       2 |  9865 | `			}` |
|       9 |  9866 | `			pSel->nReady++;` |
|       4 |  9867 | `		}` |
|      43 |  9868 | `		return PH7_OK;` |
|       - |  9869 | `	}` |
|      91 |  9870 | `	h = IoPrivateSelectHandle(pDev);` |
|      91 |  9871 | `	if( h < 0 ){` |
|       7 |  9872 | `		if( pSel->iMode == SELM_COLLECT ){` |
|       - |  9873 | `			const char *zWrapper,*zLabel;` |
|       5 |  9874 | `			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|       7 |  9875 | `			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,` |
|       2 |  9876 | `				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);` |
|       2 |  9877 | `		}` |
|       7 |  9878 | `		return PH7_OK;` |
|       - |  9879 | `	}` |
|      85 |  9880 | `	if( pSel->iMode == SELM_COLLECT ){` |
|      51 |  9881 | `		pSel->nSelectable++;` |
|      51 |  9882 | `		StreamSelectAdd(pSel,h);` |
|      51 |  9883 | `		return PH7_OK;` |
|       - |  9884 | `	}` |
|      35 |  9885 | `	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){` |
|      23 |  9886 | `		if( pSel->pOut ){` |
|      23 |  9887 | `			ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|      11 |  9888 | `		}` |
|      23 |  9889 | `		pSel->nReady++;` |
|      11 |  9890 | `	}` |
|      35 |  9891 | `	return PH7_OK;` |
|      71 |  9892 | `}` |
|       - |  9893 | `/* The wait itself, over the pair the caller's numbers were normalised into. */` |
|      24 |  9894 | `static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,` |
|       - |  9895 | `	int *pErrno)` |
|       1 |  9896 | `{` |
|       - |  9897 | `#ifdef STREAM_SELECT_OK` |
|      25 |  9898 | `	struct timeval tv,*pTv = 0;` |
|       - |  9899 | `	int rc;` |
|      25 |  9900 | `	if( !bBlock ){` |
|      25 |  9901 | `		tv.tv_sec = (long)iSec;` |
|      25 |  9902 | `		tv.tv_usec = (long)iUsec;` |
|      25 |  9903 | `		pTv = &tv;` |
|      12 |  9904 | `	}` |
|      37 |  9905 | `	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],` |
|      12 |  9906 | `		&pSel->aSet[SEL_EXCEPT],pTv);` |
|      25 |  9907 | `	if( rc < 0 && pErrno ){` |
|       - |  9908 | `#ifdef __WINNT__` |
|     ! 0 |  9909 | `		*pErrno = WSAGetLastError();` |
|       - |  9910 | `#else` |
|     ! 0 |  9911 | `		*pErrno = errno;` |
|       - |  9912 | `#endif` |
|     ! 0 |  9913 | `	}` |
|      25 |  9914 | `	return rc;` |
|       - |  9915 | `#else` |
|       - |  9916 | `	/* No select() to call: a Windows build with no socket layer. */` |
|       - |  9917 | `	SXUNUSED(pSel);` |
|       - |  9918 | `	SXUNUSED(iSec);` |
|       - |  9919 | `	SXUNUSED(iUsec);` |
|       - |  9920 | `	SXUNUSED(bBlock);` |
|       - |  9921 | `	if( pErrno ){ *pErrno = 0; }` |
|       - |  9922 | `	return -1;` |
|       - |  9923 | `#endif` |
|       1 |  9924 | `}` |
|       - |  9925 | `/* Walk one of the three arguments, if it IS one. */` |
|     200 |  9926 | `static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|       - |  9927 | `	int iMode)` |
|       1 |  9928 | `{` |
|     201 |  9929 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|      89 |  9930 | `		return;` |
|       - |  9931 | `	}` |
|     113 |  9932 | `	pSel->iWhich = iWhich;` |
|     113 |  9933 | `	pSel->iMode = iMode;` |
|     113 |  9934 | `	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);` |
|     101 |  9935 | `}` |
|       - |  9936 | `/* Rebuild one argument from the entries that came back ready. php REPLACES the` |
|       - |  9937 | ` * array either way, so a set with nothing ready comes back empty. */` |
|      84 |  9938 | `static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|       - |  9939 | `	int iMode)` |
|       1 |  9940 | `{` |
|      85 |  9941 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|      51 |  9942 | `		return PH7_OK;` |
|       - |  9943 | `	}` |
|      35 |  9944 | `	pSel->pOut = ph7_context_new_array(pSel->pCtx);` |
|      35 |  9945 | `	if( pSel->pOut == 0 ){` |
|       - |  9946 | `		/* Leaving the caller's array alone would answer that every entry is` |
|       - |  9947 | `		 * ready, which is the one wrong answer this function must not give. */` |
|     ! 0 |  9948 | `		return PH7_ContextMemoryError(pSel->pCtx);` |
|       - |  9949 | `	}` |
|      35 |  9950 | `	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);` |
|      35 |  9951 | `	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);` |
|      35 |  9952 | `	pSel->pOut = 0;` |
|      35 |  9953 | `	return PH7_OK;` |
|      43 |  9954 | `}` |
|      46 |  9955 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  9956 | `{` |
|       - |  9957 | `	stream_select_ctx sSel;` |
|      47 |  9958 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|      47 |  9959 | `	int bBlock = 1,iErrno = 0,rc,i;` |
|      47 |  9960 | `	SyZero(&sSel,sizeof(sSel));` |
|      47 |  9961 | `	sSel.pCtx = pCtx;` |
|      47 |  9962 | `	sSel.iMaxFd = -1;` |
|       - |  9963 | `#ifdef STREAM_SELECT_OK` |
|     185 |  9964 | `	for( i = 0 ; i < 3 ; i++ ){` |
|    1243 |  9965 | `		FD_ZERO(&sSel.aSet[i]);` |
|      70 |  9966 | `	}` |
|       - |  9967 | `#endif` |
|      47 |  9968 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);` |
|      47 |  9969 | `	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);` |
|      47 |  9970 | `	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);` |
|      47 |  9971 | `	if( sSel.nSelectable < 1 ){` |
|       - |  9972 | `		/* php's own wording, and it carries no function name. It is the answer` |
|       - |  9973 | `		 * for three NULLs, for empty arrays, and for arrays holding nothing` |
|       - |  9974 | `		 * this engine can wait on — the caller asked to wait for nothing. */` |
|       7 |  9975 | `		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");` |
|       - |  9976 | `	}` |
|      41 |  9977 | `	if( sSel.bBadEntry ){` |
|       - |  9978 | `		/* Raised only once the arrays are known to hold something to wait on —` |
|       - |  9979 | `		 * the empty-arrays Error wins over it — and BEFORE the timeout is` |
|       - |  9980 | `		 * looked at, which is the order php's own pending-exception check` |
|       - |  9981 | `		 * produces. */` |
|      10 |  9982 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  9983 | `			"%s(): supplied %s is not a valid stream resource",` |
|       6 |  9984 | `			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");` |
|       - |  9985 | `	}` |
|      35 |  9986 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      33 |  9987 | `		iSec = ph7_value_to_int64(apArg[3]);` |
|      33 |  9988 | `		bBlock = 0;` |
|      33 |  9989 | `		if( iSec < 0 ){` |
|       4 |  9990 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  9991 | `				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",` |
|       1 |  9992 | `				ph7_function_name(pCtx));` |
|       - |  9993 | `		}` |
|      15 |  9994 | `	}` |
|      33 |  9995 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      15 |  9996 | `		iUsec = ph7_value_to_int64(apArg[4]);` |
|      15 |  9997 | `		if( bBlock ){` |
|       - |  9998 | `			/* php refuses the pair rather than guessing which one meant it: a` |
|       - |  9999 | `			 * NULL $seconds is "wait forever", and there is no such thing as` |
|       - | 10000 | `			 * waiting forever for five microseconds. */` |
|       3 | 10001 | `			if( iUsec != 0 ){` |
|       4 | 10002 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 10003 | `					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",` |
|       1 | 10004 | `					ph7_function_name(pCtx));` |
|     ! 0 | 10005 | `			}` |
|      13 | 10006 | `		}else if( iUsec < 0 ){` |
|       4 | 10007 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 10008 | `				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",` |
|       1 | 10009 | `				ph7_function_name(pCtx));` |
|       - | 10010 | `		}` |
|       5 | 10011 | `	}` |
|      29 | 10012 | `	if( iUsec > 999999 ){` |
|       - | 10013 | `		/* php carries the overflow into the seconds, because a tv_usec of a` |
|       - | 10014 | `		 * million or more is what Solaris and the BSDs refuse outright — so` |
|       - | 10015 | ``		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half`` |
|       - | 10016 | `		 * rather than failing. */` |
|       3 | 10017 | `		iSec += iUsec / 1000000;` |
|       3 | 10018 | `		iUsec %= 1000000;` |
|       1 | 10019 | `	}` |
|       - | 10020 | `	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer` |
|       - | 10021 | `	 * still holds bytes the script has not taken is ready NOW, whatever the OS` |
|       - | 10022 | `	 * would say about its descriptor — the device has nothing left to report.` |
|       - | 10023 | `	 * COUNTED first and stored second, because the count is what decides` |
|       - | 10024 | `	 * whether the arrays are rewritten from the buffers or from the wait. */` |
|      29 | 10025 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);` |
|      29 | 10026 | `	if( sSel.nReady > 0 ){` |
|       5 | 10027 | `		sSel.nReady = 0;` |
|       4 | 10028 | `		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK` |
|       - | 10029 | `		/* php answers only the readable ones then, and empties the other two. */` |
|       4 | 10030 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK` |
|       5 | 10031 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){` |
|     ! 0 | 10032 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 10033 | `		}` |
|       5 | 10034 | `		ph7_result_int(pCtx,sSel.nReady);` |
|       5 | 10035 | `		return PH7_OK;` |
|       - | 10036 | `	}` |
|      25 | 10037 | `	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);` |
|      25 | 10038 | `	if( rc < 0 ){` |
|       - | 10039 | `#if defined(__WINNT__) && defined(PH7_ENABLE_NET)` |
|     ! 0 | 10040 | `		const char *zErr = PH7_NetStrError(iErrno);` |
|       - | 10041 | `#else` |
|     ! 0 | 10042 | `		const char *zErr = VfsStrerror(iErrno);` |
|       - | 10043 | `#endif` |
|     ! 0 | 10044 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",` |
|     ! 0 | 10045 | `			iErrno,zErr,sSel.iMaxFd);` |
|     ! 0 | 10046 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10047 | `		return PH7_OK;` |
|       - | 10048 | `	}` |
|      24 | 10049 | `	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK` |
|      24 | 10050 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK` |
|      25 | 10051 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){` |
|     ! 0 | 10052 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 10053 | `	}` |
|       - | 10054 | `	/* The COUNT is select()'s own, not the entries kept: two array members can` |
|       - | 10055 | `	 * name one descriptor, and php answers what the OS said. */` |
|      25 | 10056 | `	ph7_result_int(pCtx,rc);` |
|      25 | 10057 | `	return PH7_OK;` |
|      24 | 10058 | `}` |
|       - | 10059 | `/*` |
|       - | 10060 | ` * bool\|int stream_socket_enable_crypto(resource $stream, bool $enable,` |
|       - | 10061 | ` *                                     ?int $crypto_method = null)` |
|       - | 10062 | ` *` |
|       - | 10063 | ` * Crypto on a socket that is already connected -- the second door to the same` |
|       - | 10064 | ` * handshake ssl:// runs inside its connect, and the one a protocol that` |
|       - | 10065 | ` * upgrades in place (SMTP's STARTTLS, IMAP's, a proxied CONNECT) needs. php` |
|       - | 10066 | ` * answers TRUE for a settled handshake, FALSE for one that failed, and 0 for` |
|       - | 10067 | ` * one that is not finished yet on a non-blocking handle; the blocking` |
|       - | 10068 | ` * handshake here settles or fails, so 0 is unreachable.` |
|       - | 10069 | ` *` |
|       - | 10070 | ` * Two refusals are the whole of its screening, and both are php's: a stream` |
|       - | 10071 | ` * with no socket under it "does not support SSL/crypto" -- a warning, false --` |
|       - | 10072 | ` * and enabling with no method named anywhere is a ValueError, because php has` |
|       - | 10073 | ` * no default to fall back on once the context carries none.` |
|       - | 10074 | ` */` |
|      20 | 10075 | `PH7_PRIVATE int PH7_builtin_stream_socket_enable_crypto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 10076 | `{` |
|       - | 10077 | `	io_private *pDev;` |
|      21 | 10078 | `	int rc = PH7_OK,bEnable;` |
|       - | 10079 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - | 10080 | `	sock_private *pSock;` |
|       - | 10081 | `	phl_stream_ctx *pCtxRes;` |
|      21 | 10082 | `	int iMethod = 0;` |
|       - | 10083 | `	char zErr[512],zHost[256];` |
|       - | 10084 | `#endif` |
|      21 | 10085 | `	if( nArg < 2 ){` |
|     ! 0 | 10086 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10087 | `		return PH7_OK;` |
|       - | 10088 | `	}` |
|      21 | 10089 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"$stream",&rc);` |
|      21 | 10090 | `	if( pDev == 0 ){` |
|     ! 0 | 10091 | `		return rc;` |
|       - | 10092 | `	}` |
|      21 | 10093 | `	bEnable = ph7_value_to_bool(apArg[1]) ? 1 : 0;` |
|       - | 10094 | `#if defined(PH7_ENABLE_OPENSSL)` |
|      19 | 10095 | `	pSock = (pDev->pStream == &sTCP_Stream && pDev->pHandle` |
|      16 | 10096 | `	      && ((sock_private *)pDev->pHandle)->sock != PH7_NET_INVALID_SOCKET)` |
|      26 | 10097 | `		? (sock_private *)pDev->pHandle : 0;` |
|      21 | 10098 | `	if( !bEnable ){` |
|       - | 10099 | `		/* The two halves are asymmetric, and measurably so. DISABLING on a` |
|       - | 10100 | `		 * device that is not a transport at all -- a file, php://memory, a` |
|       - | 10101 | `		 * userland wrapper -- warns like the enabling half does and then` |
|       - | 10102 | `		 * answers TRUE anyway: the request reaches a stream with no crypto` |
|       - | 10103 | `		 * option to answer it, and php reads the untouched success its own` |
|       - | 10104 | `		 * call left behind. Disabling on a real` |
|       - | 10105 | `		 * SOCKET is answered by the socket's own ops, and one carrying no` |
|       - | 10106 | `		 * session says false. So the return value reports what CHANGED, not` |
|       - | 10107 | `		 * whether the handle is now plain, and the two devices disagree about` |
|       - | 10108 | `		 * a handle that has no crypto either way. */` |
|       9 | 10109 | `		if( pSock == 0 ){` |
|       3 | 10110 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 10111 | `				"This stream does not support SSL/crypto");` |
|       3 | 10112 | `			ph7_result_bool(pCtx,1);` |
|       3 | 10113 | `			return PH7_OK;` |
|       - | 10114 | `		}` |
|       7 | 10115 | `		if( pSock->pSsl == 0 ){` |
|       5 | 10116 | `			ph7_result_bool(pCtx,0);` |
|       5 | 10117 | `			return PH7_OK;` |
|       - | 10118 | `		}` |
|       2 | 10119 | `		SockSslDrop(pSock);` |
|       - | 10120 | `		/* FALSE even though the session really was torn down: php's crypto op` |
|       - | 10121 | `		 * answers the shutdown by falling off the end of a function whose only` |
|       - | 10122 | `		 * other exit is the handshake's, so the value that reaches the script` |
|       - | 10123 | `		 * is the one that means "no handshake completed". Disabling therefore` |
|       - | 10124 | `		 * never reports success on a socket -- the handle is plain afterwards` |
|       - | 10125 | `		 * either way, and the return value says nothing about it. */` |
|       2 | 10126 | `		ph7_result_bool(pCtx,0);` |
|       2 | 10127 | `		return PH7_OK;` |
|       - | 10128 | `	}` |
|      13 | 10129 | `	if( pSock == 0 ){` |
|       - | 10130 | `		/* ENABLING is the half that screens the device: php asks the stream to` |
|       - | 10131 | `		 * set crypto up before it asks it to switch on, and a device with no` |
|       - | 10132 | `		 * transport ops is refused there by name. */` |
|       3 | 10133 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 10134 | `			"This stream does not support SSL/crypto");` |
|       3 | 10135 | `		ph7_result_bool(pCtx,0);` |
|       3 | 10136 | `		return PH7_OK;` |
|       - | 10137 | `	}` |
|      11 | 10138 | `	if( pSock->pSsl \|\| pSock->bSslSeen ){` |
|       - | 10139 | `		/* A handle that has already carried a session is refused, and stays` |
|       - | 10140 | `		 * refused after a disable: php frees nothing until the handle closes,` |
|       - | 10141 | `		 * so its SSL object is still there for the setup step to trip over. The` |
|       - | 10142 | `		 * WARNING is the blocking handle's alone -- on a non-blocking one php` |
|       - | 10143 | `		 * treats the same state as "the handshake this loop is driving is` |
|       - | 10144 | `		 * already up" and says nothing -- but the answer is FALSE either way,` |
|       - | 10145 | `		 * because no handshake completed inside this call. */` |
|       6 | 10146 | `		if( pDev->bNonBlock == 0 ){` |
|       4 | 10147 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 10148 | `				"SSL/TLS already set-up for this stream");` |
|       2 | 10149 | `		}` |
|       6 | 10150 | `		ph7_result_bool(pCtx,0);` |
|       6 | 10151 | `		return PH7_OK;` |
|       - | 10152 | `	}` |
|       5 | 10153 | `	pCtxRes = (phl_stream_ctx *)pDev->pCtxRes;` |
|       5 | 10154 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       2 | 10155 | `		iMethod = (int)ph7_value_to_int64(apArg[2]);` |
|       1 | 10156 | `	}else{` |
|       3 | 10157 | `		ph7_value *pOpt = pCtxRes ? PH7_StreamCtxOption(pCtxRes,"ssl","crypto_method") : 0;` |
|       3 | 10158 | `		if( pOpt ){` |
|     ! 0 | 10159 | `			iMethod = (int)ph7_value_to_int64(pOpt);` |
|     ! 0 | 10160 | `		}` |
|       - | 10161 | `	}` |
|       5 | 10162 | `	if( iMethod == 0 ){` |
|       4 | 10163 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 10164 | `			"%s(): Argument #3 ($crypto_method) must be specified when enabling encryption",` |
|       1 | 10165 | `			ph7_function_name(pCtx));` |
|       - | 10166 | `	}` |
|       - | 10167 | `	/* The name to verify the peer against, when the script named none: php uses` |
|       - | 10168 | `	 * the host half of the address the handle was OPENED with, which is why a` |
|       - | 10169 | `	 * connection made through fsockopen() verifies against the host it dialled` |
|       - | 10170 | `	 * and an ACCEPTED one (opened by no name at all) has nothing to check. */` |
|       2 | 10171 | `	zHost[0] = 0;` |
|       2 | 10172 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|       2 | 10173 | `		const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|       2 | 10174 | `		int nUri = (int)SyBlobLength(&pDev->sUri),iPort,nXport,nRest,bDgram,iXCrypto;` |
|       - | 10175 | `		const char *zXport,*zRest;` |
|       2 | 10176 | `		if( SockParseAddress(zUri,nUri,zHost,(int)sizeof(zHost),&iPort,&zXport,&nXport,` |
|       1 | 10177 | `				&zRest,&nRest,&bDgram,&iXCrypto) != SOCK_ADDR_OK ){` |
|     ! 0 | 10178 | `			zHost[0] = 0;` |
|     ! 0 | 10179 | `		}` |
|       1 | 10180 | `	}` |
|       2 | 10181 | `	if( SockSslHandshake(pCtx,pSock,iMethod,pCtxRes,zHost,zErr,(int)sizeof(zErr)) != PH7_OK ){` |
|       - | 10182 | `		/* One warning only. Unlike the ssl:// opener and the tls:// accept,` |
|       - | 10183 | `		 * which each add their own sentence for the door that could not be` |
|       - | 10184 | `		 * opened, this door reports nothing of its own: the crypto layer's text` |
|       - | 10185 | `		 * IS the answer, and false is the rest of it. */` |
|       2 | 10186 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zErr);` |
|       2 | 10187 | `		ph7_result_bool(pCtx,0);` |
|       2 | 10188 | `		return PH7_OK;` |
|       - | 10189 | `	}` |
|     ! 0 | 10190 | `	ph7_result_bool(pCtx,1);` |
|     ! 0 | 10191 | `	return PH7_OK;` |
|       - | 10192 | `#else` |
|       - | 10193 | `	SXUNUSED(bEnable);` |
|       - | 10194 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 10195 | `		"This stream does not support SSL/crypto");` |
|       - | 10196 | `	ph7_result_bool(pCtx,0);` |
|       - | 10197 | `	return PH7_OK;` |
|       - | 10198 | `#endif` |
|      11 | 10199 | `}` |
|       - | 10200 | `/*` |
|       - | 10201 | ` * array stream_get_transports(void)` |
|       - | 10202 | ` *` |
|       - | 10203 | ` * The transports a stream_socket_client()/fsockopen() address may name. php's` |
|       - | 10204 | ` * own list is what its build registered, so this is what THIS engine can open,` |
|       - | 10205 | ` * in php's own registration order: the ssl/tls/unix set is a recorded scope gap` |
|       - | 10206 | ` * (recorded), and answering for transports that are not there would tell a script a` |
|       - | 10207 | ` * connection will work when it cannot.` |
|       - | 10208 | ` */` |
|      14 | 10209 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 10210 | `{` |
|       - | 10211 | `	ph7_value *pArr,*pV;` |
|       7 | 10212 | `	SXUNUSED(nArg);` |
|       7 | 10213 | `	SXUNUSED(apArg);` |
|      17 | 10214 | `	pArr = ph7_context_new_array(pCtx);` |
|      17 | 10215 | `	pV = ph7_context_new_scalar(pCtx);` |
|      17 | 10216 | `	if( pArr == 0 \|\| pV == 0 ){` |
|     ! 0 | 10217 | `		ph7_result_null(pCtx);` |
|     ! 0 | 10218 | `		return PH7_OK;` |
|       - | 10219 | `	}` |
|       - | 10220 | `#ifdef PH7_ENABLE_NET` |
|       - | 10221 | `	{` |
|       - | 10222 | `		/* php's REGISTRATION order, not an alphabetical one: tcp and udp first,` |
|       - | 10223 | `		 * then the socket transports its build added, then ssl/tls. The` |
|       - | 10224 | `		 * unix:// and udg:// pair php lists between them is a recorded scope` |
|       - | 10225 | `		 * gap, and answering for transports that are not there would tell a` |
|       - | 10226 | `		 * script a connection will work when it cannot. */` |
|       - | 10227 | `		static const char * const azXport[] = {` |
|       - | 10228 | `			"tcp", "udp"` |
|       - | 10229 | `#if defined(PH7_ENABLE_OPENSSL)` |
|       - | 10230 | `			, "ssl", "tls", "tlsv1.0", "tlsv1.1", "tlsv1.2", "tlsv1.3"` |
|       - | 10231 | `#endif` |
|       - | 10232 | `		};` |
|       - | 10233 | `		int i;` |
|     129 | 10234 | `		for( i = 0 ; i < (int)(sizeof(azXport)/sizeof(azXport[0])) ; i++ ){` |
|     115 | 10235 | `			ph7_value_reset_string_cursor(pV);` |
|     115 | 10236 | `			ph7_value_string(pV,azXport[i],-1);` |
|     115 | 10237 | `			ph7_array_add_elem(pArr,0,pV);` |
|      59 | 10238 | `		}` |
|       - | 10239 | `	}` |
|       - | 10240 | `#endif` |
|      17 | 10241 | `	ph7_result_value(pCtx,pArr);` |
|      17 | 10242 | `	return PH7_OK;` |
|      10 | 10243 | `}` |
|       - | 10244 | `/*` |
|       - | 10245 | ` * bool stream_supports_lock(resource $stream)` |
|       - | 10246 | ` *` |
|       - | 10247 | ` * The question flock() answers with a warning if you get it wrong: only a` |
|       - | 10248 | ` * device with a real lock operation can be locked, so a memory buffer and a` |
|       - | 10249 | ` * data:// payload are false.` |
|       - | 10250 | ` */` |
|      14 | 10251 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 10252 | `{` |
|       - | 10253 | `	io_private *pDev;` |
|       - | 10254 | `	int rc;` |
|      15 | 10255 | `	if( nArg < 1 ){` |
|     ! 0 | 10256 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10257 | `		return PH7_OK;` |
|       - | 10258 | `	}` |
|      15 | 10259 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      15 | 10260 | `	if( pDev == 0 ){` |
|       3 | 10261 | `		return rc;` |
|       - | 10262 | `	}` |
|       - | 10263 | `	/* php locks at the DESCRIPTOR, so anything with one can be locked even` |
|       - | 10264 | `	 * when the device exposes no lock operation of its own (php://stdout, a` |
|       - | 10265 | `	 * pipe); a memory buffer and a data:// payload have neither and are the` |
|       - | 10266 | `	 * false answers. */` |
|      13 | 10267 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      23 | 10268 | `	ph7_result_bool(pCtx,pDev->bDir == 0` |
|      16 | 10269 | `		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)` |
|       7 | 10270 | `		    \|\| PH7_StreamPosixFd(pDev) >= 0));` |
|      13 | 10271 | `	return PH7_OK;` |
|       8 | 10272 | `}` |
|       - | 10273 | `/*` |
|       - | 10274 | ` * bool stream_is_local(resource\|string $stream)` |
|       - | 10275 | ` *` |
|       - | 10276 | ` * php answers from the WRAPPER, not from the path: a stream opened by a URL` |
|       - | 10277 | ` * wrapper is not local, one opened by no wrapper at all (a pipe) is not local` |
|       - | 10278 | ` * either, and everything else — including php:// and a path naming a scheme` |
|       - | 10279 | ` * nobody registered — is.` |
|       - | 10280 | ` */` |
|      28 | 10281 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 10282 | `{` |
|       - | 10283 | `	const ph7_io_stream *pStream;` |
|      29 | 10284 | `	if( nArg < 1 ){` |
|     ! 0 | 10285 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10286 | `		return PH7_OK;` |
|       - | 10287 | `	}` |
|      29 | 10288 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 10289 | `		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };` |
|       - | 10290 | `		int nLen,i;` |
|      21 | 10291 | `		const char *zPath = ph7_value_to_string(apArg[0],&nLen);` |
|      20 | 10292 | `		if( nLen > (int)sizeof("file://")-1` |
|      19 | 10293 | `		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0` |
|      11 | 10294 | `		 && zPath[sizeof("file://")-1] != '/'` |
|       4 | 10295 | `		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){` |
|       - | 10296 | ``			/* `file://host/path` names a REMOTE host, which php refuses rather`` |
|       - | 10297 | `			 * than reading as a local path — so the answer is not local. */` |
|       3 | 10298 | `			ph7_result_bool(pCtx,0);` |
|       3 | 10299 | `			return PH7_OK;` |
|       - | 10300 | `		}` |
|      83 | 10301 | `		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){` |
|      67 | 10302 | `			int nScheme = (int)SyStrlen(azUrlScheme[i]);` |
|      67 | 10303 | `			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){` |
|       - | 10304 | `				/* php registers these as URL wrappers whether or not this` |
|       - | 10305 | `				 * engine can OPEN them (http:// is a recorded gap), and` |
|       - | 10306 | `				 * "is this path local?" has to answer for the scheme rather` |
|       - | 10307 | `				 * than for what happens to be implemented — the unsafe` |
|       - | 10308 | `				 * direction is answering TRUE about a remote URL. */` |
|       3 | 10309 | `				ph7_result_bool(pCtx,0);` |
|       3 | 10310 | `				return PH7_OK;` |
|       - | 10311 | `			}` |
|      33 | 10312 | `		}` |
|      17 | 10313 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);` |
|       - | 10314 | `		/* An unregistered scheme has no wrapper to ask, and php answers TRUE` |
|       - | 10315 | `		 * for it — the path is taken at face value. */` |
|      17 | 10316 | `		ph7_result_bool(pCtx,pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream));` |
|      17 | 10317 | `		return PH7_OK;` |
|       - | 10318 | `	}` |
|       - | 10319 | `	{` |
|       - | 10320 | `		int rc;` |
|       9 | 10321 | `		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|       - | 10322 | `		const char *zWrapper,*zLabel;` |
|       9 | 10323 | `		if( pDev == 0 ){` |
|     ! 0 | 10324 | `			return rc;` |
|       - | 10325 | `		}` |
|       9 | 10326 | `		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|       - | 10327 | `		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */` |
|       9 | 10328 | `		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));` |
|       - | 10329 | `	}` |
|       9 | 10330 | `	return PH7_OK;` |
|      15 | 10331 | `}` |
|       - | 10332 | `/* PH7_ENABLE_NET */` |
|       - | 10333 | `/*` |
|       - | 10334 | ` * The open that fopen() is: the device lookup, the io_private, the mode` |
|       - | 10335 | ``  * translation, the handle and the meta-data record `stream_get_meta_data()` `` |
|       - | 10336 | ` * reports back. php's fopen() and its SplFileObject constructor both call` |
|       - | 10337 | ` * php_stream_open_wrapper_ex(), so both doors here share this body rather than` |
|       - | 10338 | ` * spelling the sequence twice.` |
|       - | 10339 | ` *` |
|       - | 10340 | ` * NOTHING is reported from in here. The two failures are handed back through` |
|       - | 10341 | ` * *piErr, because the two callers word them differently: fopen() warns, while` |
|       - | 10342 | ` * SplFileObject's constructor promotes the same warning to a RuntimeException` |
|       - | 10343 | ` * (php's zend_replace_error_handling). *pzErrUri is the name to report -- the` |
|       - | 10344 | ` * scheme-stripped remainder, which is what the warning has always printed.` |
|       - | 10345 | ` */` |
|    1978 | 10346 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|       - | 10347 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|       - | 10348 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri)` |
|       5 | 10349 | `{` |
|       - | 10350 | `	const ph7_io_stream *pStream;` |
|       - | 10351 | `	const char *zUri;` |
|       - | 10352 | `	ph7_value *pResource;` |
|       - | 10353 | `	io_private *pDev;` |
|       - | 10354 | `	int iLen,iOpenFlags;` |
|    1983 | 10355 | `	zUri = ph7_value_to_string(pPath,&iLen);` |
|    1983 | 10356 | `	*piErr = PH7_STREAM_OPEN_OK;` |
|    1983 | 10357 | `	*pzErrUri = zUri;` |
|       - | 10358 | `	/* Try to extract a stream */` |
|    1983 | 10359 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zUri,iLen);` |
|    1983 | 10360 | `	*pzErrUri = zUri;` |
|    1983 | 10361 | `	if( pStream == 0 ){` |
|     ! 0 | 10362 | `		*piErr = PH7_STREAM_OPEN_NODEVICE;` |
|     ! 0 | 10363 | `		return 0;` |
|       - | 10364 | `	}` |
|       - | 10365 | `	/* php's mode grammar belongs to the PLAIN-FILE wrapper and to nothing else:` |
|       - | 10366 | `	 * php://, data:// and a userland wrapper are handed whatever the caller` |
|       - | 10367 | ``	 * wrote and decide for themselves (`fopen('php://memory','zz')` opens`` |
|       - | 10368 | `	 * read-only rather than failing), so only the default device refuses. */` |
|    1983 | 10369 | `	if( StrModeToFlags(zMode,nMode,&iOpenFlags) != 0 ){` |
|      39 | 10370 | `		if( pStream == pCtx->pVm->pDefStream ){` |
|      39 | 10371 | `			*piErr = PH7_STREAM_OPEN_BADMODE;` |
|      39 | 10372 | `			return 0;` |
|       - | 10373 | `		}` |
|     ! 0 | 10374 | `		iOpenFlags = PH7_IO_OPEN_RDONLY;` |
|     ! 0 | 10375 | `	}` |
|       - | 10376 | `	/* Allocate a new IO private instance */` |
|    1945 | 10377 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    1945 | 10378 | `	if( pDev == 0 ){` |
|     ! 0 | 10379 | `		*piErr = PH7_STREAM_OPEN_NOMEM;` |
|     ! 0 | 10380 | `		return 0;` |
|       - | 10381 | `	}` |
|    1945 | 10382 | `	pResource = 0;` |
|    1945 | 10383 | `	if( pCtxArg ){` |
|      15 | 10384 | `		pResource = pCtxArg;` |
|    1939 | 10385 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|       - | 10386 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|       - | 10387 | `		 * access the underlying virtual machine.` |
|       - | 10388 | `		 */` |
|     622 | 10389 | `		pResource = pPath;` |
|     307 | 10390 | `	}` |
|       - | 10391 | `	/* Initialize the structure */` |
|    1945 | 10392 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|    1945 | 10393 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|       - | 10394 | `	/* The caller wrote this mode; a userland wrapper is handed it verbatim. */` |
|    1945 | 10395 | `	PH7_StreamArmOpenMode(pCtx->pVm,zMode,nMode);` |
|       - | 10396 | `	/* Try to get a handle */` |
|    2885 | 10397 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     940 | 10398 | `		bUseInclude,pResource,FALSE,0,ph7_function_name(pCtx));` |
|    1945 | 10399 | `	if( pDev->pHandle == 0 ){` |
|     113 | 10400 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     113 | 10401 | `		*piErr = PH7_STREAM_OPEN_FAILED;` |
|     113 | 10402 | `		return 0;` |
|       - | 10403 | `	}` |
|       - | 10404 | `	/* Remember what we were asked for: stream_get_meta_data() reports both.` |
|       - | 10405 | `	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder` |
|       - | 10406 | `	 * PH7_VmGetStreamDevice() advanced zUri past. */` |
|       - | 10407 | `	{` |
|       - | 10408 | `		int nUri;` |
|    1836 | 10409 | `		const char *zOrig = ph7_value_to_string(pPath,&nUri);` |
|    1836 | 10410 | `		const char *zMeta = zMode;` |
|    1836 | 10411 | `		int nMeta = nMode;` |
|    1831 | 10412 | `		if( is_php_stream(pStream)` |
|    1182 | 10413 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|       - | 10414 | `			/* php://output has one mode whatever it was asked for. */` |
|      12 | 10415 | `			zMeta = "wb";` |
|      12 | 10416 | `			nMeta = 2;` |
|    1828 | 10417 | `		}else if( is_php_stream(pStream)` |
|    1172 | 10418 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_INPUT ){` |
|       - | 10419 | `			/* php's input stream is built read-only whatever was asked for. */` |
|      13 | 10420 | `			zMeta = "rb";` |
|      13 | 10421 | `			nMeta = 2;` |
|    1816 | 10422 | `		}else if( is_php_stream(pStream)` |
|    1160 | 10423 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|       - | 10424 | `			/* php's memory streams do not keep the mode they were opened with:` |
|       - | 10425 | `			 * a buffer is readable and writable either way, so php reports the` |
|       - | 10426 | `			 * one it actually built. */` |
|     815 | 10427 | `			int i,bWrite = 0,bAppend = nMode > 0 && (zMode[0] == 'a' \|\| zMode[0] == 'A');` |
|    1531 | 10428 | `			for( i = 0 ; i < nMode ; i++ ){` |
|     985 | 10429 | `				if( zMode[i] == 'w' \|\| zMode[i] == 'W' \|\| zMode[i] == 'a'` |
|     776 | 10430 | `				 \|\| zMode[i] == 'A' \|\| zMode[i] == '+' ){` |
|     639 | 10431 | `					bWrite = 1;` |
|     317 | 10432 | `				}` |
|     496 | 10433 | `			}` |
|     546 | 10434 | `			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";` |
|     546 | 10435 | `			nMeta = (int)SyStrlen(zMeta);` |
|     269 | 10436 | `		}` |
|    1836 | 10437 | `		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);` |
|       - | 10438 | `	}` |
|    1836 | 10439 | `	return pDev;` |
|     964 | 10440 | `}` |
|    1848 | 10441 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 10442 | `{` |
|       - | 10443 | `	const char *zMode,*zErrUri;` |
|       - | 10444 | `	io_private *pDev;` |
|       - | 10445 | `	phl_stream_ctx *pCtxRes;` |
|    1853 | 10446 | `	int imLen,bThrew = 0,iErr;` |
|    1853 | 10447 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 10448 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 10449 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|     ! 0 | 10450 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10451 | `		return PH7_OK;` |
|       - | 10452 | `	}` |
|       - | 10453 | `	{` |
|    1853 | 10454 | `		int nPath = 0;` |
|    1853 | 10455 | `		ph7_value_to_string(apArg[0],&nPath);` |
|    1853 | 10456 | `		if( PH7_VfsEmptyPathRefused(pCtx,nPath) ){` |
|       2 | 10457 | `			return PH7_OK;` |
|       - | 10458 | `		}` |
|       - | 10459 | `	}` |
|       - | 10460 | `	/* Extract the desired access mode */` |
|    1851 | 10461 | `	if( nArg > 1 ){` |
|    1851 | 10462 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     898 | 10463 | `	}else{` |
|       - | 10464 | `		/* Set a default read-only mode */` |
|     ! 0 | 10465 | `		zMode = "r";` |
|     ! 0 | 10466 | `		imLen = (int)sizeof(char);` |
|       - | 10467 | `	}` |
|       - | 10468 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|       - | 10469 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|       - | 10470 | `	 * Resolved before the io_private chunk below, which a throw could not` |
|       - | 10471 | `	 * release. */` |
|    1851 | 10472 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    1851 | 10473 | `	if( bThrew ){` |
|       5 | 10474 | `		return PH7_OK;` |
|       - | 10475 | `	}` |
|    2745 | 10476 | `	pDev = PH7_StreamOpenPath(pCtx,apArg[0],zMode,imLen,` |
|     898 | 10477 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pCtxRes,` |
|     891 | 10478 | `		nArg > 3 ? apArg[3] : 0,&iErr,&zErrUri);` |
|    1847 | 10479 | `	if( pDev == 0 ){` |
|     145 | 10480 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|     ! 0 | 10481 | `			VfsThrowNoDeviceWarning(pCtx,zErrUri,FALSE);` |
|     145 | 10482 | `		}else if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|      55 | 10483 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|       - | 10484 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|      18 | 10485 | `				ph7_function_name(pCtx),zErrUri,imLen,zMode);` |
|     127 | 10486 | `		}else if( iErr == PH7_STREAM_OPEN_FAILED ){` |
|     109 | 10487 | `			VfsThrowOpenWarning(pCtx,zErrUri);` |
|      56 | 10488 | `		}else{` |
|     ! 0 | 10489 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|       - | 10490 | `		}` |
|     145 | 10491 | `		ph7_result_bool(pCtx,0);` |
|     145 | 10492 | `		return PH7_OK;` |
|       - | 10493 | `	}` |
|       - | 10494 | `	/* All done,return the io_private instance as a resource */` |
|    1706 | 10495 | `	ph7_result_resource(pCtx,pDev);` |
|    1706 | 10496 | `	return PH7_OK;` |
|     899 | 10497 | `}` |
|       - | 10498 | `/*` |
|       - | 10499 | ` * bool fclose(resource $handle)` |
|       - | 10500 | ` *  Closes an open file pointer` |
|       - | 10501 | ` * Parameters` |
|       - | 10502 | ` *  $handle` |
|       - | 10503 | ` *   The file pointer.` |
|       - | 10504 | ` * Return` |
|       - | 10505 | ` *  TRUE on success or FALSE on failure.` |
|       - | 10506 | ` */` |
|    3355 | 10507 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 10508 | `{` |
|       - | 10509 | `	const ph7_io_stream *pStream;` |
|       - | 10510 | `	io_private *pDev;` |
|       - | 10511 | `	ph7_vm *pVm;` |
|    3360 | 10512 | `	if( nArg < 1 ){` |
|       - | 10513 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 10514 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 10515 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10516 | `		return PH7_OK;` |
|       - | 10517 | `	}` |
|    3360 | 10518 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|       - | 10519 | `		char zGiven[64];` |
|      19 | 10520 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 10521 | `			"%s(): Argument #1 ($stream) must be of type resource, %s given",` |
|       6 | 10522 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 10523 | `	}` |
|       - | 10524 | `	/* Extract our private data */` |
|    3348 | 10525 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|       - | 10526 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError,` |
|       - | 10527 | `	 * and the name in it is the one that was CALLED -- gzclose() is this same` |
|       - | 10528 | `	 * body under another name and says gzclose(). */` |
|    3348 | 10529 | `	if( PH7_VfsResourceIsClosed(pDev) ){` |
|      14 | 10530 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 10531 | `			"%s(): Argument #1 ($stream) must be an open stream resource",` |
|       4 | 10532 | `			ph7_function_name(pCtx));` |
|       - | 10533 | `	}` |
|       - | 10534 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    3340 | 10535 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|       - | 10536 | `		/*Expecting an IO handle */` |
|     ! 0 | 10537 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|     ! 0 | 10538 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10539 | `		return PH7_OK;` |
|       - | 10540 | `	}` |
|       - | 10541 | `	/* Point to the target IO stream device */` |
|    3340 | 10542 | `	pStream = pDev->pStream;` |
|    3340 | 10543 | `	if( pStream == 0 ){` |
|       - | 10544 | `		/* Nothing to close. php's fclose() has no diagnostic for it. */` |
|     ! 0 | 10545 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10546 | `		return PH7_OK;` |
|       - | 10547 | `	}` |
|       - | 10548 | `	/* Point to the VM that own this context */` |
|    3340 | 10549 | `	pVm = pCtx->pVm;` |
|       - | 10550 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    3340 | 10551 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|       - | 10552 | `		/* The WRITE chain gets its closing call while the device is still open:` |
|       - | 10553 | `		 * a filter that buffers has nowhere else to put its tail, and php's own` |
|       - | 10554 | `		 * close flushes before it closes. */` |
|    3340 | 10555 | `		PH7_StreamFilterReleaseChains(pDev);` |
|       - | 10556 | `		/* Perform the requested operation */` |
|    3340 | 10557 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|       - | 10558 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    3340 | 10559 | `		MarkIOPrivateClosed(pDev);` |
|    1664 | 10560 | `	}` |
|       - | 10561 | `	/* Return TRUE */` |
|    3340 | 10562 | `	ph7_result_bool(pCtx,1);` |
|    3340 | 10563 | `	return PH7_OK;` |
|    1679 | 10564 | `}` |
|       - | 10565 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|       - | 10566 | `/*` |
|       - | 10567 | ` * MD5/SHA1 digest consumer.` |
|       - | 10568 | ` */` |
|     232 | 10569 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|       3 | 10570 | `{` |
|       - | 10571 | `	/* Append hex chunk verbatim */` |
|     235 | 10572 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     235 | 10573 | `	return SXRET_OK;` |
|       3 | 10574 | `}` |
|       - | 10575 | `/*` |
|       - | 10576 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|       - | 10577 | ` *  Calculates the md5 hash of a given file.` |
|       - | 10578 | ` * Parameters` |
|       - | 10579 | ` *  $uri` |
|       - | 10580 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|       - | 10581 | ` *  $raw_output` |
|       - | 10582 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|       - | 10583 | ` * Return` |
|       - | 10584 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|       - | 10585 | ` */` |
|      16 | 10586 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 10587 | `{` |
|       - | 10588 | `	const ph7_io_stream *pStream;` |
|       - | 10589 | `	unsigned char zDigest[16];` |
|      19 | 10590 | `	int raw_output  = FALSE;` |
|       - | 10591 | `	const char *zFile;` |
|       - | 10592 | `	MD5Context sCtx;` |
|       - | 10593 | `	char zBuf[8192];` |
|       - | 10594 | `	void *pHandle;` |
|       - | 10595 | `	ph7_int64 n;` |
|       - | 10596 | `	int nLen;` |
|      19 | 10597 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 10598 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 10599 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 10600 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10601 | `		return PH7_OK;` |
|       - | 10602 | `	}` |
|       - | 10603 | `	/* Extract the file path */` |
|      19 | 10604 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      19 | 10605 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 10606 | `		return PH7_OK;` |
|       - | 10607 | `	}` |
|       - | 10608 | `	/* Point to the target IO stream device */` |
|      17 | 10609 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      17 | 10610 | `	if( pStream == 0 ){` |
|     ! 0 | 10611 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 10612 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10613 | `		return PH7_OK;` |
|       - | 10614 | `	}` |
|      17 | 10615 | `	if( nArg > 1 ){` |
|     ! 0 | 10616 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     ! 0 | 10617 | `	}` |
|       - | 10618 | `	/* Try to open the file in read-only mode */` |
|      17 | 10619 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      17 | 10620 | `	if( pHandle == 0 ){` |
|       3 | 10621 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 | 10622 | `		ph7_result_bool(pCtx,0);` |
|       3 | 10623 | `		return PH7_OK;` |
|       - | 10624 | `	}` |
|       - | 10625 | `	/* Init the MD5 context */` |
|      15 | 10626 | `	MD5Init(&sCtx);` |
|       - | 10627 | `	/* Perform the requested operation */` |
|      10 | 10628 | `	for(;;){` |
|      23 | 10629 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      23 | 10630 | `		if( n < 1 ){` |
|       - | 10631 | `			/* EOF or IO error,break immediately */` |
|      15 | 10632 | `			break;` |
|       - | 10633 | `		}` |
|      10 | 10634 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|       2 | 10635 | `	}` |
|       - | 10636 | `	/* Close the stream */` |
|      15 | 10637 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 10638 | `	/* Extract the digest */` |
|      15 | 10639 | `	MD5Final(zDigest,&sCtx);` |
|      15 | 10640 | `	if( raw_output ){` |
|       - | 10641 | `		/* Output raw digest */` |
|     ! 0 | 10642 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|     ! 0 | 10643 | `	}else{` |
|       - | 10644 | `		/* Perform a binary to hex conversion */` |
|      15 | 10645 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|       - | 10646 | `	}` |
|      15 | 10647 | `	return PH7_OK;` |
|      11 | 10648 | `}` |
|       - | 10649 | `/*` |
|       - | 10650 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|       - | 10651 | ` *  Calculates the SHA1 hash of a given file.` |
|       - | 10652 | ` * Parameters` |
|       - | 10653 | ` *  $uri` |
|       - | 10654 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|       - | 10655 | ` *  $raw_output` |
|       - | 10656 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|       - | 10657 | ` * Return` |
|       - | 10658 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|       - | 10659 | ` */` |
|       4 | 10660 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 10661 | `{` |
|       - | 10662 | `	const ph7_io_stream *pStream;` |
|       - | 10663 | `	unsigned char zDigest[20];` |
|       5 | 10664 | `	int raw_output  = FALSE;` |
|       - | 10665 | `	const char *zFile;` |
|       - | 10666 | `	SHA1Context sCtx;` |
|       - | 10667 | `	char zBuf[8192];` |
|       - | 10668 | `	void *pHandle;` |
|       - | 10669 | `	ph7_int64 n;` |
|       - | 10670 | `	int nLen;` |
|       5 | 10671 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 10672 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 10673 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 10674 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10675 | `		return PH7_OK;` |
|       - | 10676 | `	}` |
|       - | 10677 | `	/* Extract the file path */` |
|       5 | 10678 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|       5 | 10679 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       2 | 10680 | `		return PH7_OK;` |
|       - | 10681 | `	}` |
|       - | 10682 | `	/* Point to the target IO stream device */` |
|       3 | 10683 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|       3 | 10684 | `	if( pStream == 0 ){` |
|     ! 0 | 10685 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 10686 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10687 | `		return PH7_OK;` |
|       - | 10688 | `	}` |
|       3 | 10689 | `	if( nArg > 1 ){` |
|     ! 0 | 10690 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|     ! 0 | 10691 | `	}` |
|       - | 10692 | `	/* Try to open the file in read-only mode */` |
|       3 | 10693 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|       3 | 10694 | `	if( pHandle == 0 ){` |
|     ! 0 | 10695 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 10696 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10697 | `		return PH7_OK;` |
|       - | 10698 | `	}` |
|       - | 10699 | `	/* Init the SHA1 context */` |
|       3 | 10700 | `	SHA1Init(&sCtx);` |
|       - | 10701 | `	/* Perform the requested operation */` |
|       2 | 10702 | `	for(;;){` |
|       5 | 10703 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|       5 | 10704 | `		if( n < 1 ){` |
|       - | 10705 | `			/* EOF or IO error,break immediately */` |
|       3 | 10706 | `			break;` |
|       - | 10707 | `		}` |
|       3 | 10708 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|       1 | 10709 | `	}` |
|       - | 10710 | `	/* Close the stream */` |
|       3 | 10711 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 10712 | `	/* Extract the digest */` |
|       3 | 10713 | `	SHA1Final(&sCtx,zDigest);` |
|       3 | 10714 | `	if( raw_output ){` |
|       - | 10715 | `		/* Output raw digest */` |
|     ! 0 | 10716 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|     ! 0 | 10717 | `	}else{` |
|       - | 10718 | `		/* Perform a binary to hex conversion */` |
|       3 | 10719 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|       - | 10720 | `	}` |
|       3 | 10721 | `	return PH7_OK;` |
|       3 | 10722 | `}` |
|       - | 10723 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 10724 | `/*` |
|       - | 10725 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|       - | 10726 | ` *  Parse a configuration file.` |
|       - | 10727 | ` * Parameters` |
|       - | 10728 | ` * $filename` |
|       - | 10729 | ` *  The filename of the ini file being parsed.` |
|       - | 10730 | ` * $process_sections` |
|       - | 10731 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|       - | 10732 | ` *  with the section names and settings included.` |
|       - | 10733 | ` *  The default for process_sections is FALSE.` |
|       - | 10734 | ` * $scanner_mode` |
|       - | 10735 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|       - | 10736 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|       - | 10737 | ` * Return` |
|       - | 10738 | ` *  The settings are returned as an associative array on success.` |
|       - | 10739 | ` *  Otherwise is returned.` |
|       - | 10740 | ` */` |
|      12 | 10741 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 10742 | `{` |
|       - | 10743 | `	const ph7_io_stream *pStream;` |
|       - | 10744 | `	const char *zFile;` |
|       - | 10745 | `	SyBlob sContents;` |
|       - | 10746 | `	void *pHandle;` |
|       - | 10747 | `	int nLen;` |
|      14 | 10748 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      14 | 10749 | `	sxi32 rc = PH7_OK;` |
|      14 | 10750 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|       - | 10751 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 10752 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|     ! 0 | 10753 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10754 | `		return PH7_OK;` |
|       - | 10755 | `	}` |
|      14 | 10756 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|       7 | 10757 | `		iMode = ph7_value_to_int(apArg[2]);` |
|       6 | 10758 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|       6 | 10759 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|       - | 10760 | `			/* php screens the mode BEFORE touching the file */` |
|       - | 10761 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|       3 | 10762 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|       3 | 10763 | `			ph7_result_bool(pCtx,0);` |
|       3 | 10764 | `			return PH7_OK;` |
|       - | 10765 | `		}` |
|       2 | 10766 | `	}` |
|       - | 10767 | `	/* Extract the file path */` |
|      12 | 10768 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      12 | 10769 | `	if( nLen < 1 ){` |
|       - | 10770 | `		/* One of php's three doors that names its own argument instead of` |
|       - | 10771 | ``		 * raising the stream layer's `Path must not be empty`. */`` |
|       2 | 10772 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 10773 | `			"parse_ini_file(): Argument #1 ($filename) must not be empty");` |
|       - | 10774 | `	}` |
|       - | 10775 | `	/* Point to the target IO stream device */` |
|      10 | 10776 | `	pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zFile,nLen);` |
|      10 | 10777 | `	if( pStream == 0 ){` |
|     ! 0 | 10778 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 10779 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10780 | `		return PH7_OK;` |
|       - | 10781 | `	}` |
|       - | 10782 | `	/* Try to open the file in read-only mode */` |
|      10 | 10783 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      10 | 10784 | `	if( pHandle == 0 ){` |
|     ! 0 | 10785 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     ! 0 | 10786 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10787 | `		return PH7_OK;` |
|       - | 10788 | `	}` |
|      10 | 10789 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|       - | 10790 | `	/* Read the whole file */` |
|      10 | 10791 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      10 | 10792 | `	if( SyBlobLength(&sContents) < 1 ){` |
|       - | 10793 | `		/* Empty buffer,return FALSE */` |
|     ! 0 | 10794 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 10795 | `	}else{` |
|       - | 10796 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      17 | 10797 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|       7 | 10798 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode,zFile);` |
|       - | 10799 | `	}` |
|       - | 10800 | `	/* Close the stream */` |
|      10 | 10801 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|       - | 10802 | `	/* Release the working buffer */` |
|      10 | 10803 | `	SyBlobRelease(&sContents);` |
|       - | 10804 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      10 | 10805 | `	return rc;` |
|       8 | 10806 | `}` |
|       - | 10807 | `/* ZIP archive processing moved to vfs_zip.c */` |
|       - | 10808 | `#else /* PH7_DISABLE_DISK_IO */` |
|       - | 10809 | `/*` |
|       - | 10810 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|       - | 10811 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|       - | 10812 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|       - | 10813 | ` * to any non-VFS resource).` |
|       - | 10814 | ` */` |
|       - | 10815 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|       - | 10816 | `{` |
|       - | 10817 | `	SXUNUSED(pResource);` |
|       - | 10818 | `	return "Unknown";` |
|       - | 10819 | `}` |
|       - | 10820 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|       - | 10821 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|       - | 10822 | `{` |
|       - | 10823 | `	SXUNUSED(pResource);` |
|       - | 10824 | `	return 0;` |
|       - | 10825 | `}` |
|       - | 10826 | `/* No streams means no stream contexts either, but PH7_VmReset still calls this. */` |
|       - | 10827 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|       - | 10828 | `{` |
|       - | 10829 | `	SXUNUSED(pVm);` |
|       - | 10830 | `}` |
|       - | 10831 | `/* Same for the filter registry. */` |
|       - | 10832 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|       - | 10833 | `{` |
|       - | 10834 | `	SXUNUSED(pVm);` |
|       - | 10835 | `}` |
|       - | 10836 | `/* No stream handle exists to be counted, but the three value doors call these` |
|       - | 10837 | ` * unconditionally -- they are on the hot path and cannot afford a build test. */` |
|       - | 10838 | `PH7_PRIVATE int PH7_StreamValueRef(void *pResource)` |
|       - | 10839 | `{` |
|       - | 10840 | `	SXUNUSED(pResource);` |
|       - | 10841 | `	return 0;` |
|       - | 10842 | `}` |
|       - | 10843 | `PH7_PRIVATE void PH7_StreamValueUnref(void *pResource)` |
|       - | 10844 | `{` |
|       - | 10845 | `	SXUNUSED(pResource);` |
|       - | 10846 | `}` |
|       - | 10847 | `#endif /* PH7_DISABLE_DISK_IO */` |
|       - | 10848 |  |
