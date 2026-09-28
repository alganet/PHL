/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * The http:// stream wrapper -- php's one built-in PROTOCOL wrapper, and the
 * device behind `file_get_contents('http://...')`, `fopen('http://...')`,
 * `get_headers()` and every reader that takes a URL.
 *
 * It is a WRAPPER rather than a transport: tcp:// hands a script the bytes of a
 * socket, and this speaks a request over one and hands back the RESPONSE BODY,
 * with everything else about the exchange -- the status line, the response
 * headers, the redirect chain it walked -- published beside the handle as
 * `$http_response_header` / `stream_get_meta_data()['wrapper_data']`.
 *
 * Four contracts are derived from php 8.5 rather than from the RFCs, because
 * php's wrapper does not implement the RFCs:
 *
 *   1. THE REQUEST'S HEADER ORDER is fixed, and the `http` context options and
 *      two ini directives feed named slots inside it. A user header wins over
 *      the slot it names -- a `Host:` of one's own suppresses php's, a
 *      `Connection:` suppresses `Connection: close`, an `Authorization:`
 *      suppresses the one the URL's userinfo would have produced.
 *   2. THE STATUS LINE IS `atoi(line + 9)`. php checks no prefix at all, so
 *      `HTTP/2.0 200 OK` is a 200 and `HTTP/2 200 OK` is a 0 (its digits sit at
 *      the wrong offset), and any line under ten bytes has no code. Anything
 *      outside 200..399 is `HTTP request failed! <the raw line>` unless
 *      `ignore_errors` says otherwise; a 1xx is DISCARDED, headers and all, and
 *      the next response read in its place.
 *   3. THE BODY ENDS WHERE THE SOCKET DOES. `Content-Length` is announced and
 *      never enforced -- php hands back the short body a lying header promised
 *      100 bytes of, and the long one it promised 2 -- and the single framing
 *      php DOES apply is `Transfer-Encoding: chunked`, whose header line is then
 *      not part of the response headers at all.
 *   4. A RELATIVE `Location:` IS RESOLVED AGAINST THE PATH, not against the
 *      directory: php keeps everything up to and including the last `/` and then
 *      joins with ANOTHER `/`, so a redirect from `/a/b` to `rel` is `/a//rel`.
 *      The exceptions are a path that is just `/` (or empty), where the join
 *      is direct, and a Location of at most ONE byte, which is not joined at
 *      all but put under the root.
 *
 * What is NOT here is https://: php's is this wrapper over the ssl:// transport,
 * which this build has not got (PLAN.md §5, and §10 for the crypto policy).
 */
/*
 * The response headers of the LAST http exchange, kept on the VM because two
 * consumers outlive the handle: php writes `$http_response_header` into the
 * scope that called the opener (whether the open SUCCEEDED or not -- a 404 is a
 * failed open with a full set of headers), and php 8.4's
 * http_get_last_response_headers() answers them until something clears it.
 */
PH7_PRIVATE void PH7_HttpPublishHeaders(ph7_vm *pVm,SyBlob *pLines)
{
	SyBlobReset(&pVm->sHttpRespHdrs);
	if( pLines && SyBlobLength(pLines) > 0 ){
		SyBlobAppend(&pVm->sHttpRespHdrs,SyBlobData(pLines),SyBlobLength(pLines));
	}
	pVm->bHttpRespHdrs = 1;
	pVm->bHttpRespFresh = 1;
}
/*
 * The stored lines as php's array of strings, or 0 when nothing has been
 * recorded at all. One entry per line: an exchange that read nothing answers the
 * EMPTY array, and one that read a single blank line answers one empty entry --
 * two states php tells apart and a '\n'-separated store only can because every
 * line carries its own terminator.
 */
PH7_PRIVATE ph7_value * PH7_HttpHeaderArray(ph7_vm *pVm,SyBlob *pLines)
{
	ph7_value *pArr,*pLine;
	const char *zIn,*zEnd,*zCur;
	if( pLines == 0 || !pVm->bHttpRespHdrs ){
		return 0;
	}
	pArr = ph7_new_array(pVm);
	pLine = ph7_new_scalar(pVm);
	if( pArr == 0 || pLine == 0 ){
		if( pArr ){ ph7_release_value(pVm,pArr); }
		if( pLine ){ ph7_release_value(pVm,pLine); }
		return 0;
	}
	/* Every recorded line carries a trailing '\n', which is what tells ONE
	 * empty line (php's answer for a connection that ended where a status line
	 * was due) from NO lines at all. */
	zIn = (const char *)SyBlobData(pLines);
	zEnd = &zIn[SyBlobLength(pLines)];
	while( zIn < zEnd ){
		zCur = zIn;
		while( zCur < zEnd && zCur[0] != '\n' ){
			zCur++;
		}
		ph7_value_string(pLine,zIn,(int)(zCur - zIn));
		ph7_array_add_elem(pArr,0,pLine);
		ph7_value_reset_string_cursor(pLine);
		zIn = zCur < zEnd ? &zCur[1] : zEnd;
	}
	ph7_release_value(pVm,pLine);
	return pArr;
}
/*
 * `$http_response_header`, written into the frame that called the opener. php
 * writes it from the stream layer, so every door -- file_get_contents(), fopen(),
 * file(), readfile(), copy(), get_headers() -- has it, and an open that never
 * reached a response leaves the caller's previous value alone.
 */
PH7_PRIVATE void PH7_HttpFlushResponseHeaders(ph7_vm *pVm)
{
	static const char zVar[] = "http_response_header";
	ph7_value *pArr,*pSlot;
	SyString sName;
	if( !pVm->bHttpRespFresh ){
		return;
	}
	pVm->bHttpRespFresh = 0;
	pArr = PH7_HttpHeaderArray(pVm,&pVm->sHttpRespHdrs);
	if( pArr == 0 ){
		return;
	}
	SyStringInitFromBuf(&sName,zVar,sizeof(zVar)-1);
	pSlot = VmExtractMemObj(pVm,&sName,TRUE,TRUE);
	if( pSlot ){
		PH7_MemObjStore(pArr,pSlot);
	}
	ph7_release_value(pVm,pArr);
}
/* http_clear_last_response_headers(): php drops the store, and the getter then
 * answers NULL rather than an empty array. */
PH7_PRIVATE void PH7_HttpClearResponseHeaders(ph7_vm *pVm)
{
	SyBlobReset(&pVm->sHttpRespHdrs);
	pVm->bHttpRespHdrs = 0;
	pVm->bHttpRespFresh = 0;
}
#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)
/*
 * An open http:// handle.
 *
 * The socket stays open for the whole life of the handle -- php's body IS the
 * rest of the connection -- and sRaw holds whatever came off it with the header
 * block, since a single recv() reads past the blank line more often than not.
 */
typedef struct http_private http_private;
struct http_private
{
	ph7_vm *pVm;
	ph7_socket sock;      /* the connection the response is still arriving on */
	SyBlob sRaw;          /* bytes read off the socket and not yet consumed */
	sxu32 nRawOfft;       /* read cursor inside sRaw */
	SyBlob sOut;          /* DECHUNKED bytes waiting for the reader (chunked only) */
	sxu32 nOutOfft;       /* read cursor inside sOut */
	ph7_int64 iPos;       /* bytes handed to the script: php's ftell() for this stream */
	sxu8 bEof;            /* the peer closed and sRaw is drained */
	sxu8 bChunked;        /* the body is chunk-framed */
	sxu8 bChunkDone;      /* the zero-length chunk has been read */
	sxi64 iChunkLeft;     /* bytes still owed by the chunk being read (-1: read a size line) */
	SyBlob sHdrs;         /* the response header LINES, '\n'-separated */
};
/* php reads a socket in blocks; 8K is the size every other reader here uses. */
#define HTTP_CHUNK_READ 8192
/* php's own redirect default, which the `max_redirects` option overrides. */
#define HTTP_MAX_REDIRECTS 20
/* php reads the status line into `char tmp_line[128]`, which its line reader
 * fills to at most 126 bytes; everything past that is discarded with the rest
 * of the line. Header lines are read into an allocated buffer and uncapped. */
