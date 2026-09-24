/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stddef.h> /* NULL */
/*
 * Section:
 *    Error-handling builtins: assert, trigger_error, error_reporting,
 *    error_log, the error/exception handler get/set/restore family,
 *    error_get_last/clear_last and the debug_backtrace family.
 *    Registration rows stay in vm.c's aVmFunc[].
 * Status:
 *    Stable.
 */
/*
 * bool assert(mixed $assertion)
 *  Checks if assertion is FALSE.
 * Parameter
 *  $assertion
 *    The assertion to test.
 * Return
 *  FALSE if the assertion is false, TRUE otherwise.
 */
PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int iFlags,iResult;
	const char *zDesc;
	iFlags = pVm->iAssertFlags;
	if( iFlags & (PH7_ASSERT_DISABLE|PH7_ASSERT_ZEND_OFF) ){
		/* Assertion is disabled (assert.active=0) or compiled out
		 * (zend.assertions<1); the call is elided entirely -- even a missing
		 * argument is not diagnosed -- and it evaluates to TRUE (PHP 8). */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* PHP 8: ArgumentCountError if no arguments */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"assert() expects at least 1 argument, 0 given"
			);
	}
	/* PHP 8: No string evaluation.  All values are cast to boolean. */
	iResult = ph7_value_to_bool(apArg[0]);
	if( !iResult ){
		/* Assertion failed */
		/* Extract optional description */
		zDesc = 0;
		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
			zDesc = ph7_value_to_string(apArg[1],0);
		}
		if( iFlags & PH7_ASSERT_CALLBACK ){
			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};
			ph7_value sFile,sLine;
			ph7_value *apCbArg[3];
			SyString *pFile;
			/* Extract the processed script */
			pFile = (SyString *)SySetPeek(&pVm->aFiles);
			if( pFile == 0 ){
				pFile = (SyString *)&sFileName;
			}
			/* Invoke the callback */
			PH7_MemObjInitFromString(pVm,&sFile,pFile);
			PH7_MemObjInitFromInt(pVm,&sLine,0);
			apCbArg[0] = &sFile;
			apCbArg[1] = &sLine;
			apCbArg[2] = apArg[0];
			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);
			/* Clean-up the mess left behind */
			PH7_MemObjRelease(&sFile);
			PH7_MemObjRelease(&sLine);
		}
		if( iFlags & PH7_ASSERT_BAIL ){
			/* Abort VM execution immediately */
			return PH7_ABORT;
		}
		/* PHP 8: throw AssertionError by default */
		if( zDesc && zDesc[0] != '\0' ){
			return PH7_VmThrowException(pCtx,
				"AssertionError",
				"%s",
				zDesc
				);
		}else{
			/* php renders the assertion's compile-time SOURCE (`assert(1 == 2)`);
			 * the compiler captured it in the call-site map. An
			 * INDIRECT call (call_user_func, a callable string/variable) has no
			 * source in php either — its AssertionError carries an EMPTY message
			 * (probed php 8.5). */
			VmCallArgMap *pMap = pCtx->pArgMap;
			if( pMap && pMap->sAssertSrc.nByte > 0 ){
				return PH7_VmThrowException(pCtx,
					"AssertionError",
					"assert(%z)",
					&pMap->sAssertSrc
					);
			}
			return PH7_VmThrowException(pCtx,
				"AssertionError",
				"%s",
				""
				);
		}
	}
	/* Assertion passed */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Section:
 *  Error reporting functions.
 * Status:
 *    Stable.
 */
/*
 * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])
 *  Generates a user-level error/warning/notice message.
 * Parameters
 *  $error_msg
 *   The designated error message for this error. It's limited to 1024 characters
 *   in length. Any additional characters beyond 1024 will be truncated.
 * $error_type
 *  The designated error type for this error. It only works with the E_USER family
 *  of constants, and will default to E_USER_NOTICE.
 * Return
 *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.
 */
PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nErr = 1024; /* E_USER_NOTICE — php's default level */
	if( nArg > 0 ){
		const char *zErr;
		int nLen;
		/* Extract the error message */
		zErr = ph7_value_to_string(apArg[0],&nLen);
		if( nArg > 1 ){
			/* php 8: only the E_USER_* levels are accepted — anything else is a
			 * catchable ValueError. The RAW errno flows to the display label map
			 * and to a user error handler (php hands the handler 1024, not a
			 * translated engine severity). */
			nErr = ph7_value_to_int(apArg[1]);
			if( nErr != 256 && nErr != 512 && nErr != 1024 && nErr != 16384 ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"trigger_error(): Argument #2 ($error_level) must be one of E_USER_ERROR, E_USER_WARNING, E_USER_NOTICE, or E_USER_DEPRECATED");
			}
			if( nErr == 256 /* E_USER_ERROR */ ){
				/* php only DEPRECATES the user-fatal level; PHL rejects it. */
				return PH7_VmThrowException(pCtx,"ValueError",
					"trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead");
			}
		}
		/* Report error (consults an installed error handler, then displays) */
		PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);
		if( nErr == 256 /* E_USER_ERROR */
		 && !ph7_value_is_callable(&pCtx->pVm->sErrCB) ){
			/* php: an unhandled user fatal halts with exit 255 (pre-fix the
			 * PH7_ABORT here was overwritten by the throw's status, so
			 * E_USER_ERROR silently CONTINUED). With a handler installed the
			 * script continues — the handler-returned-false renormalization is
			 * a recorded nuance. No php stack trace either (recorded). */
			pCtx->pVm->iExitStatus = 255;
			pCtx->pVm->bHaltRequested = 1;
			return PH7_ABORT;
		}
		/* Return true */
		ph7_result_bool(pCtx,1);
	}else{
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int error_reporting([int $level])
 *  Sets which PHP errors are reported.
 * Parameters
 *  $level
 *   The new error_reporting level. It takes on either a bitmask, or named constants.
 *   Using named constants is strongly encouraged to ensure compatibility for future versions.
 *   As error levels are added, the range of integers increases, so older integer-based error
 *   levels will not always behave as expected.
 *   The available error level constants and the actual meanings of these error levels are described
 *   in the predefined constants.
 * Return
 *   Returns the old error_reporting level or the current level if no level
 *   parameter is given.
 */
PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int nOld;
	/* Extract the old reporting level */
	nOld = pVm->bErrReport ? (int)pVm->iErrMask : 0;
	if( pVm->nErrSuppress > 0 ){
		/* Inside the '@' silence operator php reports the level masked down to
		 * the errors '@' cannot suppress: E_ERROR|E_PARSE|E_CORE_ERROR|
		 * E_COMPILE_ERROR|E_USER_ERROR|E_RECOVERABLE_ERROR (== 4437). A custom
		 * error handler (e.g. PHPUnit's) consults error_reporting() to honor
		 * '@', so a suppressed warning must fall outside the returned mask. */
		nOld &= 4437;
	}
	if( nArg > 0 ){
		int nNew;
		/* Keep the LEVEL, not just an on/off bit: php masks per-severity. */
		nNew = ph7_value_to_int(apArg[0]);
		pVm->iErrMask = (sxi32)nNew;
		pVm->bErrReport = nNew != 0;
	}
	/* Return the old level */
	ph7_result_int(pCtx,nOld);
	return PH7_OK;
}
/*
 * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])
 *  Send an error message somewhere.
 * Parameter
 *  $message
 *   The error message that should be logged.
 *  $message_type
 *   Says where the error should go. The possible message types are as follows:
 *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism
 *       or a file, depending on what the error_log configuration directive is set to.
 *       This is the default option.
 *    1 message is sent by email to the address in the destination parameter.
 *      This is the only message type where the fourth parameter, extra_headers is used.
 *    2  No longer an option.
 *    3  message is appended to the file destination. A newline is not automatically added
 *       to the end of the message string.
 *    4  message is sent directly to the SAPI logging handler.
 *  $destination
 *   The destination. Its meaning depends on the message_type parameter as described above.
 *  $extra_headers
 *   The extra headers. It's used when the message_type parameter is set to 1
 * Return
 *  TRUE on success or FALSE on failure.
 *
 * This used to invoke the PH7_VM_CONFIG_ERR_LOG_HANDLER embedder callback and
 * NOTHING else: with no callback installed — which is every CLI run — the whole
 * function was a no-op that answered TRUE. `error_log($msg)` printed nothing,
 * `error_log($msg, 3, $file)` wrote no file and still said it had, and a
 * destination that could not be opened answered TRUE as well. Every "log this
 * and carry on" call in a program silently disappeared, which is the one thing
 * a logging call must not do.
 *
 * php's own routing, which is what it does now: type 3 APPENDS the message
 * verbatim to $destination (no newline added, no timestamp) and answers FALSE
 * with the open warning when it cannot; type 2 is php's ValueError; type 1 is
 * mail(), which this engine has no transport for, so it answers FALSE rather
 * than claiming a delivery; and every other value — 0, 4, and anything
 * unrecognised, all of which php sends to the SAPI logger — writes the message
 * plus a newline to the diagnostics stream. The embedder callback still wins
 * when one is installed: that is what it is for.
 */
PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMessage,*zDest,*zHeader;
	ph7_vm *pVm = pCtx->pVm;
	int iType = 0, nMsg = 0, nDest = 0;
	if( nArg < 1 ){
		/* Missing log message,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zMessage = ph7_value_to_string(apArg[0],&nMsg);
	zDest = zHeader = ""; /* Empty string */
	if( nArg > 1 ){
		iType = ph7_value_to_int(apArg[1]);
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		zDest = ph7_value_to_string(apArg[2],&nDest);
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		zHeader = ph7_value_to_string(apArg[3],0);
	}
	if( iType == 2 ){
		/* php removed the TCP/IP destination in 8.0 and refuses the type outright. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"TCP/IP option is not available for error logging");
	}
	if( pVm->xErrLog ){
		/* An embedder took the routing over (PH7_VM_CONFIG_ERR_LOG_HANDLER). */
		pVm->xErrLog(zMessage,iType,zDest,zHeader);
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( iType == 1 ){
		/* mail(): no transport in this engine, so the message did NOT go out.
		 * Answering TRUE for it was the old no-op's worst face. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#ifndef PH7_DISABLE_BUILTIN_FUNC
	if( iType == 3 ){
		/* Appended VERBATIM: php adds neither a newline nor a timestamp here. */
		if( PH7_VfsAppendFile(pCtx,zDest,zMessage,nMsg) != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	/* 0 (the configured log), 4 (the SAPI logger) and every other value: the
	 * diagnostics stream, message plus a newline — php's CLI shape with no
	 * `error_log` ini set. */
	{
		ph7_output_consumer *pCons = pVm->sVmErrConsumer.xConsumer
			? &pVm->sVmErrConsumer : &pVm->sVmConsumer;
		SyBlob sOut;
		SyBlobInit(&sOut,&pVm->sAllocator);
		SyBlobAppend(&sOut,zMessage,(sxu32)nMsg);
		SyBlobAppend(&sOut,"\n",sizeof(char));
		pCons->xConsumer(SyBlobData(&sOut),SyBlobLength(&sOut),pCons->pUserData);
		SyBlobRelease(&sOut);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * php's set_error_handler()/set_exception_handler() stack, both directions.
 *
 * PH7 kept ONE saved slot per handler, so a third `set` overwrote the first
 * one's save and the matching `restore` could not find it again; and a NULL
 * argument was treated as a failure instead of the ordinary stack entry php
 * pushes for it. Both routines below are shared by the error and the exception
 * handler because php implements them the same way.
 */
static sxi32 VmHandlerPush(
	ph7_vm *pVm,
	ph7_value *pActive,   /* the currently installed handler */
	sxi64 *piLevels,      /* its $error_levels (unused by the exception handler) */
	SySet *pStack,        /* the saved entries underneath it */
	ph7_value *pNewCb,    /* the replacement, or 0 for the `null` reset */
	sxi64 iLevels
	)
{
	VmHandlerSlot sSlot;
	sxi32 rc;
	PH7_MemObjInit(pVm,&sSlot.sCb);
	PH7_MemObjStore(pActive,&sSlot.sCb);
	sSlot.iLevels = *piLevels;
	rc = SySetPut(pStack,(const void *)&sSlot);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sSlot.sCb);
		return rc;
	}
	PH7_MemObjRelease(pActive);
	MemObjSetType(pActive,MEMOBJ_NULL);
	if( pNewCb ){
		PH7_MemObjStore(pNewCb,pActive);
	}
	*piLevels = iLevels;
	return SXRET_OK;
}
static void VmHandlerPop(ph7_vm *pVm,ph7_value *pActive,sxi64 *piLevels,SySet *pStack)
{
	VmHandlerSlot *pSlot = (VmHandlerSlot *)SySetPop(pStack);
	PH7_MemObjRelease(pActive);
	MemObjSetType(pActive,MEMOBJ_NULL);
	*piLevels = PH7_E_ALL_MASK;
	if( pSlot ){
		PH7_MemObjStore(&pSlot->sCb,pActive);
		*piLevels = pSlot->iLevels;
		PH7_MemObjRelease(&pSlot->sCb);
	}
	SXUNUSED(pVm);
}
/*
 * bool restore_exception_handler(void)
 *  Restores the previously defined exception handler function.
 * Parameter
 *  None
 * Return
 *  TRUE if the exception handler is restored.FALSE otherwise
 */
PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iLevels = PH7_E_ALL_MASK;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* An empty stack pops to NO handler: php answers TRUE either way, because the
	 * return value says "the call is valid", not "a handler was in place". */
	VmHandlerPop(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * callable set_exception_handler(callable $exception_handler)
 *  Sets a user-defined exception handler function.
 *  Sets the default exception handler if an exception is not caught within a try/catch block.
 * NOTE
 *  Execution will NOT stop after the exception_handler calls for example die/exit unlike
 *  the satndard PHP engine.
 * Parameters
 *  $exception_handler
 *   Name of the function to be called when an uncaught exception occurs.
 *   This handler function needs to accept one parameter, which will be the exception object
 *   that was thrown.
 *  Note:
 *   NULL may be passed instead, to reset this handler to its default state.
 * Return
 *  Returns the name of the previously defined exception handler, or NULL on error.
 *  If no previous handler was defined, NULL is also returned. If NULL is passed
 *  resetting the handler to its default state, TRUE is returned.
 */
PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iLevels = PH7_E_ALL_MASK;
	/* php answers the handler this call REPLACES -- the active one, not the entry
	 * saved under it. */
	if( ph7_value_is_callable(&pVm->sExceptionCB) ){
		ph7_result_value(pCtx,&pVm->sExceptionCB); /* Will make it's own copy */
	}else{
		ph7_result_null(pCtx);
	}
	if( nArg < 1 ){
		return PH7_OK;
	}
	if( !ph7_value_is_null(apArg[0]) ){
		/* php REFUSES anything else that cannot be called, naming why. PH7
		 * answered TRUE and kept the old handler. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sExceptionCB,&iLevels,&pVm->aExceptionCBSaved,
			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],PH7_E_ALL_MASK) ){
		/* Out of memory. Nothing was installed and nothing was saved, so answering
		 * the previous handler would claim a replacement that did not happen. */
		return PH7_VmThrowException(pCtx,"Error",
			"%s(): out of memory installing the handler",ph7_function_name(pCtx));
	}
	return PH7_OK;
}
/*
 * bool restore_error_handler(void)
 *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.
 * Parameters:
 *  None.
 * Return
 *  Always TRUE.
 */
PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* The popped handler's $error_levels comes back with it. An empty stack pops
	 * to NO handler; php answers TRUE either way, because the return value says
	 * "the call is valid", not "a handler was in place". */
	VmHandlerPop(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * value set_error_handler(callable $error_handler)
 *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.
 *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  Sets a user-defined error handler function.
 *  This function can be used for defining your own way of handling errors during
 *  runtime, for example in applications in which you need to do cleanup of data/files
 *  when a critical error happens, or when you need to trigger an error under certain
 *  conditions (using trigger_error()).
 * Parameters
 *  $error_handler
 *   The user function needs to accept two parameters: the error code, and a string
 *   describing the error.
 *   Then there are three optional parameters that may be supplied: the filename in which
 *   the error occurred, the line number in which the error occurred, and the context in which
 *   the error occurred (an array that points to the active symbol table at the point the error occurred).
 *   The function can be shown as:
 *    handler ( int $errno , string $errstr [, string $errfile])
 *     errno
 *       The first parameter, errno, contains the level of the error raised, as an integer.
 *   errstr
 *      The second parameter, errstr, contains the error message, as a string.
 *   errfile
 *      The third parameter is optional, errfile, which contains the filename that the error
 *     was raised in, as a string.
 *  Note:
 *   NULL may be passed instead, to reset this handler to its default state.
 * Return
 *  The handler that was ACTIVE before this call, or NULL when there was none --
 *  including for the `null` reset, which php pushes onto the handler stack like
 *  any other value rather than treating as a failure.
 */
PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iLevels;
	/* php answers the handler this call REPLACES -- the active one, not the entry
	 * saved under it. Returning the saved entry answered the handler from one
	 * level further down (and NULL for the very first replacement of a handler
	 * that was really there). */
	if( ph7_value_is_callable(&pVm->sErrCB) ){
		ph7_result_value(pCtx,&pVm->sErrCB); /* Will make it's own copy */
	}else{
		ph7_result_null(pCtx);
	}
	if( nArg < 1 ){
		return PH7_OK;
	}
	/* $error_levels rides WITH the handler: it is read at full width (php ANDs
	 * a zend_long, so 2^32+1024 still selects E_USER_NOTICE) and is pushed and
	 * popped with it. */
	iLevels = nArg > 1 ? ph7_value_to_int64(apArg[1]) : PH7_E_ALL_MASK;
	if( !ph7_value_is_null(apArg[0]) ){
		/* php REFUSES anything else that cannot be called, naming why. PH7
		 * answered TRUE and kept the old handler, so a misspelled handler name
		 * left the program reporting through the engine's own path in silence. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	/* A `null` argument is a real stack entry: it silences the handler until a
	 * restore_error_handler() pops it and brings the previous one -- with ITS
	 * levels -- back. */
	if( SXRET_OK != VmHandlerPush(pVm,&pVm->sErrCB,&pVm->iErrCBLevels,&pVm->aErrCBSaved,
			ph7_value_is_null(apArg[0]) ? 0 : apArg[0],iLevels) ){
		/* Out of memory. Nothing was installed and nothing was saved, so answering
		 * the previous handler would claim a replacement that did not happen. */
		return PH7_VmThrowException(pCtx,"Error",
			"%s(): out of memory installing the handler",ph7_function_name(pCtx));
	}
	return PH7_OK;
}
/*
 * ?callable get_error_handler(void)     -- php 8.5
 * ?callable get_exception_handler(void) -- php 8.5
 *  Return the currently installed handler callable (the ACTIVE slot [1] that the
 *  dispatch paths call), or NULL when none is set.
 */
PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ph7_value_is_callable(&pVm->sErrCB) ){
		ph7_result_value(pCtx,&pVm->sErrCB);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ph7_value_is_callable(&pVm->sExceptionCB) ){
		ph7_result_value(pCtx,&pVm->sExceptionCB);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )
 *  Generates a backtrace.
 * Paramaeter
 *  $options
 *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.
 *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus
 *   all the function/method arguments, to save memory.
 * $limit
 *   (Not Used)
 * Return
 *  An array.The possible returned elements are as follows:
 *          Possible returned elements from debug_backtrace()
 *          Name        Type      Description
 *          ------      ------     -----------
 *          function    string    The current function name. See also __FUNCTION__.
 *          line        integer   The current line number. See also __LINE__.
 *          file 	    string 	  The current file name. See also __FILE__.
 *          class       string    The current class name. See also __CLASS__
 *          object      object    The current object.
 *          args        array     If inside a function, this lists the functions arguments.
 *                                If inside an included file, this lists the included file name(s).
 */
/*
 * array error_get_last()
 *  Return the last error that reached default processing, or NULL if there was none.
 *  PH7 registered debug_backtrace() under this name, so it answered with a stack trace
 *  and never reported an error at all.
 */
PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray,*pValue;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVm->nLastErrType == 0 ){
		/* No error yet */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_value_int(pValue,(int)pVm->nLastErrType);
	ph7_array_add_strkey_elem(pArray,"type",pValue);
	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrMsg),
		(int)SyBlobLength(&pVm->sLastErrMsg));
	ph7_array_add_strkey_elem(pArray,"message",pValue);
	ph7_value_reset_string_cursor(pValue);
	ph7_value_string(pValue,(const char *)SyBlobData(&pVm->sLastErrFile),
		(int)SyBlobLength(&pVm->sLastErrFile));
	ph7_array_add_strkey_elem(pArray,"file",pValue);
	ph7_value_reset_string_cursor(pValue);
	ph7_value_int(pValue,(int)pVm->nLastErrLine);
	ph7_array_add_strkey_elem(pArray,"line",pValue);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * void error_clear_last()
 *  Clear the most recent error so a subsequent error_get_last() returns NULL
 *  (php uses this to detect whether an operation itself raised an error).
 */
PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pVm->nLastErrType = 0;
	pVm->nLastErrLine = 0;
	SyBlobReset(&pVm->sLastErrMsg);
	SyBlobReset(&pVm->sLastErrFile);
	return PH7_OK;
}
/*
 * php's $limit reaches the walk through zend_fetch_debug_backtrace's `int`
 * parameter, a plain narrowing cast -- so PHP_INT_MAX arrives as -1 and reports
 * NO frames at all, while 2^32 arrives as 0 and reports every one. Spelled
 * through unsigned arithmetic because the two's-complement wrap of an
 * out-of-range signed conversion is implementation-defined.
 */
