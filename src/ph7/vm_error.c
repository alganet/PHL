/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Error, diagnostics and type-enforcement machinery: PH7_VmThrowError
 *    and the error-handler invocation path, enum materialization and
 *    on-demand class constants, scalar/union/property/constant/return
 *    type enforcement, the TypeError/ArgumentCountError throwers,
 *    uncaught-exception rendering, VmBuildBacktrace, and the exception
 *    core VmUncaughtException/VmThrowException.
 * Status:
 *    Stable.
 */
/*
 * Remember a diagnostic for error_get_last(). php records the last error that reached
 * DEFAULT processing: one hidden by '@' or by error_reporting() still counts, but one a
 * user handler claimed (by returning true) does not -- so this is called only on the
 * default-processing path.
 */
static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)
{
	pVm->nLastErrType = iErr;
	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;
	SyBlobReset(&pVm->sLastErrMsg);
	if( zMsg && nMsg > 0 ){
		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);
	}
	SyBlobReset(&pVm->sLastErrFile);
	if( pFile ){
		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);
	}
}
/*
 * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,
 * or -- for embedders that never wired one -- the program-output consumer, so a
 * diagnostic is never silently swallowed.
 */
static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)
{
	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;
}
/*
 * Append the platform newline and hand a finished diagnostic blob to a consumer.
 * bTrack counts the bytes toward program output length (only the stdout DISPLAY
 * copy is program output; the stderr LOG copy is not, and must not perturb
 * headers_sent()/output accounting).
 */
static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)
{
	sxi32 rc;
	/* Append a new line */
#ifdef __WINNT__
	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);
#else
	SyBlobAppend(pMsg,"\n",sizeof(char));
#endif
	/* Invoke the output consumer callback */
	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);
	if( bTrack ){
		VmTrackOutput(pVm, SyBlobLength(pMsg));
	}
	return rc;
}
/*
 * Route an already-formatted diagnostic blob (the uncaught-exception path builds
 * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream
 * when log_errors is on, else to the program-output stream when display_errors
 * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).
 */
static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)
{
	if( pVm->bLogErrors ){
		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);
	}
	if( pVm->bDisplayErrors ){
		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);
	}
	return SXRET_OK;
}
/*
 * Throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error()] for additional
 * information.
 */
static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)
{
	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){
		ph7_value apArg[4];
		ph7_value *apArgPtr[4];
		ph7_value sResult;
		SyString sErr;
		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no
		 * E_* constant for. The reporting mask and the "Notice: " label already read
		 * it as E_NOTICE; the handler was the one place it leaked, so a userland
		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an
		 * `if ($errno & E_NOTICE)` test simply never fired. */
		if( iErr == PH7_CTX_NOTICE ){
			iErr = 8; /* E_NOTICE */
		}
		/* Prepare arguments */
		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);
			/* use explicit message length to avoid reading past buffer */
			SyStringInitFromBuf(&sErr,zMessage,nLen);
			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);
		if( pFile ){
			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);
			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);
		}else{
			PH7_MemObjInit(pVm,&apArg[2]);
		}
		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);
		PH7_MemObjInit(pVm,&sResult);
		/* Set up pointer array */
		apArgPtr[0] = &apArg[0];
		apArgPtr[1] = &apArg[1];
		apArgPtr[2] = &apArg[2];
		apArgPtr[3] = &apArg[3];
		/* Call the handler */
		{
			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);
			if( rcCb == PH7_EXCEPTION || rcCb == PH7_ABORT ){
				/* The handler threw (or aborted) instead of returning: php never
				 * reports the original diagnostic then — the exception supersedes
				 * it (and is routed by the boundary parking / fetch-point router).
				 * Reporting it here would print a spurious Warning AFTER the
				 * user's catch already ran. */
				PH7_MemObjRelease(&apArg[0]);
				PH7_MemObjRelease(&apArg[1]);
				PH7_MemObjRelease(&apArg[2]);
				PH7_MemObjRelease(&apArg[3]);
				PH7_MemObjRelease(&sResult);
				return FALSE;
			}
		}
		/* Check return value */
		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){
			PH7_MemObjToBool(&sResult);
		}
		/* Release */
		PH7_MemObjRelease(&apArg[0]);
		PH7_MemObjRelease(&apArg[1]);
		PH7_MemObjRelease(&apArg[2]);
		PH7_MemObjRelease(&apArg[3]);
		PH7_MemObjRelease(&sResult);
		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)
		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */
		return sResult.x.iVal == 0 ? TRUE : FALSE;
	}
	/* No handler, always call error handler */
	return TRUE;
}
/*
 * php's diagnostic label for a severity/errno (display shape; the caller
 * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and
 * raw E_* errnos passed through by builtins/deprecation sites map by value.
 * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR
 * diagnostics are PHL-specific continue-running notices php never prints, so
 * claiming php's "Fatal error" there would mislabel them — the per-diagnostic
 * severity reclassification is the remaining §6 audit tail. Note the raw
 * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this
 * only picks the DISPLAY label.
 */
/*
 * Map an internal severity onto php's error_reporting bit, then ask whether the current
 * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero
 * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed
 * every deprecation.
 */
static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)
{
	sxi32 iBit;
	if( !pVm->bErrReport ){
		return 0;
	}
	switch( iErr ){
	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */
		iBit = 2; break;
	case 512  /* E_USER_WARNING */:
		iBit = 512; break;
	case PH7_CTX_NOTICE:             /* 3 */
	case 8    /* E_NOTICE */:
		iBit = 8; break;
	case 1024 /* E_USER_NOTICE */:
		iBit = 1024; break;
	case 8192 /* E_DEPRECATED */:
		iBit = 8192; break;
	case 16384 /* E_USER_DEPRECATED */:
		iBit = 16384; break;
	case 256  /* E_USER_ERROR */:
		iBit = 256; break;
	default:
		iBit = 1; /* E_ERROR and everything else fatal-ish */
		break;
	}
	return (pVm->iErrMask & iBit) != 0;
}
static const char * VmDiagnosticLabel(sxi32 iErr)
{
	switch(iErr){
	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */
	case 512  /* E_USER_WARNING */:
		return "Warning";
	case PH7_CTX_NOTICE:           /* 3 */
	case 8    /* E_NOTICE */:
	case 1024 /* E_USER_NOTICE */:
		return "Notice";
	case 8192  /* E_DEPRECATED */:
	case 16384 /* E_USER_DEPRECATED */:
		return "Deprecated";
	case 256 /* E_USER_ERROR */:
		return "Fatal error";
	default:
		return "Error";
	}
}
/*
 * Append php's display-shape diagnostic header/trailer around a message:
 * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1
 * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;
 * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can
 * be authored cross-engine with --EXPECTF--.
 */
static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)
{
	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));
}
static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)
{
	if( pFile ){
		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,
			nLine ? nLine : 1);
	}
}
/*
 * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP `
 * prefix and TWO spaces after the colon, matching the compile-error path
 * (compile.c) and stock CLI's stderr log copy.
 */
static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)
{
	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);
	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));
}
/*
 * Prepend php's `func(): ` qualifier to a diagnostic body.
 *
 * php puts the raising function's name in the MESSAGE, not in the printed header,
 * so its user error handler ($errstr), its error_get_last()['message'] and its
 * printed copy all carry the same text. PHL used to add it in the two header
 * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that
 * matched on the function name never fired, and error_get_last() answered a body
 * php never produces. Building it into the message here is the single place that
 * fixes all three. Builtins that already spell the qualifier into their own text
 * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.
 */
static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)
{
	if( pFuncName && pFuncName->nByte > 0 ){
		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);
		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);
	}
}
/*
 * Emit a runtime diagnostic as php's two copies, each behind its own ini gate
 * (the caller has already cleared the error_reporting() mask and the '@' gate):
 *   - LOG copy     -> the error (stderr) stream when log_errors is on:
 *                     `PHP LABEL:  BODY in FILE on line N`
 *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:
 *                     `\nLABEL: BODY in FILE on line N`
 * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).
 * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,
 * keeping program stdout clean. BODY/location are shared; only the header and the
 * display copy's leading blank line differ. sWorker is reused across the two
 * copies; BODY must live in a separate buffer (it does at both call sites).
 */
static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,
	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)
{
	SyBlob *pWorker = &pVm->sWorker;
	sxi32 rc = SXRET_OK;
	if( pVm->bLogErrors ){
		SyBlobReset(pWorker);
		VmDiagnosticLogHeader(pWorker,iErr);
		SyBlobAppend(pWorker,zBody,nBody);
		VmDiagnosticLocation(pWorker,pFile,nLine);
		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);
	}
	if( pVm->bDisplayErrors ){
		sxi32 rc2;
		SyBlobReset(pWorker);
		/* php's text-mode display copy is prefixed with a blank line */
		SyBlobAppend(pWorker,"\n",sizeof(char));
		VmDiagnosticHeader(pWorker,iErr);
		SyBlobAppend(pWorker,zBody,nBody);
		VmDiagnosticLocation(pWorker,pFile,nLine);
		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);
		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather
		 * than letting a later successful write mask it */
		if( rc == SXRET_OK ){
			rc = rc2;
		}
	}
	return rc;
}
PH7_PRIVATE sxi32 PH7_VmThrowError(
	ph7_vm *pVm,         /* Target VM */
	SyString *pFuncName, /* Function name. NULL otherwise */
	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/
	const char *zMessage /* Null terminated error message */
	)
{
	SyBlob sMsg;
	SyString *pFile;
	sxu32 nMsg = (sxu32)SyStrlen(zMessage);
	sxi32 rc = SXRET_OK;
	/* Peek the processed file if available */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pFuncName && pFuncName->nByte > 0 ){
		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an
		 * out-of-memory fatal through this path with none, and must not need
		 * an allocation to say so. */
		VmDiagnosticQualify(&sMsg,pFuncName);
		SyBlobAppend(&sMsg,zMessage,nMsg);
		zMessage = (const char *)SyBlobData(&sMsg);
		nMsg = SyBlobLength(&sMsg);
	}
	/* Check for user error handler. php calls it whatever error_reporting() says
	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */
	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){
		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);
		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){
			/* error_reporting() masks a severity out of the DISPLAY, and inside
			 * '@' php still runs the handler (done just above) but prints
			 * nothing itself. */
			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);
		}
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Raise an out-of-memory fatal and request a clean VM halt.
 *
 * This is the single choke point for surfacing an allocation failure that would
 * otherwise produce a silently-wrong result (a truncated string/array returned
 * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a
 * fatal-level diagnostic, sets a nonzero process exit status, and requests a
 * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs
 * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers
 * return the value of this function (PH7_ABORT) directly, or `goto Abort` after
 * calling it from a VM op.
 */
PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)
{
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");
	/* Non-catchable, terminate with a PHP-like fatal exit status */
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	return PH7_ABORT;
}
/*
 * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.
 */
PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)
{
	return PH7_VmMemoryError(pCtx->pVm);
}
/*
 * php 8.1: an implicit float->int conversion deprecates when it loses
 * precision. Used by the integer-only OPERATORS (%, |, &, ^, <<, >>, ~),
 * which truncate their operands. An integral float like 2.0 is silent.
 * (Builtin int PARAMETERS need the same treatment — they coerce through each
 * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)
 */
/* php only DEPRECATES a lossy float->int operand (`5 % 2.7`, `3 | 1.5`); PHL targets
 * php's non-deprecated surface and rejects it with a TypeError. An INTEGRAL float
 * (`4.0 % 3`) loses nothing and is accepted. Returns SXRET_OK to continue, or the
 * throw status for the caller to route via PH7_DISPATCH_ENFORCE_RC. */
PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)
{
	double r;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_REAL) == 0 ){
		return SXRET_OK;
	}
	r = (double)pVal->rVal;
	if( r == (double)(sxi64)r ){
		return SXRET_OK;
	}
	return VmThrowFixedError(pVm,"TypeError",
		"Implicit conversion from float to int loses precision");
}
/*
 * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage
 * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows
 * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,
 * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the
 * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.
 */
PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)
{
	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is
	 * heap-bound, so only an embedder-configured cap can trip. */
	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;
}
/*
 * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack
 * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the
 * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine
 * start/resume entries (which splice frames in before that wrapper runs). Always
 * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to
 * keep native re-entries off a finite C stack).
 */
PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)
{
	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;
}
/*
 * Raise the recursion-limit fatal and request a clean VM halt. Mirrors
 * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size
 * reached": a catchable Error can't be used here because PH7 runs the catch
 * body (and renders an uncaught exception) inline at the throw-site depth —
 * which is already over the cap, so getMessage()/__toString()/the catch body
 * would re-trip the limit and recurse forever. A clean fatal removes the old
 * silent "return NULL and continue" hazard while keeping the promise that deep
 * recursion never panics: it unwinds via the abort path and still runs
 * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole
 * site testing the PHP call-depth cap); native nesting has its own fatal
 * (VmNativeNestingFatal).
 *
 * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes
 * this idempotent, so an error handler that itself recurses past the cap can't
 * re-enter and loop.
 */
PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)
{
	if( pVm->bHaltRequested ){
		return PH7_ABORT;
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);
	return PH7_ABORT;
}
/*
 * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the
 * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the
 * two limits are distinguishable. Non-catchable for the same at-depth reason.
 */
PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)
{
	if( pVm->bHaltRequested ){
		return PH7_ABORT;
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");
	return PH7_ABORT;
}
/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 */
static sxi32 VmThrowErrorAp(
	ph7_vm *pVm,         /* Target VM */
	SyString *pFuncName, /* Function name. NULL otherwise */
	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */
	const char *zFormat, /* Format message */
	va_list ap           /* Variable list of arguments */
	)
{
	SyBlob sMsg;
	SyString *pFile;
	sxi32 rc = SXRET_OK;
	/* Peek the processed file if available */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	/* Format the raw message behind php's `func(): ` qualifier */
	SyBlobInit(&sMsg, &pVm->sAllocator);
	VmDiagnosticQualify(&sMsg,pFuncName);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,
	 * whatever error_reporting() says -- the mask only gates the built-in printer,
	 * and a handler is expected to consult error_reporting() itself. Testing the
	 * mask up here instead skipped the handler entirely for a masked severity. */
	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){
		/* No handler or handler returned TRUE, normal processing — unless the
		 * expression is under '@', which suppresses the printed diagnostic. */
		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);
		if( !VmErrReportWants(pVm,iErr) || pVm->nErrSuppress > 0 ){
			SyBlobRelease(&sMsg);
			return SXRET_OK;
		}
		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),
			SyBlobLength(&sMsg),pFile,pVm->nCurLine);
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Return the class currently active on the self-stack (the innermost `self`
 * scope), or NULL when executing outside any class context.
 */
PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)
{
	if( SySetUsed(&pVm->aSelf) > 0 ){
		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);
		return apSelf[SySetUsed(&pVm->aSelf)-1];
	}
	return 0;
}
/*
 * May the engine run an exception class's __construct for a throw it is raising
 * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in
 * turn (a message-formatting error, or — the case that forced this — a
 * constructor whose own class is not method-mounted yet, which OP_CALL reports
 * as an undefined function and therefore as another engine throw). Each such
 * throw would construct another exception and recurse until the native-nesting
 * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting
 * (an engine throw from inside a user exception's constructor) working and
 * stops the self-feeding case at four levels: the innermost exception is simply
 * left with an empty message. On TRUE the caller must decrement nExcCtorDepth
 * after the call.
 */
#define VM_EXC_CTOR_MAX_DEPTH 4
static int VmExcCtorEnter(ph7_vm *pVm)
{
	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){
		return 0;
	}
	pVm->nExcCtorDepth++;
	return 1;
}
/*
 * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it
 * with the message held in *pMsg, and throw it from the current frame. Consumes
 * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the
 * class is unavailable or the engine is aborting. Shared scaffolding for the
 * typed-property / uninitialized-property / readonly error throwers.
 */
PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)
{
	ph7_class *pErrClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	VmFrame *pFrame;
	sxi32 rc;
	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);
	if( pErrClass == 0 ){
		SyBlobRelease(pMsg);
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);
	if( pThis == 0 ){
		SyBlobRelease(pMsg);
		return PH7_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);
	if( pCons && VmExcCtorEnter(&(*pVm)) ){
		ph7_value sArg;
		ph7_value *apArg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
		pVm->nExcCtorDepth--;
	}
	SyBlobRelease(pMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.
 * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that
 * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT
 * when the class is unavailable / the engine is aborting) so the caller routes the
 * result through its normal goto Exception / goto Abort.
 */
PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg, &pVm->sAllocator);
	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));
	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);
}
/*
 * Enum case singletons (PHP 8.1).
 *
 * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose
 * slot holds THE singleton instance of the enum class for that case; strict
 * `===` between two accesses is then the ordinary instance-pointer identity.
 * Materialization is lazy and all-at-once on first access, matching php: the
 * backing-value type check and the duplicate-value check only fire when a
 * case (or cases()/from()/tryFrom()) is first touched.
 */
/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the
 * readonly write-once latch so later user writes raise php's "Cannot modify
 * readonly property" through the normal store path. */

/* Return the backing value (the `value` property) of an already-materialized
 * enum case, or 0 when unavailable (pure enum / not yet materialized). */
PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)
{
	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);
	ph7_class_instance *pObj;
	SyHashEntry *pEntry;
	if( pSlot == 0 || (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pSlot->x.pOther;
	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);
	if( pEntry == 0 ){
		return 0;
	}
	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);
}
/*
 * Raise the pending self-referencing-constant Error recorded by an inner
 * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —
 * a throw inside an initializer mini-exec cannot be routed to a user catch
 * (pre-existing engine restriction), so the outermost, opcode-level evaluation
 * raises it. Returns the throw status to park/route.
 */
PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)
{
	SyBlob sMsg;
	ph7_class_attr *pAttr = pVm->pConstCycleAttr;
	ph7_class *pOwner = pVm->pConstCycleClass;
	pVm->pConstCycleAttr = 0;
	pVm->pConstCycleClass = 0;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",
		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases
 * materialize lazily and individually on first access — the backing-value
 * type check fires per case, and the duplicate-value check compares only
 * against cases that have already materialized (a broken sibling case does
 * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT
 * of a thrown catchable error — TypeError (backing type mismatch) or Error
 * (duplicate value / self-reference) — which the caller routes
 * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).
 */
PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)
{
	ph7_class_attr **apCase;
	ph7_class_instance *pObj;
	ph7_value *pSlot;
	ph7_value sBacking,sPropVal;
	sxu32 i;
	if( pCase->nIdx != SXU32_HIGH ){
		return SXRET_OK;
	}
	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){
		/* `case A = self::A->value` — record the cycle; the outermost
		 * evaluation raises it (see VmConstCycleThrow). */
		if( pVm->pConstCycleAttr == 0 ){
			pVm->pConstCycleAttr = pCase;
			pVm->pConstCycleClass = pClass;
		}
		return SXRET_OK;
	}
	PH7_MemObjInit(pVm,&sBacking);
	if( pClass->nEnumBacking != 0 ){
		if( pCase->pNativeValue ){
			/* A NATIVE enum states its backing value as a literal: there is no
			 * compiler to have emitted the byte-code branch below, and a literal
			 * is what that byte-code would have produced anyway. */
			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);
		}else if( SySetUsed(&pCase->aByteCode) > 0 ){
			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */
			ph7_class *pSaveCtx = pVm->pConstEvalClass;
			sxi32 rcExec;
			pVm->pConstEvalClass = pClass;
			pCase->iFlags |= PH7_CLASS_ATTR_EVALING;
			pVm->nConstEvalDepth++;
			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);
			pVm->nConstEvalDepth--;
			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;
			pVm->pConstEvalClass = pSaveCtx;
			if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT ){
				/* The backing expression raised: abandon materialization and
				 * hand the status to the caller to park/route. */
				PH7_MemObjRelease(&sBacking);
				return rcExec;
			}
			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){
				PH7_MemObjRelease(&sBacking);
				return VmConstCycleThrow(&(*pVm));
			}
		}
		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){
			/* php: TypeError, checked lazily at first case access */
			SyBlob sMsg;
			const char *zGiven = ph7_type_name(&sBacking);
			PH7_MemObjRelease(&sBacking);
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",
				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");
			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
		}
		if( pClass->nEnumBacking == MEMOBJ_INT ){
			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL|MEMOBJ_INT,
			 * the typed-constant leniency) to a genuine int. */
			PH7_MemObjToInteger(&sBacking);
		}else{
			PH7_MemObjToString(&sBacking);
		}
		/* php: two cases sharing one backing value are an Error — compared
		 * against already-materialized cases only (php registers values as
		 * each case evaluates). */
		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){
			ph7_value *pPrev;
			int bDup = 0;
			if( apCase[i] == pCase ){
				continue;
			}
			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);
			if( pPrev ){
				if( pClass->nEnumBacking == MEMOBJ_INT ){
					bDup = (pPrev->x.iVal == sBacking.x.iVal);
				}else{
					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)
						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),
							SyBlobLength(&sBacking.sBlob)) == 0;
				}
			}
			if( bDup ){
				/* php prints the two cases in DECLARATION order regardless of
				 * which one is being evaluated. */
				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;
				SyBlob sMsg;
				sxu32 j;
				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){
					if( apCase[j] == pCase ){ break; }
				}
				if( j < i ){
					pFirst = pCase;
					pSecond = apCase[i];
				}
				PH7_MemObjRelease(&sBacking);
				SyBlobInit(&sMsg,&pVm->sAllocator);
				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",
					&pClass->sName,&pFirst->sName,&pSecond->sName);
				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
			}
		}
	}
	/* Create the singleton and fill its readonly props */
	pObj = PH7_NewClassInstance(&(*pVm),pClass);
	if( pObj == 0 ){
		PH7_MemObjRelease(&sBacking);
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Cannot create enum case %z::%z due to a memory failure",
			&pClass->sName,&pCase->sName);
		return PH7_ABORT;
	}
	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);
	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);
	PH7_MemObjRelease(&sPropVal);
	if( pClass->nEnumBacking != 0 ){
		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);
	}
	PH7_MemObjRelease(&sBacking);
	/* Park the singleton in the case's constant slot. The slot takes over
	 * the instance's initial iRef=1 (synthesized-object invariant). */
	pSlot = PH7_ReserveMemObj(&(*pVm));
	if( pSlot == 0 ){
		PH7_ClassInstanceUnref(pObj);
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Cannot reserve a memory object for enum case %z::%z",
			&pClass->sName,&pCase->sName);
		return PH7_ABORT;
	}
	pSlot->x.pOther = pObj;
	MemObjSetType(pSlot,MEMOBJ_OBJ);
	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);
	pCase->nIdx = pSlot->nIdx;
	return SXRET_OK;
}
/*
 * Materialize EVERY case singleton of [pClass], in declaration order — the
 * cases()/from()/tryFrom() entry point (php equally evaluates all cases
 * there, so a broken case surfaces its error at the same point).
 */
PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_attr **apCase;
	sxu32 n;
	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		return SXRET_OK;
	}
	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return SXRET_OK;
}
/*
 * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,
 * or 0 when the name does not name an enum.
 */
PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)
{
	ph7_class *pClass;
	if( (pName->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pName->sBlob) < 1 ){
		return 0;
	}
	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),
		SyBlobLength(&pName->sBlob),FALSE,0);
	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		pClass = pClass->pNextName;
	}
	return pClass;
}
/*
 * The line a LAZY class initializer about to run should report a throw at: the
 * line of the access that triggered it. Answers 0 — meaning "keep the
 * initializer's own line" — when the access site is INTERNAL code (a prelude
 * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its
 * line numbers belong to an embedded source that PH7_VmStampThrowableSite will
 * not name, so reporting one would pair a foreign line with the user's file.
 */
static sxu32 VmLazyInitLineHere(ph7_vm *pVm)
{
	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;
	if( pInner && pInner->pUserData
	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){
		return 0;
	}
	return pVm->nCurLine;
}
/*
 * Hand a reserved memory-object slot back to the free list. Takes the INDEX,
 * not the pointer: aMemObj is a by-value SySet, so any nested evaluation (an
 * initializer, or the constructor of the very TypeError being raised) can grow
 * and REALLOC the pool, leaving a pointer taken before it dangling — the rule
 * VmLocalExecIntoObj is built around. The slot's contents are released first:
 * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string
 * blob / array / object left in there would be orphaned once per evaluation,
 * which for a constant that re-evaluates on every access grows without bound.
 */
static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)
{
	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	VmSlot sSlot;
	if( pObj == 0 ){
		return;
	}
	PH7_MemObjRelease(pObj);
	sSlot.nIdx = nIdx;
	sSlot.pUserData = 0;
	SySetPut(&pVm->aFreeObj,(const void *)&sSlot);
}
/*
 * Evaluate a class constant's initializer on demand.
 *
 * Constant slots are normally filled eagerly at class mount, but a constant
 * whose initializer references ANOTHER not-yet-mounted constant (same class —
 * `const B = self::A + 1` — or a class mounted later in hash order) reaches
 * OP_MEMBER with nIdx still unset; before this helper the load silently
 * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the
 * enum work, 13 Jul 2026). Evaluates the initializer now — with
 * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and
 * leaves the mount loop's later visit to skip it (nIdx already set).
 * A re-entrant evaluation of the SAME constant is php's catchable
 * "Cannot declare self-referencing constant" Error.
 */
PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	ph7_value *pMemObj;
	if( pAttr->nIdx != SXU32_HIGH
		|| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0
		|| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){
		return SXRET_OK;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){
		/* Cycle: record it for the OUTERMOST evaluation to raise
		 * (VmConstCycleThrow) — a throw at this inner level would be lost
		 * inside the initializer mini-exec. Loads NULL benignly here. */
		if( pVm->pConstCycleAttr == 0 ){
			pVm->pConstCycleAttr = pAttr;
			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
		}
		return SXRET_OK;
	}
	pMemObj = PH7_ReserveMemObj(&(*pVm));
	if( pMemObj == 0 ){
		return SXERR_MEM;
	}
	if( pAttr->pNativeValue ){
		/* A NATIVE class's constant carries a literal instead of byte-code. The
		 * mount loop materializes those, but it stopped visiting untyped constants
		 * when they went lazy (17th session), so this path — the only one an
		 * untyped constant now reaches — has to know about them too. It did not,
		 * which is why every native class constant read NULL: nothing had declared
		 * one until the date family did (DateTimeInterface::ATOM,
		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no
		 * failure path to mirror below. */
		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);
		pAttr->nIdx = pMemObj->nIdx;
		return SXRET_OK;
	}
	if( SySetUsed(&pAttr->aByteCode) > 0 ){
		ph7_class *pSaveCtx = pVm->pConstEvalClass;
		void *pSaveFrame = pVm->pConstEvalFrame;
		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;
		sxu32 nSaveLazyLine;
		sxi32 nSaveLazyDepth;
		sxu32 nSlot;
		sxi32 rcExec;
		pAttr->iFlags |= PH7_CLASS_ATTR_EVALING;
		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
		/* Mark the frame current at eval start: while it stays current, self::/
		 * parent:: in the initializer resolve to pConstEvalClass rather than the
		 * enclosing method's class (VmLocalExec pushes no frame of its own). */
		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);
		/* php evaluates this expression HERE, at the access, so that is the line a
		 * throw out of its own bytecode carries. */
		nSaveLazyLine = pVm->nLazyInitLine;
		nSaveLazyDepth = pVm->nLazyInitDepth;
		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));
		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */
		pVm->nConstEvalDepth++;
		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);
		pVm->nConstEvalDepth--;
		pVm->nLazyInitLine = nSaveLazyLine;
		pVm->nLazyInitDepth = nSaveLazyDepth;
		pVm->pConstEvalClass = pSaveCtx;
		pVm->pConstEvalFrame = pSaveFrame;
		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;
		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */
		if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT
		 || pVm->pConstCycleAttr != pSaveCycle ){
			/* The initializer FAILED. Do not memoize the slot: php evaluates a
			 * class constant's expression at each access until one of them
			 * succeeds, so `class C { const K = UNDEF; }` raises
			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the
			 * first. Memoizing left the constant reading NULL, in silence, for
			 * the rest of the run — and made a static default that named it
			 * (whose own evaluation is deferred to first access) find it
			 * materialized and raise nothing at all. Give the reserved slot back
			 * and leave nIdx unset, which is what keys the on-demand path.
			 * A recorded CYCLE is a failure the same way: it does not throw where
			 * it is found — an inner level only records it — but the value is
			 * unusable and the next access must be able to detect it again.
			 * No loop: each access runs the initializer once and raises. */
			VmRecycleMemObj(&(*pVm),nSlot);
			if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT ){
				/* Hand the status to the caller to park/route. */
				return rcExec;
			}
			if( pVm->nConstEvalDepth == 0 ){
				/* Outermost level: raise the cycle here, at opcode level, where it
				 * routes to a catch. Deeper in, the record travels outward. */
				return VmConstCycleThrow(&(*pVm));
			}
			return SXRET_OK;
		}
		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE
			 * memoizing, so a mismatch leaves the slot unmaterialized and the next
			 * access raises again — php re-runs the whole materialization each
			 * time. A pass may widen int -> float in place, which is the value
			 * memoized below. The check can THROW, and constructing that TypeError
			 * runs php code that may grow (and realloc) aMemObj — so the slot is
			 * addressed by index from here on, never through pMemObj. */
			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);
			if( rcType != SXRET_OK ){
				VmRecycleMemObj(&(*pVm),nSlot);
				return rcType;
			}
		}
		/* Memoize the value. */
		pAttr->nIdx = nSlot;
		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);
		return SXRET_OK;
	}
	pAttr->nIdx = pMemObj->nIdx;
	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);
	return SXRET_OK;
}
/*
 * Public seam for the constant-slot readers outside vm.c (reflection,
 * get_class_vars): class constants evaluate lazily, so a listing-style read
 * must materialize the slot first. Returns SXRET_OK or a throw status.
 */
PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	if( pAttr->nIdx != SXU32_HIGH || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
		return SXRET_OK;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){
		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);
	}
	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);
}
/*
 * Throw php's catchable Error for an append (`$a[] = v`) whose saturated
 * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap
 * layer; the store opcodes route the returned PH7_EXCEPTION through the
 * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.
 */
PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table
 * has no name to bind. A compile-time fatal in php (NOT a catchable Error);
 * raised at the store site here with the same message and the same
 * non-catchable outcome. Returns PH7_ABORT (dispatched via
 * PH7_DISPATCH_ENFORCE_RC at the store sites).
 */
PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)
{
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	return PH7_ABORT;
}
/*
 * Throw a PHP-compatible TypeError whose message describes a failed typed
 * property assignment. Called from the STORE path when coercion is not
 * possible.
 */
static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)
{
	ph7_class_attr *pAttr = pVmAttr->pAttr;
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;
	char zType[192];
	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,
		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	/* Prefer the declaring class over the runtime instance class so that an
	 * inherited typed property reports its original owner, matching PHP. */
	if( pOwner ){
		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",
			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);
	}else{
		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",
			zGiven,&pAttr->sName,zTypeText);
	}
	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
}
/*
 * Throw a PHP-compatible Error for reading an uninitialized typed property.
 */
PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",
		zKind,&pOwner->sName,&pAttr->sName);
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Throw the PHP-compatible Error raised on an illegal write to a readonly
 * property (PHP 8.1). bModify TRUE → a write to an already-initialized property
 * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from
 * a scope that cannot satisfy the readonly set-scope ("Cannot modify
 * protected(set) readonly property C::$x from {global scope|scope X}").
 */
/*
 * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility
 * property from a scope its set-visibility excludes:
 * "Cannot modify private(set) property C::$x from {global scope|scope X}".
 */
static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	ph7_class *pActive = VmCurrentSelf(pVm);
	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pActive ){
		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",
			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);
	}else{
		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",
			zVis,&pOwner->sName,&pAttr->sName);
	}
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Check the PHP 8.4 asymmetric set-visibility of a property write against the
 * active class scope. private(set): only the DECLARING class scope may write
 * (subclasses excluded); protected(set): the declaring class or a subclass.
 * Returns SXRET_OK when allowed, else the throw status.
 */
PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)
{
	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;
	ph7_class *pActive = VmCurrentSelf(pVm);
	int bOk;
	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){
		bOk = (pActive != 0 && pActive == pDecl);
	}else{
		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));
	}
	if( !bOk ){
		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);
	}
	return SXRET_OK;
}
static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( bModify ){
		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);
	}else{
		ph7_class *pActive = VmCurrentSelf(pVm);
		if( pActive ){
			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",
				&pOwner->sName,&pAttr->sName,&pActive->sName);
		}else{
			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",
				&pOwner->sName,&pAttr->sName);
		}
	}
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment
 * and decrement opcodes mutate the per-instance slot directly, bypassing
 * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A
 * readonly property reached by `++`/`--` is necessarily already initialized (an
 * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always
 * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the
 * PH7_EXCEPTION/PH7_ABORT produced by the throw.
 */
PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)
{
	SyHashEntry *pSlot;
	VmClassAttr *pVmAttr;
	if( nIdx == SXU32_HIGH || SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){
		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */
	}
	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));
	if( pSlot == 0 ){
		return SXRET_OK; /* Not a typed slot */
	}
	pVmAttr = (VmClassAttr *)pSlot->pUserData;
	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){
		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);
	}
	if( pVmAttr->pAttr
	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET|PH7_CLASS_ATTR_PROTECTED_SET)) ){
		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */
		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);
	}
	return SXRET_OK;
}
/*
 * Enforce a typed-property assignment. On entry pValue holds the incoming
 * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).
 * For class types, instanceof is verified.
 *
 * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION
 * after throwing TypeError, or PH7_ABORT on fatal error.
 */

/*
 * Numeric-string classification used by union weak-mode coercion. Returns:
 *   1 if the string is a strictly-numeric integer (no fraction, no exponent)
 *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)
 *   0 if it's not strictly numeric.
 */
static int VmStringNumericKind(ph7_value *pValue)
{
	const char *z, *zEnd, *zTail;
	sxu32 n;
	sxu8 bReal = 0;
	sxi32 rc;
	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	z = (const char *)SyBlobData(&pValue->sBlob);
	n = SyBlobLength(&pValue->sBlob);
	zEnd = z + n;
	if( n == 0 ) return 0;
	zTail = 0;
	rc = SyStrIsNumeric(z,n,&bReal,&zTail);
	if( rc != SXRET_OK || zTail == 0 ) return 0;
	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;
	if( zTail != zEnd ) return 0;
	return bReal ? 2 : 1;
}

/*
 * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.
 * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms
 * (they are not scalar keywords), so without this every enforcement site —
 * return, parameter, property, union alternative — would have to string-match
 * the name itself.
 * Centralising it here keeps the four sites consistent and is the single place
 * to extend when another literal/pseudo type is added.
 *   returns  1 : recognised pseudo-type AND the value satisfies it
 *            0 : recognised pseudo-type AND the value does NOT satisfy it
 *           -1 : not a pseudo-type (caller should treat sClass as a real class)
 */
PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)
{
	const char *z = pClass->zString;
	sxu32 n = pClass->nByte;
	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){
		return 1; /* `mixed` accepts any value, including null */
	}
	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){
		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;
	}
	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){
		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;
	}
	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){
		/* php's `callable` type is is_callable() itself — a function-name string,
		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke
		 * object; scope-sensitive, so a private method is callable only from
		 * inside. Same predicate the builtin answers with, so the type and
		 * is_callable() can never disagree. (Parameters and return values only:
		 * the compiler rejects `callable` on a property or class constant, as
		 * php does.) */
		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;
	}
	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){
		/* iterable === array | Traversable */
		if( pValue->iFlags & MEMOBJ_HASHMAP ){
			return 1;
		}
		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){
				return 1;
			}
		}
		return 0;
	}
	return -1;
}
/*
 * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class
 * the hint was DECLARED in, never the class the value happens to be reached
 * through. php binds the keyword where the hint is written, so
 * `class P { public function s(): self {…} }` still expects a P when called on a
 * `Q extends P` — resolving against the runtime (late-static-binding) class made
 * such a return, and a `self`-typed property written from an inherited method,
 * throw a TypeError over perfectly valid code.
 *
 * pDecl is the declaring class (0 when the site cannot name one); pUsing is the
 * composing/runtime class to stand in for a TRAIT — php flattens a trait into
 * the using class, and the trait itself sits in no instanceof hierarchy — or 0
 * to fall back to the active self. This is the same rule OP_CALL already applies
 * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one
 * VmResolveTypeClass applies to `parent`.
 */
PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)
{
	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){
		return pDecl;
	}
	return pUsing ? pUsing : VmCurrentSelf(pVm);
}
/*
 * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When
 * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive
 * scalar coercion). When bStrict is non-zero, only exact type matches are
 * accepted, plus the single implicit widening int -> float (so an int value
 * against a `float|X` union succeeds; string -> int does not).
 * Returns SXRET_OK on accept (pValue may have been mutated by the cast),
 * SXERR_INVALID on reject. Caller is responsible for the actual TypeError
 * throw.
 *
 * The class match for object values resolves `self`/`parent` alternatives
 * against *pSelf* — the DECLARING scope of the hint the union came from, which
 * every caller computes through VmHintScopeClass (the self-stack top is the
 * runtime class, which is the wrong answer for an inherited hint).
 */