#define HTTP_STATUS_LINE_MAX 126
/* ------------------------------------------------------------------------- */
/* Reading the connection                                                      */
/* ------------------------------------------------------------------------- */
/*
 * Drop what has already been consumed from a buffer, so a long body does not
 * grow one allocation per read.
 */
static void HttpCompact(SyBlob *pBuf,sxu32 *pOfft)
{
	if( *pOfft == 0 ){
		return;
	}
	if( *pOfft >= SyBlobLength(pBuf) ){
		SyBlobReset(pBuf);
	}else{
		sxu32 nLeft = SyBlobLength(pBuf) - *pOfft;
		SyMemcpy(&((char *)SyBlobData(pBuf))[*pOfft],SyBlobData(pBuf),nLeft);
		pBuf->nByte = nLeft;
	}
	*pOfft = 0;
}
/*
 * One recv() worth of bytes into sRaw. Answers 1 when something arrived, 0 at
 * the end of the connection, -1 for a read error.
 */
static int HttpFill(http_private *pH)
{
	char zBuf[HTTP_CHUNK_READ];
	int n;
	if( pH->bEof || pH->sock == PH7_NET_INVALID_SOCKET ){
		return 0;
	}
	n = PH7_NetRecv(pH->sock,zBuf,(int)sizeof(zBuf),0);
	if( n == 0 ){
		pH->bEof = 1;
		return 0;
	}
	if( n < 0 ){
		/* A timed-out or interrupted read ends the body here: php's wrapper has
		 * no retry either, and the bytes already in hand are the answer. */
		pH->bEof = 1;
		return -1;
	}
	HttpCompact(&pH->sRaw,&pH->nRawOfft);
	if( SyBlobAppend(&pH->sRaw,zBuf,(sxu32)n) != SXRET_OK ){
		return -1;
	}
	return 1;
}
/* How many unconsumed bytes sRaw is holding. */
static sxu32 HttpRawLeft(http_private *pH)
{
	return SyBlobLength(&pH->sRaw) > pH->nRawOfft
		? SyBlobLength(&pH->sRaw) - pH->nRawOfft : 0;
}
/*
 * One line of the header block, with its terminator removed. php accepts both
 * CRLF and a bare LF, and a header block that simply ENDS (the peer closed
 * before the blank line) is not an error -- it is a response with no body.
 *
 * Answers 1 for a line, 0 at the end of the connection.
 */
static int HttpReadLine(http_private *pH,SyBlob *pLine,int *pnTerm)
{
	SyBlobReset(pLine);
	if( pnTerm ){
		*pnTerm = 0;
	}
	for(;;){
		const char *zBase = (const char *)SyBlobData(&pH->sRaw);
		sxu32 n = HttpRawLeft(pH),i;
		for( i = 0 ; i < n ; ++i ){
			if( zBase[pH->nRawOfft + i] == '\n' ){
				sxu32 nCopy = i;
				if( nCopy > 0 && zBase[pH->nRawOfft + nCopy - 1] == '\r' ){
					nCopy--;
				}
				if( nCopy > 0 ){
					SyBlobAppend(pLine,&zBase[pH->nRawOfft],nCopy);
				}
				if( pnTerm ){
					/* The LF plus the CR before it, if there was one. */
					*pnTerm = (int)(i - nCopy) + 1;
				}
				pH->nRawOfft += i + 1;
				return 1;
			}
		}
		if( HttpFill(pH) < 1 ){
			/* No terminator will ever arrive. Whatever is buffered is the last
			 * line, which is what makes php report a bodiless reply's first
			 * bytes as its `status line` -- and report them with NO trailing
			 * newline, since the line it read carried none. */
			if( n > 0 ){
				SyBlobAppend(pLine,&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],n);
				pH->nRawOfft += n;
				return 1;
			}
			return 0;
		}
	}
}
/*
 * Pump the chunk decoder until sOut can serve nWant bytes or the body ends.
 * php applies its `dechunk` filter for this, so a chunk EXTENSION (`3;ext=1`)
 * and the trailer headers after the final chunk are consumed and never seen.
 */
static void HttpPumpChunks(http_private *pH,sxu32 nWant)
{
	SyBlob sLine;
	SyBlobInit(&sLine,&pH->pVm->sAllocator);
	while( !pH->bChunkDone && SyBlobLength(&pH->sOut) - pH->nOutOfft < nWant ){
		if( pH->iChunkLeft < 0 ){
			/* UNSIGNED on purpose: a size line is whatever bytes arrived, and a
			 * hundred hex digits of it must wrap rather than overflow a signed
			 * accumulator. Anything past INT32_MAX is capped -- the connection
			 * ends long before, and "read to the end" is what both engines then
			 * do. */
			sxu64 iSize = 0;
			const char *zLine;
			sxu32 nLine,i;
			if( HttpReadLine(pH,&sLine,0) == 0 ){
				pH->bChunkDone = 1;
				break;
			}
			zLine = (const char *)SyBlobData(&sLine);
			nLine = SyBlobLength(&sLine);
			if( nLine == 0 ){
				/* The CRLF that closes the previous chunk's data. */
				continue;
			}
			for( i = 0 ; i < nLine && SyisHex(zLine[i]) ; ++i ){
				if( iSize > 0x7FFFFFFF ){
					continue; /* already capped; keep consuming the digits */
				}
				iSize = iSize * 16 + (sxu64)SyHexToint(zLine[i]);
			}
			if( iSize > 0x7FFFFFFF ){
				iSize = 0x7FFFFFFF;
			}
			if( i == 0 ){
				/* Not a size line at all: the framing is broken and php's
				 * filter stops there rather than guessing. */
				pH->bChunkDone = 1;
				break;
			}
			if( iSize == 0 ){
				/* The final chunk. Its trailer headers run to the blank line
				 * and belong to nobody. */
				while( HttpReadLine(pH,&sLine,0) == 1 && SyBlobLength(&sLine) > 0 ){
					;
				}
				pH->bChunkDone = 1;
				break;
			}
			pH->iChunkLeft = (sxi64)iSize;
		}
		if( HttpRawLeft(pH) == 0 && HttpFill(pH) < 1 ){
			pH->bChunkDone = 1;
			break;
		}
		{
			sxu32 nHave = HttpRawLeft(pH);
			sxu32 nTake = (sxu32)(pH->iChunkLeft < (sxi64)nHave ? pH->iChunkLeft : nHave);
			HttpCompact(&pH->sOut,&pH->nOutOfft);
			SyBlobAppend(&pH->sOut,
				&((const char *)SyBlobData(&pH->sRaw))[pH->nRawOfft],nTake);
			pH->nRawOfft += nTake;
			pH->iChunkLeft -= nTake;
			if( pH->iChunkLeft == 0 ){
				pH->iChunkLeft = -1;
			}
		}
	}
	SyBlobRelease(&sLine);
}
/* ------------------------------------------------------------------------- */
/* The request                                                                 */
/* ------------------------------------------------------------------------- */
/* php's own trim set, which is what the header block's two ends are cut with. */
static int HttpIsTrimByte(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0 || c == 0x0B;
}
/*
 * Does the script's own header block already carry this header? php compares
 * case-insensitively at the START of a line, which is what lets a caller take
 * over php's Host, Connection, User-Agent, Content-Length, Content-Type and
 * Authorization slots.
 */
static int HttpUserHas(SyBlob *pHdrs,const char *zName)
{
	const char *zIn = (const char *)SyBlobData(pHdrs);
	sxu32 nIn = SyBlobLength(pHdrs),nName = (sxu32)SyStrlen(zName);
	sxu32 i = 0;
	while( i < nIn ){
		sxu32 nStart = i;
		while( i < nIn && zIn[i] != '\n' ){
			i++;
		}
		if( i - nStart >= nName && SyStrnicmp(&zIn[nStart],zName,nName) == 0 ){
			return 1;
		}
		i++;
	}
	return 0;
}
/*
 * The `header` context option, in the two shapes php reads: one string (whose
 * own line breaks separate the headers) or an ARRAY of them, whose STRING
 * entries are joined with CRLF and whose other entries are dropped. Anything
 * that is neither -- an int, a float, an object -- is ignored in silence.
 *
 * php then trims the WHOLE block at both ends and emits what is left verbatim.
 * It does not touch the inside, which is worth being exact about: a blank line
 * in the middle ENDS the request's header block, and everything the script put
 * after it becomes the request BODY.
 */