static sxi32 VmBacktraceLimit(int nArg,ph7_value **apArg)
{
	sxu32 uL;
	if( nArg < 2 || apArg[1] == 0 ){
		return 0;
	}
	uL = (sxu32)((sxu64)ph7_value_to_int64(apArg[1]) & 0xFFFFFFFF);
	return (uL <= (sxu32)SXI32_HIGH) ? (sxi32)uL : -(sxi32)(SXU32_HIGH - uL) - 1;
}
PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pList;
	/* $options (php default DEBUG_BACKTRACE_PROVIDE_OBJECT): bit 1 attaches the
	 * frame's $this as 'object', bit 2 (IGNORE_ARGS) suppresses the 'args' list. */
	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 1 /*PROVIDE_OBJECT*/;
	/* $limit: how many frames to report, 0 meaning all of them. It was declared
	 * in aBuiltinSig[], screened as an int and then never read, so asking for the
	 * caller alone answered the WHOLE stack -- and a program that logs
	 * debug_backtrace(0, 1) per request logged the entire chain every time. */
	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);
	/* php returns a LIST of frames, innermost first -- one entry per ACTIVE call, each
	 * describing the callee (function/class) and the position of the CALL SITE.
	 * VmBuildBacktrace walks the full frame chain (shared with the Throwable trace
	 * stamp); PH7 originally returned only the innermost frame here. */
	pList = ph7_context_new_array(pCtx);
	if( pList == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);
	/* Return the freshly created list */
	ph7_result_value(pCtx,pList);
	/*
	 * Don't worry about freeing memory, everything will be released automatically
	 * as soon we return from this function.
	 */
	return PH7_OK;
}
/*
 * Generate a small backtrace.
 * Store the generated dump in the given BLOB
 */
