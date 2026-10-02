# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 304/360 lines (84.44%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <stddef.h> /* NULL */` |
|     - |    8 | `/*` |
|     - |    9 | ` * Section:` |
|     - |   10 | ` *    Error-handling builtins: assert, trigger_error, error_reporting,` |
|     - |   11 | ` *    error_log, the error/exception handler get/set/restore family,` |
|     - |   12 | ` *    error_get_last/clear_last and the debug_backtrace family.` |
|     - |   13 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|     - |   14 | ` * Status:` |
|     - |   15 | ` *    Stable.` |
|     - |   16 | ` */` |
|     - |   17 | `/*` |
|     - |   18 | ` * bool assert(mixed $assertion)` |
|     - |   19 | ` *  Checks if assertion is FALSE.` |
|     - |   20 | ` * Parameter` |
|     - |   21 | ` *  $assertion` |
|     - |   22 | ` *    The assertion to test.` |
|     - |   23 | ` * Return` |
|     - |   24 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|     - |   25 | ` */` |
|    66 |   26 | `PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |   27 | `{` |
|    71 |   28 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   29 | `	int iFlags,iResult;` |
|     - |   30 | `	const char *zDesc;` |
|    71 |   31 | `	iFlags = pVm->iAssertFlags;` |
|    71 |   32 | `	if( iFlags & (PH7_ASSERT_DISABLE\|PH7_ASSERT_ZEND_OFF) ){` |
|     - |   33 | `		/* Assertion is disabled (assert.active=0) or compiled out` |
|     - |   34 | `		 * (zend.assertions<1); the call is elided entirely -- even a missing` |
|     - |   35 | `		 * argument is not diagnosed -- and it evaluates to TRUE (PHP 8). */` |
|    11 |   36 | `		ph7_result_bool(pCtx,1);` |
|    11 |   37 | `		return PH7_OK;` |
|     - |   38 | `	}` |
|     - |   39 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|    60 |   40 | `	if( nArg < 1 ){` |
|   ! 0 |   41 | `		return PH7_VmThrowException(pCtx,` |
|     - |   42 | `			"ArgumentCountError",` |
|     - |   43 | `			"assert() expects at least 1 argument, 0 given"` |
|     - |   44 | `			);` |
|     - |   45 | `	}` |
|     - |   46 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|    60 |   47 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|    60 |   48 | `	if( !iResult ){` |
|     - |   49 | `		/* Assertion failed */` |
|     - |   50 | `		/* Extract optional description */` |
|    60 |   51 | `		zDesc = 0;` |
|    60 |   52 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|     5 |   53 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|     2 |   54 | `		}` |
|    60 |   55 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|     - |   56 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|     - |   57 | `			ph7_value sFile,sLine;` |
|     - |   58 | `			ph7_value *apCbArg[3];` |
|     - |   59 | `			SyString *pFile;` |
|     - |   60 | `			/* Extract the processed script */` |
|   ! 0 |   61 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   ! 0 |   62 | `			if( pFile == 0 ){` |
|   ! 0 |   63 | `				pFile = (SyString *)&sFileName;` |
|   ! 0 |   64 | `			}` |
|     - |   65 | `			/* Invoke the callback */` |
|   ! 0 |   66 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|   ! 0 |   67 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|   ! 0 |   68 | `			apCbArg[0] = &sFile;` |
|   ! 0 |   69 | `			apCbArg[1] = &sLine;` |
|   ! 0 |   70 | `			apCbArg[2] = apArg[0];` |
|   ! 0 |   71 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|     - |   72 | `			/* Clean-up the mess left behind */` |
|   ! 0 |   73 | `			PH7_MemObjRelease(&sFile);` |
|   ! 0 |   74 | `			PH7_MemObjRelease(&sLine);` |
|   ! 0 |   75 | `		}` |
|    60 |   76 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|     - |   77 | `			/* Abort VM execution immediately */` |
|   ! 0 |   78 | `			return PH7_ABORT;` |
|     - |   79 | `		}` |
|     - |   80 | `		/* PHP 8: throw AssertionError by default */` |
|    60 |   81 | `		if( zDesc && zDesc[0] != '\0' ){` |
|     7 |   82 | `			return PH7_VmThrowException(pCtx,` |
|     - |   83 | `				"AssertionError",` |
|     - |   84 | `				"%s",` |
|     2 |   85 | `				zDesc` |
|     - |   86 | `				);` |
|   ! 0 |   87 | `		}else{` |
|     - |   88 | ``			/* php renders the assertion's compile-time SOURCE (`assert(1 == 2)`);`` |
|     - |   89 | `			 * the compiler captured it in the call-site map. An` |
|     - |   90 | `			 * INDIRECT call (call_user_func, a callable string/variable) has no` |
|     - |   91 | `			 * source in php either — its AssertionError carries an EMPTY message` |
|     - |   92 | `			 * (probed php 8.5). */` |
|    56 |   93 | `			VmCallArgMap *pMap = pCtx->pArgMap;` |
|    56 |   94 | `			if( pMap && pMap->sAssertSrc.nByte > 0 ){` |
|    76 |   95 | `				return PH7_VmThrowException(pCtx,` |
|     - |   96 | `					"AssertionError",` |
|     - |   97 | `					"assert(%z)",` |
|    24 |   98 | `					&pMap->sAssertSrc` |
|     - |   99 | `					);` |
|     - |  100 | `			}` |
|     5 |  101 | `			return PH7_VmThrowException(pCtx,` |
|     - |  102 | `				"AssertionError",` |
|     - |  103 | `				"%s",` |
|     - |  104 | `				""` |
|     - |  105 | `				);` |
|     - |  106 | `		}` |
|     - |  107 | `	}` |
|     - |  108 | `	/* Assertion passed */` |
|   ! 0 |  109 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  110 | `	return PH7_OK;` |
|    38 |  111 | `}` |
|     - |  112 | `/*` |
|     - |  113 | ` * Section:` |
|     - |  114 | ` *  Error reporting functions.` |
|     - |  115 | ` * Status:` |
|     - |  116 | ` *    Stable.` |
|     - |  117 | ` */` |
|     - |  118 | `/*` |
|     - |  119 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|     - |  120 | ` *  Generates a user-level error/warning/notice message.` |
|     - |  121 | ` * Parameters` |
|     - |  122 | ` *  $error_msg` |
|     - |  123 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|     - |  124 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|     - |  125 | ` * $error_type` |
|     - |  126 | ` *  The designated error type for this error. It only works with the E_USER family` |
|     - |  127 | ` *  of constants, and will default to E_USER_NOTICE.` |
|     - |  128 | ` * Return` |
|     - |  129 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|     - |  130 | ` */` |
|   124 |  131 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  132 | `{` |
|   129 |  133 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|   129 |  134 | `	if( nArg > 0 ){` |
|     - |  135 | `		const char *zErr;` |
|     - |  136 | `		int nLen;` |
|     - |  137 | `		/* Extract the error message */` |
|   129 |  138 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|   129 |  139 | `		if( nArg > 1 ){` |
|     - |  140 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|     - |  141 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|     - |  142 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|     - |  143 | `			 * translated engine severity). */` |
|   129 |  144 | `			nErr = ph7_value_to_int(apArg[1]);` |
|   129 |  145 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|   ! 0 |  146 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  147 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|     - |  148 | `			}` |
|   129 |  149 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|     - |  150 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|     3 |  151 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  152 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|     - |  153 | `			}` |
|    61 |  154 | `		}` |
|     - |  155 | `		/* Report error (consults an installed error handler, then displays) */` |
|   127 |  156 | `		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|   122 |  157 | `		if( nErr == 256 /* E_USER_ERROR */` |
|    66 |  158 | `		 && !ph7_value_is_callable(&pCtx->pVm->sErrCB) ){` |
|     - |  159 | `			/* php: an unhandled user fatal halts with exit 255 (pre-fix the` |
|     - |  160 | `			 * PH7_ABORT here was overwritten by the throw's status, so` |
|     - |  161 | `			 * E_USER_ERROR silently CONTINUED). With a handler installed the` |
|     - |  162 | `			 * script continues — the handler-returned-false renormalization is` |
|     - |  163 | `			 * a recorded nuance. No php stack trace either (recorded). */` |
|   ! 0 |  164 | `			pCtx->pVm->iExitStatus = 255;` |
|   ! 0 |  165 | `			pCtx->pVm->bHaltRequested = 1;` |
|   ! 0 |  166 | `			return PH7_ABORT;` |
|     - |  167 | `		}` |
|     - |  168 | `		/* Return true */` |
|   127 |  169 | `		ph7_result_bool(pCtx,1);` |
|    66 |  170 | `	}else{` |
|     - |  171 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  172 | `		ph7_result_bool(pCtx,0);` |
|     - |  173 | `	}` |
|   127 |  174 | `	return PH7_OK;` |
|    67 |  175 | `}` |
|     - |  176 | `/*` |
|     - |  177 | ` * int error_reporting([int $level])` |
|     - |  178 | ` *  Sets which PHP errors are reported.` |
|     - |  179 | ` * Parameters` |
|     - |  180 | ` *  $level` |
|     - |  181 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|     - |  182 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|     - |  183 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|     - |  184 | ` *   levels will not always behave as expected.` |
|     - |  185 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|     - |  186 | ` *   in the predefined constants.` |
|     - |  187 | ` * Return` |
|     - |  188 | ` *   Returns the old error_reporting level or the current level if no level` |
|     - |  189 | ` *   parameter is given.` |
|     - |  190 | ` */` |
|  1082 |  191 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  192 | `{` |
|  1087 |  193 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  194 | `	int nOld;` |
|     - |  195 | `	/* Extract the old reporting level */` |
|  1087 |  196 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|  1087 |  197 | `	if( pVm->nErrSuppress > 0 ){` |
|     - |  198 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|     - |  199 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|     - |  200 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|     - |  201 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|     - |  202 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|   387 |  203 | `		nOld &= 4437;` |
|   191 |  204 | `	}` |
|  1087 |  205 | `	if( nArg > 0 ){` |
|     - |  206 | `		int nNew;` |
|     - |  207 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|   186 |  208 | `		nNew = ph7_value_to_int(apArg[0]);` |
|   186 |  209 | `		pVm->iErrMask = (sxi32)nNew;` |
|   186 |  210 | `		pVm->bErrReport = nNew != 0;` |
|   186 |  211 | `		pVm->bErrMaskSet = 1;` |
|    90 |  212 | `	}` |
|     - |  213 | `	/* Return the old level */` |
|  1087 |  214 | `	ph7_result_int(pCtx,nOld);` |
|  1087 |  215 | `	return PH7_OK;` |
|     5 |  216 | `}` |
|     - |  217 | `/*` |
|     - |  218 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|     - |  219 | ` *  Send an error message somewhere.` |
|     - |  220 | ` * Parameter` |
|     - |  221 | ` *  $message` |
|     - |  222 | ` *   The error message that should be logged.` |
|     - |  223 | ` *  $message_type` |
|     - |  224 | ` *   Says where the error should go. The possible message types are as follows:` |
|     - |  225 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|     - |  226 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|     - |  227 | ` *       This is the default option.` |
|     - |  228 | ` *    1 message is sent by email to the address in the destination parameter.` |
|     - |  229 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|     - |  230 | ` *    2  No longer an option.` |
|     - |  231 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|     - |  232 | ` *       to the end of the message string.` |
|     - |  233 | ` *    4  message is sent directly to the SAPI logging handler.` |
|     - |  234 | ` *  $destination` |
|     - |  235 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|     - |  236 | ` *  $extra_headers` |
|     - |  237 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|     - |  238 | ` * Return` |
|     - |  239 | ` *  TRUE on success or FALSE on failure.` |
|     - |  240 | ` *` |
|     - |  241 | ` * This used to invoke the PH7_VM_CONFIG_ERR_LOG_HANDLER embedder callback and` |
|     - |  242 | ` * NOTHING else: with no callback installed — which is every CLI run — the whole` |
|     - |  243 | `` * function was a no-op that answered TRUE. `error_log($msg)` printed nothing,`` |
|     - |  244 | `` * `error_log($msg, 3, $file)` wrote no file and still said it had, and a`` |
|     - |  245 | ` * destination that could not be opened answered TRUE as well. Every "log this` |
|     - |  246 | ` * and carry on" call in a program silently disappeared, which is the one thing` |
|     - |  247 | ` * a logging call must not do.` |
|     - |  248 | ` *` |
|     - |  249 | ` * php's own routing, which is what it does now: type 3 APPENDS the message` |
|     - |  250 | ` * verbatim to $destination (no newline added, no timestamp) and answers FALSE` |
|     - |  251 | ` * with the open warning when it cannot; type 2 is php's ValueError; type 1 is` |
|     - |  252 | ` * mail(), which this engine has no transport for, so it answers FALSE rather` |
|     - |  253 | ` * than claiming a delivery; 0 and anything unrecognised go to the CONFIGURED` |
|     - |  254 | `` * logger (the `error_log` destination, timestamped, falling back to the`` |
|     - |  255 | ` * diagnostics stream when it is unset or will not open); and 4 is the SAPI` |
|     - |  256 | ` * logger itself, which is the diagnostics stream and never the file. The embedder callback still wins` |
|     - |  257 | ` * when one is installed: that is what it is for.` |
|     - |  258 | ` */` |
|    26 |  259 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  260 | `{` |
|     - |  261 | `	const char *zMessage,*zDest,*zHeader;` |
|    29 |  262 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  263 | `	int iType = 0, nMsg = 0, nDest = 0;` |
|    29 |  264 | `	if( nArg < 1 ){` |
|     - |  265 | `		/* Missing log message,return FALSE */` |
|   ! 0 |  266 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  267 | `		return PH7_OK;` |
|     - |  268 | `	}` |
|    29 |  269 | `	zMessage = ph7_value_to_string(apArg[0],&nMsg);` |
|    29 |  270 | `	zDest = zHeader = ""; /* Empty string */` |
|    29 |  271 | `	if( nArg > 1 ){` |
|    27 |  272 | `		iType = ph7_value_to_int(apArg[1]);` |
|    12 |  273 | `	}` |
|    29 |  274 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    14 |  275 | `		zDest = ph7_value_to_string(apArg[2],&nDest);` |
|     6 |  276 | `	}` |
|    29 |  277 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|   ! 0 |  278 | `		zHeader = ph7_value_to_string(apArg[3],0);` |
|   ! 0 |  279 | `	}` |
|    29 |  280 | `	if( iType == 2 ){` |
|     - |  281 | `		/* php removed the TCP/IP destination in 8.0 and refuses the type outright. */` |
|     3 |  282 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  283 | `			"TCP/IP option is not available for error logging");` |
|     - |  284 | `	}` |
|    27 |  285 | `	if( pVm->xErrLog ){` |
|     - |  286 | `		/* An embedder took the routing over (PH7_VM_CONFIG_ERR_LOG_HANDLER). */` |
|   ! 0 |  287 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|   ! 0 |  288 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 |  289 | `		return PH7_OK;` |
|     - |  290 | `	}` |
|    27 |  291 | `	if( iType == 1 ){` |
|     - |  292 | `		/* mail(): no transport in this engine, so the message did NOT go out.` |
|     - |  293 | `		 * Answering TRUE for it was the old no-op's worst face. */` |
|   ! 0 |  294 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  295 | `		return PH7_OK;` |
|     - |  296 | `	}` |
|     - |  297 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    27 |  298 | `	if( iType == 3 ){` |
|     - |  299 | `		/* Appended VERBATIM: php adds neither a newline nor a timestamp here. */` |
|    12 |  300 | `		if( PH7_VfsAppendFile(pCtx,zDest,zMessage,nMsg) != PH7_OK ){` |
|     3 |  301 | `			ph7_result_bool(pCtx,0);` |
|     3 |  302 | `			return PH7_OK;` |
|     - |  303 | `		}` |
|    10 |  304 | `		ph7_result_bool(pCtx,1);` |
|    10 |  305 | `		return PH7_OK;` |
|     - |  306 | `	}` |
|     - |  307 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - |  308 | `	/* 0 and every unrecognised value go to the CONFIGURED logger, which is the` |
|     - |  309 | ``	 * `error_log` destination when one is set: php appends the message there`` |
|     - |  310 | ``	 * behind its `[d-M-Y H:i:s e] ` timestamp. Routing them to the diagnostics`` |
|     - |  311 | `	 * stream unconditionally is what made a program's own log calls miss the file` |
|     - |  312 | `	 * it had just named. 4 is the SAPI logger by definition and never the file, so` |
|     - |  313 | ``	 * `error_log($m, 4)` still reaches the stream with the destination set — that`` |
|     - |  314 | `	 * is the whole difference between the two types.` |
|     - |  315 | `	 *` |
|     - |  316 | `	 * php answers TRUE either way, INCLUDING when the destination will not open:` |
|     - |  317 | `	 * the fallback to the stream is a successful log as far as the caller is` |
|     - |  318 | `	 * concerned, and only type 3, which names its own file, reports a failure. */` |
|    16 |  319 | `	if( iType != 4 && PH7_VmErrorLogToFile(pVm,zMessage,(sxu32)nMsg) ){` |
|     5 |  320 | `		ph7_result_bool(pCtx,1);` |
|     5 |  321 | `		return PH7_OK;` |
|     - |  322 | `	}` |
|     - |  323 | `	/* The diagnostics stream, message plus a newline — php's CLI shape with no` |
|     - |  324 | ``	 * `error_log` ini set, and the SAPI logger type 4 always takes. */`` |
|     - |  325 | `	{` |
|    17 |  326 | `		ph7_output_consumer *pCons = pVm->sVmErrConsumer.xConsumer` |
|    10 |  327 | `			? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|     - |  328 | `		SyBlob sOut;` |
|    12 |  329 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|    12 |  330 | `		SyBlobAppend(&sOut,zMessage,(sxu32)nMsg);` |
|    12 |  331 | `		SyBlobAppend(&sOut,"\n",sizeof(char));` |
|    12 |  332 | `		pCons->xConsumer(SyBlobData(&sOut),SyBlobLength(&sOut),pCons->pUserData);` |
|    12 |  333 | `		SyBlobRelease(&sOut);` |
|     - |  334 | `	}` |
|    12 |  335 | `	ph7_result_bool(pCtx,1);` |
|    12 |  336 | `	return PH7_OK;` |
|    16 |  337 | `}` |
|     - |  338 | `/*` |
|     - |  339 | ` * php's set_error_handler()/set_exception_handler() stack, both directions.` |
|     - |  340 | ` *` |
|     - |  341 | `` * PH7 kept ONE saved slot per handler, so a third `set` overwrote the first`` |
|     - |  342 | `` * one's save and the matching `restore` could not find it again; and a NULL`` |
|     - |  343 | ` * argument was treated as a failure instead of the ordinary stack entry php` |
|     - |  344 | ` * pushes for it. Both routines below are shared by the error and the exception` |
|     - |  345 | ` * handler because php implements them the same way.` |
|     - |  346 | ` */` |
| 15373 |  347 | `static sxi32 VmHandlerPush(` |
|     - |  348 | `	ph7_vm *pVm,` |
|     - |  349 | `	ph7_value *pActive,   /* the currently installed handler */` |
|     - |  350 | `	sxi64 *piLevels,      /* its $error_levels (unused by the exception handler) */` |
|     - |  351 | `	SySet *pStack,        /* the saved entries underneath it */` |
|     - |  352 | ``	ph7_value *pNewCb,    /* the replacement, or 0 for the `null` reset */`` |
|     - |  353 | `	sxi64 iLevels` |
|     - |  354 | `	)` |
|     5 |  355 | `{` |
|     - |  356 | `	VmHandlerSlot sSlot;` |
|     - |  357 | `	sxi32 rc;` |
| 15378 |  358 | `	PH7_MemObjInit(pVm,&sSlot.sCb);` |
| 15378 |  359 | `	PH7_MemObjStore(pActive,&sSlot.sCb);` |
| 15378 |  360 | `	sSlot.iLevels = *piLevels;` |
| 15378 |  361 | `	rc = SySetPut(pStack,(const void *)&sSlot);` |
| 15378 |  362 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  363 | `		PH7_MemObjRelease(&sSlot.sCb);` |
|   ! 0 |  364 | `		return rc;` |
|     - |  365 | `	}` |
| 15378 |  366 | `	PH7_MemObjRelease(pActive);` |
| 15378 |  367 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
| 15378 |  368 | `	if( pNewCb ){` |
|  8872 |  369 | `		PH7_MemObjStore(pNewCb,pActive);` |
|  4432 |  370 | `	}` |
| 15378 |  371 | `	*piLevels = iLevels;` |
| 15378 |  372 | `	return SXRET_OK;` |
|  7690 |  373 | `}` |
|  2134 |  374 | `static void VmHandlerPop(ph7_vm *pVm,ph7_value *pActive,sxi64 *piLevels,SySet *pStack)` |
|     5 |  375 | `{` |
|  2139 |  376 | `	VmHandlerSlot *pSlot = (VmHandlerSlot *)SySetPop(pStack);` |
|  2139 |  377 | `	PH7_MemObjRelease(pActive);` |
|  2139 |  378 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
|  2139 |  379 | `	*piLevels = PH7_E_ALL_MASK;` |
|  2139 |  380 | `	if( pSlot ){` |
|  2135 |  381 | `		PH7_MemObjStore(&pSlot->sCb,pActive);` |
|  2135 |  382 | `		*piLevels = pSlot->iLevels;` |
|  2135 |  383 | `		PH7_MemObjRelease(&pSlot->sCb);` |
|  1065 |  384 | `	}` |
|  1067 |  385 | `	SXUNUSED(pVm);` |
|  2139 |  386 | `}` |
|     - |  387 | `/*` |
|     - |  388 | ` * bool restore_exception_handler(void)` |
|     - |  389 | ` *  Restores the previously defined exception handler function.` |
|     - |  390 | ` * Parameter` |
|     - |  391 | ` *  None` |
|     - |  392 | ` * Return` |
|     - |  393 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|     - |  394 | ` */` |
|    12 |  395 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  396 | `{` |
|    13 |  397 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  398 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     6 |  399 | `	SXUNUSED(nArg); /* cc warning */` |
|     6 |  400 | `	SXUNUSED(apArg);` |
|     - |  401 | `	/* An empty stack pops to NO handler: php answers TRUE either way, because the` |
|     - |  402 | `	 * return value says "the call is valid", not "a handler was in place". */` |
|    13 |  403 | `	VmHandlerPop(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved);` |
|    13 |  404 | `	ph7_result_bool(pCtx,1);` |
|    13 |  405 | `	return PH7_OK;` |
|     1 |  406 | `}` |
|     - |  407 | `/*` |
|     - |  408 | ` * callable set_exception_handler(callable $exception_handler)` |
|     - |  409 | ` *  Sets a user-defined exception handler function.` |
|     - |  410 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|     - |  411 | ` * NOTE` |
|     - |  412 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|     - |  413 | ` *  the satndard PHP engine.` |
|     - |  414 | ` * Parameters` |
|     - |  415 | ` *  $exception_handler` |
|     - |  416 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|     - |  417 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|     - |  418 | ` *   that was thrown.` |
|     - |  419 | ` *  Note:` |
|     - |  420 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  421 | ` * Return` |
|     - |  422 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|     - |  423 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  424 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  425 | ` */` |
|    16 |  426 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  427 | `{` |
|    19 |  428 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  429 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     - |  430 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  431 | `	 * saved under it. */` |
|    19 |  432 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     5 |  433 | `		ph7_result_value(pCtx,&pVm->sExceptionCB); /* Will make it's own copy */` |
|     3 |  434 | `	}else{` |
|    15 |  435 | `		ph7_result_null(pCtx);` |
|     - |  436 | `	}` |
|    19 |  437 | `	if( nArg < 1 ){` |
|   ! 0 |  438 | `		return PH7_OK;` |
|     - |  439 | `	}` |
|    19 |  440 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  441 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  442 | `		 * answered TRUE and kept the old handler. */` |
|    15 |  443 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|    15 |  444 | `		if( rcCb != PH7_OK ){` |
|     3 |  445 | `			return rcCb;` |
|     - |  446 | `		}` |
|     5 |  447 | `	}` |
|    24 |  448 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved,` |
|    14 |  449 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],PH7_E_ALL_MASK) ){` |
|     - |  450 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  451 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  452 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  453 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  454 | `	}` |
|    17 |  455 | `	return PH7_OK;` |
|    11 |  456 | `}` |
|     - |  457 | `/*` |
|     - |  458 | ` * bool restore_error_handler(void)` |
|     - |  459 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  460 | ` * Parameters:` |
|     - |  461 | ` *  None.` |
|     - |  462 | ` * Return` |
|     - |  463 | ` *  Always TRUE.` |
|     - |  464 | ` */` |
|  2122 |  465 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  466 | `{` |
|  2127 |  467 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1061 |  468 | `	SXUNUSED(nArg); /* cc warning */` |
|  1061 |  469 | `	SXUNUSED(apArg);` |
|     - |  470 | `	/* The popped handler's $error_levels comes back with it. An empty stack pops` |
|     - |  471 | `	 * to NO handler; php answers TRUE either way, because the return value says` |
|     - |  472 | `	 * "the call is valid", not "a handler was in place". */` |
|  2127 |  473 | `	VmHandlerPop(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved);` |
|  2127 |  474 | `	ph7_result_bool(pCtx,1);` |
|  2127 |  475 | `	return PH7_OK;` |
|     5 |  476 | `}` |
|     - |  477 | `/*` |
|     - |  478 | ` * value set_error_handler(callable $error_handler)` |
|     - |  479 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  480 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  481 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  482 | ` *  Sets a user-defined error handler function.` |
|     - |  483 | ` *  This function can be used for defining your own way of handling errors during` |
|     - |  484 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|     - |  485 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|     - |  486 | ` *  conditions (using trigger_error()).` |
|     - |  487 | ` * Parameters` |
|     - |  488 | ` *  $error_handler` |
|     - |  489 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|     - |  490 | ` *   describing the error.` |
|     - |  491 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|     - |  492 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|     - |  493 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|     - |  494 | ` *   The function can be shown as:` |
|     - |  495 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|     - |  496 | ` *     errno` |
|     - |  497 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|     - |  498 | ` *   errstr` |
|     - |  499 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|     - |  500 | ` *   errfile` |
|     - |  501 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|     - |  502 | ` *     was raised in, as a string.` |
|     - |  503 | ` *  Note:` |
|     - |  504 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  505 | ` * Return` |
|     - |  506 | ` *  The handler that was ACTIVE before this call, or NULL when there was none --` |
|     - |  507 | `` *  including for the `null` reset, which php pushes onto the handler stack like`` |
|     - |  508 | ` *  any other value rather than treating as a failure.` |
|     - |  509 | ` */` |
| 15363 |  510 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  511 | `{` |
| 15368 |  512 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  513 | `	sxi64 iLevels;` |
|     - |  514 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  515 | `	 * saved under it. Returning the saved entry answered the handler from one` |
|     - |  516 | `	 * level further down (and NULL for the very first replacement of a handler` |
|     - |  517 | `	 * that was really there). */` |
| 15368 |  518 | `	if( ph7_value_is_callable(&pVm->sErrCB) ){` |
|  8331 |  519 | `		ph7_result_value(pCtx,&pVm->sErrCB); /* Will make it's own copy */` |
|  4168 |  520 | `	}else{` |
|  7042 |  521 | `		ph7_result_null(pCtx);` |
|     - |  522 | `	}` |
| 15368 |  523 | `	if( nArg < 1 ){` |
|   ! 0 |  524 | `		return PH7_OK;` |
|     - |  525 | `	}` |
|     - |  526 | `	/* $error_levels rides WITH the handler: it is read at full width (php ANDs` |
|     - |  527 | `	 * a zend_long, so 2^32+1024 still selects E_USER_NOTICE) and is pushed and` |
|     - |  528 | `	 * popped with it. */` |
| 15368 |  529 | `	iLevels = nArg > 1 ? ph7_value_to_int64(apArg[1]) : PH7_E_ALL_MASK;` |
| 15368 |  530 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  531 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  532 | `		 * answered TRUE and kept the old handler, so a misspelled handler name` |
|     - |  533 | `		 * left the program reporting through the engine's own path in silence. */` |
|  8866 |  534 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|  8866 |  535 | `		if( rcCb != PH7_OK ){` |
|     5 |  536 | `			return rcCb;` |
|     - |  537 | `		}` |
|  4427 |  538 | `	}` |
|     - |  539 | ``	/* A `null` argument is a real stack entry: it silences the handler until a`` |
|     - |  540 | `	 * restore_error_handler() pops it and brings the previous one -- with ITS` |
|     - |  541 | `	 * levels -- back. */` |
| 23042 |  542 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved,` |
| 15359 |  543 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],iLevels) ){` |
|     - |  544 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  545 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  546 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  547 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  548 | `	}` |
| 15364 |  549 | `	return PH7_OK;` |
|  7685 |  550 | `}` |
|     - |  551 | `/*` |
|     - |  552 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|     - |  553 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|     - |  554 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|     - |  555 | ` *  dispatch paths call), or NULL when none is set.` |
|     - |  556 | ` */` |
|    34 |  557 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  558 | `{` |
|    36 |  559 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  560 | `	SXUNUSED(nArg);` |
|    17 |  561 | `	SXUNUSED(apArg);` |
|    36 |  562 | `	if( ph7_value_is_callable(&pVm->sErrCB) ){` |
|    24 |  563 | `		ph7_result_value(pCtx,&pVm->sErrCB);` |
|    13 |  564 | `	}else{` |
|    14 |  565 | `		ph7_result_null(pCtx);` |
|     - |  566 | `	}` |
|    36 |  567 | `	return PH7_OK;` |
|     2 |  568 | `}` |
|    16 |  569 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  570 | `{` |
|    18 |  571 | `	ph7_vm *pVm = pCtx->pVm;` |
|     8 |  572 | `	SXUNUSED(nArg);` |
|     8 |  573 | `	SXUNUSED(apArg);` |
|    18 |  574 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     8 |  575 | `		ph7_result_value(pCtx,&pVm->sExceptionCB);` |
|     5 |  576 | `	}else{` |
|    12 |  577 | `		ph7_result_null(pCtx);` |
|     - |  578 | `	}` |
|    18 |  579 | `	return PH7_OK;` |
|     2 |  580 | `}` |
|     - |  581 | `/*` |
|     - |  582 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|     - |  583 | ` *  Generates a backtrace.` |
|     - |  584 | ` * Paramaeter` |
|     - |  585 | ` *  $options` |
|     - |  586 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|     - |  587 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|     - |  588 | ` *   all the function/method arguments, to save memory.` |
|     - |  589 | ` * $limit` |
|     - |  590 | ` *   (Not Used)` |
|     - |  591 | ` * Return` |
|     - |  592 | ` *  An array.The possible returned elements are as follows:` |
|     - |  593 | ` *          Possible returned elements from debug_backtrace()` |
|     - |  594 | ` *          Name        Type      Description` |
|     - |  595 | ` *          ------      ------     -----------` |
|     - |  596 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|     - |  597 | ` *          line        integer   The current line number. See also __LINE__.` |
|     - |  598 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|     - |  599 | ` *          class       string    The current class name. See also __CLASS__` |
|     - |  600 | ` *          object      object    The current object.` |
|     - |  601 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|     - |  602 | ` *                                If inside an included file, this lists the included file name(s).` |
|     - |  603 | ` */` |
|     - |  604 | `/*` |
|     - |  605 | ` * array error_get_last()` |
|     - |  606 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|     - |  607 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|     - |  608 | ` *  and never reported an error at all.` |
|     - |  609 | ` */` |
|    80 |  610 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  611 | `{` |
|    84 |  612 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  613 | `	ph7_value *pArray,*pValue;` |
|    40 |  614 | `	SXUNUSED(nArg);` |
|    40 |  615 | `	SXUNUSED(apArg);` |
|    84 |  616 | `	if( pVm->nLastErrType == 0 ){` |
|     - |  617 | `		/* No error yet */` |
|     8 |  618 | `		ph7_result_null(pCtx);` |
|     8 |  619 | `		return PH7_OK;` |
|     - |  620 | `	}` |
|    77 |  621 | `	pArray = ph7_context_new_array(pCtx);` |
|    77 |  622 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    77 |  623 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  624 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  625 | `		ph7_result_null(pCtx);` |
|   ! 0 |  626 | `		return PH7_OK;` |
|     - |  627 | `	}` |
|    77 |  628 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|    77 |  629 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|   114 |  630 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|    74 |  631 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|    77 |  632 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|    77 |  633 | `	ph7_value_reset_string_cursor(pValue);` |
|   114 |  634 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|    74 |  635 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|    77 |  636 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|    77 |  637 | `	ph7_value_reset_string_cursor(pValue);` |
|    77 |  638 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|    77 |  639 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|    77 |  640 | `	ph7_result_value(pCtx,pArray);` |
|    77 |  641 | `	return PH7_OK;` |
|    44 |  642 | `}` |
|     - |  643 | `/*` |
|     - |  644 | ` * void error_clear_last()` |
|     - |  645 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|     - |  646 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|     - |  647 | ` */` |
|     6 |  648 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  649 | `{` |
|     7 |  650 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  651 | `	SXUNUSED(nArg);` |
|     3 |  652 | `	SXUNUSED(apArg);` |
|     7 |  653 | `	pVm->nLastErrType = 0;` |
|     7 |  654 | `	pVm->nLastErrLine = 0;` |
|     7 |  655 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|     7 |  656 | `	SyBlobReset(&pVm->sLastErrFile);` |
|     7 |  657 | `	return PH7_OK;` |
|     1 |  658 | `}` |
|     - |  659 | `/*` |
|     - |  660 | ``  * php's $limit reaches the walk through zend_fetch_debug_backtrace's `int` `` |
|     - |  661 | ` * parameter, a plain narrowing cast -- so PHP_INT_MAX arrives as -1 and reports` |
|     - |  662 | ` * NO frames at all, while 2^32 arrives as 0 and reports every one. Spelled` |
|     - |  663 | ` * through unsigned arithmetic because the two's-complement wrap of an` |
|     - |  664 | ` * out-of-range signed conversion is implementation-defined.` |
|     - |  665 | ` */` |
|   154 |  666 | `static sxi32 VmBacktraceLimit(int nArg,ph7_value **apArg)` |
|     4 |  667 | `{` |
|     - |  668 | `	sxu32 uL;` |
|   158 |  669 | `	if( nArg < 2 \|\| apArg[1] == 0 ){` |
|   107 |  670 | `		return 0;` |
|     - |  671 | `	}` |
|    52 |  672 | `	uL = (sxu32)((sxu64)ph7_value_to_int64(apArg[1]) & 0xFFFFFFFF);` |
|    52 |  673 | `	return (uL <= (sxu32)SXI32_HIGH) ? (sxi32)uL : -(sxi32)(SXU32_HIGH - uL) - 1;` |
|    81 |  674 | `}` |
|   112 |  675 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  676 | `{` |
|   116 |  677 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  678 | `	ph7_value *pList;` |
|     - |  679 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|     - |  680 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|   116 |  681 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|     - |  682 | `	/* $limit: how many frames to report, 0 meaning all of them. It was declared` |
|     - |  683 | `	 * in aBuiltinSig[], screened as an int and then never read, so asking for the` |
|     - |  684 | `	 * caller alone answered the WHOLE stack -- and a program that logs` |
|     - |  685 | `	 * debug_backtrace(0, 1) per request logged the entire chain every time. */` |
|   116 |  686 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|     - |  687 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|     - |  688 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|     - |  689 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|     - |  690 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|   116 |  691 | `	pList = ph7_context_new_array(pCtx);` |
|   116 |  692 | `	if( pList == 0 ){` |
|   ! 0 |  693 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  694 | `		ph7_result_null(pCtx);` |
|   ! 0 |  695 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  696 | `		SXUNUSED(apArg);` |
|   ! 0 |  697 | `		return PH7_OK;` |
|     - |  698 | `	}` |
|   116 |  699 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|     - |  700 | `	/* Return the freshly created list */` |
|   116 |  701 | `	ph7_result_value(pCtx,pList);` |
|     - |  702 | `	/*` |
|     - |  703 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|     - |  704 | `	 * as soon we return from this function.` |
|     - |  705 | `	 */` |
|   116 |  706 | `	return PH7_OK;` |
|    60 |  707 | `}` |
|     - |  708 | `/*` |
|     - |  709 | ` * Generate a small backtrace.` |
|     - |  710 | ` * Store the generated dump in the given BLOB` |
|     - |  711 | ` */` |
|     2 |  712 | `static int VmMiniBacktrace(` |
|     - |  713 | `	ph7_vm *pVm, /* Target VM */` |
|     - |  714 | `	SyBlob *pOut /* Store Dump here */` |
|     - |  715 | `	)` |
|     1 |  716 | `{` |
|     3 |  717 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  718 | `	ph7_vm_func *pFunc;` |
|     - |  719 | `	ph7_class *pClass;` |
|     - |  720 | `	SyString *pFile;` |
|     - |  721 | `	/* Called function */` |
|     3 |  722 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     3 |  723 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     3 |  724 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  725 | `	if( pFrame->pParent && pFunc ){` |
|     3 |  726 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|     3 |  727 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|     2 |  728 | `	}else{` |
|   ! 0 |  729 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|     - |  730 | `	}` |
|     3 |  731 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|     - |  732 | `	/* Current processed script */` |
|     3 |  733 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3 |  734 | `	if( pFile ){` |
|     3 |  735 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  736 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|     3 |  737 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|     3 |  738 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|     1 |  739 | `	}` |
|     - |  740 | `	/* Top class */` |
|     3 |  741 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     3 |  742 | `	if( pClass ){` |
|   ! 0 |  743 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|   ! 0 |  744 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|   ! 0 |  745 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|   ! 0 |  746 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|   ! 0 |  747 | `	}` |
|     3 |  748 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|     - |  749 | `	/* All done */` |
|     3 |  750 | `	return SXRET_OK;` |
|     1 |  751 | `}` |
|     - |  752 | `/*` |
|     - |  753 | ` * void debug_print_backtrace(int $options = 0, int $limit = 0)` |
|     - |  754 | ` *  Prints a backtrace.` |
|     - |  755 | ` *` |
|     - |  756 | ` *  Both arguments were declared in aBuiltinSig[] and read by nothing, and what` |
|     - |  757 | ` *  the function PRINTED was not a backtrace at all: PH7's own` |
|     - |  758 | `` *  `[Called function: f][Processed file: x]` line, describing ONE frame in a`` |
|     - |  759 | ``  *  shape no php ever produced. php prints the same `#N file(line): func(args)` `` |
|     - |  760 | `` *  body getTraceAsString() renders -- without the `#N {main}` marker, which is`` |
|     - |  761 | ` *  the bottom of an exception's trace and not a frame -- and honours the same` |
|     - |  762 | ` *  $options bits and $limit as debug_backtrace().` |
|     - |  763 | ` */` |
|    42 |  764 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  765 | `{` |
|    43 |  766 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  767 | `	SyBlob sDump;` |
|     - |  768 | `	ph7_value *pList;` |
|    43 |  769 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 0;` |
|    43 |  770 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|    43 |  771 | `	pList = ph7_context_new_array(pCtx);` |
|    43 |  772 | `	if( pList == 0 ){` |
|   ! 0 |  773 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  774 | `		return PH7_OK;` |
|     - |  775 | `	}` |
|    43 |  776 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|    43 |  777 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|    43 |  778 | `	PH7_VmTraceToString(pVm,pList,FALSE,&sDump);` |
|     - |  779 | `	/* Output backtrace */` |
|    43 |  780 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     - |  781 | `	/* All done,cleanup */` |
|    43 |  782 | `	SyBlobRelease(&sDump);` |
|    43 |  783 | `	return PH7_OK;` |
|    22 |  784 | `}` |
|     - |  785 | `/*` |
|     - |  786 | ` * string debug_string_backtrace()` |
|     - |  787 | ` *  Generate a backtrace` |
|     - |  788 | ` * Parameters` |
|     - |  789 | ` * None` |
|     - |  790 | ` * Return` |
|     - |  791 | ` *  A mini backtrace().` |
|     - |  792 | ` * Note that this is a symisc extension.` |
|     - |  793 | ` */` |
|     2 |  794 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  795 | `{` |
|     3 |  796 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  797 | `	SyBlob sDump;` |
|     3 |  798 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  799 | `	/* Generate the backtrace */` |
|     3 |  800 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  801 | `	/* Return the backtrace */` |
|     3 |  802 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|     - |  803 | `	/* All done,cleanup */` |
|     3 |  804 | `	SyBlobRelease(&sDump);` |
|     1 |  805 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  806 | `	SXUNUSED(apArg);` |
|     3 |  807 | `	return PH7_OK;` |
|     1 |  808 | `}` |
|     - |  809 |  |
