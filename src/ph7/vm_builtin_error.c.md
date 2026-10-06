# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 318/375 lines (84.80%)

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
|   144 |  131 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  132 | `{` |
|   149 |  133 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|   149 |  134 | `	if( nArg > 0 ){` |
|     - |  135 | `		const char *zErr;` |
|     - |  136 | `		int nLen;` |
|     - |  137 | `		/* Extract the error message */` |
|   149 |  138 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|   149 |  139 | `		if( nArg > 1 ){` |
|     - |  140 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|     - |  141 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|     - |  142 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|     - |  143 | `			 * translated engine severity). */` |
|   147 |  144 | `			nErr = ph7_value_to_int(apArg[1]);` |
|   147 |  145 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|   ! 0 |  146 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  147 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|     - |  148 | `			}` |
|   147 |  149 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|     - |  150 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|     3 |  151 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  152 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|     - |  153 | `			}` |
|    70 |  154 | `		}` |
|   147 |  155 | `		if( PH7_VmPreludeBuiltinFrame(pCtx->pVm,0,0) ){` |
|     - |  156 | `			/* Raised by the body of a builtin written as embedded PHP (hex2bin,` |
|     - |  157 | `			 * glob, array_count_values, tempnam). php's is a C function raising` |
|     - |  158 | `			 * its own diagnostic: the ENGINE level, not the user one a handler` |
|     - |  159 | `			 * and error_reporting() see -- E_WARNING where this said` |
|     - |  160 | `			 * E_USER_WARNING -- and no trigger_error frame in the trace. The` |
|     - |  161 | `			 * frame test answers only when the innermost activation is such a` |
|     - |  162 | `			 * body, so a user callback reached from one keeps the user level. */` |
|    40 |  163 | `			ph7_vm *pVm = pCtx->pVm;` |
|    40 |  164 | `			VmNativeCall *pNat = pVm->pNativeCall;` |
|    40 |  165 | `			SyString *pSavedCallee = pVm->pCalleeName;` |
|    40 |  166 | `			switch( nErr ){` |
|    36 |  167 | `			case 512:   nErr = 2;    break; /* E_WARNING */` |
|     5 |  168 | `			case 1024:  nErr = 8;    break; /* E_NOTICE */` |
|   ! 0 |  169 | `			case 16384: nErr = 8192; break; /* E_DEPRECATED */` |
|     - |  170 | `			}` |
|    40 |  171 | `			if( pNat && pNat->pName == &pCtx->pFunc->sName ){` |
|    40 |  172 | `				pNat->bElided = 1;` |
|    18 |  173 | `			}` |
|     - |  174 | `			/* A user error handler is still entered by an internal function, so its` |
|     - |  175 | `			 * frame carries no call site -- but the internal frame above it is the` |
|     - |  176 | `			 * builtin's own activation, which the walk lists anyway. */` |
|    40 |  177 | `			pVm->pCalleeName = 0;` |
|    40 |  178 | `			PH7_VmThrowError(pVm, NULL, nErr, zErr);` |
|    40 |  179 | `			pVm->pCalleeName = pSavedCallee;` |
|    22 |  180 | `		}else{` |
|     - |  181 | `			/* Report error (consults an installed error handler, then displays) */` |
|   110 |  182 | `			PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|     - |  183 | `		}` |
|   142 |  184 | `		if( nErr == 256 /* E_USER_ERROR */` |
|    76 |  185 | `		 && ph7_value_is_null(&pCtx->pVm->sErrCB) ){` |
|     - |  186 | `			/* php: an unhandled user fatal halts with exit 255 (pre-fix the` |
|     - |  187 | `			 * PH7_ABORT here was overwritten by the throw's status, so` |
|     - |  188 | `			 * E_USER_ERROR silently CONTINUED). With a handler installed the` |
|     - |  189 | `			 * script continues — the handler-returned-false renormalization is` |
|     - |  190 | `			 * a recorded nuance. No php stack trace either (recorded). */` |
|   ! 0 |  191 | `			pCtx->pVm->iExitStatus = 255;` |
|   ! 0 |  192 | `			pCtx->pVm->bHaltRequested = 1;` |
|   ! 0 |  193 | `			return PH7_ABORT;` |
|     - |  194 | `		}` |
|     - |  195 | `		/* Return true */` |
|   147 |  196 | `		ph7_result_bool(pCtx,1);` |
|    76 |  197 | `	}else{` |
|     - |  198 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  199 | `		ph7_result_bool(pCtx,0);` |
|     - |  200 | `	}` |
|   147 |  201 | `	return PH7_OK;` |
|    77 |  202 | `}` |
|     - |  203 | `/*` |
|     - |  204 | ` * int error_reporting([int $level])` |
|     - |  205 | ` *  Sets which PHP errors are reported.` |
|     - |  206 | ` * Parameters` |
|     - |  207 | ` *  $level` |
|     - |  208 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|     - |  209 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|     - |  210 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|     - |  211 | ` *   levels will not always behave as expected.` |
|     - |  212 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|     - |  213 | ` *   in the predefined constants.` |
|     - |  214 | ` * Return` |
|     - |  215 | ` *   Returns the old error_reporting level or the current level if no level` |
|     - |  216 | ` *   parameter is given.` |
|     - |  217 | ` */` |
|  1142 |  218 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  219 | `{` |
|  1147 |  220 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  221 | `	int nOld;` |
|     - |  222 | `	/* Extract the old reporting level */` |
|  1147 |  223 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|  1147 |  224 | `	if( pVm->nErrSuppress > 0 ){` |
|     - |  225 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|     - |  226 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|     - |  227 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|     - |  228 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|     - |  229 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|   433 |  230 | `		nOld &= 4437;` |
|   214 |  231 | `	}` |
|  1147 |  232 | `	if( nArg > 0 ){` |
|     - |  233 | `		int nNew;` |
|     - |  234 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|   200 |  235 | `		nNew = ph7_value_to_int(apArg[0]);` |
|   200 |  236 | `		pVm->iErrMask = (sxi32)nNew;` |
|   200 |  237 | `		pVm->bErrReport = nNew != 0;` |
|   200 |  238 | `		pVm->bErrMaskSet = 1;` |
|    97 |  239 | `	}` |
|     - |  240 | `	/* Return the old level */` |
|  1147 |  241 | `	ph7_result_int(pCtx,nOld);` |
|  1147 |  242 | `	return PH7_OK;` |
|     5 |  243 | `}` |
|     - |  244 | `/*` |
|     - |  245 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|     - |  246 | ` *  Send an error message somewhere.` |
|     - |  247 | ` * Parameter` |
|     - |  248 | ` *  $message` |
|     - |  249 | ` *   The error message that should be logged.` |
|     - |  250 | ` *  $message_type` |
|     - |  251 | ` *   Says where the error should go. The possible message types are as follows:` |
|     - |  252 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|     - |  253 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|     - |  254 | ` *       This is the default option.` |
|     - |  255 | ` *    1 message is sent by email to the address in the destination parameter.` |
|     - |  256 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|     - |  257 | ` *    2  No longer an option.` |
|     - |  258 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|     - |  259 | ` *       to the end of the message string.` |
|     - |  260 | ` *    4  message is sent directly to the SAPI logging handler.` |
|     - |  261 | ` *  $destination` |
|     - |  262 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|     - |  263 | ` *  $extra_headers` |
|     - |  264 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|     - |  265 | ` * Return` |
|     - |  266 | ` *  TRUE on success or FALSE on failure.` |
|     - |  267 | ` *` |
|     - |  268 | ` * This used to invoke the PH7_VM_CONFIG_ERR_LOG_HANDLER embedder callback and` |
|     - |  269 | ` * NOTHING else: with no callback installed — which is every CLI run — the whole` |
|     - |  270 | `` * function was a no-op that answered TRUE. `error_log($msg)` printed nothing,`` |
|     - |  271 | `` * `error_log($msg, 3, $file)` wrote no file and still said it had, and a`` |
|     - |  272 | ` * destination that could not be opened answered TRUE as well. Every "log this` |
|     - |  273 | ` * and carry on" call in a program silently disappeared, which is the one thing` |
|     - |  274 | ` * a logging call must not do.` |
|     - |  275 | ` *` |
|     - |  276 | ` * php's own routing, which is what it does now: type 3 APPENDS the message` |
|     - |  277 | ` * verbatim to $destination (no newline added, no timestamp) and answers FALSE` |
|     - |  278 | ` * with the open warning when it cannot; type 2 is php's ValueError; type 1 is` |
|     - |  279 | ` * mail(), which this engine has no transport for, so it answers FALSE rather` |
|     - |  280 | ` * than claiming a delivery; 0 and anything unrecognised go to the CONFIGURED` |
|     - |  281 | `` * logger (the `error_log` destination, timestamped, falling back to the`` |
|     - |  282 | ` * diagnostics stream when it is unset or will not open); and 4 is the SAPI` |
|     - |  283 | ` * logger itself, which is the diagnostics stream and never the file. The embedder callback still wins` |
|     - |  284 | ` * when one is installed: that is what it is for.` |
|     - |  285 | ` */` |
|    26 |  286 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  287 | `{` |
|     - |  288 | `	const char *zMessage,*zDest,*zHeader;` |
|    29 |  289 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  290 | `	int iType = 0, nMsg = 0, nDest = 0;` |
|    29 |  291 | `	if( nArg < 1 ){` |
|     - |  292 | `		/* Missing log message,return FALSE */` |
|   ! 0 |  293 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  294 | `		return PH7_OK;` |
|     - |  295 | `	}` |
|    29 |  296 | `	zMessage = ph7_value_to_string(apArg[0],&nMsg);` |
|    29 |  297 | `	zDest = zHeader = ""; /* Empty string */` |
|    29 |  298 | `	if( nArg > 1 ){` |
|    27 |  299 | `		iType = ph7_value_to_int(apArg[1]);` |
|    12 |  300 | `	}` |
|    29 |  301 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    14 |  302 | `		zDest = ph7_value_to_string(apArg[2],&nDest);` |
|     6 |  303 | `	}` |
|    29 |  304 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|   ! 0 |  305 | `		zHeader = ph7_value_to_string(apArg[3],0);` |
|   ! 0 |  306 | `	}` |
|    29 |  307 | `	if( iType == 2 ){` |
|     - |  308 | `		/* php removed the TCP/IP destination in 8.0 and refuses the type outright. */` |
|     3 |  309 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  310 | `			"TCP/IP option is not available for error logging");` |
|     - |  311 | `	}` |
|    27 |  312 | `	if( pVm->xErrLog ){` |
|     - |  313 | `		/* An embedder took the routing over (PH7_VM_CONFIG_ERR_LOG_HANDLER). */` |
|   ! 0 |  314 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|   ! 0 |  315 | `		ph7_result_bool(pCtx,1);` |
|   ! 0 |  316 | `		return PH7_OK;` |
|     - |  317 | `	}` |
|    27 |  318 | `	if( iType == 1 ){` |
|     - |  319 | `		/* mail(): no transport in this engine, so the message did NOT go out.` |
|     - |  320 | `		 * Answering TRUE for it was the old no-op's worst face. */` |
|   ! 0 |  321 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  322 | `		return PH7_OK;` |
|     - |  323 | `	}` |
|     - |  324 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    27 |  325 | `	if( iType == 3 ){` |
|     - |  326 | `		/* Appended VERBATIM: php adds neither a newline nor a timestamp here. */` |
|    12 |  327 | `		if( PH7_VfsAppendFile(pCtx,zDest,zMessage,nMsg) != PH7_OK ){` |
|     3 |  328 | `			ph7_result_bool(pCtx,0);` |
|     3 |  329 | `			return PH7_OK;` |
|     - |  330 | `		}` |
|    10 |  331 | `		ph7_result_bool(pCtx,1);` |
|    10 |  332 | `		return PH7_OK;` |
|     - |  333 | `	}` |
|     - |  334 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - |  335 | `	/* 0 and every unrecognised value go to the CONFIGURED logger, which is the` |
|     - |  336 | ``	 * `error_log` destination when one is set: php appends the message there`` |
|     - |  337 | ``	 * behind its `[d-M-Y H:i:s e] ` timestamp. Routing them to the diagnostics`` |
|     - |  338 | `	 * stream unconditionally is what made a program's own log calls miss the file` |
|     - |  339 | `	 * it had just named. 4 is the SAPI logger by definition and never the file, so` |
|     - |  340 | ``	 * `error_log($m, 4)` still reaches the stream with the destination set — that`` |
|     - |  341 | `	 * is the whole difference between the two types.` |
|     - |  342 | `	 *` |
|     - |  343 | `	 * php answers TRUE either way, INCLUDING when the destination will not open:` |
|     - |  344 | `	 * the fallback to the stream is a successful log as far as the caller is` |
|     - |  345 | `	 * concerned, and only type 3, which names its own file, reports a failure. */` |
|    16 |  346 | `	if( iType != 4 && PH7_VmErrorLogToFile(pVm,zMessage,(sxu32)nMsg) ){` |
|     5 |  347 | `		ph7_result_bool(pCtx,1);` |
|     5 |  348 | `		return PH7_OK;` |
|     - |  349 | `	}` |
|     - |  350 | `	/* The diagnostics stream, message plus a newline — php's CLI shape with no` |
|     - |  351 | ``	 * `error_log` ini set, and the SAPI logger type 4 always takes. */`` |
|     - |  352 | `	{` |
|    17 |  353 | `		ph7_output_consumer *pCons = pVm->sVmErrConsumer.xConsumer` |
|    10 |  354 | `			? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|     - |  355 | `		SyBlob sOut;` |
|    12 |  356 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|    12 |  357 | `		SyBlobAppend(&sOut,zMessage,(sxu32)nMsg);` |
|    12 |  358 | `		SyBlobAppend(&sOut,"\n",sizeof(char));` |
|    12 |  359 | `		pCons->xConsumer(SyBlobData(&sOut),SyBlobLength(&sOut),pCons->pUserData);` |
|    12 |  360 | `		SyBlobRelease(&sOut);` |
|     - |  361 | `	}` |
|    12 |  362 | `	ph7_result_bool(pCtx,1);` |
|    12 |  363 | `	return PH7_OK;` |
|    16 |  364 | `}` |
|     - |  365 | `/*` |
|     - |  366 | ` * php's set_error_handler()/set_exception_handler() stack, both directions.` |
|     - |  367 | ` *` |
|     - |  368 | `` * PH7 kept ONE saved slot per handler, so a third `set` overwrote the first`` |
|     - |  369 | `` * one's save and the matching `restore` could not find it again; and a NULL`` |
|     - |  370 | ` * argument was treated as a failure instead of the ordinary stack entry php` |
|     - |  371 | ` * pushes for it. Both routines below are shared by the error and the exception` |
|     - |  372 | ` * handler because php implements them the same way.` |
|     - |  373 | ` */` |
| 15987 |  374 | `static sxi32 VmHandlerPush(` |
|     - |  375 | `	ph7_vm *pVm,` |
|     - |  376 | `	ph7_value *pActive,   /* the currently installed handler */` |
|     - |  377 | `	sxi64 *piLevels,      /* its $error_levels (unused by the exception handler) */` |
|     - |  378 | `	SySet *pStack,        /* the saved entries underneath it */` |
|     - |  379 | ``	ph7_value *pNewCb,    /* the replacement, or 0 for the `null` reset */`` |
|     - |  380 | `	sxi64 iLevels` |
|     - |  381 | `	)` |
|     5 |  382 | `{` |
|     - |  383 | `	VmHandlerSlot sSlot;` |
|     - |  384 | `	sxi32 rc;` |
| 15992 |  385 | `	PH7_MemObjInit(pVm,&sSlot.sCb);` |
| 15992 |  386 | `	PH7_MemObjStore(pActive,&sSlot.sCb);` |
| 15992 |  387 | `	sSlot.iLevels = *piLevels;` |
| 15992 |  388 | `	rc = SySetPut(pStack,(const void *)&sSlot);` |
| 15992 |  389 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  390 | `		PH7_MemObjRelease(&sSlot.sCb);` |
|   ! 0 |  391 | `		return rc;` |
|     - |  392 | `	}` |
| 15992 |  393 | `	PH7_MemObjRelease(pActive);` |
| 15992 |  394 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
| 15992 |  395 | `	if( pNewCb ){` |
|  9244 |  396 | `		PH7_MemObjStore(pNewCb,pActive);` |
|  4618 |  397 | `	}` |
| 15992 |  398 | `	*piLevels = iLevels;` |
| 15992 |  399 | `	return SXRET_OK;` |
|  7997 |  400 | `}` |
|  2184 |  401 | `static void VmHandlerPop(ph7_vm *pVm,ph7_value *pActive,sxi64 *piLevels,SySet *pStack)` |
|     5 |  402 | `{` |
|  2189 |  403 | `	VmHandlerSlot *pSlot = (VmHandlerSlot *)SySetPop(pStack);` |
|  2189 |  404 | `	PH7_MemObjRelease(pActive);` |
|  2189 |  405 | `	MemObjSetType(pActive,MEMOBJ_NULL);` |
|  2189 |  406 | `	*piLevels = PH7_E_ALL_MASK;` |
|  2189 |  407 | `	if( pSlot ){` |
|  2185 |  408 | `		PH7_MemObjStore(&pSlot->sCb,pActive);` |
|  2185 |  409 | `		*piLevels = pSlot->iLevels;` |
|  2185 |  410 | `		PH7_MemObjRelease(&pSlot->sCb);` |
|  1090 |  411 | `	}` |
|  1092 |  412 | `	SXUNUSED(pVm);` |
|  2189 |  413 | `}` |
|     - |  414 | `/*` |
|     - |  415 | ` * bool restore_exception_handler(void)` |
|     - |  416 | ` *  Restores the previously defined exception handler function.` |
|     - |  417 | ` * Parameter` |
|     - |  418 | ` *  None` |
|     - |  419 | ` * Return` |
|     - |  420 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|     - |  421 | ` */` |
|    14 |  422 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  423 | `{` |
|    15 |  424 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  425 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     7 |  426 | `	SXUNUSED(nArg); /* cc warning */` |
|     7 |  427 | `	SXUNUSED(apArg);` |
|     - |  428 | `	/* An empty stack pops to NO handler: php answers TRUE either way, because the` |
|     - |  429 | `	 * return value says "the call is valid", not "a handler was in place". */` |
|    15 |  430 | `	VmHandlerPop(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved);` |
|    15 |  431 | `	ph7_result_bool(pCtx,1);` |
|    15 |  432 | `	return PH7_OK;` |
|     1 |  433 | `}` |
|     - |  434 | `/*` |
|     - |  435 | ` * callable set_exception_handler(callable $exception_handler)` |
|     - |  436 | ` *  Sets a user-defined exception handler function.` |
|     - |  437 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|     - |  438 | ` * NOTE` |
|     - |  439 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|     - |  440 | ` *  the satndard PHP engine.` |
|     - |  441 | ` * Parameters` |
|     - |  442 | ` *  $exception_handler` |
|     - |  443 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|     - |  444 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|     - |  445 | ` *   that was thrown.` |
|     - |  446 | ` *  Note:` |
|     - |  447 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  448 | ` * Return` |
|     - |  449 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|     - |  450 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  451 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  452 | ` */` |
|    18 |  453 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  454 | `{` |
|    21 |  455 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  456 | `	sxi64 iLevels = PH7_E_ALL_MASK;` |
|     - |  457 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  458 | `	 * saved under it. */` |
|    21 |  459 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     5 |  460 | `		ph7_result_value(pCtx,&pVm->sExceptionCB); /* Will make it's own copy */` |
|     3 |  461 | `	}else{` |
|    17 |  462 | `		ph7_result_null(pCtx);` |
|     - |  463 | `	}` |
|    21 |  464 | `	if( nArg < 1 ){` |
|   ! 0 |  465 | `		return PH7_OK;` |
|     - |  466 | `	}` |
|    21 |  467 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  468 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  469 | `		 * answered TRUE and kept the old handler. */` |
|    17 |  470 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|    17 |  471 | `		if( rcCb != PH7_OK ){` |
|     3 |  472 | `			return rcCb;` |
|     - |  473 | `		}` |
|     6 |  474 | `	}` |
|    27 |  475 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved,` |
|    16 |  476 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],PH7_E_ALL_MASK) ){` |
|     - |  477 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  478 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  479 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  480 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  481 | `	}` |
|    19 |  482 | `	return PH7_OK;` |
|    12 |  483 | `}` |
|     - |  484 | `/*` |
|     - |  485 | ` * bool restore_error_handler(void)` |
|     - |  486 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  487 | ` * Parameters:` |
|     - |  488 | ` *  None.` |
|     - |  489 | ` * Return` |
|     - |  490 | ` *  Always TRUE.` |
|     - |  491 | ` */` |
|  2170 |  492 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  493 | `{` |
|  2175 |  494 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1085 |  495 | `	SXUNUSED(nArg); /* cc warning */` |
|  1085 |  496 | `	SXUNUSED(apArg);` |
|     - |  497 | `	/* The popped handler's $error_levels comes back with it. An empty stack pops` |
|     - |  498 | `	 * to NO handler; php answers TRUE either way, because the return value says` |
|     - |  499 | `	 * "the call is valid", not "a handler was in place". */` |
|  2175 |  500 | `	VmHandlerPop(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved);` |
|  2175 |  501 | `	ph7_result_bool(pCtx,1);` |
|  2175 |  502 | `	return PH7_OK;` |
|     5 |  503 | `}` |
|     - |  504 | `/*` |
|     - |  505 | ` * value set_error_handler(callable $error_handler)` |
|     - |  506 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  507 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  508 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  509 | ` *  Sets a user-defined error handler function.` |
|     - |  510 | ` *  This function can be used for defining your own way of handling errors during` |
|     - |  511 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|     - |  512 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|     - |  513 | ` *  conditions (using trigger_error()).` |
|     - |  514 | ` * Parameters` |
|     - |  515 | ` *  $error_handler` |
|     - |  516 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|     - |  517 | ` *   describing the error.` |
|     - |  518 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|     - |  519 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|     - |  520 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|     - |  521 | ` *   The function can be shown as:` |
|     - |  522 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|     - |  523 | ` *     errno` |
|     - |  524 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|     - |  525 | ` *   errstr` |
|     - |  526 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|     - |  527 | ` *   errfile` |
|     - |  528 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|     - |  529 | ` *     was raised in, as a string.` |
|     - |  530 | ` *  Note:` |
|     - |  531 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  532 | ` * Return` |
|     - |  533 | ` *  The handler that was ACTIVE before this call, or NULL when there was none --` |
|     - |  534 | `` *  including for the `null` reset, which php pushes onto the handler stack like`` |
|     - |  535 | ` *  any other value rather than treating as a failure.` |
|     - |  536 | ` */` |
| 15975 |  537 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  538 | `{` |
| 15980 |  539 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  540 | `	sxi64 iLevels;` |
|     - |  541 | `	/* php answers the handler this call REPLACES -- the active one, not the entry` |
|     - |  542 | `	 * saved under it. Returning the saved entry answered the handler from one` |
|     - |  543 | `	 * level further down (and NULL for the very first replacement of a handler` |
|     - |  544 | `	 * that was really there). */` |
| 15980 |  545 | `	if( !ph7_value_is_null(&pVm->sErrCB) ){` |
|  8641 |  546 | `		ph7_result_value(pCtx,&pVm->sErrCB); /* Will make it's own copy */` |
|  4323 |  547 | `	}else{` |
|  7344 |  548 | `		ph7_result_null(pCtx);` |
|     - |  549 | `	}` |
| 15980 |  550 | `	if( nArg < 1 ){` |
|   ! 0 |  551 | `		return PH7_OK;` |
|     - |  552 | `	}` |
|     - |  553 | `	/* $error_levels rides WITH the handler: it is read at full width (php ANDs` |
|     - |  554 | `	 * a zend_long, so 2^32+1024 still selects E_USER_NOTICE) and is pushed and` |
|     - |  555 | `	 * popped with it. */` |
| 15980 |  556 | `	iLevels = nArg > 1 ? ph7_value_to_int64(apArg[1]) : PH7_E_ALL_MASK;` |
| 15980 |  557 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     - |  558 | `		/* php REFUSES anything else that cannot be called, naming why. PH7` |
|     - |  559 | `		 * answered TRUE and kept the old handler, so a misspelled handler name` |
|     - |  560 | `		 * left the program reporting through the engine's own path in silence. */` |
|  9236 |  561 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|  9236 |  562 | `		if( rcCb != PH7_OK ){` |
|     5 |  563 | `			return rcCb;` |
|     - |  564 | `		}` |
|  4612 |  565 | `	}` |
|     - |  566 | ``	/* A `null` argument is a real stack entry: it silences the handler until a`` |
|     - |  567 | `	 * restore_error_handler() pops it and brings the previous one -- with ITS` |
|     - |  568 | `	 * levels -- back. */` |
| 23960 |  569 | `	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved,` |
| 15971 |  570 | `			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],iLevels) ){` |
|     - |  571 | `		/* Out of memory. Nothing was installed and nothing was saved, so answering` |
|     - |  572 | `		 * the previous handler would claim a replacement that did not happen. */` |
|   ! 0 |  573 | `		return PH7_VmThrowException(pCtx,"Error",` |
|   ! 0 |  574 | `			"%s(): out of memory installing the handler",ph7_function_name(pCtx));` |
|     - |  575 | `	}` |
| 15976 |  576 | `	return PH7_OK;` |
|  7991 |  577 | `}` |
|     - |  578 | `/*` |
|     - |  579 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|     - |  580 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|     - |  581 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|     - |  582 | ` *  dispatch paths call), or NULL when none is set.` |
|     - |  583 | ` */` |
|    36 |  584 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  585 | `{` |
|    39 |  586 | `	ph7_vm *pVm = pCtx->pVm;` |
|    18 |  587 | `	SXUNUSED(nArg);` |
|    18 |  588 | `	SXUNUSED(apArg);` |
|    39 |  589 | `	if( !ph7_value_is_null(&pVm->sErrCB) ){` |
|    27 |  590 | `		ph7_result_value(pCtx,&pVm->sErrCB);` |
|    15 |  591 | `	}else{` |
|    14 |  592 | `		ph7_result_null(pCtx);` |
|     - |  593 | `	}` |
|    39 |  594 | `	return PH7_OK;` |
|     3 |  595 | `}` |
|    16 |  596 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  597 | `{` |
|    18 |  598 | `	ph7_vm *pVm = pCtx->pVm;` |
|     8 |  599 | `	SXUNUSED(nArg);` |
|     8 |  600 | `	SXUNUSED(apArg);` |
|    18 |  601 | `	if( ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     8 |  602 | `		ph7_result_value(pCtx,&pVm->sExceptionCB);` |
|     5 |  603 | `	}else{` |
|    12 |  604 | `		ph7_result_null(pCtx);` |
|     - |  605 | `	}` |
|    18 |  606 | `	return PH7_OK;` |
|     2 |  607 | `}` |
|     - |  608 | `/*` |
|     - |  609 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|     - |  610 | ` *  Generates a backtrace.` |
|     - |  611 | ` * Paramaeter` |
|     - |  612 | ` *  $options` |
|     - |  613 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|     - |  614 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|     - |  615 | ` *   all the function/method arguments, to save memory.` |
|     - |  616 | ` * $limit` |
|     - |  617 | ` *   (Not Used)` |
|     - |  618 | ` * Return` |
|     - |  619 | ` *  An array.The possible returned elements are as follows:` |
|     - |  620 | ` *          Possible returned elements from debug_backtrace()` |
|     - |  621 | ` *          Name        Type      Description` |
|     - |  622 | ` *          ------      ------     -----------` |
|     - |  623 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|     - |  624 | ` *          line        integer   The current line number. See also __LINE__.` |
|     - |  625 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|     - |  626 | ` *          class       string    The current class name. See also __CLASS__` |
|     - |  627 | ` *          object      object    The current object.` |
|     - |  628 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|     - |  629 | ` *                                If inside an included file, this lists the included file name(s).` |
|     - |  630 | ` */` |
|     - |  631 | `/*` |
|     - |  632 | ` * array error_get_last()` |
|     - |  633 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|     - |  634 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|     - |  635 | ` *  and never reported an error at all.` |
|     - |  636 | ` */` |
|    88 |  637 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  638 | `{` |
|    92 |  639 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  640 | `	ph7_value *pArray,*pValue;` |
|    44 |  641 | `	SXUNUSED(nArg);` |
|    44 |  642 | `	SXUNUSED(apArg);` |
|    92 |  643 | `	if( pVm->nLastErrType == 0 ){` |
|     - |  644 | `		/* No error yet */` |
|     8 |  645 | `		ph7_result_null(pCtx);` |
|     8 |  646 | `		return PH7_OK;` |
|     - |  647 | `	}` |
|    85 |  648 | `	pArray = ph7_context_new_array(pCtx);` |
|    85 |  649 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    85 |  650 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  651 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  652 | `		ph7_result_null(pCtx);` |
|   ! 0 |  653 | `		return PH7_OK;` |
|     - |  654 | `	}` |
|    85 |  655 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|    85 |  656 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|   126 |  657 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|    82 |  658 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|    85 |  659 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|    85 |  660 | `	ph7_value_reset_string_cursor(pValue);` |
|   126 |  661 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|    82 |  662 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|    85 |  663 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|    85 |  664 | `	ph7_value_reset_string_cursor(pValue);` |
|    85 |  665 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|    85 |  666 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|    85 |  667 | `	ph7_result_value(pCtx,pArray);` |
|    85 |  668 | `	return PH7_OK;` |
|    48 |  669 | `}` |
|     - |  670 | `/*` |
|     - |  671 | ` * void error_clear_last()` |
|     - |  672 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|     - |  673 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|     - |  674 | ` */` |
|     8 |  675 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  676 | `{` |
|    10 |  677 | `	ph7_vm *pVm = pCtx->pVm;` |
|     4 |  678 | `	SXUNUSED(nArg);` |
|     4 |  679 | `	SXUNUSED(apArg);` |
|    10 |  680 | `	pVm->nLastErrType = 0;` |
|    10 |  681 | `	pVm->nLastErrLine = 0;` |
|    10 |  682 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    10 |  683 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    10 |  684 | `	return PH7_OK;` |
|     2 |  685 | `}` |
|     - |  686 | `/*` |
|     - |  687 | ``  * php's $limit reaches the walk through zend_fetch_debug_backtrace's `int` `` |
|     - |  688 | ` * parameter, a plain narrowing cast -- so PHP_INT_MAX arrives as -1 and reports` |
|     - |  689 | ` * NO frames at all, while 2^32 arrives as 0 and reports every one. Spelled` |
|     - |  690 | ` * through unsigned arithmetic because the two's-complement wrap of an` |
|     - |  691 | ` * out-of-range signed conversion is implementation-defined.` |
|     - |  692 | ` */` |
|   414 |  693 | `static sxi32 VmBacktraceLimit(int nArg,ph7_value **apArg)` |
|     5 |  694 | `{` |
|     - |  695 | `	sxu32 uL;` |
|   419 |  696 | `	if( nArg < 2 \|\| apArg[1] == 0 ){` |
|   367 |  697 | `		return 0;` |
|     - |  698 | `	}` |
|    55 |  699 | `	uL = (sxu32)((sxu64)ph7_value_to_int64(apArg[1]) & 0xFFFFFFFF);` |
|    55 |  700 | `	return (uL <= (sxu32)SXI32_HIGH) ? (sxi32)uL : -(sxi32)(SXU32_HIGH - uL) - 1;` |
|   212 |  701 | `}` |
|   370 |  702 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  703 | `{` |
|   375 |  704 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  705 | `	ph7_value *pList;` |
|     - |  706 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|     - |  707 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|   375 |  708 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|     - |  709 | `	/* $limit: how many frames to report, 0 meaning all of them. It was declared` |
|     - |  710 | `	 * in aBuiltinSig[], screened as an int and then never read, so asking for the` |
|     - |  711 | `	 * caller alone answered the WHOLE stack -- and a program that logs` |
|     - |  712 | `	 * debug_backtrace(0, 1) per request logged the entire chain every time. */` |
|   375 |  713 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|     - |  714 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|     - |  715 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|     - |  716 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|     - |  717 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|   375 |  718 | `	pList = ph7_context_new_array(pCtx);` |
|   375 |  719 | `	if( pList == 0 ){` |
|   ! 0 |  720 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  721 | `		ph7_result_null(pCtx);` |
|   ! 0 |  722 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  723 | `		SXUNUSED(apArg);` |
|   ! 0 |  724 | `		return PH7_OK;` |
|     - |  725 | `	}` |
|   375 |  726 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|     - |  727 | `	/* Return the freshly created list */` |
|   375 |  728 | `	ph7_result_value(pCtx,pList);` |
|     - |  729 | `	/*` |
|     - |  730 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|     - |  731 | `	 * as soon we return from this function.` |
|     - |  732 | `	 */` |
|   375 |  733 | `	return PH7_OK;` |
|   190 |  734 | `}` |
|     - |  735 | `/*` |
|     - |  736 | ` * Generate a small backtrace.` |
|     - |  737 | ` * Store the generated dump in the given BLOB` |
|     - |  738 | ` */` |
|     2 |  739 | `static int VmMiniBacktrace(` |
|     - |  740 | `	ph7_vm *pVm, /* Target VM */` |
|     - |  741 | `	SyBlob *pOut /* Store Dump here */` |
|     - |  742 | `	)` |
|     1 |  743 | `{` |
|     3 |  744 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  745 | `	ph7_vm_func *pFunc;` |
|     - |  746 | `	ph7_class *pClass;` |
|     - |  747 | `	SyString *pFile;` |
|     - |  748 | `	/* Called function */` |
|     3 |  749 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     3 |  750 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     3 |  751 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  752 | `	if( pFrame->pParent && pFunc ){` |
|     3 |  753 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|     3 |  754 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|     2 |  755 | `	}else{` |
|   ! 0 |  756 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|     - |  757 | `	}` |
|     3 |  758 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|     - |  759 | `	/* Current processed script */` |
|     3 |  760 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3 |  761 | `	if( pFile ){` |
|     3 |  762 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|     3 |  763 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|     3 |  764 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|     3 |  765 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|     1 |  766 | `	}` |
|     - |  767 | `	/* Top class */` |
|     3 |  768 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     3 |  769 | `	if( pClass ){` |
|   ! 0 |  770 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|   ! 0 |  771 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|   ! 0 |  772 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|   ! 0 |  773 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|   ! 0 |  774 | `	}` |
|     3 |  775 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|     - |  776 | `	/* All done */` |
|     3 |  777 | `	return SXRET_OK;` |
|     1 |  778 | `}` |
|     - |  779 | `/*` |
|     - |  780 | ` * void debug_print_backtrace(int $options = 0, int $limit = 0)` |
|     - |  781 | ` *  Prints a backtrace.` |
|     - |  782 | ` *` |
|     - |  783 | ` *  Both arguments were declared in aBuiltinSig[] and read by nothing, and what` |
|     - |  784 | ` *  the function PRINTED was not a backtrace at all: PH7's own` |
|     - |  785 | `` *  `[Called function: f][Processed file: x]` line, describing ONE frame in a`` |
|     - |  786 | ``  *  shape no php ever produced. php prints the same `#N file(line): func(args)` `` |
|     - |  787 | `` *  body getTraceAsString() renders -- without the `#N {main}` marker, which is`` |
|     - |  788 | ` *  the bottom of an exception's trace and not a frame -- and honours the same` |
|     - |  789 | ` *  $options bits and $limit as debug_backtrace().` |
|     - |  790 | ` */` |
|    44 |  791 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  792 | `{` |
|    46 |  793 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  794 | `	SyBlob sDump;` |
|     - |  795 | `	ph7_value *pList;` |
|    46 |  796 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 0;` |
|    46 |  797 | `	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);` |
|    46 |  798 | `	pList = ph7_context_new_array(pCtx);` |
|    46 |  799 | `	if( pList == 0 ){` |
|   ! 0 |  800 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  801 | `		return PH7_OK;` |
|     - |  802 | `	}` |
|    46 |  803 | `	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);` |
|    46 |  804 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|    46 |  805 | `	PH7_VmTraceToString(pVm,pList,FALSE,&sDump);` |
|     - |  806 | `	/* Output backtrace */` |
|    46 |  807 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     - |  808 | `	/* All done,cleanup */` |
|    46 |  809 | `	SyBlobRelease(&sDump);` |
|    46 |  810 | `	return PH7_OK;` |
|    24 |  811 | `}` |
|     - |  812 | `/*` |
|     - |  813 | ` * string debug_string_backtrace()` |
|     - |  814 | ` *  Generate a backtrace` |
|     - |  815 | ` * Parameters` |
|     - |  816 | ` * None` |
|     - |  817 | ` * Return` |
|     - |  818 | ` *  A mini backtrace().` |
|     - |  819 | ` * Note that this is a symisc extension.` |
|     - |  820 | ` */` |
|     2 |  821 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  822 | `{` |
|     3 |  823 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  824 | `	SyBlob sDump;` |
|     3 |  825 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  826 | `	/* Generate the backtrace */` |
|     3 |  827 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  828 | `	/* Return the backtrace */` |
|     3 |  829 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|     - |  830 | `	/* All done,cleanup */` |
|     3 |  831 | `	SyBlobRelease(&sDump);` |
|     1 |  832 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  833 | `	SXUNUSED(apArg);` |
|     3 |  834 | `	return PH7_OK;` |
|     1 |  835 | `}` |
|     - |  836 |  |
