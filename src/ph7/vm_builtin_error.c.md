# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 240/299 lines (80.27%)

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
|    38 |  131 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  132 | `{` |
|    42 |  133 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|    42 |  134 | `	if( nArg > 0 ){` |
|     - |  135 | `		const char *zErr;` |
|     - |  136 | `		int nLen;` |
|     - |  137 | `		/* Extract the error message */` |
|    42 |  138 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|    42 |  139 | `		if( nArg > 1 ){` |
|     - |  140 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|     - |  141 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|     - |  142 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|     - |  143 | `			 * translated engine severity). */` |
|    42 |  144 | `			nErr = ph7_value_to_int(apArg[1]);` |
|    42 |  145 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|   ! 0 |  146 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  147 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|     - |  148 | `			}` |
|    42 |  149 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|     - |  150 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|     3 |  151 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  152 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|     - |  153 | `			}` |
|    18 |  154 | `		}` |
|     - |  155 | `		/* Report error (consults an installed error handler, then displays) */` |
|    40 |  156 | `		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|    36 |  157 | `		if( nErr == 256 /* E_USER_ERROR */` |
|    22 |  158 | `		 && !ph7_value_is_callable(&pCtx->pVm->aErrCB[1]) ){` |
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
|    40 |  169 | `		ph7_result_bool(pCtx,1);` |
|    22 |  170 | `	}else{` |
|     - |  171 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  172 | `		ph7_result_bool(pCtx,0);` |
|     - |  173 | `	}` |
|    40 |  174 | `	return PH7_OK;` |
|    23 |  175 | `}` |
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
|   110 |  191 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  192 | `{` |
|   113 |  193 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  194 | `	int nOld;` |
|     - |  195 | `	/* Extract the old reporting level */` |
|   113 |  196 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|   113 |  197 | `	if( pVm->nErrSuppress > 0 ){` |
|     - |  198 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|     - |  199 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|     - |  200 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|     - |  201 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|     - |  202 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|    32 |  203 | `		nOld &= 4437;` |
|    15 |  204 | `	}` |
|   113 |  205 | `	if( nArg > 0 ){` |
|     - |  206 | `		int nNew;` |
|     - |  207 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|    33 |  208 | `		nNew = ph7_value_to_int(apArg[0]);` |
|    33 |  209 | `		pVm->iErrMask = (sxi32)nNew;` |
|    33 |  210 | `		pVm->bErrReport = nNew != 0;` |
|    15 |  211 | `	}` |
|     - |  212 | `	/* Return the old level */` |
|   113 |  213 | `	ph7_result_int(pCtx,nOld);` |
|   113 |  214 | `	return PH7_OK;` |
|     3 |  215 | `}` |
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
|     - |  239 | ` * NOTE:` |
|     - |  240 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|     - |  241 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|     - |  242 | ` *  configuration directive (refer to the official documentation for more information).` |
|     - |  243 | ` *  Otherwise this function is no-op.` |
|     - |  244 | ` */` |
|     4 |  245 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  246 | `{` |
|     - |  247 | `	const char *zMessage,*zDest,*zHeader;` |
|     5 |  248 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  249 | `	int iType = 0;` |
|     5 |  250 | `	if( nArg < 1 ){` |
|     - |  251 | `		/* Missing log message,return FALSE */` |
|   ! 0 |  252 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  253 | `		return PH7_OK;` |
|     - |  254 | `	}` |
|     5 |  255 | `	if( pVm->xErrLog  ){` |
|     - |  256 | `		/* Invoke the user callback */` |
|   ! 0 |  257 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|   ! 0 |  258 | `		zDest = zHeader = ""; /* Empty string */` |
|   ! 0 |  259 | `		if( nArg > 1 ){` |
|   ! 0 |  260 | `			iType = ph7_value_to_int(apArg[1]);` |
|   ! 0 |  261 | `			if( nArg > 2 ){` |
|   ! 0 |  262 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|   ! 0 |  263 | `				if( nArg > 3 ){` |
|   ! 0 |  264 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|   ! 0 |  265 | `				}` |
|   ! 0 |  266 | `			}` |
|   ! 0 |  267 | `		}` |
|   ! 0 |  268 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|   ! 0 |  269 | `	}` |
|     - |  270 | `	/* Retun TRUE */` |
|     5 |  271 | `	ph7_result_bool(pCtx,1);` |
|     5 |  272 | `	return PH7_OK;` |
|     3 |  273 | `}` |
|     - |  274 | `/*` |
|     - |  275 | ` * bool restore_exception_handler(void)` |
|     - |  276 | ` *  Restores the previously defined exception handler function.` |
|     - |  277 | ` * Parameter` |
|     - |  278 | ` *  None` |
|     - |  279 | ` * Return` |
|     - |  280 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|     - |  281 | ` */` |
|     4 |  282 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  283 | `{` |
|     5 |  284 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  285 | `	ph7_value *pOld,*pNew;` |
|     - |  286 | `	/* Point to the old and the new handler */` |
|     5 |  287 | `	pOld = &pVm->aExceptionCB[0];` |
|     5 |  288 | `	pNew = &pVm->aExceptionCB[1];` |
|     2 |  289 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  290 | `	SXUNUSED(apArg);` |
|     5 |  291 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|     - |  292 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|     - |  293 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|     - |  294 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|     - |  295 | `		 * single set_error_handler().` |
|     - |  296 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|     - |  297 | `		 * "a handler was in place". */` |
|     5 |  298 | `		PH7_MemObjRelease(pNew);` |
|     5 |  299 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|     5 |  300 | `		ph7_result_bool(pCtx,1);` |
|     5 |  301 | `		return PH7_OK;` |
|     - |  302 | `	}` |
|     - |  303 | `	/* Copy the old handler */` |
|   ! 0 |  304 | `	PH7_MemObjStore(pOld,pNew);` |
|   ! 0 |  305 | `	PH7_MemObjRelease(pOld);` |
|     - |  306 | `	/* Return TRUE */` |
|   ! 0 |  307 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 |  308 | `	return PH7_OK;` |
|     3 |  309 | `}` |
|     - |  310 | `/*` |
|     - |  311 | ` * callable set_exception_handler(callable $exception_handler)` |
|     - |  312 | ` *  Sets a user-defined exception handler function.` |
|     - |  313 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|     - |  314 | ` * NOTE` |
|     - |  315 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|     - |  316 | ` *  the satndard PHP engine.` |
|     - |  317 | ` * Parameters` |
|     - |  318 | ` *  $exception_handler` |
|     - |  319 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|     - |  320 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|     - |  321 | ` *   that was thrown.` |
|     - |  322 | ` *  Note:` |
|     - |  323 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  324 | ` * Return` |
|     - |  325 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|     - |  326 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  327 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  328 | ` */` |
|     6 |  329 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  330 | `{` |
|     9 |  331 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  332 | `	ph7_value *pOld,*pNew;` |
|     - |  333 | `	/* Point to the old and the new handler */` |
|     9 |  334 | `	pOld = &pVm->aExceptionCB[0];` |
|     9 |  335 | `	pNew = &pVm->aExceptionCB[1];` |
|     - |  336 | `	/* Return the old handler */` |
|     9 |  337 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9 |  338 | `	if( nArg > 0 ){` |
|     9 |  339 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|     - |  340 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|   ! 0 |  341 | `			PH7_MemObjRelease(pNew);` |
|   ! 0 |  342 | `			ph7_result_bool(pCtx,1);` |
|   ! 0 |  343 | `		}else{` |
|     9 |  344 | `			PH7_MemObjStore(pNew,pOld);` |
|     - |  345 | `			/* Install the new handler */` |
|     9 |  346 | `			PH7_MemObjStore(apArg[0],pNew);` |
|     - |  347 | `		}` |
|     3 |  348 | `	}` |
|     9 |  349 | `	return PH7_OK;` |
|     3 |  350 | `}` |
|     - |  351 | `/*` |
|     - |  352 | ` * bool restore_error_handler(void)` |
|     - |  353 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  354 | ` * Parameters:` |
|     - |  355 | ` *  None.` |
|     - |  356 | ` * Return` |
|     - |  357 | ` *  Always TRUE.` |
|     - |  358 | ` */` |
|    94 |  359 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  360 | `{` |
|    99 |  361 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  362 | `	ph7_value *pOld,*pNew;` |
|     - |  363 | `	/* Point to the old and the new handler */` |
|    99 |  364 | `	pOld = &pVm->aErrCB[0];` |
|    99 |  365 | `	pNew = &pVm->aErrCB[1];` |
|    47 |  366 | `	SXUNUSED(nArg); /* cc warning */` |
|    47 |  367 | `	SXUNUSED(apArg);` |
|    99 |  368 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|     - |  369 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|     - |  370 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|     - |  371 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|     - |  372 | `		 * single set_error_handler().` |
|     - |  373 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|     - |  374 | `		 * "a handler was in place". */` |
|    30 |  375 | `		PH7_MemObjRelease(pNew);` |
|    30 |  376 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|    30 |  377 | `		ph7_result_bool(pCtx,1);` |
|    30 |  378 | `		return PH7_OK;` |
|     - |  379 | `	}` |
|     - |  380 | `	/* Copy the old callback */` |
|    69 |  381 | `	PH7_MemObjStore(pOld,pNew);` |
|    69 |  382 | `	PH7_MemObjRelease(pOld);` |
|     - |  383 | `	/* Return TRUE */` |
|    69 |  384 | `	ph7_result_bool(pCtx,1);` |
|    69 |  385 | `	return PH7_OK;` |
|    52 |  386 | `}` |
|     - |  387 | `/*` |
|     - |  388 | ` * value set_error_handler(callable $error_handler)` |
|     - |  389 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  390 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|     - |  391 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|     - |  392 | ` *  Sets a user-defined error handler function.` |
|     - |  393 | ` *  This function can be used for defining your own way of handling errors during` |
|     - |  394 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|     - |  395 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|     - |  396 | ` *  conditions (using trigger_error()).` |
|     - |  397 | ` * Parameters` |
|     - |  398 | ` *  $error_handler` |
|     - |  399 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|     - |  400 | ` *   describing the error.` |
|     - |  401 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|     - |  402 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|     - |  403 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|     - |  404 | ` *   The function can be shown as:` |
|     - |  405 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|     - |  406 | ` *     errno` |
|     - |  407 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|     - |  408 | ` *   errstr` |
|     - |  409 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|     - |  410 | ` *   errfile` |
|     - |  411 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|     - |  412 | ` *     was raised in, as a string.` |
|     - |  413 | ` *  Note:` |
|     - |  414 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|     - |  415 | ` * Return` |
|     - |  416 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|     - |  417 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|     - |  418 | ` *  resetting the handler to its default state, TRUE is returned.` |
|     - |  419 | ` */` |
| 10210 |  420 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  421 | `{` |
| 10215 |  422 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  423 | `	ph7_value *pOld,*pNew;` |
|     - |  424 | `	/* Point to the old and the new handler */` |
| 10215 |  425 | `	pOld = &pVm->aErrCB[0];` |
| 10215 |  426 | `	pNew = &pVm->aErrCB[1];` |
|     - |  427 | `	/* Return the old handler */` |
| 10215 |  428 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
| 10215 |  429 | `	if( nArg > 0 ){` |
| 10215 |  430 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|     - |  431 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|  5045 |  432 | `			PH7_MemObjRelease(pNew);` |
|  5045 |  433 | `			ph7_result_bool(pCtx,1);` |
|  2523 |  434 | `		}else{` |
|  5171 |  435 | `			PH7_MemObjStore(pNew,pOld);` |
|     - |  436 | `			/* Install the new handler */` |
|  5171 |  437 | `			PH7_MemObjStore(apArg[0],pNew);` |
|     - |  438 | `		}` |
|  5105 |  439 | `	}` |
| 10215 |  440 | `	return PH7_OK;` |
|     5 |  441 | `}` |
|     - |  442 | `/*` |
|     - |  443 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|     - |  444 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|     - |  445 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|     - |  446 | ` *  dispatch paths call), or NULL when none is set.` |
|     - |  447 | ` */` |
|     8 |  448 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  449 | `{` |
|     9 |  450 | `	ph7_vm *pVm = pCtx->pVm;` |
|     4 |  451 | `	SXUNUSED(nArg);` |
|     4 |  452 | `	SXUNUSED(apArg);` |
|     9 |  453 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|     5 |  454 | `		ph7_result_value(pCtx,&pVm->aErrCB[1]);` |
|     3 |  455 | `	}else{` |
|     5 |  456 | `		ph7_result_null(pCtx);` |
|     - |  457 | `	}` |
|     9 |  458 | `	return PH7_OK;` |
|     1 |  459 | `}` |
|     4 |  460 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  461 | `{` |
|     5 |  462 | `	ph7_vm *pVm = pCtx->pVm;` |
|     2 |  463 | `	SXUNUSED(nArg);` |
|     2 |  464 | `	SXUNUSED(apArg);` |
|     5 |  465 | `	if( ph7_value_is_callable(&pVm->aExceptionCB[1]) ){` |
|     3 |  466 | `		ph7_result_value(pCtx,&pVm->aExceptionCB[1]);` |
|     2 |  467 | `	}else{` |
|     3 |  468 | `		ph7_result_null(pCtx);` |
|     - |  469 | `	}` |
|     5 |  470 | `	return PH7_OK;` |
|     1 |  471 | `}` |
|     - |  472 | `/*` |
|     - |  473 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|     - |  474 | ` *  Generates a backtrace.` |
|     - |  475 | ` * Paramaeter` |
|     - |  476 | ` *  $options` |
|     - |  477 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|     - |  478 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|     - |  479 | ` *   all the function/method arguments, to save memory.` |
|     - |  480 | ` * $limit` |
|     - |  481 | ` *   (Not Used)` |
|     - |  482 | ` * Return` |
|     - |  483 | ` *  An array.The possible returned elements are as follows:` |
|     - |  484 | ` *          Possible returned elements from debug_backtrace()` |
|     - |  485 | ` *          Name        Type      Description` |
|     - |  486 | ` *          ------      ------     -----------` |
|     - |  487 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|     - |  488 | ` *          line        integer   The current line number. See also __LINE__.` |
|     - |  489 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|     - |  490 | ` *          class       string    The current class name. See also __CLASS__` |
|     - |  491 | ` *          object      object    The current object.` |
|     - |  492 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|     - |  493 | ` *                                If inside an included file, this lists the included file name(s).` |
|     - |  494 | ` */` |
|     - |  495 | `/*` |
|     - |  496 | ` * array error_get_last()` |
|     - |  497 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|     - |  498 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|     - |  499 | ` *  and never reported an error at all.` |
|     - |  500 | ` */` |
|    10 |  501 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  502 | `{` |
|    13 |  503 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  504 | `	ph7_value *pArray,*pValue;` |
|     5 |  505 | `	SXUNUSED(nArg);` |
|     5 |  506 | `	SXUNUSED(apArg);` |
|    13 |  507 | `	if( pVm->nLastErrType == 0 ){` |
|     - |  508 | `		/* No error yet */` |
|     8 |  509 | `		ph7_result_null(pCtx);` |
|     8 |  510 | `		return PH7_OK;` |
|     - |  511 | `	}` |
|     6 |  512 | `	pArray = ph7_context_new_array(pCtx);` |
|     6 |  513 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     6 |  514 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 |  515 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  516 | `		ph7_result_null(pCtx);` |
|   ! 0 |  517 | `		return PH7_OK;` |
|     - |  518 | `	}` |
|     6 |  519 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|     6 |  520 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|     8 |  521 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|     4 |  522 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|     6 |  523 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|     6 |  524 | `	ph7_value_reset_string_cursor(pValue);` |
|     8 |  525 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|     4 |  526 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|     6 |  527 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     6 |  528 | `	ph7_value_reset_string_cursor(pValue);` |
|     6 |  529 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|     6 |  530 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|     6 |  531 | `	ph7_result_value(pCtx,pArray);` |
|     6 |  532 | `	return PH7_OK;` |
|     8 |  533 | `}` |
|     - |  534 | `/*` |
|     - |  535 | ` * void error_clear_last()` |
|     - |  536 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|     - |  537 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|     - |  538 | ` */` |
|     6 |  539 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  540 | `{` |
|     7 |  541 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  542 | `	SXUNUSED(nArg);` |
|     3 |  543 | `	SXUNUSED(apArg);` |
|     7 |  544 | `	pVm->nLastErrType = 0;` |
|     7 |  545 | `	pVm->nLastErrLine = 0;` |
|     7 |  546 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|     7 |  547 | `	SyBlobReset(&pVm->sLastErrFile);` |
|     7 |  548 | `	return PH7_OK;` |
|     1 |  549 | `}` |
|    16 |  550 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  551 | `{` |
|    19 |  552 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  553 | `	ph7_value *pList;` |
|     - |  554 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|     - |  555 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|    19 |  556 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|     - |  557 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|     - |  558 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|     - |  559 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|     - |  560 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|    19 |  561 | `	pList = ph7_context_new_array(pCtx);` |
|    19 |  562 | `	if( pList == 0 ){` |
|   ! 0 |  563 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |  564 | `		ph7_result_null(pCtx);` |
|   ! 0 |  565 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  566 | `		SXUNUSED(apArg);` |
|   ! 0 |  567 | `		return PH7_OK;` |
|     - |  568 | `	}` |
|    19 |  569 | `	VmBuildBacktrace(&(*pVm),iOptions,pList);` |
|     - |  570 | `	/* Return the freshly created list */` |
|    19 |  571 | `	ph7_result_value(pCtx,pList);` |
|     - |  572 | `	/*` |
|     - |  573 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|     - |  574 | `	 * as soon we return from this function.` |
|     - |  575 | `	 */` |
|    19 |  576 | `	return PH7_OK;` |
|    11 |  577 | `}` |
|     - |  578 | `/*` |
|     - |  579 | ` * Generate a small backtrace.` |
|     - |  580 | ` * Store the generated dump in the given BLOB` |
|     - |  581 | ` */` |
|     4 |  582 | `static int VmMiniBacktrace(` |
|     - |  583 | `	ph7_vm *pVm, /* Target VM */` |
|     - |  584 | `	SyBlob *pOut /* Store Dump here */` |
|     - |  585 | `	)` |
|     1 |  586 | `{` |
|     5 |  587 | `	VmFrame *pFrame = pVm->pFrame;` |
|     - |  588 | `	ph7_vm_func *pFunc;` |
|     - |  589 | `	ph7_class *pClass;` |
|     - |  590 | `	SyString *pFile;` |
|     - |  591 | `	/* Called function */` |
|     5 |  592 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     5 |  593 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     5 |  594 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|     5 |  595 | `	if( pFrame->pParent && pFunc ){` |
|     5 |  596 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|     5 |  597 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|     3 |  598 | `	}else{` |
|   ! 0 |  599 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|     - |  600 | `	}` |
|     5 |  601 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|     - |  602 | `	/* Current processed script */` |
|     5 |  603 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     5 |  604 | `	if( pFile ){` |
|     5 |  605 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|     5 |  606 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|     5 |  607 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|     5 |  608 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|     2 |  609 | `	}` |
|     - |  610 | `	/* Top class */` |
|     5 |  611 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     5 |  612 | `	if( pClass ){` |
|   ! 0 |  613 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|   ! 0 |  614 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|   ! 0 |  615 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|   ! 0 |  616 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|   ! 0 |  617 | `	}` |
|     5 |  618 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|     - |  619 | `	/* All done */` |
|     5 |  620 | `	return SXRET_OK;` |
|     1 |  621 | `}` |
|     - |  622 | `/*` |
|     - |  623 | ` * void debug_print_backtrace()` |
|     - |  624 | ` *  Prints a backtrace` |
|     - |  625 | ` * Parameters` |
|     - |  626 | ` * None` |
|     - |  627 | ` * Return` |
|     - |  628 | ` * NULL` |
|     - |  629 | ` */` |
|     2 |  630 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  631 | `{` |
|     3 |  632 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  633 | `	SyBlob sDump;` |
|     3 |  634 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  635 | `	/* Generate the backtrace */` |
|     3 |  636 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  637 | `	/* Output backtrace */` |
|     3 |  638 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     - |  639 | `	/* All done,cleanup */` |
|     3 |  640 | `	SyBlobRelease(&sDump);` |
|     1 |  641 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  642 | `	SXUNUSED(apArg);` |
|     3 |  643 | `	return PH7_OK;` |
|     1 |  644 | `}` |
|     - |  645 | `/*` |
|     - |  646 | ` * string debug_string_backtrace()` |
|     - |  647 | ` *  Generate a backtrace` |
|     - |  648 | ` * Parameters` |
|     - |  649 | ` * None` |
|     - |  650 | ` * Return` |
|     - |  651 | ` *  A mini backtrace().` |
|     - |  652 | ` * Note that this is a symisc extension.` |
|     - |  653 | ` */` |
|     2 |  654 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  655 | `{` |
|     3 |  656 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  657 | `	SyBlob sDump;` |
|     3 |  658 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|     - |  659 | `	/* Generate the backtrace */` |
|     3 |  660 | `	VmMiniBacktrace(pVm,&sDump);` |
|     - |  661 | `	/* Return the backtrace */` |
|     3 |  662 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|     - |  663 | `	/* All done,cleanup */` |
|     3 |  664 | `	SyBlobRelease(&sDump);` |
|     1 |  665 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  666 | `	SXUNUSED(apArg);` |
|     3 |  667 | `	return PH7_OK;` |
|     1 |  668 | `}` |
|     - |  669 |  |
