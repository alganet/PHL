/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <time.h>
/*
 * HTTP response header and status code management.
 * Implements: header(), header_remove(), headers_sent(), headers_list(),
 *             http_response_code(), setcookie(), setrawcookie().
 */

/*
 * Free all response header strings and reset the set.
 * Called from PH7_VmReset() and header_remove() with no arguments.
 */
PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm)
{
	VmResponseHeader *aHdr;
	sxu32 i, n;
	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);
	n = SySetUsed(&pVm->aResponseHeaders);
	for( i = 0; i < n; i++ ){
		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);
		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);
	}
	SySetReset(&pVm->aResponseHeaders);
}
/*
 * Remove all response headers matching the given name (case-insensitive).
 */
static void VmRemoveHeaderByName(ph7_vm *pVm, const char *zName, sxu32 nName)
{
	sxu32 i, n;
	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);
	n = SySetUsed(&pVm->aResponseHeaders);
	for( i = 0; i < n; ){
		if( aHdr[i].sName.nByte == nName &&
			SyStrnicmp(aHdr[i].sName.zString, zName, nName) == 0 ){
			/* Free the duplicated strings */
			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);
			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);
			if( i < n - 1 ){
				aHdr[i] = aHdr[n - 1];
			}
			SySetPop(&pVm->aResponseHeaders);
			n--;
		}else{
			i++;
		}
	}
}
/*
 * Store a response header in the VM.
 * If bReplace is TRUE, removes existing headers with the same name first.
 */
static sxi32 VmAddResponseHeader(ph7_vm *pVm, const char *zName, sxu32 nName,
								  const char *zValue, sxu32 nValue, int bReplace)
{
	VmResponseHeader sHeader;
	char *zNameDup, *zValueDup;
	if( bReplace ){
		VmRemoveHeaderByName(pVm, zName, nName);
	}
	/* Duplicate name and value into VM allocator */
	zNameDup = SyMemBackendStrDup(&pVm->sAllocator, zName, nName);
	zValueDup = SyMemBackendStrDup(&pVm->sAllocator, zValue, nValue);
	if( zNameDup == 0 || zValueDup == 0 ){
		return SXERR_MEM;
	}
	SyStringInitFromBuf(&sHeader.sName, zNameDup, nName);
	SyStringInitFromBuf(&sHeader.sValue, zValueDup, nValue);
	return SySetPut(&pVm->aResponseHeaders, (const void *)&sHeader);
}
/*
 * The same store, for an extension that sets a header of its own rather than
 * carrying out a script's header() call: ob_gzhandler() announces the
 * Content-Encoding it just applied. Always REPLACES, which is what php's own
 * sapi_add_header does for these two.
 */
PH7_PRIVATE void PH7_VmAddResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue)
{
	VmAddResponseHeader(pVm,zName,(sxu32)SyStrlen(zName),
		zValue,(sxu32)SyStrlen(zValue),TRUE);
}
/*
 * php's headers-already-sent warning, in the two shapes php words it: the
 * header family's, which carries NO function prefix and hangs the origin off
 * the word "by", and http_response_code()'s, which is prefixed and does not.
 * Both name WHERE the response body began; with no origin recorded (nothing
 * emitted through a real consumer) the clause is left off entirely.
 */
static void VmHeadersAlreadySent(ph7_context *pCtx,int bResponseCode)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sMsg;
	SyString sFile;
	sxu32 nLine = 0;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( bResponseCode ){
		SyBlobAppend(&sMsg,"http_response_code(): Cannot set response code - headers already sent",
			sizeof("http_response_code(): Cannot set response code - headers already sent")-1);
	}else{
		SyBlobAppend(&sMsg,"Cannot modify header information - headers already sent",
			sizeof("Cannot modify header information - headers already sent")-1);
	}
	if( PH7_VmOutputOrigin(pVm,&sFile,&nLine) ){
		char zTail[64];
		if( !bResponseCode ){
			SyBlobAppend(&sMsg," by",3);
		}
		SyBlobAppend(&sMsg," (output started at ",sizeof(" (output started at ")-1);
		SyBlobAppend(&sMsg,sFile.zString,sFile.nByte);
		SyBufferFormat(zTail,sizeof(zTail),":%u)",nLine);
		SyBlobAppend(&sMsg,zTail,(sxu32)SyStrlen(zTail));
	}
	SyBlobNullAppend(&sMsg);
	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
}
/*
 * void header(string $header [, bool $replace = true [, int $response_code = 0]])
 *   Send a raw HTTP header.
 */
