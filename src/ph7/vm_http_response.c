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
	/* In CLI mode (no HTTP context), header() is silently ignored */
	if( !pVm->bHttpContext ){
		return PH7_OK;
	}
	/* Check if headers already sent */
	if( pVm->bHeadersSent ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");
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
	if( !pVm->bHttpContext ){
		return PH7_OK;
	}
	if( pVm->bHeadersSent ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");
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
 * bool headers_sent()
 *   Returns TRUE if headers have already been sent (output started).
 */
static int vm_builtin_headers_sent(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	(void)nArg; (void)apArg;
	ph7_result_bool(pCtx, pCtx->pVm->bHeadersSent);
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
 * int http_response_code([int $code])
 *   Get or set the HTTP response status code.
 */
static int vm_builtin_http_response_code(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( !pVm->bHttpContext ){
		/* CLI mode: no HTTP context */
		if( nArg >= 1 && ph7_value_is_int(apArg[0]) ){
			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
				"Cannot set response code - headers already sent");
		}
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* HTTP context (server/CGI mode) */
	if( nArg >= 1 && ph7_value_is_int(apArg[0]) ){
		int iCode = ph7_value_to_int(apArg[0]);
		int iPrev = pVm->iResponseStatus;
		if( pVm->bHeadersSent ){
			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,
				"Cannot set response code - headers already sent");
			ph7_result_bool(pCtx, 0);
			return PH7_OK;
		}
		if( iCode >= 100 && iCode <= 599 ){
			pVm->iResponseStatus = iCode;
		}
		/* Return the previous status code */
		ph7_result_int(pCtx, iPrev);
	}else{
		/* Return current status code */
		ph7_result_int(pCtx, pVm->iResponseStatus);
	}
	return PH7_OK;
}
/*
 * Internal helper for setcookie/setrawcookie.
 * Builds a Set-Cookie header and appends it (never replaces).
 */
static int VmSetCookieImpl(ph7_context *pCtx, int nArg, ph7_value **apArg, int bEncode)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zName, *zValue;
	int nNameLen, nValueLen;
	SyBlob sWorker;
	if( nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( !pVm->bHttpContext ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( pVm->bHeadersSent ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nNameLen);
	if( nArg >= 2 ){
		zValue = ph7_value_to_string(apArg[1], &nValueLen);
	}else{
		zValue = "";
		nValueLen = 0;
	}
	SyBlobInit(&sWorker, &pVm->sAllocator);
	/* Build the cookie value */
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( bEncode ){
		/* URL-encode name and value */
		SyUriEncode(zName, (sxu32)nNameLen, PH7_VmBlobConsumer, &sWorker);
		SyBlobAppend(&sWorker, "=", 1);
		if( nValueLen > 0 ){
			SyUriEncode(zValue, (sxu32)nValueLen, PH7_VmBlobConsumer, &sWorker);
		}
	}else
#else
	(void)bEncode;
#endif
	{
		SyBlobAppend(&sWorker, zName, (sxu32)nNameLen);
		SyBlobAppend(&sWorker, "=", 1);
		if( nValueLen > 0 ){
			SyBlobAppend(&sWorker, zValue, (sxu32)nValueLen);
		}
	}
	/* expires (requires SyTimeGetDay/SyTimeGetMonth from sxlib) */
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( nArg >= 3 ){
		sxi64 iExpires = ph7_value_to_int64(apArg[2]);
		if( iExpires > 0 ){
			time_t t = (time_t)iExpires;
			struct tm tm_buf;
			char zDate[64];
			int tm_ok = 0;
#ifdef __WINNT__
			tm_ok = (gmtime_s(&tm_buf, &t) == 0);
#else
			tm_ok = (gmtime_r(&t, &tm_buf) != 0);
#endif
			if( tm_ok ){
				/* Use locale-independent day/month names */
				SyBufferFormat(zDate, sizeof(zDate),
					"%s, %02d %s %04d %02d:%02d:%02d GMT",
					SyTimeGetDay(tm_buf.tm_wday), tm_buf.tm_mday,
					SyTimeGetMonth(tm_buf.tm_mon), 1900 + tm_buf.tm_year,
					tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
				SyBlobAppend(&sWorker, "; expires=", 10);
				SyBlobAppend(&sWorker, zDate, (sxu32)SyStrlen(zDate));
			}
		}
	}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	/* path */
	if( nArg >= 4 ){
		const char *zPath = ph7_value_to_string(apArg[3], 0);
		if( zPath && zPath[0] ){
			SyBlobAppend(&sWorker, "; path=", 7);
			SyBlobAppend(&sWorker, zPath, (sxu32)SyStrlen(zPath));
		}
	}
	/* domain */
	if( nArg >= 5 ){
		const char *zDomain = ph7_value_to_string(apArg[4], 0);
		if( zDomain && zDomain[0] ){
			SyBlobAppend(&sWorker, "; domain=", 9);
			SyBlobAppend(&sWorker, zDomain, (sxu32)SyStrlen(zDomain));
		}
	}
	/* secure */
	if( nArg >= 6 && ph7_value_to_bool(apArg[5]) ){
		SyBlobAppend(&sWorker, "; secure", 8);
	}
	/* httponly */
	if( nArg >= 7 && ph7_value_to_bool(apArg[6]) ){
		SyBlobAppend(&sWorker, "; httponly", 9);
	}
	/* Append as Set-Cookie header (never replace) */
	VmAddResponseHeader(pVm, "Set-Cookie", 10,
		(const char *)SyBlobData(&sWorker), SyBlobLength(&sWorker),
		0 /* bReplace = false */);
	SyBlobRelease(&sWorker);
	ph7_result_bool(pCtx, 1);
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