/*
 * Resolve a class/interface name from a type declaration to its ph7_class*,
 * handling the `self`/`parent` aliases against the supplied scope class pSelf —
 * the class the hint was DECLARED in, which every enforcement site computes
 * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and
 * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.
 * Used by every type-enforcement site so the resolution rule — including the
 * iLoadable flag — lives in one place.
 *
 * The three keyword names are matched case-INSENSITIVELY, like every other type
 * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint
 * spelled `SELF` used to miss the exact-match arm, fall through to the class
 * table, find nothing, and leave the caller with 0 — which every caller reads as
 * "unresolvable, accept anything", so the hint enforced NOTHING.
 *
 * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-
 * compatibility target, where the type may legitimately be an interface or
 * abstract class (TRUE would filter those out → the check is skipped → any
 * object wrongly accepted). Centralizing FALSE here keeps a future caller from
 * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly
 * with TRUE; it does not go through this helper.)
 */
PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)
{
	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){
		return pSelf;
	}
	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){
		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the
		 * class the call was made through — so `P::s(): static` returning a P is a
		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT
		 * pSelf (that is where the hint was written); php allows the keyword in a
		 * return position only, but resolving it here keeps every site consistent. */
		return PH7_VmPeekTopClass(pVm);
	}
	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){
		/* A trait method's declaring class is the trait (shared by pointer); parent::
		 * resolves against the class that USED it, matching the self:: trait rule. */
		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){
			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));
		}
		return pSelf ? pSelf->pBase : 0;
	}
	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);
}
/*
 * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are
 * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with
 * no active scope, `parent` with no base class — which php rejects at COMPILE
 * time, so there is no runtime behaviour to be faithful to and the caller keeps
 * accepting. Any OTHER name that resolves to nothing is a class that does not
 * exist, and that is a mismatch (see VmClassHintMatches).
 * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,
 * `['parent','m']` — where the rule is about dispatch, not types.)
 */
static int VmHintIsScopeKeyword(const SyString *pCN)
{
	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)
		|| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)
		|| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);
}
/*
 * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the
 * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,
 * shared by the argument, variadic-element, return, property, class-constant and
 * typed-default checks — each of which then formats its own message. The
 * resolved class is handed back through *ppResolved for the message builder
 * (VmClassHintTypeName prints the resolved name, or the name as written when
 * nothing resolved).
 *
 * A name that resolves to NOTHING used to make every one of those sites skip its
 * check, so a hint naming a class that does not exist enforced nothing at all:
 * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing
 * can be an instance of a class that does not exist, so that is a mismatch.
 * A scope KEYWORD is the exception, and only half of one: with no scope to
 * resolve against there is no class to compare to — a position php rejects at
 * compile time, so any object passes — but a class hint still demands an object.
 *
 * Null never reaches here: a nullable hint accepts it at every call site before
 * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a
 * not-yet-loaded class is loaded and genuinely checked; only a name no
 * autoloader can produce fails.
 */
PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,
	ph7_value *pVal,ph7_class **ppResolved)
{
	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);
	*ppResolved = pExpected;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	if( pExpected == 0 ){
		return VmHintIsScopeKeyword(pName);
	}
	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);
}
/* A character that can be part of a type NAME, as opposed to the punctuation
 * that separates the parts of a declared type (`?A`, `A|B`, `(A&B)|null`). */
static int VmHintNameChar(int c)
{
	return !(c == '|' || c == '&' || c == '?' || c == '(' || c == ')'
		|| c == ' ' || c == '\t');
}
/*
 * Render a DECLARED type text for a message with the scope keywords RESOLVED,
 * the way php prints it: `of type self` reads `of type P`, `self|false` reads
 * `P|false`, `?static` reads `?Q`. The declared text is what the compiler
 * canonicalised (php's own member order, `?T` shorthand and all), so rewriting
 * it name-by-name keeps that shape — only the three names that cannot be
 * resolved until the call site are substituted.
 *
 * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves
 * from the ph7_class* the check already produced; this one is for the texts that
 * are only ever available as source: unions/intersections, and the property /
 * class-constant messages, which print the declared type whatever its shape.
 * An unresolvable keyword is left as written (there is no class to name).
 */
PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,
	char *zBuf,sxu32 nBuf)
{
	const char *z;
	sxu32 n, i = 0, nAt = 0;
	if( nBuf == 0 ){
		return "";
	}
	z = pDeclared ? pDeclared->zString : 0;
	n = z ? pDeclared->nByte : 0;
	while( i < n && nAt + 1 < nBuf ){
		sxu32 nStart, nCopy;
		SyString sTok;
		const SyString *pOut;
		if( !VmHintNameChar(z[i]) ){
			zBuf[nAt++] = z[i++];
			continue;
		}
		nStart = i;
		while( i < n && VmHintNameChar(z[i]) ){
			i++;
		}
		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);
		pOut = &sTok;
		if( VmHintIsScopeKeyword(&sTok) ){
			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);
			if( pRes ){
				pOut = &pRes->sName;
			}
		}
		nCopy = pOut->nByte;
		if( nCopy > nBuf - nAt - 1 ){
			nCopy = nBuf - nAt - 1;
		}
		if( nCopy > 0 ){
			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);
			nAt += nCopy;
		}
	}
	zBuf[nAt] = 0;
	return zBuf;
}
/*
 * PHL's number model flags a whole-valued real MEMOBJ_REAL|MEMOBJ_INT (the
 * float-identity leniency — see the typed-constant note above
 * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)
 * treat REAL as dominant, so such a value READS as a float. But every
 * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,
 * so an int-typed parameter / return / property / union member silently
 * kept the value LOOKING like a float where php produces a genuine int
 * (weak-mode float->int here is lossless by construction). Call this after
 * a mask ACCEPT to materialize the int. No-op for any other value/type
 * pairing — including SXU32_HIGH and int|float-style masks (REAL bit
 * present).
 *
 * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)
 * under strict_types, but the SAME dual-flagged shape also comes out of
 * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)`
 * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by
 * flags, so strict mode accepts-and-materializes both rather than
 * rejecting the php-valid `f(pow(2,3))`.
 */
PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)
{
	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0
	 && (pVal->iFlags & (MEMOBJ_INT|MEMOBJ_REAL)) == (MEMOBJ_INT|MEMOBJ_REAL) ){
		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already
		 * set, which is exactly the dual-flag case. x.iVal already holds the
		 * exact integer (MemObjTryIntger only sets the INT bit when the
		 * real->int->real round-trip is lossless); drop the REAL identity. */
		pVal->x.iVal = (sxi64)pVal->rVal;
		SyBlobRelease(&pVal->sBlob);
		MemObjSetType(pVal, MEMOBJ_INT);
	}
}
PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,
	ph7_class *pSelf)
{
	sxu32 i;
	sxu32 nAlts;
	ph7_type_alt *aAlts;
	int bHasArray, bHasObjAlt, bHasClassAlt;
	int bHasInt, bHasFloat, bHasString, bHasBool;
	int bHasIntersection = 0;
	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
	if( pValue->iFlags & MEMOBJ_NULL ){
		return bNullable ? SXRET_OK : SXERR_INVALID;
	}
	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);
	nAlts = SySetUsed(pAlts);
	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the
	 * value must match ALL its members); singleton groups are ordinary union
	 * alternatives (match ANY). Group ids are NOT dense in the stored set —
	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be
	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */
	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
	for( i = 0; i < nAlts; i++ ){
		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){
			bHasIntersection = 1;
		}
	}
	/* Intersection phase: an object satisfies an intersection group iff it is
	 * instanceof every member. Members are always class types (enforced at parse),
	 * so a non-object value can never satisfy a group. Skipped entirely for a pure
	 * union (the common case), which then pays nothing for the group machinery. */
	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){
		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
		sxu32 g;
		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){
			int bAll;
			if( aGroupCount[g] < 2 ) continue;
			bAll = 1;
			for( i = 0; i < nAlts; i++ ){
				ph7_class *pExpected;
				if( aAlts[i].nGroup != g ) continue;
				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }
				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);
				if( pExpected == 0 || !PH7_VmInstanceOf(pInst->pClass,pExpected) ){
					bAll = 0;
					break;
				}
			}
			if( bAll ) return SXRET_OK;
		}
	}
	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are
	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.
	 * A match on any one accepts the value (handles e.g. `true|int`, `?true`,
	 * `iterable|Foo`). Only singleton-group (ordinary union) atoms apply here. */
	for( i = 0; i < nAlts; i++ ){
		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
		if( aAlts[i].nType == SXU32_HIGH
		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){
			return SXRET_OK;
		}
	}
	bHasArray = bHasObjAlt = bHasClassAlt = 0;
	bHasInt = bHasFloat = bHasString = bHasBool = 0;
	for( i = 0; i < nAlts; i++ ){
		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;
		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;
		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;
		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;
		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;
		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;
		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;
	}
	/* Object handling */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		if( bHasObjAlt ) return SXRET_OK;
		if( bHasClassAlt ){
			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
			for( i = 0; i < nAlts; i++ ){
				ph7_class *pExpected;
				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;
				if( aAlts[i].nType != SXU32_HIGH ) continue;
				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);
				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){
					return SXRET_OK;
				}
			}
		}
		return SXERR_INVALID;
	}
	/* Array handling */
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		return bHasArray ? SXRET_OK : SXERR_INVALID;
	}
	/* Scalar handling — exact match first. REAL before INT: a whole-valued
	 * real carries MEMOBJ_REAL|MEMOBJ_INT (float-identity leniency), reads as
	 * a float, and must prefer a `float` member (php: 1.0 into int|float
	 * stays float); absent one, an `int` member takes it as a genuine int
	 * (php: 1.0 into int|string is int(1)) — materialize so it stops READING
	 * as a float. A pure int never carries REAL, so its arm is unaffected. */
	if( pValue->iFlags & MEMOBJ_REAL ){
		if( bHasFloat ) return SXRET_OK;
	}
	if( pValue->iFlags & MEMOBJ_INT ){
		if( bHasInt ){
			VmMaterializeIntTyped(pValue, MEMOBJ_INT);
			return SXRET_OK;
		}
	}
	if( pValue->iFlags & MEMOBJ_STRING ){
		if( bHasString ) return SXRET_OK;
	}
	if( pValue->iFlags & MEMOBJ_BOOL ){
		if( bHasBool ) return SXRET_OK;
	}
	if( bStrict ){
		/* Strict mode: only int -> float widening is allowed implicitly. */
		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){
			PH7_MemObjToReal(pValue);
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	/* Weak coercion preference order: int > float > string > bool.
	 * Numeric-string handling distinguishes integer-shaped from float-shaped
	 * to match PHP's union RFC. */
	{
		int kind = VmStringNumericKind(pValue);
		if( bHasInt ){
			/* int target accepts: bool, int (already exact), float w/o fraction,
			 * numeric-string-int. Float→int with fraction loses info → skip. */
			if( pValue->iFlags & MEMOBJ_BOOL ){
				PH7_MemObjToInteger(pValue);
				return SXRET_OK;
			}
			if( pValue->iFlags & MEMOBJ_REAL ){
				ph7_real r = pValue->rVal;
				if( r == (ph7_real)(sxi64)r ){
					PH7_MemObjToInteger(pValue);
					return SXRET_OK;
				}
			}
			if( kind == 1 ){
				PH7_MemObjToInteger(pValue);
				return SXRET_OK;
			}
		}
		if( bHasFloat ){
			if( pValue->iFlags & (MEMOBJ_BOOL|MEMOBJ_INT) ){
				PH7_MemObjToReal(pValue);
				return SXRET_OK;
			}
			if( kind == 1 || kind == 2 ){
				PH7_MemObjToReal(pValue);
				return SXRET_OK;
			}
		}
		if( bHasString ){
			if( pValue->iFlags & (MEMOBJ_BOOL|MEMOBJ_INT|MEMOBJ_REAL) ){
				PH7_MemObjToString(pValue);
				return SXRET_OK;
			}
		}
		if( bHasBool ){
			if( pValue->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_STRING) ){
				PH7_MemObjToBool(pValue);
				return SXRET_OK;
			}
		}
	}
	return SXERR_INVALID;
}

/*
 * Enforce a scalar type hint on a single argument/return value under the
 * current strict-types mode. Pre: *pVal* does not already match *nType*,
 * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).
 * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict
 * mode rejects the value. Callers throw the TypeError on rejection.
 */
PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)
{
	/* A standalone `null` type is not a weak-coercion target: only an actual
	 * null value satisfies it (and a null value matches via the flag test
	 * before this is ever called, so pVal is non-null here). Reject rather than
	 * casting the value to null — otherwise a `null`-typed parameter would
	 * silently swallow any argument. */
	if( nType == MEMOBJ_NULL ){
		return SXERR_INVALID;
	}
	/* Array and scalar never coerce into each other. php throws a TypeError
	 * rather than wrapping a scalar into a 1-element array or stringifying an
	 * array (`function f(array $a){} f(true)` — php: "must be of type array,
	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}
	 * f([1,2])` truncated the array to int(1)). The typed-property store and
	 * return-type paths already guard this inline; enforcing it here closes the
	 * same hole for typed PARAMETERS (positional/variadic/union) and generator
	 * params, which all funnel their scalar coercion through this helper. An
	 * object value against an array type is caught here too (never valid);
	 * object->scalar stays a separate case handled by the callers. */
	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){
		return SXERR_INVALID;
	}
	if( bStrict ){
		/* Only int -> float widening is allowed implicitly. */
		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){
			PH7_MemObjToReal(pVal);
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	/* Weak mode, but PHP still rejects some coercions with a TypeError rather
	 * than silently fabricating a value (mirroring the return-type path above):
	 *   - null to a non-nullable scalar (a null reaching here is always
	 *     non-nullable — the caller's guards skip nullable+null, and a
	 *     `Type $x = null` default is flagged implicitly nullable at compile time).
	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly
	 *     numeric string (optional surrounding whitespace) coerces. */
	if( pVal->iFlags & MEMOBJ_NULL ){
		return SXERR_INVALID;
	}
	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an
	 * object with __toString() coerces to a `string` parameter (its __toString
	 * is invoked by the string cast below). Every other scalar target —
	 * int/float/bool, or a `string` target on an object WITHOUT __toString —
	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced
	 * int(1)/float(1)/bool(true) for any object and "Object" for a
	 * non-stringable object passed to a string parameter. (Strict mode already
	 * rejected all of these in the bStrict block above; the array<->object case
	 * is caught by the array guard.) */
	if( pVal->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass
		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){
			return SXERR_INVALID;
		}
	}
	if( (nType == MEMOBJ_INT || nType == MEMOBJ_REAL)
		&& (pVal->iFlags & MEMOBJ_STRING)
		&& !PH7_MemObjStringIsNumeric(pVal) ){
		return SXERR_INVALID;
	}
	if( nType == MEMOBJ_INT && pVal->pVm ){
		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion
		 * (typed params, returns, typed property stores all funnel through here);
		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly
		 * like the null / non-numeric-string cases above. An INTEGRAL float loses
		 * nothing and coerces normally. */
		if( pVal->iFlags & MEMOBJ_REAL ){
			ph7_real r = pVal->rVal;
			if( r != (ph7_real)(sxi64)r ){
				return SXERR_INVALID;
			}
		}else if( pVal->iFlags & MEMOBJ_STRING ){
			SyString sStr;
			ph7_value sProbe;
			int bLossy;
			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);
			PH7_MemObjToNumeric(&sProbe);
			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;
			PH7_MemObjRelease(&sProbe);
			if( bLossy ){
				return SXERR_INVALID;
			}
		}
	}
	{
		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);
		if( xCast ) xCast(pVal);
	}
	return SXRET_OK;
}

/*
 * Render a scalar-type name suitable for the "Argument ... must be of type X"
 * TypeError message. Prefers the declared textual form when available.
 *
 * The declared SyString is length-delimited, not necessarily NUL-terminated,
 * so we bounded-copy it into the caller's *zBuf* before returning it as a
 * C string safe for "%s" formatting. If no declared text is present we fall
 * back to a static literal and ignore zBuf entirely.
 */
PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)
{
	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){
		sxu32 nCopy = SyStringLength(pDeclared);
		if( nCopy >= nBuf ) nCopy = nBuf - 1;
		if( pDeclared->zString && nCopy > 0 ){
			SyMemcpy(pDeclared->zString, zBuf, nCopy);
		}
		zBuf[nCopy] = 0;
		return zBuf;
	}
	switch( nType ){
		case MEMOBJ_INT:     return "int";
		case MEMOBJ_REAL:    return "float";
		case MEMOBJ_STRING:  return "string";
		case MEMOBJ_BOOL:    return "bool";
		case MEMOBJ_HASHMAP: return "array";
		case MEMOBJ_OBJ:     return "object";
		default:             return "scalar";
	}
}

/*
 * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint
 * the way php writes it in a TypeError:
 *
 *   - the RESOLVED class, so `self`/`parent` name the class they stand for
 *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;
 *   - `iterable` expanded to its real alternatives, `Traversable|array`;
 *   - a leading `?` whenever the hint is nullable — including the implicit
 *     `Cee $c = null` form, which php also prints as `?Cee`.
 *
 * The scalar half of this is VmScalarTypeName (which reads the declared text
 * straight off the formal, `?` included). A class hint cannot do that: its text
 * is resolved per call site, so the `?` has to be re-applied here — which is why
 * the message used to say `Cee` where php says `?Cee`.
 */
PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,
	int bNullable,char *zBuf,sxu32 nBuf)
{
	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;
	sxu32 nCopy;
	sxu32 nAt = 0;
	if( nBuf == 0 ){
		return "";
	}
	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString
	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){
		const char *zIter = bNullable ? "Traversable|array|null" : "Traversable|array";
		nCopy = SyStrlen(zIter);
		if( nCopy >= nBuf ) nCopy = nBuf - 1;
		SyMemcpy(zIter,zBuf,nCopy);
		zBuf[nCopy] = 0;
		return zBuf;
	}
	if( bNullable && nBuf > 1 ){
		zBuf[nAt++] = '?';
	}
	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;
	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;
	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);
	zBuf[nAt + nCopy] = 0;
	return zBuf;
}

/*
 * Format the class name of an object-typed ph7_value into a small caller
 * buffer, for use in TypeError messages. Returns the buffer pointer.
 */
PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)
{
	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
	SyBufferFormat(zBuf,nBuf,"%.*s",
		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);
	return zBuf;
}

PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)
{
	SyHashEntry *pSlot;
	VmClassAttr *pVmAttr;
	ph7_class_attr *pAttr;
	ph7_class *pHintScope;
	char zGivenBuf[128];
	/* php decides a typed-property store by the strict_types mode of the file the
	 * ASSIGNMENT sits in — not the class's — which is what the executing
	 * instruction's own unit mode says (pVm->bCurStrict, published under the
	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */
	int bStrict = pVm->bCurStrict ? 1 : 0;
	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));
	if( pSlot == 0 ){
		return SXRET_OK; /* Not a typed slot */
	}
	pVmAttr = (VmClassAttr *)pSlot->pUserData;
	pAttr = pVmAttr->pAttr;
	if( pAttr == 0 || (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
		return SXRET_OK;
	}
	/* `self`/`parent` in the declared type resolve against the class that DECLARED
	 * the property (a trait's members count as the composing class), not the
	 * instance's runtime class — see VmHintScopeClass. */
	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);
	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly
	 * property may be written exactly once and only from within the declaring
	 * class scope (its set-scope is protected). */
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){
		/* A readonly property is always typed and default-less, so it starts
		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*
		 * write below — making it the write-once latch (a type-rejected write
		 * leaves it set, so a later valid initialization still works). */
		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){
			/* Already initialized: any further write is forbidden, any scope —
			 * checked BEFORE the set-visibility scope, matching php's order.
			 * Exceptions that fall through to the set-scope check below:
			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and
			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,
			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */
			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;
			if( !(pCloneFr && pCloneFr->pThis
				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){
				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);
			}
		}
	}
	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET|PH7_CLASS_ATTR_PROTECTED_SET) ){
		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.
		 * An explicit set-visibility replaces readonly's implicit protected(set). */
		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);
		if( rcVis != SXRET_OK ){
			return rcVis;
		}
	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){
		/* First write (or a clone re-init) must come from within the declaring
		 * class scope (readonly's set-scope is protected — a subclass may set). */
		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;
		ph7_class *pActive = VmCurrentSelf(pVm);
		/* A readonly property imported from a TRAIT is, per php, declared in the
		 * USING class (traits are flattened in). The trait is not in any instanceof
		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */
		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){
			pDecl = pVmAttr->pOwner;
		}
		if( pActive == 0 || pDecl == 0 || !PH7_VmInstanceOf(pActive,pDecl) ){
			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);
		}
	}
	/* Union type: dispatch to the shared coercion helper, under the mode of the
	 * file the ASSIGNMENT is written in — php applies strict_types to a typed
	 * property store exactly as to an argument (`$o->u = 1.5` on an `int|string`
	 * is its TypeError there), which this used to deny outright. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,
			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,
			bStrict,pHintScope);
		if( rc == SXRET_OK ){
			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			char zBuf[128];
			return VmThrowPropertyTypeError(pVm,pVmAttr,
				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
	}
	/* NULL handling: allowed if the type is nullable, or is `mixed` (which
	 * includes null). */
	if( pValue->iFlags & MEMOBJ_NULL ){
		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)
		 || (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5
		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){
			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
			return SXRET_OK;
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");
	}
	/* standalone `null` property type (PHP 8.2): a null value was already
	 * accepted by the nullable check above, so any non-null value here is a
	 * type error. */
	if( pAttr->nType == MEMOBJ_NULL ){
		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
	}
	/* Bare 'object' type hint: accept any class instance, reject non-objects.
	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is
	 * otherwise treated as "scalar, not array" and would be rejected. */
	if( pAttr->nType == MEMOBJ_OBJ ){
		if( pValue->iFlags & MEMOBJ_OBJ ){
			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
			return SXRET_OK;
		}
		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
	}
	/* Pseudo-types stored as class-name atoms: `iterable` (array|Traversable),
	 * `true`/`false` (matching bool), `mixed` (any value — its null case is
	 * handled by the nullable check above). Checked by value before the generic
	 * class-instanceof branch, which would resolve no such class and then
	 * wrongly accept any object / reject arrays. */
	if( pAttr->nType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);
		if( rcPseudo == 1 ){
			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
		}
		/* rcPseudo == -1: real class — fall through to the instanceof branch. */
	}
	if( pAttr->nType == SXU32_HIGH ){
		/* Class / interface type. Resolve self/parent relative to the DECLARING
		 * class (pHintScope), not the instance's runtime class. */
		ph7_class *pExpected = 0;
		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){
			char zBuf[128];
			return VmThrowPropertyTypeError(pVm,pVmAttr,
				(pValue->iFlags & MEMOBJ_OBJ)
					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))
					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
		}
		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
		return SXRET_OK;
	}
	/* Scalar type, strict mode: no coercion at all, and the one widening is
	 * int -> float. The flag test the weak path below uses cannot answer this on
	 * its own — an integer-valued real carries MEMOBJ_INT as a cached
	 * representation, so `$o->i = 5.0` would read as a match — hence the value's
	 * own type is asked in ph7_type_name()'s order, float before int. */
	if( bStrict ){
		int bOk;
		if( ph7_value_is_bool(pValue) ){
			bOk = (pAttr->nType == MEMOBJ_BOOL);
		}else if( ph7_value_is_float(pValue) ){
			bOk = (pAttr->nType == MEMOBJ_REAL);
		}else if( ph7_value_is_int(pValue) ){
			bOk = (pAttr->nType == MEMOBJ_INT || pAttr->nType == MEMOBJ_REAL);
		}else if( ph7_value_is_string(pValue) ){
			bOk = (pAttr->nType == MEMOBJ_STRING);
		}else{
			/* array / resource / an object against a scalar type: no coercion in
			 * either mode, so the flag test is the whole answer (an object never
			 * carries the target's flag, and __toString is a coercion strict mode
			 * does not perform). */
			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);
		}
		if( !bOk ){
			char zObjBuf[128];
			return VmThrowPropertyTypeError(pVm,pVmAttr,
				(pValue->iFlags & MEMOBJ_OBJ)
					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))
					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
		}
		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){
			PH7_MemObjToReal(pValue); /* the int -> float widening */
		}else{
			VmMaterializeIntTyped(pValue,pAttr->nType);
		}
		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT|VM_CLASS_ATTR_TYPE_DEFER);
		return SXRET_OK;
	}
	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast
	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an
	 * object with __toString() stored into a `string` property — php coerces it
	 * via __toString, so fall through to the string cast below. */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass
		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){
			char zBuf[128];
			return VmThrowPropertyTypeError(pVm,pVmAttr,
				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));
		}
	}
	if( (pValue->iFlags & pAttr->nType) == 0 ){
		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);
		if( xCast ){
			/* Reject array<->scalar coercion to match PHP strictness */
			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
			}
			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));
			}
			/* PHP weak mode: reject string->int/float unless the string is
			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43
			 * would hide bugs and diverges from PHP's TypeError. */
			if( (pAttr->nType == MEMOBJ_INT || pAttr->nType == MEMOBJ_REAL)
			 && (pValue->iFlags & MEMOBJ_STRING)
			 && !PH7_MemObjStringIsNumeric(pValue) ){
				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");
			}
			xCast(pValue);
		}
	}else{
		/* Mask matched — an int property accepting a whole-real must
		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */
		VmMaterializeIntTyped(pValue,pAttr->nType);
	}
	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
	return SXRET_OK;
}
/*
 * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])
 * to the freshly-produced clone. The write happens in the caller's scope, so:
 *   - visibility is enforced (a private/protected property is only writable from
 *     a scope that could normally reach it — else a catchable Error),
 *   - a readonly property may be RE-initialized here (bCloneInit) provided the
 *     caller's scope could set it (else the readonly set-scope Error),
 *   - typed properties are coerced/validated exactly as a normal store, and
 *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2
 *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).
 * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.
 */
PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,
	const char *zName,sxu32 nName,ph7_value *pValue)
{
	ph7_class *pClass = pClone->pClass;
	SyHashEntry *pEntry;
	VmClassAttr *pVmAttr;
	ph7_class_attr *pAttr;
	ph7_value *pSlot;
	sxi32 rc;
	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;
	if( pEntry == 0 ){
		/* Unknown property: PHP creates a dynamic property (deprecated on a class
		 * without #[AllowDynamicProperties], but still created — the notice is a
		 * deferred residual). */
		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);
		if( pSlot == 0 ){
			return PH7_VmMemoryError(pVm);
		}
		PH7_MemObjStore(pValue,pSlot);
		return SXRET_OK;
	}
	pVmAttr = (VmClassAttr *)pEntry->pUserData;
	pAttr = pVmAttr->pAttr;
	/* Static / class-constant "properties" are not per-instance state. */
	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",
			&pClass->sName,&pAttr->sName);
		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
	}
	/* Visibility: enforced against the current (calling) frame's scope. A public
	 * readonly property passes here and is handled by the readonly set-scope check
	 * inside VmEnforcePropertyTypeOnStore below. */
	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";
		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);
		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
	}
	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */
	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Commit the (possibly coerced) value into the property slot. */
	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);
	if( pSlot ){
		PH7_MemObjStore(pValue,pSlot);
	}
	return SXRET_OK;
}
/*
 * Raise the non-catchable fatal PHP emits when a typed class constant is given
 * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it
 * prints the diagnostic, sets a nonzero exit status, requests a clean halt and
 * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).
 */
static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	char zBuf[128],zType[192];
	const char *zGiven;
	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,
		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));
	if( pValue->iFlags & MEMOBJ_OBJ ){
		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
	}else{
		zGiven = ph7_type_name(pValue);
	}
	if( bLazy ){
		/* The value only materialized at the first ACCESS, because the initializer
		 * could not be evaluated at the declaration (it named a constant that did
		 * not exist yet). php reports THAT with its own wording and its own kind:
		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of
		 * type int`, raised at the access site — not the declaration-time fatal
		 * below. The caller leaves the slot unmaterialized, so every later access
		 * re-evaluates and re-raises, as php's does. */
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",
			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);
		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
	}
	/* A class is normally mounted during the compile/VmMakeReady phase, where the
	 * code-generator's error consumer is active but the host VM output consumer is
	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,
	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").
	 * A class declared at runtime inside plain eval() reaches here with the codegen
	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer
	 * so the fatal is still reported rather than the program halting silently. */
	if( pVm->sCodeGen.xErr ){
		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,
			"Cannot use %s as value for class constant %z::%z of type %s",
			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);
	}else{
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Cannot use %s as value for class constant %z::%z of type %s",
			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);
	}
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	return SXERR_ABORT;
}
/*
 * Enforce a typed class constant's value against its declared type (PHP 8.3).
 * Unlike typed properties (weak mode), constants are checked strictly: the only
 * implicit coercion allowed is int -> float widening (so `const float X = 1` is
 * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds
 * the computed constant value (it may be widened in place). Returns SXRET_OK on
 * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.
 */
PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)
{
	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;
	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */
	if( pValue->iFlags & MEMOBJ_NULL ){
		if( bNullable || pAttr->nType == MEMOBJ_NULL ){
			return SXRET_OK;
		}
		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5
			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
	}
	/* Union type: reuse the shared coercion helper in strict mode. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,
			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
	}
	/* standalone `null` type: a non-null value is a mismatch. */
	if( pAttr->nType == MEMOBJ_NULL ){
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
	}
	/* Bare `object` type: any class instance, nothing else. */
	if( pAttr->nType == MEMOBJ_OBJ ){
		if( pValue->iFlags & MEMOBJ_OBJ ){
			return SXRET_OK;
		}
		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
	}
	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else
	 * a real class/interface verified by instanceof. */
	if( pAttr->nType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);
		if( rcPseudo == 1 ){
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
		}
		/* rcPseudo == -1: a real class/interface type. A class constant's
		 * self/parent resolve against the declaring class. */
		{
			ph7_class *pExpected = 0;
			if( !VmClassHintMatches(pVm,&pAttr->sClass,
				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){
				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
			}
		}
		return SXRET_OK;
	}
	/* Scalar type, strict: an exact flag match, or the single int -> float
	 * implicit widening. Everything else is a type error.
	 *
	 * Known lenient divergence: PHL's number model leaves a whole-valued real
	 * flagged MEMOBJ_REAL|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,
	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int`
	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)
	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so
	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal
	 * case `const int X = 1.0` is caught earlier, at definition time, by the
	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal
	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)
	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed
	 * residual needs PHL's float-identity/division model, which is out of scope. */
	if( pValue->iFlags & pAttr->nType ){
		VmMaterializeIntTyped(pValue,pAttr->nType);
		return SXRET_OK;
	}
	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){
		PH7_MemObjToReal(pValue);
		return SXRET_OK;
	}
	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);
}
/*
 * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value
 * does not match the declared type: "Cannot assign <kind> to property
 * C::$p of type T". Same value-kind naming as the constant fatal above.
 * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).
 */
static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)
{
	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
	const char *zGiven;
	char zBuf[128],zType[192];
	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,
		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));
	SyBlob sMsg;
	if( pValue->iFlags & MEMOBJ_OBJ ){
		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
	}else{
		zGiven = ph7_type_name(pValue);
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",
		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);
	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
}
/*
 * Enforce a typed INSTANCE property's computed DEFAULT value against its
 * declared type at instantiation time. php applies the typed-CONSTANT rule
 * here, not the weak store rule: the only implicit coercion is int -> float
 * widening — `public int $p = "5"` is a TypeError even in weak mode — but
 * unlike a typed constant the failure is a CATCHABLE TypeError raised when
 * the default is materialized (at `new`), php-exact since PHL evaluates
 * instance defaults per-instantiation. Matching structure of
 * VmEnforceConstantType above; only the throw differs (catchable, property
 * wording). The whole-real dual-flag leniency applies here too (php rejects
 * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a
 * php-int-producing builtin, so it accepts-and-materializes — recorded).
 * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.
 */
/*
 * The CHECK core of typed-default enforcement: validate (and possibly coerce
 * in place — int -> float widening, whole-real materialization) a computed
 * DEFAULT value against the property's declared type using the typed-CONSTANT
 * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT
 * throwing, so the static-property mount path can defer the failure (php
 * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the
 * instance path throws immediately via the wrapper below.
 */
PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)
{
	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;
	if( pValue->iFlags & MEMOBJ_NULL ){
		if( bNullable || pAttr->nType == MEMOBJ_NULL ){
			return SXRET_OK;
		}
		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5
			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,
			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	if( pAttr->nType == MEMOBJ_NULL ){
		return SXERR_INVALID;
	}
	if( pAttr->nType == MEMOBJ_OBJ ){
		if( pValue->iFlags & MEMOBJ_OBJ ){
			return SXRET_OK;
		}
		return SXERR_INVALID;
	}
	if( pAttr->nType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);
		if( rcPseudo == 1 ){
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return SXERR_INVALID;
		}
		{
			/* self/parent in the hint resolve against the declaring class. */
			ph7_class *pExpected = 0;
			if( !VmClassHintMatches(pVm,&pAttr->sClass,
				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){
				return SXERR_INVALID;
			}
		}
		return SXRET_OK;
	}
	if( pValue->iFlags & pAttr->nType ){
		VmMaterializeIntTyped(pValue,pAttr->nType);
		return SXRET_OK;
	}
	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){
		PH7_MemObjToReal(pValue);
		return SXRET_OK;
	}
	return SXERR_INVALID;
}
PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)
{
	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){
		return SXRET_OK;
	}
	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);
}
/*
 * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the
 * class chain for a static typed slot whose mount-time default failed its
 * type check, and throw php's catchable "Cannot assign <kind> to property
 * C::$s of type T" TypeError for the first one found. php evaluates static
 * defaults lazily, so the failure surfaces at the FIRST static-property
 * access (read/write/isset — any property of the class) or instantiation; a
 * never-touched class stays silent, and the throw repeats on every access
 * (the flag is not cleared — php's table materialization keeps failing too).
 * Returns SXRET_OK when nothing is pending (the class flag is only a hint),
 * else the PH7_EXCEPTION/PH7_ABORT of the throw.
 */
static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class *pScan;
	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){
		SyHashEntry *pEntry;
		SyHashResetLoopCursor(&pScan->hAttr);
		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){
			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_TYPED)) ==
				(PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_TYPED)
			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0
			 && pAttr->nIdx != SXU32_HIGH ){
				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));
				if( pSlot ){
					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;
					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){
						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
						ph7_value sNull;
						if( pValue == 0 ){
							PH7_MemObjInit(&(*pVm),&sNull);
							pValue = &sNull;
						}
						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);
					}
				}
			}
		}
	}
	return SXRET_OK;
}
/*
 * TRUE when [pClass] (or any of its bases) still owes its static table a
 * materialization: an initializer that threw at mount and was deferred
 * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check
 * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for
 * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying
 * on the flag being copied down at mount, because classes mount in hash order:
 * a subclass can be mounted before the base whose default failed.
 */
PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){
			return 1;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/*
 * Re-run the initializers of the static properties whose evaluation was
 * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).
 *
 * php builds a class's static table on first use, evaluating each slot's
 * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at
 * the declaration and raises `Undefined constant "UNDEF"` at the first access,
 * catchably, at THAT line. It also means the re-run sees the world as it is at
 * the access: a constant define()d after the class declaration resolves.
 *
 * Order is php's table order: the BASE's slots first (a subclass access raises
 * the base's broken default, not its own), then declaration order within a
 * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a
 * later failure of a SIBLING slot re-runs only what is still pending, matching
 * php's partially-materialized table. A slot that throws keeps its flag: php
 * re-raises on every access too.
 */
static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)
{
	SyHashEntry *pEntry;
	sxi32 rc;
	if( pClass->pBase ){
		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){
			/* Not pending. An inherited slot the base pass already collected
			 * lands here too (a subclass's hAttr shares the base's attribute);
			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */
			continue;
		}
		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){
			/* Pending but already RUNNING: an initializer that reads a static
			 * property re-enters this walk through OP_MEMBER, and re-running the
			 * initializer it is inside would not terminate. The in-flight slot
			 * reads as it stands, like the constant path's cycle guard — and the
			 * class keeps its hint flag so a later access retries. */
			*pbLeft = 1;
			continue;
		}
		rc = SySetPut(pOut,(const void *)&pAttr);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return SXRET_OK;
}
/*
 * Re-run the initializers of the static properties whose evaluation was
 * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).
 *
 * php builds a class's static table on first use, evaluating each slot's
 * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at
 * the declaration and raises `Undefined constant "UNDEF"` at the first access,
 * catchably, at THAT line. It also means the re-run sees the world as it is at
 * the access: a constant define()d after the class declaration resolves.
 *
 * Order is php's table order: the BASE's slots first (a subclass access raises
 * the base's broken default, not its own), then declaration order within a
 * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a
 * later failure of a SIBLING slot re-runs only what is still pending, matching
 * php's partially-materialized table. A slot that throws keeps its flag: php
 * re-raises on every access too.
 *
 * The pending slots are COLLECTED before any of them runs: an initializer is
 * user-visible execution that can re-enter this walk, and SyHash carries a
 * single shared loop cursor, so evaluating mid-walk would let the nested walk
 * cut the outer one short.
 */