static int VmMiniBacktrace(
	ph7_vm *pVm, /* Target VM */
	SyBlob *pOut /* Store Dump here */
	)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_vm_func *pFunc;
	ph7_class *pClass;
	SyString *pFile;
	/* Called function */
	pFrame = VmSkipExceptionFrames(pFrame);
	pFunc = (ph7_vm_func *)pFrame->pUserData;
	SyBlobAppend(pOut,"[",sizeof(char));
	if( pFrame->pParent && pFunc ){
		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);
		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);
	}else{
		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);
	}
	SyBlobAppend(pOut,"]",sizeof(char));
	/* Current processed script */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile ){
		SyBlobAppend(pOut,"[",sizeof(char));
		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);
		SyBlobAppend(pOut,pFile->zString,pFile->nByte);
		SyBlobAppend(pOut,"]",sizeof(char));
	}
	/* Top class */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass ){
		SyBlobAppend(pOut,"[",sizeof(char));
		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);
		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);
		SyBlobAppend(pOut,"]",sizeof(char));
	}
	SyBlobAppend(pOut,"\n",sizeof(char));
	/* All done */
	return SXRET_OK;
}
/*
 * void debug_print_backtrace(int $options = 0, int $limit = 0)
 *  Prints a backtrace.
 *
 *  Both arguments were declared in aBuiltinSig[] and read by nothing, and what
 *  the function PRINTED was not a backtrace at all: PH7's own
 *  `[Called function: f][Processed file: x]` line, describing ONE frame in a
 *  shape no php ever produced. php prints the same `#N file(line): func(args)`
 *  body getTraceAsString() renders -- without the `#N {main}` marker, which is
 *  the bottom of an exception's trace and not a frame -- and honours the same
 *  $options bits and $limit as debug_backtrace().
 */
PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sDump;
	ph7_value *pList;
	sxi32 iOptions = (nArg > 0 && apArg[0]) ? ph7_value_to_int(apArg[0]) : 0;
	sxi32 iLimit = VmBacktraceLimit(nArg,apArg);
	pList = ph7_context_new_array(pCtx);
	if( pList == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return PH7_OK;
	}
	VmBuildBacktrace(&(*pVm),iOptions,iLimit,pList);
	SyBlobInit(&sDump,&pVm->sAllocator);
	PH7_VmTraceToString(pVm,pList,FALSE,&sDump);
	/* Output backtrace */
	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	/* All done,cleanup */
	SyBlobRelease(&sDump);
	return PH7_OK;
}
/*
 * string debug_string_backtrace()
 *  Generate a backtrace
 * Parameters
 * None
 * Return
 *  A mini backtrace().
 * Note that this is a symisc extension.
 */
PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sDump;
	SyBlobInit(&sDump,&pVm->sAllocator);
	/* Generate the backtrace */
	VmMiniBacktrace(pVm,&sDump);
	/* Return the backtrace */
	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */
	/* All done,cleanup */
	SyBlobRelease(&sDump);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