static void HttpCollectUserHeaders(ph7_vm *pVm,ph7_value *pOpt,SyBlob *pHdrs)
{
	SyBlob sJoin;
	const char *zIn;
	sxu32 nIn,nStart;
	if( pOpt == 0 ){
		return;
	}
	SyBlobInit(&sJoin,&pVm->sAllocator);
	if( pOpt->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pOpt->x.pOther;
		ph7_hashmap_node *pEntry;
		pMap->pCur = pMap->pFirst;
		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){
			ph7_value *pVal = HashmapExtractNodeValue(pEntry);
			if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){
				SyBlobAppend(&sJoin,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
				SyBlobAppend(&sJoin,"\r\n",sizeof("\r\n")-1);
			}
		}
	}else if( pOpt->iFlags & MEMOBJ_STRING ){
		SyBlobAppend(&sJoin,SyBlobData(&pOpt->sBlob),SyBlobLength(&pOpt->sBlob));
	}
	zIn = (const char *)SyBlobData(&sJoin);
	nIn = SyBlobLength(&sJoin);
	nStart = 0;
	while( nStart < nIn && HttpIsTrimByte(zIn[nStart]) ){
		nStart++;
	}
	while( nIn > nStart && HttpIsTrimByte(zIn[nIn-1]) ){
		nIn--;
	}
	if( nIn > nStart ){
		SyBlobAppend(pHdrs,&zIn[nStart],nIn - nStart);
		SyBlobAppend(pHdrs,"\r\n",sizeof("\r\n")-1);
	}
	SyBlobRelease(&sJoin);
}
/* An `http` context option by name, or 0. */
static ph7_value * HttpOpt(phl_stream_ctx *pCtx,const char *zName)
{
	return pCtx ? PH7_StreamCtxOption(pCtx,"http",zName) : 0;
}
/*
 * The three readers below all work on a COPY. Every ph7_value_to_*() converts
 * its argument in place, and these arguments are the script's own context
 * entries -- reading `protocol_version` as a number must not leave a FLOAT
 * where the script put a string, which stream_context_get_options() would then
 * answer.
 */
static sxi64 HttpOptInt(ph7_vm *pVm,ph7_value *pVal,sxi64 iDefault)
{
	ph7_value sCopy;
	sxi64 iOut;
	if( pVal == 0 ){
		return iDefault;
	}
	PH7_MemObjInit(pVm,&sCopy);
	PH7_MemObjLoad(pVal,&sCopy);
	iOut = ph7_value_to_int64(&sCopy);
	PH7_MemObjRelease(&sCopy);
	return iOut;
}
static int HttpOptBool(ph7_vm *pVm,ph7_value *pVal,int bDefault)
{
	ph7_value sCopy;
	int bOut;
	if( pVal == 0 ){
		return bDefault;
	}
	PH7_MemObjInit(pVm,&sCopy);
	PH7_MemObjLoad(pVal,&sCopy);
	bOut = ph7_value_to_bool(&sCopy) != 0;
	PH7_MemObjRelease(&sCopy);
	return bOut;
}
static double HttpOptReal(ph7_vm *pVm,ph7_value *pVal,double rDefault)
{
	ph7_value sCopy;
	double rOut;
	if( pVal == 0 ){
		return rDefault;
	}
	PH7_MemObjInit(pVm,&sCopy);
	PH7_MemObjLoad(pVal,&sCopy);
	rOut = ph7_value_to_double(&sCopy);
	PH7_MemObjRelease(&sCopy);
	return rOut;
}
/* An ini directive's text, appended to pOut; answers 1 when the directive
 * carries a VALUE at all (php's unset directive is a third state, and `from`
 * is emitted for an EMPTY value but not for an absent one). */
static int HttpIniStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)
{
	if( PH7_VmIniIsUnset(pVm,zName) ){
		return 0;
	}
	SyBlobReset(pOut);
	PH7_VmIniGetStr(pVm,zName,pOut);
	return 1;
}
/* base64 of "user:pass" for the Authorization header php builds out of a URL's
 * userinfo. */
static sxi32 HttpB64Consumer(const void *pData,unsigned int nLen,void *pUserData)
{
	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);
}
/*
 * Compose the request php would put on the wire.
 *
 * The slot order is derived, not invented: request line, the URL's
 * Authorization, the `from` ini, Host, Connection, User-Agent, the automatic
 * Content-Length, the script's own headers, and last the automatic
 * Content-Type. A body follows the blank line.
 */
static void HttpBuildRequest(ph7_vm *pVm,phl_stream_ctx *pCtx,SyhttpUri *pUri,
	const char *zTarget,int nTarget,SyBlob *pOut,const char *zMethod,int nMethod,
	const char *zBody,int nBody)
{
	SyBlob sUser,sTmp;
	ph7_value *pOptV;
	SyBlobInit(&sUser,&pVm->sAllocator);
	SyBlobInit(&sTmp,&pVm->sAllocator);
	HttpCollectUserHeaders(pVm,HttpOpt(pCtx,"header"),&sUser);
	/* Request line. */
	SyBlobAppend(pOut,zMethod,(sxu32)nMethod);
	SyBlobAppend(pOut," ",1);
	SyBlobAppend(pOut,zTarget,(sxu32)nTarget);
	SyBlobAppend(pOut," HTTP/",sizeof(" HTTP/")-1);
	/* php reads the version as a DOUBLE and prints it with ONE decimal, so a
	 * string is converted rather than passed through: `'2.0'` is 2.0, an ARRAY
	 * is 1.0, and `null` is 0.0 -- each of which reaches the wire. */
	SyBlobFormat(pOut,"%.1f",HttpOptReal(pVm,HttpOpt(pCtx,"protocol_version"),1.1));
	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
	/* The URL's own credentials, which a script-supplied Authorization wins
	 * over. php base64s `user:pass` verbatim -- it decodes neither half. */
	if( SyStringLength(&pUri->sUser) > 0 && !HttpUserHas(&sUser,"authorization:") ){
		SyBlobReset(&sTmp);
		SyBlobAppend(&sTmp,pUri->sUser.zString,pUri->sUser.nByte);
		SyBlobAppend(&sTmp,":",1);
		if( SyStringLength(&pUri->sPass) > 0 ){
			SyBlobAppend(&sTmp,pUri->sPass.zString,pUri->sPass.nByte);
		}
		SyBlobAppend(pOut,"Authorization: Basic ",sizeof("Authorization: Basic ")-1);
		SyBase64Encode((const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp),
			HttpB64Consumer,pOut);
		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
	}
	/* php's `from` ini is a From: header whenever the directive has a value at
	 * all -- an EMPTY one still writes the header, which is not how the
	 * user_agent directive below behaves. */
	if( HttpIniStr(pVm,"from",&sTmp) ){
		SyBlobAppend(pOut,"From: ",sizeof("From: ")-1);
		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));
		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
	}
	if( !HttpUserHas(&sUser,"host:") ){
		SyBlobAppend(pOut,"Host: ",sizeof("Host: ")-1);
		SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);
		if( SyStringLength(&pUri->sPort) > 0
		 && !(pUri->sPort.nByte == 2 && SyStrncmp(pUri->sPort.zString,"80",2) == 0) ){
			SyBlobAppend(pOut,":",1);
			SyBlobAppend(pOut,pUri->sPort.zString,pUri->sPort.nByte);
		}
		SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
	}
	if( !HttpUserHas(&sUser,"connection:") ){
		SyBlobAppend(pOut,"Connection: close\r\n",sizeof("Connection: close\r\n")-1);
	}
	if( !HttpUserHas(&sUser,"user-agent:") ){
		pOptV = HttpOpt(pCtx,"user_agent");
		SyBlobReset(&sTmp);
		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){
			SyBlobAppend(&sTmp,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));
		}else{
			HttpIniStr(pVm,"user_agent",&sTmp);
		}
		if( SyBlobLength(&sTmp) > 0 ){
			SyBlobAppend(pOut,"User-Agent: ",sizeof("User-Agent: ")-1);
			SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));
			SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
		}
	}
	if( nBody > 0 && !HttpUserHas(&sUser,"content-length:") ){
		SyBlobFormat(pOut,"Content-Length: %d\r\n",nBody);
	}
	SyBlobAppend(pOut,SyBlobData(&sUser),SyBlobLength(&sUser));
	if( nBody > 0 && !HttpUserHas(&sUser,"content-type:") ){
		SyString sCaller;
		SyBlobAppend(pOut,"Content-Type: application/x-www-form-urlencoded\r\n",
			sizeof("Content-Type: application/x-www-form-urlencoded\r\n")-1);
		/* php SAYS so, once per request it composes: an E_NOTICE under the name
		 * of whatever function is doing the opening. */
		SyStringInitFromBuf(&sCaller,pVm->zOpenCaller ? pVm->zOpenCaller : "",
			pVm->zOpenCaller ? SyStrlen(pVm->zOpenCaller) : 0);
		PH7_VmThrowError(pVm,pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_NOTICE,
			"Content-type not specified assuming application/x-www-form-urlencoded");
	}
	SyBlobAppend(pOut,"\r\n",sizeof("\r\n")-1);
	if( nBody > 0 ){
		SyBlobAppend(pOut,zBody,(sxu32)nBody);
	}
	SyBlobRelease(&sUser);
	SyBlobRelease(&sTmp);
}
/* ------------------------------------------------------------------------- */
/* The exchange                                                                */
/* ------------------------------------------------------------------------- */
/*
 * php's failure text for an open in flight. The reason has to outlive this
 * call, so it is composed into the VM's own buffer.
 */