static int vm_builtin_header(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zHeader;
	int nLen;
	int bReplace = 1;
	int iCode = 0;
	const char *zColon;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		return PH7_OK;
	}
	/* php raises the headers-sent refusal in EVERY SAPI, the CLI included --
	 * its own header handler asks whether the response has begun before it
	 * asks whether anything will ever print the header. Only once that is
	 * settled does a build with no HTTP context stop caring. */
	if( pVm->bHeadersSent ){
		VmHeadersAlreadySent(pCtx,0);
		return PH7_OK;
	}
	if( !pVm->bHttpContext ){
		/* CLI: php takes the header and prints nothing. */
		return PH7_OK;
	}
	zHeader = ph7_value_to_string(apArg[0], &nLen);
	if( nLen < 1 ){
		return PH7_OK;
	}
	/* Reject headers containing CR or LF (prevents response splitting) */
	{
		int k;
		for( k = 0; k < nLen; k++ ){
			if( zHeader[k] == '\r' || zHeader[k] == '\n' ){
				ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
					"Header may not contain more than a single header, new line detected");
				return PH7_OK;
			}
		}
	}
	if( nArg >= 2 ){
		bReplace = ph7_value_to_bool(apArg[1]);
	}
	if( nArg >= 3 ){
		iCode = ph7_value_to_int(apArg[2]);
		if( iCode >= 100 && iCode <= 599 ){
			pVm->iResponseStatus = iCode;
		}
	}
	/* Check for HTTP/ status line */
	if( nLen >= 5 && SyStrnicmp(zHeader, "HTTP/", 5) == 0 ){
		/* e.g. "HTTP/1.1 404 Not Found" — extract status code */
		const char *z = zHeader + 5;
		const char *zEnd = zHeader + nLen;
		int iParsed = 0;
		/* Skip version */
		while( z < zEnd && *z != ' ' ) z++;
		while( z < zEnd && *z == ' ' ) z++;
		while( z < zEnd && *z >= '0' && *z <= '9' ){
			iParsed = iParsed * 10 + (*z - '0');
			z++;
		}
		if( iParsed >= 100 && iParsed <= 599 ){
			pVm->iResponseStatus = iParsed;
		}
		return PH7_OK;
	}
	/* Split on first ':' */
	{
		sxu32 nPos;
		if( SyByteFind(zHeader, (sxu32)nLen, ':', &nPos) == SXRET_OK ){
			zColon = zHeader + nPos;
		}else{
			zColon = 0;
		}
	}
	if( zColon == 0 ){
		/* No colon found — invalid header, ignore */
		return PH7_OK;
	}
	{
		sxu32 nName = (sxu32)(zColon - zHeader);
		const char *zValue = zColon + 1;
		sxu32 nValue;
		/* Skip leading whitespace in value */
		while( *zValue == ' ' || *zValue == '\t' ) zValue++;
		nValue = (sxu32)(nLen - (int)(zValue - zHeader));
		/* Auto-set 302 for Location header if status is still 200 */
		if( nName == 8 && SyStrnicmp(zHeader, "Location", 8) == 0 && pVm->iResponseStatus == 200 ){
			pVm->iResponseStatus = 302;
		}
		VmAddResponseHeader(pVm, zHeader, nName, zValue, nValue, bReplace);
	}
	return PH7_OK;
}
/*
 * void header_remove([string $name])
 *   Remove a previously set header. If no name given, remove all.
 */
