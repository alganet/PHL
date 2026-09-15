# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1726/1996 lines (86.47%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Error, diagnostics and type-enforcement machinery: PH7_VmThrowError` |
|      - |   10 | ` *    and the error-handler invocation path, enum materialization and` |
|      - |   11 | ` *    on-demand class constants, scalar/union/property/constant/return` |
|      - |   12 | ` *    type enforcement, the TypeError/ArgumentCountError throwers,` |
|      - |   13 | ` *    uncaught-exception rendering, VmBuildBacktrace, and the exception` |
|      - |   14 | ` *    core VmUncaughtException/VmThrowException.` |
|      - |   15 | ` * Status:` |
|      - |   16 | ` *    Stable.` |
|      - |   17 | ` */` |
|      - |   18 | `/*` |
|      - |   19 | ` * Remember a diagnostic for error_get_last(). php records the last error that reached` |
|      - |   20 | ` * DEFAULT processing: one hidden by '@' or by error_reporting() still counts, but one a` |
|      - |   21 | ` * user handler claimed (by returning true) does not -- so this is called only on the` |
|      - |   22 | ` * default-processing path.` |
|      - |   23 | ` */` |
|  19678 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|      5 |   25 | `{` |
|  19683 |   26 | `	pVm->nLastErrType = iErr;` |
|  19683 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|  19683 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|  19683 |   29 | `	if( zMsg && nMsg > 0 ){` |
|  19683 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   9839 |   31 | `	}` |
|  19683 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|  19683 |   33 | `	if( pFile ){` |
|  19683 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   9839 |   35 | `	}` |
|  19683 |   36 | `}` |
|    628 |   37 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|      4 |   38 | `{` |
|    632 |   39 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    632 |   40 | `	sxi32 rc = SXRET_OK;` |
|      - |   41 | `	/* Append a new line */` |
|      - |   42 | `#ifdef __WINNT__` |
|      4 |   43 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|      - |   44 | `#else` |
|    628 |   45 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|      - |   46 | `#endif` |
|      - |   47 | `	/* Invoke the output consumer callback */` |
|    632 |   48 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|    632 |   49 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|    632 |   50 | `	return rc;` |
|      4 |   51 | `}` |
|      - |   52 | `/*` |
|      - |   53 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - |   54 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|      - |   55 | ` * information.` |
|      - |   56 | ` */` |
|  19798 |   57 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|      5 |   58 | `{` |
|  19803 |   59 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|      - |   60 | `		ph7_value apArg[4];` |
|      - |   61 | `		ph7_value *apArgPtr[4];` |
|      - |   62 | `		ph7_value sResult;` |
|      - |   63 | `		SyString sErr;` |
|      - |   64 | `		/* Prepare arguments */` |
|    128 |   65 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|      - |   66 | `			/* use explicit message length to avoid reading past buffer */` |
|    128 |   67 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|    128 |   68 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|    128 |   69 | `		if( pFile ){` |
|    128 |   70 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|    128 |   71 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     66 |   72 | `		}else{` |
|    ! 0 |   73 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|      - |   74 | `		}` |
|    128 |   75 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|    128 |   76 | `		PH7_MemObjInit(pVm,&sResult);` |
|      - |   77 | `		/* Set up pointer array */` |
|    128 |   78 | `		apArgPtr[0] = &apArg[0];` |
|    128 |   79 | `		apArgPtr[1] = &apArg[1];` |
|    128 |   80 | `		apArgPtr[2] = &apArg[2];` |
|    128 |   81 | `		apArgPtr[3] = &apArg[3];` |
|      - |   82 | `		/* Call the handler */` |
|      - |   83 | `		{` |
|    128 |   84 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|    128 |   85 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|      - |   86 | `				/* The handler threw (or aborted) instead of returning: php never` |
|      - |   87 | `				 * reports the original diagnostic then — the exception supersedes` |
|      - |   88 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|      - |   89 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|      - |   90 | `				 * user's catch already ran. */` |
|      3 |   91 | `				PH7_MemObjRelease(&apArg[0]);` |
|      3 |   92 | `				PH7_MemObjRelease(&apArg[1]);` |
|      3 |   93 | `				PH7_MemObjRelease(&apArg[2]);` |
|      3 |   94 | `				PH7_MemObjRelease(&apArg[3]);` |
|      3 |   95 | `				PH7_MemObjRelease(&sResult);` |
|      3 |   96 | `				return FALSE;` |
|      - |   97 | `			}` |
|      - |   98 | `		}` |
|      - |   99 | `		/* Check return value */` |
|    126 |  100 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 |  101 | `			PH7_MemObjToBool(&sResult);` |
|    ! 0 |  102 | `		}` |
|      - |  103 | `		/* Release */` |
|    126 |  104 | `		PH7_MemObjRelease(&apArg[0]);` |
|    126 |  105 | `		PH7_MemObjRelease(&apArg[1]);` |
|    126 |  106 | `		PH7_MemObjRelease(&apArg[2]);` |
|    126 |  107 | `		PH7_MemObjRelease(&apArg[3]);` |
|    126 |  108 | `		PH7_MemObjRelease(&sResult);` |
|      - |  109 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|      - |  110 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|    126 |  111 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|      - |  112 | `	}` |
|      - |  113 | `	/* No handler, always call error handler */` |
|  19679 |  114 | `	return TRUE;` |
|   9904 |  115 | `}` |
|      - |  116 | `/*` |
|      - |  117 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|      - |  118 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|      - |  119 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|      - |  120 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|      - |  121 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|      - |  122 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|      - |  123 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|      - |  124 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|      - |  125 | ` * only picks the DISPLAY label.` |
|      - |  126 | ` */` |
|      - |  127 | `/*` |
|      - |  128 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|      - |  129 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|      - |  130 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|      - |  131 | ` * every deprecation.` |
|      - |  132 | ` */` |
|  19678 |  133 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|      5 |  134 | `{` |
|      - |  135 | `	sxi32 iBit;` |
|  19683 |  136 | `	if( !pVm->bErrReport ){` |
|   3532 |  137 | `		return 0;` |
|      - |  138 | `	}` |
|  16153 |  139 | `	switch( iErr ){` |
|   8053 |  140 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|  16111 |  141 | `		iBit = 2; break;` |
|      5 |  142 | `	case 512  /* E_USER_WARNING */:` |
|     12 |  143 | `		iBit = 512; break;` |
|    ! 0 |  144 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|      - |  145 | `	case 8    /* E_NOTICE */:` |
|    ! 0 |  146 | `		iBit = 8; break;` |
|      3 |  147 | `	case 1024 /* E_USER_NOTICE */:` |
|      9 |  148 | `		iBit = 1024; break;` |
|    ! 0 |  149 | `	case 8192 /* E_DEPRECATED */:` |
|    ! 0 |  150 | `		iBit = 8192; break;` |
|    ! 0 |  151 | `	case 16384 /* E_USER_DEPRECATED */:` |
|    ! 0 |  152 | `		iBit = 16384; break;` |
|    ! 0 |  153 | `	case 256  /* E_USER_ERROR */:` |
|    ! 0 |  154 | `		iBit = 256; break;` |
|     13 |  155 | `	default:` |
|     30 |  156 | `		iBit = 1; /* E_ERROR and everything else fatal-ish */` |
|     26 |  157 | `		break;` |
|      - |  158 | `	}` |
|  16153 |  159 | `	return (pVm->iErrMask & iBit) != 0;` |
|   9844 |  160 | `}` |
|  19798 |  161 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|      5 |  162 | `{` |
|  19803 |  163 | `	switch(iErr){` |
|   9862 |  164 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|      - |  165 | `	case 512  /* E_USER_WARNING */:` |
|  19729 |  166 | `		return "Warning";` |
|      5 |  167 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|      - |  168 | `	case 8    /* E_NOTICE */:` |
|      - |  169 | `	case 1024 /* E_USER_NOTICE */:` |
|     14 |  170 | `		return "Notice";` |
|     19 |  171 | `	case 8192  /* E_DEPRECATED */:` |
|      - |  172 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     39 |  173 | `		return "Deprecated";` |
|    ! 0 |  174 | `	case 256 /* E_USER_ERROR */:` |
|    ! 0 |  175 | `		return "Fatal error";` |
|     13 |  176 | `	default:` |
|     30 |  177 | `		return "Error";` |
|      - |  178 | `	}` |
|   9904 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|      - |  182 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|      - |  183 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|      - |  184 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|      - |  185 | ` * be authored cross-engine with --EXPECTF--.` |
|      - |  186 | ` */` |
|  19798 |  187 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr,SyString *pFuncName)` |
|      5 |  188 | `{` |
|  19803 |  189 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|  19803 |  190 | `	if( pFuncName ){` |
|     41 |  191 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|     41 |  192 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|     18 |  193 | `	}` |
|  19803 |  194 | `}` |
|    104 |  195 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|      5 |  196 | `{` |
|    109 |  197 | `	if( pFile ){` |
|    161 |  198 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|     52 |  199 | `			nLine ? nLine : 1);` |
|     52 |  200 | `	}` |
|    109 |  201 | `}` |
|     76 |  202 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|      - |  203 | `	ph7_vm *pVm,         /* Target VM */` |
|      - |  204 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|      - |  205 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|      - |  206 | `	const char *zMessage /* Null terminated error message */` |
|      - |  207 | `	)` |
|      5 |  208 | `{` |
|     81 |  209 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      - |  210 | `	SyString *pFile;` |
|     81 |  211 | `	sxi32 rc = SXRET_OK;` |
|      - |  212 | `	/* Reset the working buffer */` |
|     81 |  213 | `	SyBlobReset(pWorker);` |
|      - |  214 | `	/* Peek the processed file if available */` |
|     81 |  215 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     81 |  216 | `	VmDiagnosticHeader(pWorker,iErr,pFuncName);` |
|     81 |  217 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|     81 |  218 | `	VmDiagnosticLocation(pWorker,pFile,pVm->nCurLine);` |
|      - |  219 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|      - |  220 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     81 |  221 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, (sxi32)pVm->nCurLine) ){` |
|     48 |  222 | `		VmRecordLastError(&(*pVm),iErr,zMessage,SyStrlen(zMessage),pFile);` |
|     48 |  223 | `		if( !VmErrReportWants(pVm,iErr) ){` |
|      - |  224 | `			/* error_reporting() masks this severity out of the DISPLAY */` |
|      3 |  225 | `			return SXRET_OK;` |
|      - |  226 | `		}` |
|     46 |  227 | `		if( pVm->nErrSuppress > 0 ){` |
|      - |  228 | `			/* Inside '@': php still runs a user handler (done just above) but` |
|      - |  229 | `			 * prints nothing itself. */` |
|      3 |  230 | `			return SXRET_OK;` |
|      - |  231 | `		}` |
|     44 |  232 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|     20 |  233 | `	}` |
|     77 |  234 | `	return rc;` |
|     43 |  235 | `}` |
|      - |  236 | `/*` |
|      - |  237 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|      - |  238 | ` *` |
|      - |  239 | ` * This is the single choke point for surfacing an allocation failure that would` |
|      - |  240 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|      - |  241 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|      - |  242 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|      - |  243 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|      - |  244 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|      - |  245 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|      - |  246 | ` * calling it from a VM op.` |
|      - |  247 | ` */` |
|    ! 0 |  248 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|    ! 0 |  249 | `{` |
|    ! 0 |  250 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - |  251 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|    ! 0 |  252 | `	pVm->iExitStatus = 255;` |
|    ! 0 |  253 | `	pVm->bHaltRequested = 1;` |
|    ! 0 |  254 | `	return PH7_ABORT;` |
|    ! 0 |  255 | `}` |
|      - |  256 | `/*` |
|      - |  257 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|      - |  258 | ` */` |
|    ! 0 |  259 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|    ! 0 |  260 | `{` |
|    ! 0 |  261 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|    ! 0 |  262 | `}` |
|      - |  263 | `/*` |
|      - |  264 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|      - |  265 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|      - |  266 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|      - |  267 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|      - |  268 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|      - |  269 | ` */` |
|      - |  270 | ``/* php only DEPRECATES a lossy float->int operand (`5 % 2.7`, `3 \| 1.5`); PHL targets`` |
|      - |  271 | ` * php's non-deprecated surface and rejects it with a TypeError. An INTEGRAL float` |
|      - |  272 | `` * (`4.0 % 3`) loses nothing and is accepted. Returns SXRET_OK to continue, or the`` |
|      - |  273 | ` * throw status for the caller to route via PH7_DISPATCH_ENFORCE_RC. */` |
|   4182 |  274 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|      5 |  275 | `{` |
|      - |  276 | `	double r;` |
|   4187 |  277 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_REAL) == 0 ){` |
|   4187 |  278 | `		return SXRET_OK;` |
|      - |  279 | `	}` |
|    ! 0 |  280 | `	r = (double)pVal->rVal;` |
|    ! 0 |  281 | `	if( r == (double)(sxi64)r ){` |
|    ! 0 |  282 | `		return SXRET_OK;` |
|      - |  283 | `	}` |
|    ! 0 |  284 | `	return VmThrowFixedError(pVm,"TypeError",` |
|      - |  285 | `		"Implicit conversion from float to int loses precision");` |
|   2096 |  286 | `}` |
|      - |  287 | `/*` |
|      - |  288 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|      - |  289 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|      - |  290 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|      - |  291 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|      - |  292 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|      - |  293 | ` */` |
|  89128 |  294 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|      5 |  295 | `{` |
|      - |  296 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|      - |  297 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  89133 |  298 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|      5 |  299 | `}` |
|      - |  300 | `/*` |
|      - |  301 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|      - |  302 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|      - |  303 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|      - |  304 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|      - |  305 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|      - |  306 | ` * keep native re-entries off a finite C stack).` |
|      - |  307 | ` */` |
| 158510 |  308 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|      5 |  309 | `{` |
| 158515 |  310 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|      5 |  311 | `}` |
|      - |  312 | `/*` |
|      - |  313 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|      - |  314 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|      - |  315 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|      - |  316 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|      - |  317 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|      - |  318 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|      - |  319 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|      - |  320 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|      - |  321 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|      - |  322 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|      - |  323 | ` * (VmNativeNestingFatal).` |
|      - |  324 | ` *` |
|      - |  325 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|      - |  326 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|      - |  327 | ` * re-enter and loop.` |
|      - |  328 | ` */` |
|      2 |  329 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|      1 |  330 | `{` |
|      3 |  331 | `	if( pVm->bHaltRequested ){` |
|    ! 0 |  332 | `		return PH7_ABORT;` |
|      - |  333 | `	}` |
|      3 |  334 | `	pVm->iExitStatus = 255;` |
|      3 |  335 | `	pVm->bHaltRequested = 1;` |
|      3 |  336 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|      3 |  337 | `	return PH7_ABORT;` |
|      2 |  338 | `}` |
|      - |  339 | `/*` |
|      - |  340 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|      - |  341 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|      - |  342 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|      - |  343 | ` */` |
|      4 |  344 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|      1 |  345 | `{` |
|      5 |  346 | `	if( pVm->bHaltRequested ){` |
|    ! 0 |  347 | `		return PH7_ABORT;` |
|      - |  348 | `	}` |
|      5 |  349 | `	pVm->iExitStatus = 255;` |
|      5 |  350 | `	pVm->bHaltRequested = 1;` |
|      5 |  351 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|      5 |  352 | `	return PH7_ABORT;` |
|      3 |  353 | `}` |
|      - |  354 | `/*` |
|      - |  355 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - |  356 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|      - |  357 | ` * information.` |
|      - |  358 | ` */` |
|  19722 |  359 | `static sxi32 VmThrowErrorAp(` |
|      - |  360 | `	ph7_vm *pVm,         /* Target VM */` |
|      - |  361 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|      - |  362 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|      - |  363 | `	const char *zFormat, /* Format message */` |
|      - |  364 | `	va_list ap           /* Variable list of arguments */` |
|      - |  365 | `	)` |
|      5 |  366 | `{` |
|  19727 |  367 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      - |  368 | `	SyBlob sMsg;` |
|      - |  369 | `	SyString *pFile;` |
|  19727 |  370 | `	sxi32 rc = SXRET_OK;` |
|      - |  371 | `	/* Reset the working buffer */` |
|  19727 |  372 | `	SyBlobReset(pWorker);` |
|      - |  373 | `	/* Peek the processed file if available */` |
|  19727 |  374 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  19727 |  375 | `	VmDiagnosticHeader(pWorker,iErr,pFuncName);` |
|      - |  376 | `	/* Format the raw message */` |
|  19727 |  377 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|  19727 |  378 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      - |  379 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|      - |  380 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|      - |  381 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|      - |  382 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|  19727 |  383 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|      - |  384 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|      - |  385 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|  19639 |  386 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|  19639 |  387 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|  19611 |  388 | `			SyBlobRelease(&sMsg);` |
|  19611 |  389 | `			return SXRET_OK;` |
|      - |  390 | `		}` |
|     31 |  391 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     31 |  392 | `		VmDiagnosticLocation(pWorker,pFile,pVm->nCurLine);` |
|     31 |  393 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|     14 |  394 | `	}` |
|    120 |  395 | `	SyBlobRelease(&sMsg);` |
|    120 |  396 | `	return rc;` |
|   9866 |  397 | `}` |
|      - |  398 | `/*` |
|      - |  399 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|      - |  400 | ` * scope), or NULL when executing outside any class context.` |
|      - |  401 | ` */` |
|    256 |  402 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|      5 |  403 | `{` |
|    261 |  404 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|    125 |  405 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|    125 |  406 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      - |  407 | `	}` |
|    141 |  408 | `	return 0;` |
|    133 |  409 | `}` |
|      - |  410 | `/*` |
|      - |  411 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|      - |  412 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|      - |  413 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|      - |  414 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|      - |  415 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|      - |  416 | ` */` |
|    258 |  417 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|      5 |  418 | `{` |
|      - |  419 | `	ph7_class *pErrClass;` |
|      - |  420 | `	ph7_class_instance *pThis;` |
|      - |  421 | `	ph7_class_method *pCons;` |
|      - |  422 | `	VmFrame *pFrame;` |
|      - |  423 | `	sxi32 rc;` |
|    263 |  424 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|    263 |  425 | `	if( pErrClass == 0 ){` |
|    ! 0 |  426 | `		SyBlobRelease(pMsg);` |
|    ! 0 |  427 | `		return PH7_ABORT;` |
|      - |  428 | `	}` |
|    263 |  429 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|    263 |  430 | `	if( pThis == 0 ){` |
|    ! 0 |  431 | `		SyBlobRelease(pMsg);` |
|    ! 0 |  432 | `		return PH7_ABORT;` |
|      - |  433 | `	}` |
|    263 |  434 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|    263 |  435 | `	if( pCons ){` |
|      - |  436 | `		ph7_value sArg;` |
|      - |  437 | `		ph7_value *apArg[1];` |
|      - |  438 | `		SyString sMsgStr;` |
|    263 |  439 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|    263 |  440 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    263 |  441 | `		apArg[0] = &sArg;` |
|    263 |  442 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|    263 |  443 | `		PH7_MemObjRelease(&sArg);` |
|    129 |  444 | `	}` |
|    263 |  445 | `	SyBlobRelease(pMsg);` |
|    263 |  446 | `	pFrame = pVm->pFrame;` |
|    263 |  447 | `	if( pFrame ){` |
|    263 |  448 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    263 |  449 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    129 |  450 | `	}` |
|    263 |  451 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    263 |  452 | `	PH7_ClassInstanceUnref(pThis);` |
|    263 |  453 | `	if( rc == SXERR_ABORT ){` |
|     28 |  454 | `		return PH7_ABORT;` |
|      - |  455 | `	}` |
|    239 |  456 | `	return PH7_EXCEPTION;` |
|    134 |  457 | `}` |
|      - |  458 | `/*` |
|      - |  459 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|      - |  460 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|      - |  461 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|      - |  462 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|      - |  463 | ` * result through its normal goto Exception / goto Abort.` |
|      - |  464 | ` */` |
|     22 |  465 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|      3 |  466 | `{` |
|      - |  467 | `	SyBlob sMsg;` |
|     25 |  468 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|     25 |  469 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|     25 |  470 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|      3 |  471 | `}` |
|      - |  472 | `/*` |
|      - |  473 | ` * Enum case singletons (PHP 8.1).` |
|      - |  474 | ` *` |
|      - |  475 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|      - |  476 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|      - |  477 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|      - |  478 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|      - |  479 | ` * backing-value type check and the duplicate-value check only fire when a` |
|      - |  480 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|      - |  481 | ` */` |
|      - |  482 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|      - |  483 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|      - |  484 | ` * readonly property" through the normal store path. */` |
|     82 |  485 | `static void VmEnumSetInstanceProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|      - |  486 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|      1 |  487 | `{` |
|     83 |  488 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|      - |  489 | `	VmClassAttr *pVmAttr;` |
|      - |  490 | `	ph7_value *pSlot;` |
|     83 |  491 | `	if( pEntry == 0 ){` |
|    ! 0 |  492 | `		return;` |
|      - |  493 | `	}` |
|     83 |  494 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     83 |  495 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     83 |  496 | `	if( pSlot == 0 ){` |
|    ! 0 |  497 | `		return;` |
|      - |  498 | `	}` |
|     83 |  499 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     83 |  500 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     42 |  501 | `}` |
|      - |  502 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|      - |  503 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|    124 |  504 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|      1 |  505 | `{` |
|    125 |  506 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|      - |  507 | `	ph7_class_instance *pObj;` |
|      - |  508 | `	SyHashEntry *pEntry;` |
|    125 |  509 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     45 |  510 | `		return 0;` |
|      - |  511 | `	}` |
|     81 |  512 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|     81 |  513 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|     81 |  514 | `	if( pEntry == 0 ){` |
|    ! 0 |  515 | `		return 0;` |
|      - |  516 | `	}` |
|     81 |  517 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|     63 |  518 | `}` |
|      - |  519 | `/*` |
|      - |  520 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|      - |  521 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|      - |  522 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|      - |  523 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|      - |  524 | ` * raises it. Returns the throw status to park/route.` |
|      - |  525 | ` */` |
|      2 |  526 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|      1 |  527 | `{` |
|      - |  528 | `	SyBlob sMsg;` |
|      3 |  529 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|      3 |  530 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|      3 |  531 | `	pVm->pConstCycleAttr = 0;` |
|      3 |  532 | `	pVm->pConstCycleClass = 0;` |
|      3 |  533 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 |  534 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|      1 |  535 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|      3 |  536 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      1 |  537 | `}` |
|      - |  538 | `/*` |
|      - |  539 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|      - |  540 | ` * materialize lazily and individually on first access — the backing-value` |
|      - |  541 | ` * type check fires per case, and the duplicate-value check compares only` |
|      - |  542 | ` * against cases that have already materialized (a broken sibling case does` |
|      - |  543 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|      - |  544 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|      - |  545 | ` * (duplicate value / self-reference) — which the caller routes` |
|      - |  546 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|      - |  547 | ` */` |
|    108 |  548 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|      1 |  549 | `{` |
|      - |  550 | `	ph7_class_attr **apCase;` |
|      - |  551 | `	ph7_class_instance *pObj;` |
|      - |  552 | `	ph7_value *pSlot;` |
|      - |  553 | `	ph7_value sBacking,sPropVal;` |
|      - |  554 | `	sxu32 i;` |
|    109 |  555 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|     59 |  556 | `		return SXRET_OK;` |
|      - |  557 | `	}` |
|     51 |  558 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|      - |  559 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|      - |  560 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|    ! 0 |  561 | `		if( pVm->pConstCycleAttr == 0 ){` |
|    ! 0 |  562 | `			pVm->pConstCycleAttr = pCase;` |
|    ! 0 |  563 | `			pVm->pConstCycleClass = pClass;` |
|    ! 0 |  564 | `		}` |
|    ! 0 |  565 | `		return SXRET_OK;` |
|      - |  566 | `	}` |
|     51 |  567 | `	PH7_MemObjInit(pVm,&sBacking);` |
|     51 |  568 | `	if( pClass->nEnumBacking != 0 ){` |
|     41 |  569 | `		if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|      - |  570 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|     41 |  571 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      - |  572 | `			sxi32 rcExec;` |
|     41 |  573 | `			pVm->pConstEvalClass = pClass;` |
|     41 |  574 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|     41 |  575 | `			pVm->nConstEvalDepth++;` |
|     41 |  576 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|     41 |  577 | `			pVm->nConstEvalDepth--;` |
|     41 |  578 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     41 |  579 | `			pVm->pConstEvalClass = pSaveCtx;` |
|     41 |  580 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|      - |  581 | `				/* The backing expression raised: abandon materialization and` |
|      - |  582 | `				 * hand the status to the caller to park/route. */` |
|    ! 0 |  583 | `				PH7_MemObjRelease(&sBacking);` |
|    ! 0 |  584 | `				return rcExec;` |
|      - |  585 | `			}` |
|     41 |  586 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|    ! 0 |  587 | `				PH7_MemObjRelease(&sBacking);` |
|    ! 0 |  588 | `				return VmConstCycleThrow(&(*pVm));` |
|      - |  589 | `			}` |
|     20 |  590 | `		}` |
|     41 |  591 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|      - |  592 | `			/* php: TypeError, checked lazily at first case access */` |
|      - |  593 | `			SyBlob sMsg;` |
|      3 |  594 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|      3 |  595 | `			PH7_MemObjRelease(&sBacking);` |
|      3 |  596 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      2 |  597 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|      2 |  598 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|      3 |  599 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      - |  600 | `		}` |
|     39 |  601 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      - |  602 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|      - |  603 | `			 * the typed-constant leniency) to a genuine int. */` |
|     11 |  604 | `			PH7_MemObjToInteger(&sBacking);` |
|      6 |  605 | `		}else{` |
|     29 |  606 | `			PH7_MemObjToString(&sBacking);` |
|      - |  607 | `		}` |
|      - |  608 | `		/* php: two cases sharing one backing value are an Error — compared` |
|      - |  609 | `		 * against already-materialized cases only (php registers values as` |
|      - |  610 | `		 * each case evaluates). */` |
|     39 |  611 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|    159 |  612 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|      - |  613 | `			ph7_value *pPrev;` |
|    123 |  614 | `			int bDup = 0;` |
|    123 |  615 | `			if( apCase[i] == pCase ){` |
|     37 |  616 | `				continue;` |
|      - |  617 | `			}` |
|     87 |  618 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|     87 |  619 | `			if( pPrev ){` |
|     43 |  620 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      5 |  621 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|      3 |  622 | `				}else{` |
|     45 |  623 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|     38 |  624 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|     12 |  625 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|      - |  626 | `				}` |
|     21 |  627 | `			}` |
|     87 |  628 | `			if( bDup ){` |
|      - |  629 | `				/* php prints the two cases in DECLARATION order regardless of` |
|      - |  630 | `				 * which one is being evaluated. */` |
|      3 |  631 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|      - |  632 | `				SyBlob sMsg;` |
|      - |  633 | `				sxu32 j;` |
|      5 |  634 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|      5 |  635 | `					if( apCase[j] == pCase ){ break; }` |
|      2 |  636 | `				}` |
|      3 |  637 | `				if( j < i ){` |
|    ! 0 |  638 | `					pFirst = pCase;` |
|    ! 0 |  639 | `					pSecond = apCase[i];` |
|    ! 0 |  640 | `				}` |
|      3 |  641 | `				PH7_MemObjRelease(&sBacking);` |
|      3 |  642 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 |  643 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|      1 |  644 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|      3 |  645 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      - |  646 | `			}` |
|     43 |  647 | `		}` |
|     18 |  648 | `	}` |
|      - |  649 | `	/* Create the singleton and fill its readonly props */` |
|     47 |  650 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|     47 |  651 | `	if( pObj == 0 ){` |
|    ! 0 |  652 | `		PH7_MemObjRelease(&sBacking);` |
|    ! 0 |  653 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - |  654 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|    ! 0 |  655 | `			&pClass->sName,&pCase->sName);` |
|    ! 0 |  656 | `		return PH7_ABORT;` |
|      - |  657 | `	}` |
|     47 |  658 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|     47 |  659 | `	VmEnumSetInstanceProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|     47 |  660 | `	PH7_MemObjRelease(&sPropVal);` |
|     47 |  661 | `	if( pClass->nEnumBacking != 0 ){` |
|     37 |  662 | `		VmEnumSetInstanceProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|     18 |  663 | `	}` |
|     47 |  664 | `	PH7_MemObjRelease(&sBacking);` |
|      - |  665 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|      - |  666 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|     47 |  667 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|     47 |  668 | `	if( pSlot == 0 ){` |
|    ! 0 |  669 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  670 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - |  671 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|    ! 0 |  672 | `			&pClass->sName,&pCase->sName);` |
|    ! 0 |  673 | `		return PH7_ABORT;` |
|      - |  674 | `	}` |
|     47 |  675 | `	pSlot->x.pOther = pObj;` |
|     47 |  676 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|     47 |  677 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     47 |  678 | `	pCase->nIdx = pSlot->nIdx;` |
|     47 |  679 | `	return SXRET_OK;` |
|     55 |  680 | `}` |
|      - |  681 | `/*` |
|      - |  682 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|      - |  683 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|      - |  684 | ` * there, so a broken case surfaces its error at the same point).` |
|      - |  685 | ` */` |
|     46 |  686 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|      1 |  687 | `{` |
|      - |  688 | `	ph7_class_attr **apCase;` |
|      - |  689 | `	sxu32 n;` |
|     47 |  690 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|    ! 0 |  691 | `		return SXRET_OK;` |
|      - |  692 | `	}` |
|     47 |  693 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|    145 |  694 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|    103 |  695 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|    103 |  696 | `		if( rc != SXRET_OK ){` |
|      5 |  697 | `			return rc;` |
|      - |  698 | `		}` |
|     50 |  699 | `	}` |
|     43 |  700 | `	return SXRET_OK;` |
|     24 |  701 | `}` |
|      - |  702 | `/*` |
|      - |  703 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|      - |  704 | ` * or 0 when the name does not name an enum.` |
|      - |  705 | ` */` |
|     34 |  706 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|      1 |  707 | `{` |
|      - |  708 | `	ph7_class *pClass;` |
|     35 |  709 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|    ! 0 |  710 | `		return 0;` |
|      - |  711 | `	}` |
|     52 |  712 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|     17 |  713 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|     37 |  714 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      3 |  715 | `		pClass = pClass->pNextName;` |
|      1 |  716 | `	}` |
|     35 |  717 | `	return pClass;` |
|     18 |  718 | `}` |
|      - |  719 | `/*` |
|      - |  720 | ` * Evaluate a class constant's initializer on demand.` |
|      - |  721 | ` *` |
|      - |  722 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|      - |  723 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|      - |  724 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|      - |  725 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|      - |  726 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|      - |  727 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|      - |  728 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|      - |  729 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|      - |  730 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|      - |  731 | ` * "Cannot declare self-referencing constant" Error.` |
|      - |  732 | ` */` |
|    214 |  733 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  734 | `{` |
|      - |  735 | `	ph7_value *pMemObj;` |
|    214 |  736 | `	if( pAttr->nIdx != SXU32_HIGH` |
|    214 |  737 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|    216 |  738 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|    ! 0 |  739 | `		return SXRET_OK;` |
|      - |  740 | `	}` |
|    216 |  741 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|      - |  742 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|      - |  743 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|      - |  744 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|      3 |  745 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      3 |  746 | `			pVm->pConstCycleAttr = pAttr;` |
|      3 |  747 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      1 |  748 | `		}` |
|      3 |  749 | `		return SXRET_OK;` |
|      - |  750 | `	}` |
|    214 |  751 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|    214 |  752 | `	if( pMemObj == 0 ){` |
|    ! 0 |  753 | `		return SXERR_MEM;` |
|      - |  754 | `	}` |
|    214 |  755 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|    214 |  756 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|    214 |  757 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      - |  758 | `		sxi32 rcExec;` |
|    214 |  759 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|    214 |  760 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      - |  761 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|      - |  762 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|      - |  763 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|    214 |  764 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|    214 |  765 | `		pVm->nConstEvalDepth++;` |
|    214 |  766 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|    214 |  767 | `		pVm->nConstEvalDepth--;` |
|    214 |  768 | `		pVm->pConstEvalClass = pSaveCtx;` |
|    214 |  769 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|    214 |  770 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      - |  771 | `		/* Memoize before any throw so re-access doesn't loop. */` |
|    214 |  772 | `		pAttr->nIdx = pMemObj->nIdx;` |
|    214 |  773 | `		PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|    214 |  774 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|      - |  775 | `			/* The initializer raised: hand the status to the caller to` |
|      - |  776 | `			 * park/route. */` |
|    ! 0 |  777 | `			return rcExec;` |
|      - |  778 | `		}` |
|    214 |  779 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      - |  780 | `			/* A nested evaluation detected a self-referencing constant:` |
|      - |  781 | `			 * raise it here, at opcode level, where it routes to a catch. */` |
|      3 |  782 | `			return VmConstCycleThrow(&(*pVm));` |
|      - |  783 | `		}` |
|    212 |  784 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    ! 0 |  785 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|    ! 0 |  786 | `			if( rcType != SXRET_OK ){` |
|    ! 0 |  787 | `				return rcType;` |
|      - |  788 | `			}` |
|    ! 0 |  789 | `		}` |
|    212 |  790 | `		return SXRET_OK;` |
|      - |  791 | `	}` |
|    ! 0 |  792 | `	pAttr->nIdx = pMemObj->nIdx;` |
|    ! 0 |  793 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|    ! 0 |  794 | `	return SXRET_OK;` |
|    109 |  795 | `}` |
|      - |  796 | `/*` |
|      - |  797 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|      - |  798 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|      - |  799 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|      - |  800 | ` */` |
|     48 |  801 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|      1 |  802 | `{` |
|     49 |  803 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     27 |  804 | `		return SXRET_OK;` |
|      - |  805 | `	}` |
|     23 |  806 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|      5 |  807 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|      - |  808 | `	}` |
|     19 |  809 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|     25 |  810 | `}` |
|      - |  811 | `/*` |
|      - |  812 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|      - |  813 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|      - |  814 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|      - |  815 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|      - |  816 | ` */` |
|      6 |  817 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|      1 |  818 | `{` |
|      - |  819 | `	SyBlob sMsg;` |
|      7 |  820 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      7 |  821 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|      7 |  822 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      1 |  823 | `}` |
|      - |  824 | `/*` |
|      - |  825 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|      - |  826 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|      - |  827 | ` * raised at the store site here with the same message and the same` |
|      - |  828 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|      - |  829 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|      - |  830 | ` */` |
|      2 |  831 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|      1 |  832 | `{` |
|      3 |  833 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|      3 |  834 | `	pVm->iExitStatus = 255;` |
|      3 |  835 | `	pVm->bHaltRequested = 1;` |
|      3 |  836 | `	return PH7_ABORT;` |
|      1 |  837 | `}` |
|      - |  838 | `/*` |
|      - |  839 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|      - |  840 | ` * property assignment. Called from the STORE path when coercion is not` |
|      - |  841 | ` * possible.` |
|      - |  842 | ` */` |
|     54 |  843 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|      5 |  844 | `{` |
|     59 |  845 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|     59 |  846 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      - |  847 | `	SyBlob sMsg;` |
|     59 |  848 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      - |  849 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|      - |  850 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|     59 |  851 | `	if( pOwner ){` |
|     59 |  852 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|     27 |  853 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|     32 |  854 | `	}else{` |
|    ! 0 |  855 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|    ! 0 |  856 | `			zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|      - |  857 | `	}` |
|     59 |  858 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      5 |  859 | `}` |
|      - |  860 | `/*` |
|      - |  861 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|      - |  862 | ` */` |
|      6 |  863 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|      3 |  864 | `{` |
|      9 |  865 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      9 |  866 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|      - |  867 | `	SyBlob sMsg;` |
|      9 |  868 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      9 |  869 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|      3 |  870 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|      9 |  871 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      3 |  872 | `}` |
|      - |  873 | `/*` |
|      - |  874 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|      - |  875 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|      - |  876 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|      - |  877 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|      - |  878 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|      - |  879 | ` */` |
|      - |  880 | `/*` |
|      - |  881 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|      - |  882 | ` * property from a scope its set-visibility excludes:` |
|      - |  883 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|      - |  884 | ` */` |
|     14 |  885 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|      1 |  886 | `{` |
|     15 |  887 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|     15 |  888 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|     15 |  889 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|      - |  890 | `	SyBlob sMsg;` |
|     15 |  891 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     15 |  892 | `	if( pActive ){` |
|      3 |  893 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|      1 |  894 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|      2 |  895 | `	}else{` |
|     13 |  896 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|      6 |  897 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|      - |  898 | `	}` |
|     15 |  899 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      1 |  900 | `}` |
|      - |  901 | `/*` |
|      - |  902 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|      - |  903 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|      - |  904 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|      - |  905 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|      - |  906 | ` */` |
|     32 |  907 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|      1 |  908 | `{` |
|     33 |  909 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|     33 |  910 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|      - |  911 | `	int bOk;` |
|     33 |  912 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|     27 |  913 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|     14 |  914 | `	}else{` |
|      7 |  915 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|      - |  916 | `	}` |
|     33 |  917 | `	if( !bOk ){` |
|     15 |  918 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|      - |  919 | `	}` |
|     19 |  920 | `	return SXRET_OK;` |
|     17 |  921 | `}` |
|     30 |  922 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|      5 |  923 | `{` |
|     35 |  924 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      - |  925 | `	SyBlob sMsg;` |
|     35 |  926 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     35 |  927 | `	if( bModify ){` |
|     31 |  928 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|     18 |  929 | `	}else{` |
|      6 |  930 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|      6 |  931 | `		if( pActive ){` |
|    ! 0 |  932 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|    ! 0 |  933 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|    ! 0 |  934 | `		}else{` |
|      6 |  935 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|      2 |  936 | `				&pOwner->sName,&pAttr->sName);` |
|      - |  937 | `		}` |
|      - |  938 | `	}` |
|     35 |  939 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      5 |  940 | `}` |
|      - |  941 | `/*` |
|      - |  942 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|      - |  943 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|      - |  944 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|      - |  945 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|      - |  946 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|      - |  947 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|      - |  948 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|      - |  949 | ` */` |
| 419560 |  950 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|      5 |  951 | `{` |
|      - |  952 | `	SyHashEntry *pSlot;` |
|      - |  953 | `	VmClassAttr *pVmAttr;` |
| 419565 |  954 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
| 332114 |  955 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|      - |  956 | `	}` |
|  87454 |  957 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  87454 |  958 | `	if( pSlot == 0 ){` |
|  87368 |  959 | `		return SXRET_OK; /* Not a typed slot */` |
|      - |  960 | `	}` |
|     87 |  961 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     87 |  962 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     12 |  963 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|      - |  964 | `	}` |
|     74 |  965 | `	if( pVmAttr->pAttr` |
|     76 |  966 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|      - |  967 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|      7 |  968 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|      - |  969 | `	}` |
|     70 |  970 | `	return SXRET_OK;` |
| 210009 |  971 | `}` |
|      - |  972 | `/*` |
|      - |  973 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|      - |  974 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|      - |  975 | ` * For class types, instanceof is verified.` |
|      - |  976 | ` *` |
|      - |  977 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|      - |  978 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|      - |  979 | ` */` |
|      - |  980 |  |
|      - |  981 | `/*` |
|      - |  982 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|      - |  983 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|      - |  984 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|      - |  985 | ` *   0 if it's not strictly numeric.` |
|      - |  986 | ` */` |
|     18 |  987 | `static int VmStringNumericKind(ph7_value *pValue)` |
|      2 |  988 | `{` |
|      - |  989 | `	const char *z, *zEnd, *zTail;` |
|      - |  990 | `	sxu32 n;` |
|     20 |  991 | `	sxu8 bReal = 0;` |
|      - |  992 | `	sxi32 rc;` |
|     20 |  993 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 |  994 | `		return 0;` |
|      - |  995 | `	}` |
|     18 |  996 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|     18 |  997 | `	n = SyBlobLength(&pValue->sBlob);` |
|     18 |  998 | `	zEnd = z + n;` |
|     18 |  999 | `	if( n == 0 ) return 0;` |
|     18 | 1000 | `	zTail = 0;` |
|     18 | 1001 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|     18 | 1002 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|     19 | 1003 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|     15 | 1004 | `	if( zTail != zEnd ) return 0;` |
|     15 | 1005 | `	return bReal ? 2 : 1;` |
|     11 | 1006 | `}` |
|      - | 1007 |  |
|      - | 1008 | `/*` |
|      - | 1009 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|      - | 1010 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|      - | 1011 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|      - | 1012 | ` * property, union alternative — would have to string-match the name itself.` |
|      - | 1013 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|      - | 1014 | ` * to extend when another literal/pseudo type is added.` |
|      - | 1015 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|      - | 1016 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|      - | 1017 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|      - | 1018 | ` */` |
|    652 | 1019 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|      5 | 1020 | `{` |
|    657 | 1021 | `	const char *z = pClass->zString;` |
|    657 | 1022 | `	sxu32 n = pClass->nByte;` |
|    657 | 1023 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|     88 | 1024 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|      - | 1025 | `	}` |
|    573 | 1026 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|     15 | 1027 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|      - | 1028 | `	}` |
|    559 | 1029 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|      3 | 1030 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|      - | 1031 | `	}` |
|    557 | 1032 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|      - | 1033 | `		/* iterable === array \| Traversable */` |
|     17 | 1034 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      7 | 1035 | `			return 1;` |
|      - | 1036 | `		}` |
|     11 | 1037 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|      5 | 1038 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      5 | 1039 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|      5 | 1040 | `				return 1;` |
|      - | 1041 | `			}` |
|    ! 0 | 1042 | `		}` |
|      7 | 1043 | `		return 0;` |
|      - | 1044 | `	}` |
|    541 | 1045 | `	return -1;` |
|    331 | 1046 | `}` |
|      - | 1047 | `/*` |
|      - | 1048 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|      - | 1049 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|      - | 1050 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|      - | 1051 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|      - | 1052 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|      - | 1053 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|      - | 1054 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|      - | 1055 | ` * throw.` |
|      - | 1056 | ` *` |
|      - | 1057 | ` * The class match for object values consults the active VM self-stack to` |
|      - | 1058 | `` * resolve `self`/`parent` aliases when present.`` |
|      - | 1059 | ` */` |
|      - | 1060 | `/*` |
|      - | 1061 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|      - | 1062 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf`` |
|      - | 1063 | ` * (the active self for params/returns/properties, or the declaring class for a` |
|      - | 1064 | ` * class constant). Used by every type-enforcement site so the resolution rule —` |
|      - | 1065 | ` * including the iLoadable flag — lives in one place.` |
|      - | 1066 | ` *` |
|      - | 1067 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|      - | 1068 | ` * compatibility target, where the type may legitimately be an interface or` |
|      - | 1069 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|      - | 1070 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|      - | 1071 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|      - | 1072 | ` * with TRUE; it does not go through this helper.)` |
|      - | 1073 | ` */` |
|    584 | 1074 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|      5 | 1075 | `{` |
|    589 | 1076 | `	if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|     30 | 1077 | `		return pSelf;` |
|      - | 1078 | `	}` |
|    561 | 1079 | `	if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      - | 1080 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|      - | 1081 | `		 * resolves against the runtime using class, matching the self:: trait rule. */` |
|      7 | 1082 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|    ! 0 | 1083 | `			pSelf = PH7_VmPeekTopClass(pVm);` |
|    ! 0 | 1084 | `		}` |
|      7 | 1085 | `		return pSelf ? pSelf->pBase : 0;` |
|      - | 1086 | `	}` |
|    555 | 1087 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|    297 | 1088 | `}` |
|    176 | 1089 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|      5 | 1090 | `{` |
|      - | 1091 | `	sxu32 i;` |
|      - | 1092 | `	sxu32 nAlts;` |
|      - | 1093 | `	ph7_type_alt *aAlts;` |
|      - | 1094 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|      - | 1095 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|    181 | 1096 | `	int bHasIntersection = 0;` |
|      - | 1097 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    181 | 1098 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|     19 | 1099 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|      - | 1100 | `	}` |
|    165 | 1101 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|    165 | 1102 | `	nAlts = SySetUsed(pAlts);` |
|      - | 1103 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|      - | 1104 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|      - | 1105 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|      - | 1106 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|      - | 1107 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   5285 | 1108 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    493 | 1109 | `	for( i = 0; i < nAlts; i++ ){` |
|    333 | 1110 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|     36 | 1111 | `			bHasIntersection = 1;` |
|     16 | 1112 | `		}` |
|    169 | 1113 | `	}` |
|      - | 1114 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|      - | 1115 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|      - | 1116 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|      - | 1117 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|    165 | 1118 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|     36 | 1119 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     36 | 1120 | `		ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|      - | 1121 | `		sxu32 g;` |
|    422 | 1122 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|      - | 1123 | `			int bAll;` |
|    410 | 1124 | `			if( aGroupCount[g] < 2 ) continue;` |
|     36 | 1125 | `			bAll = 1;` |
|     88 | 1126 | `			for( i = 0; i < nAlts; i++ ){` |
|      - | 1127 | `				ph7_class *pExpected;` |
|     68 | 1128 | `				if( aAlts[i].nGroup != g ) continue;` |
|     64 | 1129 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|     64 | 1130 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);` |
|     64 | 1131 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|     16 | 1132 | `					bAll = 0;` |
|     16 | 1133 | `					break;` |
|      - | 1134 | `				}` |
|     28 | 1135 | `			}` |
|     36 | 1136 | `			if( bAll ) return SXRET_OK;` |
|     10 | 1137 | `		}` |
|      6 | 1138 | `	}` |
|      - | 1139 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|      - | 1140 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|      - | 1141 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|      - | 1142 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|    425 | 1143 | `	for( i = 0; i < nAlts; i++ ){` |
|    287 | 1144 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|    258 | 1145 | `		if( aAlts[i].nType == SXU32_HIGH` |
|    158 | 1146 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|      3 | 1147 | `			return SXRET_OK;` |
|      - | 1148 | `		}` |
|    133 | 1149 | `	}` |
|    143 | 1150 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|    143 | 1151 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|    423 | 1152 | `	for( i = 0; i < nAlts; i++ ){` |
|    285 | 1153 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|    261 | 1154 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|    215 | 1155 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|    215 | 1156 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|    215 | 1157 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|    109 | 1158 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     81 | 1159 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|    ! 0 | 1160 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|    133 | 1161 | `	}` |
|      - | 1162 | `	/* Object handling */` |
|    143 | 1163 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     39 | 1164 | `		if( bHasObjAlt ) return SXRET_OK;` |
|     39 | 1165 | `		if( bHasClassAlt ){` |
|     26 | 1166 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     26 | 1167 | `			ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|     50 | 1168 | `			for( i = 0; i < nAlts; i++ ){` |
|      - | 1169 | `				ph7_class *pExpected;` |
|     44 | 1170 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     36 | 1171 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|     36 | 1172 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);` |
|     36 | 1173 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|     19 | 1174 | `					return SXRET_OK;` |
|      - | 1175 | `				}` |
|     12 | 1176 | `			}` |
|      3 | 1177 | `		}` |
|     22 | 1178 | `		return SXERR_INVALID;` |
|      - | 1179 | `	}` |
|      - | 1180 | `	/* Array handling */` |
|    109 | 1181 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     11 | 1182 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|      - | 1183 | `	}` |
|      - | 1184 | `	/* Scalar handling — exact match first */` |
|    100 | 1185 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|     46 | 1186 | `		if( bHasInt ) return SXRET_OK;` |
|      1 | 1187 | `	}` |
|     60 | 1188 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      5 | 1189 | `		if( bHasFloat ) return SXRET_OK;` |
|    ! 0 | 1190 | `	}` |
|     56 | 1191 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     54 | 1192 | `		if( bHasString ) return SXRET_OK;` |
|      8 | 1193 | `	}` |
|     20 | 1194 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 1195 | `		if( bHasBool ) return SXRET_OK;` |
|    ! 0 | 1196 | `	}` |
|     20 | 1197 | `	if( bStrict ){` |
|      - | 1198 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|    ! 0 | 1199 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|    ! 0 | 1200 | `			PH7_MemObjToReal(pValue);` |
|    ! 0 | 1201 | `			return SXRET_OK;` |
|      - | 1202 | `		}` |
|    ! 0 | 1203 | `		return SXERR_INVALID;` |
|      - | 1204 | `	}` |
|      - | 1205 | `	/* Weak coercion preference order: int > float > string > bool.` |
|      - | 1206 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|      - | 1207 | `	 * to match PHP's union RFC. */` |
|      - | 1208 | `	{` |
|     20 | 1209 | `		int kind = VmStringNumericKind(pValue);` |
|     20 | 1210 | `		if( bHasInt ){` |
|      - | 1211 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|      - | 1212 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|     18 | 1213 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 1214 | `				PH7_MemObjToInteger(pValue);` |
|    ! 0 | 1215 | `				return SXRET_OK;` |
|      - | 1216 | `			}` |
|     18 | 1217 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 1218 | `				ph7_real r = pValue->rVal;` |
|    ! 0 | 1219 | `				if( r == (ph7_real)(sxi64)r ){` |
|    ! 0 | 1220 | `					PH7_MemObjToInteger(pValue);` |
|    ! 0 | 1221 | `					return SXRET_OK;` |
|      - | 1222 | `				}` |
|    ! 0 | 1223 | `			}` |
|     18 | 1224 | `			if( kind == 1 ){` |
|      9 | 1225 | `				PH7_MemObjToInteger(pValue);` |
|      9 | 1226 | `				return SXRET_OK;` |
|      - | 1227 | `			}` |
|      4 | 1228 | `		}` |
|     12 | 1229 | `		if( bHasFloat ){` |
|     10 | 1230 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|    ! 0 | 1231 | `				PH7_MemObjToReal(pValue);` |
|    ! 0 | 1232 | `				return SXRET_OK;` |
|      - | 1233 | `			}` |
|     10 | 1234 | `			if( kind == 1 \|\| kind == 2 ){` |
|      7 | 1235 | `				PH7_MemObjToReal(pValue);` |
|      7 | 1236 | `				return SXRET_OK;` |
|      - | 1237 | `			}` |
|      1 | 1238 | `		}` |
|      5 | 1239 | `		if( bHasString ){` |
|    ! 0 | 1240 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    ! 0 | 1241 | `				PH7_MemObjToString(pValue);` |
|    ! 0 | 1242 | `				return SXRET_OK;` |
|      - | 1243 | `			}` |
|    ! 0 | 1244 | `		}` |
|      5 | 1245 | `		if( bHasBool ){` |
|    ! 0 | 1246 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|    ! 0 | 1247 | `				PH7_MemObjToBool(pValue);` |
|    ! 0 | 1248 | `				return SXRET_OK;` |
|      - | 1249 | `			}` |
|    ! 0 | 1250 | `		}` |
|      - | 1251 | `	}` |
|      5 | 1252 | `	return SXERR_INVALID;` |
|     93 | 1253 | `}` |
|      - | 1254 |  |
|      - | 1255 | `/*` |
|      - | 1256 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|      - | 1257 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|      - | 1258 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|      - | 1259 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|      - | 1260 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|      - | 1261 | ` */` |
|    100 | 1262 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|      3 | 1263 | `{` |
|      - | 1264 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|      - | 1265 | `	 * null value satisfies it (and a null value matches via the flag test` |
|      - | 1266 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|      - | 1267 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|      - | 1268 | `	 * silently swallow any argument. */` |
|    103 | 1269 | `	if( nType == MEMOBJ_NULL ){` |
|      3 | 1270 | `		return SXERR_INVALID;` |
|      - | 1271 | `	}` |
|    101 | 1272 | `	if( bStrict ){` |
|      - | 1273 | `		/* Only int -> float widening is allowed implicitly. */` |
|     15 | 1274 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|      3 | 1275 | `			PH7_MemObjToReal(pVal);` |
|      3 | 1276 | `			return SXRET_OK;` |
|      - | 1277 | `		}` |
|     13 | 1278 | `		return SXERR_INVALID;` |
|      - | 1279 | `	}` |
|      - | 1280 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|      - | 1281 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|      - | 1282 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|      - | 1283 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|      - | 1284 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|      - | 1285 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|      - | 1286 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     89 | 1287 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     20 | 1288 | `		return SXERR_INVALID;` |
|      - | 1289 | `	}` |
|     68 | 1290 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|     62 | 1291 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|     63 | 1292 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|     18 | 1293 | `		return SXERR_INVALID;` |
|      - | 1294 | `	}` |
|     55 | 1295 | `	if( nType == MEMOBJ_INT && pVal->pVm ){` |
|      - | 1296 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion` |
|      - | 1297 | `		 * (typed params, returns, typed property stores all funnel through here);` |
|      - | 1298 | `		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly` |
|      - | 1299 | `		 * like the null / non-numeric-string cases above. An INTEGRAL float loses` |
|      - | 1300 | `		 * nothing and coerces normally. */` |
|     35 | 1301 | `		if( pVal->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 1302 | `			ph7_real r = pVal->rVal;` |
|    ! 0 | 1303 | `			if( r != (ph7_real)(sxi64)r ){` |
|    ! 0 | 1304 | `				return SXERR_INVALID;` |
|    ! 0 | 1305 | `			}` |
|     35 | 1306 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      - | 1307 | `			SyString sStr;` |
|      - | 1308 | `			ph7_value sProbe;` |
|      - | 1309 | `			int bLossy;` |
|     31 | 1310 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     31 | 1311 | `			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|     31 | 1312 | `			PH7_MemObjToNumeric(&sProbe);` |
|     31 | 1313 | `			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;` |
|     31 | 1314 | `			PH7_MemObjRelease(&sProbe);` |
|     31 | 1315 | `			if( bLossy ){` |
|    ! 0 | 1316 | `				return SXERR_INVALID;` |
|      - | 1317 | `			}` |
|     14 | 1318 | `		}` |
|     16 | 1319 | `	}` |
|      - | 1320 | `	{` |
|     55 | 1321 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|     55 | 1322 | `		if( xCast ) xCast(pVal);` |
|      - | 1323 | `	}` |
|     55 | 1324 | `	return SXRET_OK;` |
|     53 | 1325 | `}` |
|      - | 1326 |  |
|      - | 1327 | `/*` |
|      - | 1328 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|      - | 1329 | ` * TypeError message. Prefers the declared textual form when available.` |
|      - | 1330 | ` *` |
|      - | 1331 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|      - | 1332 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|      - | 1333 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|      - | 1334 | ` * back to a static literal and ignore zBuf entirely.` |
|      - | 1335 | ` */` |
|     48 | 1336 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|      4 | 1337 | `{` |
|     52 | 1338 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     52 | 1339 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     52 | 1340 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     52 | 1341 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     52 | 1342 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|     24 | 1343 | `		}` |
|     52 | 1344 | `		zBuf[nCopy] = 0;` |
|     52 | 1345 | `		return zBuf;` |
|      - | 1346 | `	}` |
|    ! 0 | 1347 | `	switch( nType ){` |
|    ! 0 | 1348 | `		case MEMOBJ_INT:     return "int";` |
|    ! 0 | 1349 | `		case MEMOBJ_REAL:    return "float";` |
|    ! 0 | 1350 | `		case MEMOBJ_STRING:  return "string";` |
|    ! 0 | 1351 | `		case MEMOBJ_BOOL:    return "bool";` |
|    ! 0 | 1352 | `		case MEMOBJ_HASHMAP: return "array";` |
|    ! 0 | 1353 | `		case MEMOBJ_OBJ:     return "object";` |
|    ! 0 | 1354 | `		default:             return "scalar";` |
|      - | 1355 | `	}` |
|     28 | 1356 | `}` |
|      - | 1357 |  |
|      - | 1358 | `/*` |
|      - | 1359 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|      - | 1360 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|      - | 1361 | ` */` |
|     34 | 1362 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|      5 | 1363 | `{` |
|     39 | 1364 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     56 | 1365 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|     34 | 1366 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|     39 | 1367 | `	return zBuf;` |
|      5 | 1368 | `}` |
|      - | 1369 |  |
|  13782 | 1370 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|      5 | 1371 | `{` |
|      - | 1372 | `	SyHashEntry *pSlot;` |
|      - | 1373 | `	VmClassAttr *pVmAttr;` |
|      - | 1374 | `	ph7_class_attr *pAttr;` |
|  13787 | 1375 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  13787 | 1376 | `	if( pSlot == 0 ){` |
|  13299 | 1377 | `		return SXRET_OK; /* Not a typed slot */` |
|      - | 1378 | `	}` |
|    493 | 1379 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|    493 | 1380 | `	pAttr = pVmAttr->pAttr;` |
|    493 | 1381 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|    ! 0 | 1382 | `		return SXRET_OK;` |
|      - | 1383 | `	}` |
|      - | 1384 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|      - | 1385 | `	 * property may be written exactly once and only from within the declaring` |
|      - | 1386 | `	 * class scope (its set-scope is protected). */` |
|    493 | 1387 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      - | 1388 | `		/* A readonly property is always typed and default-less, so it starts` |
|      - | 1389 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|      - | 1390 | `		 * write below — making it the write-once latch (a type-rejected write` |
|      - | 1391 | `		 * leaves it set, so a later valid initialization still works). */` |
|     79 | 1392 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|      - | 1393 | `			/* Already initialized: any further write is forbidden, any scope —` |
|      - | 1394 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|      - | 1395 | `			 * Exceptions that fall through to the set-scope check below:` |
|      - | 1396 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|      - | 1397 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|      - | 1398 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|     22 | 1399 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     22 | 1400 | `			if( !(pCloneFr && pCloneFr->pThis` |
|     12 | 1401 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|     20 | 1402 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|      - | 1403 | `			}` |
|      1 | 1404 | `		}` |
|     29 | 1405 | `	}` |
|    477 | 1406 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|      - | 1407 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|      - | 1408 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|     27 | 1409 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|     27 | 1410 | `		if( rcVis != SXRET_OK ){` |
|     13 | 1411 | `			return rcVis;` |
|      1 | 1412 | `		}` |
|    458 | 1413 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      - | 1414 | `		/* First write (or a clone re-init) must come from within the declaring` |
|      - | 1415 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|     61 | 1416 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|     61 | 1417 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|      - | 1418 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|      - | 1419 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|      - | 1420 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|     61 | 1421 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|      5 | 1422 | `			pDecl = pVmAttr->pOwner;` |
|      2 | 1423 | `		}` |
|     61 | 1424 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|      6 | 1425 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|      - | 1426 | `		}` |
|     26 | 1427 | `	}` |
|      - | 1428 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|      - | 1429 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|      - | 1430 | `	 * matching PHP's documented behavior. */` |
|    461 | 1431 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|     68 | 1432 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|     42 | 1433 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|      - | 1434 | `			0 /* bStrict: properties never apply strict_types */);` |
|     47 | 1435 | `		if( rc == SXRET_OK ){` |
|     33 | 1436 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     33 | 1437 | `			return SXRET_OK;` |
|      - | 1438 | `		}` |
|     16 | 1439 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      - | 1440 | `			char zBuf[128];` |
|     11 | 1441 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      3 | 1442 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 1443 | `		}` |
|      9 | 1444 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1445 | `	}` |
|      - | 1446 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|      - | 1447 | `	 * includes null). */` |
|    419 | 1448 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|     16 | 1449 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|     14 | 1450 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      2 | 1451 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|     17 | 1452 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     17 | 1453 | `			return SXRET_OK;` |
|      - | 1454 | `		}` |
|      3 | 1455 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|      - | 1456 | `	}` |
|      - | 1457 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|      - | 1458 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|      - | 1459 | `	 * type error. */` |
|    403 | 1460 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|    ! 0 | 1461 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1462 | `	}` |
|      - | 1463 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|      - | 1464 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|      - | 1465 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|    403 | 1466 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     12 | 1467 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      5 | 1468 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      5 | 1469 | `			return SXRET_OK;` |
|      - | 1470 | `		}` |
|      7 | 1471 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1472 | `	}` |
|      - | 1473 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|      - | 1474 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|      - | 1475 | `	 * handled by the nullable check above). Checked by value before the generic` |
|      - | 1476 | `	 * class-instanceof branch, which would resolve no such class and then` |
|      - | 1477 | `	 * wrongly accept any object / reject arrays. */` |
|    393 | 1478 | `	if( pAttr->nType == SXU32_HIGH ){` |
|     55 | 1479 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|     55 | 1480 | `		if( rcPseudo == 1 ){` |
|     11 | 1481 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     11 | 1482 | `			return SXRET_OK;` |
|      - | 1483 | `		}` |
|     45 | 1484 | `		if( rcPseudo == 0 ){` |
|      3 | 1485 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1486 | `		}` |
|      - | 1487 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|     20 | 1488 | `	}` |
|    381 | 1489 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      - | 1490 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|      - | 1491 | `		 * currently active on the self-stack. */` |
|     43 | 1492 | `		ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,VmCurrentSelf(pVm));` |
|     43 | 1493 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1494 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1495 | `		}` |
|     43 | 1496 | `		if( pExpected ){` |
|     39 | 1497 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     39 | 1498 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      - | 1499 | `				char zBuf[128];` |
|     15 | 1500 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      4 | 1501 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 1502 | `			}` |
|     14 | 1503 | `		}` |
|     35 | 1504 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     35 | 1505 | `		return SXRET_OK;` |
|      - | 1506 | `	}` |
|      - | 1507 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|      - | 1508 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|    341 | 1509 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      - | 1510 | `		char zBuf[128];` |
|     11 | 1511 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      3 | 1512 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 1513 | `	}` |
|    335 | 1514 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|     35 | 1515 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|     35 | 1516 | `		if( xCast ){` |
|      - | 1517 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|     35 | 1518 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      3 | 1519 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1520 | `			}` |
|     33 | 1521 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|      6 | 1522 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|      - | 1523 | `			}` |
|      - | 1524 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|      - | 1525 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|      - | 1526 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|     24 | 1527 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|     23 | 1528 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|     26 | 1529 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|     15 | 1530 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|      - | 1531 | `			}` |
|     14 | 1532 | `			xCast(pValue);` |
|      6 | 1533 | `		}` |
|      6 | 1534 | `	}` |
|    317 | 1535 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|    317 | 1536 | `	return SXRET_OK;` |
|   6896 | 1537 | `}` |
|      - | 1538 | `/*` |
|      - | 1539 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|      - | 1540 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|      - | 1541 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|      - | 1542 | ` *     a scope that could normally reach it — else a catchable Error),` |
|      - | 1543 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|      - | 1544 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|      - | 1545 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|      - | 1546 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|      - | 1547 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|      - | 1548 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|      - | 1549 | ` */` |
|     18 | 1550 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|      - | 1551 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|      1 | 1552 | `{` |
|     19 | 1553 | `	ph7_class *pClass = pClone->pClass;` |
|      - | 1554 | `	SyHashEntry *pEntry;` |
|      - | 1555 | `	VmClassAttr *pVmAttr;` |
|      - | 1556 | `	ph7_class_attr *pAttr;` |
|      - | 1557 | `	ph7_value *pSlot;` |
|      - | 1558 | `	sxi32 rc;` |
|     19 | 1559 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|     19 | 1560 | `	if( pEntry == 0 ){` |
|      - | 1561 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|      - | 1562 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|      - | 1563 | `		 * deferred residual). */` |
|    ! 0 | 1564 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|    ! 0 | 1565 | `		if( pSlot == 0 ){` |
|    ! 0 | 1566 | `			return PH7_VmMemoryError(pVm);` |
|      - | 1567 | `		}` |
|    ! 0 | 1568 | `		PH7_MemObjStore(pValue,pSlot);` |
|    ! 0 | 1569 | `		return SXRET_OK;` |
|      - | 1570 | `	}` |
|     19 | 1571 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     19 | 1572 | `	pAttr = pVmAttr->pAttr;` |
|      - | 1573 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|     19 | 1574 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|      - | 1575 | `		SyBlob sMsg;` |
|    ! 0 | 1576 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    ! 0 | 1577 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|    ! 0 | 1578 | `			&pClass->sName,&pAttr->sName);` |
|    ! 0 | 1579 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|      - | 1582 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|      - | 1583 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|     19 | 1584 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      3 | 1585 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|      3 | 1586 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      - | 1587 | `		SyBlob sMsg;` |
|      3 | 1588 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1589 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|      3 | 1590 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      - | 1591 | `	}` |
|      - | 1592 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|     17 | 1593 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|     17 | 1594 | `	if( rc != SXRET_OK ){` |
|      3 | 1595 | `		return rc;` |
|      - | 1596 | `	}` |
|      - | 1597 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|     15 | 1598 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     15 | 1599 | `	if( pSlot ){` |
|     15 | 1600 | `		PH7_MemObjStore(pValue,pSlot);` |
|      7 | 1601 | `	}` |
|     15 | 1602 | `	return SXRET_OK;` |
|     10 | 1603 | `}` |
|      - | 1604 | `/*` |
|      - | 1605 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|      - | 1606 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|      - | 1607 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|      - | 1608 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|      - | 1609 | ` */` |
|      4 | 1610 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|      2 | 1611 | `{` |
|      6 | 1612 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      - | 1613 | `	char zBuf[128];` |
|      - | 1614 | `	const char *zGiven;` |
|      6 | 1615 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 1616 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|    ! 0 | 1617 | `	}else{` |
|      6 | 1618 | `		zGiven = ph7_type_name(pValue);` |
|      - | 1619 | `	}` |
|      - | 1620 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|      - | 1621 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|      - | 1622 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|      - | 1623 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|      - | 1624 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|      - | 1625 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|      - | 1626 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|      6 | 1627 | `	if( pVm->sCodeGen.xErr ){` |
|      4 | 1628 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|      - | 1629 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|      1 | 1630 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|      2 | 1631 | `	}else{` |
|      4 | 1632 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - | 1633 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|      1 | 1634 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|      - | 1635 | `	}` |
|      6 | 1636 | `	pVm->iExitStatus = 255;` |
|      6 | 1637 | `	pVm->bHaltRequested = 1;` |
|      6 | 1638 | `	return SXERR_ABORT;` |
|      2 | 1639 | `}` |
|      - | 1640 | `/*` |
|      - | 1641 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|      - | 1642 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|      - | 1643 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|      - | 1644 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|      - | 1645 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|      - | 1646 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|      - | 1647 | ` */` |
|     32 | 1648 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|      3 | 1649 | `{` |
|     35 | 1650 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|      - | 1651 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|     35 | 1652 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      3 | 1653 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      3 | 1654 | `			return SXRET_OK;` |
|      - | 1655 | `		}` |
|    ! 0 | 1656 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|    ! 0 | 1657 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|    ! 0 | 1658 | `			return SXRET_OK;` |
|      - | 1659 | `		}` |
|    ! 0 | 1660 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1661 | `	}` |
|      - | 1662 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|     33 | 1663 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      5 | 1664 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){` |
|      5 | 1665 | `			return SXRET_OK;` |
|      - | 1666 | `		}` |
|    ! 0 | 1667 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1668 | `	}` |
|      - | 1669 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|     29 | 1670 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|    ! 0 | 1671 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1672 | `	}` |
|      - | 1673 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|     29 | 1674 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|    ! 0 | 1675 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 1676 | `			return SXRET_OK;` |
|      - | 1677 | `		}` |
|    ! 0 | 1678 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1679 | `	}` |
|      - | 1680 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|      - | 1681 | `	 * a real class/interface verified by instanceof. */` |
|     29 | 1682 | `	if( pAttr->nType == SXU32_HIGH ){` |
|    ! 0 | 1683 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|    ! 0 | 1684 | `		if( rcPseudo == 1 ){` |
|    ! 0 | 1685 | `			return SXRET_OK;` |
|      - | 1686 | `		}` |
|    ! 0 | 1687 | `		if( rcPseudo == 0 ){` |
|    ! 0 | 1688 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1689 | `		}` |
|      - | 1690 | `		/* rcPseudo == -1: a real class/interface type. */` |
|    ! 0 | 1691 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1692 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1693 | `		}` |
|      - | 1694 | `		{` |
|      - | 1695 | `			/* A class constant's self/parent resolve against the declaring class. */` |
|    ! 0 | 1696 | `			ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,pClass);` |
|    ! 0 | 1697 | `			if( pExpected ){` |
|    ! 0 | 1698 | `				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|    ! 0 | 1699 | `				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|    ! 0 | 1700 | `					return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      - | 1701 | `				}` |
|    ! 0 | 1702 | `			}` |
|      - | 1703 | `		}` |
|    ! 0 | 1704 | `		return SXRET_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|      - | 1707 | `	 * implicit widening. Everything else is a type error.` |
|      - | 1708 | `	 *` |
|      - | 1709 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|      - | 1710 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|      - | 1711 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|      - | 1712 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|      - | 1713 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|      - | 1714 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|      - | 1715 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|      - | 1716 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|      - | 1717 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|      - | 1718 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|      - | 1719 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|     29 | 1720 | `	if( pValue->iFlags & pAttr->nType ){` |
|     21 | 1721 | `		return SXRET_OK;` |
|      - | 1722 | `	}` |
|      9 | 1723 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|      3 | 1724 | `		PH7_MemObjToReal(pValue);` |
|      3 | 1725 | `		return SXRET_OK;` |
|      - | 1726 | `	}` |
|      6 | 1727 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|     19 | 1728 | `}` |
|      - | 1729 |  |
|      - | 1730 | `/*` |
|      - | 1731 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - | 1732 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|      - | 1733 | ` * information.` |
|      - | 1734 | ` * ------------------------------------` |
|      - | 1735 | ` * Simple boring wrapper function.` |
|      - | 1736 | ` * ------------------------------------` |
|      - | 1737 | ` */` |
|     48 | 1738 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|      4 | 1739 | `{` |
|      - | 1740 | `	va_list ap;` |
|      - | 1741 | `	sxi32 rc;` |
|     52 | 1742 | `	va_start(ap,zFormat);` |
|     52 | 1743 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     52 | 1744 | `	va_end(ap);` |
|     52 | 1745 | `	return rc;` |
|      4 | 1746 | `}` |
|      - | 1747 | `/*` |
|      - | 1748 | ` * Throw a TypeError exception from within the VM execution loop.` |
|      - | 1749 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|      - | 1750 | ` */` |
|    114 | 1751 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|      5 | 1752 | `{` |
|      - | 1753 | `	ph7_class *pClass;` |
|      - | 1754 | `	ph7_class_instance *pThis;` |
|      - | 1755 | `	ph7_class_method *pCons;` |
|      - | 1756 | `	ph7_value sArg;` |
|      - | 1757 | `	ph7_value *apArg[1];` |
|      - | 1758 | `	SyBlob sMsg;` |
|      - | 1759 | `	SyString sMsgStr;` |
|      - | 1760 | `	VmFrame *pFrame;` |
|      - | 1761 | `	sxi32 rc;` |
|    119 | 1762 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|    119 | 1763 | `	if( pClass == 0 ){` |
|    ! 0 | 1764 | `		return PH7_ABORT;` |
|      - | 1765 | `	}` |
|    119 | 1766 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    119 | 1767 | `	if( pThis == 0 ){` |
|    ! 0 | 1768 | `		return PH7_ABORT;` |
|      - | 1769 | `	}` |
|    119 | 1770 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      - | 1771 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|      - | 1772 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|    119 | 1773 | `	if( pOwnerClass ){` |
|     20 | 1774 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|      9 | 1775 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|     11 | 1776 | `	}else{` |
|    101 | 1777 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|     48 | 1778 | `			pFuncName,nArg,pArgName,zExpected,zGiven);` |
|      - | 1779 | `	}` |
|    119 | 1780 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    119 | 1781 | `	if( pCons ){` |
|    119 | 1782 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    119 | 1783 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    119 | 1784 | `		apArg[0] = &sArg;` |
|    119 | 1785 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|    119 | 1786 | `		PH7_MemObjRelease(&sArg);` |
|     57 | 1787 | `	}` |
|    119 | 1788 | `	SyBlobRelease(&sMsg);` |
|    119 | 1789 | `	pFrame = pVm->pFrame;` |
|    119 | 1790 | `	if( pFrame ){` |
|    119 | 1791 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    119 | 1792 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     57 | 1793 | `	}` |
|    119 | 1794 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    119 | 1795 | `	PH7_ClassInstanceUnref(pThis);` |
|    119 | 1796 | `	if( rc == SXERR_ABORT ){` |
|      6 | 1797 | `		return PH7_ABORT;` |
|      - | 1798 | `	}` |
|    115 | 1799 | `	return PH7_EXCEPTION;` |
|     62 | 1800 | `}` |
|      - | 1801 | `/*` |
|      - | 1802 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|      - | 1803 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|      - | 1804 | ` * before a required parameter as implicitly required), excluding a trailing` |
|      - | 1805 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|      - | 1806 | ` * pick php's wording — "exactly N expected" when required == total,` |
|      - | 1807 | ` * "at least N" when trailing optionals exist.` |
|      - | 1808 | ` */` |
|   4688 | 1809 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|      5 | 1810 | `{` |
|   4693 | 1811 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|   4693 | 1812 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|   4693 | 1813 | `	sxu32 nRequired = 0;` |
|      - | 1814 | `	sxu32 n;` |
|   4693 | 1815 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|    297 | 1816 | `		nFormal--;` |
|    146 | 1817 | `	}` |
|  15359 | 1818 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|  10671 | 1819 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    823 | 1820 | `			nRequired = n + 1;` |
|    409 | 1821 | `		}` |
|   5338 | 1822 | `	}` |
|   4693 | 1823 | `	*pnNonVariadic = nFormal;` |
|   4693 | 1824 | `	return nRequired;` |
|      5 | 1825 | `}` |
|      - | 1826 | `/*` |
|      - | 1827 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|      - | 1828 | ` * with too few arguments:` |
|      - | 1829 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|      - | 1830 | ` *   {exactly\|at least} M expected` |
|      - | 1831 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|      - | 1832 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|      - | 1833 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|      - | 1834 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|      - | 1835 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|      - | 1836 | ` */` |
|     26 | 1837 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1838 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|      3 | 1839 | `{` |
|      - | 1840 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|      - | 1841 | `	SyBlob sMsg;` |
|     29 | 1842 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     29 | 1843 | `	if( pOwnerClass ){` |
|      5 | 1844 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|      2 | 1845 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|      3 | 1846 | `	}else{` |
|     25 | 1847 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|      - | 1848 | `	}` |
|     29 | 1849 | `	if( bCallSite ){` |
|     27 | 1850 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|     27 | 1851 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|     12 | 1852 | `	}` |
|     29 | 1853 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|     13 | 1854 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|      - | 1855 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     29 | 1856 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      3 | 1857 | `}` |
|      - | 1858 | `/*` |
|      - | 1859 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|      - | 1860 | ` * called with too few arguments, in php's ZPP wording:` |
|      - | 1861 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|      - | 1862 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|      - | 1863 | ` * pluralized on the expected count). Hosted builtin FUNCTIONS are not routed` |
|      - | 1864 | ` * here: their PHL signatures don't always mirror php's true arity (e.g.` |
|      - | 1865 | ` * array_unshift is (&$pArray)+func_get_args for php's (array, ...$values)),` |
|      - | 1866 | ` * so their in-body self-checks own the message.` |
|      - | 1867 | ` */` |
|      2 | 1868 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1869 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic)` |
|      1 | 1870 | `{` |
|      - | 1871 | `	SyBlob sMsg;` |
|      3 | 1872 | `	const char *zKind = (nRequired >= nNonVariadic) ? "exactly" : "at least";` |
|      3 | 1873 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|      3 | 1874 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1875 | `	if( pOwnerClass ){` |
|      3 | 1876 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      1 | 1877 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      2 | 1878 | `	}else{` |
|    ! 0 | 1879 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|    ! 0 | 1880 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      - | 1881 | `	}` |
|      - | 1882 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      3 | 1883 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      1 | 1884 | `}` |
|      - | 1885 | `/*` |
|      - | 1886 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|      - | 1887 | ` * named or positional argument resolved to:` |
|      - | 1888 | ` *   C::f(): Argument #N ($x) not passed` |
|      - | 1889 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|      - | 1890 | ` */` |
|      2 | 1891 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1892 | `	sxu32 nArg,SyString *pArgName)` |
|      1 | 1893 | `{` |
|      - | 1894 | `	SyBlob sMsg;` |
|      3 | 1895 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1896 | `	if( pOwnerClass ){` |
|    ! 0 | 1897 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|    ! 0 | 1898 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|    ! 0 | 1899 | `	}else{` |
|      3 | 1900 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|      - | 1901 | `	}` |
|      - | 1902 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      3 | 1903 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      1 | 1904 | `}` |
|      - | 1905 | `/*` |
|      - | 1906 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|      - | 1907 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|      - | 1908 | ` */` |
|      - | 1909 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|      - | 1910 | ` * The message is copied into the instance by __construct, so the caller owns` |
|      - | 1911 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|     24 | 1912 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|      5 | 1913 | `{` |
|      - | 1914 | `	ph7_class *pClass;` |
|      - | 1915 | `	ph7_class_instance *pThis;` |
|      - | 1916 | `	ph7_class_method *pCons;` |
|      - | 1917 | `	ph7_value sArg;` |
|      - | 1918 | `	ph7_value *apArg[1];` |
|      - | 1919 | `	SyString sMsgStr;` |
|      - | 1920 | `	VmFrame *pFrame;` |
|      - | 1921 | `	sxi32 rc;` |
|     29 | 1922 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     29 | 1923 | `	if( pClass == 0 ){` |
|    ! 0 | 1924 | `		return PH7_ABORT;` |
|      - | 1925 | `	}` |
|     29 | 1926 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     29 | 1927 | `	if( pThis == 0 ){` |
|    ! 0 | 1928 | `		return PH7_ABORT;` |
|      - | 1929 | `	}` |
|     29 | 1930 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     29 | 1931 | `	if( pCons ){` |
|     29 | 1932 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|     29 | 1933 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     29 | 1934 | `		apArg[0] = &sArg;` |
|     29 | 1935 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     29 | 1936 | `		PH7_MemObjRelease(&sArg);` |
|     12 | 1937 | `	}` |
|     29 | 1938 | `	pFrame = pVm->pFrame;` |
|     29 | 1939 | `	if( pFrame ){` |
|     29 | 1940 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     29 | 1941 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     12 | 1942 | `	}` |
|     29 | 1943 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     29 | 1944 | `	PH7_ClassInstanceUnref(pThis);` |
|     29 | 1945 | `	if( rc == SXERR_ABORT ){` |
|      9 | 1946 | `		return PH7_ABORT;` |
|      - | 1947 | `	}` |
|     21 | 1948 | `	return PH7_EXCEPTION;` |
|     17 | 1949 | `}` |
|     22 | 1950 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|      5 | 1951 | `{` |
|      - | 1952 | `	SyBlob sMsg;` |
|      - | 1953 | `	sxi32 rc;` |
|     27 | 1954 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     27 | 1955 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|     11 | 1956 | `		pFuncName,zExpected,zGiven);` |
|     27 | 1957 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|     27 | 1958 | `	SyBlobRelease(&sMsg);` |
|     27 | 1959 | `	return rc;` |
|      5 | 1960 | `}` |
|      - | 1961 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|      - | 1962 | `` * explicit `return` at compile time, so this fires only for an implicit return. */`` |
|      2 | 1963 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,SyString *pFuncName)` |
|      1 | 1964 | `{` |
|      - | 1965 | `	SyBlob sMsg;` |
|      - | 1966 | `	sxi32 rc;` |
|      3 | 1967 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1968 | `	SyBlobFormat(&sMsg,"%z(): never-returning function must not implicitly return",` |
|      1 | 1969 | `		pFuncName);` |
|      3 | 1970 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      3 | 1971 | `	SyBlobRelease(&sMsg);` |
|      3 | 1972 | `	return rc;` |
|      1 | 1973 | `}` |
|      - | 1974 | `/*` |
|      - | 1975 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|      - | 1976 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|      - | 1977 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|      - | 1978 | ` */` |
|    124 | 1979 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|      4 | 1980 | `{` |
|    128 | 1981 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     24 | 1982 | `		return pVal->x.iVal ? "true" : "false";` |
|      - | 1983 | `	}` |
|    106 | 1984 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     22 | 1985 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     22 | 1986 | `		if( pThis && pThis->pClass ){` |
|     22 | 1987 | `			SyString *pName = &pThis->pClass->sName;` |
|     22 | 1988 | `			sxu32 n = pName->nByte;` |
|     22 | 1989 | `			if( n >= nBuf ){` |
|    ! 0 | 1990 | `				n = nBuf - 1;` |
|    ! 0 | 1991 | `			}` |
|     22 | 1992 | `			SyMemcpy(pName->zString,zBuf,n);` |
|     22 | 1993 | `			zBuf[n] = 0;` |
|     22 | 1994 | `			return zBuf;` |
|      - | 1995 | `		}` |
|    ! 0 | 1996 | `		return "object";` |
|      - | 1997 | `	}` |
|     88 | 1998 | `	return ph7_type_name(pVal);` |
|     66 | 1999 | `}` |
|      - | 2000 | `/*` |
|      - | 2001 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|      - | 2002 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|      - | 2003 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|      - | 2004 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|      - | 2005 | ` */` |
|     18 | 2006 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|      3 | 2007 | `{` |
|      - | 2008 | `	ph7_class *pClass;` |
|      - | 2009 | `	ph7_class_instance *pThis;` |
|      - | 2010 | `	ph7_class_method *pCons;` |
|      - | 2011 | `	ph7_value sArg;` |
|      - | 2012 | `	ph7_value *apArg[1];` |
|      - | 2013 | `	SyBlob sMsg;` |
|      - | 2014 | `	SyString sMsgStr;` |
|      - | 2015 | `	VmFrame *pFrame;` |
|      - | 2016 | `	sxi32 rc;` |
|     21 | 2017 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|      - | 2018 | `	char zNameBuf[64];` |
|     21 | 2019 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|     21 | 2020 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|     21 | 2021 | `	if( pClass == 0 ){` |
|    ! 0 | 2022 | `		return PH7_ABORT;` |
|      - | 2023 | `	}` |
|     21 | 2024 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     21 | 2025 | `	if( pThis == 0 ){` |
|    ! 0 | 2026 | `		return PH7_ABORT;` |
|      - | 2027 | `	}` |
|     21 | 2028 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     21 | 2029 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|     21 | 2030 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     21 | 2031 | `	if( pCons ){` |
|     21 | 2032 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     21 | 2033 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     21 | 2034 | `		apArg[0] = &sArg;` |
|     21 | 2035 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     21 | 2036 | `		PH7_MemObjRelease(&sArg);` |
|      9 | 2037 | `	}` |
|     21 | 2038 | `	SyBlobRelease(&sMsg);` |
|     21 | 2039 | `	pFrame = pVm->pFrame;` |
|     21 | 2040 | `	if( pFrame ){` |
|     21 | 2041 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     21 | 2042 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      9 | 2043 | `	}` |
|     21 | 2044 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     21 | 2045 | `	PH7_ClassInstanceUnref(pThis);` |
|     21 | 2046 | `	if( rc == SXERR_ABORT ){` |
|    ! 0 | 2047 | `		return PH7_ABORT;` |
|      - | 2048 | `	}` |
|     21 | 2049 | `	return PH7_EXCEPTION;` |
|     12 | 2050 | `}` |
|      - | 2051 | `/*` |
|      - | 2052 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|      - | 2053 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|      - | 2054 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|      - | 2055 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|      - | 2056 | ` */` |
|      - | 2057 | `/*` |
|      - | 2058 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|      - | 2059 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|      - | 2060 | ` * null SyString yields an empty C string. Returns zBuf.` |
|      - | 2061 | ` */` |
|    118 | 2062 | `PH7_PRIVATE const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|      5 | 2063 | `{` |
|      - | 2064 | `	sxu32 nCopy;` |
|    123 | 2065 | `	if( nBuf == 0 ) return "";` |
|    123 | 2066 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|    ! 0 | 2067 | `		zBuf[0] = 0;` |
|    ! 0 | 2068 | `		return zBuf;` |
|      - | 2069 | `	}` |
|    123 | 2070 | `	nCopy = SyStringLength(pStr);` |
|    123 | 2071 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|    123 | 2072 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|    123 | 2073 | `	zBuf[nCopy] = 0;` |
|    123 | 2074 | `	return zBuf;` |
|     64 | 2075 | `}` |
|      - | 2076 |  |
|      - | 2077 | `/*` |
|      - | 2078 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|      - | 2079 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|      - | 2080 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|      - | 2081 | ` * type field.` |
|      - | 2082 | ` */` |
| 107446 | 2083 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|      5 | 2084 | `{` |
| 107451 | 2085 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|      5 | 2086 | `}` |
|   9200 | 2087 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|      5 | 2088 | `{` |
|   9205 | 2089 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|   9205 | 2090 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|      - | 2091 | `	const char *zGiven;` |
|      - | 2092 | `	char zBuf[128];` |
|      - | 2093 | `	char zTypeBuf[128];` |
|      - | 2094 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|   9205 | 2095 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|    ! 0 | 2096 | `		return SXRET_OK;` |
|      - | 2097 | `	}` |
|      - | 2098 | `	/* never return type: the function must not return at all. An explicit` |
|      - | 2099 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|      - | 2100 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|      - | 2101 | `	 * the call site). */` |
|   9205 | 2102 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|      3 | 2103 | `		return VmThrowNeverReturnError(pVm,&pFunc->sName);` |
|      - | 2104 | `	}` |
|      - | 2105 | `	/* void return type: the function must not produce a value. */` |
|   9203 | 2106 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|    121 | 2107 | `		if( pValue == 0 ){` |
|    119 | 2108 | `			return SXRET_OK;` |
|      - | 2109 | `		}` |
|      - | 2110 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|      - | 2111 | `		 * still counts as "returned a value" here. */` |
|      3 | 2112 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      3 | 2113 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|      - | 2114 | `	}` |
|      - | 2115 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|      - | 2116 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|      - | 2117 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|   9087 | 2118 | `	if( pValue == 0 ){` |
|    ! 0 | 2119 | `		const char *zExpected = "value";` |
|    ! 0 | 2120 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|    ! 0 | 2121 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|    ! 0 | 2122 | `		}` |
|    ! 0 | 2123 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|      - | 2124 | `	}` |
|      - | 2125 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|      - | 2126 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|      - | 2127 | `	 * matching how every other typed return reports a missing value.) */` |
|   9087 | 2128 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|      5 | 2129 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|      3 | 2130 | `			return SXRET_OK;` |
|      - | 2131 | `		}` |
|      4 | 2132 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|      1 | 2133 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 2134 | `	}` |
|      - | 2135 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|      - | 2136 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|      - | 2137 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|   9083 | 2138 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|     18 | 2139 | `		return SXRET_OK;` |
|      - | 2140 | `	}` |
|      - | 2141 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|      - | 2142 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|      - | 2143 | `	 * Check by value before the real-class instanceof branch below. */` |
|   9069 | 2144 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|    141 | 2145 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|    141 | 2146 | `		if( rcPseudo == 1 ){` |
|     86 | 2147 | `			return SXRET_OK;` |
|      - | 2148 | `		}` |
|     59 | 2149 | `		if( rcPseudo == 0 ){` |
|      9 | 2150 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      4 | 2151 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|      2 | 2152 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 2153 | `		}` |
|      - | 2154 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|     25 | 2155 | `	}` |
|      - | 2156 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|      - | 2157 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|      - | 2158 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|   8983 | 2159 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      - | 2160 | `		sxi32 rcU;` |
|     25 | 2161 | `		const char *zExpected = "union";` |
|     25 | 2162 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|     25 | 2163 | `		if( rcU == SXRET_OK ){` |
|     21 | 2164 | `			return SXRET_OK;` |
|      - | 2165 | `		}` |
|      5 | 2166 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      3 | 2167 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      4 | 2168 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 2169 | `			zGiven = "null";` |
|    ! 0 | 2170 | `		}else{` |
|      3 | 2171 | `			zGiven = ph7_type_name(pValue);` |
|      - | 2172 | `		}` |
|      5 | 2173 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      5 | 2174 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      2 | 2175 | `		}` |
|      5 | 2176 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2177 | `	}` |
|      - | 2178 | `	/* Class return type — instanceof check. The class name is a length-` |
|      - | 2179 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|      - | 2180 | `	 * it into the TypeError message. */` |
|   8961 | 2181 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     55 | 2182 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      - | 2183 | `		const char *zExpected;` |
|     55 | 2184 | `		ph7_class *pExpected = VmResolveTypeClass(pVm,pClassName,VmCurrentSelf(pVm));` |
|     55 | 2185 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|     55 | 2186 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      5 | 2187 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      5 | 2188 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2189 | `		}` |
|     51 | 2190 | `		if( pExpected ){` |
|     45 | 2191 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     45 | 2192 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      3 | 2193 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      3 | 2194 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2195 | `			}` |
|     19 | 2196 | `		}` |
|     49 | 2197 | `		return SXRET_OK;` |
|      - | 2198 | `	}` |
|      - | 2199 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|      - | 2200 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|      - | 2201 | `	 * non-nullable scalar return — a TypeError. */` |
|   8911 | 2202 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 2203 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2204 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      - | 2205 | `			"null");` |
|      - | 2206 | `	}` |
|      - | 2207 | `	/* Exact match? Done. */` |
|   8911 | 2208 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|   8905 | 2209 | `		return SXRET_OK;` |
|      - | 2210 | `	}` |
|      - | 2211 | `	/* Object->scalar is never compatible. */` |
|      9 | 2212 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 2213 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|    ! 0 | 2214 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2215 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|    ! 0 | 2216 | `			zGiven);` |
|      - | 2217 | `	}` |
|      - | 2218 | `	/* Array <-> scalar is never compatible. */` |
|      9 | 2219 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|    ! 0 | 2220 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2221 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|    ! 0 | 2222 | `			ph7_type_name(pValue));` |
|      - | 2223 | `	}` |
|      - | 2224 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|      - | 2225 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|      - | 2226 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|      - | 2227 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|      6 | 2228 | `	if( !bStrict` |
|      5 | 2229 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|      4 | 2230 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|      7 | 2231 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|      4 | 2232 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      1 | 2233 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      - | 2234 | `			"string");` |
|      - | 2235 | `	}` |
|      6 | 2236 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|      3 | 2237 | `		return SXRET_OK;` |
|      - | 2238 | `	}` |
|      4 | 2239 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      1 | 2240 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      1 | 2241 | `		ph7_type_name(pValue));` |
|   4605 | 2242 | `}` |
|      - | 2243 | `/*` |
|      - | 2244 | ` * Report a fatal named-argument error.` |
|      - | 2245 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|      - | 2246 | ` */` |
|     12 | 2247 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|      3 | 2248 | `{` |
|      - | 2249 | `	SyBlob sMsg;` |
|      - | 2250 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|      - | 2251 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|      - | 2252 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|      - | 2253 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|      - | 2254 | `	 * unconditional fatal even inside try/catch. */` |
|     15 | 2255 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     15 | 2256 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|     15 | 2257 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      3 | 2258 | `}` |
|      - | 2259 | `/*` |
|      - | 2260 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - | 2261 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|      - | 2262 | ` * information.` |
|      - | 2263 | ` * ------------------------------------` |
|      - | 2264 | ` * Simple boring wrapper function.` |
|      - | 2265 | ` * ------------------------------------` |
|      - | 2266 | ` */` |
|  19674 | 2267 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|      5 | 2268 | `{` |
|      - | 2269 | `	sxi32 rc;` |
|  19679 | 2270 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|  19679 | 2271 | `	return rc;` |
|      5 | 2272 | `}` |
|      - | 2273 | `/*` |
|      - | 2274 | ` * Resolve function context from the current frame.` |
|      - | 2275 | ` */` |
|      - | 2276 | `/*` |
|      - | 2277 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|      - | 2278 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|      - | 2279 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|      - | 2280 | ` * straight at the function's own name otherwise.` |
|      - | 2281 | ` */` |
|   1732 | 2282 | `static int VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|      5 | 2283 | `{` |
|   1737 | 2284 | `	const char *zName = pFunc->sName.zString;` |
|   1737 | 2285 | `	int nName = (int)pFunc->sName.nByte;` |
|   1926 | 2286 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   2081 | 2287 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|   1737 | 2288 | `	if( bClosure ){` |
|      - | 2289 | `		int n;` |
|    325 | 2290 | `		if( pFunc->sFile.nByte > 0 ){` |
|    485 | 2291 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|    320 | 2292 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|    165 | 2293 | `		}else{` |
|    ! 0 | 2294 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|      - | 2295 | `		}` |
|    325 | 2296 | `		*pzOut = pVm->zDisplayName;` |
|    325 | 2297 | `		return n;` |
|      - | 2298 | `	}` |
|   1417 | 2299 | `	*pzOut = zName;` |
|   1417 | 2300 | `	return nName;` |
|    871 | 2301 | `}` |
|   1090 | 2302 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|      4 | 2303 | `{` |
|      - | 2304 | `	VmFrame *pFrame;` |
|      - | 2305 | `	ph7_vm_func *pFunc;` |
|   1094 | 2306 | `	*pzFuncName = 0;` |
|   1094 | 2307 | `	*pnFuncLen = 0;` |
|   1094 | 2308 | `	pFrame = pVm->pFrame;` |
|   1094 | 2309 | `	if( pFrame == 0 ){` |
|    ! 0 | 2310 | `		return;` |
|      - | 2311 | `	}` |
|   1094 | 2312 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|   1094 | 2313 | `	if( pFrame->pParent == 0 ){` |
|   1060 | 2314 | `		return;` |
|      - | 2315 | `	}` |
|     38 | 2316 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     38 | 2317 | `	if( pFunc == 0 ){` |
|    ! 0 | 2318 | `		return;` |
|      - | 2319 | `	}` |
|     38 | 2320 | `	*pnFuncLen = VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|    549 | 2321 | `}` |
|      - | 2322 | `/*` |
|      - | 2323 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|      - | 2324 | ` *` |
|      - | 2325 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|      - | 2326 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|      - | 2327 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|      - | 2328 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|      - | 2329 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|      - | 2330 | ` *             trailer.` |
|      - | 2331 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|      - | 2332 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|      - | 2333 | ` * call; this routine only appends.` |
|      - | 2334 | ` */` |
|    566 | 2335 | `static void VmRenderUncaughtEntry(` |
|      - | 2336 | `	ph7_vm *pVm,SyBlob *pOut,` |
|      - | 2337 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|      - | 2338 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|      - | 2339 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|      - | 2340 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|      4 | 2341 | `{` |
|      - | 2342 | `	SyString *pFile;` |
|    570 | 2343 | `	if( nThrowLine == 0 ){` |
|      5 | 2344 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|      2 | 2345 | `	}` |
|    570 | 2346 | `	if( nCallLine == 0 ){` |
|    570 | 2347 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|    570 | 2348 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|    283 | 2349 | `	}` |
|    570 | 2350 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|    ! 0 | 2351 | `		zClass = "Exception";` |
|    ! 0 | 2352 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|    ! 0 | 2353 | `	}` |
|    570 | 2354 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|    538 | 2355 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|    267 | 2356 | `	}` |
|    570 | 2357 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    570 | 2358 | `	if( bFirst ){` |
|    564 | 2359 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|    284 | 2360 | `	}else{` |
|      8 | 2361 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|      - | 2362 | `	}` |
|    570 | 2363 | `	SyBlobAppend(pOut,zClass,nClass);` |
|    570 | 2364 | `	if( zMsg && nMsg > 0 ){` |
|    570 | 2365 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|    570 | 2366 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|    283 | 2367 | `	}` |
|    570 | 2368 | `	if( pFile ){` |
|    570 | 2369 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|    283 | 2370 | `	}` |
|    570 | 2371 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|    570 | 2372 | `	if( pFile ){` |
|    570 | 2373 | `		SyBlobAppend(pOut,"#0 ",sizeof("#0 ")-1);` |
|    570 | 2374 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|    570 | 2375 | `		if( zFuncName && nFuncLen > 0 ){` |
|      - | 2376 | `			/* php reports a trace frame at its CALL SITE, not at the line running` |
|      - | 2377 | `			 * inside it. */` |
|     38 | 2378 | `			SyBlobFormat(pOut,"(%u): %.*s()\n",nCallLine,nFuncLen,zFuncName);` |
|     21 | 2379 | `		}else{` |
|    536 | 2380 | `			SyBlobFormat(pOut,"(%u): {main}\n",nCallLine);` |
|      4 | 2381 | `		}` |
|    283 | 2382 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|    ! 0 | 2383 | `		SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|    ! 0 | 2384 | `	}else{` |
|    ! 0 | 2385 | `		SyBlobAppend(pOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|      - | 2386 | `	}` |
|    570 | 2387 | `	SyBlobAppend(pOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|    570 | 2388 | `	if( bLast && pFile ){` |
|    564 | 2389 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|    564 | 2390 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|    280 | 2391 | `	}` |
|    570 | 2392 | `}` |
|      - | 2393 | `/*` |
|      - | 2394 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|      - | 2395 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|      - | 2396 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|      - | 2397 | ` */` |
|      4 | 2398 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|      1 | 2399 | `{` |
|      - | 2400 | `	SyBlob sOut;` |
|      - | 2401 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|      - | 2402 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|      5 | 2403 | `	pVm->iExitStatus = 255;` |
|      5 | 2404 | `	if( !pVm->bErrReport ){` |
|    ! 0 | 2405 | `		return PH7_OK;` |
|      - | 2406 | `	}` |
|      5 | 2407 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      5 | 2408 | `	VmRenderUncaughtEntry(pVm,&sOut,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|      5 | 2409 | `	VmCallErrorHandler(pVm,&sOut);` |
|      5 | 2410 | `	SyBlobRelease(&sOut);` |
|      5 | 2411 | `	return PH7_ABORT;` |
|      3 | 2412 | `}` |
|      - | 2413 | `/*` |
|      - | 2414 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|      - | 2415 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|      - | 2416 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|      - | 2417 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|      - | 2418 | ` */` |
|      - | 2419 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|    562 | 2420 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|      4 | 2421 | `{` |
|      - | 2422 | `	ph7_value *pValue;` |
|      - | 2423 | `	ph7_class_instance *pPrev;` |
|      - | 2424 | `	ph7_class *pThrowable;` |
|    566 | 2425 | `	if( pThis == 0 ){` |
|    ! 0 | 2426 | `		return 0;` |
|      - | 2427 | `	}` |
|    566 | 2428 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|    566 | 2429 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    560 | 2430 | `		return 0;` |
|      - | 2431 | `	}` |
|      8 | 2432 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|      - | 2433 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|      - | 2434 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|      - | 2435 | `	 * never renders a stray object as an exception entry. */` |
|      8 | 2436 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      8 | 2437 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|    ! 0 | 2438 | `		return 0;` |
|      - | 2439 | `	}` |
|      8 | 2440 | `	return pPrev;` |
|    285 | 2441 | `}` |
|      - | 2442 | `/*` |
|      - | 2443 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|      - | 2444 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|      - | 2445 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|      - | 2446 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|      - | 2447 | ` */` |
|     16 | 2448 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|      2 | 2449 | `{` |
|      - | 2450 | `	ph7_value *pValue;` |
|     18 | 2451 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|    ! 0 | 2452 | `		return;` |
|      - | 2453 | `	}` |
|     18 | 2454 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     18 | 2455 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      3 | 2456 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|      - | 2457 | `	}` |
|     16 | 2458 | `	pPrev->iRef++;` |
|      - | 2459 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|      - | 2460 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|     16 | 2461 | `	PH7_MemObjRelease(pValue);` |
|     16 | 2462 | `	pValue->x.pOther = pPrev;` |
|     16 | 2463 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|     10 | 2464 | `}` |
|      - | 2465 | `/*` |
|      - | 2466 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|      - | 2467 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|      - | 2468 | ` * absent or yields an empty string.` |
|      - | 2469 | ` */` |
|      - | 2470 | `/*` |
|      - | 2471 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|      - | 2472 | ` * 0 when the class exposes no getLine().` |
|      - | 2473 | ` */` |
|    562 | 2474 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      4 | 2475 | `{` |
|      - | 2476 | `	ph7_class_method *pGetLine;` |
|      - | 2477 | `	ph7_value sLine;` |
|    566 | 2478 | `	sxu32 nLine = 0;` |
|    566 | 2479 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|    566 | 2480 | `	if( pGetLine == 0 ){` |
|    ! 0 | 2481 | `		return 0;` |
|      - | 2482 | `	}` |
|    566 | 2483 | `	PH7_MemObjInit(pVm,&sLine);` |
|    566 | 2484 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|    566 | 2485 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|    566 | 2486 | `		if( n > 0 ){` |
|    566 | 2487 | `			nLine = (sxu32)n;` |
|    281 | 2488 | `		}` |
|    281 | 2489 | `	}` |
|    566 | 2490 | `	PH7_MemObjRelease(&sLine);` |
|    566 | 2491 | `	return nLine;` |
|    285 | 2492 | `}` |
|    562 | 2493 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|      4 | 2494 | `{` |
|      - | 2495 | `	ph7_class_method *pGetMessage;` |
|      - | 2496 | `	ph7_value sMsg;` |
|      - | 2497 | `	const char *zTmp;` |
|      - | 2498 | `	int nTmp;` |
|    566 | 2499 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|    566 | 2500 | `	if( pGetMessage == 0 ){` |
|    ! 0 | 2501 | `		return;` |
|      - | 2502 | `	}` |
|    566 | 2503 | `	PH7_MemObjInit(pVm,&sMsg);` |
|    566 | 2504 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|    566 | 2505 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|    566 | 2506 | `		if( zTmp && nTmp > 0 ){` |
|    566 | 2507 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|    281 | 2508 | `		}` |
|    281 | 2509 | `	}` |
|    566 | 2510 | `	PH7_MemObjRelease(&sMsg);` |
|    285 | 2511 | `}` |
|      - | 2512 | `/*` |
|      - | 2513 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|      - | 2514 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|      - | 2515 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|      - | 2516 | ` * outermost (the actually-uncaught) exception.` |
|      - | 2517 | ` *` |
|      - | 2518 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|      - | 2519 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|      - | 2520 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|      - | 2521 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|      - | 2522 | ` */` |
|      - | 2523 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|    556 | 2524 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|      4 | 2525 | `{` |
|      - | 2526 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|    560 | 2527 | `	int nChain = 0;` |
|      - | 2528 | `	int i;` |
|      - | 2529 | `	SyBlob sOut;` |
|      - | 2530 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|      - | 2531 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|      - | 2532 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|    560 | 2533 | `	pVm->iExitStatus = 255;` |
|    560 | 2534 | `	if( !pVm->bErrReport ){` |
|    ! 0 | 2535 | `		return PH7_OK;` |
|      - | 2536 | `	}` |
|      - | 2537 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|      - | 2538 | `	 * collected) or the hard cap. */` |
|   1122 | 2539 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|    574 | 2540 | `		for( i = 0 ; i < nChain ; ++i ){` |
|     10 | 2541 | `			if( apChain[i] == pThis ){` |
|    ! 0 | 2542 | `				pThis = 0; /* cycle: stop the walk */` |
|    ! 0 | 2543 | `				break;` |
|      - | 2544 | `			}` |
|      6 | 2545 | `		}` |
|    566 | 2546 | `		if( pThis == 0 ){` |
|    ! 0 | 2547 | `			break;` |
|      - | 2548 | `		}` |
|    566 | 2549 | `		apChain[nChain++] = pThis;` |
|    566 | 2550 | `		pThis = VmExceptionGetPrevious(pThis);` |
|      4 | 2551 | `	}` |
|    560 | 2552 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      - | 2553 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|      - | 2554 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|   1122 | 2555 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|    566 | 2556 | `		ph7_class_instance *pEnt = apChain[i];` |
|      - | 2557 | `		SyBlob sMsg;` |
|    566 | 2558 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    566 | 2559 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|    847 | 2560 | `		VmRenderUncaughtEntry(pVm,&sOut,` |
|    562 | 2561 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|    562 | 2562 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|    281 | 2563 | `			zFuncName,nFuncLen,` |
|    562 | 2564 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|    281 | 2565 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|    281 | 2566 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|    566 | 2567 | `		SyBlobRelease(&sMsg);` |
|    285 | 2568 | `	}` |
|    560 | 2569 | `	VmCallErrorHandler(pVm,&sOut);` |
|    560 | 2570 | `	SyBlobRelease(&sOut);` |
|    560 | 2571 | `	return PH7_ABORT;` |
|    282 | 2572 | `}` |
|      - | 2573 | `/*` |
|      - | 2574 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|      - | 2575 | ` *` |
|      - | 2576 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|      - | 2577 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|      - | 2578 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|      - | 2579 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|      - | 2580 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|      - | 2581 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|      - | 2582 | ` */` |
|   2878 | 2583 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|      5 | 2584 | `{` |
|   2883 | 2585 | `	if( pVm->bCoalesceArmed ){` |
|      8 | 2586 | `		if( pVm->pCoalesceObj ){` |
|      8 | 2587 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|      3 | 2588 | `		}` |
|      8 | 2589 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|      8 | 2590 | `		pVm->pCoalesceObj = 0;` |
|      8 | 2591 | `		pVm->bCoalesceArmed = 0;` |
|      3 | 2592 | `	}` |
|   2883 | 2593 | `}` |
|      - | 2594 | `/*` |
|      - | 2595 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|      - | 2596 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|      - | 2597 | ` * is a literal, non-formatted string; callers that need formatting should` |
|      - | 2598 | ` * build the SyBlob themselves and pass its data + length.` |
|      - | 2599 | ` *` |
|      - | 2600 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|      - | 2601 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|      - | 2602 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|      - | 2603 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|      - | 2604 | ` */` |
|     74 | 2605 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|      - | 2606 | `	ph7_vm *pVm,` |
|      - | 2607 | `	const char *zClass,` |
|      - | 2608 | `	const char *zMsg,` |
|      - | 2609 | `	sxu32 nMsg` |
|      5 | 2610 | `){` |
|      - | 2611 | `	ph7_class *pClass;` |
|      - | 2612 | `	ph7_class_instance *pThis;` |
|      - | 2613 | `	ph7_class_method *pCons;` |
|      - | 2614 | `	VmFrame *pFrame;` |
|      - | 2615 | `	sxi32 rc;` |
|     79 | 2616 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|     79 | 2617 | `	if( pClass == 0 ){` |
|    ! 0 | 2618 | `		return SXERR_ABORT;` |
|      - | 2619 | `	}` |
|     79 | 2620 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|     79 | 2621 | `	if( pThis == 0 ){` |
|    ! 0 | 2622 | `		return SXERR_ABORT;` |
|      - | 2623 | `	}` |
|     79 | 2624 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     79 | 2625 | `	if( pCons ){` |
|      - | 2626 | `		ph7_value sArg;` |
|      - | 2627 | `		ph7_value *apArg[1];` |
|      - | 2628 | `		SyString sMsgStr;` |
|     79 | 2629 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|     79 | 2630 | `		PH7_MemObjInit(pVm,&sArg);` |
|     79 | 2631 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|     79 | 2632 | `		apArg[0] = &sArg;` |
|     79 | 2633 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|     79 | 2634 | `		PH7_MemObjRelease(&sArg);` |
|     37 | 2635 | `	}` |
|     79 | 2636 | `	pFrame = pVm->pFrame;` |
|     79 | 2637 | `	if( pFrame ){` |
|     79 | 2638 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     79 | 2639 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     37 | 2640 | `	}` |
|     79 | 2641 | `	rc = VmThrowException(pVm,pThis);` |
|     79 | 2642 | `	PH7_ClassInstanceUnref(pThis);` |
|     79 | 2643 | `	return rc;` |
|     42 | 2644 | `}` |
|      - | 2645 | `/*` |
|      - | 2646 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|      - | 2647 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|      - | 2648 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|      - | 2649 | ` *` |
|      - | 2650 | ` *   int/float/bool/null      arithmetic proceeds` |
|      - | 2651 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|      - | 2652 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|      - | 2653 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|      - | 2654 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|      - | 2655 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|      - | 2656 | ` *   object/resource          TypeError, naming the object's CLASS` |
|      - | 2657 | ` *` |
|      - | 2658 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|      - | 2659 | ` */` |
|      - | 2660 | `/*` |
|      - | 2661 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|      - | 2662 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|      - | 2663 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|      - | 2664 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|      - | 2665 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|      - | 2666 | ` * to depth 1).` |
|      - | 2667 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|      - | 2668 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|      - | 2669 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|      - | 2670 | ` * frame shape is file/line/function[/class/type], matching the default` |
|      - | 2671 | ` * zend.exception_ignore_args=On.` |
|      - | 2672 | ` */` |
|   2728 | 2673 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)` |
|      5 | 2674 | `{` |
|      - | 2675 | `	SyString *pFile;` |
|      - | 2676 | `	VmFrame *pFrame;` |
|      - | 2677 | `	ph7_value *pValue;` |
|   2733 | 2678 | `	pValue = ph7_new_scalar(&(*pVm));` |
|   2733 | 2679 | `	if( pValue == 0 ){` |
|    ! 0 | 2680 | `		return;` |
|      - | 2681 | `	}` |
|   2733 | 2682 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2733 | 2683 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|   4431 | 2684 | `	while( pFrame ){` |
|   4431 | 2685 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      - | 2686 | `		ph7_value *pEntry;` |
|   4431 | 2687 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|      - | 2688 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   1369 | 2689 | `			break;` |
|      - | 2690 | `		}` |
|   1703 | 2691 | `		pEntry = ph7_new_array(&(*pVm));` |
|   1703 | 2692 | `		if( pEntry == 0 ){` |
|    ! 0 | 2693 | `			break;` |
|      - | 2694 | `		}` |
|      - | 2695 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|      - | 2696 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|      - | 2697 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|      - | 2698 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|      - | 2699 | `		 * include-stack top for a call made at global scope. */` |
|      - | 2700 | `		{` |
|   1703 | 2701 | `			SyString *pFrameFile = pFile;` |
|   1703 | 2702 | `			if( pFrame->pParent->pUserData ){` |
|    183 | 2703 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|    183 | 2704 | `				if( pCaller->sFile.nByte > 0 ){` |
|     71 | 2705 | `					pFrameFile = &pCaller->sFile;` |
|     33 | 2706 | `				}` |
|     89 | 2707 | `			}` |
|   1703 | 2708 | `			if( pFrameFile ){` |
|   1703 | 2709 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   1703 | 2710 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   1703 | 2711 | `				ph7_value_reset_string_cursor(pValue);` |
|    849 | 2712 | `			}` |
|      - | 2713 | `		}` |
|   1703 | 2714 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   1703 | 2715 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|      - | 2716 | `		{` |
|   1703 | 2717 | `			const char *zDisp = 0;` |
|   1703 | 2718 | `			int nDisp = VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   1703 | 2719 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|      - | 2720 | `		}` |
|   1703 | 2721 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   1703 | 2722 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 2723 | `		{` |
|      - | 2724 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|      - | 2725 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|      - | 2726 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|      - | 2727 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|      - | 2728 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|      - | 2729 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|      - | 2730 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   1703 | 2731 | `			SyString *pClsName = 0;` |
|   1703 | 2732 | `			const char *zType = "->";` |
|   1703 | 2733 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|    443 | 2734 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|    443 | 2735 | `				zType = pFrame->pThis ? "->" : "::";` |
|   1484 | 2736 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|    ! 0 | 2737 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|    ! 0 | 2738 | `			}` |
|   1703 | 2739 | `			if( pClsName ){` |
|    443 | 2740 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|    443 | 2741 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|    443 | 2742 | `				ph7_value_reset_string_cursor(pValue);` |
|    443 | 2743 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|    443 | 2744 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|    443 | 2745 | `				ph7_value_reset_string_cursor(pValue);` |
|    443 | 2746 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){` |
|      9 | 2747 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      9 | 2748 | `					if( pObjVal ){` |
|      9 | 2749 | `						pFrame->pThis->iRef++;` |
|      9 | 2750 | `						pObjVal->x.pOther = pFrame->pThis;` |
|      9 | 2751 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      9 | 2752 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      9 | 2753 | `						ph7_release_value(&(*pVm),pObjVal);` |
|      4 | 2754 | `					}` |
|      4 | 2755 | `				}` |
|    219 | 2756 | `			}` |
|      - | 2757 | `		}` |
|   1703 | 2758 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      9 | 2759 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      9 | 2760 | `			if( pArg ){` |
|      9 | 2761 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      - | 2762 | `				sxu32 n;` |
|     17 | 2763 | `				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){` |
|      9 | 2764 | `					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|      9 | 2765 | `					if( pObj ){` |
|      9 | 2766 | `						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      4 | 2767 | `					}` |
|      5 | 2768 | `				}` |
|      9 | 2769 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      9 | 2770 | `				ph7_release_value(&(*pVm),pArg);` |
|      4 | 2771 | `			}` |
|      4 | 2772 | `		}` |
|   1703 | 2773 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   1703 | 2774 | `		ph7_release_value(&(*pVm),pEntry);` |
|   1703 | 2775 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|      5 | 2776 | `	}` |
|   2733 | 2777 | `	ph7_release_value(&(*pVm),pValue);` |
|   1369 | 2778 | `}` |
|      - | 2779 | `/*` |
|      - | 2780 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|      - | 2781 | ` *` |
|      - | 2782 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|      - | 2783 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|      - | 2784 | ` * calls parent::__construct still reports the right position. The embedded` |
|      - | 2785 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|      - | 2786 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|      - | 2787 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|      - | 2788 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|      - | 2789 | ` */` |
|   8384 | 2790 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      5 | 2791 | `{` |
|      - | 2792 | `	static const char *azField[] = { "file", "line", "trace" };` |
|      - | 2793 | `	ph7_class *pThrowable;` |
|      - | 2794 | `	SyString *pFile;` |
|      - | 2795 | `	SyString *pSiteFile;` |
|      - | 2796 | `	sxu32 n;` |
|   8389 | 2797 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|    ! 0 | 2798 | `		return;` |
|      - | 2799 | `	}` |
|   8389 | 2800 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|   8389 | 2801 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   5671 | 2802 | `		return;` |
|      - | 2803 | `	}` |
|   2723 | 2804 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2723 | 2805 | `	pSiteFile = pFile;` |
|      - | 2806 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|      - | 2807 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|      - | 2808 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|      - | 2809 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|      - | 2810 | `	{` |
|   2723 | 2811 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|   2723 | 2812 | `		if( pInner && pInner->pUserData ){` |
|   1059 | 2813 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|   1059 | 2814 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|    795 | 2815 | `				pSiteFile = &pInnerFunc->sFile;` |
|    395 | 2816 | `			}` |
|    527 | 2817 | `		}` |
|      - | 2818 | `	}` |
|  10877 | 2819 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|      - | 2820 | `		SyHashEntry *pEntry;` |
|      - | 2821 | `		VmClassAttr *pVmAttr;` |
|      - | 2822 | `		ph7_value *pAttrValue;` |
|   8159 | 2823 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|   8159 | 2824 | `		if( pEntry == 0 ){` |
|    ! 0 | 2825 | `			continue;` |
|      - | 2826 | `		}` |
|   8159 | 2827 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   8159 | 2828 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   8159 | 2829 | `		if( pAttrValue == 0 ){` |
|    ! 0 | 2830 | `			continue;` |
|      - | 2831 | `		}` |
|   8159 | 2832 | `		if( n == 0 ){` |
|   2723 | 2833 | `			if( pSiteFile ){` |
|   2723 | 2834 | `				PH7_MemObjRelease(pAttrValue);` |
|   2723 | 2835 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   1364 | 2836 | `			}` |
|   6800 | 2837 | `		}else if( n == 1 ){` |
|   2723 | 2838 | `			PH7_MemObjRelease(pAttrValue);` |
|   2723 | 2839 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)(pVm->nCurLine ? pVm->nCurLine : 1));` |
|   1364 | 2840 | `		}else{` |
|      - | 2841 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|      - | 2842 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|      - | 2843 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|      - | 2844 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|   2723 | 2845 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|   2723 | 2846 | `			if( pList == 0 ){` |
|    ! 0 | 2847 | `				continue;` |
|      - | 2848 | `			}` |
|   2723 | 2849 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);` |
|      - | 2850 | `			/* Building the trace reserves new memobjs, which may realloc` |
|      - | 2851 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|      - | 2852 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|      - | 2853 | `			 * AFTER the walk before releasing/storing into it. */` |
|   2723 | 2854 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   2723 | 2855 | `			if( pAttrValue ){` |
|   2723 | 2856 | `				PH7_MemObjRelease(pAttrValue);` |
|   2723 | 2857 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   1359 | 2858 | `			}` |
|   2723 | 2859 | `			ph7_release_value(&(*pVm),pList);` |
|      - | 2860 | `		}` |
|   4082 | 2861 | `	}` |
|   4197 | 2862 | `}` |
|     44 | 2863 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|      2 | 2864 | `{` |
|     46 | 2865 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      3 | 2866 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      3 | 2867 | `		if( pInst && pInst->pClass ){` |
|      3 | 2868 | `			return pInst->pClass->sName.zString;` |
|      - | 2869 | `		}` |
|    ! 0 | 2870 | `	}` |
|     44 | 2871 | `	return ph7_type_name(pVal);` |
|     24 | 2872 | `}` |
|  25420 | 2873 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|      5 | 2874 | `{` |
|  25425 | 2875 | `	int bBadL = 0, bBadR = 0;` |
|      - | 2876 | `	int i;` |
|      - | 2877 | `	ph7_value *apOperand[2];` |
|  25425 | 2878 | `	apOperand[0] = pLeft;` |
|  25425 | 2879 | `	apOperand[1] = pRight;` |
|      - | 2880 | `	/* array + array is php's union operator, not arithmetic */` |
|  25420 | 2881 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|  13041 | 2882 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|   3827 | 2883 | `		return SXRET_OK;` |
|      - | 2884 | `	}` |
|  64799 | 2885 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  43201 | 2886 | `		ph7_value *pVal = apOperand[i];` |
|  43201 | 2887 | `		int bBad = 0;` |
|  43201 | 2888 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      5 | 2889 | `			bBad = 1;` |
|  43199 | 2890 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     50 | 2891 | `			const char *zTail = 0;` |
|     50 | 2892 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     50 | 2893 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|      - | 2894 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     11 | 2895 | `				bBad = 1;` |
|      6 | 2896 | `			}else{` |
|      - | 2897 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|      - | 2898 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|      - | 2899 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     42 | 2900 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|      3 | 2901 | `					zTail++;` |
|      1 | 2902 | `				}` |
|     40 | 2903 | `				if( zTail < zEnd ){` |
|     20 | 2904 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      9 | 2905 | `				}` |
|      - | 2906 | `			}` |
|     24 | 2907 | `		}` |
|  43201 | 2908 | `		if( bBad ){` |
|     15 | 2909 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|      7 | 2910 | `		}` |
|  21603 | 2911 | `	}` |
|  21603 | 2912 | `	if( bBadL \|\| bBadR ){` |
|      - | 2913 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|      - | 2914 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|      - | 2915 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     22 | 2916 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|      7 | 2917 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     15 | 2918 | `		return SXERR_INVALID;` |
|      - | 2919 | `	}` |
|  21589 | 2920 | `	return SXRET_OK;` |
|  12715 | 2921 | `}` |
|      - | 2922 | `/*` |
|      - | 2923 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|      - | 2924 | ` */` |
|   1388 | 2925 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      5 | 2926 | `{` |
|      - | 2927 | `	ph7_vm *pVm;` |
|      - | 2928 | `	ph7_class *pClass;` |
|      - | 2929 | `	ph7_class_instance *pThis;` |
|      - | 2930 | `	ph7_class_method *pCons;` |
|      - | 2931 | `	ph7_value sArg;` |
|      - | 2932 | `	ph7_value *apArg[1];` |
|      - | 2933 | `	SyBlob sMsg;` |
|      - | 2934 | `	SyString sMsgStr;` |
|      - | 2935 | `	VmFrame *pFrame;` |
|      - | 2936 | `	va_list ap;` |
|      - | 2937 | `	sxi32 rc;` |
|      - | 2938 |  |
|   1393 | 2939 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|    ! 0 | 2940 | `		return PH7_ABORT;` |
|      - | 2941 | `	}` |
|   1393 | 2942 | `	pVm = pCtx->pVm;` |
|   1393 | 2943 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|    ! 0 | 2944 | `		zClass = "Error";` |
|    ! 0 | 2945 | `	}` |
|   1393 | 2946 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|   1393 | 2947 | `	if( pClass == 0 ){` |
|    ! 0 | 2948 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|      - | 2949 | `			"Cannot throw internal exception, class '%s' is not available",` |
|    ! 0 | 2950 | `			zClass` |
|      - | 2951 | `			);` |
|      - | 2952 | `	}` |
|   1393 | 2953 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|   1393 | 2954 | `	if( pThis == 0 ){` |
|    ! 0 | 2955 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|      - | 2956 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|      - | 2957 | `			);` |
|      - | 2958 | `	}` |
|      - | 2959 |  |
|   1393 | 2960 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   1393 | 2961 | `	va_start(ap,zFormat);` |
|   1393 | 2962 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|   1393 | 2963 | `	va_end(ap);` |
|      - | 2964 |  |
|   1393 | 2965 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   1393 | 2966 | `	if( pCons ){` |
|   1393 | 2967 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|   1393 | 2968 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   1393 | 2969 | `		apArg[0] = &sArg;` |
|   1393 | 2970 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   1393 | 2971 | `		PH7_MemObjRelease(&sArg);` |
|    694 | 2972 | `	}` |
|   1393 | 2973 | `	SyBlobRelease(&sMsg);` |
|      - | 2974 |  |
|   1393 | 2975 | `	pFrame = pVm->pFrame;` |
|   1393 | 2976 | `	if( pFrame ){` |
|   1393 | 2977 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   1393 | 2978 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    694 | 2979 | `	}` |
|   1393 | 2980 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   1393 | 2981 | `	PH7_ClassInstanceUnref(pThis);` |
|   1393 | 2982 | `	if( rc == SXERR_ABORT ){` |
|    502 | 2983 | `		return PH7_ABORT;` |
|      - | 2984 | `	}` |
|    895 | 2985 | `	return PH7_EXCEPTION;` |
|    699 | 2986 | `}` |
|      - | 2987 | `/*` |
|      - | 2988 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|      - | 2989 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|      - | 2990 | ` */` |
|    ! 0 | 2991 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|    ! 0 | 2992 | `{` |
|      - | 2993 | `	ph7_vm *pVm;` |
|      - | 2994 | `	SyBlob sMsg;` |
|    ! 0 | 2995 | `	const char *zFuncName = 0;` |
|    ! 0 | 2996 | `	int nFuncLen = 0;` |
|      - | 2997 | `	va_list ap;` |
|      - | 2998 | `	sxi32 rc;` |
|      - | 2999 |  |
|    ! 0 | 3000 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|    ! 0 | 3001 | `		return PH7_OK;` |
|      - | 3002 | `	}` |
|    ! 0 | 3003 | `	pVm = pCtx->pVm;` |
|    ! 0 | 3004 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|    ! 0 | 3005 | `		zClass = "Error";` |
|    ! 0 | 3006 | `	}` |
|      - | 3007 |  |
|    ! 0 | 3008 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      - | 3009 |  |
|    ! 0 | 3010 | `	va_start(ap,zFormat);` |
|    ! 0 | 3011 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|    ! 0 | 3012 | `	va_end(ap);` |
|      - | 3013 |  |
|    ! 0 | 3014 | `	if( pCtx->pFunc ){` |
|    ! 0 | 3015 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|    ! 0 | 3016 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|    ! 0 | 3017 | `	}` |
|    ! 0 | 3018 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|    ! 0 | 3019 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|    ! 0 | 3020 | `	}` |
|    ! 0 | 3021 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|    ! 0 | 3022 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|    ! 0 | 3023 | `	SyBlobRelease(&sMsg);` |
|    ! 0 | 3024 | `	return rc;` |
|    ! 0 | 3025 | `}` |
|      - | 3026 | `/*` |
|      - | 3027 | ` * The following routine is invoked by the engine when an uncaught` |
|      - | 3028 | ` * exception is triggered.` |
|      - | 3029 | ` */` |
|    558 | 3030 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|      - | 3031 | `	ph7_vm *pVm, /* Target VM */` |
|      - | 3032 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|      - | 3033 | `	)` |
|      4 | 3034 | `{` |
|      - | 3035 | `	ph7_value *apArg[2],sArg;` |
|    562 | 3036 | `	int nArg = 1;` |
|      - | 3037 | `	sxi32 rc;` |
|    562 | 3038 | `	if( pVm->nExceptDepth > 15 ){` |
|      - | 3039 | `		/* Nesting limit reached */` |
|    ! 0 | 3040 | `		return SXRET_OK;` |
|      - | 3041 | `	}` |
|      - | 3042 | `	/* Call any exception handler if available */` |
|    562 | 3043 | `	PH7_MemObjInit(pVm,&sArg);` |
|    562 | 3044 | `	if( pThis ){` |
|      - | 3045 | `		/* Load the exception instance */` |
|    562 | 3046 | `		sArg.x.pOther = pThis;` |
|    562 | 3047 | `		pThis->iRef++;` |
|    562 | 3048 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|    283 | 3049 | `	}else{` |
|    ! 0 | 3050 | `		nArg = 0;` |
|      - | 3051 | `	}` |
|    562 | 3052 | `	apArg[0] = &sArg;` |
|      - | 3053 | `	/* Call the exception handler if available */` |
|    562 | 3054 | `	pVm->nExceptDepth++;` |
|    562 | 3055 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|    562 | 3056 | `	pVm->nExceptDepth--;` |
|    562 | 3057 | `	if( rc != SXRET_OK ){` |
|      - | 3058 | `		const char *zFuncName;` |
|      - | 3059 | `		int nFuncLen;` |
|    560 | 3060 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      - | 3061 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|    560 | 3062 | `		if( pThis ){` |
|      - | 3063 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|      - | 3064 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|      - | 3065 | `			 * renders byte-identically to the historical single-entry report. */` |
|    560 | 3066 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|    282 | 3067 | `		}else{` |
|      - | 3068 | `			/* No instance (internal report path) — default-class single entry. */` |
|    ! 0 | 3069 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|      - | 3070 | `		}` |
|      - | 3071 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|    560 | 3072 | `		rc = SXERR_ABORT;` |
|    278 | 3073 | `	}` |
|    562 | 3074 | `	PH7_MemObjRelease(&sArg);` |
|    562 | 3075 | `	return rc;` |
|    283 | 3076 | `}` |
|      - | 3077 | `/*` |
|      - | 3078 | ` * Throw a user exception.` |
|      - | 3079 | ` *` |
|      - | 3080 | ` * Exception dispatch follows this sequence:` |
|      - | 3081 | ` *` |
|      - | 3082 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|      - | 3083 | ` *    try/catch whose catch block matches the exception class.` |
|      - | 3084 | ` *` |
|      - | 3085 | ` * 2. If NO catch matches:` |
|      - | 3086 | ` *    a. Run finally (if present) for the current try block.` |
|      - | 3087 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|      - | 3088 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|      - | 3089 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|      - | 3090 | ` *       exception in pVm->pPendingException instead of reporting it` |
|      - | 3091 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|      - | 3092 | ` *    d. Otherwise, report as truly uncaught.` |
|      - | 3093 | ` *` |
|      - | 3094 | ` * 3. If a catch DOES match:` |
|      - | 3095 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|      - | 3096 | ` *       aException stack and resetting it. This prevents a re-throw` |
|      - | 3097 | ` *       inside the catch body from immediately propagating past our` |
|      - | 3098 | ` *       finally block.` |
|      - | 3099 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|      - | 3100 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|      - | 3101 | ` *       no handlers (they're hidden), so the exception is deferred` |
|      - | 3102 | ` *       in pPendingException (step 2c).` |
|      - | 3103 | ` *    c. Restore outer handlers from the saved copy.` |
|      - | 3104 | ` *    d. Run finally (if present).` |
|      - | 3105 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|      - | 3106 | ` *       that handlers are restored and finally has run.` |
|      - | 3107 | ` */` |
|      - | 3108 | `/*` |
|      - | 3109 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|      - | 3110 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|      - | 3111 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|      - | 3112 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|      - | 3113 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|      - | 3114 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|      - | 3115 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|      - | 3116 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|      - | 3117 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|      - | 3118 | ` */` |
|    100 | 3119 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|      5 | 3120 | `{` |
|    109 | 3121 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|     39 | 3122 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     39 | 3123 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|     39 | 3124 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|    ! 0 | 3125 | `			break; /* reached an outer exec's / legacy handler */` |
|      - | 3126 | `		}` |
|     39 | 3127 | `		(void)SySetPop(&pVm->aException);` |
|     39 | 3128 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|     39 | 3129 | `		if( pT->iHasFinally ){` |
|     35 | 3130 | `			*pPc = pT->iFinallyPc;` |
|     35 | 3131 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|     35 | 3132 | `			return 1;` |
|      - | 3133 | `		}` |
|      - | 3134 | `		/* No finally: tear the try's transparent frame down now. */` |
|      5 | 3135 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|    ! 0 | 3136 | `			VmLeaveFrame(&(*pVm));` |
|    ! 0 | 3137 | `		}` |
|      5 | 3138 | `		VmExcRelease(&(*pVm),pT);` |
|      1 | 3139 | `	}` |
|     73 | 3140 | `	return 0;` |
|     55 | 3141 | `}` |
|      - | 3142 | `/*` |
|      - | 3143 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|      - | 3144 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|      - | 3145 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|      - | 3146 | ` *` |
|      - | 3147 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|      - | 3148 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|      - | 3149 | ` *    and redirect to the catch body (iHandlerPc).` |
|      - | 3150 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|      - | 3151 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|      - | 3152 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|      - | 3153 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|      - | 3154 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|      - | 3155 | ` */` |
|      - | 3156 | `/*` |
|      - | 3157 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|      - | 3158 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|      - | 3159 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|      - | 3160 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|      - | 3161 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|      - | 3162 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|      - | 3163 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|      - | 3164 | ` * case) is unchanged: no wrapper.` |
|      - | 3165 | ` */` |
|    194 | 3166 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|      5 | 3167 | `{` |
|    199 | 3168 | `	VmFrame *pWrap = 0;` |
|      - | 3169 | `	VmFrame *pThrowSite;` |
|      - | 3170 | `	sxi32 rc;` |
|    199 | 3171 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     97 | 3172 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3173 | `	}` |
|    104 | 3174 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|      - | 3175 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|    ! 0 | 3176 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3177 | `	}` |
|    104 | 3178 | `	pThrowSite = pWrap->pParent;` |
|    104 | 3179 | `	pWrap->pParent = pOwner;` |
|    104 | 3180 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|    104 | 3181 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3182 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|      - | 3183 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|      - | 3184 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|      - | 3185 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|    104 | 3186 | `	if( pVm->pFrame == pWrap ){` |
|    104 | 3187 | `		VmLeaveFrame(&(*pVm));` |
|     51 | 3188 | `	}` |
|    104 | 3189 | `	pVm->pFrame = pThrowSite;` |
|    104 | 3190 | `	return rc;` |
|    102 | 3191 | `}` |
|      - | 3192 | `/*` |
|      - | 3193 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|      - | 3194 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|      - | 3195 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|      - | 3196 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|      - | 3197 | ` */` |
|      - | 3198 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|     76 | 3199 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - | 3200 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|      5 | 3201 | `{` |
|     81 | 3202 | `	if( pCatch ){` |
|     71 | 3203 | `		pException->iInCatch = 1;` |
|     71 | 3204 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|     71 | 3205 | `		if( pThis ){ pThis->iRef++; }` |
|     71 | 3206 | `		pException->pInflight = pThis;` |
|     71 | 3207 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|     71 | 3208 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|     71 | 3209 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|     71 | 3210 | `		return SXRET_OK;` |
|      - | 3211 | `	}` |
|     13 | 3212 | `	if( pException->iHasFinally ){` |
|      - | 3213 | `		VmFinallyAction sAct;` |
|      8 | 3214 | `		SyZero(&sAct,sizeof(sAct));` |
|      8 | 3215 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      8 | 3216 | `		if( pThis ){ pThis->iRef++; }` |
|      8 | 3217 | `		sAct.pExc = pThis;` |
|      8 | 3218 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      8 | 3219 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      8 | 3220 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      8 | 3221 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      8 | 3222 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      8 | 3223 | `		return SXRET_OK;` |
|      - | 3224 | `	}` |
|      - | 3225 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|      - | 3226 | `	 * flat native stack instead of mutual recursion. */` |
|      6 | 3227 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|    ! 0 | 3228 | `		VmLeaveFrame(&(*pVm));` |
|    ! 0 | 3229 | `	}` |
|      6 | 3230 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      6 | 3231 | `	return VM_THROW_KEEP_UNWINDING;` |
|     43 | 3232 | `}` |
|   2686 | 3233 | `PH7_PRIVATE sxi32 VmThrowException(` |
|      - | 3234 | `	ph7_vm *pVm,              /* Target VM */` |
|      - | 3235 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|      - | 3236 | `	)` |
|      5 | 3237 | `{` |
|      - | 3238 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|      - | 3239 | `	ph7_exception **apException;` |
|   1343 | 3240 | `	ph7_exception *pException;` |
|     90 | 3241 | `Rethrow:` |
|      - | 3242 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|      - | 3243 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|      - | 3244 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|      - | 3245 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|      - | 3246 | `	 * so the throw path must be too). */` |
|      - | 3247 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|      - | 3248 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|      - | 3249 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|   2871 | 3250 | `	VmCoalesceDisarm(pVm);` |
|      - | 3251 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|      - | 3252 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|      - | 3253 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|      - | 3254 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|      - | 3255 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|      - | 3256 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|      - | 3257 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|      - | 3258 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|   2866 | 3259 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|     23 | 3260 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|     18 | 3261 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|      8 | 3262 | `	}` |
|      - | 3263 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|      - | 3264 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|      - | 3265 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|      - | 3266 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|      - | 3267 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|      - | 3268 | `	 * that owns the pending return, so it must leave that return intact. */` |
|      - | 3269 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|      - | 3270 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|      - | 3271 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|      - | 3272 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|   2871 | 3273 | `	pVm->pResumeFrame = 0;` |
|      - | 3274 | `	/* Point to the stack of loaded exceptions */` |
|   2871 | 3275 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|   2871 | 3276 | `	pException = 0;` |
|   2871 | 3277 | `	pCatch = 0;` |
|   2871 | 3278 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|      - | 3279 | `		ph7_exception_block *aCatch;` |
|      - | 3280 | `		ph7_class *pClass;` |
|      - | 3281 | `		SyString *aNames;` |
|      - | 3282 | `		sxu32 nNames;` |
|      - | 3283 | `		int matched;` |
|      - | 3284 | `		sxu32 j,k;` |
|      - | 3285 | `		/* Locate the appropriate block to execute */` |
|   2243 | 3286 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|   2243 | 3287 | `		(void)SySetPop(&pVm->aException);` |
|   2243 | 3288 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      - | 3289 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|      - | 3290 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|      - | 3291 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|   2257 | 3292 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|      - | 3293 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|   2115 | 3294 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|   2115 | 3295 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|   2115 | 3296 | `			matched = 0;` |
|   2151 | 3297 | `			for( k = 0 ; k < nNames ; ++k ){` |
|      - | 3298 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|      - | 3299 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|      - | 3300 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|   2137 | 3301 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|   2137 | 3302 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|      - | 3303 | `					/* No such class, or trait — cannot match */` |
|    ! 0 | 3304 | `					continue;` |
|      - | 3305 | `				}` |
|   2137 | 3306 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|   2101 | 3307 | `					matched = 1;` |
|   2101 | 3308 | `					break;` |
|      - | 3309 | `				}` |
|     21 | 3310 | `			}` |
|   2115 | 3311 | `			if( matched ){` |
|      - | 3312 | `				/* Catch block found,break immediately */` |
|   2101 | 3313 | `				pCatch = &aCatch[j];` |
|   2101 | 3314 | `				break;` |
|      - | 3315 | `			}` |
|     10 | 3316 | `		}` |
|   1119 | 3317 | `	}` |
|      - | 3318 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|      - | 3319 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|      - | 3320 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|      - | 3321 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|      - | 3322 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|      - | 3323 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|      - | 3324 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|   2871 | 3325 | `	if( pException ){` |
|   2243 | 3326 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   1119 | 3327 | `	}` |
|      - | 3328 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|      - | 3329 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|      - | 3330 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|   2871 | 3331 | `	if( pException && pException->iInlined ){` |
|     81 | 3332 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|     81 | 3333 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|      - | 3334 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|      6 | 3335 | `			goto Rethrow;` |
|      - | 3336 | `		}` |
|     77 | 3337 | `		return rcInline;` |
|      - | 3338 | `	}` |
|      - | 3339 | `	/* Execute the cached block if available */` |
|   2795 | 3340 | `	if( pCatch == 0 ){` |
|      - | 3341 | `		sxi32 rc;` |
|      - | 3342 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|    765 | 3343 | `		if( pException && pException->iHasFinally ){` |
|    135 | 3344 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    135 | 3345 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    135 | 3346 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|    135 | 3347 | `			pException->iFinallyDone = 1;` |
|      - | 3348 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|      - | 3349 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    135 | 3350 | `			pVm->pInflightException = pThis;` |
|    135 | 3351 | `			pVm->nInflightExcBase = nExcBefore;` |
|      - | 3352 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|      - | 3353 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    135 | 3354 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    135 | 3355 | `			pVm->pInflightException = pSaveInflight;` |
|    135 | 3356 | `			pVm->nInflightExcBase = nSaveBase;` |
|    135 | 3357 | `			if( rc == SXERR_ABORT ){` |
|      3 | 3358 | `				VmExcRelease(&(*pVm),pException);` |
|      3 | 3359 | `				return SXERR_ABORT;` |
|      - | 3360 | `			}` |
|      - | 3361 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|      - | 3362 | `			 * semantics). The finally stored it on the body frame it returns from` |
|      - | 3363 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|      - | 3364 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|      - | 3365 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|      - | 3366 | `			 * takes the value instead of unwinding) and resume in place.` |
|      - | 3367 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|      - | 3368 | `			 * the same transport an in-place catch uses — and unwind as an` |
|      - | 3369 | `			 * exception; the owner's activation consumes the resume` |
|      - | 3370 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|      - | 3371 | `			 * its bHasRet tail materializes the return. */` |
|      - | 3372 | `			{` |
|    133 | 3373 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    133 | 3374 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    133 | 3375 | `				if( pOwnerFrame->bHasRet ){` |
|      8 | 3376 | `					if( pOwnerFrame == pThrowFrame ){` |
|      5 | 3377 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|      5 | 3378 | `						VmExcRelease(&(*pVm),pException);` |
|      5 | 3379 | `						return SXRET_OK;` |
|      - | 3380 | `					}` |
|      3 | 3381 | `					pVm->pResumeFrame = pOwnerFrame;` |
|      3 | 3382 | `					pVm->iResumePc = pException->iLandingPc;` |
|      3 | 3383 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|      3 | 3384 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|      3 | 3385 | `					VmExcRelease(&(*pVm),pException);` |
|      3 | 3386 | `					return PH7_EXCEPTION;` |
|      - | 3387 | `				}` |
|      - | 3388 | `			}` |
|      - | 3389 | `			/* The finally threw an exception that superseded pThis — it either` |
|      - | 3390 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|      - | 3391 | `			 * (which consumed an entry from the exception stack). Either way the` |
|      - | 3392 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|      - | 3393 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|    127 | 3394 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|     16 | 3395 | `				VmExcRelease(&(*pVm),pException);` |
|     16 | 3396 | `				return PH7_EXCEPTION;` |
|      - | 3397 | `			}` |
|     54 | 3398 | `		}` |
|      - | 3399 | `		/* Check if there is an outer exception handler on the stack */` |
|    743 | 3400 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|      - | 3401 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|      - | 3402 | `			 * iteration per unwound level instead of one native frame. */` |
|    114 | 3403 | `			VmExcRelease(&(*pVm),pException);` |
|    114 | 3404 | `			goto Rethrow;` |
|      - | 3405 | `		}` |
|      - | 3406 | `		/* No outer handler. If the handlers were temporarily hidden` |
|      - | 3407 | `		 * (catch body re-throw with finally pending), defer the` |
|      - | 3408 | `		 * exception instead of reporting it uncaught.` |
|      - | 3409 | `		 */` |
|    633 | 3410 | `		if( pVm->pPendingException == 0 && pThis ){` |
|      - | 3411 | `			/* Check if we are inside a catch execution with hidden handlers` |
|      - | 3412 | `			 * by looking for a catch frame on the stack.` |
|      - | 3413 | `			 */` |
|    633 | 3414 | `			VmFrame *pF = pVm->pFrame;` |
|    633 | 3415 | `			int inCatch = 0;` |
|   1231 | 3416 | `			while( pF ){` |
|    673 | 3417 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|     73 | 3418 | `					inCatch = 1;` |
|     73 | 3419 | `					break;` |
|      - | 3420 | `				}` |
|    602 | 3421 | `				pF = pF->pParent;` |
|      4 | 3422 | `			}` |
|    633 | 3423 | `			if( inCatch ){` |
|      - | 3424 | `				/* Defer — will be re-thrown after finally runs */` |
|     73 | 3425 | `				pThis->iRef++;` |
|     73 | 3426 | `				pVm->pPendingException = pThis;` |
|     73 | 3427 | `				VmExcRelease(&(*pVm),pException);` |
|     73 | 3428 | `				return SXRET_OK;` |
|      - | 3429 | `			}` |
|    279 | 3430 | `		}` |
|      - | 3431 | `		/* Truly uncaught */` |
|    562 | 3432 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|    562 | 3433 | `		if( rc == SXRET_OK && pException ){` |
|    ! 0 | 3434 | `			VmFrame *pFrame = pVm->pFrame;` |
|    ! 0 | 3435 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|    ! 0 | 3436 | `			if( pException->pFrame == pFrame ){` |
|    ! 0 | 3437 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|    ! 0 | 3438 | `			}` |
|    ! 0 | 3439 | `		}` |
|    562 | 3440 | `		VmExcRelease(&(*pVm),pException);` |
|    562 | 3441 | `		return rc;` |
|    ! 0 | 3442 | `	}else{` |
|   2035 | 3443 | `		VmFrame *pFrame = pVm->pFrame;` |
|   2035 | 3444 | `		ph7_exception **apSaved = 0;` |
|      - | 3445 | `		sxu32 nSavedCount;` |
|      - | 3446 | `		sxi32 rc;` |
|      - | 3447 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|      - | 3448 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|      - | 3449 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|      - | 3450 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|      - | 3451 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|   2035 | 3452 | `		VmFrame *pCatchBody = pException->pFrame;` |
|   2035 | 3453 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|   2035 | 3454 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|   2035 | 3455 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   2035 | 3456 | `		if( pException->pFrame == pFrame ){` |
|   1157 | 3457 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|    576 | 3458 | `		}` |
|      - | 3459 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|      - | 3460 | `		 * body re-throws, the exception does not immediately propagate past` |
|      - | 3461 | `		 * our finally block. We save the stack contents and restore after.` |
|      - | 3462 | `		 */` |
|   2035 | 3463 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|   2035 | 3464 | `		if( nSavedCount > 0 ){` |
|    142 | 3465 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     46 | 3466 | `				nSavedCount * sizeof(ph7_exception *));` |
|     96 | 3467 | `			if( apSaved ){` |
|    142 | 3468 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|     46 | 3469 | `					nSavedCount * sizeof(ph7_exception *));` |
|     96 | 3470 | `				SySetReset(&pVm->aException);` |
|     46 | 3471 | `			}` |
|     46 | 3472 | `		}` |
|      - | 3473 | `		/* Create the catch frame (made transparent below) */` |
|   2035 | 3474 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|   2035 | 3475 | `		if( rc == SXRET_OK ){` |
|      - | 3476 | `			ph7_value *pObj;` |
|      - | 3477 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|      - | 3478 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|      - | 3479 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|      - | 3480 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|      - | 3481 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|      - | 3482 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|      - | 3483 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|      - | 3484 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|      - | 3485 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|   2035 | 3486 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|      - | 3487 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|      - | 3488 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|      - | 3489 | `			 * against the live current scope rather than a freed frame. */` |
|   2035 | 3490 | `			if( pCatchBody ){` |
|   2035 | 3491 | `				pFrame->pParent = pCatchBody;` |
|   1015 | 3492 | `			}` |
|      - | 3493 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|      - | 3494 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|      - | 3495 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|      - | 3496 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|      - | 3497 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|      - | 3498 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|      - | 3499 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|      - | 3500 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|   2035 | 3501 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|      - | 3502 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|   3050 | 3503 | `			pObj = (pCatch->sThis.nByte > 0)` |
|   2028 | 3504 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|   2035 | 3505 | `			if( pObj ){` |
|      - | 3506 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|      - | 3507 | `				 * so it may already hold a value from a prior catch or assignment.` |
|      - | 3508 | `				 * Pin the new instance, then release the slot's prior contents` |
|      - | 3509 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|      - | 3510 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|      - | 3511 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|   2031 | 3512 | `				pThis->iRef++;` |
|   2031 | 3513 | `				PH7_MemObjRelease(pObj);` |
|   2031 | 3514 | `				pObj->x.pOther = pThis;` |
|   2031 | 3515 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   1013 | 3516 | `			}` |
|      - | 3517 | `			/* Execute the catch block */` |
|   2035 | 3518 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|      - | 3519 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|      - | 3520 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|      - | 3521 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|      - | 3522 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|      - | 3523 | `			 * unbalanced — never pop somebody else's frame. */` |
|   2035 | 3524 | `			if( pVm->pFrame == pFrame ){` |
|   2035 | 3525 | `				VmLeaveFrame(&(*pVm));` |
|   1015 | 3526 | `			}` |
|   2035 | 3527 | `			pVm->pFrame = pThrowSite;` |
|   1015 | 3528 | `		}` |
|      - | 3529 | `		/* Restore the outer exception handlers */` |
|   2035 | 3530 | `		if( apSaved ){` |
|      - | 3531 | `			sxu32 k;` |
|      - | 3532 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|      - | 3533 | `			 * the catch body) are normally already consumed; on an abnormal` |
|      - | 3534 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|      - | 3535 | `			 * linger — release those activations before discarding the set. */` |
|     96 | 3536 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|     96 | 3537 | `			SySetReset(&pVm->aException);` |
|    840 | 3538 | `			for(k = 0; k < nSavedCount; k++){` |
|    748 | 3539 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    376 | 3540 | `			}` |
|     96 | 3541 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|     46 | 3542 | `		}` |
|      - | 3543 | `		/* Execute the finally block after catch */` |
|   2035 | 3544 | `		if( pException->iHasFinally ){` |
|      - | 3545 | `			sxi32 rcf;` |
|      - | 3546 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|      - | 3547 | `			 * from, its pending-return write generation (set if the catch above` |
|      - | 3548 | `			 * returned), and the exception-stack depth. After the finally we use` |
|      - | 3549 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|      - | 3550 | `			 * catch-return. */` |
|      - | 3551 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|      - | 3552 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|      - | 3553 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|      - | 3554 | `			 * supersede decision belong to the owner, not the thrower. */` |
|     69 | 3555 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|     69 | 3556 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|     69 | 3557 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|      - | 3558 | `			/* The exception in flight while this finally runs is the catch body's` |
|      - | 3559 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|      - | 3560 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|      - | 3561 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|      - | 3562 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|     69 | 3563 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|     69 | 3564 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|     69 | 3565 | `			pException->iFinallyDone = 1;` |
|     69 | 3566 | `			pVm->pInflightException = pVm->pPendingException;` |
|     69 | 3567 | `			pVm->nInflightExcBase = nExcBefore;` |
|      - | 3568 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|     69 | 3569 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|     69 | 3570 | `			pVm->pInflightException = pSaveInflight;` |
|     69 | 3571 | `			pVm->nInflightExcBase = nSaveBase;` |
|     69 | 3572 | `			if( rcf == SXERR_ABORT ){` |
|    ! 0 | 3573 | `				VmExcRelease(&(*pVm),pException);` |
|    ! 0 | 3574 | `				return SXERR_ABORT;` |
|      - | 3575 | `			}` |
|      - | 3576 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|      - | 3577 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|      - | 3578 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|      - | 3579 | `			 * either case that exception supersedes this try's catch-return — but` |
|      - | 3580 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|      - | 3581 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|      - | 3582 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|     64 | 3583 | `			if( (rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore)` |
|     45 | 3584 | `			 && pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|     12 | 3585 | `				VmClearFrameReturn(pBody);` |
|      5 | 3586 | `			}` |
|     69 | 3587 | `			if( rcf == PH7_EXCEPTION ){` |
|      - | 3588 | `				/* The finally's exception propagated past this try; drop any deferred` |
|      - | 3589 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|      - | 3590 | `				 * reaches the frame that caught the finally's throw. */` |
|     19 | 3591 | `				if( pVm->pPendingException ){` |
|    ! 0 | 3592 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|    ! 0 | 3593 | `					pVm->pPendingException = 0;` |
|    ! 0 | 3594 | `				}` |
|     19 | 3595 | `				VmExcRelease(&(*pVm),pException);` |
|     19 | 3596 | `				return PH7_EXCEPTION;` |
|      - | 3597 | `			}` |
|     24 | 3598 | `		}` |
|   2019 | 3599 | `		if( rc == SXERR_ABORT ){` |
|      5 | 3600 | `			VmExcRelease(&(*pVm),pException);` |
|      5 | 3601 | `			return SXERR_ABORT;` |
|      - | 3602 | `		}` |
|      - | 3603 | `		/* If the catch body re-threw, the exception was deferred in` |
|      - | 3604 | `		 * pPendingException (because outer handlers were hidden).` |
|      - | 3605 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|      - | 3606 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|      - | 3607 | `		 * the catch frame having been left above), which swallows the in-flight` |
|      - | 3608 | `		 * exception (PHP semantics).` |
|      - | 3609 | `		 */` |
|   2015 | 3610 | `		if( pVm->pPendingException ){` |
|      - | 3611 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|     73 | 3612 | `			if( !(pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame))->bHasRet ){` |
|     69 | 3613 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|     69 | 3614 | `				pVm->pPendingException = 0;` |
|     69 | 3615 | `				VmExcRelease(&(*pVm),pException);` |
|      - | 3616 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|     69 | 3617 | `				pThis = pReThrow;` |
|     69 | 3618 | `				goto Rethrow;` |
|      - | 3619 | `			}` |
|      - | 3620 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|      6 | 3621 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      6 | 3622 | `			pVm->pPendingException = 0;` |
|      2 | 3623 | `		}` |
|      - | 3624 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|      - | 3625 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|      - | 3626 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|      - | 3627 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|   1949 | 3628 | `		pVm->pResumeFrame = pCatchBody;` |
|   1949 | 3629 | `		pVm->iResumePc = iCatchPc;` |
|   1949 | 3630 | `		pVm->pResumeInstr = pCatchInstr;` |
|   1949 | 3631 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|      - | 3632 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|      - | 3633 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|      - | 3634 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|   1949 | 3635 | `		VmExcRelease(&(*pVm),pException);` |
|      - | 3636 | `	}` |
|   1949 | 3637 | `	return SXRET_OK;` |
|   1348 | 3638 | `}` |
|      - | 3639 |  |