static void HttpFail(ph7_vm *pVm,const char *zText,int nText)
{
	sxu32 nCopy = (sxu32)(nText < 0 ? (int)SyStrlen(zText) : nText);
	if( nCopy >= sizeof(pVm->zOpenErrBuf) ){
		nCopy = sizeof(pVm->zOpenErrBuf) - 1;
	}
	SyMemcpy(zText,pVm->zOpenErrBuf,nCopy);
	pVm->zOpenErrBuf[nCopy] = 0;
	PH7_StreamSetOpenError(pVm,pVm->zOpenErrBuf);
}
/*
 * php's port rule: whatever the URL spells, else 80.
 */
static int HttpUriPort(SyhttpUri *pUri)
{
	sxi32 iPort = 0;
	if( SyStringLength(&pUri->sPort) < 1 ){
		return 80;
	}
	SyStrToInt32(pUri->sPort.zString,pUri->sPort.nByte,(void *)&iPort,0);
	return iPort > 0 ? (int)iPort : 80;
}
/*
 * The request TARGET: the URL's path and query, `/` when it has neither, and
 * the whole URL when `request_fulluri` says so (which is what a proxy that
 * insists on absolute-form gets).
 */
static void HttpRequestTarget(SyhttpUri *pUri,const char *zUrl,int nUrl,
	int bFullUri,SyBlob *pOut)
{
	SyBlobReset(pOut);
	if( bFullUri ){
		SyBlobAppend(pOut,zUrl,(sxu32)nUrl);
		return;
	}
	if( SyStringLength(&pUri->sPath) < 1 ){
		SyBlobAppend(pOut,"/",1);
	}else{
		SyBlobAppend(pOut,pUri->sPath.zString,pUri->sPath.nByte);
	}
	if( SyStringLength(&pUri->sQuery) > 0 ){
		SyBlobAppend(pOut,"?",1);
		SyBlobAppend(pOut,pUri->sQuery.zString,pUri->sQuery.nByte);
	}
}
/*
 * Where a `Location:` points, as an absolute `http://…` URL.
 *
 * php recognizes exactly two shapes: one with a scheme (taken whole, host and
 * all) and one starting with `/` (the path, on the same host). Everything else
 * is joined to the CURRENT path, up to and including its last `/`, with another
 * `/` between -- so `/a/b` + `rel` is `/a//rel`, which php does not normalize.
 * A path that is just `/` joins directly, and so does an empty one. A Location
 * of at most one byte is never joined: php puts it straight under the root, so
 * `x` is `/x` and an empty one is `/` (php 8.5.11 settled that last case, which
 * earlier builds answered from a read past the end of the header).
 */
static void HttpResolveLocation(SyhttpUri *pUri,const char *zLoc,sxu32 nLoc,SyBlob *pOut)
{
	sxu32 i;
	SyBlobReset(pOut);
	for( i = 0 ; i + 2 < nLoc ; ++i ){
		if( zLoc[i] == ':' && zLoc[i+1] == '/' && zLoc[i+2] == '/' ){
			/* An absolute URL. php takes it as written, whatever the scheme --
			 * an unsupported one then fails at the device lookup. */
			SyBlobAppend(pOut,zLoc,nLoc);
			return;
		}
		if( zLoc[i] == '/' || zLoc[i] == '?' || zLoc[i] == '#' ){
			break;
		}
	}
	SyBlobAppend(pOut,"http://",sizeof("http://")-1);
	if( SyStringLength(&pUri->sUser) > 0 ){
		SyBlobAppend(pOut,pUri->sUser.zString,pUri->sUser.nByte);
		if( SyStringLength(&pUri->sPass) > 0 ){
			SyBlobAppend(pOut,":",1);
			SyBlobAppend(pOut,pUri->sPass.zString,pUri->sPass.nByte);
		}
		SyBlobAppend(pOut,"@",1);
	}
	SyBlobAppend(pOut,pUri->sHost.zString,pUri->sHost.nByte);
	if( SyStringLength(&pUri->sPort) > 0 ){
		SyBlobAppend(pOut,":",1);
		SyBlobAppend(pOut,pUri->sPort.zString,pUri->sPort.nByte);
	}
	if( nLoc > 0 && zLoc[0] == '/' ){
		SyBlobAppend(pOut,zLoc,nLoc);
		return;
	}
	if( nLoc < 2 ){
		SyBlobAppend(pOut,"/",1);
		SyBlobAppend(pOut,zLoc,nLoc);
		return;
	}
	{
		const char *zPath = pUri->sPath.zString;
		sxu32 nPath = pUri->sPath.nByte,nKeep = 0;
		for( i = 0 ; i < nPath ; ++i ){
			if( zPath[i] == '/' ){
				nKeep = i + 1;
			}
		}
		if( nKeep < 2 ){
			/* The path is `/` or has no directory part at all: php joins with
			 * a single separator. */
			SyBlobAppend(pOut,"/",1);
		}else{
			SyBlobAppend(pOut,zPath,nKeep);
			SyBlobAppend(pOut,"/",1);
		}
		SyBlobAppend(pOut,zLoc,nLoc);
	}
}
/*
 * Read one whole response: its status line, its header block, and whatever of
 * the body arrived with them.
 *
 * Answers the status code php would compute -- `atoi(line + 9)`, which is 0 for
 * anything shorter than ten bytes and for a line whose digits sit elsewhere --
 * and fills *pzStatus with the raw status line, which is what php's failure
 * names. A 1xx is swallowed here and the next response read in its place, along
 * with the headers it had already recorded.
 */