static int vm_builtin_header_remove(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	/* The refusal comes first here too, for the reason header() gives. */
	if( pVm->bHeadersSent ){
		VmHeadersAlreadySent(pCtx,0);
		return PH7_OK;
	}
	if( !pVm->bHttpContext ){
		return PH7_OK;
	}
	if( nArg < 1 ){
		/* Remove all headers */
		PH7_VmReleaseResponseHeaders(pVm);
	}else{
		const char *zName = ph7_value_to_string(apArg[0], 0);
		VmRemoveHeaderByName(pVm, zName, (sxu32)SyStrlen(zName));
	}
	return PH7_OK;
}
/*
 * bool headers_sent([&$filename [, &$line]])
 *   Whether the response body has begun -- and, through its two by-ref
 *   out-params, WHERE it began. php writes both whatever the answer is: the
 *   empty string and 0 while nothing has been emitted, so a caller that reads
 *   them never sees its own previous value. This engine declared neither, so
 *   both variables kept whatever they held.
 */
static int vm_builtin_headers_sent(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyString sFile;
	sxu32 nLine = 0;
	PH7_VmOutputOrigin(pVm,&sFile,&nLine);
	if( nArg > 0 ){
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal ){
			ph7_value_string(pVal,sFile.nByte > 0 ? sFile.zString : "",(int)sFile.nByte);
			PH7_VmStoreArgByRef(pVm,apArg[0],pVal);
		}
	}
	if( nArg > 1 ){
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal ){
			ph7_value_int64(pVal,(ph7_int64)nLine);
			PH7_VmStoreArgByRef(pVm,apArg[1],pVal);
		}
	}
	ph7_result_bool(pCtx, pVm->bHeadersSent);
	return PH7_OK;
}
/*
 * array headers_list()
 *   Returns a list of response headers as "Name: Value" strings.
 */
static int vm_builtin_headers_list(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	ph7_value *pEntry;
	VmResponseHeader *aHdr;
	sxu32 i, n;
	(void)nArg; (void)apArg;
	pArray = ph7_context_new_array(pCtx);
	pEntry = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pEntry == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);
	n = SySetUsed(&pVm->aResponseHeaders);
	for( i = 0; i < n; i++ ){
		ph7_value_reset_string_cursor(pEntry);
		ph7_value_string_format(pEntry, "%.*s: %.*s",
			(int)aHdr[i].sName.nByte, aHdr[i].sName.zString,
			(int)aHdr[i].sValue.nByte, aHdr[i].sValue.zString);
		ph7_array_add_elem(pArray, 0, pEntry);
	}
	ph7_result_value(pCtx, pArray);
	return PH7_OK;
}
/*
 * int|bool http_response_code([int $response_code = 0])
 *   Get or set the HTTP response status code.
 *
 * php keeps ONE code, in every SAPI. Zero means "nothing set": a CLI script
 * that has not set one reads FALSE, and the first set answers TRUE rather than
 * a previous code -- while a request-driven run starts at 200 and every set
 * answers the code it replaced. A zero ARGUMENT is a read and not a write, and
 * php range-checks nothing at all, so 99 and -5 are stored as written. The one
 * refusal is a response that has already begun.
 *
 * This engine used to answer FALSE for every call outside the server and warn
 * on every set, so a CLI script could neither set a code nor read one back,
 * and it clamped 100..599 in the server where php clamps nothing.
 */
