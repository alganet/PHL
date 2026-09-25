# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 300/356 lines (84.27%)

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
|   110 |  131 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  132 | `{` |
|   114 |  133 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|   114 |  134 | `	if( nArg > 0 ){` |
|     - |  135 | `		const char *zErr;` |
|     - |  136 | `		int nLen;` |
|     - |  137 | `		/* Extract the error message */` |
|   114 |  138 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|   114 |  139 | `		if( nArg > 1 ){` |
|     - |  140 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|     - |  141 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|     - |  142 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|     - |  143 | `			 * translated engine severity). */` |
|   114 |  144 | `			nErr = ph7_value_to_int(apArg[1]);` |
|   114 |  145 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|   ! 0 |  146 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  147 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|     - |  148 | `			}` |
|   114 |  149 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|     - |  150 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|     3 |  151 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  152 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|     - |  153 | `			}` |
|    54 |  154 | `		}` |
|     - |  155 | `		/* Report error (consults an installed error handler, then displays) */` |
|   112 |  156 | `		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|   108 |  157 | `		if( nErr == 256 /* E_USER_ERROR */` |
|    58 |  158 | `		 && !ph7_value_is_callable(&pCtx->pVm->sErrCB) ){` |
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
|   112 |  169 | `		ph7_result_bool(pCtx,1);` |
|    58 |  170 | `	}else{` |
|     - |  171 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  172 | `		ph7_result_bool(pCtx,0);` |
|     - |  173 | `	}` |
|   112 |  174 | `	return PH7_OK;` |
|    59 |  175 | `}` |
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
|   266 |  191 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  192 | `{` |
|   271 |  193 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  194 | `	int nOld;` |
|     - |  195 | `	/* Extract the old reporting level */` |
|   271 |  196 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|   271 |  197 | `	if( pVm->nErrSuppress > 0 ){` |
|     - |  198 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|     - |  199 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|     - |  200 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|     - |  201 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|     - |  202 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|    76 |  203 | `		nOld &= 4437;` |
|    36 |  204 | `	}` |
|   271 |  205 | `	if( nArg > 0 ){` |
|     - |  206 | `		int nNew;` |
|     - |  207 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|    67 |  208 | `		nNew = ph7_value_to_int(apArg[0]);` |
|    67 |  209 | `		pVm->iErrMask = (sxi32)nNew;` |
|    67 |  210 | `		pVm->bErrReport = nNew != 0;` |
|    31 |  211 | `	}` |
|     - |  212 | `	/* Return the old level */` |
|   271 |  213 | `	ph7_result_int(pCtx,nOld);` |
|   271 |  214 | `	return PH7_OK;` |
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
|     - |  323 | ` * php's set_error_handler()/set_exception_handler() stack, both directions.` |
|     - |  324 | ` *` |
|     - |  325 | `` * PH7 kept ONE saved slot per handler, so a third `set` overwrote the first`` |
|     - |  326 | `` * one's save and the matching `restore` could not find it again; and a NULL`` |
|     - |  327 | ` * argument was treated as a failure instead of the ordinary stack entry php` |
|     - |  328 | ` * pushes for it. Both routines below are shared by the error and the exception` |
|     - |  329 | ` * handler because php implements them the same way.` |
|     - |  330 | ` */` |
| 11762 |  331 | `static sxi32 VmHandlerPush(` |
|     - |  332 | `	ph7_vm *pVm,` |
|     - |  333 | `	ph7_value *pActive,   /* the currently installed handler */` |
|     - |  334 | `	sxi64 *piLevels,      /* its $error_levels (unused by the exception handler) */` |
|     - |  335 | `	SySet *pStack,        /* the saved entries underneath it */` |
|     - |  336 | ``	ph7_value *pNewCb,    /* the replacement, or 0 for the `null` reset */`` |
|     - |  337 | `	sxi64 iLevels` |
|     - |  338 | `	)` |
|     5 |  339 | `{` |
|     - |  340 | `	VmHandlerSlot sSlot;` |
|     - |  341 | `	sxi32 rc;` |
| 11767 |  342 | `	PH7_MemObjInit(pVm,&sSlot.sCb);` |
| 11767 |  343 | `	PH7_MemObjStore(pActive,&sSlot.sCb);` |
| 11767 |  344 | `	sSlot.iLevels = *piLevels;` |
| 11767 |  345 | `	rc = SySetPut(pStack,(const void *)&sSlot);` |
| 11767 |  346 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  347 | `		PH7_MemObjRelease(&sSlot.sCb);` |
|   ! 0 |  348 | `		return rc;` |
|     - |  349 | `	}` |
| 11767 |  350 | `	PH7_MemObjRelease(pActive);` |
| 11767 |  351 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
| 11767 |  352 | `	if( pNewCb ){` |
|  6207 |  353 | `		PH7_MemObjStore(pNewCb,pActive);` |
|  3101 |  354 | `	}` |
| 11767 |  355 | `	*piLevels = iLevels;` |
| 11767 |  356 | `	return SXRET_OK;` |
|  5886 |  357 | `}` |
|   588 |  358 | `static void VmHandlerPop(ph7_vm *pVm,ph7_value *pActive,sxi64 *piLevels,SySet *pStack)` |
|     5 |  359 | `{` |
|   593 |  360 | `	VmHandlerSlot *pSlot = (VmHandlerSlot *)SySetPop(pStack);` |
|   593 |  361 | `	PH7_MemObjRelease(pActive);` |
|   593 |  362 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
|   593 |  363 | `	*piLevels = PH7_E_ALL_MASK;` |
|   593 |  364 | `	if( pSlot ){` |
|   589 |  365 | `		PH7_MemObjStore(&pSlot->sCb,pActive);` |
|   589 |  366 | `		*piLevels = pSlot->iLevels;` |
|   589 |  367 | `		PH7_MemObjRelease(&pSlot->sCb);` |
|   292 |  368 | `	}` |
|   294 |  369 | `	SXUNUSED(pVm);` |
|   593 |  370 | `}` |
|     - |  371 | `/*` |
|     - |  372 | ` * bool restore_exception_handler(void)` |
|     - |  373 | ` *  Restores the previously defined exception handler function.` |
|     - |  374 | ` * Parameter` |
|     - |  375 | ` *  None` |
|     - |  376 | ` * Return` |
|     - |  377 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|     - |  378 | ` */` |
|    12 |  379 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  380 | `{` |
|    13 |  381 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  382 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     6 |  383 | `	SXUNUSED(nArg); /* cc warning */` |
|     6 |  384 | `	SXUNUSED(apArg);` |
|     - |  385 | `	/* An empty stack pops to NO handler: php answers TRUE either way, because the` |
|     - |  386 | `	 * return value says "the call is valid", not "a handler was in place". */` |
|    13 |  387 | `	VmHandlerPop(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved);` |
|    13 |  388 | `	ph7_result_bool(pCtx,1);` |
|    13 |  389 | `	return PH7_OK;` |
|     1 |  390 | `}` |
|     - |  391 | `/*` |
|     - |  392 | ` * callable set_exception_handler(callable $exception_handler)` |
|     - |  393 | ` *  Sets a user-defined exception handler function.` |
|     - |  394 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|     - |  395 | ` * NOTE` |
|     - |  396 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|     - |  397 | ` *  the satndard PHP engine.` |
|     - |  398 | ` * Parameters` |
|     - |  399 | ` *  $exception_handler` |
|     - |  400 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|     - |  401 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|     - |  402 | ` *   that was thrown.` |
|     - |  403 | ` *  Note:` |
|     - |  404 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  405 | ` * Return` |
|     - |  406 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|     - |  407 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  408 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  409 | ` */` |
|    16 |  410 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  411 | `{` |
|    19 |  412 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  413 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     - |  414 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  415 | `	 * saved under it. */` |
|    19 |  416 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     5 |  417 | `		ph7_result_value(pCtx,&pVm->sExceptionCB); /* Will make it's own copy */` |
|     3 |  418 | `	}else{` |
|    15 |  419 | `		ph7_result_null(pCtx);` |
|     - |  420 | `	}` |
|    19 |  421 | `	if( nArg < 1 ){` |
|   ! 0 |  422 | `		return PH7_OK;` |
|     - |  423 | `	}` |
|    19 |  424 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  425 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  426 | `		 * answered TRUE and kept the old handler. */` |
|    15 |  427 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|    15 |  428 | `		if( rcCb != PH7_OK ){` |
|     3 |  429 | `			return rcCb;` |
|     - |  430 | `		}` |
|     5 |  431 | `	}` |
|    24 |  432 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved,` |
|    14 |  433 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],PH7_E_ALL_MASK) ){` |
|     - |  434 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  435 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  436 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  437 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  438 | `	}` |
|    17 |  439 | `	return PH7_OK;` |
|    11 |  440 | `}` |
|     - |  441 | `/*` |
|     - |  442 | ` * bool restore_error_handler(void)` |
|     - |  443 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  444 | ` * Parameters:` |
|     - |  445 | ` *  None.` |
|     - |  446 | ` * Return` |
|     - |  447 | ` *  Always TRUE.` |
|     - |  448 | ` */` |
|   576 |  449 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  450 | `{` |
|   581 |  451 | `	ph7_vm *pVm = pCtx->pVm;` |
|   288 |  452 | `	SXUNUSED(nArg); /* cc warning */` |
|   288 |  453 | `	SXUNUSED(apArg);` |
|     - |  454 | `	/* The popped handler's $error_levels comes back with it. An empty stack pops` |
|     - |  455 | `	 * to NO handler; php answers TRUE either way, because the return value says` |
|     - |  456 | `	 * "the call is valid", not "a handler was in place". */` |
|   581 |  457 | `	VmHandlerPop(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved);` |
|   581 |  458 | `	ph7_result_bool(pCtx,1);` |
|   581 |  459 | `	return PH7_OK;` |
|     5 |  460 | `}` |
|     - |  461 | `/*` |
|     - |  462 | ` * value set_error_handler(callable $error_handler)` |
|     - |  463 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  464 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  465 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  466 | ` *  Sets a user-defined error handler function.` |
|     - |  467 | ` *  This function can be used for defining your own way of handling errors during` |
|     - |  468 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|     - |  469 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|     - |  470 | ` *  conditions (using trigger_error()).` |
|     - |  471 | ` * Parameters` |
|     - |  472 | ` *  $error_handler` |
|     - |  473 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|     - |  474 | ` *   describing the error.` |
|     - |  475 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|     - |  476 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|     - |  477 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|     - |  478 | ` *   The function can be shown as:` |
|     - |  479 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|     - |  480 | ` *     errno` |
|     - |  481 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|     - |  482 | ` *   errstr` |
|     - |  483 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|     - |  484 | ` *   errfile` |
|     - |  485 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|     - |  486 | ` *     was raised in, as a string.` |
|     - |  487 | ` *  Note:` |
|     - |  488 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  489 | ` * Return` |
|     - |  490 | ` *  The handler that was ACTIVE before this call, or NULL when there was none --` |
|     - |  491 | `` *  including for the `null` reset, which php pushes onto the handler stack like`` |
|     - |  492 | ` *  any other value rather than treating as a failure.` |
|     - |  493 | ` */` |
| 11752 |  494 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  495 | `{` |
| 11757 |  496 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  497 | `	sxi64 iLevels;` |
|     - |  498 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  499 | `	 * saved under it. Returning the saved entry answered the handler from one` |
|     - |  500 | `	 * level further down (and NULL for the very first replacement of a handler` |
|     - |  501 | `	 * that was really there). */` |
| 11757 |  502 | `	if( ph7_value_is_callable(&pVm->sErrCB) ){` |
|  6079 |  503 | `		ph7_result_value(pCtx,&pVm->sErrCB); /* Will make it's own copy */` |
|  3041 |  504 | `	}else{` |
|  5681 |  505 | `		ph7_result_null(pCtx);` |
|     - |  506 | `	}` |
| 11757 |  507 | `	if( nArg < 1 ){` |
|   ! 0 |  508 | `		return PH7_OK;` |
|     - |  509 | `	}` |
|     - |  510 | `	/* $error_levels rides WITH the handler: it is read at full width (php ANDs` |
|     - |  511 | `	 * a zend_long, so 2^32+1024 still selects E_USER_NOTICE) and is pushed and` |
|     - |  512 | `	 * popped with it. */` |
| 11757 |  513 | `	iLevels = nArg > 1 ? ph7_value_to_int64(apArg[1]) : PH7_E_ALL_MASK;` |
| 11757 |  514 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  515 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  516 | `		 * answered TRUE and kept the old handler, so a misspelled handler name` |
|     - |  517 | `		 * left the program reporting through the engine's own path in silence. */` |
|  6201 |  518 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|  6201 |  519 | `		if( rcCb != PH7_OK ){` |
|     5 |  520 | `			return rcCb;` |
|     - |  521 | `		}` |
|  3096 |  522 | `	}` |
|     - |  523 | ``	/* A `null` argument is a real stack entry: it silences the handler until a`` |
|     - |  524 | `	 * restore_error_handler() pops it and brings the previous one -- with ITS` |
|     - |  525 | `	 * levels -- back. */` |
| 17627 |  526 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved,` |
| 11748 |  527 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],iLevels) ){` |
|     - |  528 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  529 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  530 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  531 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  532 | `	}` |
| 11753 |  533 | `	return PH7_OK;` |
|  5881 |  534 | `}` |
|     - |  535 | `/*` |
|     - |  536 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|     - |  537 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|     - |  538 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|     - |  539 | ` *  dispatch paths call), or NULL when none is set.` |
|     - |  540 | ` */` |
|    34 |  541 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  542 | `{` |
|    36 |  543 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  544 | `	SXUNUSED(nArg);` |
|    17 |  545 | `	SXUNUSED(apArg);` |
|    36 |  546 | `	if( ph7_value_is_callable(&pVm->sErrCB) ){` |
|    24 |  547 | `		ph7_result_value(pCtx,&pVm->sErrCB);` |
|    13 |  548 | `	}else{` |
|    14 |  549 | `		ph7_result_null(pCtx);` |
|     - |  550 | `	}` |
|    36 |  551 | `	return PH7_OK;` |
|     2 |  552 | `}` |
|    16 |  553 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  554 | `{` |
|    18 |  555 | `	ph7_vm *pVm = pCtx->pVm;` |
|     8 |  556 | `	SXUNUSED(nArg);` |
|     8 |  557 | `	SXUNUSED(apArg);` |
|    18 |  558 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     8 |  559 | `		ph7_result_value(pCtx,&pVm->sExceptionCB);` |
|     5 |  560 | `	}else{` |
|    12 |  561 | `		ph7_result_null(pCtx);` |
|     - |  562 | `	}` |
|    18 |  563 | `	return PH7_OK;` |
|     2 |  564 | `}` |
|     - |  565 | `/*` |
|     - |  566 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|     - |  567 | ` *  Generates a backtrace.` |
|     - |  568 | ` * Paramaeter` |
|     - |  569 | ` *  $options` |
|     - |  570 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|     - |  571 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|     - |  572 | ` *   all the function/method arguments, to save memory.` |
|     - |  573 | ` * $limit` |
|     - |  574 | ` *   (Not Used)` |
|     - |  575 | ` * Return` |
|     - |  576 | ` *  An array.The possible returned elements are as follows:` |
|     - |  577 | ` *          Possible returned elements from debug_backtrace()` |
|     - |  578 | ` *          Name        Type      Description` |
|     - |  579 | ` *          ------      ------     -----------` |
|     - |  580 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|     - |  581 | ` *          line        integer   The current line number. See also __LINE__.` |
|     - |  582 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|     - |  583 | ` *          class       string    The current class name. See also __CLASS__` |
|     - |  584 | ` *          object      object    The current object.` |
|     - |  585 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|     - |  586 | ` *                                If inside an included file, this lists the included file name(s).` |
|     - |  587 | ` */` |
|     - |  588 | `/*` |
|     - |  589 | ` * array error_get_last()` |
|     - |  590 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|     - |  591 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|     - |  592 | ` *  and never reported an error at all.` |
|     - |  593 | ` */` |
|    10 |  594 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  595 | `{` |
|    12 |  596 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  597 | `	ph7_value *pArray,*pValue;` |
|     5 |  598 | `	SXUNUSED(nArg);` |
|     5 |  599 | `	SXUNUSED(apArg);` |
|    12 |  600 | `	if( pVm->nLastErrType == 0 ){` |
|     - |  601 | `		/* No error yet */` |
|     8 |  602 | `		ph7_result_null(pCtx);` |
|     8 |  603 | `		return PH7_OK;` |
|     - |  604 | `	}` |
|     5 |  605 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  606 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     5 |  607 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  608 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  609 | `		ph7_result_null(pCtx);` |
|   ! 0 |  610 | `		return PH7_OK;` |
|     - |  611 | `	}` |
|     5 |  612 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|     5 |  613 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|     7 |  614 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|     4 |  615 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|     5 |  616 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|     5 |  617 | `	ph7_value_reset_string_cursor(pValue);` |
|     7 |  618 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|     4 |  619 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|     5 |  620 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     5 |  621 | `	ph7_value_reset_string_cursor(pValue);` |
|     5 |  622 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|     5 |  623 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|     5 |  624 | `	ph7_result_value(pCtx,pArray);` |
|     5 |  625 | `	return PH7_OK;` |
|     7 |  626 | `}` |
|     - |  627 | `/*` |
|     - |  628 | ` * void error_clear_last()` |
|     - |  629 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|     - |  630 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|     - |  631 | ` */` |
|     6 |  632 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  633 | `{` |
|     7 |  634 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  635 | `	SXUNUSED(nArg);` |
|     3 |  636 | `	SXUNUSED(apArg);` |
|     7 |  637 | `	pVm->nLastErrType = 0;` |
|     7 |  638 | `	pVm->nLastErrLine = 0;` |
|     7 |  639 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|     7 |  640 | `	SyBlobReset(&pVm->sLastErrFile);` |
|     7 |  641 | `	return PH7_OK;` |
|     1 |  642 | `}` |
|     - |  643 | `/*` |
|     - |  644 | ``  * php's $limit reaches the walk through zend_fetch_debug_backtrace's `int` `` |
|     - |  645 | ` * parameter, a plain narrowing cast -- so PHP_INT_MAX arrives as -1 and reports` |
|     - |  646 | ` * NO frames at all, while 2^32 arrives as 0 and reports every one. Spelled` |
|     - |  647 | ` * through unsigned arithmetic because the two's-complement wrap of an` |
|     - |  648 | ` * out-of-range signed conversion is implementation-defined.` |
|     - |  649 | ` */` |
|    96 |  650 | `static sxi32 VmBacktraceLimit(int nArg,ph7_value **apArg)` |
|     3 |  651 | `{` |
|     - |  652 | `	sxu32 uL;` |
|    99 |  653 | `	if( nArg < 2 \|\| apArg[1] == 0 ){` |
|    51 |  654 | `		return 0;` |
|     - |  655 | `	}` |
|    49 |  656 | `	uL = (sxu32)((sxu64)ph7_value_to_int64(apArg[1]) & 0xFFFFFFFF);` |
|    49 |  657 | `	return (uL <= (sxu32)SXI32_HIGH) ? (sxi32)uL : -(sxi32)(SXU32_HIGH - uL) - 1;` |
|    51 |  658 | `}` |
|    54 |  659 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  660 | `{` |
|    57 |  661 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  662 | `	ph7_value *pList;` |
|     - |  663 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|     - |  664 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|    57 |  665 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|     - |  666 | `	/* $limit: how many frames to report, 0 meaning all of them. It was declared` |
|     - |  667 | `	 * in aBuiltinSig[], screened as an int and then never read, so asking for the` |
|     - |  668 | `	 * caller alone answered the WHOLE stack -- and a program that logs` |
|     - |  669 | `	 * debug_backtrace(0, 1) per request logged the entire chain every time. */` |
|    57 |  670 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|     - |  671 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|     - |  672 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|     - |  673 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|     - |  674 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|    57 |  675 | `	pList = ph7_context_new_array(pCtx);` |
|    57 |  676 | `	if( pList == 0 ){` |
|   ! 0 |  677 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  678 | `		ph7_result_null(pCtx);` |
|   ! 0 |  679 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  680 | `		SXUNUSED(apArg);` |
|   ! 0 |  681 | `		return PH7_OK;` |
|     - |  682 | `	}` |
|    57 |  683 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|     - |  684 | `	/* Return the freshly created list */` |
|    57 |  685 | `	ph7_result_value(pCtx,pList);` |
|     - |  686 | `	/*` |
|     - |  687 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|     - |  688 | `	 * as soon we return from this function.` |
|     - |  689 | `	 */` |
|    57 |  690 | `	return PH7_OK;` |
|    30 |  691 | `}` |
|     - |  692 | `/*` |
|     - |  693 | ` * Generate a small backtrace.` |
|     - |  694 | ` * Store the generated dump in the given BLOB` |
|     - |  695 | ` */` |
|     2 |  696 | `static int VmMiniBacktrace(` |
|     - |  697 | `	ph7_vm *pVm, /* Target VM */` |
|     - |  698 | `	SyBlob *pOut /* Store Dump here */` |
|     - |  699 | `	)` |
|     1 |  700 | `{` |
|     3 |  701 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  702 | `	ph7_vm_func *pFunc;` |
|     - |  703 | `	ph7_class *pClass;` |
|     - |  704 | `	SyString *pFile;` |
|     - |  705 | `	/* Called function */` |
|     3 |  706 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     3 |  707 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     3 |  708 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  709 | `	if( pFrame->pParent && pFunc ){` |
|     3 |  710 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|     3 |  711 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|     2 |  712 | `	}else{` |
|   ! 0 |  713 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|     - |  714 | `	}` |
|     3 |  715 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|     - |  716 | `	/* Current processed script */` |
|     3 |  717 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3 |  718 | `	if( pFile ){` |
|     3 |  719 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  720 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|     3 |  721 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|     3 |  722 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|     1 |  723 | `	}` |
|     - |  724 | `	/* Top class */` |
|     3 |  725 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     3 |  726 | `	if( pClass ){` |
|   ! 0 |  727 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|   ! 0 |  728 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|   ! 0 |  729 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|   ! 0 |  730 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|   ! 0 |  731 | `	}` |
|     3 |  732 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|     - |  733 | `	/* All done */` |
|     3 |  734 | `	return SXRET_OK;` |
|     1 |  735 | `}` |
|     - |  736 | `/*` |
|     - |  737 | ` * void debug_print_backtrace(int $options = 0, int $limit = 0)` |
|     - |  738 | ` *  Prints a backtrace.` |
|     - |  739 | ` *` |
|     - |  740 | ` *  Both arguments were declared in aBuiltinSig[] and read by nothing, and what` |
|     - |  741 | ` *  the function PRINTED was not a backtrace at all: PH7's own` |
|     - |  742 | `` *  `[Called function: f][Processed file: x]` line, describing ONE frame in a`` |
|     - |  743 | ``  *  shape no php ever produced. php prints the same `#N file(line): func(args)` `` |
|     - |  744 | `` *  body getTraceAsString() renders -- without the `#N {main}` marker, which is`` |
|     - |  745 | ` *  the bottom of an exception's trace and not a frame -- and honours the same` |
|     - |  746 | ` *  $options bits and $limit as debug_backtrace().` |
|     - |  747 | ` */` |
|    42 |  748 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  749 | `{` |
|    43 |  750 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  751 | `	SyBlob sDump;` |
|     - |  752 | `	ph7_value *pList;` |
|    43 |  753 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 0;` |
|    43 |  754 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|    43 |  755 | `	pList = ph7_context_new_array(pCtx);` |
|    43 |  756 | `	if( pList == 0 ){` |
|   ! 0 |  757 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  758 | `		return PH7_OK;` |
|     - |  759 | `	}` |
|    43 |  760 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|    43 |  761 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|    43 |  762 | `	PH7_VmTraceToString(pVm,pList,FALSE,&sDump);` |
|     - |  763 | `	/* Output backtrace */` |
|    43 |  764 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     - |  765 | `	/* All done,cleanup */` |
|    43 |  766 | `	SyBlobRelease(&sDump);` |
|    43 |  767 | `	return PH7_OK;` |
|    22 |  768 | `}` |
|     - |  769 | `/*` |
|     - |  770 | ` * string debug_string_backtrace()` |
|     - |  771 | ` *  Generate a backtrace` |
|     - |  772 | ` * Parameters` |
|     - |  773 | ` * None` |
|     - |  774 | ` * Return` |
|     - |  775 | ` *  A mini backtrace().` |
|     - |  776 | ` * Note that this is a symisc extension.` |
|     - |  777 | ` */` |
|     2 |  778 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  779 | `{` |
|     3 |  780 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  781 | `	SyBlob sDump;` |
|     3 |  782 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  783 | `	/* Generate the backtrace */` |
|     3 |  784 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  785 | `	/* Return the backtrace */` |
|     3 |  786 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|     - |  787 | `	/* All done,cleanup */` |
|     3 |  788 | `	SyBlobRelease(&sDump);` |
|     1 |  789 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  790 | `	SXUNUSED(apArg);` |
|     3 |  791 | `	return PH7_OK;` |
|     1 |  792 | `}` |
|     - |  793 |  |