static int HttpReadResponse(http_private *pH,SyBlob *pHdrs,SyBlob *pStatus,
	SyBlob *pLocation,int *pbHasLocation,int *pbAnyLine)
{
	SyBlob sLine,sNext;
	int iCode = 0,nTerm = 0,nNextTerm = 0,bStale;
	SyBlobInit(&sLine,&pH->pVm->sAllocator);
	SyBlobInit(&sNext,&pH->pVm->sAllocator);
	*pbHasLocation = 0;
	for(;;){
		sxu32 nHdrStart = SyBlobLength(pHdrs);
		bStale = 0;
		/* php reads every line into ONE buffer and does not clear it when the
		 * read fails, so a connection that ends where a status line was due
		 * reports the LAST line it did read -- which after an informational
		 * response is that response's own blank line. */
		if( HttpReadLine(pH,&sNext,&nNextTerm) ){
			SyBlobReset(&sLine);
			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));
			nTerm = nNextTerm;
			if( SyBlobLength(&sLine) > HTTP_STATUS_LINE_MAX ){
				/* php reads the STATUS line into a 128-byte stack buffer and
				 * throws the rest of that line away -- a header line, read into
				 * a buffer it allocates, has no such cap. A line that hit the
				 * cap carried no terminator either. */
				sLine.nByte = HTTP_STATUS_LINE_MAX;
				nTerm = 0;
			}
			*pbAnyLine = 1;
		}else{
			bStale = 1;
			if( !*pbAnyLine ){
				/* Nothing was ever read: there is no line to report and php's
				 * header array stays EMPTY rather than gaining a blank entry. */
				break;
			}
		}
		/* php's refusal prints the status line AS READ, terminator and all --
		 * so a reply that simply ran out of bytes is named with no newline
		 * after it, and an ordinary one keeps the CRLF it arrived with. */
		SyBlobReset(pStatus);
		SyBlobAppend(pStatus,SyBlobData(&sLine),SyBlobLength(&sLine));
		if( nTerm == 2 ){
			SyBlobAppend(pStatus,"\r\n",sizeof("\r\n")-1);
		}else if( nTerm == 1 ){
			SyBlobAppend(pStatus,"\n",1);
		}
		iCode = 0;
		if( SyBlobLength(&sLine) > 9 ){
			sxi32 iTmp = 0;
			SyStrToInt32(&((const char *)SyBlobData(&sLine))[9],
				SyBlobLength(&sLine) - 9,(void *)&iTmp,0);
			iCode = (int)iTmp;
		}
		SyBlobAppend(pHdrs,SyBlobData(&sLine),SyBlobLength(&sLine));
		SyBlobAppend(pHdrs,"\n",1);
		if( bStale ){
			break;
		}
		/* The header block. A continuation line (one opening with a space or a
		 * tab) belongs to the header before it, joined by a single space. */
		for(;;){
			const char *zLine;
			sxu32 nLine,nColon;
			if( HttpReadLine(pH,&sNext,&nNextTerm) == 0 ){
				break;
			}
			SyBlobReset(&sLine);
			SyBlobAppend(&sLine,SyBlobData(&sNext),SyBlobLength(&sNext));
			nTerm = nNextTerm;
			zLine = (const char *)SyBlobData(&sLine);
			nLine = SyBlobLength(&sLine);
			/* php right-trims a HEADER line -- and only a header line: the
			 * status line above keeps whatever trailing blanks it arrived
			 * with. Done after the length is taken, so a line of nothing but
			 * blanks is still the CONTINUATION its first byte makes it. */
			while( nLine > 0 && (zLine[nLine-1] == ' ' || zLine[nLine-1] == '\t')
			    && !(zLine[0] == ' ' || zLine[0] == '\t') ){
				nLine--;
			}
			if( nLine < 1 ){
				break; /* the blank line: the body starts here */
			}
			if( zLine[0] == ' ' || zLine[0] == '\t' ){
				sxu32 i = 0;
				while( i < nLine && (zLine[i] == ' ' || zLine[i] == '\t') ){
					i++;
				}
				while( nLine > i && (zLine[nLine-1] == ' ' || zLine[nLine-1] == '\t') ){
					nLine--;
				}
				/* Back over the separator the previous line closed with: the
				 * two are ONE header, joined by a single space whatever the
				 * continuation was indented with. */
				if( pHdrs->nByte > 0 ){
					pHdrs->nByte--;
				}
				SyBlobAppend(pHdrs," ",1);
				SyBlobAppend(pHdrs,&zLine[i],nLine - i);
				SyBlobAppend(pHdrs,"\n",1);
				continue;
			}
			for( nColon = 0 ; nColon < nLine && zLine[nColon] != ':' ; ++nColon ){
				;
			}
			if( nColon >= nLine ){
				/* php refuses the whole response for a header line with no
				 * colon in it, and says so in exactly these words. */
				HttpFail(pH->pVm,
					"HTTP invalid response format (no colon in header line)!",-1);
				SyBlobRelease(&sLine);
				SyBlobRelease(&sNext);
				return -1;
			}
			if( nColon == sizeof("Transfer-Encoding")-1
			 && !pH->pVm->bHttpGetHeaders
			 && SyStrnicmp(zLine,"Transfer-Encoding",sizeof("Transfer-Encoding")-1) == 0 ){
				sxu32 nOfft;
				if( SyBlobSearch(&zLine[nColon],nLine - nColon,"chunked",
						sizeof("chunked")-1,&nOfft) == SXRET_OK ){
					/* php frames the body with its dechunk filter and drops
					 * the header that asked for it: a script reading the
					 * response headers never sees this line. */
					pH->bChunked = 1;
					pH->iChunkLeft = -1;
					continue;
				}
			}
			if( nColon == sizeof("Location")-1
			 && SyStrnicmp(zLine,"Location",sizeof("Location")-1) == 0 ){
				/* php trims the value at BOTH ends before resolving it. */
				sxu32 i = nColon + 1,nStop = nLine;
				while( i < nStop && HttpIsTrimByte(zLine[i]) ){
					i++;
				}
				while( nStop > i && HttpIsTrimByte(zLine[nStop-1]) ){
					nStop--;
				}
				SyBlobReset(pLocation);
				SyBlobAppend(pLocation,&zLine[i],nStop - i);
				*pbHasLocation = 1;
			}
			SyBlobAppend(pHdrs,zLine,nLine);
			SyBlobAppend(pHdrs,"\n",1);
		}
		if( iCode >= 100 && iCode < 200 && iCode != 101 ){
			/* An informational response is not the answer: php drops it, drops
			 * the headers it collected for it, and reads the next one. 101 is
			 * php's one exception -- a protocol SWITCH is the last thing that
			 * will ever be spoken as HTTP on this connection, so there is no
			 * next response to read and php reports the 101 itself. */
			pHdrs->nByte = nHdrStart;
			pH->bChunked = 0;
			*pbHasLocation = 0;
			continue;
		}
		break;
	}
	SyBlobRelease(&sLine);
	SyBlobRelease(&sNext);
	return iCode;
}
/* Release everything the handle owns; the handle itself goes with it. */
static void HttpFree(http_private *pH)
{
	if( pH == 0 ){
		return;
	}
	if( pH->sock != PH7_NET_INVALID_SOCKET ){
		PH7_NetClose(pH->sock);
		pH->sock = PH7_NET_INVALID_SOCKET;
	}
	SyBlobRelease(&pH->sRaw);
	SyBlobRelease(&pH->sOut);
	SyBlobRelease(&pH->sHdrs);
	SyMemBackendFree(&pH->pVm->sAllocator,pH);
}
/* The connection this exchange runs over, or php's own failure text. */
static int HttpConnect(http_private *pH,SyhttpUri *pUri,phl_stream_ctx *pCtx)
{
	SyBlob sHost;
	const char *zHost;
	const char *zErr = "";
	int iErrno = 0,iTimeoutMs,rc = -1;
	ph7_value *pOptV;
	ph7_socket sock;
	/* The resolver wants a NUL-terminated name, and a host is whatever the URL
	 * spelled -- no fixed buffer, because the failure below NAMES it. */
	SyBlobInit(&sHost,&pH->pVm->sAllocator);
	SyBlobAppend(&sHost,pUri->sHost.zString,pUri->sHost.nByte);
	SyBlobNullAppend(&sHost);
	zHost = (const char *)SyBlobData(&sHost);
	/* php bounds the exchange by the `timeout` option, and by
	 * default_socket_timeout when the script named none. */
	iTimeoutMs = (int)(PH7_VmIniGetInt(pH->pVm,"default_socket_timeout",60) * 1000);
	pOptV = HttpOpt(pCtx,"timeout");
	if( pOptV ){
		double rSec = HttpOptReal(pH->pVm,pOptV,0);
		iTimeoutMs = rSec > 0 ? (int)(rSec * 1000) : 0;
	}
	sock = PH7_NetConnect(zHost,HttpUriPort(pUri),iTimeoutMs,0,&iErrno,&zErr);
	if( sock == PH7_NET_INVALID_SOCKET ){
		if( iErrno == PH7_NET_ERR_RESOLVE ){
			SyBlob sMsg;
			SyString sCaller;
			SyBlobInit(&sMsg,&pH->pVm->sAllocator);
			SyBlobFormat(&sMsg,
				"php_network_getaddresses: getaddrinfo for %z failed: Name or service not known",
				&pUri->sHost);
			SyBlobNullAppend(&sMsg);
			HttpFail(pH->pVm,(const char *)SyBlobData(&sMsg),(int)SyBlobLength(&sMsg)-1);
			/* php says this one TWICE: the resolver's own failure under the
			 * calling function's name, and then the caller's failed-open
			 * sentence carrying it as the reason. A refused CONNECT is only the
			 * second -- the resolver is where php has the extra warning. */
			SyStringInitFromBuf(&sCaller,pH->pVm->zOpenCaller ? pH->pVm->zOpenCaller : "",
				pH->pVm->zOpenCaller ? SyStrlen(pH->pVm->zOpenCaller) : 0);
			PH7_VmThrowError(pH->pVm,pH->pVm->zOpenCaller ? &sCaller : 0,PH7_CTX_WARNING,
				(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
		}else{
			HttpFail(pH->pVm,zErr && zErr[0] ? zErr : "Connection refused",-1);
		}
	}else{
		pH->sock = sock;
		pH->bEof = 0;
		rc = PH7_OK;
	}
	SyBlobRelease(&sHost);
	return rc;
}
/*
 * Walk the exchange: connect, ask, read, and follow whatever redirects php
 * would follow. Answers PH7_OK with the handle sitting on the body, or -1 with
 * the failure already named.
 */
static int HttpRun(http_private *pH,const char *zUrl,int nUrl,phl_stream_ctx *pCtx)
{
	SyBlob sUrl,sReq,sStatus,sLoc,sTarget,sMethod;
	ph7_value *pOptV;
	int iRedirLeft = HTTP_MAX_REDIRECTS,bFollow = 1,bIgnoreErr = 0,rc = -1;
	int bAnyLine = 0,bAnswered = 0;
	const char *zBody = 0;
	int nBody = 0;
	ph7_vm *pVm = pH->pVm;
	SyBlobInit(&sUrl,&pVm->sAllocator);
	SyBlobInit(&sReq,&pVm->sAllocator);
	SyBlobInit(&sStatus,&pVm->sAllocator);
	SyBlobInit(&sLoc,&pVm->sAllocator);
	SyBlobInit(&sTarget,&pVm->sAllocator);
	SyBlobInit(&sMethod,&pVm->sAllocator);
	SyBlobAppend(&sUrl,zUrl,(sxu32)nUrl);
	/* php drops the previous exchange's headers before this one starts, so an
	 * open that never reaches a response leaves http_get_last_response_headers()
	 * answering NULL rather than the set before it -- while the CALLER's own
	 * $http_response_header, which is only written when there is something to
	 * write, keeps whatever it held. */
	PH7_HttpClearResponseHeaders(pVm);
	pOptV = HttpOpt(pCtx,"max_redirects");
	if( pOptV ){
		iRedirLeft = (int)HttpOptInt(pVm,pOptV,HTTP_MAX_REDIRECTS);
	}
	pOptV = HttpOpt(pCtx,"follow_location");
	if( pOptV ){
		bFollow = HttpOptInt(pVm,pOptV,1) != 0;
	}
	pOptV = HttpOpt(pCtx,"ignore_errors");
	if( pOptV ){
		bIgnoreErr = HttpOptBool(pVm,pOptV,0);
	}
	if( pVm->bHttpGetHeaders ){
		/* get_headers() asks for the HEADERS of whatever answered, so php opens
		 * with this on whatever the caller's context said. */
		bIgnoreErr = 1;
	}
	pOptV = HttpOpt(pCtx,"method");
	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) ){
		SyBlobAppend(&sMethod,SyBlobData(&pOptV->sBlob),SyBlobLength(&pOptV->sBlob));
	}
	if( SyBlobLength(&sMethod) < 1 ){
		SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);
	}
	pOptV = HttpOpt(pCtx,"content");
	if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){
		zBody = (const char *)SyBlobData(&pOptV->sBlob);
		nBody = (int)SyBlobLength(&pOptV->sBlob);
	}
	for(;;){
		SyhttpUri sUri;
		int iCode,bHasLoc = 0,bFullUri = 0;
		if( PH7_VmHttpSplitURI(&sUri,(const char *)SyBlobData(&sUrl),
				SyBlobLength(&sUrl)) != SXRET_OK
		 || SyStringLength(&sUri.sHost) < 1 ){
			HttpFail(pVm,"Unable to parse the URL",-1);
			goto done;
		}
		pOptV = HttpOpt(pCtx,"request_fulluri");
		bFullUri = HttpOptBool(pVm,pOptV,0);
		HttpRequestTarget(&sUri,(const char *)SyBlobData(&sUrl),
			(int)SyBlobLength(&sUrl),bFullUri,&sTarget);
		/* php's `proxy` option moves the CONNECTION and leaves everything else
		 * alone: the request line and the Host header still describe the
		 * origin server. The address is a transport URL of its own. */
		pOptV = HttpOpt(pCtx,"proxy");
		if( pOptV && (pOptV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pOptV->sBlob) > 0 ){
			SyhttpUri sProxy;
			if( PH7_VmHttpSplitURI(&sProxy,(const char *)SyBlobData(&pOptV->sBlob),
					SyBlobLength(&pOptV->sBlob)) == SXRET_OK
			 && SyStringLength(&sProxy.sHost) > 0 ){
				if( HttpConnect(pH,&sProxy,pCtx) != PH7_OK ){
					goto done;
				}
			}else{
				HttpFail(pVm,"Unable to parse the proxy address",-1);
				goto done;
			}
		}else if( HttpConnect(pH,&sUri,pCtx) != PH7_OK ){
			goto done;
		}
		SyBlobReset(&sReq);
		HttpBuildRequest(pVm,pCtx,&sUri,(const char *)SyBlobData(&sTarget),
			(int)SyBlobLength(&sTarget),&sReq,
			(const char *)SyBlobData(&sMethod),(int)SyBlobLength(&sMethod),zBody,nBody);
		if( PH7_NetSendAll(pH->sock,SyBlobData(&sReq),(int)SyBlobLength(&sReq)) != PH7_OK ){
			HttpFail(pVm,"Connection refused",-1);
			goto done;
		}
		bAnswered = 1;
		iCode = HttpReadResponse(pH,&pH->sHdrs,&sStatus,&sLoc,&bHasLoc,&bAnyLine);
		if( iCode < 0 ){
			goto done; /* the malformed-header refusal named itself */
		}
		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && --iRedirLeft >= 1 ){
			SyBlob sNext;
			SyBlobInit(&sNext,&pVm->sAllocator);
			HttpResolveLocation(&sUri,(const char *)SyBlobData(&sLoc),
				SyBlobLength(&sLoc),&sNext);
			SyBlobReset(&sUrl);
			SyBlobAppend(&sUrl,SyBlobData(&sNext),SyBlobLength(&sNext));
			SyBlobRelease(&sNext);
			/* php keeps the method and the body only for the two codes that
			 * promise them; every older redirect becomes a bodiless GET. */
			if( iCode != 307 && iCode != 308 ){
				SyBlobReset(&sMethod);
				SyBlobAppend(&sMethod,"GET",sizeof("GET")-1);
				zBody = 0;
				nBody = 0;
			}
			PH7_NetClose(pH->sock);
			pH->sock = PH7_NET_INVALID_SOCKET;
			SyBlobReset(&pH->sRaw);
			pH->nRawOfft = 0;
			SyBlobReset(&pH->sOut);
			pH->nOutOfft = 0;
			pH->bEof = 0;
			pH->bChunked = 0;
			pH->bChunkDone = 0;
			pH->iChunkLeft = -1;
			continue;
		}
		if( iCode >= 300 && iCode < 400 && bHasLoc && bFollow && !pVm->bHttpGetHeaders ){
			/* The follow above declined because the count ran out. php calls that
			 * a failed open -- except for a headers-only one (get_headers()),
			 * which stops where it is and answers the headers it collected,
			 * silently. */
			HttpFail(pVm,"Redirection limit reached, aborting",-1);
			goto done;
		}
		if( (iCode < 200 || iCode >= 400) && !bIgnoreErr ){
			/* php's own sentence, and the status LINE it carries keeps the
			 * terminator it was read with. */
			char zMsg[320];
			if( bAnyLine ){
				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed! %.*s",
					(int)SyBlobLength(&sStatus),(const char *)SyBlobData(&sStatus));
			}else{
				/* Nothing came back at all: php names no line, and not even the
				 * space that would separate one. */
				SyBufferFormat(zMsg,sizeof(zMsg),"HTTP request failed!");
			}
			HttpFail(pVm,zMsg,-1);
			goto done;
		}
		rc = PH7_OK;
		break;
	}