static int vm_builtin_http_response_code(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_int64 iCode = 0;
	if( nArg >= 1 ){
		iCode = ph7_value_to_int64(apArg[0]);
	}
	if( iCode == 0 ){
		/* A read: the standing code, or FALSE when there is none. */
		if( pVm->iResponseStatus ){
			ph7_result_int(pCtx, pVm->iResponseStatus);
		}else{
			ph7_result_bool(pCtx, 0);
		}
		return PH7_OK;
	}
	if( pVm->bHeadersSent ){
		VmHeadersAlreadySent(pCtx,1);
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( pVm->iResponseStatus ){
		ph7_result_int(pCtx, pVm->iResponseStatus);
	}else{
		ph7_result_bool(pCtx, 1);
	}
	pVm->iResponseStatus = (int)iCode;
	return PH7_OK;
}
/*
 * int connection_status()
 *   The state of the connection to the client: NORMAL, or one of the two ways
 *   php ends a request early. A CLI run has no client to lose and no time
 *   limit that ends anything, so php's own answer there is NORMAL for the
 *   whole run -- which is what makes the constant, not the number, the thing
 *   a program should compare against.
 */
static int vm_builtin_connection_status(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	(void)nArg; (void)apArg;
	ph7_result_int(pCtx, 0 /* CONNECTION_NORMAL */);
	return PH7_OK;
}
/*
 * int connection_aborted()
 *   Whether the client has gone away. Zero for the same reason.
 */
static int vm_builtin_connection_aborted(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	(void)nArg; (void)apArg;
	ph7_result_int(pCtx, 0);
	return PH7_OK;
}
/*
 * int ignore_user_abort([?bool $enable = null])
 *   Read or write the `ignore_user_abort` directive, answering the value it
 *   held BEFORE the call -- which is what makes the setting restorable. `null`
 *   is the value that only reads. The directive is a real one (ini_get() and
 *   ini_set() reach the same slot), so a program that saves and restores it
 *   through either door sees the same number.
 */
static int vm_builtin_ignore_user_abort(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iOld = PH7_VmIniGetInt(pVm,"ignore_user_abort",0);
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		int bOn = ph7_value_to_bool(apArg[0]);
		PH7_VmIniSet(pVm,"ignore_user_abort",sizeof("ignore_user_abort")-1,
			bOn ? "1" : "0",1,"ignore_user_abort()");
	}
	ph7_result_int(pCtx, (int)(iOld ? 1 : 0));
	return PH7_OK;
}
/*
 * The bytes a cookie NAME may not carry, and the ones a RAW value may not: php
 * refuses them where they are written rather than emitting a header a proxy would
 * read as two. A setcookie() VALUE is url-encoded and so has no such rule.
 */
#define VM_COOKIE_NAME_BAD  "=,; \t\r\n\013\014"
#define VM_COOKIE_VALUE_BAD  ",; \t\r\n\013\014"

static int VmCookieBadByte(const char *zVal,sxu32 nVal,const char *zBad,sxu32 nBad)
{
	sxu32 i;
	for( i = 0 ; i < nVal ; i++ ){
		if( SyByteFind(zBad,nBad,zVal[i],0) == SXRET_OK ){
			return 1;
		}
	}
	return 0;
}
/*
 * The options ARRAY php's third parameter has taken since 7.3 -- declared in
 * aBuiltinSig[] here and read as an INT, so every documented
 * `setcookie($n,$v,['expires'=>…,'samesite'=>'Lax'])` emitted a cookie with no
 * attributes at ALL: no expiry, no path, no SameSite. The key match is
 * case-insensitive and an unknown key is a ValueError, so a typo cannot silently
 * drop the attribute that makes the cookie safe.
 */
struct VmCookieOpts {
	sxi64 iExpires;
	/* COPIES, not the walk's own pointers: ph7_array_walk hands the callback a
	 * value it reuses for the next entry, so a `const char *` kept out of it is
	 * dangling by the time the header is built. */
	SyBlob sPath, sDomain, sSame;
	int bSecure, bHttpOnly, bPartitioned;
	char zBadKey[64];
};
static void VmCookieOptStr(SyBlob *pOut,ph7_value *pVal)
{
	int nVal = 0;
	const char *zVal = ph7_value_to_string(pVal,&nVal);
	SyBlobReset(pOut);
	if( nVal > 0 ){
		SyBlobAppend(pOut,zVal,(sxu32)nVal);
	}
}
static int VmCookieOptionWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	struct VmCookieOpts *pOpt = (struct VmCookieOpts *)pUserData;
	const char *zKey;
	int nKey = 0;
	zKey = ph7_value_to_string(pKey,&nKey);
	if( nKey == (int)sizeof("expires")-1 && SyStrnicmp(zKey,"expires",(sxu32)nKey) == 0 ){
		pOpt->iExpires = ph7_value_to_int64(pVal);
	}else if( nKey == (int)sizeof("path")-1 && SyStrnicmp(zKey,"path",(sxu32)nKey) == 0 ){
		VmCookieOptStr(&pOpt->sPath,pVal);
	}else if( nKey == (int)sizeof("domain")-1 && SyStrnicmp(zKey,"domain",(sxu32)nKey) == 0 ){
		VmCookieOptStr(&pOpt->sDomain,pVal);
	}else if( nKey == (int)sizeof("samesite")-1 && SyStrnicmp(zKey,"samesite",(sxu32)nKey) == 0 ){
		VmCookieOptStr(&pOpt->sSame,pVal);
	}else if( nKey == (int)sizeof("secure")-1 && SyStrnicmp(zKey,"secure",(sxu32)nKey) == 0 ){
		pOpt->bSecure = ph7_value_to_bool(pVal);
	}else if( nKey == (int)sizeof("httponly")-1 && SyStrnicmp(zKey,"httponly",(sxu32)nKey) == 0 ){
		pOpt->bHttpOnly = ph7_value_to_bool(pVal);
	}else if( nKey == (int)sizeof("partitioned")-1 && SyStrnicmp(zKey,"partitioned",(sxu32)nKey) == 0 ){
		pOpt->bPartitioned = ph7_value_to_bool(pVal);
	}else{
		sxu32 nCopy = (sxu32)nKey;
		if( nCopy > sizeof(pOpt->zBadKey)-1 ){
			nCopy = sizeof(pOpt->zBadKey)-1;
		}
		SyMemcpy(zKey,pOpt->zBadKey,nCopy);
		pOpt->zBadKey[nCopy] = 0;
		return SXERR_ABORT;   /* the caller reports which key it was */
	}
	return PH7_OK;
}
/*
 * An HTTP-date: "Thu, 19 Nov 1981 08:52:00 GMT", with the three-letter day and
 * month names an HTTP-date is defined in (locale-independent, from sxlib's own
 * tables). Answers the byte count, or 0 for a time the C library will not break
 * down.
 */
PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf)
{
#ifdef PH7_DISABLE_BUILTIN_FUNC
	/* The day/month name tables live behind the same switch: a build without the
	 * builtins has no date to print. */
	(void)iWhen; (void)zBuf; (void)nBuf;
	return 0;
#else
	time_t t = (time_t)iWhen;
	struct tm tm_buf;
	int tm_ok;
#ifdef __WINNT__
	tm_ok = (gmtime_s(&tm_buf,&t) == 0);
#else
	tm_ok = (gmtime_r(&t,&tm_buf) != 0);
#endif
	if( !tm_ok ){
		return 0;
	}
	return SyBufferFormat(zBuf,(sxu32)nBuf,"%.3s, %02d %.3s %04d %02d:%02d:%02d GMT",
		SyTimeGetDay(tm_buf.tm_wday),tm_buf.tm_mday,
		SyTimeGetMonth(tm_buf.tm_mon),1900 + tm_buf.tm_year,
		tm_buf.tm_hour,tm_buf.tm_min,tm_buf.tm_sec);
#endif
}
/* Queue a response header, replacing any of the same name. */
PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,
	sxu32 nValue)
{
	VmAddResponseHeader(pVm,zName,(sxu32)SyStrlen(zName),zValue,nValue,1);
}
/*
 * Drop any Set-Cookie already queued for this cookie NAME. php does this before it
 * sends the session cookie (php_session_remove_cookie): a request that regenerates
 * its id must not leave the OLD id in the reply beside the new one, and only the
 * cookie of that name goes -- the header name is shared with every other cookie.
 */
PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName)
{
	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);
	sxu32 i,n = SySetUsed(&pVm->aResponseHeaders);
	for( i = 0 ; i < n ; ){
		const SyString *pVal = &aHdr[i].sValue;
		if( aHdr[i].sName.nByte == sizeof("Set-Cookie")-1
		 && SyStrnicmp(aHdr[i].sName.zString,"Set-Cookie",sizeof("Set-Cookie")-1) == 0
		 && pVal->nByte > nName
		 && SyMemcmp(pVal->zString,zName,nName) == 0
		 && pVal->zString[nName] == '=' ){
			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sName.zString);
			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sValue.zString);
			if( i < n - 1 ){
				aHdr[i] = aHdr[n - 1];
			}
			SySetPop(&pVm->aResponseHeaders);
			n--;
		}else{
			i++;
		}
	}
}
/*
 * Build the Set-Cookie value php builds, and append it (never replace).
 * PH7_PRIVATE because the session's own cookie is this same header with the
 * session's parameters, not a second spelling of it.
 */
PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,
	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,
	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,
	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned)
{
	SyBlob sWorker;
	SyBlobInit(&sWorker,&pVm->sAllocator);
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( bEncode ){
		/* RAW url-encoding, php's: a space becomes %20 and `~` is left alone,
		 * which is not what urlencode() does — and a cookie value is read back by
		 * a browser, so the two spellings are not interchangeable. */
		SyUriEncodeRaw(zName,nName,PH7_VmBlobConsumer,&sWorker);
	}else
#else
	(void)bEncode;
#endif
	{
		SyBlobAppend(&sWorker,zName,nName);
	}
	SyBlobAppend(&sWorker,"=",1);
	if( nValue < 1 ){
		/* php replaces an EMPTY value with the literal `deleted` and dates the
		 * cookie to the epoch: that is what "unset this cookie" IS on the wire. */
		SyBlobAppend(&sWorker,"deleted",sizeof("deleted")-1);
		iExpires = 1;
	}else
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( bEncode ){
		SyUriEncodeRaw(zValue,nValue,PH7_VmBlobConsumer,&sWorker);
	}else
#endif
	{
		SyBlobAppend(&sWorker,zValue,nValue);
	}
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( iExpires > 0 ){
		char zDate[80];
		int nDate = PH7_VmHttpDate(iExpires,zDate,(int)sizeof(zDate));
		if( nDate > 0 ){
			sxi64 iMaxAge = iExpires - (sxi64)time(0);
			char zTail[64];
			SyBlobAppend(&sWorker,"; expires=",sizeof("; expires=")-1);
			SyBlobAppend(&sWorker,zDate,(sxu32)nDate);
			nDate = SyBufferFormat(zTail,sizeof(zTail),"; Max-Age=%qd",
				iMaxAge < 0 ? (sxi64)0 : iMaxAge);
			SyBlobAppend(&sWorker,zTail,(sxu32)nDate);
		}
	}
#else
	(void)iExpires;
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	if( nPath > 0 ){
		SyBlobAppend(&sWorker,"; path=",sizeof("; path=")-1);
		SyBlobAppend(&sWorker,zPath,nPath);
	}
	if( nDomain > 0 ){
		SyBlobAppend(&sWorker,"; domain=",sizeof("; domain=")-1);
		SyBlobAppend(&sWorker,zDomain,nDomain);
	}
	if( bSecure ){
		SyBlobAppend(&sWorker,"; secure",sizeof("; secure")-1);
	}
	if( bHttpOnly ){
		SyBlobAppend(&sWorker,"; HttpOnly",sizeof("; HttpOnly")-1);
	}
	if( nSame > 0 ){
		SyBlobAppend(&sWorker,"; SameSite=",sizeof("; SameSite=")-1);
		SyBlobAppend(&sWorker,zSame,nSame);
	}
	if( bPartitioned ){
		SyBlobAppend(&sWorker,"; Partitioned",sizeof("; Partitioned")-1);
	}
	VmAddResponseHeader(pVm,"Set-Cookie",10,
		(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),
		0 /* bReplace = false */);
	SyBlobRelease(&sWorker);
}
/*
 * Internal helper for setcookie/setrawcookie.
 */
