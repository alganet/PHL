# src/ph7/vm_builtin_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 236/295 lines (80.00%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include <stddef.h> /* NULL */` |
|    - |    8 | `/*` |
|    - |    9 | ` * Section:` |
|    - |   10 | ` *    Error-handling builtins: assert, trigger_error, error_reporting,` |
|    - |   11 | ` *    error_log, the error/exception handler get/set/restore family,` |
|    - |   12 | ` *    error_get_last/clear_last and the debug_backtrace family.` |
|    - |   13 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|    - |   14 | ` * Status:` |
|    - |   15 | ` *    Stable.` |
|    - |   16 | ` */` |
|    - |   17 | `/*` |
|    - |   18 | ` * bool assert(mixed $assertion)` |
|    - |   19 | ` *  Checks if assertion is FALSE.` |
|    - |   20 | ` * Parameter` |
|    - |   21 | ` *  $assertion` |
|    - |   22 | ` *    The assertion to test.` |
|    - |   23 | ` * Return` |
|    - |   24 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|    - |   25 | ` */` |
|   22 |   26 | `PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |   27 | `{` |
|   27 |   28 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   29 | `	int iFlags,iResult;` |
|    - |   30 | `	const char *zDesc;` |
|   27 |   31 | `	iFlags = pVm->iAssertFlags;` |
|   27 |   32 | `	if( iFlags & (PH7_ASSERT_DISABLE\|PH7_ASSERT_ZEND_OFF) ){` |
|    - |   33 | `		/* Assertion is disabled (assert.active=0) or compiled out` |
|    - |   34 | `		 * (zend.assertions<1); the call is elided entirely -- even a missing` |
|    - |   35 | `		 * argument is not diagnosed -- and it evaluates to TRUE (PHP 8). */` |
|   11 |   36 | `		ph7_result_bool(pCtx,1);` |
|   11 |   37 | `		return PH7_OK;` |
|    - |   38 | `	}` |
|    - |   39 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|   16 |   40 | `	if( nArg < 1 ){` |
|  ! 0 |   41 | `		return PH7_VmThrowException(pCtx,` |
|    - |   42 | `			"ArgumentCountError",` |
|    - |   43 | `			"assert() expects at least 1 argument, 0 given"` |
|    - |   44 | `			);` |
|    - |   45 | `	}` |
|    - |   46 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|   16 |   47 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|   16 |   48 | `	if( !iResult ){` |
|    - |   49 | `		/* Assertion failed */` |
|    - |   50 | `		/* Extract optional description */` |
|   16 |   51 | `		zDesc = 0;` |
|   16 |   52 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|    3 |   53 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|    1 |   54 | `		}` |
|   16 |   55 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|    - |   56 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|    - |   57 | `			ph7_value sFile,sLine;` |
|    - |   58 | `			ph7_value *apCbArg[3];` |
|    - |   59 | `			SyString *pFile;` |
|    - |   60 | `			/* Extract the processed script */` |
|  ! 0 |   61 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  ! 0 |   62 | `			if( pFile == 0 ){` |
|  ! 0 |   63 | `				pFile = (SyString *)&sFileName;` |
|  ! 0 |   64 | `			}` |
|    - |   65 | `			/* Invoke the callback */` |
|  ! 0 |   66 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|  ! 0 |   67 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|  ! 0 |   68 | `			apCbArg[0] = &sFile;` |
|  ! 0 |   69 | `			apCbArg[1] = &sLine;` |
|  ! 0 |   70 | `			apCbArg[2] = apArg[0];` |
|  ! 0 |   71 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|    - |   72 | `			/* Clean-up the mess left behind */` |
|  ! 0 |   73 | `			PH7_MemObjRelease(&sFile);` |
|  ! 0 |   74 | `			PH7_MemObjRelease(&sLine);` |
|  ! 0 |   75 | `		}` |
|   16 |   76 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|    - |   77 | `			/* Abort VM execution immediately */` |
|  ! 0 |   78 | `			return PH7_ABORT;` |
|    - |   79 | `		}` |
|    - |   80 | `		/* PHP 8: throw AssertionError by default */` |
|   16 |   81 | `		if( zDesc && zDesc[0] != '\0' ){` |
|    4 |   82 | `			return PH7_VmThrowException(pCtx,` |
|    - |   83 | `				"AssertionError",` |
|    - |   84 | `				"%s",` |
|    1 |   85 | `				zDesc` |
|    - |   86 | `				);` |
|  ! 0 |   87 | `		}else{` |
|   13 |   88 | `			return PH7_VmThrowException(pCtx,` |
|    - |   89 | `				"AssertionError",` |
|    - |   90 | `				"assert(false)"` |
|    - |   91 | `				);` |
|    - |   92 | `		}` |
|    - |   93 | `	}` |
|    - |   94 | `	/* Assertion passed */` |
|  ! 0 |   95 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |   96 | `	return PH7_OK;` |
|   16 |   97 | `}` |
|    - |   98 | `/*` |
|    - |   99 | ` * Section:` |
|    - |  100 | ` *  Error reporting functions.` |
|    - |  101 | ` * Status:` |
|    - |  102 | ` *    Stable.` |
|    - |  103 | ` */` |
|    - |  104 | `/*` |
|    - |  105 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|    - |  106 | ` *  Generates a user-level error/warning/notice message.` |
|    - |  107 | ` * Parameters` |
|    - |  108 | ` *  $error_msg` |
|    - |  109 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|    - |  110 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|    - |  111 | ` * $error_type` |
|    - |  112 | ` *  The designated error type for this error. It only works with the E_USER family` |
|    - |  113 | ` *  of constants, and will default to E_USER_NOTICE.` |
|    - |  114 | ` * Return` |
|    - |  115 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|    - |  116 | ` */` |
|   24 |  117 | `PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  118 | `{` |
|   29 |  119 | `	int nErr = 1024; /* E_USER_NOTICE — php's default level */` |
|   29 |  120 | `	if( nArg > 0 ){` |
|    - |  121 | `		const char *zErr;` |
|    - |  122 | `		int nLen;` |
|    - |  123 | `		/* Extract the error message */` |
|   29 |  124 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|   29 |  125 | `		if( nArg > 1 ){` |
|    - |  126 | `			/* php 8: only the E_USER_* levels are accepted — anything else is a` |
|    - |  127 | `			 * catchable ValueError. The RAW errno flows to the display label map` |
|    - |  128 | `			 * and to a user error handler (php hands the handler 1024, not a` |
|    - |  129 | `			 * translated engine severity). */` |
|   29 |  130 | `			nErr = ph7_value_to_int(apArg[1]);` |
|   29 |  131 | `			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){` |
|  ! 0 |  132 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  133 | `					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");` |
|    - |  134 | `			}` |
|   29 |  135 | `			if( nErr == 256 /* E_USER_ERROR */ ){` |
|    - |  136 | `				/* php only DEPRECATES the user-fatal level; PHL rejects it. */` |
|    3 |  137 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  138 | `					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");` |
|    - |  139 | `			}` |
|   11 |  140 | `		}` |
|    - |  141 | `		/* Report error (consults an installed error handler, then displays) */` |
|   27 |  142 | `		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|   22 |  143 | `		if( nErr == 256 /* E_USER_ERROR */` |
|   16 |  144 | `		 && !ph7_value_is_callable(&pCtx->pVm->aErrCB[1]) ){` |
|    - |  145 | `			/* php: an unhandled user fatal halts with exit 255 (pre-fix the` |
|    - |  146 | `			 * PH7_ABORT here was overwritten by the throw's status, so` |
|    - |  147 | `			 * E_USER_ERROR silently CONTINUED). With a handler installed the` |
|    - |  148 | `			 * script continues — the handler-returned-false renormalization is` |
|    - |  149 | `			 * a recorded nuance. No php stack trace either (recorded). */` |
|  ! 0 |  150 | `			pCtx->pVm->iExitStatus = 255;` |
|  ! 0 |  151 | `			pCtx->pVm->bHaltRequested = 1;` |
|  ! 0 |  152 | `			return PH7_ABORT;` |
|    - |  153 | `		}` |
|    - |  154 | `		/* Return true */` |
|   27 |  155 | `		ph7_result_bool(pCtx,1);` |
|   16 |  156 | `	}else{` |
|    - |  157 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  158 | `		ph7_result_bool(pCtx,0);` |
|    - |  159 | `	}` |
|   27 |  160 | `	return PH7_OK;` |
|   17 |  161 | `}` |
|    - |  162 | `/*` |
|    - |  163 | ` * int error_reporting([int $level])` |
|    - |  164 | ` *  Sets which PHP errors are reported.` |
|    - |  165 | ` * Parameters` |
|    - |  166 | ` *  $level` |
|    - |  167 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|    - |  168 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|    - |  169 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|    - |  170 | ` *   levels will not always behave as expected.` |
|    - |  171 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|    - |  172 | ` *   in the predefined constants.` |
|    - |  173 | ` * Return` |
|    - |  174 | ` *   Returns the old error_reporting level or the current level if no level` |
|    - |  175 | ` *   parameter is given.` |
|    - |  176 | ` */` |
|   80 |  177 | `PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  178 | `{` |
|   83 |  179 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  180 | `	int nOld;` |
|    - |  181 | `	/* Extract the old reporting level */` |
|   83 |  182 | `	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;` |
|   83 |  183 | `	if( pVm->nErrSuppress > 0 ){` |
|    - |  184 | `		/* Inside the '@' silence operator php reports the level masked down to` |
|    - |  185 | `		 * the errors '@' cannot suppress: E_ERROR\|E_PARSE\|E_CORE_ERROR\|` |
|    - |  186 | `		 * E_COMPILE_ERROR\|E_USER_ERROR\|E_RECOVERABLE_ERROR (== 4437). A custom` |
|    - |  187 | `		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor` |
|    - |  188 | `		 * '@', so a suppressed warning must fall outside the returned mask. */` |
|   10 |  189 | `		nOld &= 4437;` |
|    4 |  190 | `	}` |
|   83 |  191 | `	if( nArg > 0 ){` |
|    - |  192 | `		int nNew;` |
|    - |  193 | `		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */` |
|   27 |  194 | `		nNew = ph7_value_to_int(apArg[0]);` |
|   27 |  195 | `		pVm->iErrMask = (sxi32)nNew;` |
|   27 |  196 | `		pVm->bErrReport = nNew != 0;` |
|   12 |  197 | `	}` |
|    - |  198 | `	/* Return the old level */` |
|   83 |  199 | `	ph7_result_int(pCtx,nOld);` |
|   83 |  200 | `	return PH7_OK;` |
|    3 |  201 | `}` |
|    - |  202 | `/*` |
|    - |  203 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|    - |  204 | ` *  Send an error message somewhere.` |
|    - |  205 | ` * Parameter` |
|    - |  206 | ` *  $message` |
|    - |  207 | ` *   The error message that should be logged.` |
|    - |  208 | ` *  $message_type` |
|    - |  209 | ` *   Says where the error should go. The possible message types are as follows:` |
|    - |  210 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|    - |  211 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|    - |  212 | ` *       This is the default option.` |
|    - |  213 | ` *    1 message is sent by email to the address in the destination parameter.` |
|    - |  214 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|    - |  215 | ` *    2  No longer an option.` |
|    - |  216 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|    - |  217 | ` *       to the end of the message string.` |
|    - |  218 | ` *    4  message is sent directly to the SAPI logging handler.` |
|    - |  219 | ` *  $destination` |
|    - |  220 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|    - |  221 | ` *  $extra_headers` |
|    - |  222 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|    - |  223 | ` * Return` |
|    - |  224 | ` *  TRUE on success or FALSE on failure.` |
|    - |  225 | ` * NOTE:` |
|    - |  226 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|    - |  227 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|    - |  228 | ` *  configuration directive (refer to the official documentation for more information).` |
|    - |  229 | ` *  Otherwise this function is no-op.` |
|    - |  230 | ` */` |
|    4 |  231 | `PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  232 | `{` |
|    - |  233 | `	const char *zMessage,*zDest,*zHeader;` |
|    5 |  234 | `	ph7_vm *pVm = pCtx->pVm;` |
|    5 |  235 | `	int iType = 0;` |
|    5 |  236 | `	if( nArg < 1 ){` |
|    - |  237 | `		/* Missing log message,return FALSE */` |
|  ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  239 | `		return PH7_OK;` |
|    - |  240 | `	}` |
|    5 |  241 | `	if( pVm->xErrLog  ){` |
|    - |  242 | `		/* Invoke the user callback */` |
|  ! 0 |  243 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|  ! 0 |  244 | `		zDest = zHeader = ""; /* Empty string */` |
|  ! 0 |  245 | `		if( nArg > 1 ){` |
|  ! 0 |  246 | `			iType = ph7_value_to_int(apArg[1]);` |
|  ! 0 |  247 | `			if( nArg > 2 ){` |
|  ! 0 |  248 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|  ! 0 |  249 | `				if( nArg > 3 ){` |
|  ! 0 |  250 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|  ! 0 |  251 | `				}` |
|  ! 0 |  252 | `			}` |
|  ! 0 |  253 | `		}` |
|  ! 0 |  254 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|  ! 0 |  255 | `	}` |
|    - |  256 | `	/* Retun TRUE */` |
|    5 |  257 | `	ph7_result_bool(pCtx,1);` |
|    5 |  258 | `	return PH7_OK;` |
|    3 |  259 | `}` |
|    - |  260 | `/*` |
|    - |  261 | ` * bool restore_exception_handler(void)` |
|    - |  262 | ` *  Restores the previously defined exception handler function.` |
|    - |  263 | ` * Parameter` |
|    - |  264 | ` *  None` |
|    - |  265 | ` * Return` |
|    - |  266 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|    - |  267 | ` */` |
|    4 |  268 | `PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  269 | `{` |
|    5 |  270 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  271 | `	ph7_value *pOld,*pNew;` |
|    - |  272 | `	/* Point to the old and the new handler */` |
|    5 |  273 | `	pOld = &pVm->aExceptionCB[0];` |
|    5 |  274 | `	pNew = &pVm->aExceptionCB[1];` |
|    2 |  275 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  276 | `	SXUNUSED(apArg);` |
|    5 |  277 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|    - |  278 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|    - |  279 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|    - |  280 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|    - |  281 | `		 * single set_error_handler().` |
|    - |  282 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|    - |  283 | `		 * "a handler was in place". */` |
|    5 |  284 | `		PH7_MemObjRelease(pNew);` |
|    5 |  285 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|    5 |  286 | `		ph7_result_bool(pCtx,1);` |
|    5 |  287 | `		return PH7_OK;` |
|    - |  288 | `	}` |
|    - |  289 | `	/* Copy the old handler */` |
|  ! 0 |  290 | `	PH7_MemObjStore(pOld,pNew);` |
|  ! 0 |  291 | `	PH7_MemObjRelease(pOld);` |
|    - |  292 | `	/* Return TRUE */` |
|  ! 0 |  293 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  294 | `	return PH7_OK;` |
|    3 |  295 | `}` |
|    - |  296 | `/*` |
|    - |  297 | ` * callable set_exception_handler(callable $exception_handler)` |
|    - |  298 | ` *  Sets a user-defined exception handler function.` |
|    - |  299 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|    - |  300 | ` * NOTE` |
|    - |  301 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|    - |  302 | ` *  the satndard PHP engine.` |
|    - |  303 | ` * Parameters` |
|    - |  304 | ` *  $exception_handler` |
|    - |  305 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|    - |  306 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|    - |  307 | ` *   that was thrown.` |
|    - |  308 | ` *  Note:` |
|    - |  309 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|    - |  310 | ` * Return` |
|    - |  311 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|    - |  312 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|    - |  313 | ` *  resetting the handler to its default state, TRUE is returned.` |
|    - |  314 | ` */` |
|    6 |  315 | `PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  316 | `{` |
|    8 |  317 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  318 | `	ph7_value *pOld,*pNew;` |
|    - |  319 | `	/* Point to the old and the new handler */` |
|    8 |  320 | `	pOld = &pVm->aExceptionCB[0];` |
|    8 |  321 | `	pNew = &pVm->aExceptionCB[1];` |
|    - |  322 | `	/* Return the old handler */` |
|    8 |  323 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    8 |  324 | `	if( nArg > 0 ){` |
|    8 |  325 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|    - |  326 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|  ! 0 |  327 | `			PH7_MemObjRelease(pNew);` |
|  ! 0 |  328 | `			ph7_result_bool(pCtx,1);` |
|  ! 0 |  329 | `		}else{` |
|    8 |  330 | `			PH7_MemObjStore(pNew,pOld);` |
|    - |  331 | `			/* Install the new handler */` |
|    8 |  332 | `			PH7_MemObjStore(apArg[0],pNew);` |
|    - |  333 | `		}` |
|    3 |  334 | `	}` |
|    8 |  335 | `	return PH7_OK;` |
|    2 |  336 | `}` |
|    - |  337 | `/*` |
|    - |  338 | ` * bool restore_error_handler(void)` |
|    - |  339 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|    - |  340 | ` * Parameters:` |
|    - |  341 | ` *  None.` |
|    - |  342 | ` * Return` |
|    - |  343 | ` *  Always TRUE.` |
|    - |  344 | ` */` |
|   48 |  345 | `PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  346 | `{` |
|   52 |  347 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  348 | `	ph7_value *pOld,*pNew;` |
|    - |  349 | `	/* Point to the old and the new handler */` |
|   52 |  350 | `	pOld = &pVm->aErrCB[0];` |
|   52 |  351 | `	pNew = &pVm->aErrCB[1];` |
|   24 |  352 | `	SXUNUSED(nArg); /* cc warning */` |
|   24 |  353 | `	SXUNUSED(apArg);` |
|   52 |  354 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|    - |  355 | `		/* Nothing SAVED underneath — but php pops the handler stack regardless, so the` |
|    - |  356 | `		 * ACTIVE handler must still go (reporting reverts to the engine's own). Returning` |
|    - |  357 | `		 * early here left it installed, making restore_error_handler() a no-op after a` |
|    - |  358 | `		 * single set_error_handler().` |
|    - |  359 | `		 * php answers TRUE either way: the return value says "the call is valid", not` |
|    - |  360 | `		 * "a handler was in place". */` |
|   15 |  361 | `		PH7_MemObjRelease(pNew);` |
|   15 |  362 | `		MemObjSetType(pNew,MEMOBJ_NULL);` |
|   15 |  363 | `		ph7_result_bool(pCtx,1);` |
|   15 |  364 | `		return PH7_OK;` |
|    - |  365 | `	}` |
|    - |  366 | `	/* Copy the old callback */` |
|   37 |  367 | `	PH7_MemObjStore(pOld,pNew);` |
|   37 |  368 | `	PH7_MemObjRelease(pOld);` |
|    - |  369 | `	/* Return TRUE */` |
|   37 |  370 | `	ph7_result_bool(pCtx,1);` |
|   37 |  371 | `	return PH7_OK;` |
|   28 |  372 | `}` |
|    - |  373 | `/*` |
|    - |  374 | ` * value set_error_handler(callable $error_handler)` |
|    - |  375 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|    - |  376 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|    - |  377 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|    - |  378 | ` *  Sets a user-defined error handler function.` |
|    - |  379 | ` *  This function can be used for defining your own way of handling errors during` |
|    - |  380 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|    - |  381 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|    - |  382 | ` *  conditions (using trigger_error()).` |
|    - |  383 | ` * Parameters` |
|    - |  384 | ` *  $error_handler` |
|    - |  385 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|    - |  386 | ` *   describing the error.` |
|    - |  387 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|    - |  388 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|    - |  389 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|    - |  390 | ` *   The function can be shown as:` |
|    - |  391 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|    - |  392 | ` *     errno` |
|    - |  393 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|    - |  394 | ` *   errstr` |
|    - |  395 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|    - |  396 | ` *   errfile` |
|    - |  397 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|    - |  398 | ` *     was raised in, as a string.` |
|    - |  399 | ` *  Note:` |
|    - |  400 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|    - |  401 | ` * Return` |
|    - |  402 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|    - |  403 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|    - |  404 | ` *  resetting the handler to its default state, TRUE is returned.` |
|    - |  405 | ` */` |
| 9972 |  406 | `PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  407 | `{` |
| 9977 |  408 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  409 | `	ph7_value *pOld,*pNew;` |
|    - |  410 | `	/* Point to the old and the new handler */` |
| 9977 |  411 | `	pOld = &pVm->aErrCB[0];` |
| 9977 |  412 | `	pNew = &pVm->aErrCB[1];` |
|    - |  413 | `	/* Return the old handler */` |
| 9977 |  414 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
| 9977 |  415 | `	if( nArg > 0 ){` |
| 9977 |  416 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|    - |  417 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
| 4961 |  418 | `			PH7_MemObjRelease(pNew);` |
| 4961 |  419 | `			ph7_result_bool(pCtx,1);` |
| 2481 |  420 | `		}else{` |
| 5017 |  421 | `			PH7_MemObjStore(pNew,pOld);` |
|    - |  422 | `			/* Install the new handler */` |
| 5017 |  423 | `			PH7_MemObjStore(apArg[0],pNew);` |
|    - |  424 | `		}` |
| 4986 |  425 | `	}` |
| 9977 |  426 | `	return PH7_OK;` |
|    5 |  427 | `}` |
|    - |  428 | `/*` |
|    - |  429 | ` * ?callable get_error_handler(void)     -- php 8.5` |
|    - |  430 | ` * ?callable get_exception_handler(void) -- php 8.5` |
|    - |  431 | ` *  Return the currently installed handler callable (the ACTIVE slot [1] that the` |
|    - |  432 | ` *  dispatch paths call), or NULL when none is set.` |
|    - |  433 | ` */` |
|    8 |  434 | `PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  435 | `{` |
|    9 |  436 | `	ph7_vm *pVm = pCtx->pVm;` |
|    4 |  437 | `	SXUNUSED(nArg);` |
|    4 |  438 | `	SXUNUSED(apArg);` |
|    9 |  439 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|    5 |  440 | `		ph7_result_value(pCtx,&pVm->aErrCB[1]);` |
|    3 |  441 | `	}else{` |
|    5 |  442 | `		ph7_result_null(pCtx);` |
|    - |  443 | `	}` |
|    9 |  444 | `	return PH7_OK;` |
|    1 |  445 | `}` |
|    4 |  446 | `PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  447 | `{` |
|    5 |  448 | `	ph7_vm *pVm = pCtx->pVm;` |
|    2 |  449 | `	SXUNUSED(nArg);` |
|    2 |  450 | `	SXUNUSED(apArg);` |
|    5 |  451 | `	if( ph7_value_is_callable(&pVm->aExceptionCB[1]) ){` |
|    3 |  452 | `		ph7_result_value(pCtx,&pVm->aExceptionCB[1]);` |
|    2 |  453 | `	}else{` |
|    3 |  454 | `		ph7_result_null(pCtx);` |
|    - |  455 | `	}` |
|    5 |  456 | `	return PH7_OK;` |
|    1 |  457 | `}` |
|    - |  458 | `/*` |
|    - |  459 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|    - |  460 | ` *  Generates a backtrace.` |
|    - |  461 | ` * Paramaeter` |
|    - |  462 | ` *  $options` |
|    - |  463 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|    - |  464 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|    - |  465 | ` *   all the function/method arguments, to save memory.` |
|    - |  466 | ` * $limit` |
|    - |  467 | ` *   (Not Used)` |
|    - |  468 | ` * Return` |
|    - |  469 | ` *  An array.The possible returned elements are as follows:` |
|    - |  470 | ` *          Possible returned elements from debug_backtrace()` |
|    - |  471 | ` *          Name        Type      Description` |
|    - |  472 | ` *          ------      ------     -----------` |
|    - |  473 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|    - |  474 | ` *          line        integer   The current line number. See also __LINE__.` |
|    - |  475 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|    - |  476 | ` *          class       string    The current class name. See also __CLASS__` |
|    - |  477 | ` *          object      object    The current object.` |
|    - |  478 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|    - |  479 | ` *                                If inside an included file, this lists the included file name(s).` |
|    - |  480 | ` */` |
|    - |  481 | `/*` |
|    - |  482 | ` * array error_get_last()` |
|    - |  483 | ` *  Return the last error that reached default processing, or NULL if there was none.` |
|    - |  484 | ` *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace` |
|    - |  485 | ` *  and never reported an error at all.` |
|    - |  486 | ` */` |
|    8 |  487 | `PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  488 | `{` |
|   10 |  489 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  490 | `	ph7_value *pArray,*pValue;` |
|    4 |  491 | `	SXUNUSED(nArg);` |
|    4 |  492 | `	SXUNUSED(apArg);` |
|   10 |  493 | `	if( pVm->nLastErrType == 0 ){` |
|    - |  494 | `		/* No error yet */` |
|    8 |  495 | `		ph7_result_null(pCtx);` |
|    8 |  496 | `		return PH7_OK;` |
|    - |  497 | `	}` |
|    3 |  498 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  499 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    3 |  500 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|  ! 0 |  501 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  502 | `		ph7_result_null(pCtx);` |
|  ! 0 |  503 | `		return PH7_OK;` |
|    - |  504 | `	}` |
|    3 |  505 | `	ph7_value_int(pValue,(int)pVm->nLastErrType);` |
|    3 |  506 | `	ph7_array_add_strkey_elem(pArray,"type",pValue);` |
|    4 |  507 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),` |
|    2 |  508 | `		(int)SyBlobLength(&pVm->sLastErrMsg));` |
|    3 |  509 | `	ph7_array_add_strkey_elem(pArray,"message",pValue);` |
|    3 |  510 | `	ph7_value_reset_string_cursor(pValue);` |
|    4 |  511 | `	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),` |
|    2 |  512 | `		(int)SyBlobLength(&pVm->sLastErrFile));` |
|    3 |  513 | `	ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|    3 |  514 | `	ph7_value_reset_string_cursor(pValue);` |
|    3 |  515 | `	ph7_value_int(pValue,(int)pVm->nLastErrLine);` |
|    3 |  516 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|    3 |  517 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  518 | `	return PH7_OK;` |
|    6 |  519 | `}` |
|    - |  520 | `/*` |
|    - |  521 | ` * void error_clear_last()` |
|    - |  522 | ` *  Clear the most recent error so a subsequent error_get_last() returns NULL` |
|    - |  523 | ` *  (php uses this to detect whether an operation itself raised an error).` |
|    - |  524 | ` */` |
|    6 |  525 | `PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  526 | `{` |
|    7 |  527 | `	ph7_vm *pVm = pCtx->pVm;` |
|    3 |  528 | `	SXUNUSED(nArg);` |
|    3 |  529 | `	SXUNUSED(apArg);` |
|    7 |  530 | `	pVm->nLastErrType = 0;` |
|    7 |  531 | `	pVm->nLastErrLine = 0;` |
|    7 |  532 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    7 |  533 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    7 |  534 | `	return PH7_OK;` |
|    1 |  535 | `}` |
|   10 |  536 | `PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  537 | `{` |
|   11 |  538 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  539 | `	ph7_value *pList;` |
|    - |  540 | `	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the` |
|    - |  541 | `	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */` |
|   11 |  542 | `	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;` |
|    - |  543 | `	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each` |
|    - |  544 | `	 * describing the callee (function/class) and the position of the CALL SITE.` |
|    - |  545 | `	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace` |
|    - |  546 | `	 * stamp); PH7 originally returned only the innermost frame here. */` |
|   11 |  547 | `	pList = ph7_context_new_array(pCtx);` |
|   11 |  548 | `	if( pList == 0 ){` |
|  ! 0 |  549 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  550 | `		ph7_result_null(pCtx);` |
|  ! 0 |  551 | `		SXUNUSED(nArg); /* cc warning */` |
|  ! 0 |  552 | `		SXUNUSED(apArg);` |
|  ! 0 |  553 | `		return PH7_OK;` |
|    - |  554 | `	}` |
|   11 |  555 | `	VmBuildBacktrace(&(*pVm),iOptions,pList);` |
|    - |  556 | `	/* Return the freshly created list */` |
|   11 |  557 | `	ph7_result_value(pCtx,pList);` |
|    - |  558 | `	/*` |
|    - |  559 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|    - |  560 | `	 * as soon we return from this function.` |
|    - |  561 | `	 */` |
|   11 |  562 | `	return PH7_OK;` |
|    6 |  563 | `}` |
|    - |  564 | `/*` |
|    - |  565 | ` * Generate a small backtrace.` |
|    - |  566 | ` * Store the generated dump in the given BLOB` |
|    - |  567 | ` */` |
|    4 |  568 | `static int VmMiniBacktrace(` |
|    - |  569 | `	ph7_vm *pVm, /* Target VM */` |
|    - |  570 | `	SyBlob *pOut /* Store Dump here */` |
|    - |  571 | `	)` |
|    1 |  572 | `{` |
|    5 |  573 | `	VmFrame *pFrame = pVm->pFrame;` |
|    - |  574 | `	ph7_vm_func *pFunc;` |
|    - |  575 | `	ph7_class *pClass;` |
|    - |  576 | `	SyString *pFile;` |
|    - |  577 | `	/* Called function */` |
|    5 |  578 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    5 |  579 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|    5 |  580 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|    5 |  581 | `	if( pFrame->pParent && pFunc ){` |
|    5 |  582 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|    5 |  583 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|    3 |  584 | `	}else{` |
|  ! 0 |  585 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|    - |  586 | `	}` |
|    5 |  587 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|    - |  588 | `	/* Current processed script */` |
|    5 |  589 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    5 |  590 | `	if( pFile ){` |
|    5 |  591 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|    5 |  592 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|    5 |  593 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|    5 |  594 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|    2 |  595 | `	}` |
|    - |  596 | `	/* Top class */` |
|    5 |  597 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|    5 |  598 | `	if( pClass ){` |
|  ! 0 |  599 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|  ! 0 |  600 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|  ! 0 |  601 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|  ! 0 |  602 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|  ! 0 |  603 | `	}` |
|    5 |  604 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|    - |  605 | `	/* All done */` |
|    5 |  606 | `	return SXRET_OK;` |
|    1 |  607 | `}` |
|    - |  608 | `/*` |
|    - |  609 | ` * void debug_print_backtrace()` |
|    - |  610 | ` *  Prints a backtrace` |
|    - |  611 | ` * Parameters` |
|    - |  612 | ` * None` |
|    - |  613 | ` * Return` |
|    - |  614 | ` * NULL` |
|    - |  615 | ` */` |
|    2 |  616 | `PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  617 | `{` |
|    3 |  618 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  619 | `	SyBlob sDump;` |
|    3 |  620 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|    - |  621 | `	/* Generate the backtrace */` |
|    3 |  622 | `	VmMiniBacktrace(pVm,&sDump);` |
|    - |  623 | `	/* Output backtrace */` |
|    3 |  624 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|    - |  625 | `	/* All done,cleanup */` |
|    3 |  626 | `	SyBlobRelease(&sDump);` |
|    1 |  627 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  628 | `	SXUNUSED(apArg);` |
|    3 |  629 | `	return PH7_OK;` |
|    1 |  630 | `}` |
|    - |  631 | `/*` |
|    - |  632 | ` * string debug_string_backtrace()` |
|    - |  633 | ` *  Generate a backtrace` |
|    - |  634 | ` * Parameters` |
|    - |  635 | ` * None` |
|    - |  636 | ` * Return` |
|    - |  637 | ` *  A mini backtrace().` |
|    - |  638 | ` * Note that this is a symisc extension.` |
|    - |  639 | ` */` |
|    2 |  640 | `PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  641 | `{` |
|    3 |  642 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  643 | `	SyBlob sDump;` |
|    3 |  644 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|    - |  645 | `	/* Generate the backtrace */` |
|    3 |  646 | `	VmMiniBacktrace(pVm,&sDump);` |
|    - |  647 | `	/* Return the backtrace */` |
|    3 |  648 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|    - |  649 | `	/* All done,cleanup */` |
|    3 |  650 | `	SyBlobRelease(&sDump);` |
|    1 |  651 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  652 | `	SXUNUSED(apArg);` |
|    3 |  653 | `	return PH7_OK;` |
|    1 |  654 | `}` |
|    - |  655 |  |