done:
	/* The headers belong to the SCRIPT whether the open worked or not: php
	 * fills $http_response_header for a 404 exactly as it does for a 200, and
	 * leaves it untouched for an exchange that never reached a response. */
	if( bAnswered ){
		PH7_HttpPublishHeaders(pVm,&pH->sHdrs);
	}
	SyBlobRelease(&sUrl);
	SyBlobRelease(&sReq);
	SyBlobRelease(&sStatus);
	SyBlobRelease(&sLoc);
	SyBlobRelease(&sTarget);
	SyBlobRelease(&sMethod);
	return rc;
}
/* ------------------------------------------------------------------------- */
/* The device                                                                  */
/* ------------------------------------------------------------------------- */
static int HttpStream_Open(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)
{
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	http_private *pH;
	SyBlob sUrl;
	int rc;
	if( pVm == 0 ){
		return -1;
	}
	if( (iMode & (PH7_IO_OPEN_RDWR|PH7_IO_OPEN_APPEND|PH7_IO_OPEN_TRUNC
			|PH7_IO_OPEN_EXCL)) != 0 ){
		/* php's screen here is TEXTUAL -- `strpbrk(mode, "awx+")` -- and these
		 * four flags are what those four characters parse to, which is why
		 * `c` (create-if-absent, and nothing else) is the one write-ish mode
		 * php lets through: it connects and hands back a readable stream like
		 * any other. Said before a socket is made. */
		PH7_StreamSetOpenError(pVm,"HTTP wrapper does not support writeable connections");
		return -1;
	}
	pH = (http_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(http_private));
	if( pH == 0 ){
		return -1;
	}
	SyZero(pH,sizeof(http_private));
	pH->pVm = pVm;
	pH->sock = PH7_NET_INVALID_SOCKET;
	pH->iChunkLeft = -1;
	SyBlobInit(&pH->sRaw,&pVm->sAllocator);
	SyBlobInit(&pH->sOut,&pVm->sAllocator);
	SyBlobInit(&pH->sHdrs,&pVm->sAllocator);
	/* PH7_VmGetStreamDevice() hands the wrapper what is left after the scheme;
	 * every redirect below is resolved against a WHOLE url, so it goes back on. */
	SyBlobInit(&sUrl,&pVm->sAllocator);
	SyBlobAppend(&sUrl,"http://",sizeof("http://")-1);
	SyBlobAppend(&sUrl,zName,(sxu32)SyStrlen(zName));
	rc = HttpRun(pH,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl),
		(phl_stream_ctx *)pVm->pOpenCtx);
	SyBlobRelease(&sUrl);
	if( rc != PH7_OK ){
		HttpFree(pH);
		return -1;
	}
	*ppHandle = (void *)pH;
	return PH7_OK;
}
static void HttpStream_Close(void *pHandle)
{
	HttpFree((http_private *)pHandle);
}
static ph7_int64 HttpStream_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)
{
	http_private *pH = (http_private *)pHandle;
	SyBlob *pSrc;
	sxu32 *pOfft,nHave,nTake;
	if( pH == 0 || nRead < 1 ){
		return -1;
	}
	if( pH->bChunked ){
		HttpPumpChunks(pH,(sxu32)nRead);
		pSrc = &pH->sOut;
		pOfft = &pH->nOutOfft;
	}else{
		while( HttpRawLeft(pH) < (sxu32)nRead && !pH->bEof ){
			if( HttpFill(pH) < 1 ){
				break;
			}
		}
		pSrc = &pH->sRaw;
		pOfft = &pH->nRawOfft;
	}
	nHave = SyBlobLength(pSrc) > *pOfft ? SyBlobLength(pSrc) - *pOfft : 0;
	if( nHave < 1 ){
		return 0;
	}
	nTake = nHave < (sxu32)nRead ? nHave : (sxu32)nRead;
	SyMemcpy(&((const char *)SyBlobData(pSrc))[*pOfft],pBuffer,nTake);
	*pOfft += nTake;
	pH->iPos += nTake;
	return (ph7_int64)nTake;
}
/* php's http stream is not writable; a write on one is the same false every
 * read-only device answers. */