static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)
{
	SySet aPending; /* ph7_class_attr * , php's static-table order */
	ph7_class_attr **apPending;
	sxu32 n,nUsed;
	sxi32 rc;
	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));
	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);
	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);
	nUsed = SySetUsed(&aPending);
	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){
		ph7_class_attr *pAttr = apPending[n];
		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
		ph7_class *pSaveCtx;
		void *pSaveFrame;
		sxu32 nSaveLazyLine;
		sxi32 nSaveLazyDepth;
		ph7_value *pMemObj;
		sxi32 rcExec;
		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){
			continue; /* the base pass already ran this shared slot */
		}
		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
		if( pMemObj == 0 ){
			continue;
		}
		pSaveCtx = pVm->pConstEvalClass;
		pSaveFrame = pVm->pConstEvalFrame;
		pVm->pConstEvalClass = pOwner;
		/* Unlike the mount pass, this runs at an arbitrary point in execution —
		 * possibly inside a METHOD of another class. Mark the frame current at
		 * eval start so self::/parent:: in the initializer resolve against the
		 * DECLARING class rather than that method's, exactly as the class-constant
		 * on-demand path does (VmLocalExec pushes no frame of its own). */
		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);
		/* The initializer runs HERE, at the access: that is the line php reports
		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */
		nSaveLazyLine = pVm->nLazyInitLine;
		nSaveLazyDepth = pVm->nLazyInitDepth;
		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));
		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */
		pAttr->iFlags |= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */
		pVm->nConstEvalDepth++;
		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);
		pVm->nConstEvalDepth--;
		pVm->nLazyInitLine = nSaveLazyLine;
		pVm->nLazyInitDepth = nSaveLazyDepth;
		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;
		pVm->pConstEvalClass = pSaveCtx;
		pVm->pConstEvalFrame = pSaveFrame;
		if( rcExec == PH7_EXCEPTION || rcExec == PH7_ABORT ){
			/* Raised at the access site, where it belongs: hand the status to the
			 * caller to route (a catch here is the user's own). */
			rc = rcExec;
			break;
		}
		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;
		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){
			/* The initializer named a self-referencing constant. Like the mount
			 * path, the innermost evaluation only RECORDS it; raise it here, at
			 * the access, where a catch can see it. */
			rc = VmConstCycleThrow(&(*pVm));
			break;
		}
		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)
		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
			/* Now that a value exists, apply the typed-default rule the mount pass
			 * had to skip. Flag the slot as well so every later access re-throws
			 * through the deferred-type scan, as php's failing materialization does. */
			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));
			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){
				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;
				pVmAttr->iState |= VM_CLASS_ATTR_TYPE_DEFER;
				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);
				break;
			}
		}
	}
	if( rc != SXRET_OK ){
		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */
	}
	SySetRelease(&aPending);
	return rc;
}
/*
 * Materialize [pClass]'s static table, php's way: evaluate whatever the mount
 * pass deferred, then raise any typed-default failure. Called by the sites php
 * materializes at — the first static-PROPERTY access (read, write, isset; a
 * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and
 * instantiation. Returns SXRET_OK when the table is (or already was) whole,
 * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.
 */
PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)
{
	int bLeft = 0;
	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);
	if( rc == SXRET_OK ){
		rc = VmThrowDeferredStaticType(&(*pVm),pClass);
	}
	if( rc == SXRET_OK && !bLeft ){
		/* The table is whole and nothing failed: retire the hint on the whole
		 * chain, so the ordinary static accesses that follow stop paying for the
		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it
		 * set — php's materialization keeps failing too. */
		ph7_class *pScan;
		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){
			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;
		}
	}
	return rc;
}

/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 * ------------------------------------
 * Simple boring wrapper function.
 * ------------------------------------
 */
PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
	va_start(ap,zFormat);
	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * Throw a TypeError exception from within the VM execution loop.
 * Used for user-defined function type hint violations (e.g. object type hint).
 */
PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyBlob sMsg;
	SyString sMsgStr;
	SyString *pFuncName = &pCallee->sName;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free
	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */
	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name
	 * for a VARIADIC-collected element (many values share the one variadic
	 * formal, so no single name applies) — "Argument #2 must be of type …". */
	if( pOwnerClass ){
		/* A property hook is named after its PROPERTY, never after the method
		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */
		SyBlob sHook;
		SyBlobInit(&sHook,&pVm->sAllocator);
		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){
			if( pArgName ){
				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",
					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),
					nArg,pArgName,zExpected,zGiven);
			}else{
				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",
					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),
					nArg,zExpected,zGiven);
			}
			SyBlobRelease(&sHook);
			goto ArgMsgBuilt;
		}
		SyBlobRelease(&sHook);
		if( pArgName ){
			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",
				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);
		}else{
			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",
				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);
		}
	}else{
		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */
		const char *zShow = 0;
		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);
		if( pArgName ){
			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",
				nShow,zShow,nArg,pArgName,zExpected,zGiven);
		}else{
			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",
				nShow,zShow,nArg,zExpected,zGiven);
		}
	}
ArgMsgBuilt:
	/* php appends the CALL SITE to a userland callee's type error — internal
	 * (hosted C) functions get the bare message. nCurLine is the line of the
	 * call instruction being bound, which is exactly php's "called in". */
	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){
		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);
		if( pCallFile && pCallFile->nByte > 0 ){
			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);
		}
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Type-check (and weak-mode coerce, in place) ONE element collected into a
 * variadic parameter — the shared per-element enforcement for BOTH the
 * positional and the named-argument binding paths of OP_CALL.
 *
 * nArgPos is php's 1-based argument number for the message: a positional
 * element uses its overall call position; a NAMED element always reports
 * (total positional args) + 1, whichever named element fails. The `($name)`
 * clause is omitted (pArgName = 0): many values share the one variadic
 * formal, so no single parameter name applies.
 *
 * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or
 * PH7_EXCEPTION after throwing php's TypeError otherwise.
 */
PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,
	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)
{
	sxi32 rc;
	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){
		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,
			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){
			const char *zGiven;
			const char *zExpected = "union";
			char zBuf[128];
			char zTypeBuf[128];
			if( pVal->iFlags & MEMOBJ_OBJ ){
				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));
			}else if( pVal->iFlags & MEMOBJ_NULL ){
				zGiven = "null";
			}else{
				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));
			}
			if( SyStringLength(&pFormal->sTypeName) > 0 ){
				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,
					zTypeBuf,sizeof(zTypeBuf));
			}
			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);
			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		return SXRET_OK;
	}
	if( pFormal->nType < 1
	 || ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){
		return SXRET_OK;
	}
	if( pFormal->nType == SXU32_HIGH ){
		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced
		 * per element exactly like the non-variadic paths. */
		SyString *pName = &pFormal->sClass;
		ph7_class *pClass;
		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);
		if( rcPseudo == 0 ){
			/* Recognised pseudo-type; value mismatches */
			char zTypeBuf[128],zGivenBuf[128];
			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,
				VmClassHintTypeName(pName,0,
					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put
		 * through the shared VmClassHintMatches rule (self/parent/static,
		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves
		 * to nothing). Non-nullable here — the guard above skips nullable+null —
		 * so ANY non-object is a TypeError, matching php. */
		pClass = 0;
		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){
			char zTypeBuf[128],zGivenBuf[128];
			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,
				VmClassHintTypeName(pName,pClass,
					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		return SXRET_OK;
	}
	if( (pVal->iFlags & pFormal->nType) == 0 ){
		if( pFormal->nType == MEMOBJ_OBJ ){
			char zGivenBuf[128];
			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,
				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){
			char zTypeBuf[128];
			char zGivenBuf[128];
			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,
				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));
			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
	}else{
		/* Mask matched — an int variadic accepting a whole-real materializes
		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */
		VmMaterializeIntTyped(pVal,pFormal->nType);
	}
	return SXRET_OK;
}
/*
 * Count php's REQUIRED arity for a user function: formals up to and including
 * the LAST one with no default value (php 8 treats an optional declared
 * before a required parameter as implicitly required), excluding a trailing
 * variadic. Also reports the total non-variadic formal count so callers can
 * pick php's wording — "exactly N expected" when required == total,
 * "at least N" when trailing optionals exist.
 */
PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)
{
	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	sxu32 nFormal = SySetUsed(&pFunc->aArgs);
	sxu32 nRequired = 0;
	sxu32 n;
	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){
		nFormal--;
	}
	for( n = 0 ; n < nFormal ; ++n ){
		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){
			nRequired = n + 1;
		}
	}
	*pnNonVariadic = nFormal;
	return nRequired;
}
/*
 * Throw php's catchable ArgumentCountError for a user function/method called
 * with too few arguments:
 *   Too few arguments to function C::f(), N passed in FILE on line L and
 *   {exactly|at least} M expected
 * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line
 * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a
 * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals
 * when line tracking lands. bCallSite=FALSE omits the segment entirely,
 * matching php for Fiber::start() (no userland call site in the message).
 */
PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,
	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)
{
	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",
			&pOwnerClass->sName,pFuncName,nPassed);
	}else{
		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);
	}
	if( bCallSite ){
		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);
		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);
	}
	SyBlobFormat(&sMsg," and %s %u expected",
		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);
	/* VmThrowBuiltinError consumes (releases) sMsg */
	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);
}
/*
 * Throw php's catchable Error for a by-reference parameter handed something that
 * cannot be referenced:
 *   C::m(): Argument #1 ($x) could not be passed by reference
 * php refuses this at the CALL, before the callee's ZPP runs, and it decides from
 * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from
 * the value that arrived. The class prefix follows the same rule as the too-few
 * ArgumentCountError above — php names the method's owner, and PHL used to report
 * the bare `m()`.
 */
PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,
	sxu32 nArgPos,SyString *pArgName)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) could not be passed by reference",
			&pOwnerClass->sName,pFuncName,nArgPos,pArgName);
	}else{
		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) could not be passed by reference",
			pFuncName,nArgPos,pArgName);
	}
	/* VmThrowBuiltinError consumes (releases) sMsg */
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method
 * called with too few arguments, in php's ZPP wording:
 *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given
 * (php words internal callables this way — no call-site segment, argument(s)
 * pluralized on the expected count).
 *
 * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP
 * says "exactly" only when the callable accepts no more than it requires, and
 * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is
 * NOT the user-function rule VmThrowTooFewArgs applies: php words
 * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.
 */
PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,
	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)
{
	SyBlob sMsg;
	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";
	const char *zPlural = (nRequired == 1) ? "" : "s";
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",
			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);
	}else{
		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",
			pFuncName,zKind,nRequired,zPlural,nPassed);
	}
	/* VmThrowBuiltinError consumes (releases) sMsg */
	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);
}
/*
 * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too
 * MANY arguments, in php's ZPP wording:
 *   count_chars() expects at most 2 arguments, 3 given
 * "exactly" when the bounds coincide, "at most" when optional parameters make
 * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin
 * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,
 * so the caller must not route such a callee here.
 */
PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,
	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)
{
	SyBlob sMsg;
	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";
	const char *zPlural = (nMax == 1) ? "" : "s";
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",
			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);
	}else{
		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",
			pFuncName,zKind,nMax,zPlural,nPassed);
	}
	/* VmThrowBuiltinError consumes (releases) sMsg */
	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);
}
/*
 * Throw php's named-call ArgumentCountError for a required parameter no
 * named or positional argument resolved to:
 *   C::f(): Argument #N ($x) not passed
 * (php's named-hole shape — no file/line or expected-count segment).
 */
PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,
	sxu32 nArg,SyString *pArgName)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	if( pOwnerClass ){
		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",
			&pOwnerClass->sName,pFuncName,nArg,pArgName);
	}else{
		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);
	}
	/* VmThrowBuiltinError consumes (releases) sMsg */
	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);
}
/*
 * Throw a PHP-compatible TypeError describing a return-value type mismatch.
 * Message format: "funcname(): Return value must be of type X, Y returned".
 */
/* Build a catchable TypeError from a pre-formatted message blob and throw it.
 * The message is copied into the instance by __construct, so the caller owns
 * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */
static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * php names a property HOOK after the property it belongs to — `C::$p::get()`,
 * `C::$p::set()` — never after a method, because in php a hook is not one. PHL
 * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /
 * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a
 * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every
 * other callee falls through to the ordinary Class::method rendering.
 */
#define PH7_HOOK_METH_PFX "__phl_hook_"
PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)
{
	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;
	if( pName->zString == 0 || pName->nByte <= nPfx + 4 ){
		return 0;
	}
	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){
		return 0;
	}
	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){
		*pzKind = "get";
	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){
		*pzKind = "set";
	}else{
		return 0;
	}
	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));
	return 1;
}
PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)
{
	SyString sProp;
	const char *zKind;
	if( pClass == 0 || !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){
		return 0;
	}
	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);
	return 1;
}
/*
 * The callee name php puts in front of a return-side message. A METHOD is
 * qualified with its DECLARING class ("P::m", even when called on a subclass);
 * anything else uses its display name (which is also what strips a closure's
 * internal key). The argument-side messages take the owner class as a parameter
 * instead — they are thrown from call sites that already resolved it.
 */
static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)
{
	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){
			return;
		}
		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);
		return;
	}
	{
		const char *zShow = 0;
		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);
		if( zShow && nShow > 0 ){
			SyBlobAppend(pOut,zShow,(sxu32)nShow);
		}
	}
}
static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)
{
	SyBlob sMsg,sName;
	sxi32 rc;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobInit(&sName,&pVm->sAllocator);
	VmReturnFuncName(pVm,pFunc,&sName);
	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",
		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);
	rc = VmThrowTypeErrorMsg(pVm,&sMsg);
	SyBlobRelease(&sName);
	SyBlobRelease(&sMsg);
	return rc;
}
/* A never-returning function that returned normally (fall-off). PHP bans an
 * explicit `return` at compile time, so this fires only for an implicit return.
 * php calls it a "method" when it is one. */
static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)
{
	SyBlob sMsg,sName;
	sxi32 rc;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobInit(&sName,&pVm->sAllocator);
	VmReturnFuncName(pVm,pFunc,&sName);
	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",
		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),
		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");
	rc = VmThrowTypeErrorMsg(pVm,&sMsg);
	SyBlobRelease(&sName);
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * Format the "X given" portion of error messages following PHP's value-name
 * convention: "true"/"false" for booleans, class name for objects, otherwise
 * the bare type name. zBuf must hold at least 64 bytes.
 */
PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)
{
	if( pVal->iFlags & MEMOBJ_BOOL ){
		return pVal->x.iVal ? "true" : "false";
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
		if( pThis && pThis->pClass ){
			SyString *pName = &pThis->pClass->sName;
			sxu32 n = pName->nByte;
			if( n >= nBuf ){
				n = nBuf - 1;
			}
			SyMemcpy(pName->zString,zBuf,n);
			zBuf[n] = 0;
			return zBuf;
		}
		return "object";
	}
	return ph7_type_name(pVal);
}
/*
 * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a
 * non-array value at runtime. Matches the message and class PHP raises
 * ("Only arrays and Traversables can be unpacked, X given"). The class is
 * \TypeError for objects, \Error otherwise — matching PHP's distinction.
 */
PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)
{
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyBlob sMsg;
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;
	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";
	char zNameBuf[64];
	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));
	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);
	if( pClass == 0 ){
		return PH7_ABORT;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ABORT;
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Enforce the declared return type of *pFunc* against the value returned
 * (or NULL if the function returned without a value). Mutates *pValue* to
 * perform allowed widening (int->float) or weak-mode coercion. On
 * violation, throws TypeError and returns PH7_EXCEPTION.
 */
/*
 * TRUE if a function declares a return type that must be enforced — a single
 * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is
 * left 0). The return-enforcement gates must consult both, not just the single
 * type field.
 */
PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)
{
	return pFunc->nReturnType > 0 || SySetUsed(&pFunc->aReturnUnion) > 0;
}
PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)
{
	int bStrict = pFunc->bStrictTypes ? 1 : 0;
	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;
	const char *zGiven;
	ph7_class *pHintScope;
	char zBuf[128];
	char zTypeBuf[128];
	/* Untyped function: no enforcement (no single type and no union/intersection). */
	if( !VmFuncHasReturnType(pFunc) ){
		return SXRET_OK;
	}
	/* never return type: the function must not return at all. An explicit
	 * `return` is a compile error, so reaching here means the function ran off
	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at
	 * the call site). */
	if( pFunc->nReturnType == MEMOBJ_NEVER ){
		return VmThrowNeverReturnError(pVm,pFunc);
	}
	/* void return type: the function must not produce a value. */
	if( pFunc->nReturnType == MEMOBJ_VOID ){
		if( pValue == 0 ){
			return SXRET_OK;
		}
		/* `set => expr` is php's SHORTHAND for assigning expr to the backing
		 * store, not a return: php compiles no return statement there at all,
		 * and still reports the hook's return type as void. PHL carries the
		 * value out of the hook body to hand it to the write-back dispatcher,
		 * so the one implicit value this arm must not reject is that one. */
		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){
			return SXRET_OK;
		}
		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL
		 * still counts as "returned a value" here. */
		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);
		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);
	}
	/* Fell off the end or a bare `return;` with no value: PHP requires any typed
	 * return (even a nullable one) to return a value explicitly — only an explicit
	 * `return null;` satisfies a nullable type, which is handled below. */
	if( pValue == 0 ){
		const char *zExpected = "value";
		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,
				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));
		}
		/* php's word for "no value at all" is `none`, not `null` — `null returned`
		 * is what it says for an explicit `return null;`, a different program. */
		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");
	}
	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a
	 * TypeError. (Falling off the end is handled by the generic check above,
	 * matching how every other typed return reports a missing value.) */
	if( pFunc->nReturnType == MEMOBJ_NULL ){
		if( pValue->iFlags & MEMOBJ_NULL ){
			return SXRET_OK;
		}
		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",
			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
	}
	/* An explicit `return null` satisfies any nullable return type (`?T`, `T|null`,
	 * `A|B|null`) uniformly — handle it before the per-shape branches below, none
	 * of which (scalar/class/union) carry their own nullable check. */
	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){
		return SXRET_OK;
	}
	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),
	 * `true`/`false` (the matching bool literal), `iterable` (array|Traversable).
	 * Check by value before the real-class instanceof branch below. */
	if( pFunc->nReturnType == SXU32_HIGH ){
		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);
		if( rcPseudo == 1 ){
			return SXRET_OK;
		}
		if( rcPseudo == 0 ){
			return VmThrowTypeErrorForReturn(pVm,pFunc,
				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
		}
		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */
	}
	/* The two branches below are the only ones that can name a class, so the
	 * hint scope is resolved here rather than on every scalar/void return.
	 * `self`/`parent` in a return hint resolve against the class that DECLARED
	 * the callee — the check runs inside the callee's own frame, so the same walk
	 * `self::` uses answers it (a closure's bound scope included). The self-STACK
	 * top is the late-static-binding class, which made an inherited `: self`
	 * demand the SUBCLASS. See VmHintScopeClass. */
	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);
	/* Union/intersection return type — delegate. A null alternative is not stored
	 * in aReturnUnion (dropped at parse), so nullability comes from the func's
	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */
	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){
		sxi32 rcU;
		const char *zExpected = "union";
		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);
		if( rcU == SXRET_OK ){
			return SXRET_OK;
		}
		if( pValue->iFlags & MEMOBJ_OBJ ){
			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
		}else if( pValue->iFlags & MEMOBJ_NULL ){
			zGiven = "null";
		}else{
			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));
		}
		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,
				zTypeBuf,sizeof(zTypeBuf));
		}
		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);
	}
	/* Class return type — instanceof check. The class name is a length-
	 * delimited SyString; copy it into a local buffer before formatting
	 * it into the TypeError message. */
	if( pFunc->nReturnType == SXU32_HIGH ){
		SyString *pClassName = &pFunc->sReturnClass;
		ph7_class *pExpected = 0;
		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){
			if( pValue->iFlags & MEMOBJ_OBJ ){
				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
			}else{
				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));
			}
			return VmThrowTypeErrorForReturn(pVm,pFunc,
				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);
		}
		return SXRET_OK;
	}
	/* Scalar return type. A nullable scalar accepting null was already handled by
	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a
	 * non-nullable scalar return — a TypeError. */
	if( pValue->iFlags & MEMOBJ_NULL ){
		return VmThrowTypeErrorForReturn(pVm,pFunc,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			"null");
	}
	/* Exact match? Done. An `: int` return accepting a whole-real
	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */
	if( pValue->iFlags & pFunc->nReturnType ){
		VmMaterializeIntTyped(pValue,pFunc->nReturnType);
		return SXRET_OK;
	}
	/* Object->scalar is never compatible, EXCEPT an object with __toString()
	 * returned as a `string`: php coerces it in weak mode. Fall through to
	 * VmEnforceScalarType below, which invokes __toString in weak mode and
	 * still rejects the object under strict_types. */
	if( pValue->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;
		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass
		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){
			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));
			return VmThrowTypeErrorForReturn(pVm,pFunc,
				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
				zGiven);
		}
	}
	/* Array <-> scalar is never compatible. */
	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){
		return VmThrowTypeErrorForReturn(pVm,pFunc,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
	}
	/* PHP's weak-mode rule: string -> int/float is allowed only if the
	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43
	 * would hide the bug and diverges from PHP. Strict mode falls through
	 * to VmEnforceScalarType below which rejects string->int outright. */
	if( !bStrict
	 && (pFunc->nReturnType == MEMOBJ_INT || pFunc->nReturnType == MEMOBJ_REAL)
	 && (pValue->iFlags & MEMOBJ_STRING)
	 && !PH7_MemObjStringIsNumeric(pValue) ){
		return VmThrowTypeErrorForReturn(pVm,pFunc,
			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
			"string");
	}
	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){
		return SXRET_OK;
	}
	return VmThrowTypeErrorForReturn(pVm,pFunc,
		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),
		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));
}
/*
 * Report a fatal named-argument error.
 * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.
 */
PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)
{
	SyBlob sMsg;
	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)`
	 * runs the catch), so build a real exception object and hand the resulting
	 * PH7_EXCEPTION back for the caller to route. This used to report straight
	 * to the uncaught renderer, which made every named-argument mistake an
	 * unconditional fatal even inside try/catch. */
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobAppend(&sMsg,zMsg,nMsg);
	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
}
/*
 * Format and throw a run-time error and invoke the supplied VM output consumer callback.
 * Refer to the implementation of [ph7_context_throw_error_format()] for additional
 * information.
 * ------------------------------------
 * Simple boring wrapper function.
 * ------------------------------------
 */
PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)
{
	sxi32 rc;
	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);
	return rc;
}
/*
 * Resolve function context from the current frame.
 */
/*
 * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name
 * ("[closure_3]") that doubles as their lookup key; php reports them as
 * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or
 * straight at the function's own name otherwise.
 */
PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)
{
	const char *zName = pFunc->sName.zString;
	int nName = (int)pFunc->sName.nByte;
	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)
		|| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);
	/* A property hook is not a method in php and never shows the name PHL
	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what
	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace
	 * report from inside one, and what makes the trace line read
	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's
	 * `C::$p::get` for free. */
	{
		SyString sProp;
		const char *zKind;
		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){
			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),
				"$%z::%s",&sProp,zKind);
			*pzOut = pVm->zDisplayName;
			return n;
		}
	}
	if( bClosure ){
		int n;
		if( pFunc->sClosureName.nByte > 0 ){
			/* The compiler built php's name: it words the ENCLOSING scope, which a
			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */
			*pzOut = pFunc->sClosureName.zString;
			return (int)pFunc->sClosureName.nByte;
		}
		if( pFunc->sFile.nByte > 0 ){
			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),
				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);
		}else{
			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");
		}
		*pzOut = pVm->zDisplayName;
		return n;
	}
	*pzOut = zName;
	return nName;
}
PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)
{
	VmFrame *pFrame;
	ph7_vm_func *pFunc;
	*pzFuncName = 0;
	*pnFuncLen = 0;
	pFrame = pVm->pFrame;
	if( pFrame == 0 ){
		return;
	}
	pFrame = VmSkipExceptionFrames(pFrame);
	if( pFrame->pParent == 0 ){
		return;
	}
	pFunc = (ph7_vm_func *)pFrame->pUserData;
	if( pFunc == 0 ){
		return;
	}
	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);
}
/*
 * Append the exception's own rendered stack trace (php's "#N file(line):
 * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it
 * emitted a trace. The instance's trace walks the FULL frame chain, so this is
 * what the uncaught report should print; getTraceAsString() lives in the
 * built-in library and already produces php's exact byte format, which keeps
 * one renderer for both the caught (userland) and uncaught (engine) paths.
 * Returns 0 when there is no instance or no such method, leaving the caller to
 * synthesize what it can.
 */
static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)
{
	ph7_class_method *pGetTrace;
	ph7_value sTrace;
	const char *zTmp;
	int nTmp;
	int bDone = 0;
	int bSaved;
	if( pThis == 0 ){
		return 0;
	}
	if( pVm->bRenderingUncaught ){
		/* Already inside a report: do not run userland trace code again. */
		return 0;
	}
	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);
	if( pGetTrace == 0 ){
		return 0;
	}
	PH7_MemObjInit(pVm,&sTrace);
	/* getTraceAsString() is userland code from the builtin prelude; if it (or
	 * anything it calls) throws, the throw would be reported by this very
	 * renderer. Fence the window so that report falls back to the synthesized
	 * trace rather than re-entering here forever. */
	bSaved = pVm->bRenderingUncaught;
	pVm->bRenderingUncaught = 1;
	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){
		zTmp = ph7_value_to_string(&sTrace,&nTmp);
		if( zTmp && nTmp > 0 ){
			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);
			bDone = 1;
		}
	}
	PH7_MemObjRelease(&sTrace);
	pVm->bRenderingUncaught = bSaved;
	return bDone;
}
/*
 * Render one exception entry of an uncaught-exception report into pOut.
 *
 * The output is the single-exception PHP format, factored so a `$previous`
 * chain can emit several entries into one blob (see VmReportUncaughtChain):
 *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a
 *             chained entry is prefixed "\n\nNext " (blank-line separated).
 *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"
 *             trailer.
 * A single entry (bFirst && bLast) is byte-identical to the historical output.
 * The caller owns the blob lifecycle (init/release) and the output consumer
 * call; this routine only appends.
 */
static void VmRenderUncaughtEntry(
	ph7_vm *pVm,SyBlob *pOut,
	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */
	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,
	const char *zFuncName,int nFuncLen,int bFirst,int bLast,
	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */
	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */
{
	SyString *pFile;
	if( nThrowLine == 0 ){
		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;
	}
	if( nCallLine == 0 ){
		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;
		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;
	}
	if( zClass == 0 || nClass == 0 ){
		zClass = "Exception";
		nClass = (sxu32)sizeof("Exception") - 1;
	}
	if( zFuncName == 0 || nFuncLen <= 0 ){
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
	}
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( bFirst ){
		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);
	}else{
		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);
	}
	SyBlobAppend(pOut,zClass,nClass);
	if( zMsg && nMsg > 0 ){
		SyBlobAppend(pOut,": ",sizeof(": ")-1);
		SyBlobAppend(pOut,zMsg,nMsg);
	}
	if( pFile ){
		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);
	}
	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);
	/* Prefer the exception's OWN trace: it walks the full frame chain (shared
	 * with debug_backtrace) and getTraceAsString() already renders php's exact
	 * "#N file(line): func()" body plus the "#N {main}" terminator. The
	 * synthesized fallback below can only ever describe ONE frame, so it
	 * silently dropped every intermediate frame of a nested call chain and, at
	 * file scope, invented a "#0 file(line): {main}" entry that php does not
	 * print ({main} is the bottom marker, not a called frame). */
	if( pVm->bRenderingUncaught || !VmAppendExceptionTrace(pVm,pExc,pOut) ){
		int bFrame = 0;
		if( zFuncName && nFuncLen > 0 ){
			if( pFile ){
				/* php reports a trace frame at its CALL SITE, not at the line
				 * running inside it. */
				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",
					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);
			}else{
				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);
			}
			bFrame = 1;
		}
		/* {main} closes the trace, numbered after whatever frames precede it. */
		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",
			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);
	}
	if( bLast && pFile ){
		SyBlobAppend(pOut,"\n",sizeof("\n")-1);
		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);
	}
}
/*
 * Emit a PHP-compatible uncaught exception message and stack trace for a
 * single exception (no `$previous` chain). Used by the callers that have no
 * exception instance to walk (internal Error reports, type errors, ...).
 */
PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)
{
	SyBlob sOut;
	/* An uncaught exception is a fatal: php exits 255 whether or not the
	 * report is displayed. Set the status before the bErrReport gate. */
	pVm->iExitStatus = 255;
	if( !pVm->bErrReport ){
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);
	VmCallErrorHandler(pVm,&sOut);
	SyBlobRelease(&sOut);
	return PH7_ABORT;
}
/*
 * Return the `$previous` exception linked on pThis (the Throwable data model:
 * a protected $previous property, inherited by every user subclass), or NULL
 * if there is none / it isn't an object. Reads the per-instance attribute
 * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.
 */
static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };
static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)
{
	ph7_value *pValue;
	ph7_class_instance *pPrev;
	ph7_class *pThrowable;
	if( pThis == 0 ){
		return 0;
	}
	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);
	if( pValue == 0 || (pValue->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pPrev = (ph7_class_instance *)pValue->x.pOther;
	/* PHP's $previous is always a Throwable; a PHL subclass could assign a
	 * non-Throwable to the (untyped, protected) slot — ignore it so the report
	 * never renders a stray object as an exception entry. */
	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);
	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){
		return 0;
	}
	return pPrev;
}
/*
 * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous
 * yet (PHP keeps an explicitly-constructed previous and never overrides it).
 * pThis takes a ref on pPrev so it outlives the superseded exception; the
 * slot is released by the normal instance teardown. No-op on any miss.
 */
static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)
{
	ph7_value *pValue;
	if( pThis == 0 || pPrev == 0 || pThis == pPrev ){
		return;
	}
	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);
	if( pValue == 0 || (pValue->iFlags & MEMOBJ_OBJ) != 0 ){
		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */
	}
	pPrev->iRef++;
	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a
	 * non-Throwable); release frees any such buffer before the overwrite. */
	PH7_MemObjRelease(pValue);
	pValue->x.pOther = pPrev;
	MemObjSetType(pValue,MEMOBJ_OBJ);
}
/*
 * Append the message of the exception instance pThis to pOut by invoking its
 * getMessage() (so a user override is honored). A no-op if the method is
 * absent or yields an empty string.
 */
/*
 * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).
 * 0 when the class exposes no getLine().
 */
static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_class_method *pGetLine;
	ph7_value sLine;
	sxu32 nLine = 0;
	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);
	if( pGetLine == 0 ){
		return 0;
	}
	PH7_MemObjInit(pVm,&sLine);
	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){
		sxi64 n = ph7_value_to_int64(&sLine);
		if( n > 0 ){
			nLine = (sxu32)n;
		}
	}
	PH7_MemObjRelease(&sLine);
	return nLine;
}
static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)
{
	ph7_class_method *pGetMessage;
	ph7_value sMsg;
	const char *zTmp;
	int nTmp;
	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);
	if( pGetMessage == 0 ){
		return;
	}
	PH7_MemObjInit(pVm,&sMsg);
	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){
		zTmp = ph7_value_to_string(&sMsg,&nTmp);
		if( zTmp && nTmp > 0 ){
			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);
		}
	}
	PH7_MemObjRelease(&sMsg);
}
/*
 * Emit a PHP-compatible uncaught-exception report for pThis, walking its
 * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each
 * outer one as "Next ...", with a single "thrown in ..." trailer after the
 * outermost (the actually-uncaught) exception.
 *
 * The walk starts at pThis (outermost) and follows $previous inward; entries
 * are rendered in reverse (deepest first). A cyclic $previous (e.g.
 * $a->previous = $a, or A<->B) is broken on the first already-seen instance so
 * it is not duplicated, and the depth is hard-capped as a final backstop.
 */
#define VM_EXCEPTION_CHAIN_MAX 64
static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)
{
	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];
	int nChain = 0;
	int i;
	SyBlob sOut;
	/* Same rule as VmReportUncaughtException: an uncaught exception is a
	 * fatal — php exits 255 whether or not the report is displayed. One rule
	 * per report entry point, so a future direct caller can't miss it. */
	pVm->iExitStatus = 255;
	if( !pVm->bErrReport ){
		return PH7_OK;
	}
	/* Collect outermost -> deepest, stopping on a cycle (an instance already
	 * collected) or the hard cap. */
	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){
		for( i = 0 ; i < nChain ; ++i ){
			if( apChain[i] == pThis ){
				pThis = 0; /* cycle: stop the walk */
				break;
			}
		}
		if( pThis == 0 ){
			break;
		}
		apChain[nChain++] = pThis;
		pThis = VmExceptionGetPrevious(pThis);
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),
	 * index 0 is the outermost (gets the "thrown in" trailer). */
	for( i = nChain - 1 ; i >= 0 ; --i ){
		ph7_class_instance *pEnt = apChain[i];
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		VmExtractExceptionMessage(pVm,pEnt,&sMsg);
		VmRenderUncaughtEntry(pVm,&sOut,pEnt,
			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,
			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),
			zFuncName,nFuncLen,
			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */
			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */
			VmExtractExceptionLine(pVm,pEnt),0);
		SyBlobRelease(&sMsg);
	}
	VmCallErrorHandler(pVm,&sOut);
	SyBlobRelease(&sOut);
	return PH7_ABORT;
}
/*
 * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.
 *
 * Arming holds a ref on the cached ArrayAccess instance so it survives the
 * intervening RHS evaluation until NULLC_STORE consumes it. Anything that
 * abandons that store path before NULLC_STORE runs — an exception thrown
 * while evaluating the RHS, a re-arm for a different target — must disarm
 * here, both to release the leaked instance ref/key and to stop a later
 * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.
 */
PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)
{
	if( pVm->bCoalesceArmed ){
		if( pVm->pCoalesceObj ){
			PH7_ClassInstanceUnref(pVm->pCoalesceObj);
		}
		PH7_MemObjRelease(&pVm->sCoalesceKey);
		pVm->pCoalesceObj = 0;
		pVm->bCoalesceArmed = 0;
	}
}
/*
 * Throw a PHP-compatible exception of the named class from inside the VM
 * bytecode dispatch loop (where no ph7_context is available). The message
 * is a literal, non-formatted string; callers that need formatting should
 * build the SyBlob themselves and pass its data + length.
 *
 * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on
 * successful throw (the caller should typically `goto Abort` afterwards if
 * the surrounding opcode cannot continue). Mirrors the inline pattern in
 * PH7_OP_THROW (see "case PH7_OP_THROW").
 */
PH7_PRIVATE sxi32 VmThrowFromVm(
	ph7_vm *pVm,
	const char *zClass,
	const char *zMsg,
	sxu32 nMsg
){
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	VmFrame *pFrame;
	sxi32 rc;
	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);
	if( pClass == 0 ){
		return SXERR_ABORT;
	}
	pThis = PH7_NewClassInstance(pVm,pClass);
	if( pThis == 0 ){
		return SXERR_ABORT;
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons && VmExcCtorEnter(pVm) ){
		ph7_value sArg;
		ph7_value *apArg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);
		PH7_MemObjInit(pVm,&sArg);
		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
		pVm->nExcCtorDepth--;
	}
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pVm,pThis);
	PH7_ClassInstanceUnref(pThis);
	return rc;
}
/*
 * php's arithmetic operand contract, which PH7 never enforced — every case below was a
 * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1`
 * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.
 *
 *   int/float/bool/null      arithmetic proceeds
 *   fully numeric string     proceeds ("1e3", " 5 ")
 *   LEADING-numeric string   proceeds on the numeric prefix, with a warning
 *                            ("5abc" + 1 == 6, "A non-numeric value encountered")
 *   non-numeric string       TypeError: Unsupported operand types: int + string
 *   array                    TypeError, EXCEPT array + array, which is php's union
 *   object/resource          TypeError, naming the object's CLASS
 *
 * Returns SXRET_OK to proceed, or the status of the thrown TypeError.
 */
/*
 * Build php's backtrace (innermost active call first) into pList: one map per
 * ACTIVE call frame, describing the callee (function/class) and the CALL SITE
 * position -- so a frame can see who called it. Shared by debug_backtrace() and
 * the Throwable trace stamp so BOTH walk the full frame chain identically (the
 * stamp used to emit only the innermost frame, truncating every exception trace
 * to depth 1).
 *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as
 *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).
 * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default
 * frame shape is file/line/function[/class/type], matching the default
 * zend.exception_ignore_args=On.
 */
PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)
{
	SyString *pFile;
	VmFrame *pFrame;
	ph7_value *pValue;
	pValue = ph7_new_scalar(&(*pVm));
	if( pValue == 0 ){
		return;
	}
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;
	while( pFrame ){
		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;
		ph7_value *pEntry;
		if( pFrame->pParent == 0 || pFunc == 0 ){
			/* The global frame is not a call: php stops before it (no "{main}"). */
			break;
		}
		pEntry = ph7_new_array(&(*pVm));
		if( pEntry == 0 ){
			break;
		}
		/* php's key order: file, line, function[, class, type][, object][, args].
		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which
		 * lives in the CALLER function's defining file. Use that (not the include-
		 * stack top, which is wrong once a call chain spans files); fall back to the
		 * include-stack top for a call made at global scope. */
		{
			SyString *pFrameFile = pFile;
			if( pFrame->pParent->pUserData ){
				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;
				if( pCaller->sFile.nByte > 0 ){
					pFrameFile = &pCaller->sFile;
				}
			}
			if( pFrameFile ){
				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);
				ph7_array_add_strkey_elem(pEntry,"file",pValue);
				ph7_value_reset_string_cursor(pValue);
			}
		}
		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));
		ph7_array_add_strkey_elem(pEntry,"line",pValue);
		{
			const char *zDisp = 0;
			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);
			ph7_value_string(pValue,zDisp,nDisp);
		}
		ph7_array_add_strkey_elem(pEntry,"function",pValue);
		ph7_value_reset_string_cursor(pValue);
		{
			/* php's 'class' is the DECLARING class of the executing method (where it
			 * is defined), NOT the runtime $this class — an inherited method called
			 * on a subclass reports the base. pFunc->pUserData is that declaring
			 * class (oo.c installs methods with it). 'type' is '->' for an instance
			 * call and '::' for a static one (so static frames get class/type too,
			 * which the old $this-only path dropped). Fall back to the $this class
			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */
			SyString *pClsName = 0;
			const char *zType = "->";
			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
				pClsName = &((ph7_class *)pFunc->pUserData)->sName;
				zType = pFrame->pThis ? "->" : "::";
			}else if( pFrame->pThis && pFrame->pThis->pClass ){
				pClsName = &pFrame->pThis->pClass->sName;
			}
			if( pClsName ){
				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);
				ph7_array_add_strkey_elem(pEntry,"class",pValue);
				ph7_value_reset_string_cursor(pValue);
				ph7_value_string(pValue,zType,(int)SyStrlen(zType));
				ph7_array_add_strkey_elem(pEntry,"type",pValue);
				ph7_value_reset_string_cursor(pValue);
				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){
					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));
					if( pObjVal ){
						pFrame->pThis->iRef++;
						pObjVal->x.pOther = pFrame->pThis;
						MemObjSetType(pObjVal,MEMOBJ_OBJ);
						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);
						ph7_release_value(&(*pVm),pObjVal);
					}
				}
			}
		}
		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){
			ph7_value *pArg = ph7_new_array(&(*pVm));
			if( pArg ){
				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);
				sxu32 n;
				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){
					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);
					if( pObj ){
						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);
					}
				}
				ph7_array_add_strkey_elem(pEntry,"args",pArg);
				ph7_release_value(&(*pVm),pArg);
			}
		}
		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);
		ph7_release_value(&(*pVm),pEntry);
		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;
	}
	ph7_release_value(&(*pVm),pValue);
}
/*
 * Stamp a freshly created Throwable with the site it was created at.
 *
 * php records `file`/`line` on the OBJECT at creation time -- not inside
 * Exception::__construct -- so a subclass that overrides the constructor and never
 * calls parent::__construct still reports the right position. The embedded
 * Exception/Error constructors used to assign `$this->line = __LINE__`, which
 * resolved against the EMBEDDED chunk (always line 1); they no longer touch either
 * field, and this runs for every instantiation path (OP_NEW and the engine's own
 * VmThrowBuiltinError / VmThrowFixedError).
 */
PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)
{
	static const char *azField[] = { "file", "line", "trace" };
	ph7_class *pThrowable;
	SyString *pFile;
	SyString *pSiteFile;
	sxu32 n;
	if( pThis == 0 || pThis->pClass == 0 ){
		return;
	}
	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);
	if( pThrowable == 0 || !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){
		return;
	}
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	pSiteFile = pFile;
	/* getFile() is the file where `new` executed = the DEFINING file of the
	 * innermost active function (aFiles tracks include nesting, not the running
	 * function's source, so it is wrong once a call chain spans files). Fall back
	 * to the include-stack top at global scope / for engine-created throwables. */
	{
		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;
		if( pInner && pInner->pUserData ){
			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;
			if( pInnerFunc->sFile.nByte > 0 ){
				pSiteFile = &pInnerFunc->sFile;
			}
		}
	}
	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){
		SyHashEntry *pEntry;
		VmClassAttr *pVmAttr;
		ph7_value *pAttrValue;
		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));
		if( pEntry == 0 ){
			continue;
		}
		pVmAttr = (VmClassAttr *)pEntry->pUserData;
		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
		if( pAttrValue == 0 ){
			continue;
		}
		if( n == 0 ){
			if( pSiteFile ){
				PH7_MemObjRelease(pAttrValue);
				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);
			}
		}else if( n == 1 ){
			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its
			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so
			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the
			 * access's line, not the declaration's. The depth test keeps the window
			 * off everything the initializer calls — an autoloader, a nested
			 * constant's evaluation — which report their own lines in both engines.
			 * (Enum cases never set it: php reports the CASE's own line there, and
			 * PHL already matches.) */
			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)
				? pVm->nLazyInitLine
				: (pVm->nCurLine ? pVm->nCurLine : 1);
			PH7_MemObjRelease(pAttrValue);
			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);
		}else{
			/* trace: php captures the FULL backtrace at the CREATION site (innermost
			 * = the function that ran `new`, reported at its call site). Reuse the
			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default
			 * exception-trace shape (file/line/function[/class/type]). */
			ph7_value *pList = ph7_new_array(&(*pVm));
			if( pList == 0 ){
				continue;
			}
			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);
			/* Building the trace reserves new memobjs, which may realloc
			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,
			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot
			 * AFTER the walk before releasing/storing into it. */
			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
			if( pAttrValue ){
				PH7_MemObjRelease(pAttrValue);
				PH7_MemObjStore(pList,pAttrValue);
			}
			ph7_release_value(&(*pVm),pList);
		}
	}
}
PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)
{
	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( pInst && pInst->pClass ){
			return pInst->pClass->sName.zString;
		}
	}
	return ph7_type_name(pVal);
}
/*
 * php's ++/-- operand contract: an array, object or resource operand is a
 * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment
 * resource"), never the silent no-op PH7 used to perform. Word it once here so
 * the plain opcode path and the hooked-property path cannot drift apart.
 * bIncr selects the verb; the value itself is left untouched by php.
 */
PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)
{
	const char *zVerb = bIncr ? "increment" : "decrement";
	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( pInst && pInst->pClass ){
			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);
			return;
		}
	}
	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));
}
/*
 * php's `&`, `|` and `^` over TWO STRINGS: a per-BYTE operation whose result is
 * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered
 * int(0) for every such pair because it cast both sides to int first. `&` and
 * `^` stop at the SHORTER operand; `|` runs to the LONGER one, the missing bytes
 * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '|'
 * or '^'; the result is appended to *pOut (the caller owns it).
 */
PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)
{
	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);
	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);
	sxu32 nL = SyBlobLength(&pLeft->sBlob);
	sxu32 nR = SyBlobLength(&pRight->sBlob);
	sxu32 nMin = nL < nR ? nL : nR;
	sxu32 i;
	for( i = 0 ; i < nMin ; ++i ){
		unsigned char c;
		if( cOp == '|' ){
			c = (unsigned char)(zL[i] | zR[i]);
		}else if( cOp == '^' ){
			c = (unsigned char)(zL[i] ^ zR[i]);
		}else{
			c = (unsigned char)(zL[i] & zR[i]);
		}
		SyBlobAppend(pOut,(const void *)&c,sizeof(char));
	}
	if( cOp == '|' && nL != nR ){
		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];
		sxu32 nTail = (nL > nR ? nL : nR) - nMin;
		SyBlobAppend(pOut,(const void *)zTail,nTail);
	}
}
/*
 * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic
 * type name: zend spells a BOOL there as the literal `true`/`false` (where
 * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the
 * types `~` refuses reach this — a string is complemented byte-wise and a
 * number is complemented as an integer — and an UNINITIALIZED slot is php's
 * null, which is what an undefined variable answers.
 */
PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)
{
	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( pInst && pInst->pClass ){
			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);
			return;
		}
	}
	if( pVal->iFlags & MEMOBJ_BOOL ){
		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",
			pVal->x.iVal ? "true" : "false");
	}else if( pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_RES|MEMOBJ_OBJ) ){
		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));
	}else{
		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",
			sizeof("Cannot perform bitwise not on null")-1);
	}
}
/*
 * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both
 * inherit the MULTIPLICATION operand contract -- message included: `-$o` is
 * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is
 * "string * int", while a leading-numeric string ("-\"12abc\"") warns and
 * computes with the prefix. Classify pVal against that contract.
 */
PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)
{
	ph7_value sInt;
	sxi32 rc;
	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);
	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);
	PH7_MemObjRelease(&sInt);
	return rc;
}
PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)
{
	int bBadL = 0, bBadR = 0;
	int i;
	ph7_value *apOperand[2];
	apOperand[0] = pLeft;
	apOperand[1] = pRight;
	/* array + array is php's union operator, not arithmetic */
	if( zOp[0] == '+' && zOp[1] == '\0'
	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){
		return SXRET_OK;
	}
	for( i = 0 ; i < 2 ; ++i ){
		ph7_value *pVal = apOperand[i];
		int bBad = 0;
		if( pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
			bBad = 1;
		}else if( pVal->iFlags & MEMOBJ_STRING ){
			const char *zTail = 0;
			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);
			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){
				/* Nothing numeric at all ("abc", "") -> TypeError. */
				bBad = 1;
			}else{
				/* Starts with a number. php only calls it numeric when the WHOLE string
				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a
				 * leading-numeric string -- php warns and computes with the prefix. */
				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){
					zTail++;
				}
				if( zTail < zEnd ){
					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");
				}
			}
		}
		if( bBad ){
			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }
		}
	}
	if( bBadL || bBadR ){
		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,
		 * or the catch runs with the abandoned operands still on it and execution resumes
		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */
		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",
			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));
		return SXERR_INVALID;
	}
	return SXRET_OK;
}
/*
 * Throw an internal exception instance that can be intercepted by try/catch.
 * iCode becomes the exception's $code (php's second constructor argument);
 * pass 0 for the engine errors that leave it at its default.
 */
static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)
{
	ph7_vm *pVm;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg,sCode;
	ph7_value *apArg[2];
	SyBlob sMsg;
	SyString sMsgStr;
	VmFrame *pFrame;
	sxi32 rc;

	if( pCtx == 0 || pCtx->pVm == 0 ){
		return PH7_ABORT;
	}
	pVm = pCtx->pVm;
	if( zClass == 0 || zClass[0] == 0 ){
		zClass = "Error";
	}
	/* The two degraded paths below cannot build the exception object, so they report an
	 * uncaught fatal with a trace instead. They record whatever status that reporting
	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds
	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a
	 * reported error (VmHostFuncThrowRc). */
	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);
	if( pClass == 0 ){
		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,
			"Cannot throw internal exception, class '%s' is not available",
			zClass
			);
		return pCtx->nThrowRc;
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,
			"Cannot throw internal exception, PH7 is running out of memory"
			);
		return pCtx->nThrowRc;
	}

	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormatAp(&sMsg,zFormat,ap);

	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		int nArg = 1;
		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		if( iCode != 0 ){
			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);
			apArg[1] = &sCode;
			nArg = 2;
		}
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);
		if( iCode != 0 ){
			PH7_MemObjRelease(&sCode);
		}
		PH7_MemObjRelease(&sArg);
	}
	SyBlobRelease(&sMsg);

	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		pCtx->nThrowRc = PH7_ABORT;
		return PH7_ABORT;
	}
	/* Record the status on the CALL CONTEXT as well. A host function that raises
	 * here and then returns PH7_OK anyway — because the throw sits in a shared
	 * argument-validation helper whose callers have no status channel — would
	 * otherwise let OP_CALL treat the call as a normal return: the catch has
	 * already run in place, so execution would carry on INSIDE the try the throw
	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()
	 * re-reads this at the boundary; a caller that DOES propagate its rc is
	 * unaffected (the escalation only fires on a non-throwing status). Same
	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */
	pCtx->nThrowRc = PH7_EXCEPTION;
	return PH7_EXCEPTION;
}
PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
	va_start(ap,zFormat);
	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);
	va_end(ap);
	return rc;
}
/* Same, carrying php's exception $code (JsonException gets json_last_error()). */
PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
	va_start(ap,zFormat);
	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * The status a host function's own throw should have returned. Consulted at the
 * single OP_CALL host boundary right after the C routine returns: it upgrades a
 * normal-looking status to the one PH7_VmThrowException recorded on the context,
 * and is the identity when the routine never threw or already reported it.
 *
 * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw
 * already reported" status: it is a control transfer the fiber machinery must
 * honour (the CALL's state is saved and the operand stack is left mid-flight), and
 * no path produces both — every Fiber throw returns PH7_EXCEPTION.
 */
PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)
{
	if( pCtx->nThrowRc == 0
	 || rc == PH7_ABORT || rc == PH7_EXCEPTION || rc == PH7_SUSPEND ){
		return rc;
	}
	return pCtx->nThrowRc;
}
/*
 * Throw an internal error as a PHP-like uncaught exception message with stack trace.
 * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.
 */
PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)
{
	ph7_vm *pVm;
	SyBlob sMsg;
	const char *zFuncName = 0;
	int nFuncLen = 0;
	va_list ap;
	sxi32 rc;

	if( pCtx == 0 || pCtx->pVm == 0 ){
		return PH7_OK;
	}
	pVm = pCtx->pVm;
	if( zClass == 0 || zClass[0] == 0 ){
		zClass = "Error";
	}

	SyBlobInit(&sMsg,&pVm->sAllocator);

	va_start(ap,zFormat);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	va_end(ap);

	if( pCtx->pFunc ){
		zFuncName = pCtx->pFunc->sName.zString;
		nFuncLen = (int)pCtx->pFunc->sName.nByte;
	}
	if( zFuncName == 0 || nFuncLen <= 0 ){
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
	}
	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),
		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * The following routine is invoked by the engine when an uncaught
 * exception is triggered.
 */
PH7_PRIVATE sxi32 VmUncaughtException(
	ph7_vm *pVm, /* Target VM */
	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */
	)
{
	ph7_value *apArg[2],sArg;
	int nArg = 1;
	sxi32 rc;
	if( pVm->nMuteThrow > 0 ){
		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's
		 * default at mount) — php has not reached this code, so nothing may be
		 * observable: no exception handler runs, no report is printed and the
		 * exit status stays put. Unwind the mini-program at once; the mount path
		 * rolls the attempt back and re-runs the initializer at first access. */
		return SXERR_ABORT;
	}
	if( pVm->nExceptDepth > 15 ){
		/* Nesting limit reached */
		return SXRET_OK;
	}
	/* Call any exception handler if available */
	PH7_MemObjInit(pVm,&sArg);
	if( pThis ){
		/* Load the exception instance */
		sArg.x.pOther = pThis;
		pThis->iRef++;
		MemObjSetType(&sArg,MEMOBJ_OBJ);
	}else{
		nArg = 0;
	}
	apArg[0] = &sArg;
	/* Call the exception handler if available */
	pVm->nExceptDepth++;
	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);
	pVm->nExceptDepth--;
	if( rc != SXRET_OK ){
		const char *zFuncName;
		int nFuncLen;
		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);
		/* Both report entry points below stamp iExitStatus = 255 themselves. */
		if( pThis ){
			/* Walk the $previous chain: deepest is "Uncaught", each outer one
			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and
			 * renders byte-identically to the historical single-entry report. */
			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);
		}else{
			/* No instance (internal report path) — default-class single entry. */
			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);
		}
		/* Tell the upper layer to stop VM execution immediately  */
		rc = SXERR_ABORT;
	}
	PH7_MemObjRelease(&sArg);
	return rc;
}
/*
 * Throw a user exception.
 *
 * Exception dispatch follows this sequence:
 *
 * 1. Walk the exception stack (pVm->aException) from top to find a
 *    try/catch whose catch block matches the exception class.
 *
 * 2. If NO catch matches:
 *    a. Run finally (if present) for the current try block.
 *    b. If outer handlers exist on the stack, re-throw recursively.
 *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)
 *       whose outer handlers were temporarily hidden, DEFER the
 *       exception in pVm->pPendingException instead of reporting it
 *       uncaught. It will be re-thrown after finally runs (step 3d).
 *    d. Otherwise, report as truly uncaught.
 *
 * 3. If a catch DOES match:
 *    a. Temporarily HIDE all outer exception handlers by saving the
 *       aException stack and resetting it. This prevents a re-throw
 *       inside the catch body from immediately propagating past our
 *       finally block.
 *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH
 *       frame. If the catch body throws, dispatch recurses but finds
 *       no handlers (they're hidden), so the exception is deferred
 *       in pPendingException (step 2c).
 *    c. Restore outer handlers from the saved copy.
 *    d. Run finally (if present).
 *    e. If pPendingException is set (catch re-threw), re-throw it now
 *       that handlers are restored and finally has run.
 */
/*
 * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.
 * Pops each enclosing try's handler (this function's inline trys, innermost first) off
 * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the
 * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the
 * finally runs). A try without a finally is torn down (handler + transparent frame) and
 * the walk continues. Returns 0 when no more of this function's inline trys remain (the
 * action is terminal: materialize the return / take the break jump). A try WITH a finally
 * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.
 * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).
 */
PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)
{
	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){
		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);
		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];
		if( !pT->iInlined || pT->pOwnerInstr != (void *)aInstr ){
			break; /* reached an outer exec's / legacy handler */
		}
		(void)SySetPop(&pVm->aException);
		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */
		if( pT->iHasFinally ){
			*pPc = pT->iFinallyPc;
			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */
			return 1;
		}
		/* No finally: tear the try's transparent frame down now. */
		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
			VmLeaveFrame(&(*pVm));
		}
		VmExcRelease(&(*pVm),pT);
	}
	return 0;
}
/*
 * ROOT C: classify a throw against an INLINE try (generator body) and set up a
 * pc-redirect for the throw site — never runs bytecode itself. pException has been
 * popped off aException by the caller; pCatch is the matching catch block or 0.
 *
 *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch
 *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,
 *    and redirect to the catch body (iHandlerPc).
 *  - no catch but finally: queue a RETHROW action and redirect to the finally
 *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.
 *  - no catch, no finally: leave this try's transparent frame and propagate to the
 *    next handler (inline or legacy) by re-entering VmThrowException.
 * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).
 */
/*
 * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope
 * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented
 * onto pOwner, exactly the catch body's mechanism (see the re-parent note in
 * VmThrowException's catch path). Before this, a finally reached by a throw
 * from a NESTED call ran against the throw-site frame: it read the wrong
 * function's variables and a `return` inside it parked on the wrong body
 * (the finally_return_cross_frame twin). Same-frame execution (the common
 * case) is unchanged: no wrapper.
 */
static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)
{
	VmFrame *pWrap = 0;
	VmFrame *pThrowSite;
	sxi32 rc;
	if( pOwner == 0 || pOwner == VmSkipExceptionFrames(pVm->pFrame) ){
		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	}
	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){
		/* OOM: degrade to in-place execution rather than losing the finally. */
		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	}
	pThrowSite = pWrap->pParent;
	pWrap->pParent = pOwner;
	pWrap->iFlags |= VM_FRAME_EXCEPTION;
	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);
	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then
	 * restore the real throw site so the unwind continues normally. Guarded:
	 * a finally that suspends/aborts mid-mini-program can leave the frame
	 * chain unbalanced — never pop somebody else's frame. */
	if( pVm->pFrame == pWrap ){
		VmLeaveFrame(&(*pVm));
	}
	pVm->pFrame = pThrowSite;
	return rc;
}
/*
 * VmThrowInline -> VmThrowException private protocol: "no catch/finally in
 * this inline try — keep unwinding outward" (the caller loops back to its
 * Rethrow label). Aliased so the generic retry code's other contract (the
 * sxmem xMemError release-and-retry callback) is not confused with this one.
 */
