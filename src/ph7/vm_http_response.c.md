# src/ph7/vm_http_response.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 319/383 lines (83.29%)

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
|    16 |   17 | `PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm)` |
|   ! 0 |   18 | `{` |
|     - |   19 | `	VmResponseHeader *aHdr;` |
|     - |   20 | `	sxu32 i, n;` |
|    16 |   21 | `	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    16 |   22 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|    46 |   23 | `	for( i = 0; i < n; i++ ){` |
|    30 |   24 | `		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);` |
|    30 |   25 | `		SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);` |
|    15 |   26 | `	}` |
|    16 |   27 | `	SySetReset(&pVm->aResponseHeaders);` |
|    16 |   28 | `}` |
|     - |   29 | `/*` |
|     - |   30 | ` * Remove all response headers matching the given name (case-insensitive).` |
|     - |   31 | ` */` |
|    40 |   32 | `static void VmRemoveHeaderByName(ph7_vm *pVm, const char *zName, sxu32 nName)` |
|   ! 0 |   33 | `{` |
|     - |   34 | `	sxu32 i, n;` |
|    40 |   35 | `	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    40 |   36 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|   100 |   37 | `	for( i = 0; i < n; ){` |
|    64 |   38 | `		if( aHdr[i].sName.nByte == nName &&` |
|     8 |   39 | `			SyStrnicmp(aHdr[i].sName.zString, zName, nName) == 0 ){` |
|     - |   40 | `			/* Free the duplicated strings */` |
|     2 |   41 | `			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sName.zString);` |
|     2 |   42 | `			SyMemBackendFree(&pVm->sAllocator, (void *)aHdr[i].sValue.zString);` |
|     2 |   43 | `			if( i < n - 1 ){` |
|   ! 0 |   44 | `				aHdr[i] = aHdr[n - 1];` |
|   ! 0 |   45 | `			}` |
|     2 |   46 | `			SySetPop(&pVm->aResponseHeaders);` |
|     2 |   47 | `			n--;` |
|     1 |   48 | `		}else{` |
|    58 |   49 | `			i++;` |
|     - |   50 | `		}` |
|   ! 0 |   51 | `	}` |
|    40 |   52 | `}` |
|     - |   53 | `/*` |
|     - |   54 | ` * Store a response header in the VM.` |
|     - |   55 | ` * If bReplace is TRUE, removes existing headers with the same name first.` |
|     - |   56 | ` */` |
|   160 |   57 | `static sxi32 VmAddResponseHeader(ph7_vm *pVm, const char *zName, sxu32 nName,` |
|     - |   58 | `								  const char *zValue, sxu32 nValue, int bReplace)` |
|     4 |   59 | `{` |
|     - |   60 | `	VmResponseHeader sHeader;` |
|     - |   61 | `	char *zNameDup, *zValueDup;` |
|   164 |   62 | `	if( bReplace ){` |
|    40 |   63 | `		VmRemoveHeaderByName(pVm, zName, nName);` |
|    20 |   64 | `	}` |
|     - |   65 | `	/* Duplicate name and value into VM allocator */` |
|   164 |   66 | `	zNameDup = SyMemBackendStrDup(&pVm->sAllocator, zName, nName);` |
|   164 |   67 | `	zValueDup = SyMemBackendStrDup(&pVm->sAllocator, zValue, nValue);` |
|   164 |   68 | `	if( zNameDup == 0 \|\| zValueDup == 0 ){` |
|   ! 0 |   69 | `		return SXERR_MEM;` |
|     - |   70 | `	}` |
|   164 |   71 | `	SyStringInitFromBuf(&sHeader.sName, zNameDup, nName);` |
|   164 |   72 | `	SyStringInitFromBuf(&sHeader.sValue, zValueDup, nValue);` |
|   164 |   73 | `	return SySetPut(&pVm->aResponseHeaders, (const void *)&sHeader);` |
|    84 |   74 | `}` |
|     - |   75 | `/*` |
|     - |   76 | ` * void header(string $header [, bool $replace = true [, int $response_code = 0]])` |
|     - |   77 | ` *   Send a raw HTTP header.` |
|     - |   78 | ` */` |
|    22 |   79 | `static int vm_builtin_header(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |   80 | `{` |
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
|    13 |  168 | `}` |
|     - |  169 | `/*` |
|     - |  170 | ` * void header_remove([string $name])` |
|     - |  171 | ` *   Remove a previously set header. If no name given, remove all.` |
|     - |  172 | ` */` |
|   ! 0 |  173 | `static int vm_builtin_header_remove(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 |  174 | `{` |
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
|   ! 0 |  191 | `}` |
|     - |  192 | `/*` |
|     - |  193 | ` * bool headers_sent()` |
|     - |  194 | ` *   Returns TRUE if headers have already been sent (output started).` |
|     - |  195 | ` */` |
|     8 |  196 | `static int vm_builtin_headers_sent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  197 | `{` |
|     4 |  198 | `	(void)nArg; (void)apArg;` |
|     9 |  199 | `	ph7_result_bool(pCtx, pCtx->pVm->bHeadersSent);` |
|     9 |  200 | `	return PH7_OK;` |
|     1 |  201 | `}` |
|     - |  202 | `/*` |
|     - |  203 | ` * array headers_list()` |
|     - |  204 | ` *   Returns a list of response headers as "Name: Value" strings.` |
|     - |  205 | ` */` |
|    12 |  206 | `static int vm_builtin_headers_list(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 |  207 | `{` |
|    15 |  208 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  209 | `	ph7_value *pArray;` |
|     - |  210 | `	ph7_value *pEntry;` |
|     - |  211 | `	VmResponseHeader *aHdr;` |
|     - |  212 | `	sxu32 i, n;` |
|     6 |  213 | `	(void)nArg; (void)apArg;` |
|    15 |  214 | `	pArray = ph7_context_new_array(pCtx);` |
|    15 |  215 | `	pEntry = ph7_context_new_scalar(pCtx);` |
|    15 |  216 | `	if( pArray == 0 \|\| pEntry == 0 ){` |
|   ! 0 |  217 | `		ph7_result_null(pCtx);` |
|   ! 0 |  218 | `		return PH7_OK;` |
|     - |  219 | `	}` |
|    15 |  220 | `	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    15 |  221 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|    25 |  222 | `	for( i = 0; i < n; i++ ){` |
|    10 |  223 | `		ph7_value_reset_string_cursor(pEntry);` |
|    15 |  224 | `		ph7_value_string_format(pEntry, "%.*s: %.*s",` |
|    10 |  225 | `			(int)aHdr[i].sName.nByte, aHdr[i].sName.zString,` |
|    10 |  226 | `			(int)aHdr[i].sValue.nByte, aHdr[i].sValue.zString);` |
|    10 |  227 | `		ph7_array_add_elem(pArray, 0, pEntry);` |
|     5 |  228 | `	}` |
|    15 |  229 | `	ph7_result_value(pCtx, pArray);` |
|    15 |  230 | `	return PH7_OK;` |
|     9 |  231 | `}` |
|     - |  232 | `/*` |
|     - |  233 | ` * int http_response_code([int $code])` |
|     - |  234 | ` *   Get or set the HTTP response status code.` |
|     - |  235 | ` */` |
|    16 |  236 | `static int vm_builtin_http_response_code(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  237 | `{` |
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
|     9 |  268 | `}` |
|     - |  269 | `/*` |
|     - |  270 | ` * The bytes a cookie NAME may not carry, and the ones a RAW value may not: php` |
|     - |  271 | ` * refuses them where they are written rather than emitting a header a proxy would` |
|     - |  272 | ` * read as two. A setcookie() VALUE is url-encoded and so has no such rule.` |
|     - |  273 | ` */` |
|     - |  274 | `#define VM_COOKIE_NAME_BAD  "=,; \t\r\n\013\014"` |
|     - |  275 | `#define VM_COOKIE_VALUE_BAD  ",; \t\r\n\013\014"` |
|     - |  276 |  |
|    42 |  277 | `static int VmCookieBadByte(const char *zVal,sxu32 nVal,const char *zBad,sxu32 nBad)` |
|     1 |  278 | `{` |
|     - |  279 | `	sxu32 i;` |
|   137 |  280 | `	for( i = 0 ; i < nVal ; i++ ){` |
|   103 |  281 | `		if( SyByteFind(zBad,nBad,zVal[i],0) == SXRET_OK ){` |
|     9 |  282 | `			return 1;` |
|     - |  283 | `		}` |
|    48 |  284 | `	}` |
|    35 |  285 | `	return 0;` |
|    22 |  286 | `}` |
|     - |  287 | `/*` |
|     - |  288 | ` * The options ARRAY php's third parameter has taken since 7.3 -- declared in` |
|     - |  289 | ` * aBuiltinSig[] here and read as an INT, so every documented` |
|     - |  290 | `` * `setcookie($n,$v,['expires'=>…,'samesite'=>'Lax'])` emitted a cookie with no`` |
|     - |  291 | ` * attributes at ALL: no expiry, no path, no SameSite. The key match is` |
|     - |  292 | ` * case-insensitive and an unknown key is a ValueError, so a typo cannot silently` |
|     - |  293 | ` * drop the attribute that makes the cookie safe.` |
|     - |  294 | ` */` |
|     - |  295 | `struct VmCookieOpts {` |
|     - |  296 | `	sxi64 iExpires;` |
|     - |  297 | `	/* COPIES, not the walk's own pointers: ph7_array_walk hands the callback a` |
|     - |  298 | ``	 * value it reuses for the next entry, so a `const char *` kept out of it is`` |
|     - |  299 | `	 * dangling by the time the header is built. */` |
|     - |  300 | `	SyBlob sPath, sDomain, sSame;` |
|     - |  301 | `	int bSecure, bHttpOnly, bPartitioned;` |
|     - |  302 | `	char zBadKey[64];` |
|     - |  303 | `};` |
|    14 |  304 | `static void VmCookieOptStr(SyBlob *pOut,ph7_value *pVal)` |
|     1 |  305 | `{` |
|    15 |  306 | `	int nVal = 0;` |
|    15 |  307 | `	const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|    15 |  308 | `	SyBlobReset(pOut);` |
|    15 |  309 | `	if( nVal > 0 ){` |
|    15 |  310 | `		SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|     7 |  311 | `	}` |
|    15 |  312 | `}` |
|    30 |  313 | `static int VmCookieOptionWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  314 | `{` |
|    31 |  315 | `	struct VmCookieOpts *pOpt = (struct VmCookieOpts *)pUserData;` |
|     - |  316 | `	const char *zKey;` |
|    31 |  317 | `	int nKey = 0;` |
|    31 |  318 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    31 |  319 | `	if( nKey == (int)sizeof("expires")-1 && SyStrnicmp(zKey,"expires",(sxu32)nKey) == 0 ){` |
|     5 |  320 | `		pOpt->iExpires = ph7_value_to_int64(pVal);` |
|    29 |  321 | `	}else if( nKey == (int)sizeof("path")-1 && SyStrnicmp(zKey,"path",(sxu32)nKey) == 0 ){` |
|     2 |  322 | `		VmCookieOptStr(&pOpt->sPath,pVal);` |
|    26 |  323 | `	}else if( nKey == (int)sizeof("domain")-1 && SyStrnicmp(zKey,"domain",(sxu32)nKey) == 0 ){` |
|     2 |  324 | `		VmCookieOptStr(&pOpt->sDomain,pVal);` |
|    24 |  325 | `	}else if( nKey == (int)sizeof("samesite")-1 && SyStrnicmp(zKey,"samesite",(sxu32)nKey) == 0 ){` |
|     7 |  326 | `		VmCookieOptStr(&pOpt->sSame,pVal);` |
|    20 |  327 | `	}else if( nKey == (int)sizeof("secure")-1 && SyStrnicmp(zKey,"secure",(sxu32)nKey) == 0 ){` |
|     7 |  328 | `		pOpt->bSecure = ph7_value_to_bool(pVal);` |
|    14 |  329 | `	}else if( nKey == (int)sizeof("httponly")-1 && SyStrnicmp(zKey,"httponly",(sxu32)nKey) == 0 ){` |
|     2 |  330 | `		pOpt->bHttpOnly = ph7_value_to_bool(pVal);` |
|    10 |  331 | `	}else if( nKey == (int)sizeof("partitioned")-1 && SyStrnicmp(zKey,"partitioned",(sxu32)nKey) == 0 ){` |
|     5 |  332 | `		pOpt->bPartitioned = ph7_value_to_bool(pVal);` |
|     3 |  333 | `	}else{` |
|     5 |  334 | `		sxu32 nCopy = (sxu32)nKey;` |
|     5 |  335 | `		if( nCopy > sizeof(pOpt->zBadKey)-1 ){` |
|   ! 0 |  336 | `			nCopy = sizeof(pOpt->zBadKey)-1;` |
|   ! 0 |  337 | `		}` |
|     5 |  338 | `		SyMemcpy(zKey,pOpt->zBadKey,nCopy);` |
|     5 |  339 | `		pOpt->zBadKey[nCopy] = 0;` |
|     5 |  340 | `		return SXERR_ABORT;   /* the caller reports which key it was */` |
|     - |  341 | `	}` |
|    27 |  342 | `	return PH7_OK;` |
|    16 |  343 | `}` |
|     - |  344 | `/*` |
|     - |  345 | ` * An HTTP-date: "Thu, 19 Nov 1981 08:52:00 GMT", with the three-letter day and` |
|     - |  346 | ` * month names an HTTP-date is defined in (locale-independent, from sxlib's own` |
|     - |  347 | ` * tables). Answers the byte count, or 0 for a time the C library will not break` |
|     - |  348 | ` * down.` |
|     - |  349 | ` */` |
|    18 |  350 | `PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf)` |
|     1 |  351 | `{` |
|     - |  352 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - |  353 | `	/* The day/month name tables live behind the same switch: a build without the` |
|     - |  354 | `	 * builtins has no date to print. */` |
|     - |  355 | `	(void)iWhen; (void)zBuf; (void)nBuf;` |
|     - |  356 | `	return 0;` |
|     - |  357 | `#else` |
|    19 |  358 | `	time_t t = (time_t)iWhen;` |
|     - |  359 | `	struct tm tm_buf;` |
|     - |  360 | `	int tm_ok;` |
|     - |  361 | `#ifdef __WINNT__` |
|     1 |  362 | `	tm_ok = (gmtime_s(&tm_buf,&t) == 0);` |
|     - |  363 | `#else` |
|    18 |  364 | `	tm_ok = (gmtime_r(&t,&tm_buf) != 0);` |
|     - |  365 | `#endif` |
|    19 |  366 | `	if( !tm_ok ){` |
|   ! 0 |  367 | `		return 0;` |
|     - |  368 | `	}` |
|    28 |  369 | `	return SyBufferFormat(zBuf,(sxu32)nBuf,"%.3s, %02d %.3s %04d %02d:%02d:%02d GMT",` |
|     9 |  370 | `		SyTimeGetDay(tm_buf.tm_wday),tm_buf.tm_mday,` |
|    18 |  371 | `		SyTimeGetMonth(tm_buf.tm_mon),1900 + tm_buf.tm_year,` |
|     9 |  372 | `		tm_buf.tm_hour,tm_buf.tm_min,tm_buf.tm_sec);` |
|     - |  373 | `#endif` |
|    10 |  374 | `}` |
|     - |  375 | `/* Queue a response header, replacing any of the same name. */` |
|    28 |  376 | `PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,` |
|     - |  377 | `	sxu32 nValue)` |
|   ! 0 |  378 | `{` |
|    28 |  379 | `	VmAddResponseHeader(pVm,zName,(sxu32)SyStrlen(zName),zValue,nValue,1);` |
|    28 |  380 | `}` |
|     - |  381 | `/*` |
|     - |  382 | ` * Drop any Set-Cookie already queued for this cookie NAME. php does this before it` |
|     - |  383 | ` * sends the session cookie (php_session_remove_cookie): a request that regenerates` |
|     - |  384 | ` * its id must not leave the OLD id in the reply beside the new one, and only the` |
|     - |  385 | ` * cookie of that name goes -- the header name is shared with every other cookie.` |
|     - |  386 | ` */` |
|   104 |  387 | `PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|     4 |  388 | `{` |
|   108 |  389 | `	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|   108 |  390 | `	sxu32 i,n = SySetUsed(&pVm->aResponseHeaders);` |
|   192 |  391 | `	for( i = 0 ; i < n ; ){` |
|    88 |  392 | `		const SyString *pVal = &aHdr[i].sValue;` |
|    84 |  393 | `		if( aHdr[i].sName.nByte == sizeof("Set-Cookie")-1` |
|    81 |  394 | `		 && SyStrnicmp(aHdr[i].sName.zString,"Set-Cookie",sizeof("Set-Cookie")-1) == 0` |
|    78 |  395 | `		 && pVal->nByte > nName` |
|    78 |  396 | `		 && SyMemcmp(pVal->zString,zName,nName) == 0` |
|    76 |  397 | `		 && pVal->zString[nName] == '=' ){` |
|    70 |  398 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sName.zString);` |
|    70 |  399 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sValue.zString);` |
|    70 |  400 | `			if( i < n - 1 ){` |
|     2 |  401 | `				aHdr[i] = aHdr[n - 1];` |
|     1 |  402 | `			}` |
|    70 |  403 | `			SySetPop(&pVm->aResponseHeaders);` |
|    70 |  404 | `			n--;` |
|    37 |  405 | `		}else{` |
|    19 |  406 | `			i++;` |
|     - |  407 | `		}` |
|     4 |  408 | `	}` |
|   108 |  409 | `}` |
|     - |  410 | `/*` |
|     - |  411 | ` * Build the Set-Cookie value php builds, and append it (never replace).` |
|     - |  412 | ` * PH7_PRIVATE because the session's own cookie is this same header with the` |
|     - |  413 | ` * session's parameters, not a second spelling of it.` |
|     - |  414 | ` */` |
|   118 |  415 | `PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|     - |  416 | `	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,` |
|     - |  417 | `	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,` |
|     - |  418 | `	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned)` |
|     4 |  419 | `{` |
|     - |  420 | `	SyBlob sWorker;` |
|   122 |  421 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     - |  422 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   122 |  423 | `	if( bEncode ){` |
|     - |  424 | ``		/* RAW url-encoding, php's: a space becomes %20 and `~` is left alone,`` |
|     - |  425 | `		 * which is not what urlencode() does — and a cookie value is read back by` |
|     - |  426 | `		 * a browser, so the two spellings are not interchangeable. */` |
|   120 |  427 | `		SyUriEncodeRaw(zName,nName,PH7_VmBlobConsumer,&sWorker);` |
|    62 |  428 | `	}else` |
|     - |  429 | `#else` |
|     - |  430 | `	(void)bEncode;` |
|     - |  431 | `#endif` |
|     - |  432 | `	{` |
|     2 |  433 | `		SyBlobAppend(&sWorker,zName,nName);` |
|     - |  434 | `	}` |
|   122 |  435 | `	SyBlobAppend(&sWorker,"=",1);` |
|   122 |  436 | `	if( nValue < 1 ){` |
|     - |  437 | ``		/* php replaces an EMPTY value with the literal `deleted` and dates the`` |
|     - |  438 | `		 * cookie to the epoch: that is what "unset this cookie" IS on the wire. */` |
|     2 |  439 | `		SyBlobAppend(&sWorker,"deleted",sizeof("deleted")-1);` |
|     2 |  440 | `		iExpires = 1;` |
|     1 |  441 | `	}else` |
|     - |  442 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   120 |  443 | `	if( bEncode ){` |
|   118 |  444 | `		SyUriEncodeRaw(zValue,nValue,PH7_VmBlobConsumer,&sWorker);` |
|    61 |  445 | `	}else` |
|     - |  446 | `#endif` |
|     - |  447 | `	{` |
|     2 |  448 | `		SyBlobAppend(&sWorker,zValue,nValue);` |
|     - |  449 | `	}` |
|     - |  450 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   122 |  451 | `	if( iExpires > 0 ){` |
|     - |  452 | `		char zDate[80];` |
|    11 |  453 | `		int nDate = PH7_VmHttpDate(iExpires,zDate,(int)sizeof(zDate));` |
|    11 |  454 | `		if( nDate > 0 ){` |
|    11 |  455 | `			sxi64 iMaxAge = iExpires - (sxi64)time(0);` |
|     - |  456 | `			char zTail[64];` |
|    11 |  457 | `			SyBlobAppend(&sWorker,"; expires=",sizeof("; expires=")-1);` |
|    11 |  458 | `			SyBlobAppend(&sWorker,zDate,(sxu32)nDate);` |
|    21 |  459 | `			nDate = SyBufferFormat(zTail,sizeof(zTail),"; Max-Age=%qd",` |
|    10 |  460 | `				iMaxAge < 0 ? (sxi64)0 : iMaxAge);` |
|    11 |  461 | `			SyBlobAppend(&sWorker,zTail,(sxu32)nDate);` |
|     5 |  462 | `		}` |
|     5 |  463 | `	}` |
|     - |  464 | `#else` |
|     - |  465 | `	(void)iExpires;` |
|     - |  466 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|   122 |  467 | `	if( nPath > 0 ){` |
|   112 |  468 | `		SyBlobAppend(&sWorker,"; path=",sizeof("; path=")-1);` |
|   112 |  469 | `		SyBlobAppend(&sWorker,zPath,nPath);` |
|    54 |  470 | `	}` |
|   122 |  471 | `	if( nDomain > 0 ){` |
|    11 |  472 | `		SyBlobAppend(&sWorker,"; domain=",sizeof("; domain=")-1);` |
|    11 |  473 | `		SyBlobAppend(&sWorker,zDomain,nDomain);` |
|     5 |  474 | `	}` |
|   122 |  475 | `	if( bSecure ){` |
|    13 |  476 | `		SyBlobAppend(&sWorker,"; secure",sizeof("; secure")-1);` |
|     6 |  477 | `	}` |
|   122 |  478 | `	if( bHttpOnly ){` |
|    11 |  479 | `		SyBlobAppend(&sWorker,"; HttpOnly",sizeof("; HttpOnly")-1);` |
|     5 |  480 | `	}` |
|   122 |  481 | `	if( nSame > 0 ){` |
|    11 |  482 | `		SyBlobAppend(&sWorker,"; SameSite=",sizeof("; SameSite=")-1);` |
|    11 |  483 | `		SyBlobAppend(&sWorker,zSame,nSame);` |
|     5 |  484 | `	}` |
|   122 |  485 | `	if( bPartitioned ){` |
|     5 |  486 | `		SyBlobAppend(&sWorker,"; Partitioned",sizeof("; Partitioned")-1);` |
|     2 |  487 | `	}` |
|   181 |  488 | `	VmAddResponseHeader(pVm,"Set-Cookie",10,` |
|   118 |  489 | `		(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),` |
|     - |  490 | `		0 /* bReplace = false */);` |
|   122 |  491 | `	SyBlobRelease(&sWorker);` |
|   122 |  492 | `}` |
|     - |  493 | `/*` |
|     - |  494 | ` * Internal helper for setcookie/setrawcookie.` |
|     - |  495 | ` */` |
|    38 |  496 | `static int VmSetCookieImpl(ph7_context *pCtx, int nArg, ph7_value **apArg, int bEncode)` |
|     1 |  497 | `{` |
|    39 |  498 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  499 | `	const char *zName, *zValue;` |
|    39 |  500 | `	const char *zFunc = bEncode ? "setcookie" : "setrawcookie";` |
|    39 |  501 | `	int nNameLen, nValueLen = 0, bBadOpt = 0;` |
|     - |  502 | `	struct VmCookieOpts sOpt;` |
|    39 |  503 | `	if( nArg < 1 ){` |
|   ! 0 |  504 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  505 | `		return PH7_OK;` |
|     - |  506 | `	}` |
|    39 |  507 | `	zName = ph7_value_to_string(apArg[0], &nNameLen);` |
|    39 |  508 | `	if( nNameLen < 1 ){` |
|     4 |  509 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     1 |  510 | `			"%s(): Argument #1 ($name) must not be empty",zFunc);` |
|     - |  511 | `	}` |
|    37 |  512 | `	if( VmCookieBadByte(zName,(sxu32)nNameLen,` |
|     - |  513 | `		VM_COOKIE_NAME_BAD,sizeof(VM_COOKIE_NAME_BAD)-1) ){` |
|     7 |  514 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  515 | `			"%s(): Argument #1 ($name) cannot contain \"=\", \",\", \";\","` |
|     2 |  516 | `			" \" \", \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);` |
|     - |  517 | `	}` |
|    33 |  518 | `	if( nArg >= 2 ){` |
|    33 |  519 | `		zValue = ph7_value_to_string(apArg[1], &nValueLen);` |
|    17 |  520 | `	}else{` |
|   ! 0 |  521 | `		zValue = "";` |
|     - |  522 | `	}` |
|    33 |  523 | `	if( !bEncode && VmCookieBadByte(zValue,(sxu32)nValueLen,` |
|     - |  524 | `		VM_COOKIE_VALUE_BAD,sizeof(VM_COOKIE_VALUE_BAD)-1) ){` |
|     - |  525 | `		/* Only the RAW value reaches the wire unchanged, so only it is screened;` |
|     - |  526 | `		 * setcookie() url-encodes and can carry anything. */` |
|     7 |  527 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  528 | `			"%s(): Argument #2 ($value) cannot contain \",\", \";\", \" \","` |
|     2 |  529 | `			" \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);` |
|     - |  530 | `	}` |
|    29 |  531 | `	SyZero(&sOpt,sizeof(sOpt));` |
|    29 |  532 | `	SyBlobInit(&sOpt.sPath,&pVm->sAllocator);` |
|    29 |  533 | `	SyBlobInit(&sOpt.sDomain,&pVm->sAllocator);` |
|    29 |  534 | `	SyBlobInit(&sOpt.sSame,&pVm->sAllocator);` |
|    29 |  535 | `	if( nArg >= 3 && ph7_value_is_array(apArg[2]) ){` |
|    15 |  536 | `		bBadOpt = ph7_array_walk(apArg[2],VmCookieOptionWalker,&sOpt) != PH7_OK;` |
|    22 |  537 | `	}else if( nArg >= 3 ){` |
|     2 |  538 | `		sOpt.iExpires = ph7_value_to_int64(apArg[2]);` |
|     2 |  539 | `		if( nArg >= 4 ){` |
|     2 |  540 | `			VmCookieOptStr(&sOpt.sPath,apArg[3]);` |
|     1 |  541 | `		}` |
|     2 |  542 | `		if( nArg >= 5 ){` |
|     2 |  543 | `			VmCookieOptStr(&sOpt.sDomain,apArg[4]);` |
|     1 |  544 | `		}` |
|     2 |  545 | `		if( nArg >= 6 ){` |
|     2 |  546 | `			sOpt.bSecure = ph7_value_to_bool(apArg[5]);` |
|     1 |  547 | `		}` |
|     2 |  548 | `		if( nArg >= 7 ){` |
|     2 |  549 | `			sOpt.bHttpOnly = ph7_value_to_bool(apArg[6]);` |
|     1 |  550 | `		}` |
|     1 |  551 | `	}` |
|    29 |  552 | `	if( !bBadOpt ){` |
|     - |  553 | `		/* php's CLI SAPI takes the header and answers TRUE even though nothing will` |
|     - |  554 | `		 * ever print it; only a real header ALREADY sent is a refusal. */` |
|    25 |  555 | `		if( pVm->bHeadersSent ){` |
|   ! 0 |  556 | `			ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  557 | `				"Cannot modify header information - headers already sent");` |
|   ! 0 |  558 | `			ph7_result_bool(pCtx, 0);` |
|   ! 0 |  559 | `		}else{` |
|    25 |  560 | `			if( pVm->bHttpContext ){` |
|    21 |  561 | `				PH7_VmEmitCookie(pVm,zName,(sxu32)nNameLen,zValue,(sxu32)nValueLen,bEncode,` |
|     7 |  562 | `					sOpt.iExpires,` |
|    14 |  563 | `					(const char *)SyBlobData(&sOpt.sPath),SyBlobLength(&sOpt.sPath),` |
|    14 |  564 | `					(const char *)SyBlobData(&sOpt.sDomain),SyBlobLength(&sOpt.sDomain),` |
|     7 |  565 | `					sOpt.bSecure,sOpt.bHttpOnly,` |
|    14 |  566 | `					(const char *)SyBlobData(&sOpt.sSame),SyBlobLength(&sOpt.sSame),` |
|     7 |  567 | `					sOpt.bPartitioned);` |
|     7 |  568 | `			}` |
|    25 |  569 | `			ph7_result_bool(pCtx, 1);` |
|     - |  570 | `		}` |
|    12 |  571 | `	}` |
|    29 |  572 | `	SyBlobRelease(&sOpt.sPath);` |
|    29 |  573 | `	SyBlobRelease(&sOpt.sDomain);` |
|    29 |  574 | `	SyBlobRelease(&sOpt.sSame);` |
|    29 |  575 | `	if( bBadOpt ){` |
|     7 |  576 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 |  577 | `			"%s(): option \"%s\" is invalid",zFunc,sOpt.zBadKey);` |
|     - |  578 | `	}` |
|    25 |  579 | `	return PH7_OK;` |
|    20 |  580 | `}` |
|     - |  581 | `/*` |
|     - |  582 | ` * bool setcookie(string $name [, string $value [, int $expires [, string $path` |
|     - |  583 | ` *                [, string $domain [, bool $secure [, bool $httponly]]]]]])` |
|     - |  584 | ` */` |
|    32 |  585 | `static int vm_builtin_setcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  586 | `{` |
|    33 |  587 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 1 /* URL-encode */);` |
|     1 |  588 | `}` |
|     - |  589 | `/*` |
|     - |  590 | ` * bool setrawcookie(string $name [, string $value [, ...]])` |
|     - |  591 | ` */` |
|     6 |  592 | `static int vm_builtin_setrawcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  593 | `{` |
|     7 |  594 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 0 /* no encoding */);` |
|     1 |  595 | `}` |
|     - |  596 | `/*` |
|     - |  597 | ` * Register all HTTP response functions with the VM.` |
|     - |  598 | ` */` |
|  4552 |  599 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm)` |
|     5 |  600 | `{` |
|     - |  601 | `	static const ph7_builtin_func aFunc[] = {` |
|     - |  602 | `		{ "header",             vm_builtin_header             },` |
|     - |  603 | `		{ "header_remove",      vm_builtin_header_remove      },` |
|     - |  604 | `		{ "headers_sent",       vm_builtin_headers_sent       },` |
|     - |  605 | `		{ "headers_list",       vm_builtin_headers_list       },` |
|     - |  606 | `		{ "http_response_code", vm_builtin_http_response_code },` |
|     - |  607 | `		{ "setcookie",          vm_builtin_setcookie          },` |
|     - |  608 | `		{ "setrawcookie",       vm_builtin_setrawcookie       },` |
|     - |  609 | `	};` |
|     - |  610 | `	sxu32 n;` |
| 36421 |  611 | `	for( n = 0; n < SX_ARRAYSIZE(aFunc); n++ ){` |
| 31869 |  612 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 15937 |  613 | `	}` |
|  4557 |  614 | `}` |
|     - |  615 |  |
