# src/ph7/vfs_http.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 894/1054 lines (84.82%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `/*` |
|     - |    7 | ` * The http:// stream wrapper -- php's one built-in PROTOCOL wrapper, and the` |
|     - |    8 | `` * device behind `file_get_contents('http://...')`, `fopen('http://...')`,`` |
|     - |    9 | `` * `get_headers()` and every reader that takes a URL.`` |
|     - |   10 | ` *` |
|     - |   11 | ` * It is a WRAPPER rather than a transport: tcp:// hands a script the bytes of a` |
|     - |   12 | ` * socket, and this speaks a request over one and hands back the RESPONSE BODY,` |
|     - |   13 | ` * with everything else about the exchange -- the status line, the response` |
|     - |   14 | ` * headers, the redirect chain it walked -- published beside the handle as` |
|     - |   15 | `` * `$http_response_header` / `stream_get_meta_data()['wrapper_data']`.`` |
|     - |   16 | ` *` |
|     - |   17 | ` * Four contracts are derived from php 8.5 rather than from the RFCs, because` |
|     - |   18 | ` * php's wrapper does not implement the RFCs:` |
|     - |   19 | ` *` |
|     - |   20 | `` *   1. THE REQUEST'S HEADER ORDER is fixed, and the `http` context options and`` |
|     - |   21 | ` *      two ini directives feed named slots inside it. A user header wins over` |
|     - |   22 | `` *      the slot it names -- a `Host:` of one's own suppresses php's, a`` |
|     - |   23 | ``  *      `Connection:` suppresses `Connection: close`, an `Authorization:` `` |
|     - |   24 | ` *      suppresses the one the URL's userinfo would have produced.` |
|     - |   25 | `` *   2. THE STATUS LINE IS `atoi(line + 9)`. php checks no prefix at all, so`` |
|     - |   26 | `` *      `HTTP/2.0 200 OK` is a 200 and `HTTP/2 200 OK` is a 0 (its digits sit at`` |
|     - |   27 | ` *      the wrong offset), and any line under ten bytes has no code. Anything` |
|     - |   28 | `` *      outside 200..399 is `HTTP request failed! <the raw line>` unless`` |
|     - |   29 | `` *      `ignore_errors` says otherwise; a 1xx is DISCARDED, headers and all, and`` |
|     - |   30 | ` *      the next response read in its place.` |
|     - |   31 | `` *   3. THE BODY ENDS WHERE THE SOCKET DOES. `Content-Length` is announced and`` |
|     - |   32 | ` *      never enforced -- php hands back the short body a lying header promised` |
|     - |   33 | ` *      100 bytes of, and the long one it promised 2 -- and the single framing` |
|     - |   34 | `` *      php DOES apply is `Transfer-Encoding: chunked`, whose header line is then`` |
|     - |   35 | ` *      not part of the response headers at all.` |
|     - |   36 | `` *   4. A RELATIVE `Location:` IS RESOLVED AGAINST THE PATH, not against the`` |
|     - |   37 | `` *      directory: php keeps everything up to and including the last `/` and then`` |
|     - |   38 | `` *      joins with ANOTHER `/`, so a redirect from `/a/b` to `rel` is `/a//rel`.`` |
|     - |   39 | `` *      The exceptions are a path that is just `/` (or empty), where the join`` |
|     - |   40 | ` *      is direct, and a Location of at most ONE byte, which is not joined at` |
|     - |   41 | ` *      all but put under the root.` |
|     - |   42 | ` *` |
|     - |   43 | ` * What is NOT here is https://: php's is this wrapper over the ssl:// transport,` |
|     - |   44 | ` * which this build has not got (PLAN.md §5, and §10 for the crypto policy).` |
|     - |   45 | ` */` |
|     - |   46 | `/*` |
|     - |   47 | ` * The response headers of the LAST http exchange, kept on the VM because two` |
|     - |   48 | `` * consumers outlive the handle: php writes `$http_response_header` into the`` |
|     - |   49 | ` * scope that called the opener (whether the open SUCCEEDED or not -- a 404 is a` |
|     - |   50 | ` * failed open with a full set of headers), and php 8.4's` |
|     - |   51 | ` * http_get_last_response_headers() answers them until something clears it.` |
|     - |   52 | ` */` |
|   236 |   53 | `PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines)` |
|   ! 0 |   54 | `{` |
|   236 |   55 | `	SyBlobReset(&pVm->sHttpRespHdrs);` |
|   236 |   56 | `	if( pLines && SyBlobLength(pLines) > 0 ){` |
|   234 |   57 | `		SyBlobAppend(&pVm->sHttpRespHdrs,SyBlobData(pLines),SyBlobLength(pLines));` |
|   117 |   58 | `	}` |
|   236 |   59 | `	pVm->bHttpRespHdrs = 1;` |
|   236 |   60 | `	pVm->bHttpRespFresh = 1;` |
|   236 |   61 | `}` |
|     - |   62 | `/*` |
|     - |   63 | ` * The stored lines as php's array of strings, or 0 when nothing has been` |
|     - |   64 | ` * recorded at all. One entry per line: an exchange that read nothing answers the` |
|     - |   65 | ` * EMPTY array, and one that read a single blank line answers one empty entry --` |
|     - |   66 | ` * two states php tells apart and a '\n'-separated store only can because every` |
|     - |   67 | ` * line carries its own terminator.` |
|     - |   68 | ` */` |
|   248 |   69 | `PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines)` |
|     1 |   70 | `{` |
|     - |   71 | `	ph7_value *pArr,*pLine;` |
|     - |   72 | `	const char *zIn,*zEnd,*zCur;` |
|   249 |   73 | `	if( pLines == 0 \|\| !pVm->bHttpRespHdrs ){` |
|     9 |   74 | `		return 0;` |
|     - |   75 | `	}` |
|   240 |   76 | `	pArr = ph7_new_array(pVm);` |
|   240 |   77 | `	pLine = ph7_new_scalar(pVm);` |
|   240 |   78 | `	if( pArr == 0 \|\| pLine == 0 ){` |
|   ! 0 |   79 | `		if( pArr ){ ph7_release_value(pVm,pArr); }` |
|   ! 0 |   80 | `		if( pLine ){ ph7_release_value(pVm,pLine); }` |
|   ! 0 |   81 | `		return 0;` |
|     - |   82 | `	}` |
|     - |   83 | `	/* Every recorded line carries a trailing '\n', which is what tells ONE` |
|     - |   84 | `	 * empty line (php's answer for a connection that ended where a status line` |
|     - |   85 | `	 * was due) from NO lines at all. */` |
|   240 |   86 | `	zIn = (const char *)SyBlobData(pLines);` |
|   240 |   87 | `	zEnd = &zIn[SyBlobLength(pLines)];` |
|  1264 |   88 | `	while( zIn < zEnd ){` |
|  1024 |   89 | `		zCur = zIn;` |
| 18548 |   90 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
| 17524 |   91 | `			zCur++;` |
|   ! 0 |   92 | `		}` |
|  1024 |   93 | `		ph7_value_string(pLine,zIn,(int)(zCur - zIn));` |
|  1024 |   94 | `		ph7_array_add_elem(pArr,0,pLine);` |
|  1024 |   95 | `		ph7_value_reset_string_cursor(pLine);` |
|  1024 |   96 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 |   97 | `	}` |
|   240 |   98 | `	ph7_release_value(pVm,pLine);` |
|   240 |   99 | `	return pArr;` |
|   125 |  100 | `}` |
|     - |  101 | `/*` |
|     - |  102 | `` * `$http_response_header`, written into the frame that called the opener. php`` |
|     - |  103 | ` * writes it from the stream layer, so every door -- file_get_contents(), fopen(),` |
|     - |  104 | ` * file(), readfile(), copy(), get_headers() -- has it, and an open that never` |
|     - |  105 | ` * reached a response leaves the caller's previous value alone.` |
|     - |  106 | ` */` |
| 43862 |  107 | `PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm)` |
|     5 |  108 | `{` |
|     - |  109 | `	static const char zVar[] = "http_response_header";` |
|     - |  110 | `	ph7_value *pArr,*pSlot;` |
|     - |  111 | `	SyString sName;` |
| 43867 |  112 | `	if( !pVm->bHttpRespFresh ){` |
| 43631 |  113 | `		return;` |
|     - |  114 | `	}` |
|   236 |  115 | `	pVm->bHttpRespFresh = 0;` |
|   236 |  116 | `	pArr = PH7_HttpHeaderArray(pVm,&pVm->sHttpRespHdrs);` |
|   236 |  117 | `	if( pArr == 0 ){` |
|   ! 0 |  118 | `		return;` |
|     - |  119 | `	}` |
|   236 |  120 | `	SyStringInitFromBuf(&sName,zVar,sizeof(zVar)-1);` |
|   236 |  121 | `	pSlot = VmExtractMemObj(pVm,&sName,TRUE,TRUE);` |
|   236 |  122 | `	if( pSlot ){` |
|   236 |  123 | `		PH7_MemObjStore(pArr,pSlot);` |
|   118 |  124 | `	}` |
|   236 |  125 | `	ph7_release_value(pVm,pArr);` |
| 21897 |  126 | `}` |
|     - |  127 | `/* http_clear_last_response_headers(): php drops the store, and the getter then` |
|     - |  128 | ` * answers NULL rather than an empty array. */` |
|   268 |  129 | `PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm)` |
|     1 |  130 | `{` |
|   269 |  131 | `	SyBlobReset(&pVm->sHttpRespHdrs);` |
|   269 |  132 | `	pVm->bHttpRespHdrs = 0;` |
|   269 |  133 | `	pVm->bHttpRespFresh = 0;` |
|   269 |  134 | `}` |
|     - |  135 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|     - |  136 | `/*` |
|     - |  137 | ` * An open http:// handle.` |
|     - |  138 | ` *` |
|     - |  139 | ` * The socket stays open for the whole life of the handle -- php's body IS the` |
|     - |  140 | ` * rest of the connection -- and sRaw holds whatever came off it with the header` |
|     - |  141 | ` * block, since a single recv() reads past the blank line more often than not.` |
|     - |  142 | ` */` |
|     - |  143 | `typedef struct http_private http_private;` |
|     - |  144 | `struct http_private` |
|     - |  145 | `{` |
|     - |  146 | `	ph7_vm *pVm;` |
|     - |  147 | `	phl_stream_ctx *pCtx; /* the context this exchange runs under, for its` |
|     - |  148 | ``	                       * `notification` callback; owned by the VM, so it`` |
|     - |  149 | `	                       * outlives the handle */` |
|     - |  150 | `	sxu8 bBody;           /* the headers are done: reads now carry the BODY */` |
|     - |  151 | `	sxu8 bNoFill;         /* the DRAIN below: decode what is in hand, read nothing */` |
|     - |  152 | ``	sxi64 iFileSize;      /* the `Content-Length` this response announced, or 0 */`` |
|     - |  153 | `	ph7_socket sock;      /* the connection the response is still arriving on */` |
|     - |  154 | `	SyBlob sRaw;          /* bytes read off the socket and not yet consumed */` |
|     - |  155 | `	sxu32 nRawOfft;       /* read cursor inside sRaw */` |
|     - |  156 | `	SyBlob sOut;          /* DECHUNKED bytes waiting for the reader (chunked only) */` |
|     - |  157 | `	sxu32 nOutOfft;       /* read cursor inside sOut */` |
|     - |  158 | `	ph7_int64 iPos;       /* bytes handed to the script: php's ftell() for this stream */` |
|     - |  159 | `	sxu8 bEof;            /* the peer closed and sRaw is drained */` |
|     - |  160 | `	sxu8 bChunked;        /* the body is chunk-framed */` |
|     - |  161 | `	sxu8 bChunkDone;      /* the zero-length chunk has been read */` |
|     - |  162 | `	sxi64 iChunkLeft;     /* bytes still owed by the chunk being read (-1: read a size line) */` |
|     - |  163 | `	SyBlob sHdrs;         /* the response header LINES, '\n'-separated */` |
|     - |  164 | `};` |
|     - |  165 | `/* php reads a socket in blocks; 8K is the size every other reader here uses. */` |
|     - |  166 | `#define HTTP_CHUNK_READ 8192` |
|     - |  167 | ``/* php's own redirect default, which the `max_redirects` option overrides. */`` |
|     - |  168 | `#define HTTP_MAX_REDIRECTS 20` |
|     - |  169 | ``/* php reads the status line into `char tmp_line[128]`, which its line reader`` |
|     - |  170 | ` * fills to at most 126 bytes; everything past that is discarded with the rest` |
|     - |  171 | ` * of the line. Header lines are read into an allocated buffer and uncapped. */` |
|     - |  172 | `#define HTTP_STATUS_LINE_MAX 126` |
|     - |  173 | `/* ------------------------------------------------------------------------- */` |
|     - |  174 | `/* Reading the connection                                                      */` |
|     - |  175 | `/* ------------------------------------------------------------------------- */` |
|     - |  176 | `/*` |
|     - |  177 | ` * Drop what has already been consumed from a buffer, so a long body does not` |
|     - |  178 | ` * grow one allocation per read.` |
|     - |  179 | ` */` |
|   286 |  180 | `static void HttpCompact(SyBlob *pBuf,sxu32 *pOfft)` |
|   ! 0 |  181 | `{` |
|   286 |  182 | `	if( *pOfft == 0 ){` |
|   286 |  183 | `		return;` |
|     - |  184 | `	}` |
|   ! 0 |  185 | `	if( *pOfft >= SyBlobLength(pBuf) ){` |
|   ! 0 |  186 | `		SyBlobReset(pBuf);` |
|   ! 0 |  187 | `	}else{` |
|   ! 0 |  188 | `		sxu32 nLeft = SyBlobLength(pBuf) - *pOfft;` |
|   ! 0 |  189 | `		SyMemcpy(&((char *)SyBlobData(pBuf))[*pOfft],SyBlobData(pBuf),nLeft);` |
|   ! 0 |  190 | `		pBuf->nByte = nLeft;` |
|     - |  191 | `	}` |
|   ! 0 |  192 | `	*pOfft = 0;` |
|   143 |  193 | `}` |
|     - |  194 | `/*` |
|     - |  195 | ` * One recv() worth of bytes into sRaw. Answers 1 when something arrived, 0 at` |
|     - |  196 | ` * the end of the connection, -1 for a read error.` |
|     - |  197 | ` */` |
|   446 |  198 | `static int HttpFill(http_private *pH)` |
|   ! 0 |  199 | `{` |
|     - |  200 | `	char zBuf[HTTP_CHUNK_READ];` |
|     - |  201 | `	int n;` |
|   446 |  202 | `	if( pH->bEof \|\| pH->sock == PH7_NET_INVALID_SOCKET ){` |
|     2 |  203 | `		return 0;` |
|     - |  204 | `	}` |
|   444 |  205 | `	n = PH7_NetRecv(pH->sock,zBuf,(int)sizeof(zBuf),0);` |
|   444 |  206 | `	if( n == 0 ){` |
|   172 |  207 | `		pH->bEof = 1;` |
|     - |  208 | `		/* php's notify_completed is not gated on the progress counter: a read` |
|     - |  209 | `		 * that comes back with nothing ENDS the transfer whether or not a` |
|     - |  210 | `		 * wrapper ever announced a size, which is why a connection that closes` |
|     - |  211 | `		 * where a status line was due reports it before the failure. */` |
|   172 |  212 | `		PH7_StreamCtxCompleted(pH->pCtx);` |
|   172 |  213 | `		return 0;` |
|     - |  214 | `	}` |
|   272 |  215 | `	if( n < 0 ){` |
|     - |  216 | `		/* A timed-out or interrupted read ends the body here: php's wrapper has` |
|     - |  217 | `		 * no retry either, and the bytes already in hand are the answer. */` |
|   ! 0 |  218 | `		pH->bEof = 1;` |
|   ! 0 |  219 | `		return -1;` |
|     - |  220 | `	}` |
|   272 |  221 | `	HttpCompact(&pH->sRaw,&pH->nRawOfft);` |
|   272 |  222 | `	if( SyBlobAppend(&pH->sRaw,zBuf,(sxu32)n) != SXRET_OK ){` |
|   ! 0 |  223 | `		return -1;` |
|     - |  224 | `	}` |
|   272 |  225 | `	if( !(pH->bBody && pH->bChunked) ){` |
|     - |  226 | `		/* php counts what the STREAM moved, and for a chunked body that is` |
|     - |  227 | `		 * what came out of the dechunk filter rather than what went in -- so` |
|     - |  228 | `		 * the framing bytes are counted here while the headers are being read` |
|     - |  229 | `		 * and by the decoder itself once the body starts. */` |
|   272 |  230 | `		PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)n);` |
|   136 |  231 | `	}` |
|   272 |  232 | `	return 1;` |
|   223 |  233 | `}` |
|     - |  234 | `/* How many unconsumed bytes sRaw is holding. */` |
|  2240 |  235 | `static sxu32 HttpRawLeft(http_private *pH)` |
|   ! 0 |  236 | `{` |
|  2240 |  237 | `	return SyBlobLength(&pH->sRaw) > pH->nRawOfft` |
|  2010 |  238 | `		? SyBlobLength(&pH->sRaw) - pH->nRawOfft : 0;` |
|   ! 0 |  239 | `}` |
|     - |  240 | `/*` |
|     - |  241 | ` * One line of the header block, with its terminator removed. php accepts both` |
|     - |  242 | ` * CRLF and a bare LF, and a header block that simply ENDS (the peer closed` |
|     - |  243 | ` * before the blank line) is not an error -- it is a response with no body.` |
|     - |  244 | ` *` |
|     - |  245 | ` * Answers 1 for a line, 0 at the end of the connection.` |
|     - |  246 | ` */` |
|  1364 |  247 | `static int HttpReadLine(http_private *pH,SyBlob *pLine,int *pnTerm)` |
|   ! 0 |  248 | `{` |
|  1364 |  249 | `	SyBlobReset(pLine);` |
|  1364 |  250 | `	if( pnTerm ){` |
|  1322 |  251 | `		*pnTerm = 0;` |
|   661 |  252 | `	}` |
|   954 |  253 | `	for(;;){` |
|  1636 |  254 | `		const char *zBase = (const char *)SyBlobData(&pH->sRaw);` |
|  1636 |  255 | `		sxu32 n = HttpRawLeft(pH),i;` |
| 20792 |  256 | `		for( i = 0 ; i < n ; ++i ){` |
| 20510 |  257 | `			if( zBase[pH->nRawOfft + i] == '\n' ){` |
|  1354 |  258 | `				sxu32 nCopy = i;` |
|  1354 |  259 | `				if( nCopy > 0 && zBase[pH->nRawOfft + nCopy - 1] == '\r' ){` |
|  1344 |  260 | `					nCopy--;` |
|   672 |  261 | `				}` |
|  1354 |  262 | `				if( nCopy > 0 ){` |
|  1062 |  263 | `					SyBlobAppend(pLine,&zBase[pH->nRawOfft],nCopy);` |
|   531 |  264 | `				}` |
|  1354 |  265 | `				if( pnTerm ){` |
|     - |  266 | `					/* The LF plus the CR before it, if there was one. */` |
|  1312 |  267 | `					*pnTerm = (int)(i - nCopy) + 1;` |
|   656 |  268 | `				}` |
|  1354 |  269 | `				pH->nRawOfft += i + 1;` |
|  1354 |  270 | `				return 1;` |
|     - |  271 | `			}` |
|  9578 |  272 | `		}` |
|   282 |  273 | `		if( pH->bNoFill ){` |
|     - |  274 | `			/* Mid-DRAIN: only what is already in hand may be read, and a line` |
|     - |  275 | `			 * with no terminator yet is not a line -- it stays where it is for` |
|     - |  276 | `			 * the read that will have the rest of it. */` |
|   ! 0 |  277 | `			return 0;` |
|     - |  278 | `		}` |
|   282 |  279 | `		if( HttpFill(pH) < 1 ){` |
|     - |  280 | `			/* No terminator will ever arrive. Whatever is buffered is the last` |
|     - |  281 | `			 * line, which is what makes php report a bodiless reply's first` |
|     - |  282 | ``			 * bytes as its `status line` -- and report them with NO trailing`` |
|     - |  283 | `			 * newline, since the line it read carried none. */` |
|    10 |  284 | `			if( n > 0 ){` |
|     2 |  285 | `				SyBlobAppend(pLine,&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],n);` |
|     2 |  286 | `				pH->nRawOfft += n;` |
|     2 |  287 | `				return 1;` |
|     - |  288 | `			}` |
|     8 |  289 | `			return 0;` |
|     - |  290 | `		}` |
|   ! 0 |  291 | `	}` |
|   682 |  292 | `}` |
|     - |  293 | `/*` |
|     - |  294 | ` * Decode as much of the chunked body as the bytes ALREADY READ allow, never` |
|     - |  295 | ` * touching the socket, and answer how many bytes came out. This is php's` |
|     - |  296 | `` * `dechunk` FILTER: it runs over whatever the buffer holds, all of it, and`` |
|     - |  297 | ` * what it produced is one batch -- which is why the progress counter moves` |
|     - |  298 | ` * once per socket read rather than once per chunk. A chunk EXTENSION` |
|     - |  299 | `` * (`3;ext=1`) and the trailer headers after the final chunk are consumed here`` |
|     - |  300 | ` * and never seen.` |
|     - |  301 | ` */` |
|    36 |  302 | `static sxu32 HttpDecodeBuffered(http_private *pH)` |
|   ! 0 |  303 | `{` |
|     - |  304 | `	SyBlob sLine;` |
|    36 |  305 | `	sxu32 nDone = 0;` |
|    36 |  306 | `	SyBlobInit(&sLine,&pH->pVm->sAllocator);` |
|    36 |  307 | `	pH->bNoFill = 1;` |
|    64 |  308 | `	while( !pH->bChunkDone ){` |
|    34 |  309 | `		if( pH->iChunkLeft < 0 ){` |
|     - |  310 | `			/* UNSIGNED on purpose: a size line is whatever bytes arrived, and a` |
|     - |  311 | `			 * hundred hex digits of it must wrap rather than overflow a signed` |
|     - |  312 | `			 * accumulator. Anything past INT32_MAX is capped -- the connection` |
|     - |  313 | `			 * ends long before, and "read to the end" is what both engines then` |
|     - |  314 | `			 * do. */` |
|    34 |  315 | `			sxu64 iSize = 0;` |
|     - |  316 | `			const char *zLine;` |
|     - |  317 | `			sxu32 nLine,i;` |
|    34 |  318 | `			if( HttpReadLine(pH,&sLine,0) == 0 ){` |
|     - |  319 | `				/* Only a partial size line, and nothing may be read to finish` |
|     - |  320 | `				 * it: this batch is over, but the BODY is not. */` |
|   ! 0 |  321 | `				break;` |
|     - |  322 | `			}` |
|    34 |  323 | `			zLine = (const char *)SyBlobData(&sLine);` |
|    34 |  324 | `			nLine = SyBlobLength(&sLine);` |
|    34 |  325 | `			if( nLine == 0 ){` |
|     - |  326 | `				/* The CRLF that closes the previous chunk's data. */` |
|    14 |  327 | `				continue;` |
|     - |  328 | `			}` |
|    40 |  329 | `			for( i = 0 ; i < nLine && SyisHex(zLine[i]) ; ++i ){` |
|    20 |  330 | `				if( iSize > 0x7FFFFFFF ){` |
|   ! 0 |  331 | `					continue; /* already capped; keep consuming the digits */` |
|     - |  332 | `				}` |
|    20 |  333 | `				iSize = iSize * 16 + (sxu64)SyHexToint(zLine[i]);` |
|    10 |  334 | `			}` |
|    20 |  335 | `			if( iSize > 0x7FFFFFFF ){` |
|   ! 0 |  336 | `				iSize = 0x7FFFFFFF;` |
|   ! 0 |  337 | `			}` |
|    20 |  338 | `			if( i == 0 ){` |
|     - |  339 | `				/* Not a size line at all: the framing is broken and php's` |
|     - |  340 | `				 * filter stops there rather than guessing. */` |
|   ! 0 |  341 | `				pH->bChunkDone = 1;` |
|   ! 0 |  342 | `				break;` |
|     - |  343 | `			}` |
|    20 |  344 | `			if( iSize == 0 ){` |
|     - |  345 | `				/* The final chunk. Its trailer headers run to the blank line` |
|     - |  346 | `				 * and belong to nobody. */` |
|     8 |  347 | `				while( HttpReadLine(pH,&sLine,0) == 1 && SyBlobLength(&sLine) > 0 ){` |
|     - |  348 | `					;` |
|   ! 0 |  349 | `				}` |
|     6 |  350 | `				pH->bChunkDone = 1;` |
|     6 |  351 | `				break;` |
|     - |  352 | `			}` |
|    14 |  353 | `			pH->iChunkLeft = (sxi64)iSize;` |
|     7 |  354 | `		}` |
|    14 |  355 | `		if( HttpRawLeft(pH) == 0 ){` |
|   ! 0 |  356 | `			break; /* the chunk is owed bytes that have not arrived yet */` |
|     - |  357 | `		}` |
|     - |  358 | `		{` |
|    14 |  359 | `			sxu32 nHave = HttpRawLeft(pH);` |
|    14 |  360 | `			sxu32 nTake = (sxu32)(pH->iChunkLeft < (sxi64)nHave ? pH->iChunkLeft : nHave);` |
|    14 |  361 | `			HttpCompact(&pH->sOut,&pH->nOutOfft);` |
|    21 |  362 | `			SyBlobAppend(&pH->sOut,` |
|    14 |  363 | `				&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],nTake);` |
|    14 |  364 | `			pH->nRawOfft += nTake;` |
|    14 |  365 | `			pH->iChunkLeft -= nTake;` |
|    14 |  366 | `			nDone += nTake;` |
|    14 |  367 | `			if( pH->iChunkLeft == 0 ){` |
|    14 |  368 | `				pH->iChunkLeft = -1;` |
|     7 |  369 | `			}` |
|     - |  370 | `		}` |
|   ! 0 |  371 | `	}` |
|    36 |  372 | `	pH->bNoFill = 0;` |
|    36 |  373 | `	SyBlobRelease(&sLine);` |
|    36 |  374 | `	return nDone;` |
|   ! 0 |  375 | `}` |
|     - |  376 | `/*` |
|     - |  377 | ` * Pump the decoder until sOut can serve nWant bytes or the body ends, reading` |
|     - |  378 | ` * the socket when it must -- and reporting each read's decoded output as php's` |
|     - |  379 | ` * stream layer does, once per read.` |
|     - |  380 | ` */` |
|    30 |  381 | `static void HttpPumpChunks(http_private *pH,sxu32 nWant)` |
|   ! 0 |  382 | `{` |
|    15 |  383 | `	for(;;){` |
|    30 |  384 | `		sxu32 nDec = HttpDecodeBuffered(pH);` |
|    30 |  385 | `		if( nDec > 0 ){` |
|   ! 0 |  386 | `			PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)nDec);` |
|   ! 0 |  387 | `		}` |
|    30 |  388 | `		if( pH->bChunkDone \|\| SyBlobLength(&pH->sOut) - pH->nOutOfft >= nWant ){` |
|    15 |  389 | `			break;` |
|     - |  390 | `		}` |
|   ! 0 |  391 | `		if( HttpFill(pH) < 1 ){` |
|   ! 0 |  392 | `			pH->bChunkDone = 1;` |
|   ! 0 |  393 | `			break;` |
|     - |  394 | `		}` |
|   ! 0 |  395 | `	}` |
|    30 |  396 | `}` |
|     - |  397 | `/*` |
|     - |  398 | `` * What a reader could take right now -- php's `writepos - readpos`, the bytes`` |
|     - |  399 | ` * its stream buffer is holding. A wrapper frame CREDITS this to the progress` |
|     - |  400 | ` * counter the moment it arms one, which is why a body that arrived in the same` |
|     - |  401 | ` * recv() as its headers is reported before anything has read a byte.` |
|     - |  402 | ` */` |
|   242 |  403 | `static sxi64 HttpBuffered(http_private *pH)` |
|   ! 0 |  404 | `{` |
|   242 |  405 | `	if( pH->bChunked ){` |
|   ! 0 |  406 | `		return (sxi64)(SyBlobLength(&pH->sOut) > pH->nOutOfft` |
|   ! 0 |  407 | `			? SyBlobLength(&pH->sOut) - pH->nOutOfft : 0);` |
|     - |  408 | `	}` |
|   242 |  409 | `	return (sxi64)HttpRawLeft(pH);` |
|   121 |  410 | `}` |
|     - |  411 | `/* ------------------------------------------------------------------------- */` |
|     - |  412 | `/* The request                                                                 */` |
|     - |  413 | `/* ------------------------------------------------------------------------- */` |
|     - |  414 | `/* php's own trim set, which is what the header block's two ends are cut with. */` |
|   216 |  415 | `static int HttpIsTrimByte(char c)` |
|   ! 0 |  416 | `{` |
|   216 |  417 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\r' \|\| c == 0 \|\| c == 0x0B;` |
|   ! 0 |  418 | `}` |
|     - |  419 | `/*` |
|     - |  420 | ` * Does the script's own header block already carry this header? php compares` |
|     - |  421 | ` * case-insensitively at the START of a line, which is what lets a caller take` |
|     - |  422 | ` * over php's Host, Connection, User-Agent, Content-Length, Content-Type and` |
|     - |  423 | ` * Authorization slots.` |
|     - |  424 | ` */` |
|   858 |  425 | `static int HttpUserHas(SyBlob *pHdrs,const char *zName)` |
|   ! 0 |  426 | `{` |
|   858 |  427 | `	const char *zIn = (const char *)SyBlobData(pHdrs);` |
|   858 |  428 | `	sxu32 nIn = SyBlobLength(pHdrs),nName = (sxu32)SyStrlen(zName);` |
|   858 |  429 | `	sxu32 i = 0;` |
|   944 |  430 | `	while( i < nIn ){` |
|    98 |  431 | `		sxu32 nStart = i;` |
|  1486 |  432 | `		while( i < nIn && zIn[i] != '\n' ){` |
|  1388 |  433 | `			i++;` |
|   ! 0 |  434 | `		}` |
|    98 |  435 | `		if( i - nStart >= nName && SyStrnicmp(&zIn[nStart],zName,nName) == 0 ){` |
|    12 |  436 | `			return 1;` |
|     - |  437 | `		}` |
|    86 |  438 | `		i++;` |
|   ! 0 |  439 | `	}` |
|   846 |  440 | `	return 0;` |
|   429 |  441 | `}` |
|     - |  442 | `/*` |
|     - |  443 | `` * The `header` context option, in the two shapes php reads: one string (whose`` |
|     - |  444 | ` * own line breaks separate the headers) or an ARRAY of them, whose STRING` |
|     - |  445 | ` * entries are joined with CRLF and whose other entries are dropped. Anything` |
|     - |  446 | ` * that is neither -- an int, a float, an object -- is ignored in silence.` |
|     - |  447 | ` *` |
|     - |  448 | ` * php then trims the WHOLE block at both ends and emits what is left verbatim.` |
|     - |  449 | ` * It does not touch the inside, which is worth being exact about: a blank line` |
|     - |  450 | ` * in the middle ENDS the request's header block, and everything the script put` |
|     - |  451 | ` * after it becomes the request BODY.` |
|     - |  452 | ` */` |
|   274 |  453 | `static void HttpCollectUserHeaders(ph7_vm *pVm,ph7_value *pOpt,SyBlob *pHdrs)` |
|   ! 0 |  454 | `{` |
|     - |  455 | `	SyBlob sJoin;` |
|     - |  456 | `	const char *zIn;` |
|     - |  457 | `	sxu32 nIn,nStart;` |
|   274 |  458 | `	if( pOpt == 0 ){` |
|   254 |  459 | `		return;` |
|     - |  460 | `	}` |
|    20 |  461 | `	SyBlobInit(&sJoin,&pVm->sAllocator);` |
|    20 |  462 | `	if( pOpt->iFlags & MEMOBJ_HASHMAP ){` |
|     6 |  463 | `		ph7_hashmap *pMap = (ph7_hashmap *)pOpt->x.pOther;` |
|     - |  464 | `		ph7_hashmap_node *pEntry;` |
|     6 |  465 | `		pMap->pCur = pMap->pFirst;` |
|    24 |  466 | `		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    18 |  467 | `			ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|    18 |  468 | `			if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    16 |  469 | `				SyBlobAppend(&sJoin,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|    16 |  470 | `				SyBlobAppend(&sJoin,"\r\n",sizeof("\r\n")-1);` |
|     8 |  471 | `			}` |
|   ! 0 |  472 | `		}` |
|    17 |  473 | `	}else if( pOpt->iFlags & MEMOBJ_STRING ){` |
|    14 |  474 | `		SyBlobAppend(&sJoin,SyBlobData(&pOpt->sBlob),SyBlobLength(&pOpt->sBlob));` |
|     7 |  475 | `	}` |
|    20 |  476 | `	zIn = (const char *)SyBlobData(&sJoin);` |
|    20 |  477 | `	nIn = SyBlobLength(&sJoin);` |
|    20 |  478 | `	nStart = 0;` |
|    32 |  479 | `	while( nStart < nIn && HttpIsTrimByte(zIn[nStart]) ){` |
|    12 |  480 | `		nStart++;` |
|   ! 0 |  481 | `	}` |
|    36 |  482 | `	while( nIn > nStart && HttpIsTrimByte(zIn[nIn-1]) ){` |
|    16 |  483 | `		nIn--;` |
|   ! 0 |  484 | `	}` |
|    20 |  485 | `	if( nIn > nStart ){` |
|    20 |  486 | `		SyBlobAppend(pHdrs,&zIn[nStart],nIn - nStart);` |
|    20 |  487 | `		SyBlobAppend(pHdrs,"\r\n",sizeof("\r\n")-1);` |
|    10 |  488 | `	}` |
|    20 |  489 | `	SyBlobRelease(&sJoin);` |
|   137 |  490 | `}` |
|     - |  491 | ``/* An `http` context option by name, or 0. */`` |
|  2902 |  492 | `static ph7_value * HttpOpt(phl_stream_ctx *pCtx,const char *zName)` |
|     1 |  493 | `{` |
|  2903 |  494 | `	return pCtx ? PH7_StreamCtxOption(pCtx,"http",zName) : 0;` |
|     1 |  495 | `}` |
|     - |  496 | `/*` |
|     - |  497 | ` * The three readers below all work on a COPY. Every ph7_value_to_*() converts` |
|     - |  498 | ` * its argument in place, and these arguments are the script's own context` |
|     - |  499 | `` * entries -- reading `protocol_version` as a number must not leave a FLOAT`` |
|     - |  500 | ` * where the script put a string, which stream_context_get_options() would then` |
|     - |  501 | ` * answer.` |
|     - |  502 | ` */` |
|    14 |  503 | `static sxi64 HttpOptInt(ph7_vm *pVm,ph7_value *pVal,sxi64 iDefault)` |
|   ! 0 |  504 | `{` |
|     - |  505 | `	ph7_value sCopy;` |
|     - |  506 | `	sxi64 iOut;` |
|    14 |  507 | `	if( pVal == 0 ){` |
|   ! 0 |  508 | `		return iDefault;` |
|     - |  509 | `	}` |
|    14 |  510 | `	PH7_MemObjInit(pVm,&sCopy);` |
|    14 |  511 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|    14 |  512 | `	iOut = ph7_value_to_int64(&sCopy);` |
|    14 |  513 | `	PH7_MemObjRelease(&sCopy);` |
|    14 |  514 | `	return iOut;` |
|     7 |  515 | `}` |
|   288 |  516 | `static int HttpOptBool(ph7_vm *pVm,ph7_value *pVal,int bDefault)` |
|     1 |  517 | `{` |
|     - |  518 | `	ph7_value sCopy;` |
|     - |  519 | `	int bOut;` |
|   289 |  520 | `	if( pVal == 0 ){` |
|   283 |  521 | `		return bDefault;` |
|     - |  522 | `	}` |
|     6 |  523 | `	PH7_MemObjInit(pVm,&sCopy);` |
|     6 |  524 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|     6 |  525 | `	bOut = ph7_value_to_bool(&sCopy) != 0;` |
|     6 |  526 | `	PH7_MemObjRelease(&sCopy);` |
|     6 |  527 | `	return bOut;` |
|   145 |  528 | `}` |
|   274 |  529 | `static double HttpOptReal(ph7_vm *pVm,ph7_value *pVal,double rDefault)` |
|   ! 0 |  530 | `{` |
|     - |  531 | `	ph7_value sCopy;` |
|     - |  532 | `	double rOut;` |
|   274 |  533 | `	if( pVal == 0 ){` |
|   264 |  534 | `		return rDefault;` |
|     - |  535 | `	}` |
|    10 |  536 | `	PH7_MemObjInit(pVm,&sCopy);` |
|    10 |  537 | `	PH7_MemObjLoad(pVal,&sCopy);` |
|    10 |  538 | `	rOut = ph7_value_to_double(&sCopy);` |
|    10 |  539 | `	PH7_MemObjRelease(&sCopy);` |
|    10 |  540 | `	return rOut;` |
|   137 |  541 | `}` |
|     - |  542 | `/* An ini directive's text, appended to pOut; answers 1 when the directive` |
|     - |  543 | ``  * carries a VALUE at all (php's unset directive is a third state, and `from` `` |
|     - |  544 | ` * is emitted for an EMPTY value but not for an absent one). */` |
|   542 |  545 | `static int HttpIniStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|   ! 0 |  546 | `{` |
|   542 |  547 | `	if( PH7_VmIniIsUnset(pVm,zName) ){` |
|   530 |  548 | `		return 0;` |
|     - |  549 | `	}` |
|    12 |  550 | `	SyBlobReset(pOut);` |
|    12 |  551 | `	PH7_VmIniGetStr(pVm,zName,pOut);` |
|    12 |  552 | `	return 1;` |
|   271 |  553 | `}` |
|     - |  554 | `/* base64 of "user:pass" for the Authorization header php builds out of a URL's` |
|     - |  555 | ` * userinfo. */` |
|     6 |  556 | `static sxi32 HttpB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|   ! 0 |  557 | `{` |
|     6 |  558 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|   ! 0 |  559 | `}` |
|     - |  560 | `/*` |
|     - |  561 | ` * Compose the request php would put on the wire.` |
|     - |  562 | ` *` |
|     - |  563 | ` * The slot order is derived, not invented: request line, the URL's` |
|     - |  564 | `` * Authorization, the `from` ini, Host, Connection, User-Agent, the automatic`` |
|     - |  565 | ` * Content-Length, the script's own headers, and last the automatic` |
|     - |  566 | ` * Content-Type. A body follows the blank line.` |
|     - |  567 | ` */` |
|   274 |  568 | `static void HttpBuildRequest(ph7_vm *pVm,phl_stream_ctx *pCtx,SyhttpUri *pUri,` |
|     - |  569 | `	const char *zTarget,int nTarget,SyBlob *pOut,const char *zMethod,int nMethod,` |
|     - |  570 | `	const char *zBody,int nBody)` |
|   ! 0 |  571 | `{` |
|     - |  572 | `	SyBlob sUser,sTmp;` |
|     - |  573 | `	ph7_value *pOptV;` |
|   274 |  574 | `	SyBlobInit(&sUser,&pVm->sAllocator);` |
|   274 |  575 | `	SyBlobInit(&sTmp,&pVm->sAllocator);` |
|   274 |  576 | `	HttpCollectUserHeaders(pVm,HttpOpt(pCtx,"header"),&sUser);` |
|     - |  577 | `	/* Request line. */` |
|   274 |  578 | `	SyBlobAppend(pOut,zMethod,(sxu32)nMethod);` |
|   274 |  579 | `	SyBlobAppend(pOut," ",1);` |
|   274 |  580 | `	SyBlobAppend(pOut,zTarget,(sxu32)nTarget);` |
|   274 |  581 | `	SyBlobAppend(pOut," HTTP/",sizeof(" HTTP/")-1);` |
|     - |  582 | `	/* php reads the version as a DOUBLE and prints it with ONE decimal, so a` |
|     - |  583 | ``	 * string is converted rather than passed through: `'2.0'` is 2.0, an ARRAY`` |
|     - |  584 | ``	 * is 1.0, and `null` is 0.0 -- each of which reaches the wire. */`` |
|   274 |  585 | `	SyBlobFormat(pOut,"%.1f",HttpOptReal(pVm,HttpOpt(pCtx,"protocol_version"),1.1));` |
|   274 |  586 | `	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     - |  587 | `	/* The URL's own credentials, which a script-supplied Authorization wins` |
|     - |  588 | ``	 * over. php base64s `user:pass` verbatim -- it decodes neither half. */`` |
|   274 |  589 | `	if( SyStringLength(&pUri->sUser) > 0 && !HttpUserHas(&sUser,"authorization:") ){` |
|     2 |  590 | `		SyBlobReset(&sTmp);` |
|     2 |  591 | `		SyBlobAppend(&sTmp,pUri->sUser.zString,pUri->sUser.nByte);` |
|     2 |  592 | `		SyBlobAppend(&sTmp,":",1);` |
|     2 |  593 | `		if( SyStringLength(&pUri->sPass) > 0 ){` |
|     2 |  594 | `			SyBlobAppend(&sTmp,pUri->sPass.zString,pUri->sPass.nByte);` |
|     1 |  595 | `		}` |
|     2 |  596 | `		SyBlobAppend(pOut,"Authorization: Basic ",sizeof("Authorization: Basic ")-1);` |
|     3 |  597 | `		SyBase64Encode((const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp),` |
|     1 |  598 | `			HttpB64Consumer,pOut);` |
|     2 |  599 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     1 |  600 | `	}` |
|     - |  601 | ``	/* php's `from` ini is a From: header whenever the directive has a value at`` |
|     - |  602 | `	 * all -- an EMPTY one still writes the header, which is not how the` |
|     - |  603 | `	 * user_agent directive below behaves. */` |
|   274 |  604 | `	if( HttpIniStr(pVm,"from",&sTmp) ){` |
|     6 |  605 | `		SyBlobAppend(pOut,"From: ",sizeof("From: ")-1);` |
|     6 |  606 | `		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|     6 |  607 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     3 |  608 | `	}` |
|   274 |  609 | `	if( !HttpUserHas(&sUser,"host:") ){` |
|   272 |  610 | `		SyBlobAppend(pOut,"Host: ",sizeof("Host: ")-1);` |
|   272 |  611 | `		SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);` |
|   272 |  612 | `		if( SyStringLength(&pUri->sPort) > 0` |
|   271 |  613 | `		 && !(pUri->sPort.nByte == 2 && SyStrncmp(pUri->sPort.zString,"80",2) == 0) ){` |
|   270 |  614 | `			SyBlobAppend(pOut,":",1);` |
|   270 |  615 | `			SyBlobAppend(pOut,pUri->sPort.zString,pUri->sPort.nByte);` |
|   135 |  616 | `		}` |
|   272 |  617 | `		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|   136 |  618 | `	}` |
|   274 |  619 | `	if( !HttpUserHas(&sUser,"connection:") ){` |
|   272 |  620 | `		SyBlobAppend(pOut,"Connection: close\r\n",sizeof("Connection: close\r\n")-1);` |
|   136 |  621 | `	}` |
|   274 |  622 | `	if( !HttpUserHas(&sUser,"user-agent:") ){` |
|   272 |  623 | `		pOptV = HttpOpt(pCtx,"user_agent");` |
|   272 |  624 | `		SyBlobReset(&sTmp);` |
|   272 |  625 | `		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){` |
|     4 |  626 | `			SyBlobAppend(&sTmp,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|     2 |  627 | `		}else{` |
|   268 |  628 | `			HttpIniStr(pVm,"user_agent",&sTmp);` |
|     - |  629 | `		}` |
|   272 |  630 | `		if( SyBlobLength(&sTmp) > 0 ){` |
|     6 |  631 | `			SyBlobAppend(pOut,"User-Agent: ",sizeof("User-Agent: ")-1);` |
|     6 |  632 | `			SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|     6 |  633 | `			SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|     3 |  634 | `		}` |
|   136 |  635 | `	}` |
|   274 |  636 | `	if( nBody > 0 && !HttpUserHas(&sUser,"content-length:") ){` |
|    14 |  637 | `		SyBlobFormat(pOut,"Content-Length: %d\r\n",nBody);` |
|     7 |  638 | `	}` |
|   274 |  639 | `	SyBlobAppend(pOut,SyBlobData(&sUser),SyBlobLength(&sUser));` |
|   274 |  640 | `	if( nBody > 0 && !HttpUserHas(&sUser,"content-type:") ){` |
|     - |  641 | `		SyString sCaller;` |
|    14 |  642 | `		SyBlobAppend(pOut,"Content-Type: application/x-www-form-urlencoded\r\n",` |
|     - |  643 | `			sizeof("Content-Type: application/x-www-form-urlencoded\r\n")-1);` |
|     - |  644 | `		/* php SAYS so, once per request it composes: an E_NOTICE under the name` |
|     - |  645 | `		 * of whatever function is doing the opening. */` |
|    14 |  646 | `		SyStringInitFromBuf(&sCaller,pVm->zOpenCaller ? pVm->zOpenCaller : "",` |
|     - |  647 | `			pVm->zOpenCaller ? SyStrlen(pVm->zOpenCaller) : 0);` |
|    14 |  648 | `		PH7_VmThrowError(pVm,pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_NOTICE,` |
|     - |  649 | `			"Content-type not specified assuming application/x-www-form-urlencoded");` |
|     7 |  650 | `	}` |
|   274 |  651 | `	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);` |
|   274 |  652 | `	if( nBody > 0 ){` |
|    16 |  653 | `		SyBlobAppend(pOut,zBody,(sxu32)nBody);` |
|     8 |  654 | `	}` |
|   274 |  655 | `	SyBlobRelease(&sUser);` |
|   274 |  656 | `	SyBlobRelease(&sTmp);` |
|   274 |  657 | `}` |
|     - |  658 | `/* ------------------------------------------------------------------------- */` |
|     - |  659 | `/* The exchange                                                                */` |
|     - |  660 | `/* ------------------------------------------------------------------------- */` |
|     - |  661 | `/*` |
|     - |  662 | ` * php's failure text for an open in flight. The reason has to outlive this` |
|     - |  663 | ` * call, so it is composed into the VM's own buffer.` |
|     - |  664 | ` */` |
|    36 |  665 | `static void HttpFail(ph7_vm *pVm,const char *zText,int nText)` |
|     1 |  666 | `{` |
|    37 |  667 | `	sxu32 nCopy = (sxu32)(nText < 0 ? (int)SyStrlen(zText) : nText);` |
|    37 |  668 | `	if( nCopy >= sizeof(pVm->zOpenErrBuf) ){` |
|   ! 0 |  669 | `		nCopy = sizeof(pVm->zOpenErrBuf) - 1;` |
|   ! 0 |  670 | `	}` |
|    37 |  671 | `	SyMemcpy(zText,pVm->zOpenErrBuf,nCopy);` |
|    37 |  672 | `	pVm->zOpenErrBuf[nCopy] = 0;` |
|    37 |  673 | `	PH7_StreamSetOpenError(pVm,pVm->zOpenErrBuf);` |
|    37 |  674 | `}` |
|     - |  675 | `/*` |
|     - |  676 | ` * php's port rule: whatever the URL spells, else 80.` |
|     - |  677 | ` */` |
|   284 |  678 | `static int HttpUriPort(SyhttpUri *pUri)` |
|     1 |  679 | `{` |
|   285 |  680 | `	sxi32 iPort = 0;` |
|   285 |  681 | `	if( SyStringLength(&pUri->sPort) < 1 ){` |
|   ! 0 |  682 | `		return 80;` |
|     - |  683 | `	}` |
|   285 |  684 | `	SyStrToInt32(pUri->sPort.zString,pUri->sPort.nByte,(void *)&iPort,0);` |
|   285 |  685 | `	return iPort > 0 ? (int)iPort : 80;` |
|   143 |  686 | `}` |
|     - |  687 | `/*` |
|     - |  688 | `` * The request TARGET: the URL's path and query, `/` when it has neither, and`` |
|     - |  689 | `` * the whole URL when `request_fulluri` says so (which is what a proxy that`` |
|     - |  690 | ` * insists on absolute-form gets).` |
|     - |  691 | ` */` |
|   284 |  692 | `static void HttpRequestTarget(SyhttpUri *pUri,const char *zUrl,int nUrl,` |
|     - |  693 | `	int bFullUri,SyBlob *pOut)` |
|     1 |  694 | `{` |
|   285 |  695 | `	SyBlobReset(pOut);` |
|   285 |  696 | `	if( bFullUri ){` |
|     2 |  697 | `		SyBlobAppend(pOut,zUrl,(sxu32)nUrl);` |
|     2 |  698 | `		return;` |
|     - |  699 | `	}` |
|   283 |  700 | `	if( SyStringLength(&pUri->sPath) < 1 ){` |
|     2 |  701 | `		SyBlobAppend(pOut,"/",1);` |
|     1 |  702 | `	}else{` |
|   281 |  703 | `		SyBlobAppend(pOut,pUri->sPath.zString,pUri->sPath.nByte);` |
|     - |  704 | `	}` |
|   283 |  705 | `	if( SyStringLength(&pUri->sQuery) > 0 ){` |
|     4 |  706 | `		SyBlobAppend(pOut,"?",1);` |
|     4 |  707 | `		SyBlobAppend(pOut,pUri->sQuery.zString,pUri->sQuery.nByte);` |
|     2 |  708 | `	}` |
|   143 |  709 | `}` |
|     - |  710 | `/*` |
|     - |  711 | `` * Where a `Location:` points, as an absolute `http://…` URL.`` |
|     - |  712 | ` *` |
|     - |  713 | ` * php recognizes exactly two shapes: one with a scheme (taken whole, host and` |
|     - |  714 | `` * all) and one starting with `/` (the path, on the same host). Everything else`` |
|     - |  715 | `` * is joined to the CURRENT path, up to and including its last `/`, with another`` |
|     - |  716 | `` * `/` between -- so `/a/b` + `rel` is `/a//rel`, which php does not normalize.`` |
|     - |  717 | `` * A path that is just `/` joins directly, and so does an empty one. A Location`` |
|     - |  718 | ` * of at most one byte is never joined: php puts it straight under the root, so` |
|     - |  719 | `` * `x` is `/x` and an empty one is `/` (php 8.5.11 settled that last case, which`` |
|     - |  720 | ` * earlier builds answered from a read past the end of the header).` |
|     - |  721 | ` */` |
|    38 |  722 | `static void HttpResolveLocation(SyhttpUri *pUri,const char *zLoc,sxu32 nLoc,SyBlob *pOut)` |
|   ! 0 |  723 | `{` |
|     - |  724 | `	sxu32 i;` |
|    38 |  725 | `	SyBlobReset(pOut);` |
|    60 |  726 | `	for( i = 0 ; i + 2 < nLoc ; ++i ){` |
|    54 |  727 | `		if( zLoc[i] == ':' && zLoc[i+1] == '/' && zLoc[i+2] == '/' ){` |
|     - |  728 | `			/* An absolute URL. php takes it as written, whatever the scheme --` |
|     - |  729 | `			 * an unsupported one then fails at the device lookup. */` |
|     2 |  730 | `			SyBlobAppend(pOut,zLoc,nLoc);` |
|     2 |  731 | `			return;` |
|     - |  732 | `		}` |
|    52 |  733 | `		if( zLoc[i] == '/' \|\| zLoc[i] == '?' \|\| zLoc[i] == '#' ){` |
|    15 |  734 | `			break;` |
|     - |  735 | `		}` |
|    11 |  736 | `	}` |
|    36 |  737 | `	SyBlobAppend(pOut,"http://",sizeof("http://")-1);` |
|    36 |  738 | `	if( SyStringLength(&pUri->sUser) > 0 ){` |
|   ! 0 |  739 | `		SyBlobAppend(pOut,pUri->sUser.zString,pUri->sUser.nByte);` |
|   ! 0 |  740 | `		if( SyStringLength(&pUri->sPass) > 0 ){` |
|   ! 0 |  741 | `			SyBlobAppend(pOut,":",1);` |
|   ! 0 |  742 | `			SyBlobAppend(pOut,pUri->sPass.zString,pUri->sPass.nByte);` |
|   ! 0 |  743 | `		}` |
|   ! 0 |  744 | `		SyBlobAppend(pOut,"@",1);` |
|   ! 0 |  745 | `	}` |
|    36 |  746 | `	SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);` |
|    36 |  747 | `	if( SyStringLength(&pUri->sPort) > 0 ){` |
|    36 |  748 | `		SyBlobAppend(pOut,":",1);` |
|    36 |  749 | `		SyBlobAppend(pOut,pUri->sPort.zString,pUri->sPort.nByte);` |
|    18 |  750 | `	}` |
|    36 |  751 | `	if( nLoc > 0 && zLoc[0] == '/' ){` |
|    28 |  752 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|    28 |  753 | `		return;` |
|     - |  754 | `	}` |
|     8 |  755 | `	if( nLoc < 2 ){` |
|     4 |  756 | `		SyBlobAppend(pOut,"/",1);` |
|     4 |  757 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|     4 |  758 | `		return;` |
|     - |  759 | `	}` |
|     - |  760 | `	{` |
|     4 |  761 | `		const char *zPath = pUri->sPath.zString;` |
|     4 |  762 | `		sxu32 nPath = pUri->sPath.nByte,nKeep = 0;` |
|    44 |  763 | `		for( i = 0 ; i < nPath ; ++i ){` |
|    40 |  764 | `			if( zPath[i] == '/' ){` |
|     8 |  765 | `				nKeep = i + 1;` |
|     4 |  766 | `			}` |
|    20 |  767 | `		}` |
|     4 |  768 | `		if( nKeep < 2 ){` |
|     - |  769 | ``			/* The path is `/` or has no directory part at all: php joins with`` |
|     - |  770 | `			 * a single separator. */` |
|   ! 0 |  771 | `			SyBlobAppend(pOut,"/",1);` |
|   ! 0 |  772 | `		}else{` |
|     4 |  773 | `			SyBlobAppend(pOut,zPath,nKeep);` |
|     4 |  774 | `			SyBlobAppend(pOut,"/",1);` |
|     - |  775 | `		}` |
|     4 |  776 | `		SyBlobAppend(pOut,zLoc,nLoc);` |
|     - |  777 | `	}` |
|    19 |  778 | `}` |
|     - |  779 | `/* Is this header line the named one? The name carries its own colon. */` |
|  1352 |  780 | `static int HttpHeaderIs(const char *zLine,sxu32 nLine,const char *zName,sxu32 nName)` |
|   ! 0 |  781 | `{` |
|  1352 |  782 | `	return nLine >= nName && SyStrnicmp(zLine,zName,nName) == 0;` |
|   ! 0 |  783 | `}` |
|     - |  784 | `/*` |
|     - |  785 | `` * The two headers php reports to a context's `notification` callback, told`` |
|     - |  786 | `` * apart by what each one SENDS: `Content-Type` sends its VALUE with the blanks`` |
|     - |  787 | `` * after the colon skipped, and `Content-Length` sends the whole LINE with the`` |
|     - |  788 | ` * size beside it. The size is announced only when the value is a plain run of` |
|     - |  789 | ``  * DIGITS -- `+5`, `2x` and an empty one are no announcement at all, where `007` `` |
|     - |  790 | ` * is seven -- and a run too wide for the clock saturates rather than wrapping.` |
|     - |  791 | ` */` |
|   736 |  792 | `static void HttpNotifyHeader(http_private *pH,const char *zLine,sxu32 nLine)` |
|   ! 0 |  793 | `{` |
|     - |  794 | `	sxu32 i;` |
|   736 |  795 | `	if( HttpHeaderIs(zLine,nLine,"Content-Type:",sizeof("Content-Type:")-1) ){` |
|   120 |  796 | `		i = sizeof("Content-Type:")-1;` |
|   300 |  797 | `		while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|   120 |  798 | `			i++;` |
|   ! 0 |  799 | `		}` |
|   180 |  800 | `		PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_MIME_TYPE_IS,` |
|   120 |  801 | `			PHL_STREAM_NOTIFY_SEVERITY_INFO,&zLine[i],(int)(nLine - i),0,0,0);` |
|   120 |  802 | `		return;` |
|     - |  803 | `	}` |
|   616 |  804 | `	if( HttpHeaderIs(zLine,nLine,"Content-Length:",sizeof("Content-Length:")-1) ){` |
|   250 |  805 | `		sxu64 iSize = 0;` |
|   250 |  806 | `		sxu32 nDigit = 0;` |
|   250 |  807 | `		int bOver = 0;` |
|   250 |  808 | `		i = sizeof("Content-Length:")-1;` |
|   627 |  809 | `		while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|   252 |  810 | `			i++;` |
|   ! 0 |  811 | `		}` |
|   720 |  812 | `		for( ; i < nLine ; ++i ){` |
|   474 |  813 | `			if( zLine[i] < '0' \|\| zLine[i] > '9' ){` |
|     4 |  814 | `				return; /* not a size php would announce */` |
|     - |  815 | `			}` |
|   470 |  816 | `			if( !bOver && iSize <= ((sxu64)SXI64_HIGH - (sxu64)(zLine[i] - '0')) / 10 ){` |
|   466 |  817 | `				iSize = iSize * 10 + (sxu64)(zLine[i] - '0');` |
|   233 |  818 | `			}else{` |
|     - |  819 | `				/* php reads this one with strtol, which SATURATES: a run too` |
|     - |  820 | `				 * wide for the clock is the ceiling itself, not a wrap and not` |
|     - |  821 | `				 * a refusal. */` |
|     4 |  822 | `				bOver = 1;` |
|     - |  823 | `			}` |
|   470 |  824 | `			nDigit++;` |
|   235 |  825 | `		}` |
|   246 |  826 | `		if( nDigit < 1 ){` |
|     2 |  827 | `			return;` |
|     - |  828 | `		}` |
|   244 |  829 | `		if( bOver ){` |
|     2 |  830 | `			iSize = (sxu64)SXI64_HIGH;` |
|     1 |  831 | `		}` |
|   244 |  832 | `		pH->iFileSize = (sxi64)iSize;` |
|   366 |  833 | `		PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_FILE_SIZE_IS,` |
|   122 |  834 | `			PHL_STREAM_NOTIFY_SEVERITY_INFO,zLine,(int)nLine,0,0,(sxi64)iSize);` |
|   122 |  835 | `	}` |
|   368 |  836 | `}` |
|     - |  837 | `/*` |
|     - |  838 | ` * Read one whole response: its status line, its header block, and whatever of` |
|     - |  839 | ` * the body arrived with them.` |
|     - |  840 | ` *` |
|     - |  841 | `` * Answers the status code php would compute -- `atoi(line + 9)`, which is 0 for`` |
|     - |  842 | ` * anything shorter than ten bytes and for a line whose digits sit elsewhere --` |
|     - |  843 | ` * and fills *pzStatus with the raw status line, which is what php's failure` |
|     - |  844 | ` * names. A 1xx is swallowed here and the next response read in its place, along` |
|     - |  845 | ` * with the headers it had already recorded.` |
|     - |  846 | ` */` |
|   274 |  847 | `static int HttpReadResponse(http_private *pH,SyBlob *pHdrs,SyBlob *pStatus,` |
|     - |  848 | `	SyBlob *pLocation,int *pbHasLocation,int *pbAnyLine)` |
|   ! 0 |  849 | `{` |
|     - |  850 | `	SyBlob sLine,sNext;` |
|   274 |  851 | `	int iCode = 0,nTerm = 0,nNextTerm = 0,bStale,bDiscard = 0;` |
|   274 |  852 | `	SyBlobInit(&sLine,&pH->pVm->sAllocator);` |
|   274 |  853 | `	SyBlobInit(&sNext,&pH->pVm->sAllocator);` |
|   274 |  854 | `	*pbHasLocation = 0;` |
|   141 |  855 | `	for(;;){` |
|   282 |  856 | `		sxu32 nHdrStart = SyBlobLength(pHdrs);` |
|   282 |  857 | `		bStale = 0;` |
|   282 |  858 | `		pH->iFileSize = 0;` |
|     - |  859 | `		/* php reads every line into ONE buffer and does not clear it when the` |
|     - |  860 | `		 * read fails, so a connection that ends where a status line was due` |
|     - |  861 | `		 * reports the LAST line it did read -- which after an informational` |
|     - |  862 | `		 * response is that response's own blank line. */` |
|   282 |  863 | `		if( HttpReadLine(pH,&sNext,&nNextTerm) ){` |
|   278 |  864 | `			SyBlobReset(&sLine);` |
|   278 |  865 | `			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|   278 |  866 | `			nTerm = nNextTerm;` |
|   278 |  867 | `			if( SyBlobLength(&sLine) > HTTP_STATUS_LINE_MAX ){` |
|     - |  868 | `				/* php reads the STATUS line into a 128-byte stack buffer and` |
|     - |  869 | `				 * throws the rest of that line away -- a header line, read into` |
|     - |  870 | `				 * a buffer it allocates, has no such cap. A line that hit the` |
|     - |  871 | `				 * cap carried no terminator either. */` |
|   ! 0 |  872 | `				sLine.nByte = HTTP_STATUS_LINE_MAX;` |
|   ! 0 |  873 | `				nTerm = 0;` |
|   ! 0 |  874 | `			}` |
|   278 |  875 | `			*pbAnyLine = 1;` |
|   139 |  876 | `		}else{` |
|     4 |  877 | `			bStale = 1;` |
|     4 |  878 | `			if( !*pbAnyLine ){` |
|     - |  879 | `				/* Nothing was ever read: there is no line to report and php's` |
|     - |  880 | `				 * header array stays EMPTY rather than gaining a blank entry. */` |
|     2 |  881 | `				break;` |
|     - |  882 | `			}` |
|     - |  883 | `		}` |
|     - |  884 | `		/* php's refusal prints the status line AS READ, terminator and all --` |
|     - |  885 | `		 * so a reply that simply ran out of bytes is named with no newline` |
|     - |  886 | `		 * after it, and an ordinary one keeps the CRLF it arrived with. */` |
|   280 |  887 | `		SyBlobReset(pStatus);` |
|   280 |  888 | `		SyBlobAppend(pStatus,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   280 |  889 | `		if( nTerm == 2 ){` |
|   276 |  890 | `			SyBlobAppend(pStatus,"\r\n",sizeof("\r\n")-1);` |
|   142 |  891 | `		}else if( nTerm == 1 ){` |
|     2 |  892 | `			SyBlobAppend(pStatus,"\n",1);` |
|     1 |  893 | `		}` |
|   280 |  894 | `		iCode = 0;` |
|   280 |  895 | `		if( SyBlobLength(&sLine) > 9 ){` |
|   276 |  896 | `			sxi32 iTmp = 0;` |
|   414 |  897 | `			SyStrToInt32(&((const char *)SyBlobData(&sLine))[9],` |
|   276 |  898 | `				SyBlobLength(&sLine) - 9,(void *)&iTmp,0);` |
|   276 |  899 | `			iCode = (int)iTmp;` |
|   138 |  900 | `		}` |
|   280 |  901 | `		SyBlobAppend(pHdrs,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   280 |  902 | `		SyBlobAppend(pHdrs,"\n",1);` |
|   280 |  903 | `		bDiscard = iCode >= 100 && iCode < 200 && iCode != 101;` |
|   280 |  904 | `		if( !bDiscard && (iCode < 200 \|\| iCode >= 400) ){` |
|     - |  905 | `			/* php tells the notifier about a status it will not open for as` |
|     - |  906 | `			 * soon as it has READ it -- before the header block, and whatever` |
|     - |  907 | ``			 * `ignore_errors` says, so the callback hears about a 404 the`` |
|     - |  908 | `			 * caller went on to read anyway. The text is the status line as it` |
|     - |  909 | `			 * arrived, terminator and all, and the code is beside it: 0 for a` |
|     - |  910 | `			 * line whose digits are not where php looks. An informational` |
|     - |  911 | `			 * response php DISCARDS is not one of these; the line it fails on` |
|     - |  912 | `			 * after one is the blank line it kept. */` |
|    36 |  913 | `			PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_FAILURE,` |
|    24 |  914 | `				PHL_STREAM_NOTIFY_SEVERITY_ERR,(const char *)SyBlobData(pStatus),` |
|    24 |  915 | `				(int)SyBlobLength(pStatus),iCode,0,0);` |
|    12 |  916 | `		}` |
|   280 |  917 | `		if( bStale ){` |
|     2 |  918 | `			break;` |
|     - |  919 | `		}` |
|     - |  920 | `		/* The header block. A continuation line (one opening with a space or a` |
|     - |  921 | `		 * tab) belongs to the header before it, joined by a single space. */` |
|   893 |  922 | `		for(;;){` |
|     - |  923 | `			const char *zLine;` |
|     - |  924 | `			sxu32 nLine,nColon;` |
|  1040 |  925 | `			if( HttpReadLine(pH,&sNext,&nNextTerm) == 0 ){` |
|     4 |  926 | `				break;` |
|     - |  927 | `			}` |
|  1036 |  928 | `			SyBlobReset(&sLine);` |
|  1036 |  929 | `			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|  1036 |  930 | `			nTerm = nNextTerm;` |
|  1036 |  931 | `			zLine = (const char *)SyBlobData(&sLine);` |
|  1036 |  932 | `			nLine = SyBlobLength(&sLine);` |
|     - |  933 | `			/* php right-trims a HEADER line -- and only a header line: the` |
|     - |  934 | `			 * status line above keeps whatever trailing blanks it arrived` |
|     - |  935 | `			 * with. Done after the length is taken, so a line of nothing but` |
|     - |  936 | `			 * blanks is still the CONTINUATION its first byte makes it. */` |
|  1442 |  937 | `			while( nLine > 0 && (zLine[nLine-1] == ' ' \|\| zLine[nLine-1] == '\t')` |
|   924 |  938 | `			    && !(zLine[0] == ' ' \|\| zLine[0] == '\t') ){` |
|    16 |  939 | `				nLine--;` |
|   ! 0 |  940 | `			}` |
|  1036 |  941 | `			if( nLine < 1 ){` |
|   272 |  942 | `				break; /* the blank line: the body starts here */` |
|     - |  943 | `			}` |
|   764 |  944 | `			if( zLine[0] == ' ' \|\| zLine[0] == '\t' ){` |
|    10 |  945 | `				sxu32 i = 0;` |
|    33 |  946 | `				while( i < nLine && (zLine[i] == ' ' \|\| zLine[i] == '\t') ){` |
|    18 |  947 | `					i++;` |
|   ! 0 |  948 | `				}` |
|    15 |  949 | `				while( nLine > i && (zLine[nLine-1] == ' ' \|\| zLine[nLine-1] == '\t') ){` |
|   ! 0 |  950 | `					nLine--;` |
|   ! 0 |  951 | `				}` |
|     - |  952 | `				/* Back over the separator the previous line closed with: the` |
|     - |  953 | `				 * two are ONE header, joined by a single space whatever the` |
|     - |  954 | `				 * continuation was indented with. */` |
|    10 |  955 | `				if( pHdrs->nByte > 0 ){` |
|    10 |  956 | `					pHdrs->nByte--;` |
|     5 |  957 | `				}` |
|    10 |  958 | `				SyBlobAppend(pHdrs," ",1);` |
|    10 |  959 | `				SyBlobAppend(pHdrs,&zLine[i],nLine - i);` |
|    10 |  960 | `				SyBlobAppend(pHdrs,"\n",1);` |
|    10 |  961 | `				continue;` |
|     - |  962 | `			}` |
|  8938 |  963 | `			for( nColon = 0 ; nColon < nLine && zLine[nColon] != ':' ; ++nColon ){` |
|     - |  964 | `				;` |
|  4092 |  965 | `			}` |
|   754 |  966 | `			if( nColon >= nLine ){` |
|     - |  967 | `				/* php refuses the whole response for a header line with no` |
|     - |  968 | `				 * colon in it, and says so in exactly these words. */` |
|     2 |  969 | `				HttpFail(pH->pVm,` |
|     - |  970 | `					"HTTP invalid response format (no colon in header line)!",-1);` |
|     2 |  971 | `				SyBlobRelease(&sLine);` |
|     2 |  972 | `				SyBlobRelease(&sNext);` |
|     2 |  973 | `				return -1;` |
|     - |  974 | `			}` |
|   752 |  975 | `			if( nColon == sizeof("Transfer-Encoding")-1` |
|   382 |  976 | `			 && !pH->pVm->bHttpGetHeaders` |
|    10 |  977 | `			 && SyStrnicmp(zLine,"Transfer-Encoding",sizeof("Transfer-Encoding")-1) == 0 ){` |
|     - |  978 | `				sxu32 nOfft;` |
|     8 |  979 | `				if( SyBlobSearch(&zLine[nColon],nLine - nColon,"chunked",` |
|     4 |  980 | `						sizeof("chunked")-1,&nOfft) == SXRET_OK ){` |
|     - |  981 | `					/* php frames the body with its dechunk filter and drops` |
|     - |  982 | `					 * the header that asked for it: a script reading the` |
|     - |  983 | `					 * response headers never sees this line. */` |
|     6 |  984 | `					pH->bChunked = 1;` |
|     6 |  985 | `					pH->iChunkLeft = -1;` |
|     6 |  986 | `					continue;` |
|     - |  987 | `				}` |
|     1 |  988 | `			}` |
|   746 |  989 | `			if( nColon == sizeof("Location")-1` |
|   398 |  990 | `			 && SyStrnicmp(zLine,"Location",sizeof("Location")-1) == 0 ){` |
|     - |  991 | `				/* php trims the value at BOTH ends before resolving it. */` |
|    50 |  992 | `				sxu32 i = nColon + 1,nStop = nLine;` |
|   102 |  993 | `				while( i < nStop && HttpIsTrimByte(zLine[i]) ){` |
|    52 |  994 | `					i++;` |
|   ! 0 |  995 | `				}` |
|    50 |  996 | `				while( nStop > i && HttpIsTrimByte(zLine[nStop-1]) ){` |
|   ! 0 |  997 | `					nStop--;` |
|   ! 0 |  998 | `				}` |
|    50 |  999 | `				SyBlobReset(pLocation);` |
|    50 | 1000 | `				SyBlobAppend(pLocation,&zLine[i],nStop - i);` |
|    50 | 1001 | `				*pbHasLocation = 1;` |
|    25 | 1002 | `			}` |
|   746 | 1003 | `			if( !bDiscard ){` |
|     - | 1004 | `				/* php drops an informational response WHOLE -- the headers it` |
|     - | 1005 | ``				 * carried are never looked at, so a `Content-Type` on a 100 is`` |
|     - | 1006 | `				 * not a mime type anybody is told about. */` |
|   736 | 1007 | `				HttpNotifyHeader(pH,zLine,nLine);` |
|   368 | 1008 | `			}` |
|   746 | 1009 | `			SyBlobAppend(pHdrs,zLine,nLine);` |
|   746 | 1010 | `			SyBlobAppend(pHdrs,"\n",1);` |
|   ! 0 | 1011 | `		}` |
|   276 | 1012 | `		if( bDiscard ){` |
|     - | 1013 | `			/* An informational response is not the answer: php drops it, drops` |
|     - | 1014 | `			 * the headers it collected for it, and reads the next one. 101 is` |
|     - | 1015 | `			 * php's one exception -- a protocol SWITCH is the last thing that` |
|     - | 1016 | `			 * will ever be spoken as HTTP on this connection, so there is no` |
|     - | 1017 | `			 * next response to read and php reports the 101 itself. */` |
|     8 | 1018 | `			pHdrs->nByte = nHdrStart;` |
|     8 | 1019 | `			pH->bChunked = 0;` |
|     8 | 1020 | `			*pbHasLocation = 0;` |
|     8 | 1021 | `			continue;` |
|     - | 1022 | `		}` |
|   268 | 1023 | `		break;` |
|   ! 0 | 1024 | `	}` |
|   272 | 1025 | `	SyBlobRelease(&sLine);` |
|   272 | 1026 | `	SyBlobRelease(&sNext);` |
|   272 | 1027 | `	return iCode;` |
|   137 | 1028 | `}` |
|     - | 1029 | `/* Release everything the handle owns; the handle itself goes with it. */` |
|   246 | 1030 | `static void HttpFree(http_private *pH)` |
|     1 | 1031 | `{` |
|   247 | 1032 | `	if( pH == 0 ){` |
|   ! 0 | 1033 | `		return;` |
|     - | 1034 | `	}` |
|   247 | 1035 | `	if( pH->sock != PH7_NET_INVALID_SOCKET ){` |
|   236 | 1036 | `		PH7_NetClose(pH->sock);` |
|   236 | 1037 | `		pH->sock = PH7_NET_INVALID_SOCKET;` |
|   118 | 1038 | `	}` |
|   247 | 1039 | `	SyBlobRelease(&pH->sRaw);` |
|   247 | 1040 | `	SyBlobRelease(&pH->sOut);` |
|   247 | 1041 | `	SyBlobRelease(&pH->sHdrs);` |
|   247 | 1042 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|   124 | 1043 | `}` |
|     - | 1044 | `/* The connection this exchange runs over, or php's own failure text. */` |
|   284 | 1045 | `static int HttpConnect(http_private *pH,SyhttpUri *pUri,phl_stream_ctx *pCtx)` |
|     1 | 1046 | `{` |
|     - | 1047 | `	SyBlob sHost;` |
|     - | 1048 | `	const char *zHost;` |
|   285 | 1049 | `	const char *zErr = "";` |
|   285 | 1050 | `	int iErrno = 0,iTimeoutMs,rc = -1;` |
|     - | 1051 | `	ph7_value *pOptV;` |
|     - | 1052 | `	ph7_socket sock;` |
|     - | 1053 | `	/* The resolver wants a NUL-terminated name, and a host is whatever the URL` |
|     - | 1054 | `	 * spelled -- no fixed buffer, because the failure below NAMES it. */` |
|   285 | 1055 | `	SyBlobInit(&sHost,&pH->pVm->sAllocator);` |
|   285 | 1056 | `	SyBlobAppend(&sHost,pUri->sHost.zString,pUri->sHost.nByte);` |
|   285 | 1057 | `	SyBlobNullAppend(&sHost);` |
|   285 | 1058 | `	zHost = (const char *)SyBlobData(&sHost);` |
|     - | 1059 | ``	/* php bounds the exchange by the `timeout` option, and by`` |
|     - | 1060 | `	 * default_socket_timeout when the script named none. */` |
|   285 | 1061 | `	iTimeoutMs = (int)(PH7_VmIniGetInt(pH->pVm,"default_socket_timeout",60) * 1000);` |
|   285 | 1062 | `	pOptV = HttpOpt(pCtx,"timeout");` |
|   285 | 1063 | `	if( pOptV ){` |
|   ! 0 | 1064 | `		double rSec = HttpOptReal(pH->pVm,pOptV,0);` |
|   ! 0 | 1065 | `		iTimeoutMs = rSec > 0 ? (int)(rSec * 1000) : 0;` |
|   ! 0 | 1066 | `	}` |
|   285 | 1067 | `	sock = PH7_NetConnect(zHost,HttpUriPort(pUri),iTimeoutMs,0,0,0,&iErrno,&zErr);` |
|   285 | 1068 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    11 | 1069 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|     - | 1070 | `			SyBlob sMsg;` |
|     - | 1071 | `			SyString sCaller;` |
|   ! 0 | 1072 | `			SyBlobInit(&sMsg,&pH->pVm->sAllocator);` |
|   ! 0 | 1073 | `			SyBlobFormat(&sMsg,` |
|     - | 1074 | `				"php_network_getaddresses: getaddrinfo for %z failed: Name or service not known",` |
|   ! 0 | 1075 | `				&pUri->sHost);` |
|   ! 0 | 1076 | `			SyBlobNullAppend(&sMsg);` |
|   ! 0 | 1077 | `			HttpFail(pH->pVm,(const char *)SyBlobData(&sMsg),(int)SyBlobLength(&sMsg)-1);` |
|     - | 1078 | `			/* php says this one TWICE: the resolver's own failure under the` |
|     - | 1079 | `			 * calling function's name, and then the caller's failed-open` |
|     - | 1080 | `			 * sentence carrying it as the reason. A refused CONNECT is only the` |
|     - | 1081 | `			 * second -- the resolver is where php has the extra warning. */` |
|   ! 0 | 1082 | `			SyStringInitFromBuf(&sCaller,pH->pVm->zOpenCaller ? pH->pVm->zOpenCaller : "",` |
|     - | 1083 | `				pH->pVm->zOpenCaller ? SyStrlen(pH->pVm->zOpenCaller) : 0);` |
|   ! 0 | 1084 | `			PH7_VmThrowError(pH->pVm,pH->pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_WARNING,` |
|   ! 0 | 1085 | `				(const char *)SyBlobData(&sMsg));` |
|   ! 0 | 1086 | `			SyBlobRelease(&sMsg);` |
|   ! 0 | 1087 | `		}else{` |
|    11 | 1088 | `			HttpFail(pH->pVm,zErr && zErr[0] ? zErr : "Connection refused",-1);` |
|     - | 1089 | `		}` |
|     6 | 1090 | `	}else{` |
|   274 | 1091 | `		pH->sock = sock;` |
|   274 | 1092 | `		pH->bEof = 0;` |
|   274 | 1093 | `		rc = PH7_OK;` |
|     - | 1094 | `		/* php reports the connection itself, per HOP -- and only for one that` |
|     - | 1095 | `		 * was MADE: a refused dial and a name that does not resolve notify` |
|     - | 1096 | `		 * nothing at all. */` |
|   274 | 1097 | `		PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_CONNECT,` |
|     - | 1098 | `			PHL_STREAM_NOTIFY_SEVERITY_INFO,0,0,0,0,0);` |
|     - | 1099 | `	}` |
|   285 | 1100 | `	SyBlobRelease(&sHost);` |
|   285 | 1101 | `	return rc;` |
|     1 | 1102 | `}` |
|     - | 1103 | `/*` |
|     - | 1104 | ` * Walk the exchange: connect, ask, read, and follow whatever redirects php` |
|     - | 1105 | ` * would follow. Answers PH7_OK with the handle sitting on the body, or -1 with` |
|     - | 1106 | ` * the failure already named.` |
|     - | 1107 | ` */` |
|   246 | 1108 | `static int HttpRun(http_private *pH,const char *zUrl,int nUrl,phl_stream_ctx *pCtx)` |
|     1 | 1109 | `{` |
|     - | 1110 | `	SyBlob sUrl,sReq,sStatus,sLoc,sTarget,sMethod,sBody;` |
|     - | 1111 | `	/* php's wrapper follows a redirect by CALLING ITSELF, and each frame arms` |
|     - | 1112 | `	 * the progress counter with its OWN announced size once the inner one has` |
|     - | 1113 | `	 * come back -- so a script watching a two-hop exchange is told the size` |
|     - | 1114 | `	 * twice, innermost first. This is that stack of frames. */` |
|     - | 1115 | `	SySet sHopSize;` |
|     - | 1116 | `	ph7_value *pOptV;` |
|   247 | 1117 | `	int iRedirLeft = HTTP_MAX_REDIRECTS,bFollow = 1,bIgnoreErr = 0,rc = -1;` |
|   247 | 1118 | `	int bAnyLine = 0,bAnswered = 0;` |
|   247 | 1119 | `	const char *zBody = 0;` |
|   247 | 1120 | `	int nBody = 0;` |
|   247 | 1121 | `	ph7_vm *pVm = pH->pVm;` |
|   247 | 1122 | `	SyBlobInit(&sUrl,&pVm->sAllocator);` |
|   247 | 1123 | `	SyBlobInit(&sReq,&pVm->sAllocator);` |
|   247 | 1124 | `	SyBlobInit(&sStatus,&pVm->sAllocator);` |
|   247 | 1125 | `	SyBlobInit(&sLoc,&pVm->sAllocator);` |
|   247 | 1126 | `	SyBlobInit(&sTarget,&pVm->sAllocator);` |
|   247 | 1127 | `	SyBlobInit(&sMethod,&pVm->sAllocator);` |
|   247 | 1128 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   247 | 1129 | `	SySetInit(&sHopSize,&pVm->sAllocator,sizeof(sxi64));` |
|   247 | 1130 | `	SyBlobAppend(&sUrl,zUrl,(sxu32)nUrl);` |
|     - | 1131 | `	/* php drops the previous exchange's headers before this one starts, so an` |
|     - | 1132 | `	 * open that never reaches a response leaves http_get_last_response_headers()` |
|     - | 1133 | `	 * answering NULL rather than the set before it -- while the CALLER's own` |
|     - | 1134 | `	 * $http_response_header, which is only written when there is something to` |
|     - | 1135 | `	 * write, keeps whatever it held. */` |
|   247 | 1136 | `	PH7_HttpClearResponseHeaders(pVm);` |
|   247 | 1137 | `	pOptV = HttpOpt(pCtx,"max_redirects");` |
|   247 | 1138 | `	if( pOptV ){` |
|    10 | 1139 | `		iRedirLeft = (int)HttpOptInt(pVm,pOptV,HTTP_MAX_REDIRECTS);` |
|     5 | 1140 | `	}` |
|   247 | 1141 | `	pOptV = HttpOpt(pCtx,"follow_location");` |
|   247 | 1142 | `	if( pOptV ){` |
|     4 | 1143 | `		bFollow = HttpOptInt(pVm,pOptV,1) != 0;` |
|     2 | 1144 | `	}` |
|   247 | 1145 | `	pOptV = HttpOpt(pCtx,"ignore_errors");` |
|   247 | 1146 | `	if( pOptV ){` |
|     4 | 1147 | `		bIgnoreErr = HttpOptBool(pVm,pOptV,0);` |
|     2 | 1148 | `	}` |
|   247 | 1149 | `	if( pVm->bHttpGetHeaders ){` |
|     - | 1150 | `		/* get_headers() asks for the HEADERS of whatever answered, so php opens` |
|     - | 1151 | `		 * with this on whatever the caller's context said. */` |
|    36 | 1152 | `		bIgnoreErr = 1;` |
|    18 | 1153 | `	}` |
|   247 | 1154 | `	pOptV = HttpOpt(pCtx,"method");` |
|   247 | 1155 | `	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){` |
|    14 | 1156 | `		SyBlobAppend(&sMethod,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|     7 | 1157 | `	}` |
|   247 | 1158 | `	if( SyBlobLength(&sMethod) < 1 ){` |
|   233 | 1159 | `		SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);` |
|   116 | 1160 | `	}` |
|   247 | 1161 | `	pOptV = HttpOpt(pCtx,"content");` |
|   247 | 1162 | `	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){` |
|     - | 1163 | `		/* COPIED, not pointed at: the exchange now runs userland code between` |
|     - | 1164 | ``		 * here and the request it builds, and a `notification` callback that`` |
|     - | 1165 | `		 * sets this very option would free the bytes under us. */` |
|    12 | 1166 | `		SyBlobAppend(&sBody,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));` |
|    12 | 1167 | `		zBody = (const char *)SyBlobData(&sBody);` |
|    12 | 1168 | `		nBody = (int)SyBlobLength(&sBody);` |
|     6 | 1169 | `	}` |
|   142 | 1170 | `	for(;;){` |
|     - | 1171 | `		SyhttpUri sUri;` |
|   285 | 1172 | `		int iCode,bHasLoc = 0,bFullUri = 0;` |
|   426 | 1173 | `		if( PH7_VmHttpSplitURI(&sUri,(const char *)SyBlobData(&sUrl),` |
|   284 | 1174 | `				SyBlobLength(&sUrl)) != SXRET_OK` |
|   285 | 1175 | `		 \|\| SyStringLength(&sUri.sHost) < 1 ){` |
|   ! 0 | 1176 | `			HttpFail(pVm,"Unable to parse the URL",-1);` |
|   ! 0 | 1177 | `			goto done;` |
|     - | 1178 | `		}` |
|   285 | 1179 | `		pOptV = HttpOpt(pCtx,"request_fulluri");` |
|   285 | 1180 | `		bFullUri = HttpOptBool(pVm,pOptV,0);` |
|   427 | 1181 | `		HttpRequestTarget(&sUri,(const char *)SyBlobData(&sUrl),` |
|   284 | 1182 | `			(int)SyBlobLength(&sUrl),bFullUri,&sTarget);` |
|     - | 1183 | ``		/* php's `proxy` option moves the CONNECTION and leaves everything else`` |
|     - | 1184 | `		 * alone: the request line and the Host header still describe the` |
|     - | 1185 | `		 * origin server. The address is a transport URL of its own. */` |
|   285 | 1186 | `		pOptV = HttpOpt(pCtx,"proxy");` |
|   286 | 1187 | `		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){` |
|     - | 1188 | `			SyhttpUri sProxy;` |
|     3 | 1189 | `			if( PH7_VmHttpSplitURI(&sProxy,(const char *)SyBlobData(&pOptV->sBlob),` |
|     2 | 1190 | `					SyBlobLength(&pOptV->sBlob)) == SXRET_OK` |
|     2 | 1191 | `			 && SyStringLength(&sProxy.sHost) > 0 ){` |
|     2 | 1192 | `				if( HttpConnect(pH,&sProxy,pCtx) != PH7_OK ){` |
|   ! 0 | 1193 | `					goto done;` |
|     - | 1194 | `				}` |
|     1 | 1195 | `			}else{` |
|   ! 0 | 1196 | `				HttpFail(pVm,"Unable to parse the proxy address",-1);` |
|   ! 0 | 1197 | `				goto done;` |
|   ! 0 | 1198 | `			}` |
|   284 | 1199 | `		}else if( HttpConnect(pH,&sUri,pCtx) != PH7_OK ){` |
|    11 | 1200 | `			goto done;` |
|     - | 1201 | `		}` |
|   274 | 1202 | `		SyBlobReset(&sReq);` |
|   411 | 1203 | `		HttpBuildRequest(pVm,pCtx,&sUri,(const char *)SyBlobData(&sTarget),` |
|   274 | 1204 | `			(int)SyBlobLength(&sTarget),&sReq,` |
|   274 | 1205 | `			(const char *)SyBlobData(&sMethod),(int)SyBlobLength(&sMethod),zBody,nBody);` |
|   274 | 1206 | `		if( PH7_NetSendAll(pH->sock,SyBlobData(&sReq),(int)SyBlobLength(&sReq)) != PH7_OK ){` |
|   ! 0 | 1207 | `			HttpFail(pVm,"Connection refused",-1);` |
|   ! 0 | 1208 | `			goto done;` |
|     - | 1209 | `		}` |
|     - | 1210 | `		/* php counts what the STREAM moved, and a write moves as much as a` |
|     - | 1211 | `		 * read: the request is on the counter too. It shows only for a context` |
|     - | 1212 | `		 * whose counter a previous exchange already armed, since this one's` |
|     - | 1213 | `		 * has not been armed yet. */` |
|   274 | 1214 | `		PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)SyBlobLength(&sReq));` |
|   274 | 1215 | `		bAnswered = 1;` |
|   274 | 1216 | `		iCode = HttpReadResponse(pH,&pH->sHdrs,&sStatus,&sLoc,&bHasLoc,&bAnyLine);` |
|   274 | 1217 | `		if( iCode < 0 ){` |
|     2 | 1218 | `			goto done; /* the malformed-header refusal named itself */` |
|     - | 1219 | `		}` |
|   272 | 1220 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow ){` |
|     - | 1221 | `			/* Announced BEFORE the count is spent, so the last hop a limit` |
|     - | 1222 | `			 * allows still reports where it was being sent -- and the address` |
|     - | 1223 | `			 * reported is the one the server WROTE, not the one it resolves` |
|     - | 1224 | `			 * to. */` |
|    69 | 1225 | `			PH7_StreamCtxNotify(pH->pCtx,PHL_STREAM_NOTIFY_REDIRECTED,` |
|    46 | 1226 | `				PHL_STREAM_NOTIFY_SEVERITY_INFO,(const char *)SyBlobData(&sLoc),` |
|    46 | 1227 | `				(int)SyBlobLength(&sLoc),0,0,0);` |
|    23 | 1228 | `		}` |
|   272 | 1229 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && --iRedirLeft >= 1 ){` |
|     - | 1230 | `			SyBlob sNext;` |
|    38 | 1231 | `			sxi64 iHop = pH->iFileSize;` |
|    38 | 1232 | `			SySetPut(&sHopSize,(const void *)&iHop);` |
|    38 | 1233 | `			SyBlobInit(&sNext,&pVm->sAllocator);` |
|    57 | 1234 | `			HttpResolveLocation(&sUri,(const char *)SyBlobData(&sLoc),` |
|    19 | 1235 | `				SyBlobLength(&sLoc),&sNext);` |
|    38 | 1236 | `			SyBlobReset(&sUrl);` |
|    38 | 1237 | `			SyBlobAppend(&sUrl,SyBlobData(&sNext),SyBlobLength(&sNext));` |
|    38 | 1238 | `			SyBlobRelease(&sNext);` |
|     - | 1239 | `			/* php keeps the method and the body only for the two codes that` |
|     - | 1240 | `			 * promise them; every older redirect becomes a bodiless GET. */` |
|    38 | 1241 | `			if( iCode != 307 && iCode != 308 ){` |
|    34 | 1242 | `				SyBlobReset(&sMethod);` |
|    34 | 1243 | `				SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);` |
|    34 | 1244 | `				zBody = 0;` |
|    34 | 1245 | `				nBody = 0;` |
|    17 | 1246 | `			}` |
|    38 | 1247 | `			PH7_NetClose(pH->sock);` |
|    38 | 1248 | `			pH->sock = PH7_NET_INVALID_SOCKET;` |
|    38 | 1249 | `			SyBlobReset(&pH->sRaw);` |
|    38 | 1250 | `			pH->nRawOfft = 0;` |
|    38 | 1251 | `			SyBlobReset(&pH->sOut);` |
|    38 | 1252 | `			pH->nOutOfft = 0;` |
|    38 | 1253 | `			pH->bEof = 0;` |
|    38 | 1254 | `			pH->bChunked = 0;` |
|    38 | 1255 | `			pH->bChunkDone = 0;` |
|    38 | 1256 | `			pH->iChunkLeft = -1;` |
|    38 | 1257 | `			continue;` |
|     - | 1258 | `		}` |
|   234 | 1259 | `		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && !pVm->bHttpGetHeaders ){` |
|     - | 1260 | `			/* The follow above declined because the count ran out. php calls that` |
|     - | 1261 | `			 * a failed open -- except for a headers-only one (get_headers()),` |
|     - | 1262 | `			 * which stops where it is and answers the headers it collected,` |
|     - | 1263 | `			 * silently. */` |
|     6 | 1264 | `			HttpFail(pVm,"Redirection limit reached, aborting",-1);` |
|     6 | 1265 | `			goto done;` |
|     - | 1266 | `		}` |
|   228 | 1267 | `		if( (iCode < 200 \|\| iCode >= 400) && !bIgnoreErr ){` |
|     - | 1268 | `			/* php's own sentence, and the status LINE it carries keeps the` |
|     - | 1269 | `			 * terminator it was read with. */` |
|     - | 1270 | `			char zMsg[320];` |
|    18 | 1271 | `			if( bAnyLine ){` |
|    24 | 1272 | `				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed! %.*s",` |
|    16 | 1273 | `					(int)SyBlobLength(&sStatus),(const char *)SyBlobData(&sStatus));` |
|     8 | 1274 | `			}else{` |
|     - | 1275 | `				/* Nothing came back at all: php names no line, and not even the` |
|     - | 1276 | `				 * space that would separate one. */` |
|     2 | 1277 | `				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed!");` |
|     - | 1278 | `			}` |
|    18 | 1279 | `			HttpFail(pVm,zMsg,-1);` |
|    18 | 1280 | `			goto done;` |
|     - | 1281 | `		}` |
|   210 | 1282 | `		rc = PH7_OK;` |
|   210 | 1283 | `		break;` |
|   ! 0 | 1284 | `	}` |
|   315 | 1285 | `	if( rc == PH7_OK ){` |
|     - | 1286 | `		/* The innermost frame arms the counter and CREDITS what came in with` |
|     - | 1287 | `		 * the headers; then every frame the redirects opened does the same on` |
|     - | 1288 | `		 * its way out, with the size IT announced. A chunked body is credited` |
|     - | 1289 | `		 * by the decoder, which is what makes the number the bytes a script` |
|     - | 1290 | `		 * will read rather than the framing they arrived in. */` |
|   210 | 1291 | `		sxi64 *aHop = (sxi64 *)SySetBasePtr(&sHopSize);` |
|   210 | 1292 | `		sxu32 i = SySetUsed(&sHopSize);` |
|   210 | 1293 | `		pH->bBody = 1;` |
|   210 | 1294 | `		PH7_StreamCtxProgressInit(pH->pCtx,pH->iFileSize);` |
|   210 | 1295 | `		if( pH->bChunked ){` |
|     6 | 1296 | `			PH7_StreamCtxProgressAdd(pH->pCtx,(sxi64)HttpDecodeBuffered(pH));` |
|     3 | 1297 | `		}else{` |
|   204 | 1298 | `			PH7_StreamCtxProgressAdd(pH->pCtx,HttpBuffered(pH));` |
|     - | 1299 | `		}` |
|   248 | 1300 | `		while( i > 0 ){` |
|    38 | 1301 | `			i--;` |
|    38 | 1302 | `			PH7_StreamCtxProgressInit(pH->pCtx,aHop[i]);` |
|    38 | 1303 | `			PH7_StreamCtxProgressAdd(pH->pCtx,HttpBuffered(pH));` |
|   ! 0 | 1304 | `		}` |
|   105 | 1305 | `	}` |
|   ! 0 | 1306 | `done:` |
|     - | 1307 | `	/* The headers belong to the SCRIPT whether the open worked or not: php` |
|     - | 1308 | `	 * fills $http_response_header for a 404 exactly as it does for a 200, and` |
|     - | 1309 | `	 * leaves it untouched for an exchange that never reached a response. */` |
|   247 | 1310 | `	if( bAnswered ){` |
|   236 | 1311 | `		PH7_HttpPublishHeaders(pVm,&pH->sHdrs);` |
|   118 | 1312 | `	}` |
|   247 | 1313 | `	SyBlobRelease(&sUrl);` |
|   247 | 1314 | `	SyBlobRelease(&sReq);` |
|   247 | 1315 | `	SyBlobRelease(&sStatus);` |
|   247 | 1316 | `	SyBlobRelease(&sLoc);` |
|   247 | 1317 | `	SyBlobRelease(&sTarget);` |
|   247 | 1318 | `	SyBlobRelease(&sMethod);` |
|   247 | 1319 | `	SyBlobRelease(&sBody);` |
|   247 | 1320 | `	SySetRelease(&sHopSize);` |
|   247 | 1321 | `	return rc;` |
|     1 | 1322 | `}` |
|     - | 1323 | `/* ------------------------------------------------------------------------- */` |
|     - | 1324 | `/* The device                                                                  */` |
|     - | 1325 | `/* ------------------------------------------------------------------------- */` |
|   264 | 1326 | `static int HttpStream_Open(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|     1 | 1327 | `{` |
|   265 | 1328 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     - | 1329 | `	http_private *pH;` |
|     - | 1330 | `	SyBlob sUrl;` |
|     - | 1331 | `	int rc;` |
|   265 | 1332 | `	if( pVm == 0 ){` |
|   ! 0 | 1333 | `		return -1;` |
|     - | 1334 | `	}` |
|   264 | 1335 | `	if( (iMode & (PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_TRUNC` |
|   133 | 1336 | `			\|PH7_IO_OPEN_EXCL)) != 0 ){` |
|     - | 1337 | ``		/* php's screen here is TEXTUAL -- `strpbrk(mode, "awx+")` -- and these`` |
|     - | 1338 | `		 * four flags are what those four characters parse to, which is why` |
|     - | 1339 | ``		 * `c` (create-if-absent, and nothing else) is the one write-ish mode`` |
|     - | 1340 | `		 * php lets through: it connects and hands back a readable stream like` |
|     - | 1341 | `		 * any other. Said before a socket is made. */` |
|    19 | 1342 | `		PH7_StreamSetOpenError(pVm,"HTTP wrapper does not support writeable connections");` |
|    19 | 1343 | `		return -1;` |
|     - | 1344 | `	}` |
|   247 | 1345 | `	pH = (http_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(http_private));` |
|   247 | 1346 | `	if( pH == 0 ){` |
|   ! 0 | 1347 | `		return -1;` |
|     - | 1348 | `	}` |
|   247 | 1349 | `	SyZero(pH,sizeof(http_private));` |
|   247 | 1350 | `	pH->pVm = pVm;` |
|   247 | 1351 | `	pH->sock = PH7_NET_INVALID_SOCKET;` |
|   247 | 1352 | `	pH->iChunkLeft = -1;` |
|   247 | 1353 | `	SyBlobInit(&pH->sRaw,&pVm->sAllocator);` |
|   247 | 1354 | `	SyBlobInit(&pH->sOut,&pVm->sAllocator);` |
|   247 | 1355 | `	SyBlobInit(&pH->sHdrs,&pVm->sAllocator);` |
|     - | 1356 | `	/* The context this open was armed with, kept for the whole life of the` |
|     - | 1357 | `	 * handle: the progress notifications belong to the READS, which happen` |
|     - | 1358 | `	 * long after PH7_StreamOpenHandle has disarmed pVm->pOpenCtx. The VM owns` |
|     - | 1359 | `	 * every context it hands out, so the pointer outlives us. */` |
|   247 | 1360 | `	pH->pCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     - | 1361 | `	/* PH7_VmGetStreamDevice() hands the wrapper what is left after the scheme;` |
|     - | 1362 | `	 * every redirect below is resolved against a WHOLE url, so it goes back on. */` |
|   247 | 1363 | `	SyBlobInit(&sUrl,&pVm->sAllocator);` |
|   247 | 1364 | `	SyBlobAppend(&sUrl,"http://",sizeof("http://")-1);` |
|   247 | 1365 | `	SyBlobAppend(&sUrl,zName,(sxu32)SyStrlen(zName));` |
|   370 | 1366 | `	rc = HttpRun(pH,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl),` |
|   246 | 1367 | `		(phl_stream_ctx *)pVm->pOpenCtx);` |
|   247 | 1368 | `	SyBlobRelease(&sUrl);` |
|   247 | 1369 | `	if( rc != PH7_OK ){` |
|    37 | 1370 | `		HttpFree(pH);` |
|    37 | 1371 | `		return -1;` |
|     - | 1372 | `	}` |
|   210 | 1373 | `	*ppHandle = (void *)pH;` |
|   210 | 1374 | `	return PH7_OK;` |
|   133 | 1375 | `}` |
|   210 | 1376 | `static void HttpStream_Close(void *pHandle)` |
|   ! 0 | 1377 | `{` |
|   210 | 1378 | `	HttpFree((http_private *)pHandle);` |
|   210 | 1379 | `}` |
|   358 | 1380 | `static ph7_int64 HttpStream_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|   ! 0 | 1381 | `{` |
|   358 | 1382 | `	http_private *pH = (http_private *)pHandle;` |
|     - | 1383 | `	SyBlob *pSrc;` |
|     - | 1384 | `	sxu32 *pOfft,nHave,nTake;` |
|   358 | 1385 | `	if( pH == 0 \|\| nRead < 1 ){` |
|   ! 0 | 1386 | `		return -1;` |
|     - | 1387 | `	}` |
|   358 | 1388 | `	if( pH->bChunked ){` |
|    30 | 1389 | `		HttpPumpChunks(pH,(sxu32)nRead);` |
|    30 | 1390 | `		pSrc = &pH->sOut;` |
|    30 | 1391 | `		pOfft = &pH->nOutOfft;` |
|    15 | 1392 | `	}else{` |
|   328 | 1393 | `		while( HttpRawLeft(pH) < (sxu32)nRead && !pH->bEof ){` |
|   164 | 1394 | `			if( HttpFill(pH) < 1 ){` |
|   164 | 1395 | `				break;` |
|     - | 1396 | `			}` |
|   ! 0 | 1397 | `		}` |
|   328 | 1398 | `		pSrc = &pH->sRaw;` |
|   328 | 1399 | `		pOfft = &pH->nRawOfft;` |
|     - | 1400 | `	}` |
|   358 | 1401 | `	nHave = SyBlobLength(pSrc) > *pOfft ? SyBlobLength(pSrc) - *pOfft : 0;` |
|   358 | 1402 | `	if( nHave < 1 ){` |
|   172 | 1403 | `		return 0;` |
|     - | 1404 | `	}` |
|   186 | 1405 | `	nTake = nHave < (sxu32)nRead ? nHave : (sxu32)nRead;` |
|   186 | 1406 | `	SyMemcpy(&((const char *)SyBlobData(pSrc))[*pOfft],pBuffer,nTake);` |
|   186 | 1407 | `	*pOfft += nTake;` |
|   186 | 1408 | `	pH->iPos += nTake;` |
|   186 | 1409 | `	return (ph7_int64)nTake;` |
|   179 | 1410 | `}` |
|     - | 1411 | `/* php's http stream is not writable; a write on one is the same false every` |
|     - | 1412 | ` * read-only device answers. */` |
|   ! 0 | 1413 | `static ph7_int64 HttpStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|   ! 0 | 1414 | `{` |
|   ! 0 | 1415 | `	SXUNUSED(pHandle);` |
|   ! 0 | 1416 | `	SXUNUSED(pBuf);` |
|   ! 0 | 1417 | `	SXUNUSED(nWrite);` |
|   ! 0 | 1418 | `	return -1;` |
|   ! 0 | 1419 | `}` |
|     - | 1420 | `/* Bytes the script has taken. php's http stream is not seekable, so this is the` |
|     - | 1421 | ` * only position it has. */` |
|     6 | 1422 | `static ph7_int64 HttpStream_Tell(void *pHandle)` |
|   ! 0 | 1423 | `{` |
|     6 | 1424 | `	http_private *pH = (http_private *)pHandle;` |
|     6 | 1425 | `	return pH ? pH->iPos : -1;` |
|   ! 0 | 1426 | `}` |
|     - | 1427 | `PH7_PRIVATE const ph7_io_stream sHTTP_Stream = {` |
|     - | 1428 | `	"http",` |
|     - | 1429 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1430 | `	HttpStream_Open,  /* xOpen */` |
|     - | 1431 | `	0,                /* xOpenDir */` |
|     - | 1432 | `	HttpStream_Close, /* xClose */` |
|     - | 1433 | `	0,                /* xCloseDir */` |
|     - | 1434 | `	HttpStream_Read,  /* xRead */` |
|     - | 1435 | `	0,                /* xReadDir */` |
|     - | 1436 | `	HttpStream_Write, /* xWrite */` |
|     - | 1437 | `	0,                /* xSeek */` |
|     - | 1438 | `	0,                /* xLock */` |
|     - | 1439 | `	0,                /* xRewindDir */` |
|     - | 1440 | `	HttpStream_Tell,  /* xTell */` |
|     - | 1441 | `	0,                /* xTrunc */` |
|     - | 1442 | `	0,                /* xSync */` |
|     - | 1443 | `	0                 /* xStat */` |
|     - | 1444 | `};` |
|     - | 1445 | `/* Is this the http:// device? Asked by the metadata reader, which labels it` |
|     - | 1446 | ` * apart, and by feof(), which reads the handle's own end. */` |
| 54466 | 1447 | `PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream)` |
|     5 | 1448 | `{` |
| 54471 | 1449 | `	return pStream == &sHTTP_Stream;` |
|     5 | 1450 | `}` |
|     - | 1451 | `/*` |
|     - | 1452 | `` * php's `wrapper_data` for an http handle: the response headers of the exchange`` |
|     - | 1453 | ` * THIS handle made, which is not the same thing as the VM's last set (a second` |
|     - | 1454 | ` * request has since overwritten that one).` |
|     - | 1455 | ` */` |
|     4 | 1456 | `PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle)` |
|   ! 0 | 1457 | `{` |
|     4 | 1458 | `	http_private *pH = (http_private *)pHandle;` |
|     - | 1459 | `	ph7_value *pArr,*pLine;` |
|     - | 1460 | `	const char *zIn,*zEnd,*zCur;` |
|     4 | 1461 | `	if( pH == 0 ){` |
|   ! 0 | 1462 | `		return 0;` |
|     - | 1463 | `	}` |
|     4 | 1464 | `	pArr = ph7_new_array(pVm);` |
|     4 | 1465 | `	pLine = ph7_new_scalar(pVm);` |
|     4 | 1466 | `	if( pArr == 0 \|\| pLine == 0 ){` |
|   ! 0 | 1467 | `		if( pArr ){ ph7_release_value(pVm,pArr); }` |
|   ! 0 | 1468 | `		if( pLine ){ ph7_release_value(pVm,pLine); }` |
|   ! 0 | 1469 | `		return 0;` |
|     - | 1470 | `	}` |
|     4 | 1471 | `	zIn = (const char *)SyBlobData(&pH->sHdrs);` |
|     4 | 1472 | `	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];` |
|    20 | 1473 | `	while( zIn < zEnd ){` |
|    16 | 1474 | `		zCur = zIn;` |
|   260 | 1475 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
|   244 | 1476 | `			zCur++;` |
|   ! 0 | 1477 | `		}` |
|    16 | 1478 | `		ph7_value_string(pLine,zIn,(int)(zCur - zIn));` |
|    16 | 1479 | `		ph7_array_add_elem(pArr,0,pLine);` |
|    16 | 1480 | `		ph7_value_reset_string_cursor(pLine);` |
|    16 | 1481 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 | 1482 | `	}` |
|     4 | 1483 | `	ph7_release_value(pVm,pLine);` |
|     4 | 1484 | `	return pArr;` |
|     2 | 1485 | `}` |
|     - | 1486 | `/* The connection an http handle is reading, so stream_select() can wait on the` |
|     - | 1487 | ` * same descriptor php's does. */` |
|   ! 0 | 1488 | `PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle)` |
|   ! 0 | 1489 | `{` |
|   ! 0 | 1490 | `	http_private *pH = (http_private *)pHandle;` |
|   ! 0 | 1491 | `	return pH ? &pH->sock : 0;` |
|   ! 0 | 1492 | `}` |
|     - | 1493 | `/*` |
|     - | 1494 | `` * php's `unread_bytes`: what the wrapper has already pulled off the socket and`` |
|     - | 1495 | ` * the script has not taken. php reads a block at a time too, so a small` |
|     - | 1496 | ` * response is fully buffered by the time the header block has been parsed --` |
|     - | 1497 | ` * and, exactly as in php, a big one is not.` |
|     - | 1498 | ` */` |
|     4 | 1499 | `PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle)` |
|   ! 0 | 1500 | `{` |
|     4 | 1501 | `	http_private *pH = (http_private *)pHandle;` |
|     4 | 1502 | `	if( pH == 0 ){` |
|   ! 0 | 1503 | `		return 0;` |
|     - | 1504 | `	}` |
|     4 | 1505 | `	if( pH->bChunked ){` |
|   ! 0 | 1506 | `		return SyBlobLength(&pH->sOut) > pH->nOutOfft` |
|   ! 0 | 1507 | `			? SyBlobLength(&pH->sOut) - pH->nOutOfft : 0;` |
|     - | 1508 | `	}` |
|     4 | 1509 | `	return HttpRawLeft(pH);` |
|     2 | 1510 | `}` |
|     - | 1511 | `/* php's feof() for an http handle: the peer has closed AND nothing is left in` |
|     - | 1512 | ` * hand. */` |
|    32 | 1513 | `PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle)` |
|   ! 0 | 1514 | `{` |
|    32 | 1515 | `	http_private *pH = (http_private *)pHandle;` |
|    32 | 1516 | `	if( pH == 0 ){` |
|   ! 0 | 1517 | `		return 1;` |
|     - | 1518 | `	}` |
|    32 | 1519 | `	if( pH->bChunked ){` |
|    24 | 1520 | `		return pH->bChunkDone && SyBlobLength(&pH->sOut) <= pH->nOutOfft;` |
|     - | 1521 | `	}` |
|     8 | 1522 | `	return pH->bEof && HttpRawLeft(pH) == 0;` |
|    16 | 1523 | `}` |
|     - | 1524 | `/* ------------------------------------------------------------------------- */` |
|     - | 1525 | `/* get_headers()                                                               */` |
|     - | 1526 | `/* ------------------------------------------------------------------------- */` |
|     - | 1527 | `/*` |
|     - | 1528 | ` * array\|false get_headers(string $url, bool $associative = false,` |
|     - | 1529 | ` *                         ?resource $context = null)` |
|     - | 1530 | ` *` |
|     - | 1531 | ` * php's one function for "ask that URL what it answers, and nothing else". It` |
|     - | 1532 | `` * is the http:// wrapper with `ignore_errors` forced on -- a 404 is a set of`` |
|     - | 1533 | ` * headers, not a failure -- opened and closed without a byte of the body read.` |
|     - | 1534 | ` *` |
|     - | 1535 | ``  * Two refusals of its own: an EMPTY url is php's `Path must not be empty` `` |
|     - | 1536 | ` * ValueError, before anything is looked up; and a url that does not resolve to` |
|     - | 1537 | ` * the HTTP wrapper specifically -- a path, an unknown scheme, even data://,` |
|     - | 1538 | ` * which IS a url wrapper -- is a warning and false.` |
|     - | 1539 | ` *` |
|     - | 1540 | `` * `$associative` reshapes the SAME lines: a line with no colon in it is a status`` |
|     - | 1541 | `` * line and takes the next INTEGER key (which is how `abcdefghi 200 OK` gets one`` |
|     - | 1542 | `` * and `HTTP/1.1 200 OK: weird` does not), everything else is keyed by the text`` |
|     - | 1543 | ` * before its first colon with the value left-trimmed after it, and a name that` |
|     - | 1544 | ` * arrives twice -- across a redirect chain included -- collects into an ARRAY.` |
|     - | 1545 | ` */` |
|    46 | 1546 | `static int PH7_builtin_get_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 1547 | `{` |
|     - | 1548 | `	const ph7_io_stream *pStream;` |
|     - | 1549 | `	phl_stream_ctx *pCtxRes;` |
|     - | 1550 | `	ph7_value *pArr,*pKey,*pVal,*pLine;` |
|     - | 1551 | `	http_private *pH;` |
|     - | 1552 | `	void *pHandle;` |
|     - | 1553 | `	const char *zUrl,*zIn,*zEnd,*zCur;` |
|    46 | 1554 | `	int nUrl = 0,bAssoc = 0,bThrew = 0,iStatus = 0;` |
|     - | 1555 | `	/* The declared signature is the screen: a non-string $url is its TypeError` |
|     - | 1556 | `	 * and a fourth argument its ArgumentCountError, both before this runs. */` |
|    46 | 1557 | `	zUrl = ph7_value_to_string(apArg[0],&nUrl);` |
|    46 | 1558 | `	if( nUrl < 1 ){` |
|     2 | 1559 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - | 1560 | `	}` |
|    44 | 1561 | `	if( nArg > 1 ){` |
|    18 | 1562 | `		bAssoc = ph7_value_to_bool(apArg[1]);` |
|     9 | 1563 | `	}` |
|    44 | 1564 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|    44 | 1565 | `	if( bThrew ){` |
|   ! 0 | 1566 | `		return PH7_OK;` |
|     - | 1567 | `	}` |
|     - | 1568 | `	{` |
|     - | 1569 | `		/* php's screen is the wrapper's URL bit, not its identity: php:// and a` |
|     - | 1570 | `		 * plain path are the refusal, and data:// -- which php DOES count as a` |
|     - | 1571 | `		 * url wrapper -- goes through and answers false further down, silently,` |
|     - | 1572 | `		 * because a stream with no response headers has nothing to give. */` |
|    44 | 1573 | `		const char *zProbe = zUrl;` |
|    44 | 1574 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,nUrl);` |
|    44 | 1575 | `		if( pStream == 0 ){` |
|     - | 1576 | `			/* php names the missing wrapper first -- and only that: the` |
|     - | 1577 | `			 * failed-open line beside it belongs to an open, and get_headers()` |
|     - | 1578 | `			 * never reaches one. */` |
|     2 | 1579 | `			VfsThrowUnknownWrapperWarning(pCtx,zUrl);` |
|     1 | 1580 | `		}` |
|    44 | 1581 | `		if( pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream) ){` |
|     6 | 1582 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|     - | 1583 | `				"This function may only be used against URLs");` |
|     6 | 1584 | `			ph7_result_bool(pCtx,0);` |
|     6 | 1585 | `			return PH7_OK;` |
|     - | 1586 | `		}` |
|    38 | 1587 | `		zUrl = zProbe;` |
|     - | 1588 | `	}` |
|     - | 1589 | ``	/* php builds its own context over the caller's, with `ignore_errors` on: a`` |
|     - | 1590 | `	 * status php would refuse to open is a set of headers here. The flag rides` |
|     - | 1591 | `	 * the VM rather than a synthesized context so the caller's own options --` |
|     - | 1592 | `	 * its method, its headers -- reach the request unchanged. */` |
|    38 | 1593 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|    38 | 1594 | `	pCtx->pVm->bHttpGetHeaders = 1;` |
|    57 | 1595 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUrl,PH7_IO_OPEN_RDONLY,` |
|    19 | 1596 | `		FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|    38 | 1597 | `	pCtx->pVm->bHttpGetHeaders = 0;` |
|    38 | 1598 | `	if( pHandle == 0 ){` |
|     2 | 1599 | `		VfsThrowOpenWarning(pCtx,zUrl);` |
|     2 | 1600 | `		ph7_result_bool(pCtx,0);` |
|     2 | 1601 | `		return PH7_OK;` |
|     - | 1602 | `	}` |
|    36 | 1603 | `	if( !PH7_HttpStreamIs(pStream) ){` |
|     - | 1604 | `		/* A url wrapper of some other kind opened fine and has no response` |
|     - | 1605 | `		 * headers to answer with. */` |
|     2 | 1606 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     2 | 1607 | `		ph7_result_bool(pCtx,0);` |
|     2 | 1608 | `		return PH7_OK;` |
|     - | 1609 | `	}` |
|    34 | 1610 | `	pH = (http_private *)pHandle;` |
|    34 | 1611 | `	pArr = ph7_context_new_array(pCtx);` |
|    34 | 1612 | `	pKey = ph7_context_new_scalar(pCtx);` |
|    34 | 1613 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    34 | 1614 | `	pLine = ph7_context_new_scalar(pCtx);` |
|    34 | 1615 | `	if( pArr == 0 \|\| pKey == 0 \|\| pVal == 0 \|\| pLine == 0 ){` |
|   ! 0 | 1616 | `		HttpStream_Close(pHandle);` |
|   ! 0 | 1617 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1618 | `		return PH7_OK;` |
|     - | 1619 | `	}` |
|    34 | 1620 | `	zIn = (const char *)SyBlobData(&pH->sHdrs);` |
|    34 | 1621 | `	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];` |
|   216 | 1622 | `	while( zIn < zEnd ){` |
|     - | 1623 | `		sxu32 nColon,nLine;` |
|   182 | 1624 | `		zCur = zIn;` |
|  2792 | 1625 | `		while( zCur < zEnd && zCur[0] != '\n' ){` |
|  2610 | 1626 | `			zCur++;` |
|   ! 0 | 1627 | `		}` |
|   182 | 1628 | `		nLine = (sxu32)(zCur - zIn);` |
|  2004 | 1629 | `		for( nColon = 0 ; nColon < nLine && zIn[nColon] != ':' ; ++nColon ){` |
|     - | 1630 | `			;` |
|   911 | 1631 | `		}` |
|   182 | 1632 | `		if( !bAssoc ){` |
|   102 | 1633 | `			ph7_value_string(pLine,zIn,(int)nLine);` |
|   102 | 1634 | `			ph7_array_add_elem(pArr,0,pLine);` |
|   102 | 1635 | `			ph7_value_reset_string_cursor(pLine);` |
|   131 | 1636 | `		}else if( nColon >= nLine ){` |
|     - | 1637 | `			/* No colon: a status line, and php numbers those from 0 whatever` |
|     - | 1638 | `			 * they say. */` |
|    14 | 1639 | `			ph7_value_string(pLine,zIn,(int)nLine);` |
|    14 | 1640 | `			ph7_value_int(pKey,iStatus++);` |
|    14 | 1641 | `			ph7_array_add_elem(pArr,pKey,pLine);` |
|    14 | 1642 | `			ph7_value_reset_string_cursor(pLine);` |
|     7 | 1643 | `		}else{` |
|    66 | 1644 | `			sxu32 i = nColon + 1;` |
|     - | 1645 | `			ph7_value *pOld;` |
|   157 | 1646 | `			while( i < nLine && (zIn[i] == ' ' \|\| zIn[i] == '\t') ){` |
|    58 | 1647 | `				i++;` |
|   ! 0 | 1648 | `			}` |
|    66 | 1649 | `			ph7_value_string(pKey,zIn,(int)nColon);` |
|    66 | 1650 | `			ph7_value_string(pVal,&zIn[i],(int)(nLine - i));` |
|    66 | 1651 | `			pOld = ph7_array_fetch(pArr,zIn,(int)nColon);` |
|    66 | 1652 | `			if( pOld == 0 ){` |
|    58 | 1653 | `				ph7_array_add_elem(pArr,pKey,pVal);` |
|    37 | 1654 | `			}else if( pOld->iFlags & MEMOBJ_HASHMAP ){` |
|     - | 1655 | `				/* Already a list of its own: this is the third and later. */` |
|   ! 0 | 1656 | `				ph7_array_add_elem(pOld,0,pVal);` |
|   ! 0 | 1657 | `			}else{` |
|     - | 1658 | `				/* php turns the pair into a LIST the moment a name repeats. */` |
|     8 | 1659 | `				ph7_value *pList = ph7_context_new_array(pCtx);` |
|     8 | 1660 | `				if( pList ){` |
|     8 | 1661 | `					ph7_array_add_elem(pList,0,pOld);` |
|     8 | 1662 | `					ph7_array_add_elem(pList,0,pVal);` |
|     8 | 1663 | `					ph7_array_add_elem(pArr,pKey,pList);` |
|     4 | 1664 | `				}` |
|     - | 1665 | `			}` |
|    66 | 1666 | `			ph7_value_reset_string_cursor(pKey);` |
|    66 | 1667 | `			ph7_value_reset_string_cursor(pVal);` |
|     - | 1668 | `		}` |
|   182 | 1669 | `		zIn = zCur < zEnd ? &zCur[1] : zEnd;` |
|   ! 0 | 1670 | `	}` |
|    34 | 1671 | `	HttpStream_Close(pHandle);` |
|    34 | 1672 | `	ph7_result_value(pCtx,pArr);` |
|    34 | 1673 | `	return PH7_OK;` |
|    23 | 1674 | `}` |
|     - | 1675 | `#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */` |
|     - | 1676 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 1677 | `/*` |
|     - | 1678 | ` * ?array http_get_last_response_headers()` |
|     - | 1679 | ` *` |
|     - | 1680 | `` * php 8.4's modern spelling of `$http_response_header`, and the same store: the`` |
|     - | 1681 | ` * response headers of the last http:// exchange, or NULL when nothing has been` |
|     - | 1682 | ` * recorded since the last clear.` |
|     - | 1683 | ` */` |
|    12 | 1684 | `static int PH7_builtin_http_get_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1685 | `{` |
|     - | 1686 | `	ph7_value *pArr;` |
|     6 | 1687 | `	SXUNUSED(nArg);` |
|     6 | 1688 | `	SXUNUSED(apArg);` |
|    13 | 1689 | `	pArr = PH7_HttpHeaderArray(pCtx->pVm,&pCtx->pVm->sHttpRespHdrs);` |
|    13 | 1690 | `	if( pArr == 0 ){` |
|     9 | 1691 | `		ph7_result_null(pCtx);` |
|     9 | 1692 | `		return PH7_OK;` |
|     - | 1693 | `	}` |
|     4 | 1694 | `	ph7_result_value(pCtx,pArr);` |
|     4 | 1695 | `	ph7_release_value(pCtx->pVm,pArr);` |
|     4 | 1696 | `	return PH7_OK;` |
|     7 | 1697 | `}` |
|     - | 1698 | `/*` |
|     - | 1699 | ` * void http_clear_last_response_headers()` |
|     - | 1700 | ` *` |
|     - | 1701 | ` * Drops the store, so the getter above answers NULL rather than the array it` |
|     - | 1702 | ` * was answering a moment ago.` |
|     - | 1703 | ` */` |
|     6 | 1704 | `static int PH7_builtin_http_clear_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1705 | `{` |
|     3 | 1706 | `	SXUNUSED(nArg);` |
|     3 | 1707 | `	SXUNUSED(apArg);` |
|     7 | 1708 | `	PH7_HttpClearResponseHeaders(pCtx->pVm);` |
|     7 | 1709 | `	ph7_result_null(pCtx);` |
|     7 | 1710 | `	return PH7_OK;` |
|     1 | 1711 | `}` |
|     - | 1712 | `/*` |
|     - | 1713 | ` * The three names a script asks an exchange about. The two getters are in every` |
|     - | 1714 | ` * build -- the store they read is -- and get_headers() goes with the wrapper it` |
|     - | 1715 | ` * is the one door of.` |
|     - | 1716 | ` */` |
|  5619 | 1717 | `PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm)` |
|     5 | 1718 | `{` |
|  5624 | 1719 | `	ph7_create_function(&(*pVm),"http_get_last_response_headers",` |
|     - | 1720 | `		PH7_builtin_http_get_last_response_headers,0);` |
|  5624 | 1721 | `	ph7_create_function(&(*pVm),"http_clear_last_response_headers",` |
|     - | 1722 | `		PH7_builtin_http_clear_last_response_headers,0);` |
|     - | 1723 | `#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)` |
|  5624 | 1724 | `	ph7_create_function(&(*pVm),"get_headers",PH7_builtin_get_headers,0);` |
|     - | 1725 | `#endif` |
|  5624 | 1726 | `}` |
|     - | 1727 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1728 |  |
