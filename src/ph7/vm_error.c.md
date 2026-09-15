# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1735/2005 lines (86.53%)

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
|  19664 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|      5 |   25 | `{` |
|  19669 |   26 | `	pVm->nLastErrType = iErr;` |
|  19669 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|  19669 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|  19669 |   29 | `	if( zMsg && nMsg > 0 ){` |
|  19669 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   9832 |   31 | `	}` |
|  19669 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|  19669 |   33 | `	if( pFile ){` |
|  19669 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   9832 |   35 | `	}` |
|  19669 |   36 | `}` |
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
|  19784 |   57 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|      5 |   58 | `{` |
|  19789 |   59 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|      - |   60 | `		ph7_value apArg[4];` |
|      - |   61 | `		ph7_value *apArgPtr[4];` |
|      - |   62 | `		ph7_value sResult;` |
|      - |   63 | `		SyString sErr;` |
|      - |   64 | `		/* Prepare arguments */` |
|    127 |   65 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|      - |   66 | `			/* use explicit message length to avoid reading past buffer */` |
|    127 |   67 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|    127 |   68 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|    127 |   69 | `		if( pFile ){` |
|    127 |   70 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|    127 |   71 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     65 |   72 | `		}else{` |
|    ! 0 |   73 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|      - |   74 | `		}` |
|    127 |   75 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|    127 |   76 | `		PH7_MemObjInit(pVm,&sResult);` |
|      - |   77 | `		/* Set up pointer array */` |
|    127 |   78 | `		apArgPtr[0] = &apArg[0];` |
|    127 |   79 | `		apArgPtr[1] = &apArg[1];` |
|    127 |   80 | `		apArgPtr[2] = &apArg[2];` |
|    127 |   81 | `		apArgPtr[3] = &apArg[3];` |
|      - |   82 | `		/* Call the handler */` |
|      - |   83 | `		{` |
|    127 |   84 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|    127 |   85 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
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
|    125 |  100 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 |  101 | `			PH7_MemObjToBool(&sResult);` |
|    ! 0 |  102 | `		}` |
|      - |  103 | `		/* Release */` |
|    125 |  104 | `		PH7_MemObjRelease(&apArg[0]);` |
|    125 |  105 | `		PH7_MemObjRelease(&apArg[1]);` |
|    125 |  106 | `		PH7_MemObjRelease(&apArg[2]);` |
|    125 |  107 | `		PH7_MemObjRelease(&apArg[3]);` |
|    125 |  108 | `		PH7_MemObjRelease(&sResult);` |
|      - |  109 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|      - |  110 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|    125 |  111 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|      - |  112 | `	}` |
|      - |  113 | `	/* No handler, always call error handler */` |
|  19665 |  114 | `	return TRUE;` |
|   9897 |  115 | `}` |
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
|  19664 |  133 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|      5 |  134 | `{` |
|      - |  135 | `	sxi32 iBit;` |
|  19669 |  136 | `	if( !pVm->bErrReport ){` |
|   3532 |  137 | `		return 0;` |
|      - |  138 | `	}` |
|  16139 |  139 | `	switch( iErr ){` |
|   8046 |  140 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|  16097 |  141 | `		iBit = 2; break;` |
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
|  16139 |  159 | `	return (pVm->iErrMask & iBit) != 0;` |
|   9837 |  160 | `}` |
|  19784 |  161 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|      5 |  162 | `{` |
|  19789 |  163 | `	switch(iErr){` |
|   9855 |  164 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|      - |  165 | `	case 512  /* E_USER_WARNING */:` |
|  19715 |  166 | `		return "Warning";` |
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
|   9897 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|      - |  182 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|      - |  183 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|      - |  184 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|      - |  185 | ` * be authored cross-engine with --EXPECTF--.` |
|      - |  186 | ` */` |
|  19784 |  187 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr,SyString *pFuncName)` |
|      5 |  188 | `{` |
|  19789 |  189 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|  19789 |  190 | `	if( pFuncName ){` |
|     41 |  191 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|     41 |  192 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|     18 |  193 | `	}` |
|  19789 |  194 | `}` |
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
| 104025 |  294 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|      5 |  295 | `{` |
|      - |  296 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|      - |  297 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
| 104030 |  298 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|      5 |  299 | `}` |
|      - |  300 | `/*` |
|      - |  301 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|      - |  302 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|      - |  303 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|      - |  304 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|      - |  305 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|      - |  306 | ` * keep native re-entries off a finite C stack).` |
|      - |  307 | ` */` |
| 158488 |  308 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|      5 |  309 | `{` |
| 158493 |  310 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
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
|      2 |  345 | `{` |
|      6 |  346 | `	if( pVm->bHaltRequested ){` |
|    ! 0 |  347 | `		return PH7_ABORT;` |
|      - |  348 | `	}` |
|      6 |  349 | `	pVm->iExitStatus = 255;` |
|      6 |  350 | `	pVm->bHaltRequested = 1;` |
|      6 |  351 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|      6 |  352 | `	return PH7_ABORT;` |
|      4 |  353 | `}` |
|      - |  354 | `/*` |
|      - |  355 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - |  356 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|      - |  357 | ` * information.` |
|      - |  358 | ` */` |
|  19708 |  359 | `static sxi32 VmThrowErrorAp(` |
|      - |  360 | `	ph7_vm *pVm,         /* Target VM */` |
|      - |  361 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|      - |  362 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|      - |  363 | `	const char *zFormat, /* Format message */` |
|      - |  364 | `	va_list ap           /* Variable list of arguments */` |
|      - |  365 | `	)` |
|      5 |  366 | `{` |
|  19713 |  367 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      - |  368 | `	SyBlob sMsg;` |
|      - |  369 | `	SyString *pFile;` |
|  19713 |  370 | `	sxi32 rc = SXRET_OK;` |
|      - |  371 | `	/* Reset the working buffer */` |
|  19713 |  372 | `	SyBlobReset(pWorker);` |
|      - |  373 | `	/* Peek the processed file if available */` |
|  19713 |  374 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  19713 |  375 | `	VmDiagnosticHeader(pWorker,iErr,pFuncName);` |
|      - |  376 | `	/* Format the raw message */` |
|  19713 |  377 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|  19713 |  378 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      - |  379 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|      - |  380 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|      - |  381 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|      - |  382 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|  19713 |  383 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|      - |  384 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|      - |  385 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|  19625 |  386 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|  19625 |  387 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|  19597 |  388 | `			SyBlobRelease(&sMsg);` |
|  19597 |  389 | `			return SXRET_OK;` |
|      - |  390 | `		}` |
|     31 |  391 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     31 |  392 | `		VmDiagnosticLocation(pWorker,pFile,pVm->nCurLine);` |
|     31 |  393 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|     14 |  394 | `	}` |
|    121 |  395 | `	SyBlobRelease(&sMsg);` |
|    121 |  396 | `	return rc;` |
|   9859 |  397 | `}` |
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
| 366365 |  950 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|      5 |  951 | `{` |
|      - |  952 | `	SyHashEntry *pSlot;` |
|      - |  953 | `	VmClassAttr *pVmAttr;` |
| 366370 |  954 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
| 279436 |  955 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|      - |  956 | `	}` |
|  86937 |  957 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  86937 |  958 | `	if( pSlot == 0 ){` |
|  86851 |  959 | `		return SXRET_OK; /* Not a typed slot */` |
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
| 183289 |  971 | `}` |
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
|      3 |  988 | `{` |
|      - |  989 | `	const char *z, *zEnd, *zTail;` |
|      - |  990 | `	sxu32 n;` |
|     21 |  991 | `	sxu8 bReal = 0;` |
|      - |  992 | `	sxi32 rc;` |
|     21 |  993 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
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
|     12 | 1006 | `}` |
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
|     15 | 1132 | `					bAll = 0;` |
|     15 | 1133 | `					break;` |
|      - | 1134 | `				}` |
|     28 | 1135 | `			}` |
|     36 | 1136 | `			if( bAll ) return SXRET_OK;` |
|      9 | 1137 | `		}` |
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
|    101 | 1185 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|     47 | 1186 | `		if( bHasInt ) return SXRET_OK;` |
|      1 | 1187 | `	}` |
|     61 | 1188 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      5 | 1189 | `		if( bHasFloat ) return SXRET_OK;` |
|    ! 0 | 1190 | `	}` |
|     57 | 1191 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     54 | 1192 | `		if( bHasString ) return SXRET_OK;` |
|      8 | 1193 | `	}` |
|     21 | 1194 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 1195 | `		if( bHasBool ) return SXRET_OK;` |
|    ! 0 | 1196 | `	}` |
|     21 | 1197 | `	if( bStrict ){` |
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
|     21 | 1209 | `		int kind = VmStringNumericKind(pValue);` |
|     21 | 1210 | `		if( bHasInt ){` |
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
|     13 | 1229 | `		if( bHasFloat ){` |
|     10 | 1230 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|    ! 0 | 1231 | `				PH7_MemObjToReal(pValue);` |
|    ! 0 | 1232 | `				return SXRET_OK;` |
|      - | 1233 | `			}` |
|     10 | 1234 | `			if( kind == 1 \|\| kind == 2 ){` |
|      7 | 1235 | `				PH7_MemObjToReal(pValue);` |
|      7 | 1236 | `				return SXRET_OK;` |
|      - | 1237 | `			}` |
|      1 | 1238 | `		}` |
|      6 | 1239 | `		if( bHasString ){` |
|    ! 0 | 1240 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    ! 0 | 1241 | `				PH7_MemObjToString(pValue);` |
|    ! 0 | 1242 | `				return SXRET_OK;` |
|      - | 1243 | `			}` |
|    ! 0 | 1244 | `		}` |
|      6 | 1245 | `		if( bHasBool ){` |
|    ! 0 | 1246 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|    ! 0 | 1247 | `				PH7_MemObjToBool(pValue);` |
|    ! 0 | 1248 | `				return SXRET_OK;` |
|      - | 1249 | `			}` |
|    ! 0 | 1250 | `		}` |
|      - | 1251 | `	}` |
|      6 | 1252 | `	return SXERR_INVALID;` |
|     93 | 1253 | `}` |
|      - | 1254 |  |
|      - | 1255 | `/*` |
|      - | 1256 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|      - | 1257 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|      - | 1258 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|      - | 1259 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|      - | 1260 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|      - | 1261 | ` */` |
|    106 | 1262 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|      3 | 1263 | `{` |
|      - | 1264 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|      - | 1265 | `	 * null value satisfies it (and a null value matches via the flag test` |
|      - | 1266 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|      - | 1267 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|      - | 1268 | `	 * silently swallow any argument. */` |
|    109 | 1269 | `	if( nType == MEMOBJ_NULL ){` |
|      3 | 1270 | `		return SXERR_INVALID;` |
|      - | 1271 | `	}` |
|    107 | 1272 | `	if( bStrict ){` |
|      - | 1273 | `		/* Only int -> float widening is allowed implicitly. */` |
|     21 | 1274 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|      3 | 1275 | `			PH7_MemObjToReal(pVal);` |
|      3 | 1276 | `			return SXRET_OK;` |
|      - | 1277 | `		}` |
|     19 | 1278 | `		return SXERR_INVALID;` |
|      - | 1279 | `	}` |
|      - | 1280 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|      - | 1281 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|      - | 1282 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|      - | 1283 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|      - | 1284 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|      - | 1285 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|      - | 1286 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     88 | 1287 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     20 | 1288 | `		return SXERR_INVALID;` |
|      - | 1289 | `	}` |
|     68 | 1290 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|     62 | 1291 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|     62 | 1292 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|     18 | 1293 | `		return SXERR_INVALID;` |
|      - | 1294 | `	}` |
|     54 | 1295 | `	if( nType == MEMOBJ_INT && pVal->pVm ){` |
|      - | 1296 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion` |
|      - | 1297 | `		 * (typed params, returns, typed property stores all funnel through here);` |
|      - | 1298 | `		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly` |
|      - | 1299 | `		 * like the null / non-numeric-string cases above. An INTEGRAL float loses` |
|      - | 1300 | `		 * nothing and coerces normally. */` |
|     34 | 1301 | `		if( pVal->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 1302 | `			ph7_real r = pVal->rVal;` |
|    ! 0 | 1303 | `			if( r != (ph7_real)(sxi64)r ){` |
|    ! 0 | 1304 | `				return SXERR_INVALID;` |
|    ! 0 | 1305 | `			}` |
|     34 | 1306 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      - | 1307 | `			SyString sStr;` |
|      - | 1308 | `			ph7_value sProbe;` |
|      - | 1309 | `			int bLossy;` |
|     30 | 1310 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     30 | 1311 | `			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|     30 | 1312 | `			PH7_MemObjToNumeric(&sProbe);` |
|     30 | 1313 | `			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;` |
|     30 | 1314 | `			PH7_MemObjRelease(&sProbe);` |
|     30 | 1315 | `			if( bLossy ){` |
|    ! 0 | 1316 | `				return SXERR_INVALID;` |
|      - | 1317 | `			}` |
|     14 | 1318 | `		}` |
|     16 | 1319 | `	}` |
|      - | 1320 | `	{` |
|     54 | 1321 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|     54 | 1322 | `		if( xCast ) xCast(pVal);` |
|      - | 1323 | `	}` |
|     54 | 1324 | `	return SXRET_OK;` |
|     56 | 1325 | `}` |
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
|     54 | 1336 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|      4 | 1337 | `{` |
|     58 | 1338 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     58 | 1339 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     58 | 1340 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     58 | 1341 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     58 | 1342 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|     27 | 1343 | `		}` |
|     58 | 1344 | `		zBuf[nCopy] = 0;` |
|     58 | 1345 | `		return zBuf;` |
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
|     31 | 1356 | `}` |
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
|  13790 | 1370 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|      5 | 1371 | `{` |
|      - | 1372 | `	SyHashEntry *pSlot;` |
|      - | 1373 | `	VmClassAttr *pVmAttr;` |
|      - | 1374 | `	ph7_class_attr *pAttr;` |
|  13795 | 1375 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  13795 | 1376 | `	if( pSlot == 0 ){` |
|  13307 | 1377 | `		return SXRET_OK; /* Not a typed slot */` |
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
|     34 | 1436 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     34 | 1437 | `			return SXRET_OK;` |
|      - | 1438 | `		}` |
|     16 | 1439 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      - | 1440 | `			char zBuf[128];` |
|     12 | 1441 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      3 | 1442 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 1443 | `		}` |
|      8 | 1444 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
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
|   6900 | 1537 | `}` |
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
|      5 | 1739 | `{` |
|      - | 1740 | `	va_list ap;` |
|      - | 1741 | `	sxi32 rc;` |
|     53 | 1742 | `	va_start(ap,zFormat);` |
|     53 | 1743 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     53 | 1744 | `	va_end(ap);` |
|     53 | 1745 | `	return rc;` |
|      5 | 1746 | `}` |
|      - | 1747 | `static int VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut);` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * Throw a TypeError exception from within the VM execution loop.` |
|      - | 1750 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|      - | 1751 | ` */` |
|    120 | 1752 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|      5 | 1753 | `{` |
|      - | 1754 | `	ph7_class *pClass;` |
|      - | 1755 | `	ph7_class_instance *pThis;` |
|      - | 1756 | `	ph7_class_method *pCons;` |
|      - | 1757 | `	ph7_value sArg;` |
|      - | 1758 | `	ph7_value *apArg[1];` |
|      - | 1759 | `	SyBlob sMsg;` |
|      - | 1760 | `	SyString sMsgStr;` |
|    125 | 1761 | `	SyString *pFuncName = &pCallee->sName;` |
|      - | 1762 | `	VmFrame *pFrame;` |
|      - | 1763 | `	sxi32 rc;` |
|    125 | 1764 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|    125 | 1765 | `	if( pClass == 0 ){` |
|    ! 0 | 1766 | `		return PH7_ABORT;` |
|      - | 1767 | `	}` |
|    125 | 1768 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    125 | 1769 | `	if( pThis == 0 ){` |
|    ! 0 | 1770 | `		return PH7_ABORT;` |
|      - | 1771 | `	}` |
|    125 | 1772 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      - | 1773 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|      - | 1774 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|    125 | 1775 | `	if( pOwnerClass ){` |
|     23 | 1776 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|     10 | 1777 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|     13 | 1778 | `	}else{` |
|      - | 1779 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|    105 | 1780 | `		const char *zShow = 0;` |
|    105 | 1781 | `		int nShow = VmFuncDisplayName(pVm,pCallee,&zShow);` |
|    105 | 1782 | `		SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|     50 | 1783 | `			nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|      - | 1784 | `	}` |
|      - | 1785 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|      - | 1786 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|      - | 1787 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|    125 | 1788 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|    117 | 1789 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    117 | 1790 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|    117 | 1791 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|     56 | 1792 | `		}` |
|     56 | 1793 | `	}` |
|    125 | 1794 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    125 | 1795 | `	if( pCons ){` |
|    125 | 1796 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    125 | 1797 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    125 | 1798 | `		apArg[0] = &sArg;` |
|    125 | 1799 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|    125 | 1800 | `		PH7_MemObjRelease(&sArg);` |
|     60 | 1801 | `	}` |
|    125 | 1802 | `	SyBlobRelease(&sMsg);` |
|    125 | 1803 | `	pFrame = pVm->pFrame;` |
|    125 | 1804 | `	if( pFrame ){` |
|    125 | 1805 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    125 | 1806 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     60 | 1807 | `	}` |
|    125 | 1808 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    125 | 1809 | `	PH7_ClassInstanceUnref(pThis);` |
|    125 | 1810 | `	if( rc == SXERR_ABORT ){` |
|      6 | 1811 | `		return PH7_ABORT;` |
|      - | 1812 | `	}` |
|    121 | 1813 | `	return PH7_EXCEPTION;` |
|     65 | 1814 | `}` |
|      - | 1815 | `/*` |
|      - | 1816 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|      - | 1817 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|      - | 1818 | ` * before a required parameter as implicitly required), excluding a trailing` |
|      - | 1819 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|      - | 1820 | ` * pick php's wording — "exactly N expected" when required == total,` |
|      - | 1821 | ` * "at least N" when trailing optionals exist.` |
|      - | 1822 | ` */` |
|   4692 | 1823 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|      5 | 1824 | `{` |
|   4697 | 1825 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|   4697 | 1826 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|   4697 | 1827 | `	sxu32 nRequired = 0;` |
|      - | 1828 | `	sxu32 n;` |
|   4697 | 1829 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|    297 | 1830 | `		nFormal--;` |
|    146 | 1831 | `	}` |
|  15375 | 1832 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|  10683 | 1833 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    823 | 1834 | `			nRequired = n + 1;` |
|    409 | 1835 | `		}` |
|   5344 | 1836 | `	}` |
|   4697 | 1837 | `	*pnNonVariadic = nFormal;` |
|   4697 | 1838 | `	return nRequired;` |
|      5 | 1839 | `}` |
|      - | 1840 | `/*` |
|      - | 1841 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|      - | 1842 | ` * with too few arguments:` |
|      - | 1843 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|      - | 1844 | ` *   {exactly\|at least} M expected` |
|      - | 1845 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|      - | 1846 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|      - | 1847 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|      - | 1848 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|      - | 1849 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|      - | 1850 | ` */` |
|     26 | 1851 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1852 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|      3 | 1853 | `{` |
|      - | 1854 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|      - | 1855 | `	SyBlob sMsg;` |
|     29 | 1856 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     29 | 1857 | `	if( pOwnerClass ){` |
|      5 | 1858 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|      2 | 1859 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|      3 | 1860 | `	}else{` |
|     25 | 1861 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|      - | 1862 | `	}` |
|     29 | 1863 | `	if( bCallSite ){` |
|     27 | 1864 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|     27 | 1865 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|     12 | 1866 | `	}` |
|     29 | 1867 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|     13 | 1868 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|      - | 1869 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     29 | 1870 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      3 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|      - | 1874 | ` * called with too few arguments, in php's ZPP wording:` |
|      - | 1875 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|      - | 1876 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|      - | 1877 | ` * pluralized on the expected count). Hosted builtin FUNCTIONS are not routed` |
|      - | 1878 | ` * here: their PHL signatures don't always mirror php's true arity (e.g.` |
|      - | 1879 | ` * array_unshift is (&$pArray)+func_get_args for php's (array, ...$values)),` |
|      - | 1880 | ` * so their in-body self-checks own the message.` |
|      - | 1881 | ` */` |
|      2 | 1882 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1883 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic)` |
|      1 | 1884 | `{` |
|      - | 1885 | `	SyBlob sMsg;` |
|      3 | 1886 | `	const char *zKind = (nRequired >= nNonVariadic) ? "exactly" : "at least";` |
|      3 | 1887 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|      3 | 1888 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1889 | `	if( pOwnerClass ){` |
|      3 | 1890 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      1 | 1891 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      2 | 1892 | `	}else{` |
|    ! 0 | 1893 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|    ! 0 | 1894 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      - | 1895 | `	}` |
|      - | 1896 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      3 | 1897 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      1 | 1898 | `}` |
|      - | 1899 | `/*` |
|      - | 1900 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|      - | 1901 | ` * named or positional argument resolved to:` |
|      - | 1902 | ` *   C::f(): Argument #N ($x) not passed` |
|      - | 1903 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|      - | 1904 | ` */` |
|      2 | 1905 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|      - | 1906 | `	sxu32 nArg,SyString *pArgName)` |
|      1 | 1907 | `{` |
|      - | 1908 | `	SyBlob sMsg;` |
|      3 | 1909 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1910 | `	if( pOwnerClass ){` |
|    ! 0 | 1911 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|    ! 0 | 1912 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|    ! 0 | 1913 | `	}else{` |
|      3 | 1914 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|      - | 1915 | `	}` |
|      - | 1916 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      3 | 1917 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|      1 | 1918 | `}` |
|      - | 1919 | `/*` |
|      - | 1920 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|      - | 1921 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|      - | 1922 | ` */` |
|      - | 1923 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|      - | 1924 | ` * The message is copied into the instance by __construct, so the caller owns` |
|      - | 1925 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|     24 | 1926 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|      5 | 1927 | `{` |
|      - | 1928 | `	ph7_class *pClass;` |
|      - | 1929 | `	ph7_class_instance *pThis;` |
|      - | 1930 | `	ph7_class_method *pCons;` |
|      - | 1931 | `	ph7_value sArg;` |
|      - | 1932 | `	ph7_value *apArg[1];` |
|      - | 1933 | `	SyString sMsgStr;` |
|      - | 1934 | `	VmFrame *pFrame;` |
|      - | 1935 | `	sxi32 rc;` |
|     29 | 1936 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     29 | 1937 | `	if( pClass == 0 ){` |
|    ! 0 | 1938 | `		return PH7_ABORT;` |
|      - | 1939 | `	}` |
|     29 | 1940 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     29 | 1941 | `	if( pThis == 0 ){` |
|    ! 0 | 1942 | `		return PH7_ABORT;` |
|      - | 1943 | `	}` |
|     29 | 1944 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     29 | 1945 | `	if( pCons ){` |
|     29 | 1946 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|     29 | 1947 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     29 | 1948 | `		apArg[0] = &sArg;` |
|     29 | 1949 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     29 | 1950 | `		PH7_MemObjRelease(&sArg);` |
|     12 | 1951 | `	}` |
|     29 | 1952 | `	pFrame = pVm->pFrame;` |
|     29 | 1953 | `	if( pFrame ){` |
|     29 | 1954 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     29 | 1955 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     12 | 1956 | `	}` |
|     29 | 1957 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     29 | 1958 | `	PH7_ClassInstanceUnref(pThis);` |
|     29 | 1959 | `	if( rc == SXERR_ABORT ){` |
|      9 | 1960 | `		return PH7_ABORT;` |
|      - | 1961 | `	}` |
|     21 | 1962 | `	return PH7_EXCEPTION;` |
|     17 | 1963 | `}` |
|     22 | 1964 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|      5 | 1965 | `{` |
|      - | 1966 | `	SyBlob sMsg;` |
|      - | 1967 | `	sxi32 rc;` |
|     27 | 1968 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     27 | 1969 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|     11 | 1970 | `		pFuncName,zExpected,zGiven);` |
|     27 | 1971 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|     27 | 1972 | `	SyBlobRelease(&sMsg);` |
|     27 | 1973 | `	return rc;` |
|      5 | 1974 | `}` |
|      - | 1975 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|      - | 1976 | `` * explicit `return` at compile time, so this fires only for an implicit return. */`` |
|      2 | 1977 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,SyString *pFuncName)` |
|      1 | 1978 | `{` |
|      - | 1979 | `	SyBlob sMsg;` |
|      - | 1980 | `	sxi32 rc;` |
|      3 | 1981 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      3 | 1982 | `	SyBlobFormat(&sMsg,"%z(): never-returning function must not implicitly return",` |
|      1 | 1983 | `		pFuncName);` |
|      3 | 1984 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      3 | 1985 | `	SyBlobRelease(&sMsg);` |
|      3 | 1986 | `	return rc;` |
|      1 | 1987 | `}` |
|      - | 1988 | `/*` |
|      - | 1989 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|      - | 1990 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|      - | 1991 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|      - | 1992 | ` */` |
|    124 | 1993 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|      3 | 1994 | `{` |
|    127 | 1995 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     24 | 1996 | `		return pVal->x.iVal ? "true" : "false";` |
|      - | 1997 | `	}` |
|    105 | 1998 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     21 | 1999 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     21 | 2000 | `		if( pThis && pThis->pClass ){` |
|     21 | 2001 | `			SyString *pName = &pThis->pClass->sName;` |
|     21 | 2002 | `			sxu32 n = pName->nByte;` |
|     21 | 2003 | `			if( n >= nBuf ){` |
|    ! 0 | 2004 | `				n = nBuf - 1;` |
|    ! 0 | 2005 | `			}` |
|     21 | 2006 | `			SyMemcpy(pName->zString,zBuf,n);` |
|     21 | 2007 | `			zBuf[n] = 0;` |
|     21 | 2008 | `			return zBuf;` |
|      - | 2009 | `		}` |
|    ! 0 | 2010 | `		return "object";` |
|      - | 2011 | `	}` |
|     87 | 2012 | `	return ph7_type_name(pVal);` |
|     65 | 2013 | `}` |
|      - | 2014 | `/*` |
|      - | 2015 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|      - | 2016 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|      - | 2017 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|      - | 2018 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|      - | 2019 | ` */` |
|     18 | 2020 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|      3 | 2021 | `{` |
|      - | 2022 | `	ph7_class *pClass;` |
|      - | 2023 | `	ph7_class_instance *pThis;` |
|      - | 2024 | `	ph7_class_method *pCons;` |
|      - | 2025 | `	ph7_value sArg;` |
|      - | 2026 | `	ph7_value *apArg[1];` |
|      - | 2027 | `	SyBlob sMsg;` |
|      - | 2028 | `	SyString sMsgStr;` |
|      - | 2029 | `	VmFrame *pFrame;` |
|      - | 2030 | `	sxi32 rc;` |
|     21 | 2031 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|      - | 2032 | `	char zNameBuf[64];` |
|     21 | 2033 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|     21 | 2034 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|     21 | 2035 | `	if( pClass == 0 ){` |
|    ! 0 | 2036 | `		return PH7_ABORT;` |
|      - | 2037 | `	}` |
|     21 | 2038 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     21 | 2039 | `	if( pThis == 0 ){` |
|    ! 0 | 2040 | `		return PH7_ABORT;` |
|      - | 2041 | `	}` |
|     21 | 2042 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     21 | 2043 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|     21 | 2044 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     21 | 2045 | `	if( pCons ){` |
|     21 | 2046 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     21 | 2047 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     21 | 2048 | `		apArg[0] = &sArg;` |
|     21 | 2049 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     21 | 2050 | `		PH7_MemObjRelease(&sArg);` |
|      9 | 2051 | `	}` |
|     21 | 2052 | `	SyBlobRelease(&sMsg);` |
|     21 | 2053 | `	pFrame = pVm->pFrame;` |
|     21 | 2054 | `	if( pFrame ){` |
|     21 | 2055 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     21 | 2056 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      9 | 2057 | `	}` |
|     21 | 2058 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     21 | 2059 | `	PH7_ClassInstanceUnref(pThis);` |
|     21 | 2060 | `	if( rc == SXERR_ABORT ){` |
|    ! 0 | 2061 | `		return PH7_ABORT;` |
|      - | 2062 | `	}` |
|     21 | 2063 | `	return PH7_EXCEPTION;` |
|     12 | 2064 | `}` |
|      - | 2065 | `/*` |
|      - | 2066 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|      - | 2067 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|      - | 2068 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|      - | 2069 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|      - | 2070 | ` */` |
|      - | 2071 | `/*` |
|      - | 2072 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|      - | 2073 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|      - | 2074 | ` * null SyString yields an empty C string. Returns zBuf.` |
|      - | 2075 | ` */` |
|    118 | 2076 | `PH7_PRIVATE const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|      5 | 2077 | `{` |
|      - | 2078 | `	sxu32 nCopy;` |
|    123 | 2079 | `	if( nBuf == 0 ) return "";` |
|    123 | 2080 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|    ! 0 | 2081 | `		zBuf[0] = 0;` |
|    ! 0 | 2082 | `		return zBuf;` |
|      - | 2083 | `	}` |
|    123 | 2084 | `	nCopy = SyStringLength(pStr);` |
|    123 | 2085 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|    123 | 2086 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|    123 | 2087 | `	zBuf[nCopy] = 0;` |
|    123 | 2088 | `	return zBuf;` |
|     64 | 2089 | `}` |
|      - | 2090 |  |
|      - | 2091 | `/*` |
|      - | 2092 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|      - | 2093 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|      - | 2094 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|      - | 2095 | ` * type field.` |
|      - | 2096 | ` */` |
| 122337 | 2097 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|      5 | 2098 | `{` |
| 122342 | 2099 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|      5 | 2100 | `}` |
|   9200 | 2101 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|      5 | 2102 | `{` |
|   9205 | 2103 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|   9205 | 2104 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|      - | 2105 | `	const char *zGiven;` |
|      - | 2106 | `	char zBuf[128];` |
|      - | 2107 | `	char zTypeBuf[128];` |
|      - | 2108 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|   9205 | 2109 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|    ! 0 | 2110 | `		return SXRET_OK;` |
|      - | 2111 | `	}` |
|      - | 2112 | `	/* never return type: the function must not return at all. An explicit` |
|      - | 2113 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|      - | 2114 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|      - | 2115 | `	 * the call site). */` |
|   9205 | 2116 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|      3 | 2117 | `		return VmThrowNeverReturnError(pVm,&pFunc->sName);` |
|      - | 2118 | `	}` |
|      - | 2119 | `	/* void return type: the function must not produce a value. */` |
|   9203 | 2120 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|    121 | 2121 | `		if( pValue == 0 ){` |
|    119 | 2122 | `			return SXRET_OK;` |
|      - | 2123 | `		}` |
|      - | 2124 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|      - | 2125 | `		 * still counts as "returned a value" here. */` |
|      3 | 2126 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      3 | 2127 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|      - | 2128 | `	}` |
|      - | 2129 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|      - | 2130 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|      - | 2131 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|   9087 | 2132 | `	if( pValue == 0 ){` |
|    ! 0 | 2133 | `		const char *zExpected = "value";` |
|    ! 0 | 2134 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|    ! 0 | 2135 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|    ! 0 | 2136 | `		}` |
|    ! 0 | 2137 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|      - | 2138 | `	}` |
|      - | 2139 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|      - | 2140 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|      - | 2141 | `	 * matching how every other typed return reports a missing value.) */` |
|   9087 | 2142 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|      5 | 2143 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|      3 | 2144 | `			return SXRET_OK;` |
|      - | 2145 | `		}` |
|      4 | 2146 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|      1 | 2147 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 2148 | `	}` |
|      - | 2149 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|      - | 2150 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|      - | 2151 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|   9083 | 2152 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|     19 | 2153 | `		return SXRET_OK;` |
|      - | 2154 | `	}` |
|      - | 2155 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|      - | 2156 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|      - | 2157 | `	 * Check by value before the real-class instanceof branch below. */` |
|   9069 | 2158 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|    141 | 2159 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|    141 | 2160 | `		if( rcPseudo == 1 ){` |
|     86 | 2161 | `			return SXRET_OK;` |
|      - | 2162 | `		}` |
|     58 | 2163 | `		if( rcPseudo == 0 ){` |
|      9 | 2164 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      4 | 2165 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|      2 | 2166 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|      - | 2167 | `		}` |
|      - | 2168 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|     25 | 2169 | `	}` |
|      - | 2170 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|      - | 2171 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|      - | 2172 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|   8983 | 2173 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      - | 2174 | `		sxi32 rcU;` |
|     25 | 2175 | `		const char *zExpected = "union";` |
|     25 | 2176 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|     25 | 2177 | `		if( rcU == SXRET_OK ){` |
|     21 | 2178 | `			return SXRET_OK;` |
|      - | 2179 | `		}` |
|      5 | 2180 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      3 | 2181 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      4 | 2182 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 2183 | `			zGiven = "null";` |
|    ! 0 | 2184 | `		}else{` |
|      3 | 2185 | `			zGiven = ph7_type_name(pValue);` |
|      - | 2186 | `		}` |
|      5 | 2187 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      5 | 2188 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      2 | 2189 | `		}` |
|      5 | 2190 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2191 | `	}` |
|      - | 2192 | `	/* Class return type — instanceof check. The class name is a length-` |
|      - | 2193 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|      - | 2194 | `	 * it into the TypeError message. */` |
|   8961 | 2195 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     54 | 2196 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      - | 2197 | `		const char *zExpected;` |
|     54 | 2198 | `		ph7_class *pExpected = VmResolveTypeClass(pVm,pClassName,VmCurrentSelf(pVm));` |
|     54 | 2199 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|     54 | 2200 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      5 | 2201 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      5 | 2202 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2203 | `		}` |
|     50 | 2204 | `		if( pExpected ){` |
|     44 | 2205 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     44 | 2206 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      3 | 2207 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      3 | 2208 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|      - | 2209 | `			}` |
|     19 | 2210 | `		}` |
|     48 | 2211 | `		return SXRET_OK;` |
|      - | 2212 | `	}` |
|      - | 2213 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|      - | 2214 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|      - | 2215 | `	 * non-nullable scalar return — a TypeError. */` |
|   8911 | 2216 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 2217 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2218 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      - | 2219 | `			"null");` |
|      - | 2220 | `	}` |
|      - | 2221 | `	/* Exact match? Done. */` |
|   8911 | 2222 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|   8905 | 2223 | `		return SXRET_OK;` |
|      - | 2224 | `	}` |
|      - | 2225 | `	/* Object->scalar is never compatible. */` |
|      9 | 2226 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 2227 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|    ! 0 | 2228 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2229 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|    ! 0 | 2230 | `			zGiven);` |
|      - | 2231 | `	}` |
|      - | 2232 | `	/* Array <-> scalar is never compatible. */` |
|      9 | 2233 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|    ! 0 | 2234 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|    ! 0 | 2235 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|    ! 0 | 2236 | `			ph7_type_name(pValue));` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|      - | 2239 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|      - | 2240 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|      - | 2241 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|      6 | 2242 | `	if( !bStrict` |
|      5 | 2243 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|      4 | 2244 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|      7 | 2245 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|      4 | 2246 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      1 | 2247 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      - | 2248 | `			"string");` |
|      - | 2249 | `	}` |
|      6 | 2250 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|      3 | 2251 | `		return SXRET_OK;` |
|      - | 2252 | `	}` |
|      4 | 2253 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      1 | 2254 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      1 | 2255 | `		ph7_type_name(pValue));` |
|   4605 | 2256 | `}` |
|      - | 2257 | `/*` |
|      - | 2258 | ` * Report a fatal named-argument error.` |
|      - | 2259 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|      - | 2260 | ` */` |
|     12 | 2261 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|      3 | 2262 | `{` |
|      - | 2263 | `	SyBlob sMsg;` |
|      - | 2264 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|      - | 2265 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|      - | 2266 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|      - | 2267 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|      - | 2268 | `	 * unconditional fatal even inside try/catch. */` |
|     15 | 2269 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     15 | 2270 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|     15 | 2271 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      3 | 2272 | `}` |
|      - | 2273 | `/*` |
|      - | 2274 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|      - | 2275 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|      - | 2276 | ` * information.` |
|      - | 2277 | ` * ------------------------------------` |
|      - | 2278 | ` * Simple boring wrapper function.` |
|      - | 2279 | ` * ------------------------------------` |
|      - | 2280 | ` */` |
|  19660 | 2281 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|      5 | 2282 | `{` |
|      - | 2283 | `	sxi32 rc;` |
|  19665 | 2284 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|  19665 | 2285 | `	return rc;` |
|      5 | 2286 | `}` |
|      - | 2287 | `/*` |
|      - | 2288 | ` * Resolve function context from the current frame.` |
|      - | 2289 | ` */` |
|      - | 2290 | `/*` |
|      - | 2291 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|      - | 2292 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|      - | 2293 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|      - | 2294 | ` * straight at the function's own name otherwise.` |
|      - | 2295 | ` */` |
|   1838 | 2296 | `static int VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|      5 | 2297 | `{` |
|   1843 | 2298 | `	const char *zName = pFunc->sName.zString;` |
|   1843 | 2299 | `	int nName = (int)pFunc->sName.nByte;` |
|   2043 | 2300 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   2201 | 2301 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|   1843 | 2302 | `	if( bClosure ){` |
|      - | 2303 | `		int n;` |
|    331 | 2304 | `		if( pFunc->sFile.nByte > 0 ){` |
|    494 | 2305 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|    326 | 2306 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|    168 | 2307 | `		}else{` |
|    ! 0 | 2308 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|      - | 2309 | `		}` |
|    331 | 2310 | `		*pzOut = pVm->zDisplayName;` |
|    331 | 2311 | `		return n;` |
|      - | 2312 | `	}` |
|   1517 | 2313 | `	*pzOut = zName;` |
|   1517 | 2314 | `	return nName;` |
|    924 | 2315 | `}` |
|   1090 | 2316 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|      4 | 2317 | `{` |
|      - | 2318 | `	VmFrame *pFrame;` |
|      - | 2319 | `	ph7_vm_func *pFunc;` |
|   1094 | 2320 | `	*pzFuncName = 0;` |
|   1094 | 2321 | `	*pnFuncLen = 0;` |
|   1094 | 2322 | `	pFrame = pVm->pFrame;` |
|   1094 | 2323 | `	if( pFrame == 0 ){` |
|    ! 0 | 2324 | `		return;` |
|      - | 2325 | `	}` |
|   1094 | 2326 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|   1094 | 2327 | `	if( pFrame->pParent == 0 ){` |
|   1060 | 2328 | `		return;` |
|      - | 2329 | `	}` |
|     38 | 2330 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     38 | 2331 | `	if( pFunc == 0 ){` |
|    ! 0 | 2332 | `		return;` |
|      - | 2333 | `	}` |
|     38 | 2334 | `	*pnFuncLen = VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|    549 | 2335 | `}` |
|      - | 2336 | `/*` |
|      - | 2337 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|      - | 2338 | ` *` |
|      - | 2339 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|      - | 2340 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|      - | 2341 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|      - | 2342 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|      - | 2343 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|      - | 2344 | ` *             trailer.` |
|      - | 2345 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|      - | 2346 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|      - | 2347 | ` * call; this routine only appends.` |
|      - | 2348 | ` */` |
|    566 | 2349 | `static void VmRenderUncaughtEntry(` |
|      - | 2350 | `	ph7_vm *pVm,SyBlob *pOut,` |
|      - | 2351 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|      - | 2352 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|      - | 2353 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|      - | 2354 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|      4 | 2355 | `{` |
|      - | 2356 | `	SyString *pFile;` |
|    570 | 2357 | `	if( nThrowLine == 0 ){` |
|      5 | 2358 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|      2 | 2359 | `	}` |
|    570 | 2360 | `	if( nCallLine == 0 ){` |
|    570 | 2361 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|    570 | 2362 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|    283 | 2363 | `	}` |
|    570 | 2364 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|    ! 0 | 2365 | `		zClass = "Exception";` |
|    ! 0 | 2366 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|    ! 0 | 2367 | `	}` |
|    570 | 2368 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|    538 | 2369 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|    267 | 2370 | `	}` |
|    570 | 2371 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    570 | 2372 | `	if( bFirst ){` |
|    564 | 2373 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|    284 | 2374 | `	}else{` |
|      8 | 2375 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|      - | 2376 | `	}` |
|    570 | 2377 | `	SyBlobAppend(pOut,zClass,nClass);` |
|    570 | 2378 | `	if( zMsg && nMsg > 0 ){` |
|    570 | 2379 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|    570 | 2380 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|    283 | 2381 | `	}` |
|    570 | 2382 | `	if( pFile ){` |
|    570 | 2383 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|    283 | 2384 | `	}` |
|    570 | 2385 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|    570 | 2386 | `	if( pFile ){` |
|    570 | 2387 | `		SyBlobAppend(pOut,"#0 ",sizeof("#0 ")-1);` |
|    570 | 2388 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|    570 | 2389 | `		if( zFuncName && nFuncLen > 0 ){` |
|      - | 2390 | `			/* php reports a trace frame at its CALL SITE, not at the line running` |
|      - | 2391 | `			 * inside it. */` |
|     38 | 2392 | `			SyBlobFormat(pOut,"(%u): %.*s()\n",nCallLine,nFuncLen,zFuncName);` |
|     21 | 2393 | `		}else{` |
|    536 | 2394 | `			SyBlobFormat(pOut,"(%u): {main}\n",nCallLine);` |
|      4 | 2395 | `		}` |
|    283 | 2396 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|    ! 0 | 2397 | `		SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|    ! 0 | 2398 | `	}else{` |
|    ! 0 | 2399 | `		SyBlobAppend(pOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|      - | 2400 | `	}` |
|    570 | 2401 | `	SyBlobAppend(pOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|    570 | 2402 | `	if( bLast && pFile ){` |
|    564 | 2403 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|    564 | 2404 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|    280 | 2405 | `	}` |
|    570 | 2406 | `}` |
|      - | 2407 | `/*` |
|      - | 2408 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|      - | 2409 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|      - | 2410 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|      - | 2411 | ` */` |
|      4 | 2412 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|      1 | 2413 | `{` |
|      - | 2414 | `	SyBlob sOut;` |
|      - | 2415 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|      - | 2416 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|      5 | 2417 | `	pVm->iExitStatus = 255;` |
|      5 | 2418 | `	if( !pVm->bErrReport ){` |
|    ! 0 | 2419 | `		return PH7_OK;` |
|      - | 2420 | `	}` |
|      5 | 2421 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      5 | 2422 | `	VmRenderUncaughtEntry(pVm,&sOut,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|      5 | 2423 | `	VmCallErrorHandler(pVm,&sOut);` |
|      5 | 2424 | `	SyBlobRelease(&sOut);` |
|      5 | 2425 | `	return PH7_ABORT;` |
|      3 | 2426 | `}` |
|      - | 2427 | `/*` |
|      - | 2428 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|      - | 2429 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|      - | 2430 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|      - | 2431 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|      - | 2432 | ` */` |
|      - | 2433 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|    562 | 2434 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|      4 | 2435 | `{` |
|      - | 2436 | `	ph7_value *pValue;` |
|      - | 2437 | `	ph7_class_instance *pPrev;` |
|      - | 2438 | `	ph7_class *pThrowable;` |
|    566 | 2439 | `	if( pThis == 0 ){` |
|    ! 0 | 2440 | `		return 0;` |
|      - | 2441 | `	}` |
|    566 | 2442 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|    566 | 2443 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    560 | 2444 | `		return 0;` |
|      - | 2445 | `	}` |
|      8 | 2446 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|      - | 2447 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|      - | 2448 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|      - | 2449 | `	 * never renders a stray object as an exception entry. */` |
|      8 | 2450 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      8 | 2451 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|    ! 0 | 2452 | `		return 0;` |
|      - | 2453 | `	}` |
|      8 | 2454 | `	return pPrev;` |
|    285 | 2455 | `}` |
|      - | 2456 | `/*` |
|      - | 2457 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|      - | 2458 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|      - | 2459 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|      - | 2460 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|      - | 2461 | ` */` |
|     16 | 2462 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|      2 | 2463 | `{` |
|      - | 2464 | `	ph7_value *pValue;` |
|     18 | 2465 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|    ! 0 | 2466 | `		return;` |
|      - | 2467 | `	}` |
|     18 | 2468 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     18 | 2469 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      3 | 2470 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|      - | 2471 | `	}` |
|     16 | 2472 | `	pPrev->iRef++;` |
|      - | 2473 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|      - | 2474 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|     16 | 2475 | `	PH7_MemObjRelease(pValue);` |
|     16 | 2476 | `	pValue->x.pOther = pPrev;` |
|     16 | 2477 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|     10 | 2478 | `}` |
|      - | 2479 | `/*` |
|      - | 2480 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|      - | 2481 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|      - | 2482 | ` * absent or yields an empty string.` |
|      - | 2483 | ` */` |
|      - | 2484 | `/*` |
|      - | 2485 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|      - | 2486 | ` * 0 when the class exposes no getLine().` |
|      - | 2487 | ` */` |
|    562 | 2488 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      4 | 2489 | `{` |
|      - | 2490 | `	ph7_class_method *pGetLine;` |
|      - | 2491 | `	ph7_value sLine;` |
|    566 | 2492 | `	sxu32 nLine = 0;` |
|    566 | 2493 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|    566 | 2494 | `	if( pGetLine == 0 ){` |
|    ! 0 | 2495 | `		return 0;` |
|      - | 2496 | `	}` |
|    566 | 2497 | `	PH7_MemObjInit(pVm,&sLine);` |
|    566 | 2498 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|    566 | 2499 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|    566 | 2500 | `		if( n > 0 ){` |
|    566 | 2501 | `			nLine = (sxu32)n;` |
|    281 | 2502 | `		}` |
|    281 | 2503 | `	}` |
|    566 | 2504 | `	PH7_MemObjRelease(&sLine);` |
|    566 | 2505 | `	return nLine;` |
|    285 | 2506 | `}` |
|    562 | 2507 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|      4 | 2508 | `{` |
|      - | 2509 | `	ph7_class_method *pGetMessage;` |
|      - | 2510 | `	ph7_value sMsg;` |
|      - | 2511 | `	const char *zTmp;` |
|      - | 2512 | `	int nTmp;` |
|    566 | 2513 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|    566 | 2514 | `	if( pGetMessage == 0 ){` |
|    ! 0 | 2515 | `		return;` |
|      - | 2516 | `	}` |
|    566 | 2517 | `	PH7_MemObjInit(pVm,&sMsg);` |
|    566 | 2518 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|    566 | 2519 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|    566 | 2520 | `		if( zTmp && nTmp > 0 ){` |
|    566 | 2521 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|    281 | 2522 | `		}` |
|    281 | 2523 | `	}` |
|    566 | 2524 | `	PH7_MemObjRelease(&sMsg);` |
|    285 | 2525 | `}` |
|      - | 2526 | `/*` |
|      - | 2527 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|      - | 2528 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|      - | 2529 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|      - | 2530 | ` * outermost (the actually-uncaught) exception.` |
|      - | 2531 | ` *` |
|      - | 2532 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|      - | 2533 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|      - | 2534 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|      - | 2535 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|      - | 2536 | ` */` |
|      - | 2537 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|    556 | 2538 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|      4 | 2539 | `{` |
|      - | 2540 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|    560 | 2541 | `	int nChain = 0;` |
|      - | 2542 | `	int i;` |
|      - | 2543 | `	SyBlob sOut;` |
|      - | 2544 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|      - | 2545 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|      - | 2546 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|    560 | 2547 | `	pVm->iExitStatus = 255;` |
|    560 | 2548 | `	if( !pVm->bErrReport ){` |
|    ! 0 | 2549 | `		return PH7_OK;` |
|      - | 2550 | `	}` |
|      - | 2551 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|      - | 2552 | `	 * collected) or the hard cap. */` |
|   1122 | 2553 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|    574 | 2554 | `		for( i = 0 ; i < nChain ; ++i ){` |
|     10 | 2555 | `			if( apChain[i] == pThis ){` |
|    ! 0 | 2556 | `				pThis = 0; /* cycle: stop the walk */` |
|    ! 0 | 2557 | `				break;` |
|      - | 2558 | `			}` |
|      6 | 2559 | `		}` |
|    566 | 2560 | `		if( pThis == 0 ){` |
|    ! 0 | 2561 | `			break;` |
|      - | 2562 | `		}` |
|    566 | 2563 | `		apChain[nChain++] = pThis;` |
|    566 | 2564 | `		pThis = VmExceptionGetPrevious(pThis);` |
|      4 | 2565 | `	}` |
|    560 | 2566 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      - | 2567 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|      - | 2568 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|   1122 | 2569 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|    566 | 2570 | `		ph7_class_instance *pEnt = apChain[i];` |
|      - | 2571 | `		SyBlob sMsg;` |
|    566 | 2572 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    566 | 2573 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|    847 | 2574 | `		VmRenderUncaughtEntry(pVm,&sOut,` |
|    562 | 2575 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|    562 | 2576 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|    281 | 2577 | `			zFuncName,nFuncLen,` |
|    562 | 2578 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|    281 | 2579 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|    281 | 2580 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|    566 | 2581 | `		SyBlobRelease(&sMsg);` |
|    285 | 2582 | `	}` |
|    560 | 2583 | `	VmCallErrorHandler(pVm,&sOut);` |
|    560 | 2584 | `	SyBlobRelease(&sOut);` |
|    560 | 2585 | `	return PH7_ABORT;` |
|    282 | 2586 | `}` |
|      - | 2587 | `/*` |
|      - | 2588 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|      - | 2589 | ` *` |
|      - | 2590 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|      - | 2591 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|      - | 2592 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|      - | 2593 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|      - | 2594 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|      - | 2595 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|      - | 2596 | ` */` |
|   2882 | 2597 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|      5 | 2598 | `{` |
|   2887 | 2599 | `	if( pVm->bCoalesceArmed ){` |
|      8 | 2600 | `		if( pVm->pCoalesceObj ){` |
|      8 | 2601 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|      3 | 2602 | `		}` |
|      8 | 2603 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|      8 | 2604 | `		pVm->pCoalesceObj = 0;` |
|      8 | 2605 | `		pVm->bCoalesceArmed = 0;` |
|      3 | 2606 | `	}` |
|   2887 | 2607 | `}` |
|      - | 2608 | `/*` |
|      - | 2609 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|      - | 2610 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|      - | 2611 | ` * is a literal, non-formatted string; callers that need formatting should` |
|      - | 2612 | ` * build the SyBlob themselves and pass its data + length.` |
|      - | 2613 | ` *` |
|      - | 2614 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|      - | 2615 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|      - | 2616 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|      - | 2617 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|      - | 2618 | ` */` |
|     74 | 2619 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|      - | 2620 | `	ph7_vm *pVm,` |
|      - | 2621 | `	const char *zClass,` |
|      - | 2622 | `	const char *zMsg,` |
|      - | 2623 | `	sxu32 nMsg` |
|      5 | 2624 | `){` |
|      - | 2625 | `	ph7_class *pClass;` |
|      - | 2626 | `	ph7_class_instance *pThis;` |
|      - | 2627 | `	ph7_class_method *pCons;` |
|      - | 2628 | `	VmFrame *pFrame;` |
|      - | 2629 | `	sxi32 rc;` |
|     79 | 2630 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|     79 | 2631 | `	if( pClass == 0 ){` |
|    ! 0 | 2632 | `		return SXERR_ABORT;` |
|      - | 2633 | `	}` |
|     79 | 2634 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|     79 | 2635 | `	if( pThis == 0 ){` |
|    ! 0 | 2636 | `		return SXERR_ABORT;` |
|      - | 2637 | `	}` |
|     79 | 2638 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     79 | 2639 | `	if( pCons ){` |
|      - | 2640 | `		ph7_value sArg;` |
|      - | 2641 | `		ph7_value *apArg[1];` |
|      - | 2642 | `		SyString sMsgStr;` |
|     79 | 2643 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|     79 | 2644 | `		PH7_MemObjInit(pVm,&sArg);` |
|     79 | 2645 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|     79 | 2646 | `		apArg[0] = &sArg;` |
|     79 | 2647 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|     79 | 2648 | `		PH7_MemObjRelease(&sArg);` |
|     37 | 2649 | `	}` |
|     79 | 2650 | `	pFrame = pVm->pFrame;` |
|     79 | 2651 | `	if( pFrame ){` |
|     79 | 2652 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     79 | 2653 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     37 | 2654 | `	}` |
|     79 | 2655 | `	rc = VmThrowException(pVm,pThis);` |
|     79 | 2656 | `	PH7_ClassInstanceUnref(pThis);` |
|     79 | 2657 | `	return rc;` |
|     42 | 2658 | `}` |
|      - | 2659 | `/*` |
|      - | 2660 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|      - | 2661 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|      - | 2662 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|      - | 2663 | ` *` |
|      - | 2664 | ` *   int/float/bool/null      arithmetic proceeds` |
|      - | 2665 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|      - | 2666 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|      - | 2667 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|      - | 2668 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|      - | 2669 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|      - | 2670 | ` *   object/resource          TypeError, naming the object's CLASS` |
|      - | 2671 | ` *` |
|      - | 2672 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|      - | 2673 | ` */` |
|      - | 2674 | `/*` |
|      - | 2675 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|      - | 2676 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|      - | 2677 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|      - | 2678 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|      - | 2679 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|      - | 2680 | ` * to depth 1).` |
|      - | 2681 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|      - | 2682 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|      - | 2683 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|      - | 2684 | ` * frame shape is file/line/function[/class/type], matching the default` |
|      - | 2685 | ` * zend.exception_ignore_args=On.` |
|      - | 2686 | ` */` |
|   2732 | 2687 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)` |
|      5 | 2688 | `{` |
|      - | 2689 | `	SyString *pFile;` |
|      - | 2690 | `	VmFrame *pFrame;` |
|      - | 2691 | `	ph7_value *pValue;` |
|   2737 | 2692 | `	pValue = ph7_new_scalar(&(*pVm));` |
|   2737 | 2693 | `	if( pValue == 0 ){` |
|    ! 0 | 2694 | `		return;` |
|      - | 2695 | `	}` |
|   2737 | 2696 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2737 | 2697 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|   4441 | 2698 | `	while( pFrame ){` |
|   4441 | 2699 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      - | 2700 | `		ph7_value *pEntry;` |
|   4441 | 2701 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|      - | 2702 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   1371 | 2703 | `			break;` |
|      - | 2704 | `		}` |
|   1709 | 2705 | `		pEntry = ph7_new_array(&(*pVm));` |
|   1709 | 2706 | `		if( pEntry == 0 ){` |
|    ! 0 | 2707 | `			break;` |
|      - | 2708 | `		}` |
|      - | 2709 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|      - | 2710 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|      - | 2711 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|      - | 2712 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|      - | 2713 | `		 * include-stack top for a call made at global scope. */` |
|      - | 2714 | `		{` |
|   1709 | 2715 | `			SyString *pFrameFile = pFile;` |
|   1709 | 2716 | `			if( pFrame->pParent->pUserData ){` |
|    183 | 2717 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|    183 | 2718 | `				if( pCaller->sFile.nByte > 0 ){` |
|     71 | 2719 | `					pFrameFile = &pCaller->sFile;` |
|     33 | 2720 | `				}` |
|     89 | 2721 | `			}` |
|   1709 | 2722 | `			if( pFrameFile ){` |
|   1709 | 2723 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   1709 | 2724 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   1709 | 2725 | `				ph7_value_reset_string_cursor(pValue);` |
|    852 | 2726 | `			}` |
|      - | 2727 | `		}` |
|   1709 | 2728 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   1709 | 2729 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|      - | 2730 | `		{` |
|   1709 | 2731 | `			const char *zDisp = 0;` |
|   1709 | 2732 | `			int nDisp = VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   1709 | 2733 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|      - | 2734 | `		}` |
|   1709 | 2735 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   1709 | 2736 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 2737 | `		{` |
|      - | 2738 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|      - | 2739 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|      - | 2740 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|      - | 2741 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|      - | 2742 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|      - | 2743 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|      - | 2744 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   1709 | 2745 | `			SyString *pClsName = 0;` |
|   1709 | 2746 | `			const char *zType = "->";` |
|   1709 | 2747 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|    445 | 2748 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|    445 | 2749 | `				zType = pFrame->pThis ? "->" : "::";` |
|   1489 | 2750 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|    ! 0 | 2751 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|    ! 0 | 2752 | `			}` |
|   1709 | 2753 | `			if( pClsName ){` |
|    445 | 2754 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|    445 | 2755 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|    445 | 2756 | `				ph7_value_reset_string_cursor(pValue);` |
|    445 | 2757 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|    445 | 2758 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|    445 | 2759 | `				ph7_value_reset_string_cursor(pValue);` |
|    445 | 2760 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){` |
|      9 | 2761 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      9 | 2762 | `					if( pObjVal ){` |
|      9 | 2763 | `						pFrame->pThis->iRef++;` |
|      9 | 2764 | `						pObjVal->x.pOther = pFrame->pThis;` |
|      9 | 2765 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      9 | 2766 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      9 | 2767 | `						ph7_release_value(&(*pVm),pObjVal);` |
|      4 | 2768 | `					}` |
|      4 | 2769 | `				}` |
|    220 | 2770 | `			}` |
|      - | 2771 | `		}` |
|   1709 | 2772 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      9 | 2773 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      9 | 2774 | `			if( pArg ){` |
|      9 | 2775 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      - | 2776 | `				sxu32 n;` |
|     17 | 2777 | `				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){` |
|      9 | 2778 | `					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|      9 | 2779 | `					if( pObj ){` |
|      9 | 2780 | `						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      4 | 2781 | `					}` |
|      5 | 2782 | `				}` |
|      9 | 2783 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      9 | 2784 | `				ph7_release_value(&(*pVm),pArg);` |
|      4 | 2785 | `			}` |
|      4 | 2786 | `		}` |
|   1709 | 2787 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   1709 | 2788 | `		ph7_release_value(&(*pVm),pEntry);` |
|   1709 | 2789 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|      5 | 2790 | `	}` |
|   2737 | 2791 | `	ph7_release_value(&(*pVm),pValue);` |
|   1371 | 2792 | `}` |
|      - | 2793 | `/*` |
|      - | 2794 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|      - | 2795 | ` *` |
|      - | 2796 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|      - | 2797 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|      - | 2798 | ` * calls parent::__construct still reports the right position. The embedded` |
|      - | 2799 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|      - | 2800 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|      - | 2801 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|      - | 2802 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|      - | 2803 | ` */` |
|   8392 | 2804 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      5 | 2805 | `{` |
|      - | 2806 | `	static const char *azField[] = { "file", "line", "trace" };` |
|      - | 2807 | `	ph7_class *pThrowable;` |
|      - | 2808 | `	SyString *pFile;` |
|      - | 2809 | `	SyString *pSiteFile;` |
|      - | 2810 | `	sxu32 n;` |
|   8397 | 2811 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|    ! 0 | 2812 | `		return;` |
|      - | 2813 | `	}` |
|   8397 | 2814 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|   8397 | 2815 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   5675 | 2816 | `		return;` |
|      - | 2817 | `	}` |
|   2727 | 2818 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2727 | 2819 | `	pSiteFile = pFile;` |
|      - | 2820 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|      - | 2821 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|      - | 2822 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|      - | 2823 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|      - | 2824 | `	{` |
|   2727 | 2825 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|   2727 | 2826 | `		if( pInner && pInner->pUserData ){` |
|   1065 | 2827 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|   1065 | 2828 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|    801 | 2829 | `				pSiteFile = &pInnerFunc->sFile;` |
|    398 | 2830 | `			}` |
|    530 | 2831 | `		}` |
|      - | 2832 | `	}` |
|  10893 | 2833 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|      - | 2834 | `		SyHashEntry *pEntry;` |
|      - | 2835 | `		VmClassAttr *pVmAttr;` |
|      - | 2836 | `		ph7_value *pAttrValue;` |
|   8171 | 2837 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|   8171 | 2838 | `		if( pEntry == 0 ){` |
|    ! 0 | 2839 | `			continue;` |
|      - | 2840 | `		}` |
|   8171 | 2841 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   8171 | 2842 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   8171 | 2843 | `		if( pAttrValue == 0 ){` |
|    ! 0 | 2844 | `			continue;` |
|      - | 2845 | `		}` |
|   8171 | 2846 | `		if( n == 0 ){` |
|   2727 | 2847 | `			if( pSiteFile ){` |
|   2727 | 2848 | `				PH7_MemObjRelease(pAttrValue);` |
|   2727 | 2849 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   1366 | 2850 | `			}` |
|   6810 | 2851 | `		}else if( n == 1 ){` |
|   2727 | 2852 | `			PH7_MemObjRelease(pAttrValue);` |
|   2727 | 2853 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)(pVm->nCurLine ? pVm->nCurLine : 1));` |
|   1366 | 2854 | `		}else{` |
|      - | 2855 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|      - | 2856 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|      - | 2857 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|      - | 2858 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|   2727 | 2859 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|   2727 | 2860 | `			if( pList == 0 ){` |
|    ! 0 | 2861 | `				continue;` |
|      - | 2862 | `			}` |
|   2727 | 2863 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);` |
|      - | 2864 | `			/* Building the trace reserves new memobjs, which may realloc` |
|      - | 2865 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|      - | 2866 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|      - | 2867 | `			 * AFTER the walk before releasing/storing into it. */` |
|   2727 | 2868 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   2727 | 2869 | `			if( pAttrValue ){` |
|   2727 | 2870 | `				PH7_MemObjRelease(pAttrValue);` |
|   2727 | 2871 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   1361 | 2872 | `			}` |
|   2727 | 2873 | `			ph7_release_value(&(*pVm),pList);` |
|      - | 2874 | `		}` |
|   4088 | 2875 | `	}` |
|   4201 | 2876 | `}` |
|     44 | 2877 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|      2 | 2878 | `{` |
|     46 | 2879 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      3 | 2880 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      3 | 2881 | `		if( pInst && pInst->pClass ){` |
|      3 | 2882 | `			return pInst->pClass->sName.zString;` |
|      - | 2883 | `		}` |
|    ! 0 | 2884 | `	}` |
|     44 | 2885 | `	return ph7_type_name(pVal);` |
|     24 | 2886 | `}` |
|  42049 | 2887 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|      5 | 2888 | `{` |
|  42054 | 2889 | `	int bBadL = 0, bBadR = 0;` |
|      - | 2890 | `	int i;` |
|      - | 2891 | `	ph7_value *apOperand[2];` |
|  42054 | 2892 | `	apOperand[0] = pLeft;` |
|  42054 | 2893 | `	apOperand[1] = pRight;` |
|      - | 2894 | `	/* array + array is php's union operator, not arithmetic */` |
|  42049 | 2895 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|  14677 | 2896 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|   3825 | 2897 | `		return SXRET_OK;` |
|      - | 2898 | `	}` |
| 114692 | 2899 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  76463 | 2900 | `		ph7_value *pVal = apOperand[i];` |
|  76463 | 2901 | `		int bBad = 0;` |
|  76463 | 2902 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      5 | 2903 | `			bBad = 1;` |
|  76461 | 2904 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     50 | 2905 | `			const char *zTail = 0;` |
|     50 | 2906 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     50 | 2907 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|      - | 2908 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     11 | 2909 | `				bBad = 1;` |
|      6 | 2910 | `			}else{` |
|      - | 2911 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|      - | 2912 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|      - | 2913 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     42 | 2914 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|      3 | 2915 | `					zTail++;` |
|      1 | 2916 | `				}` |
|     40 | 2917 | `				if( zTail < zEnd ){` |
|     20 | 2918 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      9 | 2919 | `				}` |
|      - | 2920 | `			}` |
|     24 | 2921 | `		}` |
|  76463 | 2922 | `		if( bBad ){` |
|     15 | 2923 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|      7 | 2924 | `		}` |
|  38339 | 2925 | `	}` |
|  38234 | 2926 | `	if( bBadL \|\| bBadR ){` |
|      - | 2927 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|      - | 2928 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|      - | 2929 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     22 | 2930 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|      7 | 2931 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     15 | 2932 | `		return SXERR_INVALID;` |
|      - | 2933 | `	}` |
|  38220 | 2934 | `	return SXRET_OK;` |
|  21082 | 2935 | `}` |
|      - | 2936 | `/*` |
|      - | 2937 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|      - | 2938 | ` */` |
|   1386 | 2939 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      5 | 2940 | `{` |
|      - | 2941 | `	ph7_vm *pVm;` |
|      - | 2942 | `	ph7_class *pClass;` |
|      - | 2943 | `	ph7_class_instance *pThis;` |
|      - | 2944 | `	ph7_class_method *pCons;` |
|      - | 2945 | `	ph7_value sArg;` |
|      - | 2946 | `	ph7_value *apArg[1];` |
|      - | 2947 | `	SyBlob sMsg;` |
|      - | 2948 | `	SyString sMsgStr;` |
|      - | 2949 | `	VmFrame *pFrame;` |
|      - | 2950 | `	va_list ap;` |
|      - | 2951 | `	sxi32 rc;` |
|      - | 2952 |  |
|   1391 | 2953 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|    ! 0 | 2954 | `		return PH7_ABORT;` |
|      - | 2955 | `	}` |
|   1391 | 2956 | `	pVm = pCtx->pVm;` |
|   1391 | 2957 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|    ! 0 | 2958 | `		zClass = "Error";` |
|    ! 0 | 2959 | `	}` |
|   1391 | 2960 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|   1391 | 2961 | `	if( pClass == 0 ){` |
|    ! 0 | 2962 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|      - | 2963 | `			"Cannot throw internal exception, class '%s' is not available",` |
|    ! 0 | 2964 | `			zClass` |
|      - | 2965 | `			);` |
|      - | 2966 | `	}` |
|   1391 | 2967 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|   1391 | 2968 | `	if( pThis == 0 ){` |
|    ! 0 | 2969 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|      - | 2970 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|      - | 2971 | `			);` |
|      - | 2972 | `	}` |
|      - | 2973 |  |
|   1391 | 2974 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   1391 | 2975 | `	va_start(ap,zFormat);` |
|   1391 | 2976 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|   1391 | 2977 | `	va_end(ap);` |
|      - | 2978 |  |
|   1391 | 2979 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   1391 | 2980 | `	if( pCons ){` |
|   1391 | 2981 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|   1391 | 2982 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   1391 | 2983 | `		apArg[0] = &sArg;` |
|   1391 | 2984 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   1391 | 2985 | `		PH7_MemObjRelease(&sArg);` |
|    693 | 2986 | `	}` |
|   1391 | 2987 | `	SyBlobRelease(&sMsg);` |
|      - | 2988 |  |
|   1391 | 2989 | `	pFrame = pVm->pFrame;` |
|   1391 | 2990 | `	if( pFrame ){` |
|   1391 | 2991 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   1391 | 2992 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    693 | 2993 | `	}` |
|   1391 | 2994 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   1391 | 2995 | `	PH7_ClassInstanceUnref(pThis);` |
|   1391 | 2996 | `	if( rc == SXERR_ABORT ){` |
|    502 | 2997 | `		return PH7_ABORT;` |
|      - | 2998 | `	}` |
|    893 | 2999 | `	return PH7_EXCEPTION;` |
|    698 | 3000 | `}` |
|      - | 3001 | `/*` |
|      - | 3002 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|      - | 3003 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|      - | 3004 | ` */` |
|    ! 0 | 3005 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|    ! 0 | 3006 | `{` |
|      - | 3007 | `	ph7_vm *pVm;` |
|      - | 3008 | `	SyBlob sMsg;` |
|    ! 0 | 3009 | `	const char *zFuncName = 0;` |
|    ! 0 | 3010 | `	int nFuncLen = 0;` |
|      - | 3011 | `	va_list ap;` |
|      - | 3012 | `	sxi32 rc;` |
|      - | 3013 |  |
|    ! 0 | 3014 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|    ! 0 | 3015 | `		return PH7_OK;` |
|      - | 3016 | `	}` |
|    ! 0 | 3017 | `	pVm = pCtx->pVm;` |
|    ! 0 | 3018 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|    ! 0 | 3019 | `		zClass = "Error";` |
|    ! 0 | 3020 | `	}` |
|      - | 3021 |  |
|    ! 0 | 3022 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      - | 3023 |  |
|    ! 0 | 3024 | `	va_start(ap,zFormat);` |
|    ! 0 | 3025 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|    ! 0 | 3026 | `	va_end(ap);` |
|      - | 3027 |  |
|    ! 0 | 3028 | `	if( pCtx->pFunc ){` |
|    ! 0 | 3029 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|    ! 0 | 3030 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|    ! 0 | 3031 | `	}` |
|    ! 0 | 3032 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|    ! 0 | 3033 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|    ! 0 | 3034 | `	}` |
|    ! 0 | 3035 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|    ! 0 | 3036 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|    ! 0 | 3037 | `	SyBlobRelease(&sMsg);` |
|    ! 0 | 3038 | `	return rc;` |
|    ! 0 | 3039 | `}` |
|      - | 3040 | `/*` |
|      - | 3041 | ` * The following routine is invoked by the engine when an uncaught` |
|      - | 3042 | ` * exception is triggered.` |
|      - | 3043 | ` */` |
|    558 | 3044 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|      - | 3045 | `	ph7_vm *pVm, /* Target VM */` |
|      - | 3046 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|      - | 3047 | `	)` |
|      4 | 3048 | `{` |
|      - | 3049 | `	ph7_value *apArg[2],sArg;` |
|    562 | 3050 | `	int nArg = 1;` |
|      - | 3051 | `	sxi32 rc;` |
|    562 | 3052 | `	if( pVm->nExceptDepth > 15 ){` |
|      - | 3053 | `		/* Nesting limit reached */` |
|    ! 0 | 3054 | `		return SXRET_OK;` |
|      - | 3055 | `	}` |
|      - | 3056 | `	/* Call any exception handler if available */` |
|    562 | 3057 | `	PH7_MemObjInit(pVm,&sArg);` |
|    562 | 3058 | `	if( pThis ){` |
|      - | 3059 | `		/* Load the exception instance */` |
|    562 | 3060 | `		sArg.x.pOther = pThis;` |
|    562 | 3061 | `		pThis->iRef++;` |
|    562 | 3062 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|    283 | 3063 | `	}else{` |
|    ! 0 | 3064 | `		nArg = 0;` |
|      - | 3065 | `	}` |
|    562 | 3066 | `	apArg[0] = &sArg;` |
|      - | 3067 | `	/* Call the exception handler if available */` |
|    562 | 3068 | `	pVm->nExceptDepth++;` |
|    562 | 3069 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|    562 | 3070 | `	pVm->nExceptDepth--;` |
|    562 | 3071 | `	if( rc != SXRET_OK ){` |
|      - | 3072 | `		const char *zFuncName;` |
|      - | 3073 | `		int nFuncLen;` |
|    560 | 3074 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      - | 3075 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|    560 | 3076 | `		if( pThis ){` |
|      - | 3077 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|      - | 3078 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|      - | 3079 | `			 * renders byte-identically to the historical single-entry report. */` |
|    560 | 3080 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|    282 | 3081 | `		}else{` |
|      - | 3082 | `			/* No instance (internal report path) — default-class single entry. */` |
|    ! 0 | 3083 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|      - | 3084 | `		}` |
|      - | 3085 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|    560 | 3086 | `		rc = SXERR_ABORT;` |
|    278 | 3087 | `	}` |
|    562 | 3088 | `	PH7_MemObjRelease(&sArg);` |
|    562 | 3089 | `	return rc;` |
|    283 | 3090 | `}` |
|      - | 3091 | `/*` |
|      - | 3092 | ` * Throw a user exception.` |
|      - | 3093 | ` *` |
|      - | 3094 | ` * Exception dispatch follows this sequence:` |
|      - | 3095 | ` *` |
|      - | 3096 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|      - | 3097 | ` *    try/catch whose catch block matches the exception class.` |
|      - | 3098 | ` *` |
|      - | 3099 | ` * 2. If NO catch matches:` |
|      - | 3100 | ` *    a. Run finally (if present) for the current try block.` |
|      - | 3101 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|      - | 3102 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|      - | 3103 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|      - | 3104 | ` *       exception in pVm->pPendingException instead of reporting it` |
|      - | 3105 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|      - | 3106 | ` *    d. Otherwise, report as truly uncaught.` |
|      - | 3107 | ` *` |
|      - | 3108 | ` * 3. If a catch DOES match:` |
|      - | 3109 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|      - | 3110 | ` *       aException stack and resetting it. This prevents a re-throw` |
|      - | 3111 | ` *       inside the catch body from immediately propagating past our` |
|      - | 3112 | ` *       finally block.` |
|      - | 3113 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|      - | 3114 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|      - | 3115 | ` *       no handlers (they're hidden), so the exception is deferred` |
|      - | 3116 | ` *       in pPendingException (step 2c).` |
|      - | 3117 | ` *    c. Restore outer handlers from the saved copy.` |
|      - | 3118 | ` *    d. Run finally (if present).` |
|      - | 3119 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|      - | 3120 | ` *       that handlers are restored and finally has run.` |
|      - | 3121 | ` */` |
|      - | 3122 | `/*` |
|      - | 3123 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|      - | 3124 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|      - | 3125 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|      - | 3126 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|      - | 3127 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|      - | 3128 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|      - | 3129 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|      - | 3130 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|      - | 3131 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|      - | 3132 | ` */` |
|    100 | 3133 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|      5 | 3134 | `{` |
|    109 | 3135 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|     39 | 3136 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|     39 | 3137 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|     39 | 3138 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|    ! 0 | 3139 | `			break; /* reached an outer exec's / legacy handler */` |
|      - | 3140 | `		}` |
|     39 | 3141 | `		(void)SySetPop(&pVm->aException);` |
|     39 | 3142 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|     39 | 3143 | `		if( pT->iHasFinally ){` |
|     35 | 3144 | `			*pPc = pT->iFinallyPc;` |
|     35 | 3145 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|     35 | 3146 | `			return 1;` |
|      - | 3147 | `		}` |
|      - | 3148 | `		/* No finally: tear the try's transparent frame down now. */` |
|      5 | 3149 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|    ! 0 | 3150 | `			VmLeaveFrame(&(*pVm));` |
|    ! 0 | 3151 | `		}` |
|      5 | 3152 | `		VmExcRelease(&(*pVm),pT);` |
|      1 | 3153 | `	}` |
|     73 | 3154 | `	return 0;` |
|     55 | 3155 | `}` |
|      - | 3156 | `/*` |
|      - | 3157 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|      - | 3158 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|      - | 3159 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|      - | 3160 | ` *` |
|      - | 3161 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|      - | 3162 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|      - | 3163 | ` *    and redirect to the catch body (iHandlerPc).` |
|      - | 3164 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|      - | 3165 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|      - | 3166 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|      - | 3167 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|      - | 3168 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|      - | 3169 | ` */` |
|      - | 3170 | `/*` |
|      - | 3171 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|      - | 3172 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|      - | 3173 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|      - | 3174 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|      - | 3175 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|      - | 3176 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|      - | 3177 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|      - | 3178 | ` * case) is unchanged: no wrapper.` |
|      - | 3179 | ` */` |
|    194 | 3180 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|      5 | 3181 | `{` |
|    199 | 3182 | `	VmFrame *pWrap = 0;` |
|      - | 3183 | `	VmFrame *pThrowSite;` |
|      - | 3184 | `	sxi32 rc;` |
|    199 | 3185 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|     97 | 3186 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3187 | `	}` |
|    104 | 3188 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|      - | 3189 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|    ! 0 | 3190 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3191 | `	}` |
|    104 | 3192 | `	pThrowSite = pWrap->pParent;` |
|    104 | 3193 | `	pWrap->pParent = pOwner;` |
|    104 | 3194 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|    104 | 3195 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|      - | 3196 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|      - | 3197 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|      - | 3198 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|      - | 3199 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|    104 | 3200 | `	if( pVm->pFrame == pWrap ){` |
|    104 | 3201 | `		VmLeaveFrame(&(*pVm));` |
|     51 | 3202 | `	}` |
|    104 | 3203 | `	pVm->pFrame = pThrowSite;` |
|    104 | 3204 | `	return rc;` |
|    102 | 3205 | `}` |
|      - | 3206 | `/*` |
|      - | 3207 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|      - | 3208 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|      - | 3209 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|      - | 3210 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|      - | 3211 | ` */` |
|      - | 3212 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|     76 | 3213 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - | 3214 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|      5 | 3215 | `{` |
|     81 | 3216 | `	if( pCatch ){` |
|     71 | 3217 | `		pException->iInCatch = 1;` |
|     71 | 3218 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|     71 | 3219 | `		if( pThis ){ pThis->iRef++; }` |
|     71 | 3220 | `		pException->pInflight = pThis;` |
|     71 | 3221 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|     71 | 3222 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|     71 | 3223 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|     71 | 3224 | `		return SXRET_OK;` |
|      - | 3225 | `	}` |
|     13 | 3226 | `	if( pException->iHasFinally ){` |
|      - | 3227 | `		VmFinallyAction sAct;` |
|      8 | 3228 | `		SyZero(&sAct,sizeof(sAct));` |
|      8 | 3229 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      8 | 3230 | `		if( pThis ){ pThis->iRef++; }` |
|      8 | 3231 | `		sAct.pExc = pThis;` |
|      8 | 3232 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      8 | 3233 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      8 | 3234 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      8 | 3235 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      8 | 3236 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      8 | 3237 | `		return SXRET_OK;` |
|      - | 3238 | `	}` |
|      - | 3239 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|      - | 3240 | `	 * flat native stack instead of mutual recursion. */` |
|      6 | 3241 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|    ! 0 | 3242 | `		VmLeaveFrame(&(*pVm));` |
|    ! 0 | 3243 | `	}` |
|      6 | 3244 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      6 | 3245 | `	return VM_THROW_KEEP_UNWINDING;` |
|     43 | 3246 | `}` |
|   2690 | 3247 | `PH7_PRIVATE sxi32 VmThrowException(` |
|      - | 3248 | `	ph7_vm *pVm,              /* Target VM */` |
|      - | 3249 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|      - | 3250 | `	)` |
|      5 | 3251 | `{` |
|      - | 3252 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|      - | 3253 | `	ph7_exception **apException;` |
|   1345 | 3254 | `	ph7_exception *pException;` |
|     90 | 3255 | `Rethrow:` |
|      - | 3256 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|      - | 3257 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|      - | 3258 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|      - | 3259 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|      - | 3260 | `	 * so the throw path must be too). */` |
|      - | 3261 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|      - | 3262 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|      - | 3263 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|   2875 | 3264 | `	VmCoalesceDisarm(pVm);` |
|      - | 3265 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|      - | 3266 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|      - | 3267 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|      - | 3268 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|      - | 3269 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|      - | 3270 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|      - | 3271 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|      - | 3272 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|   2870 | 3273 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|     23 | 3274 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|     18 | 3275 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|      8 | 3276 | `	}` |
|      - | 3277 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|      - | 3278 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|      - | 3279 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|      - | 3280 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|      - | 3281 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|      - | 3282 | `	 * that owns the pending return, so it must leave that return intact. */` |
|      - | 3283 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|      - | 3284 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|      - | 3285 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|      - | 3286 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|   2875 | 3287 | `	pVm->pResumeFrame = 0;` |
|      - | 3288 | `	/* Point to the stack of loaded exceptions */` |
|   2875 | 3289 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|   2875 | 3290 | `	pException = 0;` |
|   2875 | 3291 | `	pCatch = 0;` |
|   2875 | 3292 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|      - | 3293 | `		ph7_exception_block *aCatch;` |
|      - | 3294 | `		ph7_class *pClass;` |
|      - | 3295 | `		SyString *aNames;` |
|      - | 3296 | `		sxu32 nNames;` |
|      - | 3297 | `		int matched;` |
|      - | 3298 | `		sxu32 j,k;` |
|      - | 3299 | `		/* Locate the appropriate block to execute */` |
|   2247 | 3300 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|   2247 | 3301 | `		(void)SySetPop(&pVm->aException);` |
|   2247 | 3302 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      - | 3303 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|      - | 3304 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|      - | 3305 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|   2261 | 3306 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|      - | 3307 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|   2119 | 3308 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|   2119 | 3309 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|   2119 | 3310 | `			matched = 0;` |
|   2155 | 3311 | `			for( k = 0 ; k < nNames ; ++k ){` |
|      - | 3312 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|      - | 3313 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|      - | 3314 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|   2141 | 3315 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|   2141 | 3316 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|      - | 3317 | `					/* No such class, or trait — cannot match */` |
|    ! 0 | 3318 | `					continue;` |
|      - | 3319 | `				}` |
|   2141 | 3320 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|   2105 | 3321 | `					matched = 1;` |
|   2105 | 3322 | `					break;` |
|      - | 3323 | `				}` |
|     21 | 3324 | `			}` |
|   2119 | 3325 | `			if( matched ){` |
|      - | 3326 | `				/* Catch block found,break immediately */` |
|   2105 | 3327 | `				pCatch = &aCatch[j];` |
|   2105 | 3328 | `				break;` |
|      - | 3329 | `			}` |
|     10 | 3330 | `		}` |
|   1121 | 3331 | `	}` |
|      - | 3332 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|      - | 3333 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|      - | 3334 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|      - | 3335 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|      - | 3336 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|      - | 3337 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|      - | 3338 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|   2875 | 3339 | `	if( pException ){` |
|   2247 | 3340 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   1121 | 3341 | `	}` |
|      - | 3342 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|      - | 3343 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|      - | 3344 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|   2875 | 3345 | `	if( pException && pException->iInlined ){` |
|     81 | 3346 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|     81 | 3347 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|      - | 3348 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|      6 | 3349 | `			goto Rethrow;` |
|      - | 3350 | `		}` |
|     77 | 3351 | `		return rcInline;` |
|      - | 3352 | `	}` |
|      - | 3353 | `	/* Execute the cached block if available */` |
|   2799 | 3354 | `	if( pCatch == 0 ){` |
|      - | 3355 | `		sxi32 rc;` |
|      - | 3356 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|    765 | 3357 | `		if( pException && pException->iHasFinally ){` |
|    134 | 3358 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    134 | 3359 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    134 | 3360 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|    134 | 3361 | `			pException->iFinallyDone = 1;` |
|      - | 3362 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|      - | 3363 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    134 | 3364 | `			pVm->pInflightException = pThis;` |
|    134 | 3365 | `			pVm->nInflightExcBase = nExcBefore;` |
|      - | 3366 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|      - | 3367 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    134 | 3368 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    134 | 3369 | `			pVm->pInflightException = pSaveInflight;` |
|    134 | 3370 | `			pVm->nInflightExcBase = nSaveBase;` |
|    134 | 3371 | `			if( rc == SXERR_ABORT ){` |
|      3 | 3372 | `				VmExcRelease(&(*pVm),pException);` |
|      3 | 3373 | `				return SXERR_ABORT;` |
|      - | 3374 | `			}` |
|      - | 3375 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|      - | 3376 | `			 * semantics). The finally stored it on the body frame it returns from` |
|      - | 3377 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|      - | 3378 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|      - | 3379 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|      - | 3380 | `			 * takes the value instead of unwinding) and resume in place.` |
|      - | 3381 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|      - | 3382 | `			 * the same transport an in-place catch uses — and unwind as an` |
|      - | 3383 | `			 * exception; the owner's activation consumes the resume` |
|      - | 3384 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|      - | 3385 | `			 * its bHasRet tail materializes the return. */` |
|      - | 3386 | `			{` |
|    132 | 3387 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    132 | 3388 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    132 | 3389 | `				if( pOwnerFrame->bHasRet ){` |
|      8 | 3390 | `					if( pOwnerFrame == pThrowFrame ){` |
|      5 | 3391 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|      5 | 3392 | `						VmExcRelease(&(*pVm),pException);` |
|      5 | 3393 | `						return SXRET_OK;` |
|      - | 3394 | `					}` |
|      3 | 3395 | `					pVm->pResumeFrame = pOwnerFrame;` |
|      3 | 3396 | `					pVm->iResumePc = pException->iLandingPc;` |
|      3 | 3397 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|      3 | 3398 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|      3 | 3399 | `					VmExcRelease(&(*pVm),pException);` |
|      3 | 3400 | `					return PH7_EXCEPTION;` |
|      - | 3401 | `				}` |
|      - | 3402 | `			}` |
|      - | 3403 | `			/* The finally threw an exception that superseded pThis — it either` |
|      - | 3404 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|      - | 3405 | `			 * (which consumed an entry from the exception stack). Either way the` |
|      - | 3406 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|      - | 3407 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|    126 | 3408 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|     16 | 3409 | `				VmExcRelease(&(*pVm),pException);` |
|     16 | 3410 | `				return PH7_EXCEPTION;` |
|      - | 3411 | `			}` |
|     54 | 3412 | `		}` |
|      - | 3413 | `		/* Check if there is an outer exception handler on the stack */` |
|    743 | 3414 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|      - | 3415 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|      - | 3416 | `			 * iteration per unwound level instead of one native frame. */` |
|    113 | 3417 | `			VmExcRelease(&(*pVm),pException);` |
|    113 | 3418 | `			goto Rethrow;` |
|      - | 3419 | `		}` |
|      - | 3420 | `		/* No outer handler. If the handlers were temporarily hidden` |
|      - | 3421 | `		 * (catch body re-throw with finally pending), defer the` |
|      - | 3422 | `		 * exception instead of reporting it uncaught.` |
|      - | 3423 | `		 */` |
|    633 | 3424 | `		if( pVm->pPendingException == 0 && pThis ){` |
|      - | 3425 | `			/* Check if we are inside a catch execution with hidden handlers` |
|      - | 3426 | `			 * by looking for a catch frame on the stack.` |
|      - | 3427 | `			 */` |
|    633 | 3428 | `			VmFrame *pF = pVm->pFrame;` |
|    633 | 3429 | `			int inCatch = 0;` |
|   1231 | 3430 | `			while( pF ){` |
|    673 | 3431 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|     73 | 3432 | `					inCatch = 1;` |
|     73 | 3433 | `					break;` |
|      - | 3434 | `				}` |
|    602 | 3435 | `				pF = pF->pParent;` |
|      4 | 3436 | `			}` |
|    633 | 3437 | `			if( inCatch ){` |
|      - | 3438 | `				/* Defer — will be re-thrown after finally runs */` |
|     73 | 3439 | `				pThis->iRef++;` |
|     73 | 3440 | `				pVm->pPendingException = pThis;` |
|     73 | 3441 | `				VmExcRelease(&(*pVm),pException);` |
|     73 | 3442 | `				return SXRET_OK;` |
|      - | 3443 | `			}` |
|    279 | 3444 | `		}` |
|      - | 3445 | `		/* Truly uncaught */` |
|    562 | 3446 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|    562 | 3447 | `		if( rc == SXRET_OK && pException ){` |
|    ! 0 | 3448 | `			VmFrame *pFrame = pVm->pFrame;` |
|    ! 0 | 3449 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|    ! 0 | 3450 | `			if( pException->pFrame == pFrame ){` |
|    ! 0 | 3451 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|    ! 0 | 3452 | `			}` |
|    ! 0 | 3453 | `		}` |
|    562 | 3454 | `		VmExcRelease(&(*pVm),pException);` |
|    562 | 3455 | `		return rc;` |
|    ! 0 | 3456 | `	}else{` |
|   2039 | 3457 | `		VmFrame *pFrame = pVm->pFrame;` |
|   2039 | 3458 | `		ph7_exception **apSaved = 0;` |
|      - | 3459 | `		sxu32 nSavedCount;` |
|      - | 3460 | `		sxi32 rc;` |
|      - | 3461 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|      - | 3462 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|      - | 3463 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|      - | 3464 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|      - | 3465 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|   2039 | 3466 | `		VmFrame *pCatchBody = pException->pFrame;` |
|   2039 | 3467 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|   2039 | 3468 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|   2039 | 3469 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   2039 | 3470 | `		if( pException->pFrame == pFrame ){` |
|   1155 | 3471 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|    575 | 3472 | `		}` |
|      - | 3473 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|      - | 3474 | `		 * body re-throws, the exception does not immediately propagate past` |
|      - | 3475 | `		 * our finally block. We save the stack contents and restore after.` |
|      - | 3476 | `		 */` |
|   2039 | 3477 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|   2039 | 3478 | `		if( nSavedCount > 0 ){` |
|    142 | 3479 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     46 | 3480 | `				nSavedCount * sizeof(ph7_exception *));` |
|     96 | 3481 | `			if( apSaved ){` |
|    142 | 3482 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|     46 | 3483 | `					nSavedCount * sizeof(ph7_exception *));` |
|     96 | 3484 | `				SySetReset(&pVm->aException);` |
|     46 | 3485 | `			}` |
|     46 | 3486 | `		}` |
|      - | 3487 | `		/* Create the catch frame (made transparent below) */` |
|   2039 | 3488 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|   2039 | 3489 | `		if( rc == SXRET_OK ){` |
|      - | 3490 | `			ph7_value *pObj;` |
|      - | 3491 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|      - | 3492 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|      - | 3493 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|      - | 3494 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|      - | 3495 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|      - | 3496 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|      - | 3497 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|      - | 3498 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|      - | 3499 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|   2039 | 3500 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|      - | 3501 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|      - | 3502 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|      - | 3503 | `			 * against the live current scope rather than a freed frame. */` |
|   2039 | 3504 | `			if( pCatchBody ){` |
|   2039 | 3505 | `				pFrame->pParent = pCatchBody;` |
|   1017 | 3506 | `			}` |
|      - | 3507 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|      - | 3508 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|      - | 3509 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|      - | 3510 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|      - | 3511 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|      - | 3512 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|      - | 3513 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|      - | 3514 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|   2039 | 3515 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|      - | 3516 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|   3056 | 3517 | `			pObj = (pCatch->sThis.nByte > 0)` |
|   2032 | 3518 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|   2039 | 3519 | `			if( pObj ){` |
|      - | 3520 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|      - | 3521 | `				 * so it may already hold a value from a prior catch or assignment.` |
|      - | 3522 | `				 * Pin the new instance, then release the slot's prior contents` |
|      - | 3523 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|      - | 3524 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|      - | 3525 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|   2035 | 3526 | `				pThis->iRef++;` |
|   2035 | 3527 | `				PH7_MemObjRelease(pObj);` |
|   2035 | 3528 | `				pObj->x.pOther = pThis;` |
|   2035 | 3529 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   1015 | 3530 | `			}` |
|      - | 3531 | `			/* Execute the catch block */` |
|   2039 | 3532 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|      - | 3533 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|      - | 3534 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|      - | 3535 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|      - | 3536 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|      - | 3537 | `			 * unbalanced — never pop somebody else's frame. */` |
|   2039 | 3538 | `			if( pVm->pFrame == pFrame ){` |
|   2039 | 3539 | `				VmLeaveFrame(&(*pVm));` |
|   1017 | 3540 | `			}` |
|   2039 | 3541 | `			pVm->pFrame = pThrowSite;` |
|   1017 | 3542 | `		}` |
|      - | 3543 | `		/* Restore the outer exception handlers */` |
|   2039 | 3544 | `		if( apSaved ){` |
|      - | 3545 | `			sxu32 k;` |
|      - | 3546 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|      - | 3547 | `			 * the catch body) are normally already consumed; on an abnormal` |
|      - | 3548 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|      - | 3549 | `			 * linger — release those activations before discarding the set. */` |
|     96 | 3550 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|     96 | 3551 | `			SySetReset(&pVm->aException);` |
|    840 | 3552 | `			for(k = 0; k < nSavedCount; k++){` |
|    748 | 3553 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    376 | 3554 | `			}` |
|     96 | 3555 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|     46 | 3556 | `		}` |
|      - | 3557 | `		/* Execute the finally block after catch */` |
|   2039 | 3558 | `		if( pException->iHasFinally ){` |
|      - | 3559 | `			sxi32 rcf;` |
|      - | 3560 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|      - | 3561 | `			 * from, its pending-return write generation (set if the catch above` |
|      - | 3562 | `			 * returned), and the exception-stack depth. After the finally we use` |
|      - | 3563 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|      - | 3564 | `			 * catch-return. */` |
|      - | 3565 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|      - | 3566 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|      - | 3567 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|      - | 3568 | `			 * supersede decision belong to the owner, not the thrower. */` |
|     69 | 3569 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|     69 | 3570 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|     69 | 3571 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|      - | 3572 | `			/* The exception in flight while this finally runs is the catch body's` |
|      - | 3573 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|      - | 3574 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|      - | 3575 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|      - | 3576 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|     69 | 3577 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|     69 | 3578 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|     69 | 3579 | `			pException->iFinallyDone = 1;` |
|     69 | 3580 | `			pVm->pInflightException = pVm->pPendingException;` |
|     69 | 3581 | `			pVm->nInflightExcBase = nExcBefore;` |
|      - | 3582 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|     69 | 3583 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|     69 | 3584 | `			pVm->pInflightException = pSaveInflight;` |
|     69 | 3585 | `			pVm->nInflightExcBase = nSaveBase;` |
|     69 | 3586 | `			if( rcf == SXERR_ABORT ){` |
|    ! 0 | 3587 | `				VmExcRelease(&(*pVm),pException);` |
|    ! 0 | 3588 | `				return SXERR_ABORT;` |
|      - | 3589 | `			}` |
|      - | 3590 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|      - | 3591 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|      - | 3592 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|      - | 3593 | `			 * either case that exception supersedes this try's catch-return — but` |
|      - | 3594 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|      - | 3595 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|      - | 3596 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|     64 | 3597 | `			if( (rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore)` |
|     45 | 3598 | `			 && pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|     12 | 3599 | `				VmClearFrameReturn(pBody);` |
|      5 | 3600 | `			}` |
|     69 | 3601 | `			if( rcf == PH7_EXCEPTION ){` |
|      - | 3602 | `				/* The finally's exception propagated past this try; drop any deferred` |
|      - | 3603 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|      - | 3604 | `				 * reaches the frame that caught the finally's throw. */` |
|     19 | 3605 | `				if( pVm->pPendingException ){` |
|    ! 0 | 3606 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|    ! 0 | 3607 | `					pVm->pPendingException = 0;` |
|    ! 0 | 3608 | `				}` |
|     19 | 3609 | `				VmExcRelease(&(*pVm),pException);` |
|     19 | 3610 | `				return PH7_EXCEPTION;` |
|      - | 3611 | `			}` |
|     24 | 3612 | `		}` |
|   2023 | 3613 | `		if( rc == SXERR_ABORT ){` |
|      5 | 3614 | `			VmExcRelease(&(*pVm),pException);` |
|      5 | 3615 | `			return SXERR_ABORT;` |
|      - | 3616 | `		}` |
|      - | 3617 | `		/* If the catch body re-threw, the exception was deferred in` |
|      - | 3618 | `		 * pPendingException (because outer handlers were hidden).` |
|      - | 3619 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|      - | 3620 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|      - | 3621 | `		 * the catch frame having been left above), which swallows the in-flight` |
|      - | 3622 | `		 * exception (PHP semantics).` |
|      - | 3623 | `		 */` |
|   2019 | 3624 | `		if( pVm->pPendingException ){` |
|      - | 3625 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|     73 | 3626 | `			if( !(pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame))->bHasRet ){` |
|     69 | 3627 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|     69 | 3628 | `				pVm->pPendingException = 0;` |
|     69 | 3629 | `				VmExcRelease(&(*pVm),pException);` |
|      - | 3630 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|     69 | 3631 | `				pThis = pReThrow;` |
|     69 | 3632 | `				goto Rethrow;` |
|      - | 3633 | `			}` |
|      - | 3634 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|      6 | 3635 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      6 | 3636 | `			pVm->pPendingException = 0;` |
|      2 | 3637 | `		}` |
|      - | 3638 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|      - | 3639 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|      - | 3640 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|      - | 3641 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|   1953 | 3642 | `		pVm->pResumeFrame = pCatchBody;` |
|   1953 | 3643 | `		pVm->iResumePc = iCatchPc;` |
|   1953 | 3644 | `		pVm->pResumeInstr = pCatchInstr;` |
|   1953 | 3645 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|      - | 3646 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|      - | 3647 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|      - | 3648 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|   1953 | 3649 | `		VmExcRelease(&(*pVm),pException);` |
|      - | 3650 | `	}` |
|   1953 | 3651 | `	return SXRET_OK;` |
|   1350 | 3652 | `}` |
|      - | 3653 |  |