static int VmSetCookieImpl(ph7_context *pCtx, int nArg, ph7_value **apArg, int bEncode)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zName, *zValue;
	const char *zFunc = bEncode ? "setcookie" : "setrawcookie";
	int nNameLen, nValueLen = 0, bBadOpt = 0;
	struct VmCookieOpts sOpt;
	if( nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nNameLen);
	if( nNameLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($name) must not be empty",zFunc);
	}
	if( VmCookieBadByte(zName,(sxu32)nNameLen,
		VM_COOKIE_NAME_BAD,sizeof(VM_COOKIE_NAME_BAD)-1) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($name) cannot contain \"=\", \",\", \";\","
			" \" \", \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);
	}
	if( nArg >= 2 ){
		zValue = ph7_value_to_string(apArg[1], &nValueLen);
	}else{
		zValue = "";
	}
	if( !bEncode && VmCookieBadByte(zValue,(sxu32)nValueLen,
		VM_COOKIE_VALUE_BAD,sizeof(VM_COOKIE_VALUE_BAD)-1) ){
		/* Only the RAW value reaches the wire unchanged, so only it is screened;
		 * setcookie() url-encodes and can carry anything. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #2 ($value) cannot contain \",\", \";\", \" \","
			" \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);
	}
	SyZero(&sOpt,sizeof(sOpt));
	SyBlobInit(&sOpt.sPath,&pVm->sAllocator);
	SyBlobInit(&sOpt.sDomain,&pVm->sAllocator);
	SyBlobInit(&sOpt.sSame,&pVm->sAllocator);
	if( nArg >= 3 && ph7_value_is_array(apArg[2]) ){
		bBadOpt = ph7_array_walk(apArg[2],VmCookieOptionWalker,&sOpt) != PH7_OK;
	}else if( nArg >= 3 ){
		sOpt.iExpires = ph7_value_to_int64(apArg[2]);
		if( nArg >= 4 ){
			VmCookieOptStr(&sOpt.sPath,apArg[3]);
		}
		if( nArg >= 5 ){
			VmCookieOptStr(&sOpt.sDomain,apArg[4]);
		}
		if( nArg >= 6 ){
			sOpt.bSecure = ph7_value_to_bool(apArg[5]);
		}
		if( nArg >= 7 ){
			sOpt.bHttpOnly = ph7_value_to_bool(apArg[6]);
		}
	}
	if( !bBadOpt ){
		/* php's CLI SAPI takes the header and answers TRUE even though nothing will
		 * ever print it; only a real header ALREADY sent is a refusal. */
		if( pVm->bHeadersSent ){
			VmHeadersAlreadySent(pCtx,0);
			ph7_result_bool(pCtx, 0);
		}else{
			if( pVm->bHttpContext ){
				PH7_VmEmitCookie(pVm,zName,(sxu32)nNameLen,zValue,(sxu32)nValueLen,bEncode,
					sOpt.iExpires,
					(const char *)SyBlobData(&sOpt.sPath),SyBlobLength(&sOpt.sPath),
					(const char *)SyBlobData(&sOpt.sDomain),SyBlobLength(&sOpt.sDomain),
					sOpt.bSecure,sOpt.bHttpOnly,
					(const char *)SyBlobData(&sOpt.sSame),SyBlobLength(&sOpt.sSame),
					sOpt.bPartitioned);
			}
			ph7_result_bool(pCtx, 1);
		}
	}
	SyBlobRelease(&sOpt.sPath);
	SyBlobRelease(&sOpt.sDomain);
	SyBlobRelease(&sOpt.sSame);
	if( bBadOpt ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): option \"%s\" is invalid",zFunc,sOpt.zBadKey);
	}
	return PH7_OK;
}
/*
 * bool setcookie(string $name [, string $value [, int $expires [, string $path
 *                [, string $domain [, bool $secure [, bool $httponly]]]]]])
 */
static int vm_builtin_setcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return VmSetCookieImpl(pCtx, nArg, apArg, 1 /* URL-encode */);
}
/*
 * bool setrawcookie(string $name [, string $value [, ...]])
 */
static int vm_builtin_setrawcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return VmSetCookieImpl(pCtx, nArg, apArg, 0 /* no encoding */);
}
/*
 * Register all HTTP response functions with the VM.
 */
PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm)
{
	static const ph7_builtin_func aFunc[] = {
		{ "header",             vm_builtin_header             },
		{ "header_remove",      vm_builtin_header_remove      },
		{ "headers_sent",       vm_builtin_headers_sent       },
		{ "connection_status",  vm_builtin_connection_status  },
		{ "connection_aborted", vm_builtin_connection_aborted },
		{ "ignore_user_abort",  vm_builtin_ignore_user_abort  },
		{ "headers_list",       vm_builtin_headers_list       },
		{ "http_response_code", vm_builtin_http_response_code },
		{ "setcookie",          vm_builtin_setcookie          },
		{ "setrawcookie",       vm_builtin_setrawcookie       },
	};
	sxu32 n;
	for( n = 0; n < SX_ARRAYSIZE(aFunc); n++ ){
		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);
	}
}
