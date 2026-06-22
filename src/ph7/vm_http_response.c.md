# src/ph7/vm_http_response.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 114/255 lines (44.71%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <time.h>` |
|     - |    7 | `/*` |
|     - |    8 | ` * HTTP response header and status code management.` |
|     - |    9 | ` * Implements: header(), header_remove(), headers_sent(), headers_list(),` |
|     - |   10 | ` *             http_response_code(), setcookie(), setrawcookie().` |
|     - |   11 | ` */` |
|     - |   12 |  |
|     - |   13 | `/*` |
|     - |   14 | ` * Free all response header strings and reset the set.` |
|     - |   15 | ` * Called from PH7_VmReset() and header_remove() with no arguments.` |
|     - |   16 | ` */` |
|     6 |   17 | `PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm)` |
|   ! 0 |   18 |  |
|     - |   19 | `	VmResponseHeader *aHdr;` |
|     - |   20 | `	sxu32 i, n;` |
|     6 |   21 | `	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|     6 |   22 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|     6 |   23 | `	for( i = 0; i < n; i++ ){` |
|   ! 0 |   24 | `		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);` |
|   ! 0 |   25 | `		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);` |
|   ! 0 |   26 | `	}` |
|     6 |   27 | `	SySetReset(&pVm->aResponseHeaders);` |
|     6 |   28 |  |
|     - |   29 | `/*` |
|     - |   30 | ` * Remove all response headers matching the given name (case-insensitive).` |
|     - |   31 | ` */` |
|    12 |   32 | `static void VmRemoveHeaderByName(ph7_vm *pVm, const char *zName, sxu32 nName)` |
|   ! 0 |   33 |  |
|     - |   34 | `	sxu32 i, n;` |
|    12 |   35 | `	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    12 |   36 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|    18 |   37 | `	for( i = 0; i < n; ){` |
|     7 |   38 | `		if( aHdr[i].sName.nByte == nName &&` |
|     2 |   39 | `			SyStrnicmp(aHdr[i].sName.zString, zName, nName) == 0 ){` |
|     - |   40 | `			/* Free the duplicated strings */` |
|     2 |   41 | `			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);` |
|     2 |   42 | `			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);` |
|     2 |   43 | `			if( i < n - 1 ){` |
|   ! 0 |   44 | `				aHdr[i] = aHdr[n - 1];` |
|   ! 0 |   45 | `			}` |
|     2 |   46 | `			SySetPop(&pVm->aResponseHeaders);` |
|     2 |   47 | `			n--;` |
|     1 |   48 | `		}else{` |
|     4 |   49 | `			i++;` |
|     - |   50 | `		}` |
|   ! 0 |   51 | `	}` |
|    12 |   52 |  |
|     - |   53 | `/*` |
|     - |   54 | ` * Store a response header in the VM.` |
|     - |   55 | ` * If bReplace is TRUE, removes existing headers with the same name first.` |
|     - |   56 | ` */` |
|    14 |   57 | `static sxi32 VmAddResponseHeader(ph7_vm *pVm, const char *zName, sxu32 nName,` |
|     - |   58 | `								  const char *zValue, sxu32 nValue, int bReplace)` |
|   ! 0 |   59 |  |
|     - |   60 | `	VmResponseHeader sHeader;` |
|     - |   61 | `	char *zNameDup, *zValueDup;` |
|    14 |   62 | `	if( bReplace ){` |
|    12 |   63 | `		VmRemoveHeaderByName(pVm, zName, nName);` |
|     6 |   64 | `	}` |
|     - |   65 | `	/* Duplicate name and value into VM allocator */` |
|    14 |   66 | `	zNameDup = SyMemBackendStrDup(&pVm->sAllocator, zName, nName);` |
|    14 |   67 | `	zValueDup = SyMemBackendStrDup(&pVm->sAllocator, zValue, nValue);` |
|    14 |   68 | `	if( zNameDup == 0 \|\| zValueDup == 0 ){` |
|   ! 0 |   69 | `		return SXERR_MEM;` |
|     - |   70 | `	}` |
|    14 |   71 | `	SyStringInitFromBuf(&sHeader.sName, zNameDup, nName);` |
|    14 |   72 | `	SyStringInitFromBuf(&sHeader.sValue, zValueDup, nValue);` |
|    14 |   73 | `	return SySetPut(&pVm->aResponseHeaders, (const void *)&sHeader);` |
|     7 |   74 |  |
|     - |   75 | `/*` |
|     - |   76 | ` * void header(string $header [, bool $replace = true [, int $response_code = 0]])` |
|     - |   77 | ` *   Send a raw HTTP header.` |
|     - |   78 | ` */` |
|    22 |   79 | `static int vm_builtin_header(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |   80 |  |
|    24 |   81 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   82 | `	const char *zHeader;` |
|     - |   83 | `	int nLen;` |
|    24 |   84 | `	int bReplace = 1;` |
|    24 |   85 | `	int iCode = 0;` |
|     - |   86 | `	const char *zColon;` |
|    24 |   87 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|   ! 0 |   88 | `		return PH7_OK;` |
|     - |   89 | `	}` |
|     - |   90 | `	/* In CLI mode (no HTTP context), header() is silently ignored */` |
|    24 |   91 | `	if( !pVm->bHttpContext ){` |
|     8 |   92 | `		return PH7_OK;` |
|     - |   93 | `	}` |
|     - |   94 | `	/* Check if headers already sent */` |
|    16 |   95 | `	if( pVm->bHeadersSent ){` |
|   ! 0 |   96 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");` |
|   ! 0 |   97 | `		return PH7_OK;` |
|     - |   98 | `	}` |
|    16 |   99 | `	zHeader = ph7_value_to_string(apArg[0], &nLen);` |
|    16 |  100 | `	if( nLen < 1 ){` |
|   ! 0 |  101 | `		return PH7_OK;` |
|     - |  102 | `	}` |
|     - |  103 | `	/* Reject headers containing CR or LF (prevents response splitting) */` |
|     - |  104 | `	{` |
|     - |  105 | `		int k;` |
|   254 |  106 | `		for( k = 0; k < nLen; k++ ){` |
|   240 |  107 | `			if( zHeader[k] == '\r' \|\| zHeader[k] == '\n' ){` |
|     2 |  108 | `				ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  109 | `					"Header may not contain more than a single header, new line detected");` |
|     2 |  110 | `				return PH7_OK;` |
|     - |  111 | `			}` |
|   119 |  112 | `		}` |
|     - |  113 | `	}` |
|    14 |  114 | `	if( nArg >= 2 ){` |
|     2 |  115 | `		bReplace = ph7_value_to_bool(apArg[1]);` |
|     1 |  116 | `	}` |
|    14 |  117 | `	if( nArg >= 3 ){` |
|   ! 0 |  118 | `		iCode = ph7_value_to_int(apArg[2]);` |
|   ! 0 |  119 | `		if( iCode >= 100 && iCode <= 599 ){` |
|   ! 0 |  120 | `			pVm->iResponseStatus = iCode;` |
|   ! 0 |  121 | `		}` |
|   ! 0 |  122 | `	}` |
|     - |  123 | `	/* Check for HTTP/ status line */` |
|    14 |  124 | `	if( nLen >= 5 && SyStrnicmp(zHeader, "HTTP/", 5) == 0 ){` |
|     - |  125 | `		/* e.g. "HTTP/1.1 404 Not Found" — extract status code */` |
|   ! 0 |  126 | `		const char *z = zHeader + 5;` |
|   ! 0 |  127 | `		const char *zEnd = zHeader + nLen;` |
|   ! 0 |  128 | `		int iParsed = 0;` |
|     - |  129 | `		/* Skip version */` |
|   ! 0 |  130 | `		while( z < zEnd && *z != ' ' ) z++;` |
|   ! 0 |  131 | `		while( z < zEnd && *z == ' ' ) z++;` |
|   ! 0 |  132 | `		while( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  133 | `			iParsed = iParsed * 10 + (*z - '0');` |
|   ! 0 |  134 | `			z++;` |
|   ! 0 |  135 | `		}` |
|   ! 0 |  136 | `		if( iParsed >= 100 && iParsed <= 599 ){` |
|   ! 0 |  137 | `			pVm->iResponseStatus = iParsed;` |
|   ! 0 |  138 | `		}` |
|   ! 0 |  139 | `		return PH7_OK;` |
|     - |  140 | `	}` |
|     - |  141 | `	/* Split on first ':' */` |
|     - |  142 | `	{` |
|     - |  143 | `		sxu32 nPos;` |
|    14 |  144 | `		if( SyByteFind(zHeader, (sxu32)nLen, ':', &nPos) == SXRET_OK ){` |
|    14 |  145 | `			zColon = zHeader + nPos;` |
|     7 |  146 | `		}else{` |
|   ! 0 |  147 | `			zColon = 0;` |
|     - |  148 | `		}` |
|     - |  149 | `	}` |
|    14 |  150 | `	if( zColon == 0 ){` |
|     - |  151 | `		/* No colon found — invalid header, ignore */` |
|   ! 0 |  152 | `		return PH7_OK;` |
|     - |  153 | `	}` |
|     - |  154 | `	{` |
|    14 |  155 | `		sxu32 nName = (sxu32)(zColon - zHeader);` |
|    14 |  156 | `		const char *zValue = zColon + 1;` |
|     - |  157 | `		sxu32 nValue;` |
|     - |  158 | `		/* Skip leading whitespace in value */` |
|    28 |  159 | `		while( *zValue == ' ' \|\| *zValue == '\t' ) zValue++;` |
|    14 |  160 | `		nValue = (sxu32)(nLen - (int)(zValue - zHeader));` |
|     - |  161 | `		/* Auto-set 302 for Location header if status is still 200 */` |
|    14 |  162 | `		if( nName == 8 && SyStrnicmp(zHeader, "Location", 8) == 0 && pVm->iResponseStatus == 200 ){` |
|   ! 0 |  163 | `			pVm->iResponseStatus = 302;` |
|   ! 0 |  164 | `		}` |
|    14 |  165 | `		VmAddResponseHeader(pVm, zHeader, nName, zValue, nValue, bReplace);` |
|     - |  166 | `	}` |
|    14 |  167 | `	return PH7_OK;` |
|    13 |  168 |  |
|     - |  169 | `/*` |
|     - |  170 | ` * void header_remove([string $name])` |
|     - |  171 | ` *   Remove a previously set header. If no name given, remove all.` |
|     - |  172 | ` */` |
|   ! 0 |  173 | `static int vm_builtin_header_remove(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 |  174 |  |
|   ! 0 |  175 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 |  176 | `	if( !pVm->bHttpContext ){` |
|   ! 0 |  177 | `		return PH7_OK;` |
|     - |  178 | `	}` |
|   ! 0 |  179 | `	if( pVm->bHeadersSent ){` |
|   ! 0 |  180 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");` |
|   ! 0 |  181 | `		return PH7_OK;` |
|     - |  182 | `	}` |
|   ! 0 |  183 | `	if( nArg < 1 ){` |
|     - |  184 | `		/* Remove all headers */` |
|   ! 0 |  185 | `		PH7_VmReleaseResponseHeaders(pVm);` |
|   ! 0 |  186 | `	}else{` |
|   ! 0 |  187 | `		const char *zName = ph7_value_to_string(apArg[0], 0);` |
|   ! 0 |  188 | `		VmRemoveHeaderByName(pVm, zName, (sxu32)SyStrlen(zName));` |
|     - |  189 | `	}` |
|   ! 0 |  190 | `	return PH7_OK;` |
|   ! 0 |  191 |  |
|     - |  192 | `/*` |
|     - |  193 | ` * bool headers_sent()` |
|     - |  194 | ` *   Returns TRUE if headers have already been sent (output started).` |
|     - |  195 | ` */` |
|     8 |  196 | `static int vm_builtin_headers_sent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  197 |  |
|     4 |  198 | `	(void)nArg; (void)apArg;` |
|     9 |  199 | `	ph7_result_bool(pCtx, pCtx->pVm->bHeadersSent);` |
|     9 |  200 | `	return PH7_OK;` |
|     1 |  201 |  |
|     - |  202 | `/*` |
|     - |  203 | ` * array headers_list()` |
|     - |  204 | ` *   Returns a list of response headers as "Name: Value" strings.` |
|     - |  205 | ` */` |
|    10 |  206 | `static int vm_builtin_headers_list(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  207 |  |
|    12 |  208 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  209 | `	ph7_value *pArray;` |
|     - |  210 | `	ph7_value *pEntry;` |
|     - |  211 | `	VmResponseHeader *aHdr;` |
|     - |  212 | `	sxu32 i, n;` |
|     5 |  213 | `	(void)nArg; (void)apArg;` |
|    12 |  214 | `	pArray = ph7_context_new_array(pCtx);` |
|    12 |  215 | `	pEntry = ph7_context_new_scalar(pCtx);` |
|    12 |  216 | `	if( pArray == 0 \|\| pEntry == 0 ){` |
|   ! 0 |  217 | `		ph7_result_null(pCtx);` |
|   ! 0 |  218 | `		return PH7_OK;` |
|     - |  219 | `	}` |
|    12 |  220 | `	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    12 |  221 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|    22 |  222 | `	for( i = 0; i < n; i++ ){` |
|    10 |  223 | `		ph7_value_reset_string_cursor(pEntry);` |
|    15 |  224 | `		ph7_value_string_format(pEntry, "%.*s: %.*s",` |
|    10 |  225 | `			(int)aHdr[i].sName.nByte, aHdr[i].sName.zString,` |
|    10 |  226 | `			(int)aHdr[i].sValue.nByte, aHdr[i].sValue.zString);` |
|    10 |  227 | `		ph7_array_add_elem(pArray, 0, pEntry);` |
|     5 |  228 | `	}` |
|    12 |  229 | `	ph7_result_value(pCtx, pArray);` |
|    12 |  230 | `	return PH7_OK;` |
|     7 |  231 |  |
|     - |  232 | `/*` |
|     - |  233 | ` * int http_response_code([int $code])` |
|     - |  234 | ` *   Get or set the HTTP response status code.` |
|     - |  235 | ` */` |
|    16 |  236 | `static int vm_builtin_http_response_code(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  237 |  |
|    17 |  238 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  239 | `	if( !pVm->bHttpContext ){` |
|     - |  240 | `		/* CLI mode: no HTTP context */` |
|     3 |  241 | `		if( nArg >= 1 && ph7_value_is_int(apArg[0]) ){` |
|   ! 0 |  242 | `			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  243 | `				"Cannot set response code - headers already sent");` |
|   ! 0 |  244 | `		}` |
|     3 |  245 | `		ph7_result_bool(pCtx, 0);` |
|     3 |  246 | `		return PH7_OK;` |
|     - |  247 | `	}` |
|     - |  248 | `	/* HTTP context (server/CGI mode) */` |
|    17 |  249 | `	if( nArg >= 1 && ph7_value_is_int(apArg[0]) ){` |
|     6 |  250 | `		int iCode = ph7_value_to_int(apArg[0]);` |
|     6 |  251 | `		int iPrev = pVm->iResponseStatus;` |
|     6 |  252 | `		if( pVm->bHeadersSent ){` |
|   ! 0 |  253 | `			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  254 | `				"Cannot set response code - headers already sent");` |
|   ! 0 |  255 | `			ph7_result_bool(pCtx, 0);` |
|   ! 0 |  256 | `			return PH7_OK;` |
|     - |  257 | `		}` |
|     6 |  258 | `		if( iCode >= 100 && iCode <= 599 ){` |
|     6 |  259 | `			pVm->iResponseStatus = iCode;` |
|     3 |  260 | `		}` |
|     - |  261 | `		/* Return the previous status code */` |
|     6 |  262 | `		ph7_result_int(pCtx, iPrev);` |
|     3 |  263 | `	}else{` |
|     - |  264 | `		/* Return current status code */` |
|     8 |  265 | `		ph7_result_int(pCtx, pVm->iResponseStatus);` |
|     - |  266 | `	}` |
|    14 |  267 | `	return PH7_OK;` |
|     9 |  268 |  |
|     - |  269 | `/*` |
|     - |  270 | ` * Internal helper for setcookie/setrawcookie.` |
|     - |  271 | ` * Builds a Set-Cookie header and appends it (never replaces).` |
|     - |  272 | ` */` |
|   ! 0 |  273 | `static int VmSetCookieImpl(ph7_context *pCtx, int nArg, ph7_value **apArg, int bEncode)` |
|   ! 0 |  274 |  |
|   ! 0 |  275 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  276 | `	const char *zName, *zValue;` |
|     - |  277 | `	int nNameLen, nValueLen;` |
|     - |  278 | `	SyBlob sWorker;` |
|   ! 0 |  279 | `	if( nArg < 1 ){` |
|   ! 0 |  280 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  281 | `		return PH7_OK;` |
|     - |  282 | `	}` |
|   ! 0 |  283 | `	if( !pVm->bHttpContext ){` |
|   ! 0 |  284 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  285 | `		return PH7_OK;` |
|     - |  286 | `	}` |
|   ! 0 |  287 | `	if( pVm->bHeadersSent ){` |
|   ! 0 |  288 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Cannot modify header information - headers already sent");` |
|   ! 0 |  289 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  290 | `		return PH7_OK;` |
|     - |  291 | `	}` |
|   ! 0 |  292 | `	zName = ph7_value_to_string(apArg[0], &nNameLen);` |
|   ! 0 |  293 | `	if( nArg >= 2 ){` |
|   ! 0 |  294 | `		zValue = ph7_value_to_string(apArg[1], &nValueLen);` |
|   ! 0 |  295 | `	}else{` |
|   ! 0 |  296 | `		zValue = "";` |
|   ! 0 |  297 | `		nValueLen = 0;` |
|     - |  298 | `	}` |
|   ! 0 |  299 | `	SyBlobInit(&sWorker, &pVm->sAllocator);` |
|     - |  300 | `	/* Build the cookie value */` |
|     - |  301 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   ! 0 |  302 | `	if( bEncode ){` |
|     - |  303 | `		/* URL-encode name and value */` |
|   ! 0 |  304 | `		SyUriEncode(zName, (sxu32)nNameLen, PH7_VmBlobConsumer, &sWorker);` |
|   ! 0 |  305 | `		SyBlobAppend(&sWorker, "=", 1);` |
|   ! 0 |  306 | `		if( nValueLen > 0 ){` |
|   ! 0 |  307 | `			SyUriEncode(zValue, (sxu32)nValueLen, PH7_VmBlobConsumer, &sWorker);` |
|   ! 0 |  308 | `		}` |
|   ! 0 |  309 | `	}else` |
|     - |  310 | `#else` |
|     - |  311 | `	(void)bEncode;` |
|     - |  312 | `#endif` |
|     - |  313 | `	{` |
|   ! 0 |  314 | `		SyBlobAppend(&sWorker, zName, (sxu32)nNameLen);` |
|   ! 0 |  315 | `		SyBlobAppend(&sWorker, "=", 1);` |
|   ! 0 |  316 | `		if( nValueLen > 0 ){` |
|   ! 0 |  317 | `			SyBlobAppend(&sWorker, zValue, (sxu32)nValueLen);` |
|   ! 0 |  318 | `		}` |
|     - |  319 | `	}` |
|     - |  320 | `	/* expires (requires SyTimeGetDay/SyTimeGetMonth from sxlib) */` |
|     - |  321 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   ! 0 |  322 | `	if( nArg >= 3 ){` |
|   ! 0 |  323 | `		sxi64 iExpires = ph7_value_to_int64(apArg[2]);` |
|   ! 0 |  324 | `		if( iExpires > 0 ){` |
|   ! 0 |  325 | `			time_t t = (time_t)iExpires;` |
|     - |  326 | `			struct tm tm_buf;` |
|     - |  327 | `			char zDate[64];` |
|   ! 0 |  328 | `			int tm_ok = 0;` |
|     - |  329 | `#ifdef __WINNT__` |
|   ! 0 |  330 | `			tm_ok = (gmtime_s(&tm_buf, &t) == 0);` |
|     - |  331 | `#else` |
|   ! 0 |  332 | `			tm_ok = (gmtime_r(&t, &tm_buf) != 0);` |
|     - |  333 | `#endif` |
|   ! 0 |  334 | `			if( tm_ok ){` |
|     - |  335 | `				/* Use locale-independent day/month names */` |
|   ! 0 |  336 | `				SyBufferFormat(zDate, sizeof(zDate),` |
|     - |  337 | `					"%s, %02d %s %04d %02d:%02d:%02d GMT",` |
|   ! 0 |  338 | `					SyTimeGetDay(tm_buf.tm_wday), tm_buf.tm_mday,` |
|   ! 0 |  339 | `					SyTimeGetMonth(tm_buf.tm_mon), 1900 + tm_buf.tm_year,` |
|   ! 0 |  340 | `					tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);` |
|   ! 0 |  341 | `				SyBlobAppend(&sWorker, "; expires=", 10);` |
|   ! 0 |  342 | `				SyBlobAppend(&sWorker, zDate, (sxu32)SyStrlen(zDate));` |
|   ! 0 |  343 | `			}` |
|   ! 0 |  344 | `		}` |
|   ! 0 |  345 | `	}` |
|     - |  346 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - |  347 | `	/* path */` |
|   ! 0 |  348 | `	if( nArg >= 4 ){` |
|   ! 0 |  349 | `		const char *zPath = ph7_value_to_string(apArg[3], 0);` |
|   ! 0 |  350 | `		if( zPath && zPath[0] ){` |
|   ! 0 |  351 | `			SyBlobAppend(&sWorker, "; path=", 7);` |
|   ! 0 |  352 | `			SyBlobAppend(&sWorker, zPath, (sxu32)SyStrlen(zPath));` |
|   ! 0 |  353 | `		}` |
|   ! 0 |  354 | `	}` |
|     - |  355 | `	/* domain */` |
|   ! 0 |  356 | `	if( nArg >= 5 ){` |
|   ! 0 |  357 | `		const char *zDomain = ph7_value_to_string(apArg[4], 0);` |
|   ! 0 |  358 | `		if( zDomain && zDomain[0] ){` |
|   ! 0 |  359 | `			SyBlobAppend(&sWorker, "; domain=", 9);` |
|   ! 0 |  360 | `			SyBlobAppend(&sWorker, zDomain, (sxu32)SyStrlen(zDomain));` |
|   ! 0 |  361 | `		}` |
|   ! 0 |  362 | `	}` |
|     - |  363 | `	/* secure */` |
|   ! 0 |  364 | `	if( nArg >= 6 && ph7_value_to_bool(apArg[5]) ){` |
|   ! 0 |  365 | `		SyBlobAppend(&sWorker, "; secure", 8);` |
|   ! 0 |  366 | `	}` |
|     - |  367 | `	/* httponly */` |
|   ! 0 |  368 | `	if( nArg >= 7 && ph7_value_to_bool(apArg[6]) ){` |
|   ! 0 |  369 | `		SyBlobAppend(&sWorker, "; httponly", 9);` |
|   ! 0 |  370 | `	}` |
|     - |  371 | `	/* Append as Set-Cookie header (never replace) */` |
|   ! 0 |  372 | `	VmAddResponseHeader(pVm, "Set-Cookie", 10,` |
|   ! 0 |  373 | `		(const char *)SyBlobData(&sWorker), SyBlobLength(&sWorker),` |
|     - |  374 |  |
|   ! 0 |  375 | `	SyBlobRelease(&sWorker);` |
|   ! 0 |  376 | `	ph7_result_bool(pCtx, 1);` |
|   ! 0 |  377 | `	return PH7_OK;` |
|   ! 0 |  378 |  |
|     - |  379 | `/*` |
|     - |  380 | ` * bool setcookie(string $name [, string $value [, int $expires [, string $path` |
|     - |  381 | ` *                [, string $domain [, bool $secure [, bool $httponly]]]]]])` |
|     - |  382 | ` */` |
|   ! 0 |  383 | `static int vm_builtin_setcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 |  384 |  |
|   ! 0 |  385 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 1 /* URL-encode */);` |
|   ! 0 |  386 |  |
|     - |  387 | `/*` |
|     - |  388 | ` * bool setrawcookie(string $name [, string $value [, ...]])` |
|     - |  389 | ` */` |
|   ! 0 |  390 | `static int vm_builtin_setrawcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 |  391 |  |
|   ! 0 |  392 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 0 /* no encoding */);` |
|   ! 0 |  393 |  |
|     - |  394 | `/*` |
|     - |  395 | ` * Register all HTTP response functions with the VM.` |
|     - |  396 | ` */` |
|  2940 |  397 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm)` |
|     5 |  398 |  |
|     - |  399 | `	static const ph7_builtin_func aFunc[] = {` |
|     - |  400 | `		{ "header",             vm_builtin_header             },` |
|     - |  401 | `		{ "header_remove",      vm_builtin_header_remove      },` |
|     - |  402 | `		{ "headers_sent",       vm_builtin_headers_sent       },` |
|     - |  403 | `		{ "headers_list",       vm_builtin_headers_list       },` |
|     - |  404 | `		{ "http_response_code", vm_builtin_http_response_code },` |
|     - |  405 | `		{ "setcookie",          vm_builtin_setcookie          },` |
|     - |  406 | `		{ "setrawcookie",       vm_builtin_setrawcookie       },` |
|     - |  407 | `	};` |
|     - |  408 | `	sxu32 n;` |
| 23525 |  409 | `	for( n = 0; n < SX_ARRAYSIZE(aFunc); n++ ){` |
| 20585 |  410 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 10295 |  411 | `	}` |
|  2945 |  412 |  |
|     - |  413 |  |
