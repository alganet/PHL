# src/ph7/vm_http_response.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 398/450 lines (88.44%)

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
|   162 |   57 | `static sxi32 VmAddResponseHeader(ph7_vm *pVm, const char *zName, sxu32 nName,` |
|     - |   58 | `								  const char *zValue, sxu32 nValue, int bReplace)` |
|     4 |   59 | `{` |
|     - |   60 | `	VmResponseHeader sHeader;` |
|     - |   61 | `	char *zNameDup, *zValueDup;` |
|   166 |   62 | `	if( bReplace ){` |
|    40 |   63 | `		VmRemoveHeaderByName(pVm, zName, nName);` |
|    20 |   64 | `	}` |
|     - |   65 | `	/* Duplicate name and value into VM allocator */` |
|   166 |   66 | `	zNameDup = SyMemBackendStrDup(&pVm->sAllocator, zName, nName);` |
|   166 |   67 | `	zValueDup = SyMemBackendStrDup(&pVm->sAllocator, zValue, nValue);` |
|   166 |   68 | `	if( zNameDup == 0 \|\| zValueDup == 0 ){` |
|   ! 0 |   69 | `		return SXERR_MEM;` |
|     - |   70 | `	}` |
|   166 |   71 | `	SyStringInitFromBuf(&sHeader.sName, zNameDup, nName);` |
|   166 |   72 | `	SyStringInitFromBuf(&sHeader.sValue, zValueDup, nValue);` |
|   166 |   73 | `	return SySetPut(&pVm->aResponseHeaders, (const void *)&sHeader);` |
|    85 |   74 | `}` |
|     - |   75 | `/*` |
|     - |   76 | ` * The same store, for an extension that sets a header of its own rather than` |
|     - |   77 | ` * carrying out a script's header() call: ob_gzhandler() announces the` |
|     - |   78 | ` * Content-Encoding it just applied. Always REPLACES, which is what php's own` |
|     - |   79 | ` * sapi_add_header does for these two.` |
|     - |   80 | ` */` |
|   ! 0 |   81 | `PH7_PRIVATE void PH7_VmAddResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue)` |
|   ! 0 |   82 | `{` |
|   ! 0 |   83 | `	VmAddResponseHeader(pVm,zName,(sxu32)SyStrlen(zName),` |
|   ! 0 |   84 | `		zValue,(sxu32)SyStrlen(zValue),TRUE);` |
|   ! 0 |   85 | `}` |
|     - |   86 | `/*` |
|     - |   87 | ` * php's headers-already-sent warning, in the two shapes php words it: the` |
|     - |   88 | ` * header family's, which carries NO function prefix and hangs the origin off` |
|     - |   89 | ` * the word "by", and http_response_code()'s, which is prefixed and does not.` |
|     - |   90 | ` * Both name WHERE the response body began; with no origin recorded (nothing` |
|     - |   91 | ` * emitted through a real consumer) the clause is left off entirely.` |
|     - |   92 | ` */` |
|    10 |   93 | `static void VmHeadersAlreadySent(ph7_context *pCtx,int bResponseCode)` |
|     2 |   94 | `{` |
|    12 |   95 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   96 | `	SyBlob sMsg;` |
|     - |   97 | `	SyString sFile;` |
|    12 |   98 | `	sxu32 nLine = 0;` |
|    12 |   99 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    12 |  100 | `	if( bResponseCode ){` |
|     3 |  101 | `		SyBlobAppend(&sMsg,"http_response_code(): Cannot set response code - headers already sent",` |
|     - |  102 | `			sizeof("http_response_code(): Cannot set response code - headers already sent")-1);` |
|     2 |  103 | `	}else{` |
|     9 |  104 | `		SyBlobAppend(&sMsg,"Cannot modify header information - headers already sent",` |
|     - |  105 | `			sizeof("Cannot modify header information - headers already sent")-1);` |
|     - |  106 | `	}` |
|    12 |  107 | `	if( PH7_VmOutputOrigin(pVm,&sFile,&nLine) ){` |
|     - |  108 | `		char zTail[64];` |
|    12 |  109 | `		if( !bResponseCode ){` |
|     9 |  110 | `			SyBlobAppend(&sMsg," by",3);` |
|     4 |  111 | `		}` |
|    12 |  112 | `		SyBlobAppend(&sMsg," (output started at ",sizeof(" (output started at ")-1);` |
|    12 |  113 | `		SyBlobAppend(&sMsg,sFile.zString,sFile.nByte);` |
|    12 |  114 | `		SyBufferFormat(zTail,sizeof(zTail),":%u)",nLine);` |
|    12 |  115 | `		SyBlobAppend(&sMsg,zTail,(sxu32)SyStrlen(zTail));` |
|     5 |  116 | `	}` |
|    12 |  117 | `	SyBlobNullAppend(&sMsg);` |
|    12 |  118 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    12 |  119 | `	SyBlobRelease(&sMsg);` |
|    12 |  120 | `}` |
|     - |  121 | `/*` |
|     - |  122 | ` * void header(string $header [, bool $replace = true [, int $response_code = 0]])` |
|     - |  123 | ` *   Send a raw HTTP header.` |
|     - |  124 | ` */` |
|    26 |  125 | `static int vm_builtin_header(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 |  126 | `{` |
|    29 |  127 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  128 | `	const char *zHeader;` |
|     - |  129 | `	int nLen;` |
|    29 |  130 | `	int bReplace = 1;` |
|    29 |  131 | `	int iCode = 0;` |
|     - |  132 | `	const char *zColon;` |
|    29 |  133 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|   ! 0 |  134 | `		return PH7_OK;` |
|     - |  135 | `	}` |
|     - |  136 | `	/* php raises the headers-sent refusal in EVERY SAPI, the CLI included --` |
|     - |  137 | `	 * its own header handler asks whether the response has begun before it` |
|     - |  138 | `	 * asks whether anything will ever print the header. Only once that is` |
|     - |  139 | `	 * settled does a build with no HTTP context stop caring. */` |
|    29 |  140 | `	if( pVm->bHeadersSent ){` |
|     3 |  141 | `		VmHeadersAlreadySent(pCtx,0);` |
|     3 |  142 | `		return PH7_OK;` |
|     - |  143 | `	}` |
|    27 |  144 | `	if( !pVm->bHttpContext ){` |
|     - |  145 | `		/* CLI: php takes the header and prints nothing. */` |
|    11 |  146 | `		return PH7_OK;` |
|     - |  147 | `	}` |
|    16 |  148 | `	zHeader = ph7_value_to_string(apArg[0], &nLen);` |
|    16 |  149 | `	if( nLen < 1 ){` |
|   ! 0 |  150 | `		return PH7_OK;` |
|     - |  151 | `	}` |
|     - |  152 | `	/* Reject headers containing CR or LF (prevents response splitting) */` |
|     - |  153 | `	{` |
|     - |  154 | `		int k;` |
|   254 |  155 | `		for( k = 0; k < nLen; k++ ){` |
|   240 |  156 | `			if( zHeader[k] == '\r' \|\| zHeader[k] == '\n' ){` |
|     2 |  157 | `				ph7_context_throw_error(pCtx, PH7_CTX_WARNING,` |
|     - |  158 | `					"Header may not contain more than a single header, new line detected");` |
|     2 |  159 | `				return PH7_OK;` |
|     - |  160 | `			}` |
|   119 |  161 | `		}` |
|     - |  162 | `	}` |
|    14 |  163 | `	if( nArg >= 2 ){` |
|     2 |  164 | `		bReplace = ph7_value_to_bool(apArg[1]);` |
|     1 |  165 | `	}` |
|    14 |  166 | `	if( nArg >= 3 ){` |
|   ! 0 |  167 | `		iCode = ph7_value_to_int(apArg[2]);` |
|   ! 0 |  168 | `		if( iCode >= 100 && iCode <= 599 ){` |
|   ! 0 |  169 | `			pVm->iResponseStatus = iCode;` |
|   ! 0 |  170 | `		}` |
|   ! 0 |  171 | `	}` |
|     - |  172 | `	/* Check for HTTP/ status line */` |
|    14 |  173 | `	if( nLen >= 5 && SyStrnicmp(zHeader, "HTTP/", 5) == 0 ){` |
|     - |  174 | `		/* e.g. "HTTP/1.1 404 Not Found" — extract status code */` |
|   ! 0 |  175 | `		const char *z = zHeader + 5;` |
|   ! 0 |  176 | `		const char *zEnd = zHeader + nLen;` |
|   ! 0 |  177 | `		int iParsed = 0;` |
|     - |  178 | `		/* Skip version */` |
|   ! 0 |  179 | `		while( z < zEnd && *z != ' ' ) z++;` |
|   ! 0 |  180 | `		while( z < zEnd && *z == ' ' ) z++;` |
|   ! 0 |  181 | `		while( z < zEnd && *z >= '0' && *z <= '9' ){` |
|   ! 0 |  182 | `			iParsed = iParsed * 10 + (*z - '0');` |
|   ! 0 |  183 | `			z++;` |
|   ! 0 |  184 | `		}` |
|   ! 0 |  185 | `		if( iParsed >= 100 && iParsed <= 599 ){` |
|   ! 0 |  186 | `			pVm->iResponseStatus = iParsed;` |
|   ! 0 |  187 | `		}` |
|   ! 0 |  188 | `		return PH7_OK;` |
|     - |  189 | `	}` |
|     - |  190 | `	/* Split on first ':' */` |
|     - |  191 | `	{` |
|     - |  192 | `		sxu32 nPos;` |
|    14 |  193 | `		if( SyByteFind(zHeader, (sxu32)nLen, ':', &nPos) == SXRET_OK ){` |
|    14 |  194 | `			zColon = zHeader + nPos;` |
|     7 |  195 | `		}else{` |
|   ! 0 |  196 | `			zColon = 0;` |
|     - |  197 | `		}` |
|     - |  198 | `	}` |
|    14 |  199 | `	if( zColon == 0 ){` |
|     - |  200 | `		/* No colon found — invalid header, ignore */` |
|   ! 0 |  201 | `		return PH7_OK;` |
|     - |  202 | `	}` |
|     - |  203 | `	{` |
|    14 |  204 | `		sxu32 nName = (sxu32)(zColon - zHeader);` |
|    14 |  205 | `		const char *zValue = zColon + 1;` |
|     - |  206 | `		sxu32 nValue;` |
|     - |  207 | `		/* Skip leading whitespace in value */` |
|    28 |  208 | `		while( *zValue == ' ' \|\| *zValue == '\t' ) zValue++;` |
|    14 |  209 | `		nValue = (sxu32)(nLen - (int)(zValue - zHeader));` |
|     - |  210 | `		/* Auto-set 302 for Location header if status is still 200 */` |
|    14 |  211 | `		if( nName == 8 && SyStrnicmp(zHeader, "Location", 8) == 0 && pVm->iResponseStatus == 200 ){` |
|   ! 0 |  212 | `			pVm->iResponseStatus = 302;` |
|   ! 0 |  213 | `		}` |
|    14 |  214 | `		VmAddResponseHeader(pVm, zHeader, nName, zValue, nValue, bReplace);` |
|     - |  215 | `	}` |
|    14 |  216 | `	return PH7_OK;` |
|    16 |  217 | `}` |
|     - |  218 | `/*` |
|     - |  219 | ` * void header_remove([string $name])` |
|     - |  220 | ` *   Remove a previously set header. If no name given, remove all.` |
|     - |  221 | ` */` |
|     2 |  222 | `static int vm_builtin_header_remove(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  223 | `{` |
|     3 |  224 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  225 | `	/* The refusal comes first here too, for the reason header() gives. */` |
|     3 |  226 | `	if( pVm->bHeadersSent ){` |
|     3 |  227 | `		VmHeadersAlreadySent(pCtx,0);` |
|     3 |  228 | `		return PH7_OK;` |
|     - |  229 | `	}` |
|   ! 0 |  230 | `	if( !pVm->bHttpContext ){` |
|   ! 0 |  231 | `		return PH7_OK;` |
|     - |  232 | `	}` |
|   ! 0 |  233 | `	if( nArg < 1 ){` |
|     - |  234 | `		/* Remove all headers */` |
|   ! 0 |  235 | `		PH7_VmReleaseResponseHeaders(pVm);` |
|   ! 0 |  236 | `	}else{` |
|   ! 0 |  237 | `		const char *zName = ph7_value_to_string(apArg[0], 0);` |
|   ! 0 |  238 | `		VmRemoveHeaderByName(pVm, zName, (sxu32)SyStrlen(zName));` |
|     - |  239 | `	}` |
|   ! 0 |  240 | `	return PH7_OK;` |
|     2 |  241 | `}` |
|     - |  242 | `/*` |
|     - |  243 | ` * bool headers_sent([&$filename [, &$line]])` |
|     - |  244 | ` *   Whether the response body has begun -- and, through its two by-ref` |
|     - |  245 | ` *   out-params, WHERE it began. php writes both whatever the answer is: the` |
|     - |  246 | ` *   empty string and 0 while nothing has been emitted, so a caller that reads` |
|     - |  247 | ` *   them never sees its own previous value. This engine declared neither, so` |
|     - |  248 | ` *   both variables kept whatever they held.` |
|     - |  249 | ` */` |
|    14 |  250 | `static int vm_builtin_headers_sent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  251 | `{` |
|    16 |  252 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  253 | `	SyString sFile;` |
|    16 |  254 | `	sxu32 nLine = 0;` |
|    16 |  255 | `	PH7_VmOutputOrigin(pVm,&sFile,&nLine);` |
|    16 |  256 | `	if( nArg > 0 ){` |
|     7 |  257 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     7 |  258 | `		if( pVal ){` |
|     7 |  259 | `			ph7_value_string(pVal,sFile.nByte > 0 ? sFile.zString : "",(int)sFile.nByte);` |
|     7 |  260 | `			PH7_VmStoreArgByRef(pVm,apArg[0],pVal);` |
|     3 |  261 | `		}` |
|     3 |  262 | `	}` |
|    16 |  263 | `	if( nArg > 1 ){` |
|     7 |  264 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     7 |  265 | `		if( pVal ){` |
|     7 |  266 | `			ph7_value_int64(pVal,(ph7_int64)nLine);` |
|     7 |  267 | `			PH7_VmStoreArgByRef(pVm,apArg[1],pVal);` |
|     3 |  268 | `		}` |
|     3 |  269 | `	}` |
|    16 |  270 | `	ph7_result_bool(pCtx, pVm->bHeadersSent);` |
|    16 |  271 | `	return PH7_OK;` |
|     2 |  272 | `}` |
|     - |  273 | `/*` |
|     - |  274 | ` * array headers_list()` |
|     - |  275 | ` *   Returns a list of response headers as "Name: Value" strings.` |
|     - |  276 | ` */` |
|    12 |  277 | `static int vm_builtin_headers_list(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 |  278 | `{` |
|    15 |  279 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  280 | `	ph7_value *pArray;` |
|     - |  281 | `	ph7_value *pEntry;` |
|     - |  282 | `	VmResponseHeader *aHdr;` |
|     - |  283 | `	sxu32 i, n;` |
|     6 |  284 | `	(void)nArg; (void)apArg;` |
|    15 |  285 | `	pArray = ph7_context_new_array(pCtx);` |
|    15 |  286 | `	pEntry = ph7_context_new_scalar(pCtx);` |
|    15 |  287 | `	if( pArray == 0 \|\| pEntry == 0 ){` |
|   ! 0 |  288 | `		ph7_result_null(pCtx);` |
|   ! 0 |  289 | `		return PH7_OK;` |
|     - |  290 | `	}` |
|    15 |  291 | `	aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|    15 |  292 | `	n = SySetUsed(&pVm->aResponseHeaders);` |
|    25 |  293 | `	for( i = 0; i < n; i++ ){` |
|    10 |  294 | `		ph7_value_reset_string_cursor(pEntry);` |
|    15 |  295 | `		ph7_value_string_format(pEntry, "%.*s: %.*s",` |
|    10 |  296 | `			(int)aHdr[i].sName.nByte, aHdr[i].sName.zString,` |
|    10 |  297 | `			(int)aHdr[i].sValue.nByte, aHdr[i].sValue.zString);` |
|    10 |  298 | `		ph7_array_add_elem(pArray, 0, pEntry);` |
|     5 |  299 | `	}` |
|    15 |  300 | `	ph7_result_value(pCtx, pArray);` |
|    15 |  301 | `	return PH7_OK;` |
|     9 |  302 | `}` |
|     - |  303 | `/*` |
|     - |  304 | ` * int\|bool http_response_code([int $response_code = 0])` |
|     - |  305 | ` *   Get or set the HTTP response status code.` |
|     - |  306 | ` *` |
|     - |  307 | ` * php keeps ONE code, in every SAPI. Zero means "nothing set": a CLI script` |
|     - |  308 | ` * that has not set one reads FALSE, and the first set answers TRUE rather than` |
|     - |  309 | ` * a previous code -- while a request-driven run starts at 200 and every set` |
|     - |  310 | ` * answers the code it replaced. A zero ARGUMENT is a read and not a write, and` |
|     - |  311 | ` * php range-checks nothing at all, so 99 and -5 are stored as written. The one` |
|     - |  312 | ` * refusal is a response that has already begun.` |
|     - |  313 | ` *` |
|     - |  314 | ` * This engine used to answer FALSE for every call outside the server and warn` |
|     - |  315 | ` * on every set, so a CLI script could neither set a code nor read one back,` |
|     - |  316 | ` * and it clamped 100..599 in the server where php clamps nothing.` |
|     - |  317 | ` */` |
|    36 |  318 | `static int vm_builtin_http_response_code(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 |  319 | `{` |
|    39 |  320 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  321 | `	ph7_int64 iCode = 0;` |
|    39 |  322 | `	if( nArg >= 1 ){` |
|    18 |  323 | `		iCode = ph7_value_to_int64(apArg[0]);` |
|     8 |  324 | `	}` |
|    39 |  325 | `	if( iCode == 0 ){` |
|     - |  326 | `		/* A read: the standing code, or FALSE when there is none. */` |
|    25 |  327 | `		if( pVm->iResponseStatus ){` |
|    17 |  328 | `			ph7_result_int(pCtx, pVm->iResponseStatus);` |
|     9 |  329 | `		}else{` |
|     9 |  330 | `			ph7_result_bool(pCtx, 0);` |
|     - |  331 | `		}` |
|    25 |  332 | `		return PH7_OK;` |
|     - |  333 | `	}` |
|    16 |  334 | `	if( pVm->bHeadersSent ){` |
|     3 |  335 | `		VmHeadersAlreadySent(pCtx,1);` |
|     3 |  336 | `		ph7_result_bool(pCtx, 0);` |
|     3 |  337 | `		return PH7_OK;` |
|     - |  338 | `	}` |
|    13 |  339 | `	if( pVm->iResponseStatus ){` |
|    11 |  340 | `		ph7_result_int(pCtx, pVm->iResponseStatus);` |
|     6 |  341 | `	}else{` |
|     3 |  342 | `		ph7_result_bool(pCtx, 1);` |
|     - |  343 | `	}` |
|    13 |  344 | `	pVm->iResponseStatus = (int)iCode;` |
|    13 |  345 | `	return PH7_OK;` |
|    21 |  346 | `}` |
|     - |  347 | `/*` |
|     - |  348 | ` * int connection_status()` |
|     - |  349 | ` *   The state of the connection to the client: NORMAL, or one of the two ways` |
|     - |  350 | ` *   php ends a request early. A CLI run has no client to lose and no time` |
|     - |  351 | ` *   limit that ends anything, so php's own answer there is NORMAL for the` |
|     - |  352 | ` *   whole run -- which is what makes the constant, not the number, the thing` |
|     - |  353 | ` *   a program should compare against.` |
|     - |  354 | ` */` |
|     4 |  355 | `static int vm_builtin_connection_status(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  356 | `{` |
|     2 |  357 | `	(void)nArg; (void)apArg;` |
|     5 |  358 | `	ph7_result_int(pCtx, 0 /* CONNECTION_NORMAL */);` |
|     5 |  359 | `	return PH7_OK;` |
|     1 |  360 | `}` |
|     - |  361 | `/*` |
|     - |  362 | ` * int connection_aborted()` |
|     - |  363 | ` *   Whether the client has gone away. Zero for the same reason.` |
|     - |  364 | ` */` |
|     2 |  365 | `static int vm_builtin_connection_aborted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  366 | `{` |
|     1 |  367 | `	(void)nArg; (void)apArg;` |
|     3 |  368 | `	ph7_result_int(pCtx, 0);` |
|     3 |  369 | `	return PH7_OK;` |
|     1 |  370 | `}` |
|     - |  371 | `/*` |
|     - |  372 | ` * int ignore_user_abort([?bool $enable = null])` |
|     - |  373 | `` *   Read or write the `ignore_user_abort` directive, answering the value it`` |
|     - |  374 | ``  *   held BEFORE the call -- which is what makes the setting restorable. `null` `` |
|     - |  375 | ` *   is the value that only reads. The directive is a real one (ini_get() and` |
|     - |  376 | ` *   ini_set() reach the same slot), so a program that saves and restores it` |
|     - |  377 | ` *   through either door sees the same number.` |
|     - |  378 | ` */` |
|    12 |  379 | `static int vm_builtin_ignore_user_abort(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  380 | `{` |
|    13 |  381 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  382 | `	sxi64 iOld = PH7_VmIniGetInt(pVm,"ignore_user_abort",0);` |
|    13 |  383 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     7 |  384 | `		int bOn = ph7_value_to_bool(apArg[0]);` |
|    10 |  385 | `		PH7_VmIniSet(pVm,"ignore_user_abort",sizeof("ignore_user_abort")-1,` |
|     3 |  386 | `			bOn ? "1" : "0",1,"ignore_user_abort()");` |
|     3 |  387 | `	}` |
|    13 |  388 | `	ph7_result_int(pCtx, (int)(iOld ? 1 : 0));` |
|    13 |  389 | `	return PH7_OK;` |
|     1 |  390 | `}` |
|     - |  391 | `/*` |
|     - |  392 | ` * The bytes a cookie NAME may not carry, and the ones a RAW value may not: php` |
|     - |  393 | ` * refuses them where they are written rather than emitting a header a proxy would` |
|     - |  394 | ` * read as two. A setcookie() VALUE is url-encoded and so has no such rule.` |
|     - |  395 | ` */` |
|     - |  396 | `#define VM_COOKIE_NAME_BAD  "=,; \t\r\n\013\014"` |
|     - |  397 | `#define VM_COOKIE_VALUE_BAD  ",; \t\r\n\013\014"` |
|     - |  398 |  |
|    50 |  399 | `static int VmCookieBadByte(const char *zVal,sxu32 nVal,const char *zBad,sxu32 nBad)` |
|     1 |  400 | `{` |
|     - |  401 | `	sxu32 i;` |
|   153 |  402 | `	for( i = 0 ; i < nVal ; i++ ){` |
|   111 |  403 | `		if( SyByteFind(zBad,nBad,zVal[i],0) == SXRET_OK ){` |
|     9 |  404 | `			return 1;` |
|     - |  405 | `		}` |
|    52 |  406 | `	}` |
|    43 |  407 | `	return 0;` |
|    26 |  408 | `}` |
|     - |  409 | `/*` |
|     - |  410 | ` * The options ARRAY php's third parameter has taken since 7.3 -- declared in` |
|     - |  411 | ` * aBuiltinSig[] here and read as an INT, so every documented` |
|     - |  412 | `` * `setcookie($n,$v,['expires'=>…,'samesite'=>'Lax'])` emitted a cookie with no`` |
|     - |  413 | ` * attributes at ALL: no expiry, no path, no SameSite. The key match is` |
|     - |  414 | ` * case-insensitive and an unknown key is a ValueError, so a typo cannot silently` |
|     - |  415 | ` * drop the attribute that makes the cookie safe.` |
|     - |  416 | ` */` |
|     - |  417 | `struct VmCookieOpts {` |
|     - |  418 | `	sxi64 iExpires;` |
|     - |  419 | `	/* COPIES, not the walk's own pointers: ph7_array_walk hands the callback a` |
|     - |  420 | ``	 * value it reuses for the next entry, so a `const char *` kept out of it is`` |
|     - |  421 | `	 * dangling by the time the header is built. */` |
|     - |  422 | `	SyBlob sPath, sDomain, sSame;` |
|     - |  423 | `	int bSecure, bHttpOnly, bPartitioned;` |
|     - |  424 | `	char zBadKey[64];` |
|     - |  425 | `};` |
|    14 |  426 | `static void VmCookieOptStr(SyBlob *pOut,ph7_value *pVal)` |
|     1 |  427 | `{` |
|    15 |  428 | `	int nVal = 0;` |
|    15 |  429 | `	const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|    15 |  430 | `	SyBlobReset(pOut);` |
|    15 |  431 | `	if( nVal > 0 ){` |
|    15 |  432 | `		SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|     7 |  433 | `	}` |
|    15 |  434 | `}` |
|    30 |  435 | `static int VmCookieOptionWalker(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  436 | `{` |
|    31 |  437 | `	struct VmCookieOpts *pOpt = (struct VmCookieOpts *)pUserData;` |
|     - |  438 | `	const char *zKey;` |
|    31 |  439 | `	int nKey = 0;` |
|    31 |  440 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    31 |  441 | `	if( nKey == (int)sizeof("expires")-1 && SyStrnicmp(zKey,"expires",(sxu32)nKey) == 0 ){` |
|     5 |  442 | `		pOpt->iExpires = ph7_value_to_int64(pVal);` |
|    29 |  443 | `	}else if( nKey == (int)sizeof("path")-1 && SyStrnicmp(zKey,"path",(sxu32)nKey) == 0 ){` |
|     2 |  444 | `		VmCookieOptStr(&pOpt->sPath,pVal);` |
|    26 |  445 | `	}else if( nKey == (int)sizeof("domain")-1 && SyStrnicmp(zKey,"domain",(sxu32)nKey) == 0 ){` |
|     2 |  446 | `		VmCookieOptStr(&pOpt->sDomain,pVal);` |
|    24 |  447 | `	}else if( nKey == (int)sizeof("samesite")-1 && SyStrnicmp(zKey,"samesite",(sxu32)nKey) == 0 ){` |
|     7 |  448 | `		VmCookieOptStr(&pOpt->sSame,pVal);` |
|    20 |  449 | `	}else if( nKey == (int)sizeof("secure")-1 && SyStrnicmp(zKey,"secure",(sxu32)nKey) == 0 ){` |
|     7 |  450 | `		pOpt->bSecure = ph7_value_to_bool(pVal);` |
|    14 |  451 | `	}else if( nKey == (int)sizeof("httponly")-1 && SyStrnicmp(zKey,"httponly",(sxu32)nKey) == 0 ){` |
|     2 |  452 | `		pOpt->bHttpOnly = ph7_value_to_bool(pVal);` |
|    10 |  453 | `	}else if( nKey == (int)sizeof("partitioned")-1 && SyStrnicmp(zKey,"partitioned",(sxu32)nKey) == 0 ){` |
|     5 |  454 | `		pOpt->bPartitioned = ph7_value_to_bool(pVal);` |
|     3 |  455 | `	}else{` |
|     5 |  456 | `		sxu32 nCopy = (sxu32)nKey;` |
|     5 |  457 | `		if( nCopy > sizeof(pOpt->zBadKey)-1 ){` |
|   ! 0 |  458 | `			nCopy = sizeof(pOpt->zBadKey)-1;` |
|   ! 0 |  459 | `		}` |
|     5 |  460 | `		SyMemcpy(zKey,pOpt->zBadKey,nCopy);` |
|     5 |  461 | `		pOpt->zBadKey[nCopy] = 0;` |
|     5 |  462 | `		return SXERR_ABORT;   /* the caller reports which key it was */` |
|     - |  463 | `	}` |
|    27 |  464 | `	return PH7_OK;` |
|    16 |  465 | `}` |
|     - |  466 | `/*` |
|     - |  467 | ` * An HTTP-date: "Thu, 19 Nov 1981 08:52:00 GMT", with the three-letter day and` |
|     - |  468 | ` * month names an HTTP-date is defined in (locale-independent, from sxlib's own` |
|     - |  469 | ` * tables). Answers the byte count, or 0 for a time the C library will not break` |
|     - |  470 | ` * down.` |
|     - |  471 | ` */` |
|    18 |  472 | `PH7_PRIVATE int PH7_VmHttpDate(sxi64 iWhen,char *zBuf,int nBuf)` |
|     2 |  473 | `{` |
|     - |  474 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - |  475 | `	/* The day/month name tables live behind the same switch: a build without the` |
|     - |  476 | `	 * builtins has no date to print. */` |
|     - |  477 | `	(void)iWhen; (void)zBuf; (void)nBuf;` |
|     - |  478 | `	return 0;` |
|     - |  479 | `#else` |
|    20 |  480 | `	time_t t = (time_t)iWhen;` |
|     - |  481 | `	struct tm tm_buf;` |
|     - |  482 | `	int tm_ok;` |
|     - |  483 | `#ifdef __WINNT__` |
|     2 |  484 | `	tm_ok = (gmtime_s(&tm_buf,&t) == 0);` |
|     - |  485 | `#else` |
|    18 |  486 | `	tm_ok = (gmtime_r(&t,&tm_buf) != 0);` |
|     - |  487 | `#endif` |
|    20 |  488 | `	if( !tm_ok ){` |
|   ! 0 |  489 | `		return 0;` |
|     - |  490 | `	}` |
|    29 |  491 | `	return SyBufferFormat(zBuf,(sxu32)nBuf,"%.3s, %02d %.3s %04d %02d:%02d:%02d GMT",` |
|     9 |  492 | `		SyTimeGetDay(tm_buf.tm_wday),tm_buf.tm_mday,` |
|    18 |  493 | `		SyTimeGetMonth(tm_buf.tm_mon),1900 + tm_buf.tm_year,` |
|     9 |  494 | `		tm_buf.tm_hour,tm_buf.tm_min,tm_buf.tm_sec);` |
|     - |  495 | `#endif` |
|    11 |  496 | `}` |
|     - |  497 | `/* Queue a response header, replacing any of the same name. */` |
|    28 |  498 | `PH7_PRIVATE void PH7_VmSetResponseHeader(ph7_vm *pVm,const char *zName,const char *zValue,` |
|     - |  499 | `	sxu32 nValue)` |
|   ! 0 |  500 | `{` |
|    28 |  501 | `	VmAddResponseHeader(pVm,zName,(sxu32)SyStrlen(zName),zValue,nValue,1);` |
|    28 |  502 | `}` |
|     - |  503 | `/*` |
|     - |  504 | ` * Drop any Set-Cookie already queued for this cookie NAME. php does this before it` |
|     - |  505 | ` * sends the session cookie (php_session_remove_cookie): a request that regenerates` |
|     - |  506 | ` * its id must not leave the OLD id in the reply beside the new one, and only the` |
|     - |  507 | ` * cookie of that name goes -- the header name is shared with every other cookie.` |
|     - |  508 | ` */` |
|   106 |  509 | `PH7_PRIVATE void PH7_VmRemoveCookieByName(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|     4 |  510 | `{` |
|   110 |  511 | `	VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|   110 |  512 | `	sxu32 i,n = SySetUsed(&pVm->aResponseHeaders);` |
|   194 |  513 | `	for( i = 0 ; i < n ; ){` |
|    88 |  514 | `		const SyString *pVal = &aHdr[i].sValue;` |
|    84 |  515 | `		if( aHdr[i].sName.nByte == sizeof("Set-Cookie")-1` |
|    81 |  516 | `		 && SyStrnicmp(aHdr[i].sName.zString,"Set-Cookie",sizeof("Set-Cookie")-1) == 0` |
|    78 |  517 | `		 && pVal->nByte > nName` |
|    78 |  518 | `		 && SyMemcmp(pVal->zString,zName,nName) == 0` |
|    76 |  519 | `		 && pVal->zString[nName] == '=' ){` |
|    70 |  520 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sName.zString);` |
|    70 |  521 | `			SyMemBackendFree(&pVm->sAllocator,(void *)aHdr[i].sValue.zString);` |
|    70 |  522 | `			if( i < n - 1 ){` |
|     2 |  523 | `				aHdr[i] = aHdr[n - 1];` |
|     1 |  524 | `			}` |
|    70 |  525 | `			SySetPop(&pVm->aResponseHeaders);` |
|    70 |  526 | `			n--;` |
|    37 |  527 | `		}else{` |
|    19 |  528 | `			i++;` |
|     - |  529 | `		}` |
|     4 |  530 | `	}` |
|   110 |  531 | `}` |
|     - |  532 | `/*` |
|     - |  533 | ` * Build the Set-Cookie value php builds, and append it (never replace).` |
|     - |  534 | ` * PH7_PRIVATE because the session's own cookie is this same header with the` |
|     - |  535 | ` * session's parameters, not a second spelling of it.` |
|     - |  536 | ` */` |
|   120 |  537 | `PH7_PRIVATE void PH7_VmEmitCookie(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|     - |  538 | `	const char *zValue,sxu32 nValue,int bEncode,sxi64 iExpires,` |
|     - |  539 | `	const char *zPath,sxu32 nPath,const char *zDomain,sxu32 nDomain,` |
|     - |  540 | `	int bSecure,int bHttpOnly,const char *zSame,sxu32 nSame,int bPartitioned)` |
|     4 |  541 | `{` |
|     - |  542 | `	SyBlob sWorker;` |
|   124 |  543 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     - |  544 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   124 |  545 | `	if( bEncode ){` |
|     - |  546 | ``		/* RAW url-encoding, php's: a space becomes %20 and `~` is left alone,`` |
|     - |  547 | `		 * which is not what urlencode() does — and a cookie value is read back by` |
|     - |  548 | `		 * a browser, so the two spellings are not interchangeable. */` |
|   122 |  549 | `		SyUriEncodeRaw(zName,nName,PH7_VmBlobConsumer,&sWorker);` |
|    63 |  550 | `	}else` |
|     - |  551 | `#else` |
|     - |  552 | `	(void)bEncode;` |
|     - |  553 | `#endif` |
|     - |  554 | `	{` |
|     2 |  555 | `		SyBlobAppend(&sWorker,zName,nName);` |
|     - |  556 | `	}` |
|   124 |  557 | `	SyBlobAppend(&sWorker,"=",1);` |
|   124 |  558 | `	if( nValue < 1 ){` |
|     - |  559 | ``		/* php replaces an EMPTY value with the literal `deleted` and dates the`` |
|     - |  560 | `		 * cookie to the epoch: that is what "unset this cookie" IS on the wire. */` |
|     2 |  561 | `		SyBlobAppend(&sWorker,"deleted",sizeof("deleted")-1);` |
|     2 |  562 | `		iExpires = 1;` |
|     1 |  563 | `	}else` |
|     - |  564 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   122 |  565 | `	if( bEncode ){` |
|   120 |  566 | `		SyUriEncodeRaw(zValue,nValue,PH7_VmBlobConsumer,&sWorker);` |
|    62 |  567 | `	}else` |
|     - |  568 | `#endif` |
|     - |  569 | `	{` |
|     2 |  570 | `		SyBlobAppend(&sWorker,zValue,nValue);` |
|     - |  571 | `	}` |
|     - |  572 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   124 |  573 | `	if( iExpires > 0 ){` |
|     - |  574 | `		char zDate[80];` |
|    12 |  575 | `		int nDate = PH7_VmHttpDate(iExpires,zDate,(int)sizeof(zDate));` |
|    12 |  576 | `		if( nDate > 0 ){` |
|    12 |  577 | `			sxi64 iMaxAge = iExpires - (sxi64)time(0);` |
|     - |  578 | `			char zTail[64];` |
|    12 |  579 | `			SyBlobAppend(&sWorker,"; expires=",sizeof("; expires=")-1);` |
|    12 |  580 | `			SyBlobAppend(&sWorker,zDate,(sxu32)nDate);` |
|    22 |  581 | `			nDate = SyBufferFormat(zTail,sizeof(zTail),"; Max-Age=%qd",` |
|    10 |  582 | `				iMaxAge < 0 ? (sxi64)0 : iMaxAge);` |
|    12 |  583 | `			SyBlobAppend(&sWorker,zTail,(sxu32)nDate);` |
|     5 |  584 | `		}` |
|     5 |  585 | `	}` |
|     - |  586 | `#else` |
|     - |  587 | `	(void)iExpires;` |
|     - |  588 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|   124 |  589 | `	if( nPath > 0 ){` |
|   114 |  590 | `		SyBlobAppend(&sWorker,"; path=",sizeof("; path=")-1);` |
|   114 |  591 | `		SyBlobAppend(&sWorker,zPath,nPath);` |
|    55 |  592 | `	}` |
|   124 |  593 | `	if( nDomain > 0 ){` |
|    11 |  594 | `		SyBlobAppend(&sWorker,"; domain=",sizeof("; domain=")-1);` |
|    11 |  595 | `		SyBlobAppend(&sWorker,zDomain,nDomain);` |
|     5 |  596 | `	}` |
|   124 |  597 | `	if( bSecure ){` |
|    13 |  598 | `		SyBlobAppend(&sWorker,"; secure",sizeof("; secure")-1);` |
|     6 |  599 | `	}` |
|   124 |  600 | `	if( bHttpOnly ){` |
|    11 |  601 | `		SyBlobAppend(&sWorker,"; HttpOnly",sizeof("; HttpOnly")-1);` |
|     5 |  602 | `	}` |
|   124 |  603 | `	if( nSame > 0 ){` |
|    11 |  604 | `		SyBlobAppend(&sWorker,"; SameSite=",sizeof("; SameSite=")-1);` |
|    11 |  605 | `		SyBlobAppend(&sWorker,zSame,nSame);` |
|     5 |  606 | `	}` |
|   124 |  607 | `	if( bPartitioned ){` |
|     5 |  608 | `		SyBlobAppend(&sWorker,"; Partitioned",sizeof("; Partitioned")-1);` |
|     2 |  609 | `	}` |
|   184 |  610 | `	VmAddResponseHeader(pVm,"Set-Cookie",10,` |
|   120 |  611 | `		(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),` |
|     - |  612 | `		0 /* bReplace = false */);` |
|   124 |  613 | `	SyBlobRelease(&sWorker);` |
|   124 |  614 | `}` |
|     - |  615 | `/*` |
|     - |  616 | ` * Internal helper for setcookie/setrawcookie.` |
|     - |  617 | ` */` |
|    44 |  618 | `static int VmSetCookieImpl(ph7_context *pCtx, int nArg, ph7_value **apArg, int bEncode)` |
|     1 |  619 | `{` |
|    45 |  620 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  621 | `	const char *zName, *zValue;` |
|    45 |  622 | `	const char *zFunc = bEncode ? "setcookie" : "setrawcookie";` |
|    45 |  623 | `	int nNameLen, nValueLen = 0, bBadOpt = 0;` |
|     - |  624 | `	struct VmCookieOpts sOpt;` |
|    45 |  625 | `	if( nArg < 1 ){` |
|   ! 0 |  626 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  627 | `		return PH7_OK;` |
|     - |  628 | `	}` |
|    45 |  629 | `	zName = ph7_value_to_string(apArg[0], &nNameLen);` |
|    45 |  630 | `	if( nNameLen < 1 ){` |
|     4 |  631 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     1 |  632 | `			"%s(): Argument #1 ($name) must not be empty",zFunc);` |
|     - |  633 | `	}` |
|    43 |  634 | `	if( VmCookieBadByte(zName,(sxu32)nNameLen,` |
|     - |  635 | `		VM_COOKIE_NAME_BAD,sizeof(VM_COOKIE_NAME_BAD)-1) ){` |
|     7 |  636 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  637 | `			"%s(): Argument #1 ($name) cannot contain \"=\", \",\", \";\","` |
|     2 |  638 | `			" \" \", \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);` |
|     - |  639 | `	}` |
|    39 |  640 | `	if( nArg >= 2 ){` |
|    39 |  641 | `		zValue = ph7_value_to_string(apArg[1], &nValueLen);` |
|    20 |  642 | `	}else{` |
|   ! 0 |  643 | `		zValue = "";` |
|     - |  644 | `	}` |
|    39 |  645 | `	if( !bEncode && VmCookieBadByte(zValue,(sxu32)nValueLen,` |
|     - |  646 | `		VM_COOKIE_VALUE_BAD,sizeof(VM_COOKIE_VALUE_BAD)-1) ){` |
|     - |  647 | `		/* Only the RAW value reaches the wire unchanged, so only it is screened;` |
|     - |  648 | `		 * setcookie() url-encodes and can carry anything. */` |
|     7 |  649 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  650 | `			"%s(): Argument #2 ($value) cannot contain \",\", \";\", \" \","` |
|     2 |  651 | `			" \"\\t\", \"\\r\", \"\\n\", \"\\013\", or \"\\014\"",zFunc);` |
|     - |  652 | `	}` |
|    35 |  653 | `	SyZero(&sOpt,sizeof(sOpt));` |
|    35 |  654 | `	SyBlobInit(&sOpt.sPath,&pVm->sAllocator);` |
|    35 |  655 | `	SyBlobInit(&sOpt.sDomain,&pVm->sAllocator);` |
|    35 |  656 | `	SyBlobInit(&sOpt.sSame,&pVm->sAllocator);` |
|    35 |  657 | `	if( nArg >= 3 && ph7_value_is_array(apArg[2]) ){` |
|    15 |  658 | `		bBadOpt = ph7_array_walk(apArg[2],VmCookieOptionWalker,&sOpt) != PH7_OK;` |
|    28 |  659 | `	}else if( nArg >= 3 ){` |
|     2 |  660 | `		sOpt.iExpires = ph7_value_to_int64(apArg[2]);` |
|     2 |  661 | `		if( nArg >= 4 ){` |
|     2 |  662 | `			VmCookieOptStr(&sOpt.sPath,apArg[3]);` |
|     1 |  663 | `		}` |
|     2 |  664 | `		if( nArg >= 5 ){` |
|     2 |  665 | `			VmCookieOptStr(&sOpt.sDomain,apArg[4]);` |
|     1 |  666 | `		}` |
|     2 |  667 | `		if( nArg >= 6 ){` |
|     2 |  668 | `			sOpt.bSecure = ph7_value_to_bool(apArg[5]);` |
|     1 |  669 | `		}` |
|     2 |  670 | `		if( nArg >= 7 ){` |
|     2 |  671 | `			sOpt.bHttpOnly = ph7_value_to_bool(apArg[6]);` |
|     1 |  672 | `		}` |
|     1 |  673 | `	}` |
|    35 |  674 | `	if( !bBadOpt ){` |
|     - |  675 | `		/* php's CLI SAPI takes the header and answers TRUE even though nothing will` |
|     - |  676 | `		 * ever print it; only a real header ALREADY sent is a refusal. */` |
|    31 |  677 | `		if( pVm->bHeadersSent ){` |
|     5 |  678 | `			VmHeadersAlreadySent(pCtx,0);` |
|     5 |  679 | `			ph7_result_bool(pCtx, 0);` |
|     3 |  680 | `		}else{` |
|    27 |  681 | `			if( pVm->bHttpContext ){` |
|    21 |  682 | `				PH7_VmEmitCookie(pVm,zName,(sxu32)nNameLen,zValue,(sxu32)nValueLen,bEncode,` |
|     7 |  683 | `					sOpt.iExpires,` |
|    14 |  684 | `					(const char *)SyBlobData(&sOpt.sPath),SyBlobLength(&sOpt.sPath),` |
|    14 |  685 | `					(const char *)SyBlobData(&sOpt.sDomain),SyBlobLength(&sOpt.sDomain),` |
|     7 |  686 | `					sOpt.bSecure,sOpt.bHttpOnly,` |
|    14 |  687 | `					(const char *)SyBlobData(&sOpt.sSame),SyBlobLength(&sOpt.sSame),` |
|     7 |  688 | `					sOpt.bPartitioned);` |
|     7 |  689 | `			}` |
|    27 |  690 | `			ph7_result_bool(pCtx, 1);` |
|     - |  691 | `		}` |
|    15 |  692 | `	}` |
|    35 |  693 | `	SyBlobRelease(&sOpt.sPath);` |
|    35 |  694 | `	SyBlobRelease(&sOpt.sDomain);` |
|    35 |  695 | `	SyBlobRelease(&sOpt.sSame);` |
|    35 |  696 | `	if( bBadOpt ){` |
|     7 |  697 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 |  698 | `			"%s(): option \"%s\" is invalid",zFunc,sOpt.zBadKey);` |
|     - |  699 | `	}` |
|    31 |  700 | `	return PH7_OK;` |
|    23 |  701 | `}` |
|     - |  702 | `/*` |
|     - |  703 | ` * bool setcookie(string $name [, string $value [, int $expires [, string $path` |
|     - |  704 | ` *                [, string $domain [, bool $secure [, bool $httponly]]]]]])` |
|     - |  705 | ` */` |
|    36 |  706 | `static int vm_builtin_setcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  707 | `{` |
|    37 |  708 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 1 /* URL-encode */);` |
|     1 |  709 | `}` |
|     - |  710 | `/*` |
|     - |  711 | ` * bool setrawcookie(string $name [, string $value [, ...]])` |
|     - |  712 | ` */` |
|     8 |  713 | `static int vm_builtin_setrawcookie(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  714 | `{` |
|     9 |  715 | `	return VmSetCookieImpl(pCtx, nArg, apArg, 0 /* no encoding */);` |
|     1 |  716 | `}` |
|     - |  717 | `/*` |
|     - |  718 | ` * Register all HTTP response functions with the VM.` |
|     - |  719 | ` */` |
|  5619 |  720 | `PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm)` |
|     5 |  721 | `{` |
|     - |  722 | `	static const ph7_builtin_func aFunc[] = {` |
|     - |  723 | `		{ "header",             vm_builtin_header             },` |
|     - |  724 | `		{ "header_remove",      vm_builtin_header_remove      },` |
|     - |  725 | `		{ "headers_sent",       vm_builtin_headers_sent       },` |
|     - |  726 | `		{ "connection_status",  vm_builtin_connection_status  },` |
|     - |  727 | `		{ "connection_aborted", vm_builtin_connection_aborted },` |
|     - |  728 | `		{ "ignore_user_abort",  vm_builtin_ignore_user_abort  },` |
|     - |  729 | `		{ "headers_list",       vm_builtin_headers_list       },` |
|     - |  730 | `		{ "http_response_code", vm_builtin_http_response_code },` |
|     - |  731 | `		{ "setcookie",          vm_builtin_setcookie          },` |
|     - |  732 | `		{ "setrawcookie",       vm_builtin_setrawcookie       },` |
|     - |  733 | `	};` |
|     - |  734 | `	sxu32 n;` |
| 61814 |  735 | `	for( n = 0; n < SX_ARRAYSIZE(aFunc); n++ ){` |
| 56195 |  736 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 28055 |  737 | `	}` |
|  5624 |  738 | `}` |
|     - |  739 |  |