static ph7_int64 HttpStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	SXUNUSED(pHandle);
	SXUNUSED(pBuf);
	SXUNUSED(nWrite);
	return -1;
}
/* Bytes the script has taken. php's http stream is not seekable, so this is the
 * only position it has. */
static ph7_int64 HttpStream_Tell(void *pHandle)
{
	http_private *pH = (http_private *)pHandle;
	return pH ? pH->iPos : -1;
}
PH7_PRIVATE const ph7_io_stream sHTTP_Stream = {
	"http",
	PH7_IO_STREAM_VERSION,
	HttpStream_Open,  /* xOpen */
	0,                /* xOpenDir */
	HttpStream_Close, /* xClose */
	0,                /* xCloseDir */
	HttpStream_Read,  /* xRead */
	0,                /* xReadDir */
	HttpStream_Write, /* xWrite */
	0,                /* xSeek */
	0,                /* xLock */
	0,                /* xRewindDir */
	HttpStream_Tell,  /* xTell */
	0,                /* xTrunc */
	0,                /* xSync */
	0                 /* xStat */
};
/* Is this the http:// device? Asked by the metadata reader, which labels it
 * apart, and by feof(), which reads the handle's own end. */
PH7_PRIVATE int PH7_HttpStreamIs(const ph7_io_stream *pStream)
{
	return pStream == &sHTTP_Stream;
}
/*
 * php's `wrapper_data` for an http handle: the response headers of the exchange
 * THIS handle made, which is not the same thing as the VM's last set (a second
 * request has since overwritten that one).
 */
PH7_PRIVATE ph7_value * PH7_HttpStreamHeaderArray(ph7_vm *pVm,void *pHandle)
{
	http_private *pH = (http_private *)pHandle;
	ph7_value *pArr,*pLine;
	const char *zIn,*zEnd,*zCur;
	if( pH == 0 ){
		return 0;
	}
	pArr = ph7_new_array(pVm);
	pLine = ph7_new_scalar(pVm);
	if( pArr == 0 || pLine == 0 ){
		if( pArr ){ ph7_release_value(pVm,pArr); }
		if( pLine ){ ph7_release_value(pVm,pLine); }
		return 0;
	}
	zIn = (const char *)SyBlobData(&pH->sHdrs);
	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];
	while( zIn < zEnd ){
		zCur = zIn;
		while( zCur < zEnd && zCur[0] != '\n' ){
			zCur++;
		}
		ph7_value_string(pLine,zIn,(int)(zCur - zIn));
		ph7_array_add_elem(pArr,0,pLine);
		ph7_value_reset_string_cursor(pLine);
		zIn = zCur < zEnd ? &zCur[1] : zEnd;
	}
	ph7_release_value(pVm,pLine);
	return pArr;
}
/* The connection an http handle is reading, so stream_select() can wait on the
 * same descriptor php's does. */
PH7_PRIVATE ph7_socket * PH7_HttpStreamSocket(void *pHandle)
{
	http_private *pH = (http_private *)pHandle;
	return pH ? &pH->sock : 0;
}
/*
 * php's `unread_bytes`: what the wrapper has already pulled off the socket and
 * the script has not taken. php reads a block at a time too, so a small
 * response is fully buffered by the time the header block has been parsed --
 * and, exactly as in php, a big one is not.
 */