#define VM_THROW_KEEP_UNWINDING SXERR_RETRY
static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,
	ph7_exception *pException, ph7_exception_block *pCatch)
{
	if( pCatch ){
		pException->iInCatch = 1;
		SySetPut(&pVm->aException,(const void *)&pException);
		if( pThis ){ pThis->iRef++; }
		pException->pInflight = pThis;
		pVm->pInlineInstr = pException->pOwnerInstr;
		pVm->iInlinePc = pCatch->iHandlerPc;
		pVm->iInlineDrain = pException->iStackDepth;
		return SXRET_OK;
	}
	if( pException->iHasFinally ){
		VmFinallyAction sAct;
		SyZero(&sAct,sizeof(sAct));
		sAct.eKind = PH7_FA_RETHROW;
		if( pThis ){ pThis->iRef++; }
		sAct.pExc = pThis;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pVm->pInlineInstr = pException->pOwnerInstr;
		pVm->iInlinePc = pException->iFinallyPc;
		pVm->iInlineDrain = pException->iStackDepth;
		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */
		return SXRET_OK;
	}
	/* No catch, no finally: drop this try's frame and continue unwinding —
	 * flat native stack instead of mutual recursion. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */
	return VM_THROW_KEEP_UNWINDING;
}
PH7_PRIVATE sxi32 VmThrowException(
	ph7_vm *pVm,              /* Target VM */
	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */
	)
{
	ph7_exception_block *pCatch; /* Catch block to execute */
	ph7_exception **apException;
	ph7_exception *pException;
Rethrow:
	/* Unwinding to the next outer handler loops back here instead of the old
	 * self tail-call: a throw from N frames deep runs N finallys at constant
	 * native depth (the ASan deep-tier stress overflowed the C stack on the
	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,
	 * so the throw path must be too). */
	/* An in-flight throw abandons any pending null-coalesce-assign store:
	 * disarm so the RHS-evaluation throw can't leave the slot live for a
	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */
	VmCoalesceDisarm(pVm);
	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight
	 * exception (pInflightException) and a NEW exception thrown by that finally
	 * is leaving it — i.e. the exception stack has unwound to/below the depth it
	 * had when the finally started (nInflightExcBase), so no finally-local catch
	 * will handle it — link the in-flight one as its $previous. A finally
	 * exception caught locally (a try/catch inside the finally) sits ABOVE the
	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op
	 * if pThis already carries a previous, so re-entry across propagation is safe. */
	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException
	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){
		VmExceptionLinkPrevious(pThis,pVm->pInflightException);
	}
	/* A throw supersedes a pending catch/finally `return` ONLY when it actually
	 * unwinds past that return's body frame. We do NOT clear anything here: each
	 * body's pending return lives on its own frame and is discarded at that body's
	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught
	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body
	 * that owns the pending return, so it must leave that return intact. */
	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):
	 * the previous catch's landing is no longer where control should resume. Cleared
	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish
	 * records last (on its SXRET_OK return below) and therefore owns the resume. */
	pVm->pResumeFrame = 0;
	/* Point to the stack of loaded exceptions */
	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);
	pException = 0;
	pCatch = 0;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception_block *aCatch;
		ph7_class *pClass;
		SyString *aNames;
		sxu32 nNames;
		int matched;
		sxu32 j,k;
		/* Locate the appropriate block to execute */
		pException = apException[SySetUsed(&pVm->aException) - 1];
		(void)SySetPop(&pVm->aException);
		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);
		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does
		 * NOT re-match its own catches — a throw inside the catch runs the finally then
		 * propagates. Skip the class scan so pCatch stays 0. */
		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){
			/* Iterate over all class names in this catch block (multi-catch support) */
			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);
			nNames = SySetUsed(&aCatch[j].aClasses);
			matched = 0;
			for( k = 0 ; k < nNames ; ++k ){
				/* Extract the target class or interface (iLoadable=FALSE so
				 * interfaces like Throwable are resolvable as catch targets).
				 * Traits are never instance-compatible, so skip them explicitly. */
				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);
				if( pClass == 0 || (pClass->iFlags & PH7_CLASS_TRAIT) ){
					/* No such class, or trait — cannot match */
					continue;
				}
				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){
					matched = 1;
					break;
				}
			}
			if( matched ){
				/* Catch block found,break immediately */
				pCatch = &aCatch[j];
				break;
			}
		}
	}
	/* Restore the '@' error-control depth recorded when this try was entered. The throw
	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,
	 * so without this the suppression leaks and silences every later diagnostic. Done
	 * once here, where the catching try is known, because the handler is entered by two
	 * different routes below (the inline/generator pc-redirect and the legacy path) —
	 * and on a Rethrow the outermost try that actually catches gets the last word. A
	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */
	if( pException ){
		pVm->nErrSuppress = pException->iErrSuppress;
	}
	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException
	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the
	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */
	if( pException && pException->iInlined ){
		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);
		if( rcInline == VM_THROW_KEEP_UNWINDING ){
			/* No catch/finally in that inline try: keep unwinding outward. */
			goto Rethrow;
		}
		return rcInline;
	}
	/* Execute the cached block if available */
	if( pCatch == 0 ){
		sxi32 rc;
		/* No catch matched. Execute finally, then propagate to outer try/catch. */
		if( pException && pException->iHasFinally ){
			sxu32 nExcBefore = SySetUsed(&pVm->aException);
			ph7_class_instance *pSaveInflight = pVm->pInflightException;
			sxu32 nSaveBase = pVm->nInflightExcBase;
			pException->iFinallyDone = 1;
			/* Mark pThis in-flight (base = current exception-stack depth) so a throw
			 * from the finally that leaves it chains pThis as $previous; restore after. */
			pVm->pInflightException = pThis;
			pVm->nInflightExcBase = nExcBefore;
			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own
			 * variables; a `return` parks on the owning body), like the catch. */
			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);
			pVm->pInflightException = pSaveInflight;
			pVm->nInflightExcBase = nSaveBase;
			if( rc == SXERR_ABORT ){
				VmExcRelease(&(*pVm),pException);
				return SXERR_ABORT;
			}
			/* A `return` inside the finally swallows the in-flight exception (PHP
			 * semantics). The finally stored it on the body frame it returns from
			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the
			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:
			 * clear VM_FRAME_THROW (this body now returns normally, so its caller
			 * takes the value instead of unwinding) and resume in place.
			 * Cross-frame: record the OWNER's landing pad as the resume target —
			 * the same transport an in-place catch uses — and unwind as an
			 * exception; the owner's activation consumes the resume
			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and
			 * its bHasRet tail materializes the return. */
			{
				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;
				if( pOwnerFrame->bHasRet ){
					if( pOwnerFrame == pThrowFrame ){
						pThrowFrame->iFlags &= ~VM_FRAME_THROW;
						/* Record the landing pad like the cross-frame case below.
						 * OP_THROW lands on its compiler-given nJump anyway, but a
						 * VM-RAISED throw site (undefined function, non-callable,
						 * arith TypeError…) has no nJump: without a recorded target
						 * its router unwound as an exception and the Unwind discard
						 * dropped the parked return — `function f(){ try {
						 * nosuchfn(); } finally { return 5; } }` returned null
						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the
						 * try frame down and its bHasRet tail materializes the
						 * return, same as the in-place-catch landing. */
						pVm->pResumeFrame = pOwnerFrame;
						pVm->iResumePc = pException->iLandingPc;
						pVm->pResumeInstr = pException->pOwnerInstr;
						pVm->iResumeStackDepth = pException->iStackDepth;
						VmExcRelease(&(*pVm),pException);
						return SXRET_OK;
					}
					pVm->pResumeFrame = pOwnerFrame;
					pVm->iResumePc = pException->iLandingPc;
					pVm->pResumeInstr = pException->pOwnerInstr;
					pVm->iResumeStackDepth = pException->iStackDepth;
					VmExcRelease(&(*pVm),pException);
					return PH7_EXCEPTION;
				}
			}
			/* The finally threw an exception that superseded pThis — it either
			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler
			 * (which consumed an entry from the exception stack). Either way the
			 * original pThis is discarded; unwind with the finally's exception (the
			 * OP_THROW caller resumes at the catching frame or propagates). */
			if( rc == PH7_EXCEPTION || SySetUsed(&pVm->aException) < nExcBefore ){
				VmExcRelease(&(*pVm),pException);
				return PH7_EXCEPTION;
			}
		}
		/* Check if there is an outer exception handler on the stack */
		if( SySetUsed(&pVm->aException) > 0 ){
			/* Re-throw to the outer handler — flat loop (see Rethrow), one
			 * iteration per unwound level instead of one native frame. */
			VmExcRelease(&(*pVm),pException);
			goto Rethrow;
		}
		if( pVm->nMuteThrow > 0 ){
			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's
			 * default at class mount, which php would not have evaluated yet).
			 * Nothing outside the initializer may observe this throw: no
			 * deferral into pPendingException for an enclosing catch to pick up,
			 * no handler, no report. The tries pushed INSIDE the eval had their
			 * chance above; hand the status back so the mini-program unwinds and
			 * the mount path rolls the whole attempt back. */
			VmExcRelease(&(*pVm),pException);
			return SXERR_ABORT;
		}
		/* No outer handler. If the handlers were temporarily hidden
		 * (catch body re-throw with finally pending), defer the
		 * exception instead of reporting it uncaught.
		 */
		if( pVm->pPendingException == 0 && pThis ){
			/* Check if we are inside a catch execution with hidden handlers
			 * by looking for a catch frame on the stack.
			 */
			VmFrame *pF = pVm->pFrame;
			int inCatch = 0;
			while( pF ){
				if( pF->iFlags & VM_FRAME_CATCH ){
					inCatch = 1;
					break;
				}
				pF = pF->pParent;
			}
			if( inCatch ){
				/* Defer — will be re-thrown after finally runs */
				pThis->iRef++;
				pVm->pPendingException = pThis;
				VmExcRelease(&(*pVm),pException);
				return SXRET_OK;
			}
		}
		/* Truly uncaught */
		rc = VmUncaughtException(&(*pVm),pThis);
		if( rc == SXRET_OK && pException ){
			VmFrame *pFrame = pVm->pFrame;
			pFrame = VmSkipExceptionFrames(pFrame);
			if( pException->pFrame == pFrame ){
				pFrame->iFlags &= ~VM_FRAME_THROW;
			}
		}
		VmExcRelease(&(*pVm),pException);
		return rc;
	}else{
		VmFrame *pFrame = pVm->pFrame;
		ph7_exception **apSaved = 0;
		sxu32 nSavedCount;
		sxi32 rc;
		/* Snapshot the resume target BEFORE running the catch/finally mini-programs
		 * (which may push/pop nested exceptions): the body frame that owns this
		 * matching try, and its post-try landing pad. Recorded onto the VM only on
		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at
		 * THIS catching body rather than the lexically-nearest try (ROOT B). */
		VmFrame *pCatchBody = pException->pFrame;
		sxu32 iCatchPc = pException->iLandingPc;
		void *pCatchInstr = pException->pOwnerInstr;
		pFrame = VmSkipExceptionFrames(pFrame);
		if( pException->pFrame == pFrame ){
			pFrame->iFlags &= ~VM_FRAME_THROW;
		}
		/* Temporarily hide outer exception handlers so that if the catch
		 * body re-throws, the exception does not immediately propagate past
		 * our finally block. We save the stack contents and restore after.
		 */
		nSavedCount = SySetUsed(&pVm->aException);
		if( nSavedCount > 0 ){
			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,
				nSavedCount * sizeof(ph7_exception *));
			if( apSaved ){
				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,
					nSavedCount * sizeof(ph7_exception *));
				SySetReset(&pVm->aException);
			}
		}
		/* Create the catch frame (made transparent below) */
		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);
		if( rc == SXRET_OK ){
			ph7_value *pObj;
			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a
			 * throw raised in a nested call, is the deeper throw-site frame, not the
			 * body that declared the try. Re-parent onto the try-owning body
			 * (pCatchBody = pException->pFrame) and remember the throw site to restore
			 * after. This makes the transparent wrapper resolve the catch's variable
			 * scope AND its `return` target against the body that owns the try, so a
			 * `return` parks on that body — matching the recorded resume target. Before
			 * this, an in-place catch for a deep throw parked its return on the callee's
			 * frame, which the unwind then discarded (ROOT B, face c). */
			VmFrame *pThrowSite = pFrame->pParent;
			/* A NULL pCatchBody (owner invalidated at park — its frame died with
			 * a lossy deep suspend) keeps the natural parent: the catch then runs
			 * against the live current scope rather than a freed frame. */
			if( pCatchBody ){
				pFrame->pParent = pCatchBody;
			}
			/* Transparent wrapper: the catch body shares the enclosing variable
			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames
			 * resolve variables — and bind $e — against the real enclosing frame, so
			 * outer locals, $this and a closure held in a variable are all visible
			 * inside the catch (and $e/any var written there persists afterwards).
			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump
			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)
			 * are unaffected. Must be set BEFORE binding $e below. */
			pFrame->iFlags |= VM_FRAME_CATCH | VM_FRAME_EXCEPTION;
			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */
			pObj = (pCatch->sThis.nByte > 0)
				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;
			if( pObj ){
				/* The catch variable now resolves in the (shared) enclosing frame,
				 * so it may already hold a value from a prior catch or assignment.
				 * Pin the new instance, then release the slot's prior contents
				 * (runs its __destruct / frees the old value) before rebinding —
				 * iRef++ first keeps a re-thrown same exception alive across the
				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */
				pThis->iRef++;
				PH7_MemObjRelease(pObj);
				pObj->x.pOther = pThis;
				MemObjSetType(pObj,MEMOBJ_OBJ);
			}
			/* Execute the catch block */
			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);
			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then
			 * restore the real throw-site frame so the unwind continues normally.
			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body
			 * that suspends/aborts mid-mini-program can leave the frame chain
			 * unbalanced — never pop somebody else's frame. */
			if( pVm->pFrame == pFrame ){
				VmLeaveFrame(&(*pVm));
			}
			pVm->pFrame = pThrowSite;
		}
		/* Restore the outer exception handlers */
		if( apSaved ){
			sxu32 k;
			/* Entries pushed during catch execution (nested try blocks inside
			 * the catch body) are normally already consumed; on an abnormal
			 * mini-program exit (e.g. a suspend escaping the catch) they can
			 * linger — release those activations before discarding the set. */
			VmExcReleaseAll(&(*pVm),&pVm->aException);
			SySetReset(&pVm->aException);
			for(k = 0; k < nSavedCount; k++){
				SySetPut(&pVm->aException,(const void *)&apSaved[k]);
			}
			SyMemBackendFree(&pVm->sAllocator,apSaved);
		}
		/* Execute the finally block after catch */
		if( pException->iHasFinally ){
			sxi32 rcf;
			/* Snapshot, before the finally runs: the body frame this try returns
			 * from, its pending-return write generation (set if the catch above
			 * returned), and the exception-stack depth. After the finally we use
			 * these to decide whether the finally's throw superseded THIS try's
			 * catch-return. */
			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep
			 * throw the current frame is the THROW SITE, and both the catch's
			 * return (parked via the re-parented wrapper) and the finally's
			 * supersede decision belong to the owner, not the thrower. */
			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);
			sxu32 nGenBefore = pBody->nRetGen;
			sxu32 nExcBefore = SySetUsed(&pVm->aException);
			/* The exception in flight while this finally runs is the catch body's
			 * re-throw (deferred in pPendingException), if any — NOT the original
			 * pThis, which the catch already handled. A finally throw chains to that
			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;
			 * a normally-handled catch leaves nothing in flight => B->previous null). */
			ph7_class_instance *pSaveInflight = pVm->pInflightException;
			sxu32 nSaveBase = pVm->nInflightExcBase;
			pException->iFinallyDone = 1;
			pVm->pInflightException = pVm->pPendingException;
			pVm->nInflightExcBase = nExcBefore;
			/* Stage 2b: the finally runs in the owner's scope, like the catch. */
			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);
			pVm->pInflightException = pSaveInflight;
			pVm->nInflightExcBase = nSaveBase;
			if( rcf == SXERR_ABORT ){
				VmExcRelease(&(*pVm),pException);
				return SXERR_ABORT;
			}
			/* Did the finally throw an exception that escaped THIS try? Two shapes:
			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by
			 * a handler that lived BELOW this try (the exception stack shrank). In
			 * either case that exception supersedes this try's catch-return — but
			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch
			 * (same body frame) ran during the finally and OVERWROTE the slot with
			 * its own return, nRetGen advanced and that return must survive. */
			if( rcf == PH7_EXCEPTION || SySetUsed(&pVm->aException) < nExcBefore ){
				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){
					VmClearFramePending(pBody);
				}
				/* Same rule for a `break`/`continue` parked by the catch
				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.
				 * Unconditional — a jump has no nRetGen equivalent, nothing can
				 * legitimately have re-armed it during the finally. */
				pBody->nCatchJmpPc = 0;
			}
			if( rcf == PH7_EXCEPTION ){
				/* The finally's exception propagated past this try; drop any deferred
				 * re-throw and signal the OP_THROW site to unwind THIS function so it
				 * reaches the frame that caught the finally's throw. */
				if( pVm->pPendingException ){
					PH7_ClassInstanceUnref(pVm->pPendingException);
					pVm->pPendingException = 0;
				}
				VmExcRelease(&(*pVm),pException);
				return PH7_EXCEPTION;
			}
		}
		if( rc == SXERR_ABORT ){
			VmExcRelease(&(*pVm),pException);
			return SXERR_ABORT;
		}
		/* If the catch body re-threw, the exception was deferred in
		 * pPendingException (because outer handlers were hidden).
		 * Now that finally has run and handlers are restored, re-throw —
		 * unless the catch/finally issued a `return` (parked on this body frame,
		 * the catch frame having been left above), which swallows the in-flight
		 * exception (PHP semantics).
		 */
		if( pVm->pPendingException ){
			/* Stage 2b: the swallow decision reads the OWNER's parked return. */
			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);
			if( !pOwner->bHasRet ){
				ph7_class_instance *pReThrow = pVm->pPendingException;
				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the
				 * re-throw: control unwinds past the loop, so drop the loop exit rather
				 * than leave it armed for an unrelated later landing. */
				pOwner->nCatchJmpPc = 0;
				pVm->pPendingException = 0;
				VmExcRelease(&(*pVm),pException);
				/* Continue unwinding with the re-thrown exception (flat loop) */
				pThis = pReThrow;
				goto Rethrow;
			}
			/* Swallowed by the catch/finally's return: drop the deferred exception. */
			PH7_ClassInstanceUnref(pVm->pPendingException);
			pVm->pPendingException = 0;
		}
		/* The catch (and finally) ran in place and control did NOT unwind past this
		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the
		 * throwing site — which may be several frames below — resumes at this catching
		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */
		pVm->pResumeFrame = pCatchBody;
		pVm->iResumePc = iCatchPc;
		pVm->pResumeInstr = pCatchInstr;
		pVm->iResumeStackDepth = pException->iStackDepth;
		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry
		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at
		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */
		VmExcRelease(&(*pVm),pException);
	}
	return SXRET_OK;
}
