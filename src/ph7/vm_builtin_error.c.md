# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 265/318 lines (83.33%)

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
|    34 |  131 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  132 | `{` |
|    38 |  133 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|    38 |  134 | `	if( nArg > 0 ){` |
|     - |  135 | `		const char *zErr;` |
|     - |  136 | `		int nLen;` |
|     - |  137 | `		/* Extract the error message */` |
|    38 |  138 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|    38 |  139 | `		if( nArg > 1 ){` |
|     - |  140 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|     - |  141 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|     - |  142 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|     - |  143 | `			 * translated engine severity). */` |
|    38 |  144 | `			nErr = ph7_value_to_int(apArg[1]);` |
|    38 |  145 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|   ! 0 |  146 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  147 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|     - |  148 | `			}` |
|    38 |  149 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|     - |  150 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|     3 |  151 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  152 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|     - |  153 | `			}` |
|    16 |  154 | `		}` |
|     - |  155 | `		/* Report error (consults an installed error handler, then displays) */` |
|    36 |  156 | `		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|    32 |  157 | `		if( nErr == 256 /* E_USER_ERROR */` |
|    20 |  158 | `		 && !ph7_value_is_callable(&pCtx->pVm->aErrCB[1]) ){` |
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
|    36 |  169 | `		ph7_result_bool(pCtx,1);` |
|    20 |  170 | `	}else{` |
|     - |  171 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  172 | `		ph7_result_bool(pCtx,0);` |
|     - |  173 | `	}` |
|    36 |  174 | `	return PH7_OK;` |
|    21 |  175 | `}` |
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
|   204 |  191 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  192 | `{` |
|   209 |  193 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  194 | `	int nOld;` |
|     - |  195 | `	/* Extract the old reporting level */` |
|   209 |  196 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|   209 |  197 | `	if( pVm->nErrSuppress > 0 ){` |
|     - |  198 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|     - |  199 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|     - |  200 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|     - |  201 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|     - |  202 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|    60 |  203 | `		nOld &= 4437;` |
|    29 |  204 | `	}` |
|   209 |  205 | `	if( nArg > 0 ){` |
|     - |  206 | `		int nNew;` |
|     - |  207 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|    67 |  208 | `		nNew = ph7_value_to_int(apArg[0]);` |
|    67 |  209 | `		pVm->iErrMask = (sxi32)nNew;` |
|    67 |  210 | `		pVm->bErrReport = nNew != 0;` |
|    31 |  211 | `	}` |
|     - |  212 | `	/* Return the old level */` |
|   209 |  213 | `	ph7_result_int(pCtx,nOld);` |
|   209 |  214 | `	return PH7_OK;` |
|     5 |  215 | `}` |
|     - |  216 | `/*` |
|     - |  217 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|     - |  218 | ` *  Send an error message somewhere.` |
|     - |  219 | ` * Parameter` |
|     - |  220 | ` *  $message` |
|     - |  221 | ` *   The error message that should be logged.` |
|     - |  222 | ` *  $message_type` |
|     - |  223 | ` *   Says where the error should go. The possible message types are as follows:` |
|     - |  224 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|     - |  225 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|     - |  226 | ` *       This is the default option.` |
|     - |  227 | ` *    1 message is sent by email to the address in the destination parameter.` |
|     - |  228 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|     - |  229 | ` *    2  No longer an option.` |
|     - |  230 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|     - |  231 | ` *       to the end of the message string.` |
|     - |  232 | ` *    4  message is sent directly to the SAPI logging handler.` |
|     - |  233 | ` *  $destination` |
|     - |  234 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|     - |  235 | ` *  $extra_headers` |
|     - |  236 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|     - |  237 | ` * Return` |
|     - |  238 | ` *  TRUE on success or FALSE on failure.` |
|     - |  239 | ` *` |
|     - |  240 | ` * This used to invoke the PH7_VM_CONFIG_ERR_LOG_HANDLER embedder callback and` |
|     - |  241 | ` * NOTHING else: with no callback installed — which is every CLI run — the whole` |
|     - |  242 | `` * function was a no-op that answered TRUE. `error_log($msg)` printed nothing,`` |
|     - |  243 | `` * `error_log($msg, 3, $file)` wrote no file and still said it had, and a`` |
|     - |  244 | ` * destination that could not be opened answered TRUE as well. Every "log this` |
|     - |  245 | ` * and carry on" call in a program silently disappeared, which is the one thing` |
|     - |  246 | ` * a logging call must not do.` |
|     - |  247 | ` *` |
|     - |  248 | ` * php's own routing, which is what it does now: type 3 APPENDS the message` |
|     - |  249 | ` * verbatim to $destination (no newline added, no timestamp) and answers FALSE` |
|     - |  250 | ` * with the open warning when it cannot; type 2 is php's ValueError; type 1 is` |
|     - |  251 | ` * mail(), which this engine has no transport for, so it answers FALSE rather` |
|     - |  252 | ` * than claiming a delivery; and every other value — 0, 4, and anything` |
|     - |  253 | ` * unrecognised, all of which php sends to the SAPI logger — writes the message` |
|     - |  254 | ` * plus a newline to the diagnostics stream. The embedder callback still wins` |
|     - |  255 | ` * when one is installed: that is what it is for.` |
|     - |  256 | ` */` |
|    12 |  257 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  258 | `{` |
|     - |  259 | `	const char *zMessage,*zDest,*zHeader;` |
|    14 |  260 | `	ph7_vm *pVm = pCtx->pVm;` |
|    14 |  261 | `	int iType = 0, nMsg = 0, nDest = 0;` |
|    14 |  262 | `	if( nArg < 1 ){` |
|     - |  263 | `		/* Missing log message,return FALSE */` |
|   ! 0 |  264 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  265 | `		return PH7_OK;` |
|     - |  266 | `	}` |
|    14 |  267 | `	zMessage = ph7_value_to_string(apArg[0],&nMsg);` |
|    14 |  268 | `	zDest = zHeader = ""; /* Empty string */` |
|    14 |  269 | `	if( nArg > 1 ){` |
|    12 |  270 | `		iType = ph7_value_to_int(apArg[1]);` |
|     5 |  271 | `	}` |
|    14 |  272 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     9 |  273 | `		zDest = ph7_value_to_string(apArg[2],&nDest);` |
|     4 |  274 | `	}` |
|    14 |  275 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|   ! 0 |  276 | `		zHeader = ph7_value_to_string(apArg[3],0);` |
|   ! 0 |  277 | `	}` |
|    14 |  278 | `	if( iType == 2 ){` |
|     - |  279 | `		/* php removed the TCP/IP destination in 8.0 and refuses the type outright. */` |
|     3 |  280 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  281 | `			"TCP/IP option is not available for error logging");` |
|     - |  282 | `	}` |
|    12 |  283 | `	if( pVm->xErrLog ){` |
|     - |  284 | `		/* An embedder took the routing over (PH7_VM_CONFIG_ERR_LOG_HANDLER). */` |
|   ! 0 |  285 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|   ! 0 |  286 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 |  287 | `		return PH7_OK;` |
|     - |  288 | `	}` |
|    12 |  289 | `	if( iType == 1 ){` |
|     - |  290 | `		/* mail(): no transport in this engine, so the message did NOT go out.` |
|     - |  291 | `		 * Answering TRUE for it was the old no-op's worst face. */` |
|   ! 0 |  292 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  293 | `		return PH7_OK;` |
|     - |  294 | `	}` |
|     - |  295 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    12 |  296 | `	if( iType == 3 ){` |
|     - |  297 | `		/* Appended VERBATIM: php adds neither a newline nor a timestamp here. */` |
|     7 |  298 | `		if( PH7_VfsAppendFile(pCtx,zDest,zMessage,nMsg) != PH7_OK ){` |
|     3 |  299 | `			ph7_result_bool(pCtx,0);` |
|     3 |  300 | `			return PH7_OK;` |
|     - |  301 | `		}` |
|     5 |  302 | `		ph7_result_bool(pCtx,1);` |
|     5 |  303 | `		return PH7_OK;` |
|     - |  304 | `	}` |
|     - |  305 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - |  306 | `	/* 0 (the configured log), 4 (the SAPI logger) and every other value: the` |
|     - |  307 | `	 * diagnostics stream, message plus a newline — php's CLI shape with no` |
|     - |  308 | ``	 * `error_log` ini set. */`` |
|     - |  309 | `	{` |
|     7 |  310 | `		ph7_output_consumer *pCons = pVm->sVmErrConsumer.xConsumer` |
|     4 |  311 | `			? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|     - |  312 | `		SyBlob sOut;` |
|     5 |  313 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|     5 |  314 | `		SyBlobAppend(&sOut,zMessage,(sxu32)nMsg);` |
|     5 |  315 | `		SyBlobAppend(&sOut,"\n",sizeof(char));` |
|     5 |  316 | `		pCons->xConsumer(SyBlobData(&sOut),SyBlobLength(&sOut),pCons->pUserData);` |
|     5 |  317 | `		SyBlobRelease(&sOut);` |
|     - |  318 | `	}` |
|     5 |  319 | `	ph7_result_bool(pCtx,1);` |
|     5 |  320 | `	return PH7_OK;` |
|     8 |  321 | `}` |
|     - |  322 | `/*` |
|     - |  323 | ` * bool restore_exception_handler(void)` |
|     - |  324 | ` *  Restores the previously defined exception handler function.` |
|     - |  325 | ` * Parameter` |
|     - |  326 | ` *  None` |
|     - |  327 | ` * Return` |
|     - |  328 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|     - |  329 | ` */` |
|     4 |  330 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  331 | `{` |
|     5 |  332 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  333 | `	ph7_value *pOld,*pNew;` |
|     - |  334 | `	/* Point to the old and the new handler */` |
|     5 |  335 | `	pOld = &pVm->aExceptionCB[0];` |
|     5 |  336 | `	pNew = &pVm->aExceptionCB[1];` |
|     2 |  337 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  338 | `	SXUNUSED(apArg);` |
|     5 |  339 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|     - |  340 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|     - |  341 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|     - |  342 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|     - |  343 | `		 * single set_error_handler().` |
|     - |  344 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|     - |  345 | `		 * "a handler was in place". */` |
|     5 |  346 | `		PH7_MemObjRelease(pNew);` |
|     5 |  347 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|     5 |  348 | `		ph7_result_bool(pCtx,1);` |
|     5 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* Copy the old handler */` |
|   ! 0 |  352 | `	PH7_MemObjStore(pOld,pNew);` |
|   ! 0 |  353 | `	PH7_MemObjRelease(pOld);` |
|     - |  354 | `	/* Return TRUE */` |
|   ! 0 |  355 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  356 | `	return PH7_OK;` |
|     3 |  357 | `}` |
|     - |  358 | `/*` |
|     - |  359 | ` * callable set_exception_handler(callable $exception_handler)` |
|     - |  360 | ` *  Sets a user-defined exception handler function.` |
|     - |  361 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|     - |  362 | ` * NOTE` |
|     - |  363 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|     - |  364 | ` *  the satndard PHP engine.` |
|     - |  365 | ` * Parameters` |
|     - |  366 | ` *  $exception_handler` |
|     - |  367 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|     - |  368 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|     - |  369 | ` *   that was thrown.` |
|     - |  370 | ` *  Note:` |
|     - |  371 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  372 | ` * Return` |
|     - |  373 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|     - |  374 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  375 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  376 | ` */` |
|     6 |  377 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  378 | `{` |
|     8 |  379 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  380 | `	ph7_value *pOld,*pNew;` |
|     - |  381 | `	/* Point to the old and the new handler */` |
|     8 |  382 | `	pOld = &pVm->aExceptionCB[0];` |
|     8 |  383 | `	pNew = &pVm->aExceptionCB[1];` |
|     - |  384 | `	/* Return the old handler */` |
|     8 |  385 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8 |  386 | `	if( nArg > 0 ){` |
|     8 |  387 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|     - |  388 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|   ! 0 |  389 | `			PH7_MemObjRelease(pNew);` |
|   ! 0 |  390 | `			ph7_result_bool(pCtx,1);` |
|   ! 0 |  391 | `		}else{` |
|     8 |  392 | `			PH7_MemObjStore(pNew,pOld);` |
|     - |  393 | `			/* Install the new handler */` |
|     8 |  394 | `			PH7_MemObjStore(apArg[0],pNew);` |
|     - |  395 | `		}` |
|     3 |  396 | `	}` |
|     8 |  397 | `	return PH7_OK;` |
|     2 |  398 | `}` |
|     - |  399 | `/*` |
|     - |  400 | ` * bool restore_error_handler(void)` |
|     - |  401 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  402 | ` * Parameters:` |
|     - |  403 | ` *  None.` |
|     - |  404 | ` * Return` |
|     - |  405 | ` *  Always TRUE.` |
|     - |  406 | ` */` |
|   230 |  407 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  408 | `{` |
|   235 |  409 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  410 | `	ph7_value *pOld,*pNew;` |
|     - |  411 | `	/* Point to the old and the new handler */` |
|   235 |  412 | `	pOld = &pVm->aErrCB[0];` |
|   235 |  413 | `	pNew = &pVm->aErrCB[1];` |
|   115 |  414 | `	SXUNUSED(nArg); /* cc warning */` |
|   115 |  415 | `	SXUNUSED(apArg);` |
|   235 |  416 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|     - |  417 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|     - |  418 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|     - |  419 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|     - |  420 | `		 * single set_error_handler().` |
|     - |  421 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|     - |  422 | `		 * "a handler was in place". */` |
|    32 |  423 | `		PH7_MemObjRelease(pNew);` |
|    32 |  424 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|    32 |  425 | `		ph7_result_bool(pCtx,1);` |
|    32 |  426 | `		return PH7_OK;` |
|     - |  427 | `	}` |
|     - |  428 | `	/* Copy the old callback */` |
|   203 |  429 | `	PH7_MemObjStore(pOld,pNew);` |
|   203 |  430 | `	PH7_MemObjRelease(pOld);` |
|     - |  431 | `	/* Return TRUE */` |
|   203 |  432 | `	ph7_result_bool(pCtx,1);` |
|   203 |  433 | `	return PH7_OK;` |
|   120 |  434 | `}` |
|     - |  435 | `/*` |
|     - |  436 | ` * value set_error_handler(callable $error_handler)` |
|     - |  437 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  438 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  439 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  440 | ` *  Sets a user-defined error handler function.` |
|     - |  441 | ` *  This function can be used for defining your own way of handling errors during` |
|     - |  442 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|     - |  443 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|     - |  444 | ` *  conditions (using trigger_error()).` |
|     - |  445 | ` * Parameters` |
|     - |  446 | ` *  $error_handler` |
|     - |  447 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|     - |  448 | ` *   describing the error.` |
|     - |  449 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|     - |  450 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|     - |  451 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|     - |  452 | ` *   The function can be shown as:` |
|     - |  453 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|     - |  454 | ` *     errno` |
|     - |  455 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|     - |  456 | ` *   errstr` |
|     - |  457 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|     - |  458 | ` *   errfile` |
|     - |  459 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|     - |  460 | ` *     was raised in, as a string.` |
|     - |  461 | ` *  Note:` |
|     - |  462 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  463 | ` * Return` |
|     - |  464 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|     - |  465 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  466 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  467 | ` */` |
| 11160 |  468 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  469 | `{` |
| 11165 |  470 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  471 | `	ph7_value *pOld,*pNew;` |
|     - |  472 | `	/* Point to the old and the new handler */` |
| 11165 |  473 | `	pOld = &pVm->aErrCB[0];` |
| 11165 |  474 | `	pNew = &pVm->aErrCB[1];` |
|     - |  475 | `	/* Return the old handler */` |
| 11165 |  476 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
| 11165 |  477 | `	if( nArg > 0 ){` |
| 11165 |  478 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|     - |  479 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|  5439 |  480 | `			PH7_MemObjRelease(pNew);` |
|  5439 |  481 | `			ph7_result_bool(pCtx,1);` |
|  2720 |  482 | `		}else{` |
|  5727 |  483 | `			PH7_MemObjStore(pNew,pOld);` |
|     - |  484 | `			/* Install the new handler */` |
|  5727 |  485 | `			PH7_MemObjStore(apArg[0],pNew);` |
|     - |  486 | `		}` |
|  5580 |  487 | `	}` |
| 11165 |  488 | `	return PH7_OK;` |
|     5 |  489 | `}` |
|     - |  490 | `/*` |
|     - |  491 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|     - |  492 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|     - |  493 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|     - |  494 | ` *  dispatch paths call), or NULL when none is set.` |
|     - |  495 | ` */` |
|     8 |  496 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  497 | `{` |
|     9 |  498 | `	ph7_vm *pVm = pCtx->pVm;` |
|     4 |  499 | `	SXUNUSED(nArg);` |
|     4 |  500 | `	SXUNUSED(apArg);` |
|     9 |  501 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|     5 |  502 | `		ph7_result_value(pCtx,&pVm->aErrCB[1]);` |
|     3 |  503 | `	}else{` |
|     5 |  504 | `		ph7_result_null(pCtx);` |
|     - |  505 | `	}` |
|     9 |  506 | `	return PH7_OK;` |
|     1 |  507 | `}` |
|     4 |  508 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  509 | `{` |
|     5 |  510 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 |  511 | `	SXUNUSED(nArg);` |
|     2 |  512 | `	SXUNUSED(apArg);` |
|     5 |  513 | `	if( ph7_value_is_callable(&pVm->aExceptionCB[1]) ){` |
|     3 |  514 | `		ph7_result_value(pCtx,&pVm->aExceptionCB[1]);` |
|     2 |  515 | `	}else{` |
|     3 |  516 | `		ph7_result_null(pCtx);` |
|     - |  517 | `	}` |
|     5 |  518 | `	return PH7_OK;` |
|     1 |  519 | `}` |
|     - |  520 | `/*` |
|     - |  521 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|     - |  522 | ` *  Generates a backtrace.` |
|     - |  523 | ` * Paramaeter` |
|     - |  524 | ` *  $options` |
|     - |  525 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|     - |  526 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|     - |  527 | ` *   all the function/method arguments, to save memory.` |
|     - |  528 | ` * $limit` |
|     - |  529 | ` *   (Not Used)` |
|     - |  530 | ` * Return` |
|     - |  531 | ` *  An array.The possible returned elements are as follows:` |
|     - |  532 | ` *          Possible returned elements from debug_backtrace()` |
|     - |  533 | ` *          Name        Type      Description` |
|     - |  534 | ` *          ------      ------     -----------` |
|     - |  535 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|     - |  536 | ` *          line        integer   The current line number. See also __LINE__.` |
|     - |  537 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|     - |  538 | ` *          class       string    The current class name. See also __CLASS__` |
|     - |  539 | ` *          object      object    The current object.` |
|     - |  540 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|     - |  541 | ` *                                If inside an included file, this lists the included file name(s).` |
|     - |  542 | ` */` |
|     - |  543 | `/*` |
|     - |  544 | ` * array error_get_last()` |
|     - |  545 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|     - |  546 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|     - |  547 | ` *  and never reported an error at all.` |
|     - |  548 | ` */` |
|    10 |  549 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  550 | `{` |
|    13 |  551 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  552 | `	ph7_value *pArray,*pValue;` |
|     5 |  553 | `	SXUNUSED(nArg);` |
|     5 |  554 | `	SXUNUSED(apArg);` |
|    13 |  555 | `	if( pVm->nLastErrType == 0 ){` |
|     - |  556 | `		/* No error yet */` |
|     8 |  557 | `		ph7_result_null(pCtx);` |
|     8 |  558 | `		return PH7_OK;` |
|     - |  559 | `	}` |
|     6 |  560 | `	pArray = ph7_context_new_array(pCtx);` |
|     6 |  561 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     6 |  562 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  563 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  564 | `		ph7_result_null(pCtx);` |
|   ! 0 |  565 | `		return PH7_OK;` |
|     - |  566 | `	}` |
|     6 |  567 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|     6 |  568 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|     8 |  569 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|     4 |  570 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|     6 |  571 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|     6 |  572 | `	ph7_value_reset_string_cursor(pValue);` |
|     8 |  573 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|     4 |  574 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|     6 |  575 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     6 |  576 | `	ph7_value_reset_string_cursor(pValue);` |
|     6 |  577 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|     6 |  578 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|     6 |  579 | `	ph7_result_value(pCtx,pArray);` |
|     6 |  580 | `	return PH7_OK;` |
|     8 |  581 | `}` |
|     - |  582 | `/*` |
|     - |  583 | ` * void error_clear_last()` |
|     - |  584 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|     - |  585 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|     - |  586 | ` */` |
|     6 |  587 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  588 | `{` |
|     7 |  589 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  590 | `	SXUNUSED(nArg);` |
|     3 |  591 | `	SXUNUSED(apArg);` |
|     7 |  592 | `	pVm->nLastErrType = 0;` |
|     7 |  593 | `	pVm->nLastErrLine = 0;` |
|     7 |  594 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|     7 |  595 | `	SyBlobReset(&pVm->sLastErrFile);` |
|     7 |  596 | `	return PH7_OK;` |
|     1 |  597 | `}` |
|    16 |  598 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  599 | `{` |
|    19 |  600 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  601 | `	ph7_value *pList;` |
|     - |  602 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|     - |  603 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|    19 |  604 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|     - |  605 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|     - |  606 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|     - |  607 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|     - |  608 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|    19 |  609 | `	pList = ph7_context_new_array(pCtx);` |
|    19 |  610 | `	if( pList == 0 ){` |
|   ! 0 |  611 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  612 | `		ph7_result_null(pCtx);` |
|   ! 0 |  613 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  614 | `		SXUNUSED(apArg);` |
|   ! 0 |  615 | `		return PH7_OK;` |
|     - |  616 | `	}` |
|    19 |  617 | `	VmBuildBacktrace(&(*pVm),iOptions,pList);` |
|     - |  618 | `	/* Return the freshly created list */` |
|    19 |  619 | `	ph7_result_value(pCtx,pList);` |
|     - |  620 | `	/*` |
|     - |  621 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|     - |  622 | `	 * as soon we return from this function.` |
|     - |  623 | `	 */` |
|    19 |  624 | `	return PH7_OK;` |
|    11 |  625 | `}` |
|     - |  626 | `/*` |
|     - |  627 | ` * Generate a small backtrace.` |
|     - |  628 | ` * Store the generated dump in the given BLOB` |
|     - |  629 | ` */` |
|     4 |  630 | `static int VmMiniBacktrace(` |
|     - |  631 | `	ph7_vm *pVm, /* Target VM */` |
|     - |  632 | `	SyBlob *pOut /* Store Dump here */` |
|     - |  633 | `	)` |
|     1 |  634 | `{` |
|     5 |  635 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  636 | `	ph7_vm_func *pFunc;` |
|     - |  637 | `	ph7_class *pClass;` |
|     - |  638 | `	SyString *pFile;` |
|     - |  639 | `	/* Called function */` |
|     5 |  640 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     5 |  641 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     5 |  642 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|     5 |  643 | `	if( pFrame->pParent && pFunc ){` |
|     5 |  644 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|     5 |  645 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|     3 |  646 | `	}else{` |
|   ! 0 |  647 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|     - |  648 | `	}` |
|     5 |  649 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|     - |  650 | `	/* Current processed script */` |
|     5 |  651 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     5 |  652 | `	if( pFile ){` |
|     5 |  653 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|     5 |  654 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|     5 |  655 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|     5 |  656 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|     2 |  657 | `	}` |
|     - |  658 | `	/* Top class */` |
|     5 |  659 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     5 |  660 | `	if( pClass ){` |
|   ! 0 |  661 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|   ! 0 |  662 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|   ! 0 |  663 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|   ! 0 |  664 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|   ! 0 |  665 | `	}` |
|     5 |  666 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|     - |  667 | `	/* All done */` |
|     5 |  668 | `	return SXRET_OK;` |
|     1 |  669 | `}` |
|     - |  670 | `/*` |
|     - |  671 | ` * void debug_print_backtrace()` |
|     - |  672 | ` *  Prints a backtrace` |
|     - |  673 | ` * Parameters` |
|     - |  674 | ` * None` |
|     - |  675 | ` * Return` |
|     - |  676 | ` * NULL` |
|     - |  677 | ` */` |
|     2 |  678 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  679 | `{` |
|     3 |  680 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  681 | `	SyBlob sDump;` |
|     3 |  682 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  683 | `	/* Generate the backtrace */` |
|     3 |  684 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  685 | `	/* Output backtrace */` |
|     3 |  686 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     - |  687 | `	/* All done,cleanup */` |
|     3 |  688 | `	SyBlobRelease(&sDump);` |
|     1 |  689 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  690 | `	SXUNUSED(apArg);` |
|     3 |  691 | `	return PH7_OK;` |
|     1 |  692 | `}` |
|     - |  693 | `/*` |
|     - |  694 | ` * string debug_string_backtrace()` |
|     - |  695 | ` *  Generate a backtrace` |
|     - |  696 | ` * Parameters` |
|     - |  697 | ` * None` |
|     - |  698 | ` * Return` |
|     - |  699 | ` *  A mini backtrace().` |
|     - |  700 | ` * Note that this is a symisc extension.` |
|     - |  701 | ` */` |
|     2 |  702 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  703 | `{` |
|     3 |  704 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  705 | `	SyBlob sDump;` |
|     3 |  706 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  707 | `	/* Generate the backtrace */` |
|     3 |  708 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  709 | `	/* Return the backtrace */` |
|     3 |  710 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|     - |  711 | `	/* All done,cleanup */` |
|     3 |  712 | `	SyBlobRelease(&sDump);` |
|     1 |  713 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  714 | `	SXUNUSED(apArg);` |
|     3 |  715 | `	return PH7_OK;` |
|     1 |  716 | `}` |
|     - |  717 |  |