PH7_PRIVATE sxu32 PH7_HttpStreamUnread(void *pHandle)
{
	http_private *pH = (http_private *)pHandle;
	if( pH == 0 ){
		return 0;
	}
	if( pH->bChunked ){
		return SyBlobLength(&pH->sOut) > pH->nOutOfft
			? SyBlobLength(&pH->sOut) - pH->nOutOfft : 0;
	}
	return HttpRawLeft(pH);
}
/* php's feof() for an http handle: the peer has closed AND nothing is left in
 * hand. */
PH7_PRIVATE int PH7_HttpStreamAtEof(void *pHandle)
{
	http_private *pH = (http_private *)pHandle;
	if( pH == 0 ){
		return 1;
	}
	if( pH->bChunked ){
		return pH->bChunkDone && SyBlobLength(&pH->sOut) <= pH->nOutOfft;
	}
	return pH->bEof && HttpRawLeft(pH) == 0;
}
/* ------------------------------------------------------------------------- */
/* get_headers()                                                               */
/* ------------------------------------------------------------------------- */
/*
 * array|false get_headers(string $url, bool $associative = false,
 *                         ?resource $context = null)
 *
 * php's one function for "ask that URL what it answers, and nothing else". It
 * is the http:// wrapper with `ignore_errors` forced on -- a 404 is a set of
 * headers, not a failure -- opened and closed without a byte of the body read.
 *
 * Two refusals of its own: an EMPTY url is php's `Path must not be empty`
 * ValueError, before anything is looked up; and a url that does not resolve to
 * the HTTP wrapper specifically -- a path, an unknown scheme, even data://,
 * which IS a url wrapper -- is a warning and false.
 *
 * `$associative` reshapes the SAME lines: a line with no colon in it is a status
 * line and takes the next INTEGER key (which is how `abcdefghi 200 OK` gets one
 * and `HTTP/1.1 200 OK: weird` does not), everything else is keyed by the text
 * before its first colon with the value left-trimmed after it, and a name that
 * arrives twice -- across a redirect chain included -- collects into an ARRAY.
 */
static int PH7_builtin_get_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	phl_stream_ctx *pCtxRes;
	ph7_value *pArr,*pKey,*pVal,*pLine;
	http_private *pH;
	void *pHandle;
	const char *zUrl,*zIn,*zEnd,*zCur;
	int nUrl = 0,bAssoc = 0,bThrew = 0,iStatus = 0;
	/* The declared signature is the screen: a non-string $url is its TypeError
	 * and a fourth argument its ArgumentCountError, both before this runs. */
	zUrl = ph7_value_to_string(apArg[0],&nUrl);
	if( nUrl < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");
	}
	if( nArg > 1 ){
		bAssoc = ph7_value_to_bool(apArg[1]);
	}
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	{
		/* php's screen is the wrapper's URL bit, not its identity: php:// and a
		 * plain path are the refusal, and data:// -- which php DOES count as a
		 * url wrapper -- goes through and answers false further down, silently,
		 * because a stream with no response headers has nothing to give. */
		const char *zProbe = zUrl;
		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,nUrl);
		if( pStream == 0 ){
			/* php names the missing wrapper first -- and only that: the
			 * failed-open line beside it belongs to an open, and get_headers()
			 * never reaches one. */
			VfsThrowUnknownWrapperWarning(pCtx,zUrl);
		}
		if( pStream == 0 || !PH7_StreamIsUrlWrapper(pStream) ){
			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
				"This function may only be used against URLs");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		zUrl = zProbe;
	}
	/* php builds its own context over the caller's, with `ignore_errors` on: a
	 * status php would refuse to open is a set of headers here. The flag rides
	 * the VM rather than a synthesized context so the caller's own options --
	 * its method, its headers -- reach the request unchanged. */
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	pCtx->pVm->bHttpGetHeaders = 1;
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUrl,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,ph7_function_name(pCtx));
	pCtx->pVm->bHttpGetHeaders = 0;
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zUrl);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !PH7_HttpStreamIs(pStream) ){
		/* A url wrapper of some other kind opened fine and has no response
		 * headers to answer with. */
		PH7_StreamCloseHandle(pStream,pHandle);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pH = (http_private *)pHandle;
	pArr = ph7_context_new_array(pCtx);
	pKey = ph7_context_new_scalar(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	pLine = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pKey == 0 || pVal == 0 || pLine == 0 ){
		HttpStream_Close(pHandle);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = (const char *)SyBlobData(&pH->sHdrs);
	zEnd = &zIn[SyBlobLength(&pH->sHdrs)];
	while( zIn < zEnd ){
		sxu32 nColon,nLine;
		zCur = zIn;
		while( zCur < zEnd && zCur[0] != '\n' ){
			zCur++;
		}
		nLine = (sxu32)(zCur - zIn);
		for( nColon = 0 ; nColon < nLine && zIn[nColon] != ':' ; ++nColon ){
			;
		}
		if( !bAssoc ){
			ph7_value_string(pLine,zIn,(int)nLine);
			ph7_array_add_elem(pArr,0,pLine);
			ph7_value_reset_string_cursor(pLine);
		}else if( nColon >= nLine ){
			/* No colon: a status line, and php numbers those from 0 whatever
			 * they say. */
			ph7_value_string(pLine,zIn,(int)nLine);
			ph7_value_int(pKey,iStatus++);
			ph7_array_add_elem(pArr,pKey,pLine);
			ph7_value_reset_string_cursor(pLine);
		}else{
			sxu32 i = nColon + 1;
			ph7_value *pOld;
			while( i < nLine && (zIn[i] == ' ' || zIn[i] == '\t') ){
				i++;
			}
			ph7_value_string(pKey,zIn,(int)nColon);
			ph7_value_string(pVal,&zIn[i],(int)(nLine - i));
			pOld = ph7_array_fetch(pArr,zIn,(int)nColon);
			if( pOld == 0 ){
				ph7_array_add_elem(pArr,pKey,pVal);
			}else if( pOld->iFlags & MEMOBJ_HASHMAP ){
				/* Already a list of its own: this is the third and later. */
				ph7_array_add_elem(pOld,0,pVal);
			}else{
				/* php turns the pair into a LIST the moment a name repeats. */
				ph7_value *pList = ph7_context_new_array(pCtx);
				if( pList ){
					ph7_array_add_elem(pList,0,pOld);
					ph7_array_add_elem(pList,0,pVal);
					ph7_array_add_elem(pArr,pKey,pList);
				}
			}
			ph7_value_reset_string_cursor(pKey);
			ph7_value_reset_string_cursor(pVal);
		}
		zIn = zCur < zEnd ? &zCur[1] : zEnd;
	}
	HttpStream_Close(pHandle);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
#endif /* !PH7_DISABLE_DISK_IO && PH7_ENABLE_NET */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * ?array http_get_last_response_headers()
 *
 * php 8.4's modern spelling of `$http_response_header`, and the same store: the
 * response headers of the last http:// exchange, or NULL when nothing has been
 * recorded since the last clear.
 */
static int PH7_builtin_http_get_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArr = PH7_HttpHeaderArray(pCtx->pVm,&pCtx->pVm->sHttpRespHdrs);
	if( pArr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_value(pCtx,pArr);
	ph7_release_value(pCtx->pVm,pArr);
	return PH7_OK;
}
/*
 * void http_clear_last_response_headers()
 *
 * Drops the store, so the getter above answers NULL rather than the array it
 * was answering a moment ago.
 */
static int PH7_builtin_http_clear_last_response_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_HttpClearResponseHeaders(pCtx->pVm);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * The three names a script asks an exchange about. The two getters are in every
 * build -- the store they read is -- and get_headers() goes with the wrapper it
 * is the one door of.
 */
PH7_PRIVATE void PH7_HttpInstallFuncs(ph7_vm *pVm)
{
	ph7_create_function(&(*pVm),"http_get_last_response_headers",
		PH7_builtin_http_get_last_response_headers,0);
	ph7_create_function(&(*pVm),"http_clear_last_response_headers",
		PH7_builtin_http_clear_last_response_headers,0);
#if !defined(PH7_DISABLE_DISK_IO) && defined(PH7_ENABLE_NET)
	ph7_create_function(&(*pVm),"get_headers",PH7_builtin_get_headers,0);
#endif
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
