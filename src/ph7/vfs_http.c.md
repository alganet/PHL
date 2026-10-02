# src/ph7/vfs_http.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1036/1227 lines (84.43%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - |    7 | `#include <openssl/ssl.h>` |
|     - |    8 | `#endif` |
|     - |    9 | `/*` |
|     - |   10 | ` * The http:// stream wrapper -- php's one built-in PROTOCOL wrapper, and the` |
|     - |   11 | `` * device behind `file_get_contents('http://...')`, `fopen('http://...')`,`` |
|     - |   12 | `` * `get_headers()` and every reader that takes a URL.`` |
|     - |   13 | ` *` |
|     - |   14 | ` * It is a WRAPPER rather than a transport: tcp:// hands a script the bytes of a` |
|     - |   15 | ` * socket, and this speaks a request over one and hands back the RESPONSE BODY,` |
|     - |   16 | ` * with everything else about the exchange -- the status line, the response` |
|     - |   17 | ` * headers, the redirect chain it walked -- published beside the handle as` |
|     - |   18 | `` * `$http_response_header` / `stream_get_meta_data()['wrapper_data']`.`` |
|     - |   19 | ` *` |
|     - |   20 | ` * Four contracts are derived from php 8.5 rather than from the RFCs, because` |
|     - |   21 | ` * php's wrapper does not implement the RFCs:` |
|     - |   22 | ` *` |
|     - |   23 | `` *   1. THE REQUEST'S HEADER ORDER is fixed, and the `http` context options and`` |
|     - |   24 | ` *      two ini directives feed named slots inside it. A user header wins over` |
|     - |   25 | `` *      the slot it names -- a `Host:` of one's own suppresses php's, a`` |
|     - |   26 | ``  *      `Connection:` suppresses `Connection: close`, an `Authorization:` `` |
|     - |   27 | ` *      suppresses the one the URL's userinfo would have produced.` |
|     - |   28 | `` *   2. THE STATUS LINE IS `atoi(line + 9)`. php checks no prefix at all, so`` |
|     - |   29 | `` *      `HTTP/2.0 200 OK` is a 200 and `HTTP/2 200 OK` is a 0 (its digits sit at`` |
|     - |   30 | ` *      the wrong offset), and any line under ten bytes has no code. Anything` |
|     - |   31 | `` *      outside 200..399 is `HTTP request failed! <the raw line>` unless`` |
|     - |   32 | `` *      `ignore_errors` says otherwise; a 1xx is DISCARDED, headers and all, and`` |
|     - |   33 | ` *      the next response read in its place.` |
|     - |   34 | `` *   3. THE BODY ENDS WHERE THE SOCKET DOES. `Content-Length` is announced and`` |
|     - |   35 | ` *      never enforced -- php hands back the short body a lying header promised` |
|     - |   36 | ` *      100 bytes of, and the long one it promised 2 -- and the single framing` |
|     - |   37 | `` *      php DOES apply is `Transfer-Encoding: chunked`, whose header line is then`` |
|     - |   38 | ` *      not part of the response headers at all.` |
|     - |   39 | `` *   4. A RELATIVE `Location:` IS RESOLVED AGAINST THE PATH, not against the`` |
|     - |   40 | `` *      directory: php keeps everything up to and including the last `/` and then`` |
|     - |   41 | `` *      joins with ANOTHER `/`, so a redirect from `/a/b` to `rel` is `/a//rel`.`` |
|     - |   42 | `` *      The exceptions are a path that is just `/` (or empty), where the join`` |
|     - |   43 | ` *      is direct, and a Location of at most ONE byte, which is not joined at` |
|     - |   44 | ` *      all but put under the root.` |
|     - |   45 | ` *` |
|     - |   46 | ` * https:// is this same wrapper with the ssl:// transport under it, which is` |
|     - |   47 | ` * exactly what php's is: one device per SCHEME, because the name a URL was` |
|     - |   48 | ` * found under is the only thing that says whether TLS is spoken, what port it` |
|     - |   49 | `` * defaults to, and which scheme a relative `Location:` is resolved against.`` |
|     - |   50 | ` */` |
|     - |   51 | `/*` |
|     - |   52 | ` * The response headers of the LAST http exchange, kept on the VM because two` |
|     - |   53 | `` * consumers outlive the handle: php writes `$http_response_header` into the`` |
|     - |   54 | ` * scope that called the opener (whether the open SUCCEEDED or not -- a 404 is a` |
|     - |   55 | ` * failed open with a full set of headers), and php 8.4's` |
|     - |   56 | ` * http_get_last_response_headers() answers them until something clears it.` |
|     - |   57 | ` */` |
|   260 |   58 | `PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines)` |
|   ! 0 |   59 | `{` |
|   260 |   60 | `	SyBlobReset(&pVm->sHttpRespHdrs);` |
|   260 |   61 | `	if( pLines && SyBlobLength(pLines) > 0 ){` |
|   258 |   62 | `		SyBlobAppend(&pVm->sHttpRespHdrs,SyBlobData(pLines),SyBlobLength(pLines));` |
|   129 |   63 | `	}` |
|   260 |   64 | `	pVm->bHttpRespHdrs = 1;` |
|   260 |   65 | `	pVm->bHttpRespFresh = 1;` |
|   260 |   66 | `}` |
|     - |   67 | `/*` |
|     - |   68 | ` * The stored lines as php's array of strings, or 0 when nothing has been` |
|     - |   69 | ` * recorded at all. One entry per line: an exchange that read nothing answers the` |
|     - |   70 | ` * EMPTY array, and one that read a single blank line answers one empty entry --` |
|     - |   71 | ` * two states php tells apart and a '\n'-separated store only can because every` |
|     - |   72 | ` * line carries its own terminator.` |
|     - |   73 | ` */` |
|   272 |   74 | `PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines)` |
|     1 |   75 | `{` |
|     - |   76 | `	ph7_value *pArr,*pLine;` |
|     - |   77 | `	const char *zIn,*zEnd,*zCur;` |
|   273 |   78 | `	if( pLines == 0 \|\| !pVm->bHttpRespHdrs ){` |
|     9 |   79 | `		return 0;` |
|     - |   80 | `	}` |
|   264 |   81 | `	pArr = ph7_new_array(pVm);` |
|   264 |   82 | `	pLine = ph7_new_scalar(pVm);` |
|   264 |   83 | `	if( pArr == 0 \|\| pLine == 0 ){` |
|   ! 0 |   84 | `		if( pArr ){ ph7_release_value(pVm,pArr); }` |
|   ! 0 |   85 | `		if( pLine ){ ph7_release_value(pVm,pLine); }` |
|   ! 0 |   86 | `		return 0;` |
|     - |   87 | `	}` |
|     - |   88 | `	/* Every recorded line carries a trailing '\n', which is what tells ONE` |
|     - |   89 | `	 * empty line (php's answer for a connection that ended where a status line` |
|     - |   90 | `	 * was due) from NO lines at all. */` |
|   264 |   91 | `	zIn = (const char *)SyBlobData(pLines);` |
|   264 |   92 | `	zEnd = &zIn[SyBlobLength(pLines)];` |
|  1392 |   93 | `	while( zIn < zEnd ){` |
|  1128 |   94 | `		zCur = zIn;` |
| 20458 |   95 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
| 19330 |   96 | `			zCur++;` |
|   ! 0 |   97 | `		}` |
|  1128 |   98 | `		ph7_value_string(pLine,zIn,(int)(zCur - zIn));` |
|  1128 |   99 | `		ph7_array_add_elem(pArr,0,pLine);` |
|  1128 |  100 | `		ph7_value_reset_string_cursor(pLine);` |
|  1128 |  101 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 |  102 | `	}` |
|   264 |  103 | `	ph7_release_value(pVm,pLine);` |
|   264 |  104 | `	return pArr;` |
|   137 |  105 | `}` |
|     - |  106 | `/*` |
|     - |  107 | `` * `$http_response_header`, written into the frame that called the opener. php`` |
|     - |  108 | ` * writes it from the stream layer, so every door -- file_get_contents(), fopen(),` |
|     - |  109 | ` * file(), readfile(), copy(), get_headers() -- has it, and an open that never` |
|     - |  110 | ` * reached a response leaves the caller's previous value alone.` |
|     - |  111 | ` */` |
| 45829 |  112 | `PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm)` |
|     5 |  113 | `{` |
|     - |  114 | `	static const char zVar[] = "http_response_header";` |
|     - |  115 | `	ph7_value *pArr,*pSlot;` |
|     - |  116 | `	SyString sName;` |
| 45834 |  117 | `	if( !pVm->bHttpRespFresh ){` |
| 45574 |  118 | `		return;` |
|     - |  119 | `	}` |
|   260 |  120 | `	pVm->bHttpRespFresh = 0;` |
|   260 |  121 | `	pArr = PH7_HttpHeaderArray(pVm,&pVm->sHttpRespHdrs);` |
|   260 |  122 | `	if( pArr == 0 ){` |
|   ! 0 |  123 | `		return;` |
|     - |  124 | `	}` |
|   260 |  125 | `	SyStringInitFromBuf(&sName,zVar,sizeof(zVar)-1);` |
|   260 |  126 | `	pSlot = VmExtractMemObj(pVm,&sName,TRUE,TRUE);` |
|   260 |  127 | `	if( pSlot ){` |
|   260 |  128 | `		PH7_MemObjStore(pArr,pSlot);` |
|   130 |  129 | `	}` |
|   260 |  130 | `	ph7_release_value(pVm,pArr);` |
| 22853 |  131 | `}` |
|     - |  132 | `/* http_clear_last_response_headers(): php drops the store, and the getter then` |
|     - |  133 | ` * answers NULL rather than an empty array. */` |
|   294 |  134 | `PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm)` |
|     1 |  135 | `{` |
|   295 |  136 | `	SyBlobReset(&pVm->sHttpRespHdrs);` |
|   295 |  137 | `	pVm->bHttpRespHdrs = 0;` |
|   295 |  138 | `	pVm->bHttpRespFresh = 0;` |
|   295 |  139 | `}` |
|     - |  140 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|     - |  141 | `/*` |
|     - |  142 | ` * An open http:// handle.` |
|     - |  143 | ` *` |
|     - |  144 | ` * The socket stays open for the whole life of the handle -- php's body IS the` |
|     - |  145 | ` * rest of the connection -- and sRaw holds whatever came off it with the header` |
|     - |  146 | ` * block, since a single recv() reads past the blank line more often than not.` |
|     - |  147 | ` */` |
|     - |  148 | `typedef struct http_private http_private;` |
|     - |  149 | `struct http_private` |
|     - |  150 | `{` |
|     - |  151 | `	ph7_vm *pVm;` |
|     - |  152 | `	phl_stream_ctx *pCtx; /* the context this exchange runs under, for its` |
|     - |  153 | ``	                       * `notification` callback; owned by the VM, so it`` |
|     - |  154 | `	                       * outlives the handle */` |
|     - |  155 | `	sxu8 bTls;            /* this hop is speaking TLS (an https:// URL) */` |
|     - |  156 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - |  157 | `	void *pSsl;           /* the negotiated session, when it is */` |
|     - |  158 | `	void *pSslCtx;        /* and the context it was built from */` |
|     - |  159 | `#endif` |
|     - |  160 | `	sxu8 bBody;           /* the headers are done: reads now carry the BODY */` |
|     - |  161 | `	sxu8 bNoFill;         /* the DRAIN below: decode what is in hand, read nothing */` |
|     - |  162 | ``	sxi64 iFileSize;      /* the `Content-Length` this response announced, or 0 */`` |
|     - |  163 | `	ph7_socket sock;      /* the connection the response is still arriving on */` |
|     - |  164 | `	SyBlob sRaw;          /* bytes read off the socket and not yet consumed */` |
|     - |  165 | `	sxu32 nRawOfft;       /* read cursor inside sRaw */` |
|     - |  166 | `	SyBlob sOut;          /* DECHUNKED bytes waiting for the reader (chunked only) */` |
|     - |  167 | `	sxu32 nOutOfft;       /* read cursor inside sOut */` |
|     - |  168 | `	ph7_int64 iPos;       /* bytes handed to the script: php's ftell() for this stream */` |
|     - |  169 | `	sxu8 bEof;            /* the peer closed and sRaw is drained */` |
|     - |  170 | `	sxu8 bChunked;        /* the body is chunk-framed */` |
|     - |  171 | `	sxu8 bChunkDone;      /* the zero-length chunk has been read */` |
|     - |  172 | `	sxi64 iChunkLeft;     /* bytes still owed by the chunk being read (-1: read a size line) */` |
|     - |  173 | `	SyBlob sHdrs;         /* the response header LINES, '\n'-separated */` |
|     - |  174 | `};` |
|     - |  175 | `/* php reads a socket in blocks; 8K is the size every other reader here uses. */` |
|     - |  176 | `#define HTTP_CHUNK_READ 8192` |
|     - |  177 | `/*` |
|     - |  178 | ` * The two byte ops of the exchange. Under https:// the RECORD layer is the` |
|     - |  179 | ` * connection -- libssl owns the descriptor and the socket calls would read the` |
|     - |  180 | ` * ciphertext -- so every read and write below goes through the session when` |
|     - |  181 | ` * there is one. There are exactly two such sites, which is why the exchange` |
|     - |  182 | ` * itself is unchanged by TLS.` |
|     - |  183 | ` */` |
|   510 |  184 | `static int HttpRecv(http_private *pH,char *zBuf,int nBuf)` |
|   ! 0 |  185 | `{` |
|     - |  186 | `#ifdef PH7_ENABLE_OPENSSL` |
|   510 |  187 | `	if( pH->pSsl ){` |
|    46 |  188 | `		int n = SSL_read((SSL *)pH->pSsl,zBuf,nBuf);` |
|     - |  189 | `		/* Anything that is not bytes ends this exchange: the wrapper reads a` |
|     - |  190 | `		 * response to its own end, and a session that cannot answer has no` |
|     - |  191 | `		 * more of one -- the same conclusion the socket reader draws from a` |
|     - |  192 | `		 * peer that closed. */` |
|    46 |  193 | `		return n > 0 ? n : 0;` |
|     - |  194 | `	}` |
|     - |  195 | `#endif` |
|   464 |  196 | `	return PH7_NetRecv(pH->sock,zBuf,nBuf,0);` |
|   255 |  197 | `}` |
|   316 |  198 | `static int HttpSendAll(http_private *pH,const void *pBuf,int nLen)` |
|   ! 0 |  199 | `{` |
|     - |  200 | `#ifdef PH7_ENABLE_OPENSSL` |
|   316 |  201 | `	if( pH->pSsl ){` |
|    24 |  202 | `		int nSent = 0;` |
|    48 |  203 | `		while( nSent < nLen ){` |
|    24 |  204 | `			int n = SSL_write((SSL *)pH->pSsl,&((const char *)pBuf)[nSent],nLen - nSent);` |
|    24 |  205 | `			if( n < 1 ){` |
|   ! 0 |  206 | `				return -1;` |
|     - |  207 | `			}` |
|    24 |  208 | `			nSent += n;` |
|   ! 0 |  209 | `		}` |
|    24 |  210 | `		return PH7_OK;` |
|     - |  211 | `	}` |
|     - |  212 | `#endif` |
|   292 |  213 | `	return PH7_NetSendAll(pH->sock,pBuf,nLen);` |
|   158 |  214 | `}` |
|     - |  215 | `/* Close the connection, session first: the socket the session is reading` |
|     - |  216 | ` * through must outlive it. */` |
|   314 |  217 | `static void HttpDisconnect(http_private *pH)` |
|     1 |  218 | `{` |
|     - |  219 | `#ifdef PH7_ENABLE_OPENSSL` |
|   315 |  220 | `	PH7_SslDropSession(&pH->pSsl,&pH->pSslCtx);` |
|     - |  221 | `#endif` |
|   315 |  222 | `	if( pH->sock != PH7_NET_INVALID_SOCKET ){` |
|   302 |  223 | `		PH7_NetClose(pH->sock);` |
|   302 |  224 | `		pH->sock = PH7_NET_INVALID_SOCKET;` |
|   151 |  225 | `	}` |
|   315 |  226 | `}` |
|     - |  227 | ``/* php's own redirect default, which the `max_redirects` option overrides. */`` |
|     - |  228 | `/* php's whole vocabulary for a tunnel that did not come up: the CONNECT that` |
|     - |  229 | ` * could not be written and the handshake behind it share one sentence, and` |
|     - |  230 | ` * neither the proxy's own status line nor the TLS error reaches it. */` |
|     - |  231 | `#define HTTP_PROXY_TUNNEL_ERR "Cannot connect to HTTPS server through proxy"` |
|     - |  232 | `#define HTTP_MAX_REDIRECTS 20` |
|     - |  233 | ``/* php reads the status line into `char tmp_line[128]`, which its line reader`` |
|     - |  234 | ` * fills to at most 126 bytes; everything past that is discarded with the rest` |
|     - |  235 | ` * of the line. Header lines are read into an allocated buffer and uncapped. */` |
|     - |  236 | `#define HTTP_STATUS_LINE_MAX 126` |
|     - |  237 | `/* ------------------------------------------------------------------------- */` |
|     - |  238 | `/* Reading the connection                                                      */` |
|     - |  239 | `/* ------------------------------------------------------------------------- */` |
|     - |  240 | `/*` |
|     - |  241 | ` * Drop what has already been consumed from a buffer, so a long body does not` |
|     - |  242 | ` * grow one allocation per read.` |
|     - |  243 | ` */` |
|   328 |  244 | `static void HttpCompact(SyBlob *pBuf,sxu32 *pOfft)` |
|   ! 0 |  245 | `{` |
|   328 |  246 | `	if( *pOfft == 0 ){` |
|   328 |  247 | `		return;` |
|     - |  248 | `	}` |
|   ! 0 |  249 | `	if( *pOfft >= SyBlobLength(pBuf) ){` |
|   ! 0 |  250 | `		SyBlobReset(pBuf);` |
|   ! 0 |  251 | `	}else{` |
|   ! 0 |  252 | `		sxu32 nLeft = SyBlobLength(pBuf) - *pOfft;` |
|   ! 0 |  253 | `		SyMemcpy(&((char *)SyBlobData(pBuf))[*pOfft],SyBlobData(pBuf),nLeft);` |
|   ! 0 |  254 | `		pBuf->nByte = nLeft;` |
|     - |  255 | `	}` |
|   ! 0 |  256 | `	*pOfft = 0;` |
|   164 |  257 | `}` |
|     - |  258 | `/*` |
|     - |  259 | ` * One recv() worth of bytes into sRaw. Answers 1 when something arrived, 0 at` |
|     - |  260 | ` * the end of the connection, -1 for a read error.` |
|     - |  261 | ` */` |
|   512 |  262 | `static int HttpFill(http_private *pH)` |
|   ! 0 |  263 | `{` |
|     - |  264 | `	char zBuf[HTTP_CHUNK_READ];` |
|     - |  265 | `	int n;` |
|   512 |  266 | `	if( pH->bEof \|\| pH->sock == PH7_NET_INVALID_SOCKET ){` |
|     2 |  267 | `		return 0;` |
|     - |  268 | `	}` |
|   510 |  269 | `	n = HttpRecv(pH,zBuf,(int)sizeof(zBuf));` |
|   510 |  270 | `	if( n == 0 ){` |
|   196 |  271 | `		pH->bEof = 1;` |
|     - |  272 | `		/* php's notify_completed is not gated on the progress counter: a read` |
|     - |  273 | `		 * that comes back with nothing ENDS the transfer whether or not a` |
|     - |  274 | `		 * wrapper ever announced a size, which is why a connection that closes` |
|     - |  275 | `		 * where a status line was due reports it before the failure. */` |
|   196 |  276 | `		PH7_StreamCtxCompleted(pH->pCtx);` |
|   196 |  277 | `		return 0;` |
|     - |  278 | `	}` |
|   314 |  279 | `	if( n < 0 ){` |
|     - |  280 | `		/* A timed-out or interrupted read ends the body here: php's wrapper has` |
|     - |  281 | `		 * no retry either, and the bytes already in hand are the answer. */` |
|   ! 0 |  282 | `		pH->bEof = 1;` |
|   ! 0 |  283 | `		return -1;` |
|     - |  284 | `	}` |
|   314 |  285 | `	HttpCompact(&pH->sRaw,&pH->nRawOfft);` |
|   314 |  286 | `	if( SyBlobAppend(&pH->sRaw,zBuf,(sxu32)n) != SXRET_OK ){` |
|   ! 0 |  287 | `		return -1;` |
|     - |  288 | `	}` |
|   314 |  289 | `	if( !(pH->bBody && pH->bChunked) ){` |
|     - |  290 | `		/* php counts what the STREAM moved, and for a chunked body that is` |
|     - |  291 | `		 * what came out of the dechunk filter rather than what went in -- so` |
|     - |  292 | `		 * the framing bytes are counted here while the headers are being read` |
|     - |  293 | `		 * and by the decoder itself once the body starts. */` |
|   314 |  294 | `		PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)n);` |
|   157 |  295 | `	}` |
|   314 |  296 | `	return 1;` |
|   256 |  297 | `}` |
|     - |  298 | `/* How many unconsumed bytes sRaw is holding. */` |
|  2522 |  299 | `static sxu32 HttpRawLeft(http_private *pH)` |
|   ! 0 |  300 | `{` |
|  2522 |  301 | `	return SyBlobLength(&pH->sRaw) > pH->nRawOfft` |
|  2259 |  302 | `		? SyBlobLength(&pH->sRaw) - pH->nRawOfft : 0;` |
|   ! 0 |  303 | `}` |
|     - |  304 | `/*` |
|     - |  305 | ` * One line of the header block, with its terminator removed. php accepts both` |
|     - |  306 | ` * CRLF and a bare LF, and a header block that simply ENDS (the peer closed` |
|     - |  307 | ` * before the blank line) is not an error -- it is a response with no body.` |
|     - |  308 | ` *` |
|     - |  309 | ` * Answers 1 for a line, 0 at the end of the connection.` |
|     - |  310 | ` */` |
|  1528 |  311 | `static int HttpReadLine(http_private *pH,SyBlob *pLine,int *pnTerm)` |
|   ! 0 |  312 | `{` |
|  1528 |  313 | `	SyBlobReset(pLine);` |
|  1528 |  314 | `	if( pnTerm ){` |
|  1452 |  315 | `		*pnTerm = 0;` |
|   726 |  316 | `	}` |
|  1078 |  317 | `	for(;;){` |
|  1842 |  318 | `		const char *zBase = (const char *)SyBlobData(&pH->sRaw);` |
|  1842 |  319 | `		sxu32 n = HttpRawLeft(pH),i;` |
| 23612 |  320 | `		for( i = 0 ; i < n ; ++i ){` |
| 23288 |  321 | `			if( zBase[pH->nRawOfft + i] == '\n' ){` |
|  1518 |  322 | `				sxu32 nCopy = i;` |
|  1518 |  323 | `				if( nCopy > 0 && zBase[pH->nRawOfft + nCopy - 1] == '\r' ){` |
|  1508 |  324 | `					nCopy--;` |
|   754 |  325 | `				}` |
|  1518 |  326 | `				if( nCopy > 0 ){` |
|  1184 |  327 | `					SyBlobAppend(pLine,&zBase[pH->nRawOfft],nCopy);` |
|   592 |  328 | `				}` |
|  1518 |  329 | `				if( pnTerm ){` |
|     - |  330 | `					/* The LF plus the CR before it, if there was one. */` |
|  1442 |  331 | `					*pnTerm = (int)(i - nCopy) + 1;` |
|   721 |  332 | `				}` |
|  1518 |  333 | `				pH->nRawOfft += i + 1;` |
|  1518 |  334 | `				return 1;` |
|     - |  335 | `			}` |
| 10885 |  336 | `		}` |
|   324 |  337 | `		if( pH->bNoFill ){` |
|     - |  338 | `			/* Mid-DRAIN: only what is already in hand may be read, and a line` |
|     - |  339 | `			 * with no terminator yet is not a line -- it stays where it is for` |
|     - |  340 | `			 * the read that will have the rest of it. */` |
|   ! 0 |  341 | `			return 0;` |
|     - |  342 | `		}` |
|   324 |  343 | `		if( HttpFill(pH) < 1 ){` |
|     - |  344 | `			/* No terminator will ever arrive. Whatever is buffered is the last` |
|     - |  345 | `			 * line, which is what makes php report a bodiless reply's first` |
|     - |  346 | ``			 * bytes as its `status line` -- and report them with NO trailing`` |
|     - |  347 | `			 * newline, since the line it read carried none. */` |
|    10 |  348 | `			if( n > 0 ){` |
|     2 |  349 | `				SyBlobAppend(pLine,&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],n);` |
|     2 |  350 | `				pH->nRawOfft += n;` |
|     2 |  351 | `				return 1;` |
|     - |  352 | `			}` |
|     8 |  353 | `			return 0;` |
|     - |  354 | `		}` |
|   ! 0 |  355 | `	}` |
|   764 |  356 | `}` |
|     - |  357 | `/*` |
|     - |  358 | ` * Decode as much of the chunked body as the bytes ALREADY READ allow, never` |
|     - |  359 | ` * touching the socket, and answer how many bytes came out. This is php's` |
|     - |  360 | `` * `dechunk` FILTER: it runs over whatever the buffer holds, all of it, and`` |
|     - |  361 | ` * what it produced is one batch -- which is why the progress counter moves` |
|     - |  362 | ` * once per socket read rather than once per chunk. A chunk EXTENSION` |
|     - |  363 | `` * (`3;ext=1`) and the trailer headers after the final chunk are consumed here`` |
|     - |  364 | ` * and never seen.` |
|     - |  365 | ` */` |
|    36 |  366 | `static sxu32 HttpDecodeBuffered(http_private *pH)` |
|   ! 0 |  367 | `{` |
|     - |  368 | `	SyBlob sLine;` |
|    36 |  369 | `	sxu32 nDone = 0;` |
|    36 |  370 | `	SyBlobInit(&sLine,&pH->pVm->sAllocator);` |
|    36 |  371 | `	pH->bNoFill = 1;` |
|    64 |  372 | `	while( !pH->bChunkDone ){` |
|    34 |  373 | `		if( pH->iChunkLeft < 0 ){` |
|     - |  374 | `			/* UNSIGNED on purpose: a size line is whatever bytes arrived, and a` |
|     - |  375 | `			 * hundred hex digits of it must wrap rather than overflow a signed` |
|     - |  376 | `			 * accumulator. Anything past INT32_MAX is capped -- the connection` |
|     - |  377 | `			 * ends long before, and "read to the end" is what both engines then` |
|     - |  378 | `			 * do. */` |
|    34 |  379 | `			sxu64 iSize = 0;` |
|     - |  380 | `			const char *zLine;` |
|     - |  381 | `			sxu32 nLine,i;` |
|    34 |  382 | `			if( HttpReadLine(pH,&sLine,0) == 0 ){` |
|     - |  383 | `				/* Only a partial size line, and nothing may be read to finish` |
|     - |  384 | `				 * it: this batch is over, but the BODY is not. */` |
|   ! 0 |  385 | `				break;` |
|     - |  386 | `			}` |
|    34 |  387 | `			zLine = (const char *)SyBlobData(&sLine);` |
|    34 |  388 | `			nLine = SyBlobLength(&sLine);` |
|    34 |  389 | `			if( nLine == 0 ){` |
|     - |  390 | `				/* The CRLF that closes the previous chunk's data. */` |
|    14 |  391 | `				continue;` |
|     - |  392 | `			}` |
|    40 |  393 | `			for( i = 0 ; i < nLine && SyisHex(zLine[i]) ; ++i ){` |
|    20 |  394 | `				if( iSize > 0x7FFFFFFF ){` |
|   ! 0 |  395 | `					continue; /* already capped; keep consuming the digits */` |
|     - |  396 | `				}` |
|    20 |  397 | `				iSize = iSize * 16 + (sxu64)SyHexToint(zLine[i]);` |
|    10 |  398 | `			}` |
|    20 |  399 | `			if( iSize > 0x7FFFFFFF ){` |
|   ! 0 |  400 | `				iSize = 0x7FFFFFFF;` |
|   ! 0 |  401 | `			}` |
|    20 |  402 | `			if( i == 0 ){` |
|     - |  403 | `				/* Not a size line at all: the framing is broken and php's` |
|     - |  404 | `				 * filter stops there rather than guessing. */` |
|   ! 0 |  405 | `				pH->bChunkDone = 1;` |
|   ! 0 |  406 | `				break;` |
|     - |  407 | `			}` |
|    20 |  408 | `			if( iSize == 0 ){` |
|     - |  409 | `				/* The final chunk. Its trailer headers run to the blank line` |
|     - |  410 | `				 * and belong to nobody. */` |
|     8 |  411 | `				while( HttpReadLine(pH,&sLine,0) == 1 && SyBlobLength(&sLine) > 0 ){` |
|     - |  412 | `					;` |
|   ! 0 |  413 | `				}` |
|     6 |  414 | `				pH->bChunkDone = 1;` |
|     6 |  415 | `				break;` |
|     - |  416 | `			}` |
|    14 |  417 | `			pH->iChunkLeft = (sxi64)iSize;` |
|     7 |  418 | `		}` |
|    14 |  419 | `		if( HttpRawLeft(pH) == 0 ){` |
|   ! 0 |  420 | `			break; /* the chunk is owed bytes that have not arrived yet */` |
|     - |  421 | `		}` |
|     - |  422 | `		{` |
|    14 |  423 | `			sxu32 nHave = HttpRawLeft(pH);` |
|    14 |  424 | `			sxu32 nTake = (sxu32)(pH->iChunkLeft < (sxi64)nHave ? pH->iChunkLeft : nHave);` |
|    14 |  425 | `			HttpCompact(&pH->sOut,&pH->nOutOfft);` |
|    21 |  426 | `			SyBlobAppend(&pH->sOut,` |
|    14 |  427 | `				&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],nTake);` |
|    14 |  428 | `			pH->nRawOfft += nTake;` |
|    14 |  429 | `			pH->iChunkLeft -= nTake;` |
|    14 |  430 | `			nDone += nTake;` |
|    14 |  431 | `			if( pH->iChunkLeft == 0 ){` |
|    14 |  432 | `				pH->iChunkLeft = -1;` |
|     7 |  433 | `			}` |
|     - |  434 | `		}` |
|   ! 0 |  435 | `	}` |
|    36 |  436 | `	pH->bNoFill = 0;` |
|    36 |  437 | `	SyBlobRelease(&sLine);` |
|    36 |  438 | `	return nDone;` |
|   ! 0 |  439 | `}` |
|     - |  440 | `/*` |
|     - |  441 | ` * Pump the decoder until sOut can serve nWant bytes or the body ends, reading` |
|     - |  442 | ` * the socket when it must -- and reporting each read's decoded output as php's` |
|     - |  443 | ` * stream layer does, once per read.` |
|     - |  444 | ` */` |
|    30 |  445 | `static void HttpPumpChunks(http_private *pH,sxu32 nWant)` |
|   ! 0 |  446 | `{` |
|    15 |  447 | `	for(;;){` |
|    30 |  448 | `		sxu32 nDec = HttpDecodeBuffered(pH);` |
|    30 |  449 | `		if( nDec > 0 ){` |
|   ! 0 |  450 | `			PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)nDec);` |
|   ! 0 |  451 | `		}` |
|    30 |  452 | `		if( pH->bChunkDone \|\| SyBlobLength(&pH->sOut) - pH->nOutOfft >= nWant ){` |
|    15 |  453 | `			break;` |
|     - |  454 | `		}` |
|   ! 0 |  455 | `		if( HttpFill(pH) < 1 ){` |
|   ! 0 |  456 | `			pH->bChunkDone = 1;` |
|   ! 0 |  457 | `			break;` |
|     - |  458 | `		}` |
|   ! 0 |  459 | `	}` |
|    30 |  460 | `}` |
|     - |  461 | `/*` |
|     - |  462 | `` * What a reader could take right now -- php's `writepos - readpos`, the bytes`` |
|     - |  463 | ` * its stream buffer is holding. A wrapper frame CREDITS this to the progress` |
|     - |  464 | ` * counter the moment it arms one, which is why a body that arrived in the same` |
|     - |  465 | ` * recv() as its headers is reported before anything has read a byte.` |
|     - |  466 | ` */` |
|   268 |  467 | `static sxi64 HttpBuffered(http_private *pH)` |
|   ! 0 |  468 | `{` |
|   268 |  469 | `	if( pH->bChunked ){` |
|   ! 0 |  470 | `		return (sxi64)(SyBlobLength(&pH->sOut) > pH->nOutOfft` |
|   ! 0 |  471 | `			? SyBlobLength(&pH->sOut) - pH->nOutOfft : 0);` |
|     - |  472 | `	}` |
|   268 |  473 | `	return (sxi64)HttpRawLeft(pH);` |
|   134 |  474 | `}` |
|     - |  475 | `/* ------------------------------------------------------------------------- */` |
|     - |  476 | `/* The request                                                                 */` |
|     - |  477 | `/* ------------------------------------------------------------------------- */` |
|     - |  478 | `/* php's own trim set, which is what the header block's two ends are cut with. */` |
|   254 |  479 | `static int HttpIsTrimByte(char c)` |
|   ! 0 |  480 | `{` |
|   254 |  481 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\r' \|\| c == 0 \|\| c == 0x0B;` |
|   ! 0 |  482 | `}` |
|     - |  483 | `/*` |
|     - |  484 | ` * Does the script's own header block already carry this header? php compares` |
|     - |  485 | ` * case-insensitively at the START of a line, which is what lets a caller take` |
|     - |  486 | ` * over php's Host, Connection, User-Agent, Content-Length, Content-Type and` |
|     - |  487 | ` * Authorization slots.` |
|     - |  488 | ` */` |
|   936 |  489 | `static int HttpUserHas(SyBlob *pHdrs,const char *zName)` |
|   ! 0 |  490 | `{` |
|   936 |  491 | `	const char *zIn = (const char *)SyBlobData(pHdrs);` |
|   936 |  492 | `	sxu32 nIn = SyBlobLength(pHdrs),nName = (sxu32)SyStrlen(zName);` |
|   936 |  493 | `	sxu32 i = 0;` |
|  1058 |  494 | `	while( i < nIn ){` |
|   134 |  495 | `		sxu32 nStart = i;` |
|  1924 |  496 | `		while( i < nIn && zIn[i] != '\n' ){` |
|  1790 |  497 | `			i++;` |
|   ! 0 |  498 | `		}` |
|   134 |  499 | `		if( i - nStart >= nName && SyStrnicmp(&zIn[nStart],zName,nName) == 0 ){` |
|    12 |  500 | `			return 1;` |
|     - |  501 | `		}` |
|   122 |  502 | `		i++;` |
|   ! 0 |  503 | `	}` |
|   924 |  504 | `	return 0;` |
|   468 |  505 | `}` |
|     - |  506 | `/*` |
|     - |  507 | `` * The `header` context option, in the two shapes php reads: one string (whose`` |
|     - |  508 | ` * own line breaks separate the headers) or an ARRAY of them, whose STRING` |
|     - |  509 | ` * entries are joined with CRLF and whose other entries are dropped. Anything` |
|     - |  510 | ` * that is neither -- an int, a float, an object -- is ignored in silence.` |
|     - |  511 | ` *` |
|     - |  512 | ` * php then trims the WHOLE block at both ends and emits what is left verbatim.` |
|     - |  513 | ` * It does not touch the inside, which is worth being exact about: a blank line` |
|     - |  514 | ` * in the middle ENDS the request's header block, and everything the script put` |
|     - |  515 | ` * after it becomes the request BODY.` |
|     - |  516 | ` */` |
|   300 |  517 | `static void HttpCollectUserHeaders(ph7_vm *pVm,ph7_value *pOpt,SyBlob *pHdrs)` |
|   ! 0 |  518 | `{` |
|     - |  519 | `	SyBlob sJoin;` |
|     - |  520 | `	const char *zIn;` |
|     - |  521 | `	sxu32 nIn,nStart;` |
|   300 |  522 | `	if( pOpt == 0 ){` |
|   268 |  523 | `		return;` |
|     - |  524 | `	}` |
|    32 |  525 | `	SyBlobInit(&sJoin,&pVm->sAllocator);` |
|    32 |  526 | `	if( pOpt->iFlags & MEMOBJ_HASHMAP ){` |
|     8 |  527 | `		ph7_hashmap *pMap = (ph7_hashmap *)pOpt->x.pOther;` |
|     - |  528 | `		ph7_hashmap_node *pEntry;` |
|     8 |  529 | `		pMap->pCur = pMap->pFirst;` |
|    30 |  530 | `		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    22 |  531 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|    22 |  532 | `			if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    20 |  533 | `				SyBlobAppend(&sJoin,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|    20 |  534 | `				SyBlobAppend(&sJoin,"\r\n",sizeof("\r\n")-1);` |
|    10 |  535 | `			}` |
|   ! 0 |  536 | `		}` |
|    28 |  537 | `	}else if( pOpt->iFlags & MEMOBJ_STRING ){` |
|    24 |  538 | `		SyBlobAppend(&sJoin,SyBlobData(&pOpt->sBlob),SyBlobLength(&pOpt->sBlob));` |
|    12 |  539 | `	}` |
|    32 |  540 | `	zIn = (const char *)SyBlobData(&sJoin);` |
|    32 |  541 | `	nIn = SyBlobLength(&sJoin);` |
|    32 |  542 | `	nStart = 0;` |
|    44 |  543 | `	while( nStart < nIn && HttpIsTrimByte(zIn[nStart]) ){` |
|    12 |  544 | `		nStart++;` |
|   ! 0 |  545 | `	}` |
|    56 |  546 | `	while( nIn > nStart && HttpIsTrimByte(zIn[nIn-1]) ){` |
|    24 |  547 | `		nIn--;` |
|   ! 0 |  548 | `	}` |
|    32 |  549 | `	if( nIn > nStart ){` |
|    32 |  550 | `		SyBlobAppend(pHdrs,&zIn[nStart],nIn - nStart);` |
|    32 |  551 | `		SyBlobAppend(pHdrs,"\r\n",sizeof("\r\n")-1);` |
|    16 |  552 | `	}` |
|    32 |  553 | `	SyBlobRelease(&sJoin);` |
|   150 |  554 | `}` |
|     - |  555 | ``/* An `http` context option by name, or 0. */`` |
|  3210 |  556 | `static ph7_value * HttpOpt(phl_stream_ctx *pCtx,const char *zName)` |
|     1 |  557 | `{` |
|  3211 |  558 | `	return pCtx ? PH7_StreamCtxOption(pCtx,"http",zName) : 0;` |
|     1 |  559 | `}` |
|     - |  560 | `/*` |
|     - |  561 | ` * The three readers below all work on a COPY. Every ph7_value_to_*() converts` |
|     - |  562 | ` * its argument in place, and these arguments are the script's own context` |
|     - |  563 | `` * entries -- reading `protocol_version` as a number must not leave a FLOAT`` |
|     - |  564 | ` * where the script put a string, which stream_context_get_options() would then` |
|     - |  565 | ` * answer.` |
|     - |  566 | ` */` |
|    14 |  567 | `static sxi64 HttpOptInt(ph7_vm *pVm,ph7_value *pVal,sxi64 iDefault)` |
|   ! 0 |  568 | `{` |
|     - |  569 | `	ph7_value sCopy;` |
|     - |  570 | `	sxi64 iOut;` |
|    14 |  571 | `	if( pVal == 0 ){` |
|   ! 0 |  572 | `		return iDefault;` |
|     - |  573 | `	}` |
|    14 |  574 | `	PH7_MemObjInit(pVm,&sCopy);` |
|    14 |  575 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|    14 |  576 | `	iOut = ph7_value_to_int64(&sCopy);` |
|    14 |  577 | `	PH7_MemObjRelease(&sCopy);` |
|    14 |  578 | `	return iOut;` |
|     7 |  579 | `}` |
|   316 |  580 | `static int HttpOptBool(ph7_vm *pVm,ph7_value *pVal,int bDefault)` |
|     1 |  581 | `{` |
|     - |  582 | `	ph7_value sCopy;` |
|     - |  583 | `	int bOut;` |
|   317 |  584 | `	if( pVal == 0 ){` |
|   309 |  585 | `		return bDefault;` |
|     - |  586 | `	}` |
|     8 |  587 | `	PH7_MemObjInit(pVm,&sCopy);` |
|     8 |  588 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|     8 |  589 | `	bOut = ph7_value_to_bool(&sCopy) != 0;` |
|     8 |  590 | `	PH7_MemObjRelease(&sCopy);` |
|     8 |  591 | `	return bOut;` |
|   159 |  592 | `}` |
|   300 |  593 | `static double HttpOptReal(ph7_vm *pVm,ph7_value *pVal,double rDefault)` |
|   ! 0 |  594 | `{` |
|     - |  595 | `	ph7_value sCopy;` |
|     - |  596 | `	double rOut;` |
|   300 |  597 | `	if( pVal == 0 ){` |
|   290 |  598 | `		return rDefault;` |
|     - |  599 | `	}` |
|    10 |  600 | `	PH7_MemObjInit(pVm,&sCopy);` |
|    10 |  601 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|    10 |  602 | `	rOut = ph7_value_to_double(&sCopy);` |
|    10 |  603 | `	PH7_MemObjRelease(&sCopy);` |
|    10 |  604 | `	return rOut;` |
|   150 |  605 | `}` |
|     - |  606 | `/* An ini directive's text, appended to pOut; answers 1 when the directive` |
|     - |  607 | ``  * carries a VALUE at all (php's unset directive is a third state, and `from` `` |
|     - |  608 | ` * is emitted for an EMPTY value but not for an absent one). */` |
|   592 |  609 | `static int HttpIniStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|   ! 0 |  610 | `{` |
|   592 |  611 | `	if( PH7_VmIniIsUnset(pVm,zName) ){` |
|   580 |  612 | `		return 0;` |
|     - |  613 | `	}` |
|    12 |  614 | `	SyBlobReset(pOut);` |
|    12 |  615 | `	PH7_VmIniGetStr(pVm,zName,pOut);` |
|    12 |  616 | `	return 1;` |
|   296 |  617 | `}` |
|     - |  618 | `/* base64 of "user:pass" for the Authorization header php builds out of a URL's` |
|     - |  619 | ` * userinfo. */` |
|     6 |  620 | `static sxi32 HttpB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|   ! 0 |  621 | `{` |
|     6 |  622 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|   ! 0 |  623 | `}` |
|     - |  624 | `/*` |
|     - |  625 | ` * Compose the request php would put on the wire.` |
|     - |  626 | ` *` |
|     - |  627 | ` * The slot order is derived, not invented: request line, the URL's` |
|     - |  628 | `` * Authorization, the `from` ini, Host, Connection, User-Agent, the automatic`` |
|     - |  629 | ` * Content-Length, the script's own headers, and last the automatic` |
|     - |  630 | ` * Content-Type. A body follows the blank line.` |
|     - |  631 | ` */` |
|     - |  632 | `/* The port an unspelled URL of this scheme is asking for. */` |
|   366 |  633 | `static int HttpDefaultPort(int bTls)` |
|   ! 0 |  634 | `{` |
|   366 |  635 | `	return bTls ? 443 : 80;` |
|   ! 0 |  636 | `}` |
|     - |  637 | `/*` |
|     - |  638 | ` * php's port rule: whatever the URL spells, else the scheme's own default.` |
|     - |  639 | ` */` |
|   702 |  640 | `static int HttpUriPort(SyhttpUri *pUri,int bTls)` |
|     1 |  641 | `{` |
|   703 |  642 | `	sxi32 iPort = 0;` |
|   703 |  643 | `	if( SyStringLength(&pUri->sPort) < 1 ){` |
|    30 |  644 | `		return HttpDefaultPort(bTls);` |
|     - |  645 | `	}` |
|   673 |  646 | `	SyStrToInt32(pUri->sPort.zString,pUri->sPort.nByte,(void *)&iPort,0);` |
|   673 |  647 | `	return iPort > 0 ? (int)iPort : HttpDefaultPort(bTls);` |
|   352 |  648 | `}` |
|     - |  649 | `/*` |
|     - |  650 | `` * The one `Proxy-Authorization:` line among the script's own headers, as php`` |
|     - |  651 | ` * copies it onto a CONNECT request.` |
|     - |  652 | ` *` |
|     - |  653 | `` * php scans the `header` option itself here rather than reusing the block it`` |
|     - |  654 | ` * builds for the origin request: an ARRAY is walked entry by entry and the` |
|     - |  655 | ` * FIRST entry carrying one wins, a string is walked line by line, and the line` |
|     - |  656 | ` * is taken verbatim from its name to its terminator -- leading blanks skipped,` |
|     - |  657 | ` * nothing else touched. A name that merely STARTS with it does not count: the` |
|     - |  658 | `` * length up to the colon has to be the whole of `Proxy-Authorization:`.`` |
|     - |  659 | ` */` |
|    10 |  660 | `static int HttpProxyAuthLine(const char *zIn,sxu32 nIn,SyBlob *pOut)` |
|   ! 0 |  661 | `{` |
|    10 |  662 | `	sxu32 i = 0;` |
|     - |  663 | `	static const sxu32 nName = sizeof("Proxy-Authorization:")-1;` |
|    16 |  664 | `	while( i < nIn ){` |
|     - |  665 | `		sxu32 nStart,nColon,nEnd;` |
|    18 |  666 | `		while( i < nIn && (zIn[i] == ' ' \|\| zIn[i] == '\t') ){` |
|   ! 0 |  667 | `			i++;` |
|   ! 0 |  668 | `		}` |
|    12 |  669 | `		nStart = i;` |
|   180 |  670 | `		while( i < nIn && zIn[i] != ':' && zIn[i] != '\r' && zIn[i] != '\n' ){` |
|   168 |  671 | `			i++;` |
|   ! 0 |  672 | `		}` |
|    12 |  673 | `		nColon = i;` |
|    12 |  674 | `		if( i < nIn && zIn[i] == ':' ){` |
|    12 |  675 | `			i++;` |
|    92 |  676 | `			while( i < nIn && zIn[i] != '\r' && zIn[i] != '\n' ){` |
|    80 |  677 | `				i++;` |
|   ! 0 |  678 | `			}` |
|    12 |  679 | `			nEnd = i;` |
|    12 |  680 | `			if( nColon + 1 - nStart == nName` |
|     9 |  681 | `			 && SyStrnicmp(&zIn[nStart],"Proxy-Authorization:",nName) == 0 ){` |
|     6 |  682 | `				SyBlobAppend(pOut,&zIn[nStart],nEnd - nStart);` |
|     6 |  683 | `				SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     6 |  684 | `				return 1;` |
|     - |  685 | `			}` |
|     3 |  686 | `		}` |
|     6 |  687 | `		while( i < nIn && zIn[i] != '\r' && zIn[i] != '\n' ){` |
|   ! 0 |  688 | `			i++;` |
|   ! 0 |  689 | `		}` |
|    13 |  690 | `		while( i < nIn && (zIn[i] == '\r' \|\| zIn[i] == '\n') ){` |
|     4 |  691 | `			i++;` |
|   ! 0 |  692 | `		}` |
|   ! 0 |  693 | `	}` |
|     4 |  694 | `	return 0;` |
|     5 |  695 | `}` |
|     - |  696 | ``/* That line, wherever the `header` option keeps it. */`` |
|    16 |  697 | `static int HttpProxyAuth(ph7_value *pOpt,SyBlob *pOut)` |
|   ! 0 |  698 | `{` |
|    16 |  699 | `	if( pOpt == 0 ){` |
|     8 |  700 | `		return 0;` |
|     - |  701 | `	}` |
|     8 |  702 | `	if( pOpt->iFlags & MEMOBJ_HASHMAP ){` |
|     2 |  703 | `		ph7_hashmap *pMap = (ph7_hashmap *)pOpt->x.pOther;` |
|     - |  704 | `		ph7_hashmap_node *pEntry;` |
|     2 |  705 | `		pMap->pCur = pMap->pFirst;` |
|     4 |  706 | `		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|     4 |  707 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|     4 |  708 | `			if( pVal && (pVal->iFlags & MEMOBJ_STRING)` |
|     4 |  709 | `			 && HttpProxyAuthLine((const char *)SyBlobData(&pVal->sBlob),` |
|     2 |  710 | `					SyBlobLength(&pVal->sBlob),pOut) ){` |
|     2 |  711 | `				return 1;` |
|     - |  712 | `			}` |
|   ! 0 |  713 | `		}` |
|   ! 0 |  714 | `		return 0;` |
|     - |  715 | `	}` |
|     6 |  716 | `	if( (pOpt->iFlags & MEMOBJ_STRING) == 0 ){` |
|   ! 0 |  717 | `		return 0;` |
|     - |  718 | `	}` |
|     9 |  719 | `	return HttpProxyAuthLine((const char *)SyBlobData(&pOpt->sBlob),` |
|     3 |  720 | `		SyBlobLength(&pOpt->sBlob),pOut);` |
|     8 |  721 | `}` |
|     - |  722 | `/*` |
|     - |  723 | `` * Drop the first `Proxy-Authorization:` line from a header block. It addressed`` |
|     - |  724 | ` * the PROXY, and a tunnelled request is addressed to the origin -- php sends it` |
|     - |  725 | ` * on the CONNECT and takes it back out of everything behind the tunnel.` |
|     - |  726 | ` */` |
|    14 |  727 | `static void HttpDropProxyAuth(ph7_vm *pVm,SyBlob *pHdrs)` |
|   ! 0 |  728 | `{` |
|     - |  729 | `	SyBlob sKeep;` |
|    14 |  730 | `	const char *zIn = (const char *)SyBlobData(pHdrs);` |
|    14 |  731 | `	sxu32 nIn = SyBlobLength(pHdrs),i = 0;` |
|    14 |  732 | `	int bDropped = 0;` |
|    14 |  733 | `	SyBlobInit(&sKeep,&pVm->sAllocator);` |
|    28 |  734 | `	while( i < nIn ){` |
|    14 |  735 | `		sxu32 nStart = i;` |
|   300 |  736 | `		while( i < nIn && zIn[i] != '\n' ){` |
|   286 |  737 | `			i++;` |
|   ! 0 |  738 | `		}` |
|    14 |  739 | `		if( i < nIn ){` |
|    14 |  740 | `			i++; /* the LF belongs to the line it ends */` |
|     7 |  741 | `		}` |
|    14 |  742 | `		if( !bDropped && i - nStart >= sizeof("proxy-authorization:")-1` |
|    10 |  743 | `		 && SyStrnicmp(&zIn[nStart],"Proxy-Authorization:",` |
|     4 |  744 | `				sizeof("Proxy-Authorization:")-1) == 0 ){` |
|     6 |  745 | `			bDropped = 1;` |
|     6 |  746 | `			continue;` |
|     - |  747 | `		}` |
|     8 |  748 | `		SyBlobAppend(&sKeep,&zIn[nStart],i - nStart);` |
|   ! 0 |  749 | `	}` |
|    14 |  750 | `	if( bDropped ){` |
|     6 |  751 | `		SyBlobReset(pHdrs);` |
|     6 |  752 | `		SyBlobAppend(pHdrs,SyBlobData(&sKeep),SyBlobLength(&sKeep));` |
|     3 |  753 | `	}` |
|    14 |  754 | `	SyBlobRelease(&sKeep);` |
|    14 |  755 | `}` |
|   300 |  756 | `static void HttpBuildRequest(ph7_vm *pVm,phl_stream_ctx *pCtx,SyhttpUri *pUri,int bTls,` |
|     - |  757 | `	const char *zTarget,int nTarget,SyBlob *pOut,const char *zMethod,int nMethod,` |
|     - |  758 | `	const char *zBody,int nBody,int bTunnel)` |
|   ! 0 |  759 | `{` |
|     - |  760 | `	SyBlob sUser,sTmp;` |
|     - |  761 | `	ph7_value *pOptV;` |
|   300 |  762 | `	SyBlobInit(&sUser,&pVm->sAllocator);` |
|   300 |  763 | `	SyBlobInit(&sTmp,&pVm->sAllocator);` |
|   300 |  764 | `	HttpCollectUserHeaders(pVm,HttpOpt(pCtx,"header"),&sUser);` |
|   300 |  765 | `	if( bTunnel ){` |
|    14 |  766 | `		HttpDropProxyAuth(pVm,&sUser);` |
|     7 |  767 | `	}` |
|     - |  768 | `	/* Request line. */` |
|   300 |  769 | `	SyBlobAppend(pOut,zMethod,(sxu32)nMethod);` |
|   300 |  770 | `	SyBlobAppend(pOut," ",1);` |
|   300 |  771 | `	SyBlobAppend(pOut,zTarget,(sxu32)nTarget);` |
|   300 |  772 | `	SyBlobAppend(pOut," HTTP/",sizeof(" HTTP/")-1);` |
|     - |  773 | `	/* php reads the version as a DOUBLE and prints it with ONE decimal, so a` |
|     - |  774 | ``	 * string is converted rather than passed through: `'2.0'` is 2.0, an ARRAY`` |
|     - |  775 | ``	 * is 1.0, and `null` is 0.0 -- each of which reaches the wire. */`` |
|   300 |  776 | `	SyBlobFormat(pOut,"%.1f",HttpOptReal(pVm,HttpOpt(pCtx,"protocol_version"),1.1));` |
|   300 |  777 | `	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     - |  778 | `	/* The URL's own credentials, which a script-supplied Authorization wins` |
|     - |  779 | ``	 * over. php base64s `user:pass` verbatim -- it decodes neither half. */`` |
|   300 |  780 | `	if( SyStringLength(&pUri->sUser) > 0 && !HttpUserHas(&sUser,"authorization:") ){` |
|     2 |  781 | `		SyBlobReset(&sTmp);` |
|     2 |  782 | `		SyBlobAppend(&sTmp,pUri->sUser.zString,pUri->sUser.nByte);` |
|     2 |  783 | `		SyBlobAppend(&sTmp,":",1);` |
|     2 |  784 | `		if( SyStringLength(&pUri->sPass) > 0 ){` |
|     2 |  785 | `			SyBlobAppend(&sTmp,pUri->sPass.zString,pUri->sPass.nByte);` |
|     1 |  786 | `		}` |
|     2 |  787 | `		SyBlobAppend(pOut,"Authorization: Basic ",sizeof("Authorization: Basic ")-1);` |
|     3 |  788 | `		SyBase64Encode((const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp),` |
|     1 |  789 | `			HttpB64Consumer,pOut);` |
|     2 |  790 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     1 |  791 | `	}` |
|     - |  792 | ``	/* php's `from` ini is a From: header whenever the directive has a value at`` |
|     - |  793 | `	 * all -- an EMPTY one still writes the header, which is not how the` |
|     - |  794 | `	 * user_agent directive below behaves. */` |
|   300 |  795 | `	if( HttpIniStr(pVm,"from",&sTmp) ){` |
|     6 |  796 | `		SyBlobAppend(pOut,"From: ",sizeof("From: ")-1);` |
|     6 |  797 | `		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|     6 |  798 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     3 |  799 | `	}` |
|   300 |  800 | `	if( !HttpUserHas(&sUser,"host:") ){` |
|     - |  801 | `		/* The port is named only when it is not the SCHEME's own -- so an` |
|     - |  802 | `		 * https:// URL on 443 is as bare as an http:// one on 80. php reads` |
|     - |  803 | `		 * the port as a NUMBER and prints that, which is why a URL spelling` |
|     - |  804 | ``		 * `:0080` still names no port here. */`` |
|   298 |  805 | `		int iPort = HttpUriPort(pUri,bTls);` |
|   298 |  806 | `		SyBlobAppend(pOut,"Host: ",sizeof("Host: ")-1);` |
|   298 |  807 | `		SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);` |
|   298 |  808 | `		if( iPort != HttpDefaultPort(bTls) ){` |
|   282 |  809 | `			SyBlobFormat(pOut,":%d",iPort);` |
|   141 |  810 | `		}` |
|   298 |  811 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|   149 |  812 | `	}` |
|   300 |  813 | `	if( !HttpUserHas(&sUser,"connection:") ){` |
|   298 |  814 | `		SyBlobAppend(pOut,"Connection: close\r\n",sizeof("Connection: close\r\n")-1);` |
|   149 |  815 | `	}` |
|   300 |  816 | `	if( !HttpUserHas(&sUser,"user-agent:") ){` |
|   298 |  817 | `		pOptV = HttpOpt(pCtx,"user_agent");` |
|   298 |  818 | `		SyBlobReset(&sTmp);` |
|   298 |  819 | `		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){` |
|     6 |  820 | `			SyBlobAppend(&sTmp,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|     3 |  821 | `		}else{` |
|   292 |  822 | `			HttpIniStr(pVm,"user_agent",&sTmp);` |
|     - |  823 | `		}` |
|   298 |  824 | `		if( SyBlobLength(&sTmp) > 0 ){` |
|     8 |  825 | `			SyBlobAppend(pOut,"User-Agent: ",sizeof("User-Agent: ")-1);` |
|     8 |  826 | `			SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|     8 |  827 | `			SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     4 |  828 | `		}` |
|   149 |  829 | `	}` |
|   300 |  830 | `	if( nBody > 0 && !HttpUserHas(&sUser,"content-length:") ){` |
|    14 |  831 | `		SyBlobFormat(pOut,"Content-Length: %d\r\n",nBody);` |
|     7 |  832 | `	}` |
|   300 |  833 | `	SyBlobAppend(pOut,SyBlobData(&sUser),SyBlobLength(&sUser));` |
|   300 |  834 | `	if( nBody > 0 && !HttpUserHas(&sUser,"content-type:") ){` |
|     - |  835 | `		SyString sCaller;` |
|    14 |  836 | `		SyBlobAppend(pOut,"Content-Type: application/x-www-form-urlencoded\r\n",` |
|     - |  837 | `			sizeof("Content-Type: application/x-www-form-urlencoded\r\n")-1);` |
|     - |  838 | `		/* php SAYS so, once per request it composes: an E_NOTICE under the name` |
|     - |  839 | `		 * of whatever function is doing the opening. */` |
|    14 |  840 | `		SyStringInitFromBuf(&sCaller,pVm->zOpenCaller ? pVm->zOpenCaller : "",` |
|     - |  841 | `			pVm->zOpenCaller ? SyStrlen(pVm->zOpenCaller) : 0);` |
|    14 |  842 | `		PH7_VmThrowError(pVm,pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_NOTICE,` |
|     - |  843 | `			"Content-type not specified assuming application/x-www-form-urlencoded");` |
|     7 |  844 | `	}` |
|   300 |  845 | `	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|   300 |  846 | `	if( nBody > 0 ){` |
|    16 |  847 | `		SyBlobAppend(pOut,zBody,(sxu32)nBody);` |
|     8 |  848 | `	}` |
|   300 |  849 | `	SyBlobRelease(&sUser);` |
|   300 |  850 | `	SyBlobRelease(&sTmp);` |
|   300 |  851 | `}` |
|     - |  852 | `/* ------------------------------------------------------------------------- */` |
|     - |  853 | `/* The exchange                                                                */` |
|     - |  854 | `/* ------------------------------------------------------------------------- */` |
|     - |  855 | `/*` |
|     - |  856 | ` * php's failure text for an open in flight. The reason has to outlive this` |
|     - |  857 | ` * call, so it is composed into the VM's own buffer.` |
|     - |  858 | ` */` |
|    38 |  859 | `static void HttpFail(ph7_vm *pVm,const char *zText,int nText)` |
|     1 |  860 | `{` |
|    39 |  861 | `	sxu32 nCopy = (sxu32)(nText < 0 ? (int)SyStrlen(zText) : nText);` |
|    39 |  862 | `	if( nCopy >= sizeof(pVm->zOpenErrBuf) ){` |
|   ! 0 |  863 | `		nCopy = sizeof(pVm->zOpenErrBuf) - 1;` |
|   ! 0 |  864 | `	}` |
|    39 |  865 | `	SyMemcpy(zText,pVm->zOpenErrBuf,nCopy);` |
|    39 |  866 | `	pVm->zOpenErrBuf[nCopy] = 0;` |
|    39 |  867 | `	PH7_StreamSetOpenError(pVm,pVm->zOpenErrBuf);` |
|    39 |  868 | `}` |
|     - |  869 | `/*` |
|     - |  870 | `` * The request TARGET: the URL's path and query, `/` when it has neither, and`` |
|     - |  871 | `` * the whole URL when `request_fulluri` says so (which is what a proxy that`` |
|     - |  872 | ` * insists on absolute-form gets).` |
|     - |  873 | ` */` |
|   312 |  874 | `static void HttpRequestTarget(SyhttpUri *pUri,const char *zUrl,int nUrl,` |
|     - |  875 | `	int bFullUri,SyBlob *pOut)` |
|     1 |  876 | `{` |
|   313 |  877 | `	SyBlobReset(pOut);` |
|   313 |  878 | `	if( bFullUri ){` |
|     4 |  879 | `		SyBlobAppend(pOut,zUrl,(sxu32)nUrl);` |
|     4 |  880 | `		return;` |
|     - |  881 | `	}` |
|   309 |  882 | `	if( SyStringLength(&pUri->sPath) < 1 ){` |
|     2 |  883 | `		SyBlobAppend(pOut,"/",1);` |
|     1 |  884 | `	}else{` |
|   307 |  885 | `		SyBlobAppend(pOut,pUri->sPath.zString,pUri->sPath.nByte);` |
|     - |  886 | `	}` |
|   309 |  887 | `	if( SyStringLength(&pUri->sQuery) > 0 ){` |
|     6 |  888 | `		SyBlobAppend(pOut,"?",1);` |
|     6 |  889 | `		SyBlobAppend(pOut,pUri->sQuery.zString,pUri->sQuery.nByte);` |
|     3 |  890 | `	}` |
|   157 |  891 | `}` |
|     - |  892 | `/*` |
|     - |  893 | `` * Where a `Location:` points, as an absolute `http://…` URL.`` |
|     - |  894 | ` *` |
|     - |  895 | ` * php recognizes exactly two shapes: one with a scheme (taken whole, host and` |
|     - |  896 | `` * all) and one starting with `/` (the path, on the same host). Everything else`` |
|     - |  897 | `` * is joined to the CURRENT path, up to and including its last `/`, with another`` |
|     - |  898 | `` * `/` between -- so `/a/b` + `rel` is `/a//rel`, which php does not normalize.`` |
|     - |  899 | `` * A path that is just `/` joins directly, and so does an empty one. A Location`` |
|     - |  900 | ` * of at most one byte is never joined: php puts it straight under the root, so` |
|     - |  901 | `` * `x` is `/x` and an empty one is `/` (php 8.5.11 settled that last case, which`` |
|     - |  902 | ` * earlier builds answered from a read past the end of the header).` |
|     - |  903 | ` */` |
|    40 |  904 | `static void HttpResolveLocation(SyhttpUri *pUri,int bTls,const char *zLoc,sxu32 nLoc,SyBlob *pOut)` |
|   ! 0 |  905 | `{` |
|     - |  906 | `	sxu32 i;` |
|    40 |  907 | `	SyBlobReset(pOut);` |
|    62 |  908 | `	for( i = 0 ; i + 2 < nLoc ; ++i ){` |
|    56 |  909 | `		if( zLoc[i] == ':' && zLoc[i+1] == '/' && zLoc[i+2] == '/' ){` |
|     - |  910 | `			/* An absolute URL. php takes it as written, whatever the scheme --` |
|     - |  911 | `			 * an unsupported one then fails at the device lookup. */` |
|     2 |  912 | `			SyBlobAppend(pOut,zLoc,nLoc);` |
|     2 |  913 | `			return;` |
|     - |  914 | `		}` |
|    54 |  915 | `		if( zLoc[i] == '/' \|\| zLoc[i] == '?' \|\| zLoc[i] == '#' ){` |
|    16 |  916 | `			break;` |
|     - |  917 | `		}` |
|    11 |  918 | `	}` |
|     - |  919 | `	/* The URL this hop was reached by keeps its scheme: php rebuilds a` |
|     - |  920 | `	 * relative Location against the CURRENT one, so a redirect inside an` |
|     - |  921 | `	 * https:// exchange stays on TLS. */` |
|    38 |  922 | `	SyBlobAppend(pOut,bTls ? "https://" : "http://",bTls ? sizeof("https://")-1 : sizeof("http://")-1);` |
|    38 |  923 | `	if( SyStringLength(&pUri->sUser) > 0 ){` |
|   ! 0 |  924 | `		SyBlobAppend(pOut,pUri->sUser.zString,pUri->sUser.nByte);` |
|   ! 0 |  925 | `		if( SyStringLength(&pUri->sPass) > 0 ){` |
|   ! 0 |  926 | `			SyBlobAppend(pOut,":",1);` |
|   ! 0 |  927 | `			SyBlobAppend(pOut,pUri->sPass.zString,pUri->sPass.nByte);` |
|   ! 0 |  928 | `		}` |
|   ! 0 |  929 | `		SyBlobAppend(pOut,"@",1);` |
|   ! 0 |  930 | `	}` |
|    38 |  931 | `	SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);` |
|    38 |  932 | `	if( HttpUriPort(pUri,bTls) != HttpDefaultPort(bTls) ){` |
|    38 |  933 | `		SyBlobFormat(pOut,":%d",HttpUriPort(pUri,bTls));` |
|    19 |  934 | `	}` |
|    38 |  935 | `	if( nLoc > 0 && zLoc[0] == '/' ){` |
|    30 |  936 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|    30 |  937 | `		return;` |
|     - |  938 | `	}` |
|     8 |  939 | `	if( nLoc < 2 ){` |
|     4 |  940 | `		SyBlobAppend(pOut,"/",1);` |
|     4 |  941 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|     4 |  942 | `		return;` |
|     - |  943 | `	}` |
|     - |  944 | `	{` |
|     4 |  945 | `		const char *zPath = pUri->sPath.zString;` |
|     4 |  946 | `		sxu32 nPath = pUri->sPath.nByte,nKeep = 0;` |
|    44 |  947 | `		for( i = 0 ; i < nPath ; ++i ){` |
|    40 |  948 | `			if( zPath[i] == '/' ){` |
|     8 |  949 | `				nKeep = i + 1;` |
|     4 |  950 | `			}` |
|    20 |  951 | `		}` |
|     4 |  952 | `		if( nKeep < 2 ){` |
|     - |  953 | ``			/* The path is `/` or has no directory part at all: php joins with`` |
|     - |  954 | `			 * a single separator. */` |
|   ! 0 |  955 | `			SyBlobAppend(pOut,"/",1);` |
|   ! 0 |  956 | `		}else{` |
|     4 |  957 | `			SyBlobAppend(pOut,zPath,nKeep);` |
|     4 |  958 | `			SyBlobAppend(pOut,"/",1);` |
|     - |  959 | `		}` |
|     4 |  960 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|     - |  961 | `	}` |
|    20 |  962 | `}` |
|     - |  963 | `/* Is this header line the named one? The name carries its own colon. */` |
|  1492 |  964 | `static int HttpHeaderIs(const char *zLine,sxu32 nLine,const char *zName,sxu32 nName)` |
|   ! 0 |  965 | `{` |
|  1492 |  966 | `	return nLine >= nName && SyStrnicmp(zLine,zName,nName) == 0;` |
|   ! 0 |  967 | `}` |
|     - |  968 | `/*` |
|     - |  969 | `` * The two headers php reports to a context's `notification` callback, told`` |
|     - |  970 | `` * apart by what each one SENDS: `Content-Type` sends its VALUE with the blanks`` |
|     - |  971 | `` * after the colon skipped, and `Content-Length` sends the whole LINE with the`` |
|     - |  972 | ` * size beside it. The size is announced only when the value is a plain run of` |
|     - |  973 | ``  * DIGITS -- `+5`, `2x` and an empty one are no announcement at all, where `007` `` |
|     - |  974 | ` * is seven -- and a run too wide for the clock saturates rather than wrapping.` |
|     - |  975 | ` */` |
|   814 |  976 | `static void HttpNotifyHeader(http_private *pH,const char *zLine,sxu32 nLine)` |
|   ! 0 |  977 | `{` |
|     - |  978 | `	sxu32 i;` |
|   814 |  979 | `	if( HttpHeaderIs(zLine,nLine,"Content-Type:",sizeof("Content-Type:")-1) ){` |
|   136 |  980 | `		i = sizeof("Content-Type:")-1;` |
|   340 |  981 | `		while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|   136 |  982 | `			i++;` |
|   ! 0 |  983 | `		}` |
|   204 |  984 | `		PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_MIME_TYPE_IS,` |
|   136 |  985 | `			PHL_STREAM_NOTIFY_SEVERITY_INFO,&zLine[i],(int)(nLine - i),0,0,0);` |
|   136 |  986 | `		return;` |
|     - |  987 | `	}` |
|   678 |  988 | `	if( HttpHeaderIs(zLine,nLine,"Content-Length:",sizeof("Content-Length:")-1) ){` |
|   276 |  989 | `		sxu64 iSize = 0;` |
|   276 |  990 | `		sxu32 nDigit = 0;` |
|   276 |  991 | `		int bOver = 0;` |
|   276 |  992 | `		i = sizeof("Content-Length:")-1;` |
|   692 |  993 | `		while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|   278 |  994 | `			i++;` |
|   ! 0 |  995 | `		}` |
|   780 |  996 | `		for( ; i < nLine ; ++i ){` |
|   508 |  997 | `			if( zLine[i] < '0' \|\| zLine[i] > '9' ){` |
|     4 |  998 | `				return; /* not a size php would announce */` |
|     - |  999 | `			}` |
|   504 | 1000 | `			if( !bOver && iSize <= ((sxu64)SXI64_HIGH - (sxu64)(zLine[i] - '0')) / 10 ){` |
|   500 | 1001 | `				iSize = iSize * 10 + (sxu64)(zLine[i] - '0');` |
|   250 | 1002 | `			}else{` |
|     - | 1003 | `				/* php reads this one with strtol, which SATURATES: a run too` |
|     - | 1004 | `				 * wide for the clock is the ceiling itself, not a wrap and not` |
|     - | 1005 | `				 * a refusal. */` |
|     4 | 1006 | `				bOver = 1;` |
|     - | 1007 | `			}` |
|   504 | 1008 | `			nDigit++;` |
|   252 | 1009 | `		}` |
|   272 | 1010 | `		if( nDigit < 1 ){` |
|     2 | 1011 | `			return;` |
|     - | 1012 | `		}` |
|   270 | 1013 | `		if( bOver ){` |
|     2 | 1014 | `			iSize = (sxu64)SXI64_HIGH;` |
|     1 | 1015 | `		}` |
|   270 | 1016 | `		pH->iFileSize = (sxi64)iSize;` |
|   405 | 1017 | `		PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_FILE_SIZE_IS,` |
|   135 | 1018 | `			PHL_STREAM_NOTIFY_SEVERITY_INFO,zLine,(int)nLine,0,0,(sxi64)iSize);` |
|   135 | 1019 | `	}` |
|   407 | 1020 | `}` |
|     - | 1021 | `/*` |
|     - | 1022 | ` * Read one whole response: its status line, its header block, and whatever of` |
|     - | 1023 | ` * the body arrived with them.` |
|     - | 1024 | ` *` |
|     - | 1025 | `` * Answers the status code php would compute -- `atoi(line + 9)`, which is 0 for`` |
|     - | 1026 | ` * anything shorter than ten bytes and for a line whose digits sit elsewhere --` |
|     - | 1027 | ` * and fills *pzStatus with the raw status line, which is what php's failure` |
|     - | 1028 | ` * names. A 1xx is swallowed here and the next response read in its place, along` |
|     - | 1029 | ` * with the headers it had already recorded.` |
|     - | 1030 | ` */` |
|   300 | 1031 | `static int HttpReadResponse(http_private *pH,SyBlob *pHdrs,SyBlob *pStatus,` |
|     - | 1032 | `	SyBlob *pLocation,int *pbHasLocation,int *pbAnyLine)` |
|   ! 0 | 1033 | `{` |
|     - | 1034 | `	SyBlob sLine,sNext;` |
|   300 | 1035 | `	int iCode = 0,nTerm = 0,nNextTerm = 0,bStale,bDiscard = 0;` |
|   300 | 1036 | `	SyBlobInit(&sLine,&pH->pVm->sAllocator);` |
|   300 | 1037 | `	SyBlobInit(&sNext,&pH->pVm->sAllocator);` |
|   300 | 1038 | `	*pbHasLocation = 0;` |
|   154 | 1039 | `	for(;;){` |
|   308 | 1040 | `		sxu32 nHdrStart = SyBlobLength(pHdrs);` |
|   308 | 1041 | `		bStale = 0;` |
|   308 | 1042 | `		pH->iFileSize = 0;` |
|     - | 1043 | `		/* php reads every line into ONE buffer and does not clear it when the` |
|     - | 1044 | `		 * read fails, so a connection that ends where a status line was due` |
|     - | 1045 | `		 * reports the LAST line it did read -- which after an informational` |
|     - | 1046 | `		 * response is that response's own blank line. */` |
|   308 | 1047 | `		if( HttpReadLine(pH,&sNext,&nNextTerm) ){` |
|   304 | 1048 | `			SyBlobReset(&sLine);` |
|   304 | 1049 | `			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|   304 | 1050 | `			nTerm = nNextTerm;` |
|   304 | 1051 | `			if( SyBlobLength(&sLine) > HTTP_STATUS_LINE_MAX ){` |
|     - | 1052 | `				/* php reads the STATUS line into a 128-byte stack buffer and` |
|     - | 1053 | `				 * throws the rest of that line away -- a header line, read into` |
|     - | 1054 | `				 * a buffer it allocates, has no such cap. A line that hit the` |
|     - | 1055 | `				 * cap carried no terminator either. */` |
|   ! 0 | 1056 | `				sLine.nByte = HTTP_STATUS_LINE_MAX;` |
|   ! 0 | 1057 | `				nTerm = 0;` |
|   ! 0 | 1058 | `			}` |
|   304 | 1059 | `			*pbAnyLine = 1;` |
|   152 | 1060 | `		}else{` |
|     4 | 1061 | `			bStale = 1;` |
|     4 | 1062 | `			if( !*pbAnyLine ){` |
|     - | 1063 | `				/* Nothing was ever read: there is no line to report and php's` |
|     - | 1064 | `				 * header array stays EMPTY rather than gaining a blank entry. */` |
|     2 | 1065 | `				break;` |
|     - | 1066 | `			}` |
|     - | 1067 | `		}` |
|     - | 1068 | `		/* php's refusal prints the status line AS READ, terminator and all --` |
|     - | 1069 | `		 * so a reply that simply ran out of bytes is named with no newline` |
|     - | 1070 | `		 * after it, and an ordinary one keeps the CRLF it arrived with. */` |
|   306 | 1071 | `		SyBlobReset(pStatus);` |
|   306 | 1072 | `		SyBlobAppend(pStatus,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   306 | 1073 | `		if( nTerm == 2 ){` |
|   302 | 1074 | `			SyBlobAppend(pStatus,"\r\n",sizeof("\r\n")-1);` |
|   155 | 1075 | `		}else if( nTerm == 1 ){` |
|     2 | 1076 | `			SyBlobAppend(pStatus,"\n",1);` |
|     1 | 1077 | `		}` |
|   306 | 1078 | `		iCode = 0;` |
|   306 | 1079 | `		if( SyBlobLength(&sLine) > 9 ){` |
|   302 | 1080 | `			sxi32 iTmp = 0;` |
|   453 | 1081 | `			SyStrToInt32(&((const char *)SyBlobData(&sLine))[9],` |
|   302 | 1082 | `				SyBlobLength(&sLine) - 9,(void *)&iTmp,0);` |
|   302 | 1083 | `			iCode = (int)iTmp;` |
|   151 | 1084 | `		}` |
|   306 | 1085 | `		SyBlobAppend(pHdrs,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   306 | 1086 | `		SyBlobAppend(pHdrs,"\n",1);` |
|   306 | 1087 | `		bDiscard = iCode >= 100 && iCode < 200 && iCode != 101;` |
|   306 | 1088 | `		if( !bDiscard && (iCode < 200 \|\| iCode >= 400) ){` |
|     - | 1089 | `			/* php tells the notifier about a status it will not open for as` |
|     - | 1090 | `			 * soon as it has READ it -- before the header block, and whatever` |
|     - | 1091 | ``			 * `ignore_errors` says, so the callback hears about a 404 the`` |
|     - | 1092 | `			 * caller went on to read anyway. The text is the status line as it` |
|     - | 1093 | `			 * arrived, terminator and all, and the code is beside it: 0 for a` |
|     - | 1094 | `			 * line whose digits are not where php looks. An informational` |
|     - | 1095 | `			 * response php DISCARDS is not one of these; the line it fails on` |
|     - | 1096 | `			 * after one is the blank line it kept. */` |
|    36 | 1097 | `			PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_FAILURE,` |
|    24 | 1098 | `				PHL_STREAM_NOTIFY_SEVERITY_ERR,(const char *)SyBlobData(pStatus),` |
|    24 | 1099 | `				(int)SyBlobLength(pStatus),iCode,0,0);` |
|    12 | 1100 | `		}` |
|   306 | 1101 | `		if( bStale ){` |
|     2 | 1102 | `			break;` |
|     - | 1103 | `		}` |
|     - | 1104 | `		/* The header block. A continuation line (one opening with a space or a` |
|     - | 1105 | `		 * tab) belongs to the header before it, joined by a single space. */` |
|   984 | 1106 | `		for(;;){` |
|     - | 1107 | `			const char *zLine;` |
|     - | 1108 | `			sxu32 nLine,nColon;` |
|  1144 | 1109 | `			if( HttpReadLine(pH,&sNext,&nNextTerm) == 0 ){` |
|     4 | 1110 | `				break;` |
|     - | 1111 | `			}` |
|  1140 | 1112 | `			SyBlobReset(&sLine);` |
|  1140 | 1113 | `			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|  1140 | 1114 | `			nTerm = nNextTerm;` |
|  1140 | 1115 | `			zLine = (const char *)SyBlobData(&sLine);` |
|  1140 | 1116 | `			nLine = SyBlobLength(&sLine);` |
|     - | 1117 | `			/* php right-trims a HEADER line -- and only a header line: the` |
|     - | 1118 | `			 * status line above keeps whatever trailing blanks it arrived` |
|     - | 1119 | `			 * with. Done after the length is taken, so a line of nothing but` |
|     - | 1120 | `			 * blanks is still the CONTINUATION its first byte makes it. */` |
|  1585 | 1121 | `			while( nLine > 0 && (zLine[nLine-1] == ' ' \|\| zLine[nLine-1] == '\t')` |
|  1015 | 1122 | `			    && !(zLine[0] == ' ' \|\| zLine[0] == '\t') ){` |
|    16 | 1123 | `				nLine--;` |
|   ! 0 | 1124 | `			}` |
|  1140 | 1125 | `			if( nLine < 1 ){` |
|   298 | 1126 | `				break; /* the blank line: the body starts here */` |
|     - | 1127 | `			}` |
|   842 | 1128 | `			if( zLine[0] == ' ' \|\| zLine[0] == '\t' ){` |
|    10 | 1129 | `				sxu32 i = 0;` |
|    33 | 1130 | `				while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|    18 | 1131 | `					i++;` |
|   ! 0 | 1132 | `				}` |
|    15 | 1133 | `				while( nLine > i && (zLine[nLine-1] == ' ' \|\| zLine[nLine-1] == '\t') ){` |
|   ! 0 | 1134 | `					nLine--;` |
|   ! 0 | 1135 | `				}` |
|     - | 1136 | `				/* Back over the separator the previous line closed with: the` |
|     - | 1137 | `				 * two are ONE header, joined by a single space whatever the` |
|     - | 1138 | `				 * continuation was indented with. */` |
|    10 | 1139 | `				if( pHdrs->nByte > 0 ){` |
|    10 | 1140 | `					pHdrs->nByte--;` |
|     5 | 1141 | `				}` |
|    10 | 1142 | `				SyBlobAppend(pHdrs," ",1);` |
|    10 | 1143 | `				SyBlobAppend(pHdrs,&zLine[i],nLine - i);` |
|    10 | 1144 | `				SyBlobAppend(pHdrs,"\n",1);` |
|    10 | 1145 | `				continue;` |
|     - | 1146 | `			}` |
|  9896 | 1147 | `			for( nColon = 0 ; nColon < nLine && zLine[nColon] != ':' ; ++nColon ){` |
|     - | 1148 | `				;` |
|  4532 | 1149 | `			}` |
|   832 | 1150 | `			if( nColon >= nLine ){` |
|     - | 1151 | `				/* php refuses the whole response for a header line with no` |
|     - | 1152 | `				 * colon in it, and says so in exactly these words. */` |
|     2 | 1153 | `				HttpFail(pH->pVm,` |
|     - | 1154 | `					"HTTP invalid response format (no colon in header line)!",-1);` |
|     2 | 1155 | `				SyBlobRelease(&sLine);` |
|     2 | 1156 | `				SyBlobRelease(&sNext);` |
|     2 | 1157 | `				return -1;` |
|     - | 1158 | `			}` |
|   830 | 1159 | `			if( nColon == sizeof("Transfer-Encoding")-1` |
|   421 | 1160 | `			 && !pH->pVm->bHttpGetHeaders` |
|    10 | 1161 | `			 && SyStrnicmp(zLine,"Transfer-Encoding",sizeof("Transfer-Encoding")-1) == 0 ){` |
|     - | 1162 | `				sxu32 nOfft;` |
|     8 | 1163 | `				if( SyBlobSearch(&zLine[nColon],nLine - nColon,"chunked",` |
|     4 | 1164 | `						sizeof("chunked")-1,&nOfft) == SXRET_OK ){` |
|     - | 1165 | `					/* php frames the body with its dechunk filter and drops` |
|     - | 1166 | `					 * the header that asked for it: a script reading the` |
|     - | 1167 | `					 * response headers never sees this line. */` |
|     6 | 1168 | `					pH->bChunked = 1;` |
|     6 | 1169 | `					pH->iChunkLeft = -1;` |
|     6 | 1170 | `					continue;` |
|     - | 1171 | `				}` |
|     1 | 1172 | `			}` |
|   824 | 1173 | `			if( nColon == sizeof("Location")-1` |
|   438 | 1174 | `			 && SyStrnicmp(zLine,"Location",sizeof("Location")-1) == 0 ){` |
|     - | 1175 | `				/* php trims the value at BOTH ends before resolving it. */` |
|    52 | 1176 | `				sxu32 i = nColon + 1,nStop = nLine;` |
|   106 | 1177 | `				while( i < nStop && HttpIsTrimByte(zLine[i]) ){` |
|    54 | 1178 | `					i++;` |
|   ! 0 | 1179 | `				}` |
|    52 | 1180 | `				while( nStop > i && HttpIsTrimByte(zLine[nStop-1]) ){` |
|   ! 0 | 1181 | `					nStop--;` |
|   ! 0 | 1182 | `				}` |
|    52 | 1183 | `				SyBlobReset(pLocation);` |
|    52 | 1184 | `				SyBlobAppend(pLocation,&zLine[i],nStop - i);` |
|    52 | 1185 | `				*pbHasLocation = 1;` |
|    26 | 1186 | `			}` |
|   824 | 1187 | `			if( !bDiscard ){` |
|     - | 1188 | `				/* php drops an informational response WHOLE -- the headers it` |
|     - | 1189 | ``				 * carried are never looked at, so a `Content-Type` on a 100 is`` |
|     - | 1190 | `				 * not a mime type anybody is told about. */` |
|   814 | 1191 | `				HttpNotifyHeader(pH,zLine,nLine);` |
|   407 | 1192 | `			}` |
|   824 | 1193 | `			SyBlobAppend(pHdrs,zLine,nLine);` |
|   824 | 1194 | `			SyBlobAppend(pHdrs,"\n",1);` |
|   ! 0 | 1195 | `		}` |
|   302 | 1196 | `		if( bDiscard ){` |
|     - | 1197 | `			/* An informational response is not the answer: php drops it, drops` |
|     - | 1198 | `			 * the headers it collected for it, and reads the next one. 101 is` |
|     - | 1199 | `			 * php's one exception -- a protocol SWITCH is the last thing that` |
|     - | 1200 | `			 * will ever be spoken as HTTP on this connection, so there is no` |
|     - | 1201 | `			 * next response to read and php reports the 101 itself. */` |
|     8 | 1202 | `			pHdrs->nByte = nHdrStart;` |
|     8 | 1203 | `			pH->bChunked = 0;` |
|     8 | 1204 | `			*pbHasLocation = 0;` |
|     8 | 1205 | `			continue;` |
|     - | 1206 | `		}` |
|   294 | 1207 | `		break;` |
|   ! 0 | 1208 | `	}` |
|   298 | 1209 | `	SyBlobRelease(&sLine);` |
|   298 | 1210 | `	SyBlobRelease(&sNext);` |
|   298 | 1211 | `	return iCode;` |
|   150 | 1212 | `}` |
|     - | 1213 | `/* Release everything the handle owns; the handle itself goes with it. */` |
|   272 | 1214 | `static void HttpFree(http_private *pH)` |
|     1 | 1215 | `{` |
|   273 | 1216 | `	if( pH == 0 ){` |
|   ! 0 | 1217 | `		return;` |
|     - | 1218 | `	}` |
|   273 | 1219 | `	HttpDisconnect(pH);` |
|   273 | 1220 | `	SyBlobRelease(&pH->sRaw);` |
|   273 | 1221 | `	SyBlobRelease(&pH->sOut);` |
|   273 | 1222 | `	SyBlobRelease(&pH->sHdrs);` |
|   273 | 1223 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|   137 | 1224 | `}` |
|     - | 1225 | `/*` |
|     - | 1226 | ` * Ask an already-dialled proxy to open a tunnel to the origin, and read its` |
|     - | 1227 | ` * answer off.` |
|     - | 1228 | ` *` |
|     - | 1229 | ` * php's CONNECT is the shortest request it ever writes: the request LINE, an` |
|     - | 1230 | `` * optional `Proxy-Authorization:` lifted out of the script's own headers, and`` |
|     - | 1231 | `` * the blank line. No Host, no User-Agent, no `http` context option of any kind,`` |
|     - | 1232 | `` * and the version is a fixed `HTTP/1.0` rather than the one the exchange behind`` |
|     - | 1233 | ` * the tunnel will use.` |
|     - | 1234 | ` *` |
|     - | 1235 | ` * The reply is read and DROPPED -- php looks at neither the status line nor the` |
|     - | 1236 | ` * headers, so a proxy that refuses with 407 is told apart from one that agreed` |
|     - | 1237 | ` * only by the handshake that follows failing. Answers PH7_OK once the reply's` |
|     - | 1238 | ` * blank line has gone by, the caller naming the failure.` |
|     - | 1239 | ` */` |
|    16 | 1240 | `static int HttpTunnel(http_private *pH,SyhttpUri *pOrigin,phl_stream_ctx *pCtx)` |
|   ! 0 | 1241 | `{` |
|     - | 1242 | `	SyBlob sReq,sLine;` |
|    16 | 1243 | `	int rc = PH7_OK;` |
|    16 | 1244 | `	SyBlobInit(&sReq,&pH->pVm->sAllocator);` |
|    16 | 1245 | `	SyBlobInit(&sLine,&pH->pVm->sAllocator);` |
|    16 | 1246 | `	SyBlobAppend(&sReq,"CONNECT ",sizeof("CONNECT ")-1);` |
|    16 | 1247 | `	SyBlobAppend(&sReq,pOrigin->sHost.zString,pOrigin->sHost.nByte);` |
|    16 | 1248 | `	SyBlobFormat(&sReq,":%d",HttpUriPort(pOrigin,1));` |
|    16 | 1249 | `	SyBlobAppend(&sReq," HTTP/1.0\r\n",sizeof(" HTTP/1.0\r\n")-1);` |
|    16 | 1250 | `	HttpProxyAuth(HttpOpt(pCtx,"header"),&sReq);` |
|    16 | 1251 | `	SyBlobAppend(&sReq,"\r\n",sizeof("\r\n")-1);` |
|    16 | 1252 | `	if( HttpSendAll(pH,SyBlobData(&sReq),(int)SyBlobLength(&sReq)) != PH7_OK ){` |
|   ! 0 | 1253 | `		rc = -1;` |
|   ! 0 | 1254 | `	}else{` |
|    34 | 1255 | `		while( HttpReadLine(pH,&sLine,0) == 1 ){` |
|    34 | 1256 | `			if( SyBlobLength(&sLine) < 1 ){` |
|    16 | 1257 | `				break; /* the blank line: everything after it is the tunnel */` |
|     - | 1258 | `			}` |
|   ! 0 | 1259 | `		}` |
|     - | 1260 | `		/* Whatever the proxy wrote past that line is not this exchange's: the` |
|     - | 1261 | `		 * bytes from here on are TLS records, and the reader must start on` |
|     - | 1262 | `		 * them rather than on a leftover. */` |
|    16 | 1263 | `		SyBlobReset(&pH->sRaw);` |
|    16 | 1264 | `		pH->nRawOfft = 0;` |
|    16 | 1265 | `		pH->bEof = 0;` |
|     - | 1266 | `	}` |
|    16 | 1267 | `	SyBlobRelease(&sLine);` |
|    16 | 1268 | `	SyBlobRelease(&sReq);` |
|    16 | 1269 | `	return rc;` |
|   ! 0 | 1270 | `}` |
|     - | 1271 | `/* The connection this exchange runs over, or php's own failure text. The URI` |
|     - | 1272 | ` * dialed is not always the one being ASKED for -- a proxy moves the connection` |
|     - | 1273 | ` * -- so the host TLS is negotiated against is passed separately. */` |
|   312 | 1274 | `static int HttpConnect(http_private *pH,SyhttpUri *pUri,phl_stream_ctx *pCtx,` |
|     - | 1275 | `	SyString *pPeer,SyhttpUri *pTunnel)` |
|     1 | 1276 | `{` |
|     - | 1277 | `	SyBlob sHost;` |
|     - | 1278 | `	const char *zHost;` |
|   313 | 1279 | `	const char *zErr = "";` |
|   313 | 1280 | `	int iErrno = 0,iTimeoutMs,rc = -1;` |
|     - | 1281 | `	ph7_value *pOptV;` |
|     - | 1282 | `	ph7_socket sock;` |
|     - | 1283 | `	/* The resolver wants a NUL-terminated name, and a host is whatever the URL` |
|     - | 1284 | `	 * spelled -- no fixed buffer, because the failure below NAMES it. */` |
|   313 | 1285 | `	SyBlobInit(&sHost,&pH->pVm->sAllocator);` |
|   313 | 1286 | `	SyBlobAppend(&sHost,pUri->sHost.zString,pUri->sHost.nByte);` |
|   313 | 1287 | `	SyBlobNullAppend(&sHost);` |
|   313 | 1288 | `	zHost = (const char *)SyBlobData(&sHost);` |
|     - | 1289 | ``	/* php bounds the exchange by the `timeout` option, and by`` |
|     - | 1290 | `	 * default_socket_timeout when the script named none. */` |
|   313 | 1291 | `	iTimeoutMs = (int)(PH7_VmIniGetInt(pH->pVm,"default_socket_timeout",60) * 1000);` |
|   313 | 1292 | `	pOptV = HttpOpt(pCtx,"timeout");` |
|   313 | 1293 | `	if( pOptV ){` |
|   ! 0 | 1294 | `		double rSec = HttpOptReal(pH->pVm,pOptV,0);` |
|   ! 0 | 1295 | `		iTimeoutMs = rSec > 0 ? (int)(rSec * 1000) : 0;` |
|   ! 0 | 1296 | `	}` |
|     - | 1297 | `	/* A proxy is dialled in the clear whatever the origin's scheme is -- the` |
|     - | 1298 | `	 * TLS goes INSIDE the tunnel -- so its address defaults to the plain` |
|     - | 1299 | `	 * port rather than to the origin scheme's. */` |
|   313 | 1300 | `	sock = PH7_NetConnect(zHost,HttpUriPort(pUri,pTunnel ? 0 : pH->bTls),` |
|   156 | 1301 | `		iTimeoutMs,0,0,0,&iErrno,&zErr);` |
|   313 | 1302 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    11 | 1303 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|     - | 1304 | `			SyBlob sMsg;` |
|     - | 1305 | `			SyString sCaller;` |
|   ! 0 | 1306 | `			SyBlobInit(&sMsg,&pH->pVm->sAllocator);` |
|   ! 0 | 1307 | `			SyBlobFormat(&sMsg,` |
|     - | 1308 | `				"php_network_getaddresses: getaddrinfo for %z failed: Name or service not known",` |
|   ! 0 | 1309 | `				&pUri->sHost);` |
|   ! 0 | 1310 | `			SyBlobNullAppend(&sMsg);` |
|   ! 0 | 1311 | `			HttpFail(pH->pVm,(const char *)SyBlobData(&sMsg),(int)SyBlobLength(&sMsg)-1);` |
|     - | 1312 | `			/* php says this one TWICE: the resolver's own failure under the` |
|     - | 1313 | `			 * calling function's name, and then the caller's failed-open` |
|     - | 1314 | `			 * sentence carrying it as the reason. A refused CONNECT is only the` |
|     - | 1315 | `			 * second -- the resolver is where php has the extra warning. */` |
|   ! 0 | 1316 | `			SyStringInitFromBuf(&sCaller,pH->pVm->zOpenCaller ? pH->pVm->zOpenCaller : "",` |
|     - | 1317 | `				pH->pVm->zOpenCaller ? SyStrlen(pH->pVm->zOpenCaller) : 0);` |
|   ! 0 | 1318 | `			PH7_VmThrowError(pH->pVm,pH->pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_WARNING,` |
|   ! 0 | 1319 | `				(const char *)SyBlobData(&sMsg));` |
|   ! 0 | 1320 | `			SyBlobRelease(&sMsg);` |
|   ! 0 | 1321 | `		}else{` |
|    11 | 1322 | `			HttpFail(pH->pVm,zErr && zErr[0] ? zErr : "Connection refused",-1);` |
|     - | 1323 | `		}` |
|     6 | 1324 | `	}else{` |
|   302 | 1325 | `		pH->sock = sock;` |
|   302 | 1326 | `		pH->bEof = 0;` |
|   302 | 1327 | `		rc = PH7_OK;` |
|   302 | 1328 | `		if( pTunnel && HttpTunnel(pH,pTunnel,pCtx) != PH7_OK ){` |
|   ! 0 | 1329 | `			HttpFail(pH->pVm,HTTP_PROXY_TUNNEL_ERR,-1);` |
|   ! 0 | 1330 | `			HttpDisconnect(pH);` |
|   ! 0 | 1331 | `			rc = -1;` |
|   ! 0 | 1332 | `		}` |
|     - | 1333 | `#ifdef PH7_ENABLE_OPENSSL` |
|   302 | 1334 | `		if( rc == PH7_OK && pH->bTls ){` |
|     - | 1335 | ``			/* php opens `ssl://host:port` where this opens `tcp://`, so the`` |
|     - | 1336 | ``			 * handshake -- the `ssl` context options, the name checked against`` |
|     - | 1337 | `			 * the certificate, the capture written back onto the context --` |
|     - | 1338 | `			 * is the TRANSPORT's, and its refusal is the reason the failed` |
|     - | 1339 | `			 * open reports. */` |
|     - | 1340 | `			SyBlob sPeer;` |
|     - | 1341 | `			char zSslErr[512];` |
|    26 | 1342 | `			zSslErr[0] = 0;` |
|    26 | 1343 | `			SyBlobInit(&sPeer,&pH->pVm->sAllocator);` |
|    26 | 1344 | `			SyBlobAppend(&sPeer,pPeer->zString,pPeer->nByte);` |
|    26 | 1345 | `			SyBlobNullAppend(&sPeer);` |
|    39 | 1346 | `			if( PH7_SslClientHandshake(pH->pVm,pH->sock,pCtx,` |
|    26 | 1347 | `					(const char *)SyBlobData(&sPeer),&pH->pSsl,&pH->pSslCtx,` |
|    26 | 1348 | `					zSslErr,(int)sizeof(zSslErr)) != PH7_OK ){` |
|     - | 1349 | `				/* Inside a tunnel php names the PROXY rather than the` |
|     - | 1350 | `				 * handshake: from the wrapper's side the whole hop through it` |
|     - | 1351 | `				 * is the thing that did not come up. */` |
|     2 | 1352 | `				HttpFail(pH->pVm,pTunnel ? HTTP_PROXY_TUNNEL_ERR : zSslErr,-1);` |
|     2 | 1353 | `				HttpDisconnect(pH);` |
|     2 | 1354 | `				rc = -1;` |
|     1 | 1355 | `			}` |
|    26 | 1356 | `			SyBlobRelease(&sPeer);` |
|    13 | 1357 | `		}` |
|     - | 1358 | `#else` |
|     - | 1359 | `		SXUNUSED(pPeer);` |
|     - | 1360 | `#endif` |
|   302 | 1361 | `		if( rc == PH7_OK ){` |
|     - | 1362 | `			/* php reports the connection itself, per HOP -- and only for one` |
|     - | 1363 | `			 * that was MADE: a refused dial, a name that does not resolve, a` |
|     - | 1364 | `			 * tunnel the proxy would not open and a handshake that failed` |
|     - | 1365 | `			 * notify nothing at all. */` |
|   300 | 1366 | `			PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_CONNECT,` |
|     - | 1367 | `				PHL_STREAM_NOTIFY_SEVERITY_INFO,0,0,0,0,0);` |
|   150 | 1368 | `		}` |
|     - | 1369 | `	}` |
|   313 | 1370 | `	SyBlobRelease(&sHost);` |
|   313 | 1371 | `	return rc;` |
|     1 | 1372 | `}` |
|     - | 1373 | `/*` |
|     - | 1374 | ` * Walk the exchange: connect, ask, read, and follow whatever redirects php` |
|     - | 1375 | ` * would follow. Answers PH7_OK with the handle sitting on the body, or -1 with` |
|     - | 1376 | ` * the failure already named.` |
|     - | 1377 | ` */` |
|   272 | 1378 | `static int HttpRun(http_private *pH,const char *zUrl,int nUrl,phl_stream_ctx *pCtx)` |
|     1 | 1379 | `{` |
|     - | 1380 | `	SyBlob sUrl,sReq,sStatus,sLoc,sTarget,sMethod,sBody;` |
|     - | 1381 | `	/* php's wrapper follows a redirect by CALLING ITSELF, and each frame arms` |
|     - | 1382 | `	 * the progress counter with its OWN announced size once the inner one has` |
|     - | 1383 | `	 * come back -- so a script watching a two-hop exchange is told the size` |
|     - | 1384 | `	 * twice, innermost first. This is that stack of frames. */` |
|     - | 1385 | `	SySet sHopSize;` |
|     - | 1386 | `	ph7_value *pOptV;` |
|   273 | 1387 | `	int iRedirLeft = HTTP_MAX_REDIRECTS,bFollow = 1,bIgnoreErr = 0,rc = -1;` |
|   273 | 1388 | `	int bAnyLine = 0,bAnswered = 0;` |
|   273 | 1389 | `	const char *zBody = 0;` |
|   273 | 1390 | `	int nBody = 0;` |
|   273 | 1391 | `	ph7_vm *pVm = pH->pVm;` |
|   273 | 1392 | `	SyBlobInit(&sUrl,&pVm->sAllocator);` |
|   273 | 1393 | `	SyBlobInit(&sReq,&pVm->sAllocator);` |
|   273 | 1394 | `	SyBlobInit(&sStatus,&pVm->sAllocator);` |
|   273 | 1395 | `	SyBlobInit(&sLoc,&pVm->sAllocator);` |
|   273 | 1396 | `	SyBlobInit(&sTarget,&pVm->sAllocator);` |
|   273 | 1397 | `	SyBlobInit(&sMethod,&pVm->sAllocator);` |
|   273 | 1398 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   273 | 1399 | `	SySetInit(&sHopSize,&pVm->sAllocator,sizeof(sxi64));` |
|   273 | 1400 | `	SyBlobAppend(&sUrl,zUrl,(sxu32)nUrl);` |
|     - | 1401 | `	/* php drops the previous exchange's headers before this one starts, so an` |
|     - | 1402 | `	 * open that never reaches a response leaves http_get_last_response_headers()` |
|     - | 1403 | `	 * answering NULL rather than the set before it -- while the CALLER's own` |
|     - | 1404 | `	 * $http_response_header, which is only written when there is something to` |
|     - | 1405 | `	 * write, keeps whatever it held. */` |
|   273 | 1406 | `	PH7_HttpClearResponseHeaders(pVm);` |
|   273 | 1407 | `	pOptV = HttpOpt(pCtx,"max_redirects");` |
|   273 | 1408 | `	if( pOptV ){` |
|    10 | 1409 | `		iRedirLeft = (int)HttpOptInt(pVm,pOptV,HTTP_MAX_REDIRECTS);` |
|     5 | 1410 | `	}` |
|   273 | 1411 | `	pOptV = HttpOpt(pCtx,"follow_location");` |
|   273 | 1412 | `	if( pOptV ){` |
|     4 | 1413 | `		bFollow = HttpOptInt(pVm,pOptV,1) != 0;` |
|     2 | 1414 | `	}` |
|   273 | 1415 | `	pOptV = HttpOpt(pCtx,"ignore_errors");` |
|   273 | 1416 | `	if( pOptV ){` |
|     4 | 1417 | `		bIgnoreErr = HttpOptBool(pVm,pOptV,0);` |
|     2 | 1418 | `	}` |
|   273 | 1419 | `	if( pVm->bHttpGetHeaders ){` |
|     - | 1420 | `		/* get_headers() asks for the HEADERS of whatever answered, so php opens` |
|     - | 1421 | `		 * with this on whatever the caller's context said. */` |
|    36 | 1422 | `		bIgnoreErr = 1;` |
|    18 | 1423 | `	}` |
|   273 | 1424 | `	pOptV = HttpOpt(pCtx,"method");` |
|   273 | 1425 | `	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){` |
|    16 | 1426 | `		SyBlobAppend(&sMethod,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|     8 | 1427 | `	}` |
|   273 | 1428 | `	if( SyBlobLength(&sMethod) < 1 ){` |
|   257 | 1429 | `		SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);` |
|   128 | 1430 | `	}` |
|   273 | 1431 | `	pOptV = HttpOpt(pCtx,"content");` |
|   273 | 1432 | `	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){` |
|     - | 1433 | `		/* COPIED, not pointed at: the exchange now runs userland code between` |
|     - | 1434 | ``		 * here and the request it builds, and a `notification` callback that`` |
|     - | 1435 | `		 * sets this very option would free the bytes under us. */` |
|    12 | 1436 | `		SyBlobAppend(&sBody,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|    12 | 1437 | `		zBody = (const char *)SyBlobData(&sBody);` |
|    12 | 1438 | `		nBody = (int)SyBlobLength(&sBody);` |
|     6 | 1439 | `	}` |
|   156 | 1440 | `	for(;;){` |
|     - | 1441 | `		SyhttpUri sUri;` |
|   313 | 1442 | `		int iCode,bHasLoc = 0,bFullUri = 0,bTunnel = 0;` |
|   468 | 1443 | `		if( PH7_VmHttpSplitURI(&sUri,(const char *)SyBlobData(&sUrl),` |
|   312 | 1444 | `				SyBlobLength(&sUrl)) != SXRET_OK` |
|   313 | 1445 | `		 \|\| SyStringLength(&sUri.sHost) < 1 ){` |
|   ! 0 | 1446 | `			HttpFail(pVm,"Unable to parse the URL",-1);` |
|   ! 0 | 1447 | `			goto done;` |
|     - | 1448 | `		}` |
|     - | 1449 | `		/* Every hop reads its own scheme: a redirect that crosses from` |
|     - | 1450 | `		 * https:// to http:// (or back) changes the port default, the Host` |
|     - | 1451 | `		 * header's rule and whether TLS is spoken at all -- php rebuilds the` |
|     - | 1452 | `		 * whole request from the URL it is about to open, and so does this. */` |
|   482 | 1453 | `		pH->bTls = (SyStringLength(&sUri.sScheme) > 4` |
|   312 | 1454 | `			&& SyStrnicmp(sUri.sScheme.zString,"https",sizeof("https")-1) == 0) ? 1 : 0;` |
|   313 | 1455 | `		pOptV = HttpOpt(pCtx,"request_fulluri");` |
|   313 | 1456 | `		bFullUri = HttpOptBool(pVm,pOptV,0);` |
|   469 | 1457 | `		HttpRequestTarget(&sUri,(const char *)SyBlobData(&sUrl),` |
|   312 | 1458 | `			(int)SyBlobLength(&sUrl),bFullUri,&sTarget);` |
|     - | 1459 | ``		/* php's `proxy` option moves the CONNECTION and leaves everything else`` |
|     - | 1460 | `		 * alone: the request line and the Host header still describe the` |
|     - | 1461 | `		 * origin server. The address is a transport URL of its own. */` |
|   313 | 1462 | `		pOptV = HttpOpt(pCtx,"proxy");` |
|   313 | 1463 | `		bTunnel = 0;` |
|   322 | 1464 | `		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){` |
|     - | 1465 | `			SyhttpUri sProxy;` |
|    30 | 1466 | `			if( PH7_VmHttpSplitURI(&sProxy,(const char *)SyBlobData(&pOptV->sBlob),` |
|    20 | 1467 | `					SyBlobLength(&pOptV->sBlob)) == SXRET_OK` |
|    20 | 1468 | `			 && SyStringLength(&sProxy.sHost) > 0 ){` |
|     - | 1469 | `				/* An https:// origin is reached by TUNNELLING: a CONNECT` |
|     - | 1470 | `				 * exchange with the proxy first, and the handshake -- against` |
|     - | 1471 | `				 * the ORIGIN's name, not the proxy's -- inside it. A plain` |
|     - | 1472 | `				 * origin needs none of that; the proxy is simply where the` |
|     - | 1473 | `				 * request goes. */` |
|    20 | 1474 | `				bTunnel = pH->bTls;` |
|    30 | 1475 | `				if( HttpConnect(pH,&sProxy,pCtx,&sUri.sHost,` |
|    10 | 1476 | `						bTunnel ? &sUri : 0) != PH7_OK ){` |
|     2 | 1477 | `					goto done;` |
|     - | 1478 | `				}` |
|     9 | 1479 | `			}else{` |
|   ! 0 | 1480 | `				HttpFail(pVm,"Unable to parse the proxy address",-1);` |
|   ! 0 | 1481 | `				goto done;` |
|   ! 0 | 1482 | `			}` |
|   302 | 1483 | `		}else if( HttpConnect(pH,&sUri,pCtx,&sUri.sHost,0) != PH7_OK ){` |
|    11 | 1484 | `			goto done;` |
|     - | 1485 | `		}` |
|   300 | 1486 | `		SyBlobReset(&sReq);` |
|   450 | 1487 | `		HttpBuildRequest(pVm,pCtx,&sUri,pH->bTls,(const char *)SyBlobData(&sTarget),` |
|   300 | 1488 | `			(int)SyBlobLength(&sTarget),&sReq,` |
|   300 | 1489 | `			(const char *)SyBlobData(&sMethod),(int)SyBlobLength(&sMethod),zBody,nBody,` |
|   150 | 1490 | `			bTunnel);` |
|   300 | 1491 | `		if( HttpSendAll(pH,SyBlobData(&sReq),(int)SyBlobLength(&sReq)) != PH7_OK ){` |
|   ! 0 | 1492 | `			HttpFail(pVm,"Connection refused",-1);` |
|   ! 0 | 1493 | `			goto done;` |
|     - | 1494 | `		}` |
|     - | 1495 | `		/* php counts what the STREAM moved, and a write moves as much as a` |
|     - | 1496 | `		 * read: the request is on the counter too. It shows only for a context` |
|     - | 1497 | `		 * whose counter a previous exchange already armed, since this one's` |
|     - | 1498 | `		 * has not been armed yet. */` |
|   300 | 1499 | `		PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)SyBlobLength(&sReq));` |
|   300 | 1500 | `		bAnswered = 1;` |
|   300 | 1501 | `		iCode = HttpReadResponse(pH,&pH->sHdrs,&sStatus,&sLoc,&bHasLoc,&bAnyLine);` |
|   300 | 1502 | `		if( iCode < 0 ){` |
|     2 | 1503 | `			goto done; /* the malformed-header refusal named itself */` |
|     - | 1504 | `		}` |
|   298 | 1505 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow ){` |
|     - | 1506 | `			/* Announced BEFORE the count is spent, so the last hop a limit` |
|     - | 1507 | `			 * allows still reports where it was being sent -- and the address` |
|     - | 1508 | `			 * reported is the one the server WROTE, not the one it resolves` |
|     - | 1509 | `			 * to. */` |
|    72 | 1510 | `			PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_REDIRECTED,` |
|    48 | 1511 | `				PHL_STREAM_NOTIFY_SEVERITY_INFO,(const char *)SyBlobData(&sLoc),` |
|    48 | 1512 | `				(int)SyBlobLength(&sLoc),0,0,0);` |
|    24 | 1513 | `		}` |
|   298 | 1514 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && --iRedirLeft >= 1 ){` |
|     - | 1515 | `			SyBlob sNext;` |
|    40 | 1516 | `			sxi64 iHop = pH->iFileSize;` |
|    40 | 1517 | `			SySetPut(&sHopSize,(const void *)&iHop);` |
|    40 | 1518 | `			SyBlobInit(&sNext,&pVm->sAllocator);` |
|    60 | 1519 | `			HttpResolveLocation(&sUri,pH->bTls,(const char *)SyBlobData(&sLoc),` |
|    20 | 1520 | `				SyBlobLength(&sLoc),&sNext);` |
|    40 | 1521 | `			SyBlobReset(&sUrl);` |
|    40 | 1522 | `			SyBlobAppend(&sUrl,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|    40 | 1523 | `			SyBlobRelease(&sNext);` |
|     - | 1524 | `			/* php keeps the method and the body only for the two codes that` |
|     - | 1525 | `			 * promise them; every older redirect becomes a bodiless GET. */` |
|    40 | 1526 | `			if( iCode != 307 && iCode != 308 ){` |
|    36 | 1527 | `				SyBlobReset(&sMethod);` |
|    36 | 1528 | `				SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);` |
|    36 | 1529 | `				zBody = 0;` |
|    36 | 1530 | `				nBody = 0;` |
|    18 | 1531 | `			}` |
|    40 | 1532 | `			HttpDisconnect(pH);` |
|    40 | 1533 | `			SyBlobReset(&pH->sRaw);` |
|    40 | 1534 | `			pH->nRawOfft = 0;` |
|    40 | 1535 | `			SyBlobReset(&pH->sOut);` |
|    40 | 1536 | `			pH->nOutOfft = 0;` |
|    40 | 1537 | `			pH->bEof = 0;` |
|    40 | 1538 | `			pH->bChunked = 0;` |
|    40 | 1539 | `			pH->bChunkDone = 0;` |
|    40 | 1540 | `			pH->iChunkLeft = -1;` |
|    40 | 1541 | `			continue;` |
|     - | 1542 | `		}` |
|   258 | 1543 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && !pVm->bHttpGetHeaders ){` |
|     - | 1544 | `			/* The follow above declined because the count ran out. php calls that` |
|     - | 1545 | `			 * a failed open -- except for a headers-only one (get_headers()),` |
|     - | 1546 | `			 * which stops where it is and answers the headers it collected,` |
|     - | 1547 | `			 * silently. */` |
|     6 | 1548 | `			HttpFail(pVm,"Redirection limit reached, aborting",-1);` |
|     6 | 1549 | `			goto done;` |
|     - | 1550 | `		}` |
|   252 | 1551 | `		if( (iCode < 200 \|\| iCode >= 400) && !bIgnoreErr ){` |
|     - | 1552 | `			/* php's own sentence, and the status LINE it carries keeps the` |
|     - | 1553 | `			 * terminator it was read with. */` |
|     - | 1554 | `			char zMsg[320];` |
|    18 | 1555 | `			if( bAnyLine ){` |
|    24 | 1556 | `				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed! %.*s",` |
|    16 | 1557 | `					(int)SyBlobLength(&sStatus),(const char *)SyBlobData(&sStatus));` |
|     8 | 1558 | `			}else{` |
|     - | 1559 | `				/* Nothing came back at all: php names no line, and not even the` |
|     - | 1560 | `				 * space that would separate one. */` |
|     2 | 1561 | `				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed!");` |
|     - | 1562 | `			}` |
|    18 | 1563 | `			HttpFail(pVm,zMsg,-1);` |
|    18 | 1564 | `			goto done;` |
|     - | 1565 | `		}` |
|   234 | 1566 | `		rc = PH7_OK;` |
|   234 | 1567 | `		break;` |
|   ! 0 | 1568 | `	}` |
|   351 | 1569 | `	if( rc == PH7_OK ){` |
|     - | 1570 | `		/* The innermost frame arms the counter and CREDITS what came in with` |
|     - | 1571 | `		 * the headers; then every frame the redirects opened does the same on` |
|     - | 1572 | `		 * its way out, with the size IT announced. A chunked body is credited` |
|     - | 1573 | `		 * by the decoder, which is what makes the number the bytes a script` |
|     - | 1574 | `		 * will read rather than the framing they arrived in. */` |
|   234 | 1575 | `		sxi64 *aHop = (sxi64 *)SySetBasePtr(&sHopSize);` |
|   234 | 1576 | `		sxu32 i = SySetUsed(&sHopSize);` |
|   234 | 1577 | `		pH->bBody = 1;` |
|   234 | 1578 | `		PH7_StreamCtxProgressInit(pH->pCtx,pH->iFileSize);` |
|   234 | 1579 | `		if( pH->bChunked ){` |
|     6 | 1580 | `			PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)HttpDecodeBuffered(pH));` |
|     3 | 1581 | `		}else{` |
|   228 | 1582 | `			PH7_StreamCtxProgressAdd(pH->pCtx,HttpBuffered(pH));` |
|     - | 1583 | `		}` |
|   274 | 1584 | `		while( i > 0 ){` |
|    40 | 1585 | `			i--;` |
|    40 | 1586 | `			PH7_StreamCtxProgressInit(pH->pCtx,aHop[i]);` |
|    40 | 1587 | `			PH7_StreamCtxProgressAdd(pH->pCtx,HttpBuffered(pH));` |
|   ! 0 | 1588 | `		}` |
|   117 | 1589 | `	}` |
|   ! 0 | 1590 | `done:` |
|     - | 1591 | `	/* The headers belong to the SCRIPT whether the open worked or not: php` |
|     - | 1592 | `	 * fills $http_response_header for a 404 exactly as it does for a 200, and` |
|     - | 1593 | `	 * leaves it untouched for an exchange that never reached a response. */` |
|   273 | 1594 | `	if( bAnswered ){` |
|   260 | 1595 | `		PH7_HttpPublishHeaders(pVm,&pH->sHdrs);` |
|   130 | 1596 | `	}` |
|   273 | 1597 | `	SyBlobRelease(&sUrl);` |
|   273 | 1598 | `	SyBlobRelease(&sReq);` |
|   273 | 1599 | `	SyBlobRelease(&sStatus);` |
|   273 | 1600 | `	SyBlobRelease(&sLoc);` |
|   273 | 1601 | `	SyBlobRelease(&sTarget);` |
|   273 | 1602 | `	SyBlobRelease(&sMethod);` |
|   273 | 1603 | `	SyBlobRelease(&sBody);` |
|   273 | 1604 | `	SySetRelease(&sHopSize);` |
|   273 | 1605 | `	return rc;` |
|     1 | 1606 | `}` |
|     - | 1607 | `/* ------------------------------------------------------------------------- */` |
|     - | 1608 | `/* The device                                                                  */` |
|     - | 1609 | `/* ------------------------------------------------------------------------- */` |
|   290 | 1610 | `static int HttpOpenScheme(const char *zName,int iMode,ph7_value *pResource,` |
|     - | 1611 | `	void **ppHandle,int bTls)` |
|     1 | 1612 | `{` |
|   291 | 1613 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     - | 1614 | `	http_private *pH;` |
|     - | 1615 | `	SyBlob sUrl;` |
|     - | 1616 | `	int rc;` |
|   291 | 1617 | `	if( pVm == 0 ){` |
|   ! 0 | 1618 | `		return -1;` |
|     - | 1619 | `	}` |
|   290 | 1620 | `	if( (iMode & (PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_TRUNC` |
|   146 | 1621 | `			\|PH7_IO_OPEN_EXCL)) != 0 ){` |
|     - | 1622 | ``		/* php's screen here is TEXTUAL -- `strpbrk(mode, "awx+")` -- and these`` |
|     - | 1623 | `		 * four flags are what those four characters parse to, which is why` |
|     - | 1624 | ``		 * `c` (create-if-absent, and nothing else) is the one write-ish mode`` |
|     - | 1625 | `		 * php lets through: it connects and hands back a readable stream like` |
|     - | 1626 | `		 * any other. Said before a socket is made. */` |
|    19 | 1627 | `		PH7_StreamSetOpenError(pVm,"HTTP wrapper does not support writeable connections");` |
|    19 | 1628 | `		return -1;` |
|     - | 1629 | `	}` |
|   273 | 1630 | `	pH = (http_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(http_private));` |
|   273 | 1631 | `	if( pH == 0 ){` |
|   ! 0 | 1632 | `		return -1;` |
|     - | 1633 | `	}` |
|   273 | 1634 | `	SyZero(pH,sizeof(http_private));` |
|   273 | 1635 | `	pH->pVm = pVm;` |
|   273 | 1636 | `	pH->sock = PH7_NET_INVALID_SOCKET;` |
|   273 | 1637 | `	pH->iChunkLeft = -1;` |
|   273 | 1638 | `	SyBlobInit(&pH->sRaw,&pVm->sAllocator);` |
|   273 | 1639 | `	SyBlobInit(&pH->sOut,&pVm->sAllocator);` |
|   273 | 1640 | `	SyBlobInit(&pH->sHdrs,&pVm->sAllocator);` |
|     - | 1641 | `	/* The context this open was armed with, kept for the whole life of the` |
|     - | 1642 | `	 * handle: the progress notifications belong to the READS, which happen` |
|     - | 1643 | `	 * long after PH7_StreamOpenHandle has disarmed pVm->pOpenCtx. The VM owns` |
|     - | 1644 | `	 * every context it hands out, so the pointer outlives us. */` |
|   273 | 1645 | `	pH->pCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     - | 1646 | `	/* PH7_VmGetStreamDevice() hands the wrapper what is left after the scheme;` |
|     - | 1647 | `	 * every redirect below is resolved against a WHOLE url, so it goes back on. */` |
|   273 | 1648 | `	SyBlobInit(&sUrl,&pVm->sAllocator);` |
|   409 | 1649 | `	SyBlobAppend(&sUrl,bTls ? "https://" : "http://",` |
|   136 | 1650 | `		bTls ? sizeof("https://")-1 : sizeof("http://")-1);` |
|   273 | 1651 | `	SyBlobAppend(&sUrl,zName,(sxu32)SyStrlen(zName));` |
|   409 | 1652 | `	rc = HttpRun(pH,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl),` |
|   272 | 1653 | `		(phl_stream_ctx *)pVm->pOpenCtx);` |
|   273 | 1654 | `	SyBlobRelease(&sUrl);` |
|   273 | 1655 | `	if( rc != PH7_OK ){` |
|    39 | 1656 | `		HttpFree(pH);` |
|    39 | 1657 | `		return -1;` |
|     - | 1658 | `	}` |
|   234 | 1659 | `	*ppHandle = (void *)pH;` |
|   234 | 1660 | `	return PH7_OK;` |
|   146 | 1661 | `}` |
|   266 | 1662 | `static int HttpStream_Open(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|     1 | 1663 | `{` |
|   267 | 1664 | `	return HttpOpenScheme(zName,iMode,pResource,ppHandle,0);` |
|     1 | 1665 | `}` |
|     - | 1666 | `#ifdef PH7_ENABLE_OPENSSL` |
|    24 | 1667 | `static int HttpsStream_Open(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|   ! 0 | 1668 | `{` |
|    24 | 1669 | `	return HttpOpenScheme(zName,iMode,pResource,ppHandle,1);` |
|   ! 0 | 1670 | `}` |
|     - | 1671 | `#endif` |
|   234 | 1672 | `static void HttpStream_Close(void *pHandle)` |
|   ! 0 | 1673 | `{` |
|   234 | 1674 | `	HttpFree((http_private *)pHandle);` |
|   234 | 1675 | `}` |
|   406 | 1676 | `static ph7_int64 HttpStream_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|   ! 0 | 1677 | `{` |
|   406 | 1678 | `	http_private *pH = (http_private *)pHandle;` |
|     - | 1679 | `	SyBlob *pSrc;` |
|     - | 1680 | `	sxu32 *pOfft,nHave,nTake;` |
|   406 | 1681 | `	if( pH == 0 \|\| nRead < 1 ){` |
|   ! 0 | 1682 | `		return -1;` |
|     - | 1683 | `	}` |
|   406 | 1684 | `	if( pH->bChunked ){` |
|    30 | 1685 | `		HttpPumpChunks(pH,(sxu32)nRead);` |
|    30 | 1686 | `		pSrc = &pH->sOut;` |
|    30 | 1687 | `		pOfft = &pH->nOutOfft;` |
|    15 | 1688 | `	}else{` |
|   376 | 1689 | `		while( HttpRawLeft(pH) < (sxu32)nRead && !pH->bEof ){` |
|   188 | 1690 | `			if( HttpFill(pH) < 1 ){` |
|   188 | 1691 | `				break;` |
|     - | 1692 | `			}` |
|   ! 0 | 1693 | `		}` |
|   376 | 1694 | `		pSrc = &pH->sRaw;` |
|   376 | 1695 | `		pOfft = &pH->nRawOfft;` |
|     - | 1696 | `	}` |
|   406 | 1697 | `	nHave = SyBlobLength(pSrc) > *pOfft ? SyBlobLength(pSrc) - *pOfft : 0;` |
|   406 | 1698 | `	if( nHave < 1 ){` |
|   196 | 1699 | `		return 0;` |
|     - | 1700 | `	}` |
|   210 | 1701 | `	nTake = nHave < (sxu32)nRead ? nHave : (sxu32)nRead;` |
|   210 | 1702 | `	SyMemcpy(&((const char *)SyBlobData(pSrc))[*pOfft],pBuffer,nTake);` |
|   210 | 1703 | `	*pOfft += nTake;` |
|   210 | 1704 | `	pH->iPos += nTake;` |
|   210 | 1705 | `	return (ph7_int64)nTake;` |
|   203 | 1706 | `}` |
|     - | 1707 | `/* php's http stream is not writable; a write on one is the same false every` |
|     - | 1708 | ` * read-only device answers. */` |
|   ! 0 | 1709 | `static ph7_int64 HttpStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|   ! 0 | 1710 | `{` |
|   ! 0 | 1711 | `	SXUNUSED(pHandle);` |
|   ! 0 | 1712 | `	SXUNUSED(pBuf);` |
|   ! 0 | 1713 | `	SXUNUSED(nWrite);` |
|   ! 0 | 1714 | `	return -1;` |
|   ! 0 | 1715 | `}` |
|     - | 1716 | `/* Bytes the script has taken. php's http stream is not seekable, so this is the` |
|     - | 1717 | ` * only position it has. */` |
|     8 | 1718 | `static ph7_int64 HttpStream_Tell(void *pHandle)` |
|   ! 0 | 1719 | `{` |
|     8 | 1720 | `	http_private *pH = (http_private *)pHandle;` |
|     8 | 1721 | `	return pH ? pH->iPos : -1;` |
|   ! 0 | 1722 | `}` |
|     - | 1723 | `PH7_PRIVATE const ph7_io_stream sHTTP_Stream = {` |
|     - | 1724 | `	"http",` |
|     - | 1725 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1726 | `	HttpStream_Open,  /* xOpen */` |
|     - | 1727 | `	0,                /* xOpenDir */` |
|     - | 1728 | `	HttpStream_Close, /* xClose */` |
|     - | 1729 | `	0,                /* xCloseDir */` |
|     - | 1730 | `	HttpStream_Read,  /* xRead */` |
|     - | 1731 | `	0,                /* xReadDir */` |
|     - | 1732 | `	HttpStream_Write, /* xWrite */` |
|     - | 1733 | `	0,                /* xSeek */` |
|     - | 1734 | `	0,                /* xLock */` |
|     - | 1735 | `	0,                /* xRewindDir */` |
|     - | 1736 | `	HttpStream_Tell,  /* xTell */` |
|     - | 1737 | `	0,                /* xTrunc */` |
|     - | 1738 | `	0,                /* xSync */` |
|     - | 1739 | `	0                 /* xStat */` |
|     - | 1740 | `};` |
|     - | 1741 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - | 1742 | `/* php registers https:// as a device of its own over the SAME wrapper -- one` |
|     - | 1743 | ` * scheme name per entry, and the ops behind them are these. */` |
|     - | 1744 | `PH7_PRIVATE const ph7_io_stream sHTTPS_Stream = {` |
|     - | 1745 | `	"https",` |
|     - | 1746 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1747 | `	HttpsStream_Open, /* xOpen */` |
|     - | 1748 | `	0,                /* xOpenDir */` |
|     - | 1749 | `	HttpStream_Close, /* xClose */` |
|     - | 1750 | `	0,                /* xCloseDir */` |
|     - | 1751 | `	HttpStream_Read,  /* xRead */` |
|     - | 1752 | `	0,                /* xReadDir */` |
|     - | 1753 | `	HttpStream_Write, /* xWrite */` |
|     - | 1754 | `	0,                /* xSeek */` |
|     - | 1755 | `	0,                /* xLock */` |
|     - | 1756 | `	0,                /* xRewindDir */` |
|     - | 1757 | `	HttpStream_Tell,  /* xTell */` |
|     - | 1758 | `	0,                /* xTrunc */` |
|     - | 1759 | `	0,                /* xSync */` |
|     - | 1760 | `	0                 /* xStat */` |
|     - | 1761 | `};` |
|     - | 1762 | `#endif` |
|     - | 1763 | `/* Is this one of the http wrapper's devices? Asked by the metadata reader,` |
|     - | 1764 | ` * which labels them apart, by feof(), which reads the handle's own end, and by` |
|     - | 1765 | ` * the allow_url_fopen screen, which gates both schemes alike. */` |
| 58678 | 1766 | `PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream)` |
|     5 | 1767 | `{` |
|     - | 1768 | `#ifdef PH7_ENABLE_OPENSSL` |
| 58683 | 1769 | `	if( pStream == &sHTTPS_Stream ){` |
|    32 | 1770 | `		return 1;` |
|     - | 1771 | `	}` |
|     - | 1772 | `#endif` |
| 58651 | 1773 | `	return pStream == &sHTTP_Stream;` |
| 29272 | 1774 | `}` |
|     - | 1775 | `/*` |
|     - | 1776 | `` * php's `wrapper_data` for an http handle: the response headers of the exchange`` |
|     - | 1777 | ` * THIS handle made, which is not the same thing as the VM's last set (a second` |
|     - | 1778 | ` * request has since overwritten that one).` |
|     - | 1779 | ` */` |
|     6 | 1780 | `PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle)` |
|   ! 0 | 1781 | `{` |
|     6 | 1782 | `	http_private *pH = (http_private *)pHandle;` |
|     - | 1783 | `	ph7_value *pArr,*pLine;` |
|     - | 1784 | `	const char *zIn,*zEnd,*zCur;` |
|     6 | 1785 | `	if( pH == 0 ){` |
|   ! 0 | 1786 | `		return 0;` |
|     - | 1787 | `	}` |
|     6 | 1788 | `	pArr = ph7_new_array(pVm);` |
|     6 | 1789 | `	pLine = ph7_new_scalar(pVm);` |
|     6 | 1790 | `	if( pArr == 0 \|\| pLine == 0 ){` |
|   ! 0 | 1791 | `		if( pArr ){ ph7_release_value(pVm,pArr); }` |
|   ! 0 | 1792 | `		if( pLine ){ ph7_release_value(pVm,pLine); }` |
|   ! 0 | 1793 | `		return 0;` |
|     - | 1794 | `	}` |
|     6 | 1795 | `	zIn = (const char *)SyBlobData(&pH->sHdrs);` |
|     6 | 1796 | `	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];` |
|    30 | 1797 | `	while( zIn < zEnd ){` |
|    24 | 1798 | `		zCur = zIn;` |
|   390 | 1799 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
|   366 | 1800 | `			zCur++;` |
|   ! 0 | 1801 | `		}` |
|    24 | 1802 | `		ph7_value_string(pLine,zIn,(int)(zCur - zIn));` |
|    24 | 1803 | `		ph7_array_add_elem(pArr,0,pLine);` |
|    24 | 1804 | `		ph7_value_reset_string_cursor(pLine);` |
|    24 | 1805 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 | 1806 | `	}` |
|     6 | 1807 | `	ph7_release_value(pVm,pLine);` |
|     6 | 1808 | `	return pArr;` |
|     3 | 1809 | `}` |
|     - | 1810 | `/* The connection an http handle is reading, so stream_select() can wait on the` |
|     - | 1811 | ` * same descriptor php's does. */` |
|   ! 0 | 1812 | `PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle)` |
|   ! 0 | 1813 | `{` |
|   ! 0 | 1814 | `	http_private *pH = (http_private *)pHandle;` |
|   ! 0 | 1815 | `	return pH ? &pH->sock : 0;` |
|   ! 0 | 1816 | `}` |
|     - | 1817 | `/*` |
|     - | 1818 | `` * php's `unread_bytes`: what the wrapper has already pulled off the socket and`` |
|     - | 1819 | ` * the script has not taken. php reads a block at a time too, so a small` |
|     - | 1820 | ` * response is fully buffered by the time the header block has been parsed --` |
|     - | 1821 | ` * and, exactly as in php, a big one is not.` |
|     - | 1822 | ` */` |
|     6 | 1823 | `PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle)` |
|   ! 0 | 1824 | `{` |
|     6 | 1825 | `	http_private *pH = (http_private *)pHandle;` |
|     6 | 1826 | `	if( pH == 0 ){` |
|   ! 0 | 1827 | `		return 0;` |
|     - | 1828 | `	}` |
|     6 | 1829 | `	if( pH->bChunked ){` |
|   ! 0 | 1830 | `		return SyBlobLength(&pH->sOut) > pH->nOutOfft` |
|   ! 0 | 1831 | `			? SyBlobLength(&pH->sOut) - pH->nOutOfft : 0;` |
|     - | 1832 | `	}` |
|     6 | 1833 | `	return HttpRawLeft(pH);` |
|     3 | 1834 | `}` |
|     - | 1835 | `/* php's feof() for an http handle: the peer has closed AND nothing is left in` |
|     - | 1836 | ` * hand. */` |
|    34 | 1837 | `PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle)` |
|   ! 0 | 1838 | `{` |
|    34 | 1839 | `	http_private *pH = (http_private *)pHandle;` |
|    34 | 1840 | `	if( pH == 0 ){` |
|   ! 0 | 1841 | `		return 1;` |
|     - | 1842 | `	}` |
|    34 | 1843 | `	if( pH->bChunked ){` |
|    24 | 1844 | `		return pH->bChunkDone && SyBlobLength(&pH->sOut) <= pH->nOutOfft;` |
|     - | 1845 | `	}` |
|    10 | 1846 | `	return pH->bEof && HttpRawLeft(pH) == 0;` |
|    17 | 1847 | `}` |
|     - | 1848 | `/* ------------------------------------------------------------------------- */` |
|     - | 1849 | `/* get_headers()                                                               */` |
|     - | 1850 | `/* ------------------------------------------------------------------------- */` |
|     - | 1851 | `/*` |
|     - | 1852 | ` * array\|false get_headers(string $url, bool $associative = false,` |
|     - | 1853 | ` *                         ?resource $context = null)` |
|     - | 1854 | ` *` |
|     - | 1855 | ` * php's one function for "ask that URL what it answers, and nothing else". It` |
|     - | 1856 | `` * is the http:// wrapper with `ignore_errors` forced on -- a 404 is a set of`` |
|     - | 1857 | ` * headers, not a failure -- opened and closed without a byte of the body read.` |
|     - | 1858 | ` *` |
|     - | 1859 | ``  * Two refusals of its own: an EMPTY url is php's `Path must not be empty` `` |
|     - | 1860 | ` * ValueError, before anything is looked up; and a url that does not resolve to` |
|     - | 1861 | ` * the HTTP wrapper specifically -- a path, an unknown scheme, even data://,` |
|     - | 1862 | ` * which IS a url wrapper -- is a warning and false.` |
|     - | 1863 | ` *` |
|     - | 1864 | `` * `$associative` reshapes the SAME lines: a line with no colon in it is a status`` |
|     - | 1865 | `` * line and takes the next INTEGER key (which is how `abcdefghi 200 OK` gets one`` |
|     - | 1866 | `` * and `HTTP/1.1 200 OK: weird` does not), everything else is keyed by the text`` |
|     - | 1867 | ` * before its first colon with the value left-trimmed after it, and a name that` |
|     - | 1868 | ` * arrives twice -- across a redirect chain included -- collects into an ARRAY.` |
|     - | 1869 | ` */` |
|    46 | 1870 | `static int PH7_builtin_get_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 1871 | `{` |
|     - | 1872 | `	const ph7_io_stream *pStream;` |
|     - | 1873 | `	phl_stream_ctx *pCtxRes;` |
|     - | 1874 | `	ph7_value *pArr,*pKey,*pVal,*pLine;` |
|     - | 1875 | `	http_private *pH;` |
|     - | 1876 | `	void *pHandle;` |
|     - | 1877 | `	const char *zUrl,*zIn,*zEnd,*zCur;` |
|    46 | 1878 | `	int nUrl = 0,bAssoc = 0,bThrew = 0,iStatus = 0;` |
|     - | 1879 | `	/* The declared signature is the screen: a non-string $url is its TypeError` |
|     - | 1880 | `	 * and a fourth argument its ArgumentCountError, both before this runs. */` |
|    46 | 1881 | `	zUrl = ph7_value_to_string(apArg[0],&nUrl);` |
|    46 | 1882 | `	if( nUrl < 1 ){` |
|     2 | 1883 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - | 1884 | `	}` |
|    44 | 1885 | `	if( nArg > 1 ){` |
|    18 | 1886 | `		bAssoc = ph7_value_to_bool(apArg[1]);` |
|     9 | 1887 | `	}` |
|    44 | 1888 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|    44 | 1889 | `	if( bThrew ){` |
|   ! 0 | 1890 | `		return PH7_OK;` |
|     - | 1891 | `	}` |
|     - | 1892 | `	{` |
|     - | 1893 | `		/* php's screen is the wrapper's URL bit, not its identity: php:// and a` |
|     - | 1894 | `		 * plain path are the refusal, and data:// -- which php DOES count as a` |
|     - | 1895 | `		 * url wrapper -- goes through and answers false further down, silently,` |
|     - | 1896 | `		 * because a stream with no response headers has nothing to give. */` |
|    44 | 1897 | `		const char *zProbe = zUrl;` |
|    44 | 1898 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,nUrl);` |
|    44 | 1899 | `		if( pStream == 0 ){` |
|     - | 1900 | `			/* php names the missing wrapper first -- and only that: the` |
|     - | 1901 | `			 * failed-open line beside it belongs to an open, and get_headers()` |
|     - | 1902 | `			 * never reaches one. */` |
|     2 | 1903 | `			VfsThrowUnknownWrapperWarning(pCtx,zUrl);` |
|     1 | 1904 | `		}` |
|    44 | 1905 | `		if( pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream) ){` |
|     6 | 1906 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|     - | 1907 | `				"This function may only be used against URLs");` |
|     6 | 1908 | `			ph7_result_bool(pCtx,0);` |
|     6 | 1909 | `			return PH7_OK;` |
|     - | 1910 | `		}` |
|    38 | 1911 | `		zUrl = zProbe;` |
|     - | 1912 | `	}` |
|     - | 1913 | ``	/* php builds its own context over the caller's, with `ignore_errors` on: a`` |
|     - | 1914 | `	 * status php would refuse to open is a set of headers here. The flag rides` |
|     - | 1915 | `	 * the VM rather than a synthesized context so the caller's own options --` |
|     - | 1916 | `	 * its method, its headers -- reach the request unchanged. */` |
|    38 | 1917 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|    38 | 1918 | `	pCtx->pVm->bHttpGetHeaders = 1;` |
|    57 | 1919 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUrl,PH7_IO_OPEN_RDONLY,` |
|    19 | 1920 | `		FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|    38 | 1921 | `	pCtx->pVm->bHttpGetHeaders = 0;` |
|    38 | 1922 | `	if( pHandle == 0 ){` |
|     2 | 1923 | `		VfsThrowOpenWarning(pCtx,zUrl);` |
|     2 | 1924 | `		ph7_result_bool(pCtx,0);` |
|     2 | 1925 | `		return PH7_OK;` |
|     - | 1926 | `	}` |
|    36 | 1927 | `	if( !PH7_HttpStreamIs(pStream) ){` |
|     - | 1928 | `		/* A url wrapper of some other kind opened fine and has no response` |
|     - | 1929 | `		 * headers to answer with. */` |
|     2 | 1930 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     2 | 1931 | `		ph7_result_bool(pCtx,0);` |
|     2 | 1932 | `		return PH7_OK;` |
|     - | 1933 | `	}` |
|    34 | 1934 | `	pH = (http_private *)pHandle;` |
|    34 | 1935 | `	pArr = ph7_context_new_array(pCtx);` |
|    34 | 1936 | `	pKey = ph7_context_new_scalar(pCtx);` |
|    34 | 1937 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    34 | 1938 | `	pLine = ph7_context_new_scalar(pCtx);` |
|    34 | 1939 | `	if( pArr == 0 \|\| pKey == 0 \|\| pVal == 0 \|\| pLine == 0 ){` |
|   ! 0 | 1940 | `		HttpStream_Close(pHandle);` |
|   ! 0 | 1941 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1942 | `		return PH7_OK;` |
|     - | 1943 | `	}` |
|    34 | 1944 | `	zIn = (const char *)SyBlobData(&pH->sHdrs);` |
|    34 | 1945 | `	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];` |
|   216 | 1946 | `	while( zIn < zEnd ){` |
|     - | 1947 | `		sxu32 nColon,nLine;` |
|   182 | 1948 | `		zCur = zIn;` |
|  2792 | 1949 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
|  2610 | 1950 | `			zCur++;` |
|   ! 0 | 1951 | `		}` |
|   182 | 1952 | `		nLine = (sxu32)(zCur - zIn);` |
|  2004 | 1953 | `		for( nColon = 0 ; nColon < nLine && zIn[nColon] != ':' ; ++nColon ){` |
|     - | 1954 | `			;` |
|   911 | 1955 | `		}` |
|   182 | 1956 | `		if( !bAssoc ){` |
|   102 | 1957 | `			ph7_value_string(pLine,zIn,(int)nLine);` |
|   102 | 1958 | `			ph7_array_add_elem(pArr,0,pLine);` |
|   102 | 1959 | `			ph7_value_reset_string_cursor(pLine);` |
|   131 | 1960 | `		}else if( nColon >= nLine ){` |
|     - | 1961 | `			/* No colon: a status line, and php numbers those from 0 whatever` |
|     - | 1962 | `			 * they say. */` |
|    14 | 1963 | `			ph7_value_string(pLine,zIn,(int)nLine);` |
|    14 | 1964 | `			ph7_value_int(pKey,iStatus++);` |
|    14 | 1965 | `			ph7_array_add_elem(pArr,pKey,pLine);` |
|    14 | 1966 | `			ph7_value_reset_string_cursor(pLine);` |
|     7 | 1967 | `		}else{` |
|    66 | 1968 | `			sxu32 i = nColon + 1;` |
|     - | 1969 | `			ph7_value *pOld;` |
|   157 | 1970 | `			while( i < nLine && (zIn[i] == ' ' \|\| zIn[i] == '\t') ){` |
|    58 | 1971 | `				i++;` |
|   ! 0 | 1972 | `			}` |
|    66 | 1973 | `			ph7_value_string(pKey,zIn,(int)nColon);` |
|    66 | 1974 | `			ph7_value_string(pVal,&zIn[i],(int)(nLine - i));` |
|    66 | 1975 | `			pOld = ph7_array_fetch(pArr,zIn,(int)nColon);` |
|    66 | 1976 | `			if( pOld == 0 ){` |
|    58 | 1977 | `				ph7_array_add_elem(pArr,pKey,pVal);` |
|    37 | 1978 | `			}else if( pOld->iFlags & MEMOBJ_HASHMAP ){` |
|     - | 1979 | `				/* Already a list of its own: this is the third and later. */` |
|   ! 0 | 1980 | `				ph7_array_add_elem(pOld,0,pVal);` |
|   ! 0 | 1981 | `			}else{` |
|     - | 1982 | `				/* php turns the pair into a LIST the moment a name repeats. */` |
|     8 | 1983 | `				ph7_value *pList = ph7_context_new_array(pCtx);` |
|     8 | 1984 | `				if( pList ){` |
|     8 | 1985 | `					ph7_array_add_elem(pList,0,pOld);` |
|     8 | 1986 | `					ph7_array_add_elem(pList,0,pVal);` |
|     8 | 1987 | `					ph7_array_add_elem(pArr,pKey,pList);` |
|     4 | 1988 | `				}` |
|     - | 1989 | `			}` |
|    66 | 1990 | `			ph7_value_reset_string_cursor(pKey);` |
|    66 | 1991 | `			ph7_value_reset_string_cursor(pVal);` |
|     - | 1992 | `		}` |
|   182 | 1993 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 | 1994 | `	}` |
|    34 | 1995 | `	HttpStream_Close(pHandle);` |
|    34 | 1996 | `	ph7_result_value(pCtx,pArr);` |
|    34 | 1997 | `	return PH7_OK;` |
|    23 | 1998 | `}` |
|     - | 1999 | `#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */` |
|     - | 2000 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 2001 | `/*` |
|     - | 2002 | ` * ?array http_get_last_response_headers()` |
|     - | 2003 | ` *` |
|     - | 2004 | `` * php 8.4's modern spelling of `$http_response_header`, and the same store: the`` |
|     - | 2005 | ` * response headers of the last http:// exchange, or NULL when nothing has been` |
|     - | 2006 | ` * recorded since the last clear.` |
|     - | 2007 | ` */` |
|    12 | 2008 | `static int PH7_builtin_http_get_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2009 | `{` |
|     - | 2010 | `	ph7_value *pArr;` |
|     6 | 2011 | `	SXUNUSED(nArg);` |
|     6 | 2012 | `	SXUNUSED(apArg);` |
|    13 | 2013 | `	pArr = PH7_HttpHeaderArray(pCtx->pVm,&pCtx->pVm->sHttpRespHdrs);` |
|    13 | 2014 | `	if( pArr == 0 ){` |
|     9 | 2015 | `		ph7_result_null(pCtx);` |
|     9 | 2016 | `		return PH7_OK;` |
|     - | 2017 | `	}` |
|     4 | 2018 | `	ph7_result_value(pCtx,pArr);` |
|     4 | 2019 | `	ph7_release_value(pCtx->pVm,pArr);` |
|     4 | 2020 | `	return PH7_OK;` |
|     7 | 2021 | `}` |
|     - | 2022 | `/*` |
|     - | 2023 | ` * void http_clear_last_response_headers()` |
|     - | 2024 | ` *` |
|     - | 2025 | ` * Drops the store, so the getter above answers NULL rather than the array it` |
|     - | 2026 | ` * was answering a moment ago.` |
|     - | 2027 | ` */` |
|     6 | 2028 | `static int PH7_builtin_http_clear_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2029 | `{` |
|     3 | 2030 | `	SXUNUSED(nArg);` |
|     3 | 2031 | `	SXUNUSED(apArg);` |
|     7 | 2032 | `	PH7_HttpClearResponseHeaders(pCtx->pVm);` |
|     7 | 2033 | `	ph7_result_null(pCtx);` |
|     7 | 2034 | `	return PH7_OK;` |
|     1 | 2035 | `}` |
|     - | 2036 | `/*` |
|     - | 2037 | ` * The three names a script asks an exchange about. The two getters are in every` |
|     - | 2038 | ` * build -- the store they read is -- and get_headers() goes with the wrapper it` |
|     - | 2039 | ` * is the one door of.` |
|     - | 2040 | ` */` |
|  7925 | 2041 | `PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm)` |
|     5 | 2042 | `{` |
|  7930 | 2043 | `	ph7_create_function(&(*pVm),"http_get_last_response_headers",` |
|     - | 2044 | `		PH7_builtin_http_get_last_response_headers,0);` |
|  7930 | 2045 | `	ph7_create_function(&(*pVm),"http_clear_last_response_headers",` |
|     - | 2046 | `		PH7_builtin_http_clear_last_response_headers,0);` |
|     - | 2047 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|  7930 | 2048 | `	ph7_create_function(&(*pVm),"get_headers",PH7_builtin_get_headers,0);` |
|     - | 2049 | `#endif` |
|  7930 | 2050 | `}` |
|     - | 2051 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2052 |  |
