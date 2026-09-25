# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2472/2758 lines (89.63%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Error, diagnostics and type-enforcement machinery: PH7_VmThrowError` |
|       - |   10 | ` *    and the error-handler invocation path, enum materialization and` |
|       - |   11 | ` *    on-demand class constants, scalar/union/property/constant/return` |
|       - |   12 | ` *    type enforcement, the TypeError/ArgumentCountError throwers,` |
|       - |   13 | ` *    uncaught-exception rendering, VmBuildBacktrace, and the exception` |
|       - |   14 | ` *    core VmUncaughtException/VmThrowException.` |
|       - |   15 | ` * Status:` |
|       - |   16 | ` *    Stable.` |
|       - |   17 | ` */` |
|       - |   18 | `/*` |
|       - |   19 | ` * Remember a diagnostic for error_get_last(). php records the last error that reached` |
|       - |   20 | ` * DEFAULT processing: one hidden by '@' or by error_reporting() still counts, but one a` |
|       - |   21 | ` * user handler claimed (by returning true) does not -- so this is called only on the` |
|       - |   22 | ` * default-processing path.` |
|       - |   23 | ` */` |
|   25310 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|       5 |   25 | `{` |
|   25315 |   26 | `	pVm->nLastErrType = iErr;` |
|   25315 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|   25315 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|   25315 |   29 | `	if( zMsg && nMsg > 0 ){` |
|   25315 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   12655 |   31 | `	}` |
|   25315 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|   25315 |   33 | `	if( pFile ){` |
|   25315 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   12655 |   35 | `	}` |
|   25315 |   36 | `}` |
|       - |   37 | `/*` |
|       - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|       - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|       - |   40 | ` * diagnostic is never silently swallowed.` |
|       - |   41 | ` */` |
|     804 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|       4 |   43 | `{` |
|     808 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|       4 |   45 | `}` |
|       - |   46 | `/*` |
|       - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|       - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|       - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|       - |   50 | ` * headers_sent()/output accounting).` |
|       - |   51 | ` */` |
|     830 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|       4 |   53 | `{` |
|       - |   54 | `	sxi32 rc;` |
|       - |   55 | `	/* Append a new line */` |
|       - |   56 | `#ifdef __WINNT__` |
|       4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|       - |   58 | `#else` |
|     830 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|       - |   60 | `#endif` |
|       - |   61 | `	/* Invoke the output consumer callback */` |
|     834 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     834 |   63 | `	if( bTrack ){` |
|      29 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      13 |   65 | `	}` |
|     834 |   66 | `	return rc;` |
|       4 |   67 | `}` |
|       - |   68 | `/*` |
|       - |   69 | ` * Route an already-formatted diagnostic blob (the uncaught-exception path builds` |
|       - |   70 | `` * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream`` |
|       - |   71 | ` * when log_errors is on, else to the program-output stream when display_errors` |
|       - |   72 | ` * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).` |
|       - |   73 | ` */` |
|     592 |   74 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|       4 |   75 | `{` |
|     596 |   76 | `	if( pVm->bLogErrors ){` |
|     596 |   77 | `		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);` |
|       - |   78 | `	}` |
|     ! 0 |   79 | `	if( pVm->bDisplayErrors ){` |
|     ! 0 |   80 | `		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);` |
|       - |   81 | `	}` |
|     ! 0 |   82 | `	return SXRET_OK;` |
|     300 |   83 | `}` |
|       - |   84 | `/*` |
|       - |   85 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |   86 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|       - |   87 | ` * information.` |
|       - |   88 | ` */` |
|   26832 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|       5 |   90 | `{` |
|       - |   91 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|       - |   92 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|       - |   93 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|       - |   94 | `	 * straight through to the engine's own reporting, which is what returning` |
|       - |   95 | `	 * TRUE below means. */` |
|   26832 |   96 | `	if( ph7_value_is_callable(&pVm->sErrCB)` |
|   14192 |   97 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
|       - |   98 | `		ph7_value apArg[4];` |
|       - |   99 | `		ph7_value *apArgPtr[4];` |
|       - |  100 | `		ph7_value sResult;` |
|       - |  101 | `		ph7_value sRunning;` |
|       - |  102 | `		SyString sErr;` |
|       - |  103 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|       - |  104 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|       - |  105 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|       - |  106 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|       - |  107 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|    1531 |  108 | `		if( iErr == PH7_CTX_NOTICE ){` |
|     206 |  109 | `			iErr = 8; /* E_NOTICE */` |
|     101 |  110 | `		}` |
|       - |  111 | `		/* Prepare arguments */` |
|    1531 |  112 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|       - |  113 | `			/* use explicit message length to avoid reading past buffer */` |
|    1531 |  114 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|    1531 |  115 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|    1531 |  116 | `		if( pFile ){` |
|    1531 |  117 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|    1531 |  118 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     768 |  119 | `		}else{` |
|     ! 0 |  120 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|       - |  121 | `		}` |
|    1531 |  122 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|    1531 |  123 | `		PH7_MemObjInit(pVm,&sResult);` |
|       - |  124 | `		/* Set up pointer array */` |
|    1531 |  125 | `		apArgPtr[0] = &apArg[0];` |
|    1531 |  126 | `		apArgPtr[1] = &apArg[1];` |
|    1531 |  127 | `		apArgPtr[2] = &apArg[2];` |
|    1531 |  128 | `		apArgPtr[3] = &apArg[3];` |
|       - |  129 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|       - |  130 | `		 * handler itself raises reaches the engine's reporting instead of` |
|       - |  131 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|       - |  132 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|       - |  133 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|       - |  134 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|       - |  135 | `		 * the handler installed itself STAYS. */` |
|    1531 |  136 | `		PH7_MemObjInit(pVm,&sRunning);` |
|    1531 |  137 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|    1531 |  138 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|    1531 |  139 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|       - |  140 | `		/* Call the handler */` |
|       - |  141 | `		{` |
|    1531 |  142 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|    1531 |  143 | `			if( !ph7_value_is_callable(&pVm->sErrCB) ){` |
|    1529 |  144 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     762 |  145 | `			}` |
|    1531 |  146 | `			PH7_MemObjRelease(&sRunning);` |
|    1531 |  147 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|       - |  148 | `				/* The handler threw (or aborted) instead of returning: php never` |
|       - |  149 | `				 * reports the original diagnostic then — the exception supersedes` |
|       - |  150 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|       - |  151 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|       - |  152 | `				 * user's catch already ran. */` |
|       5 |  153 | `				PH7_MemObjRelease(&apArg[0]);` |
|       5 |  154 | `				PH7_MemObjRelease(&apArg[1]);` |
|       5 |  155 | `				PH7_MemObjRelease(&apArg[2]);` |
|       5 |  156 | `				PH7_MemObjRelease(&apArg[3]);` |
|       5 |  157 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  158 | `				return FALSE;` |
|       - |  159 | `			}` |
|       - |  160 | `		}` |
|       - |  161 | `		/* Check return value */` |
|    1527 |  162 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 |  163 | `			PH7_MemObjToBool(&sResult);` |
|     ! 0 |  164 | `		}` |
|       - |  165 | `		/* Release */` |
|    1527 |  166 | `		PH7_MemObjRelease(&apArg[0]);` |
|    1527 |  167 | `		PH7_MemObjRelease(&apArg[1]);` |
|    1527 |  168 | `		PH7_MemObjRelease(&apArg[2]);` |
|    1527 |  169 | `		PH7_MemObjRelease(&apArg[3]);` |
|    1527 |  170 | `		PH7_MemObjRelease(&sResult);` |
|       - |  171 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|       - |  172 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|    1527 |  173 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|       - |  174 | `	}` |
|       - |  175 | `	/* No handler, always call error handler */` |
|   25311 |  176 | `	return TRUE;` |
|   13421 |  177 | `}` |
|       - |  178 | `/*` |
|       - |  179 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|       - |  180 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|       - |  181 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|       - |  182 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|       - |  183 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|       - |  184 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|       - |  185 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|       - |  186 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|       - |  187 | ` * only picks the DISPLAY label.` |
|       - |  188 | ` */` |
|       - |  189 | `/*` |
|       - |  190 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|       - |  191 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|       - |  192 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|       - |  193 | ` * every deprecation.` |
|       - |  194 | ` */` |
|   22708 |  195 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|       5 |  196 | `{` |
|   22713 |  197 | `	switch( iErr ){` |
|   11077 |  198 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|   22159 |  199 | `		return 2;` |
|      21 |  200 | `	case 512  /* E_USER_WARNING */:` |
|      45 |  201 | `		return 512;` |
|     141 |  202 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|       - |  203 | `	case 8    /* E_NOTICE */:` |
|     286 |  204 | `		return 8;` |
|      35 |  205 | `	case 1024 /* E_USER_NOTICE */:` |
|      74 |  206 | `		return 1024;` |
|      45 |  207 | `	case 8192 /* E_DEPRECATED */:` |
|      94 |  208 | `		return 8192;` |
|      21 |  209 | `	case 16384 /* E_USER_DEPRECATED */:` |
|      43 |  210 | `		return 16384;` |
|     ! 0 |  211 | `	case 256  /* E_USER_ERROR */:` |
|     ! 0 |  212 | `		return 256;` |
|      14 |  213 | `	default:` |
|      32 |  214 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|       - |  215 | `	}` |
|   11359 |  216 | `}` |
|   25310 |  217 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|       5 |  218 | `{` |
|   25315 |  219 | `	if( !pVm->bErrReport ){` |
|    4147 |  220 | `		return 0;` |
|       - |  221 | `	}` |
|   21171 |  222 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|   12660 |  223 | `}` |
|     238 |  224 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|       4 |  225 | `{` |
|     242 |  226 | `	switch(iErr){` |
|      86 |  227 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|       - |  228 | `	case 512  /* E_USER_WARNING */:` |
|     176 |  229 | `		return "Warning";` |
|      19 |  230 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|       - |  231 | `	case 8    /* E_NOTICE */:` |
|       - |  232 | `	case 1024 /* E_USER_NOTICE */:` |
|      41 |  233 | `		return "Notice";` |
|     ! 0 |  234 | `	case 8192  /* E_DEPRECATED */:` |
|       - |  235 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  236 | `		return "Deprecated";` |
|     ! 0 |  237 | `	case 256 /* E_USER_ERROR */:` |
|     ! 0 |  238 | `		return "Fatal error";` |
|      14 |  239 | `	default:` |
|      32 |  240 | `		return "Error";` |
|       - |  241 | `	}` |
|     123 |  242 | `}` |
|       - |  243 | `/*` |
|       - |  244 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|       - |  245 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|       - |  246 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|       - |  247 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|       - |  248 | ` * be authored cross-engine with --EXPECTF--.` |
|       - |  249 | ` */` |
|      26 |  250 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|       3 |  251 | `{` |
|      29 |  252 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|      29 |  253 | `}` |
|     238 |  254 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|       4 |  255 | `{` |
|     242 |  256 | `	if( pFile ){` |
|     361 |  257 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|     119 |  258 | `			nLine ? nLine : 1);` |
|     119 |  259 | `	}` |
|     242 |  260 | `}` |
|       - |  261 | `/*` |
|       - |  262 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|       - |  263 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|       - |  264 | ` * (compile.c) and stock CLI's stderr log copy.` |
|       - |  265 | ` */` |
|     212 |  266 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|       4 |  267 | `{` |
|     216 |  268 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|     216 |  269 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|     216 |  270 | `}` |
|       - |  271 | `/*` |
|       - |  272 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|       - |  273 | ` *` |
|       - |  274 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|       - |  275 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|       - |  276 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|       - |  277 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|       - |  278 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|       - |  279 | ` * php never produces. Building it into the message here is the single place that` |
|       - |  280 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|       - |  281 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|       - |  282 | ` */` |
|   26304 |  283 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|       5 |  284 | `{` |
|   26309 |  285 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     661 |  286 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     661 |  287 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|     328 |  288 | `	}` |
|   26309 |  289 | `}` |
|       - |  290 | `/*` |
|       - |  291 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|       - |  292 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|       - |  293 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|       - |  294 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|       - |  295 | ` *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:` |
|       - |  296 | ``  *                     `\nLABEL: BODY in FILE on line N` `` |
|       - |  297 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|       - |  298 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|       - |  299 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|       - |  300 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|       - |  301 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|       - |  302 | ` */` |
|     234 |  303 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|       - |  304 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|       4 |  305 | `{` |
|     238 |  306 | `	SyBlob *pWorker = &pVm->sWorker;` |
|     238 |  307 | `	sxi32 rc = SXRET_OK;` |
|     238 |  308 | `	if( pVm->bLogErrors ){` |
|     216 |  309 | `		SyBlobReset(pWorker);` |
|     216 |  310 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|     216 |  311 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|     216 |  312 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|     216 |  313 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|     106 |  314 | `	}` |
|     238 |  315 | `	if( pVm->bDisplayErrors ){` |
|       - |  316 | `		sxi32 rc2;` |
|      29 |  317 | `		SyBlobReset(pWorker);` |
|       - |  318 | `		/* php's text-mode display copy is prefixed with a blank line */` |
|      29 |  319 | `		SyBlobAppend(pWorker,"\n",sizeof(char));` |
|      29 |  320 | `		VmDiagnosticHeader(pWorker,iErr);` |
|      29 |  321 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      29 |  322 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      29 |  323 | `		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);` |
|       - |  324 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|       - |  325 | `		 * than letting a later successful write mask it */` |
|      29 |  326 | `		if( rc == SXRET_OK ){` |
|      29 |  327 | `			rc = rc2;` |
|      13 |  328 | `		}` |
|      13 |  329 | `	}` |
|     238 |  330 | `	return rc;` |
|       4 |  331 | `}` |
|     614 |  332 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|       - |  333 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  334 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  335 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|       - |  336 | `	const char *zMessage /* Null terminated error message */` |
|       - |  337 | `	)` |
|       5 |  338 | `{` |
|       - |  339 | `	SyBlob sMsg;` |
|       - |  340 | `	SyString *pFile;` |
|     619 |  341 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     619 |  342 | `	sxi32 rc = SXRET_OK;` |
|       - |  343 | `	/* Peek the processed file if available */` |
|     619 |  344 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     619 |  345 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     619 |  346 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|       - |  347 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|       - |  348 | `		 * out-of-memory fatal through this path with none, and must not need` |
|       - |  349 | `		 * an allocation to say so. */` |
|      90 |  350 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      90 |  351 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      90 |  352 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      90 |  353 | `		nMsg = SyBlobLength(&sMsg);` |
|      43 |  354 | `	}` |
|       - |  355 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|       - |  356 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     619 |  357 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|     155 |  358 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|     155 |  359 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|       - |  360 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|       - |  361 | `			 * '@' php still runs the handler (done just above) but prints` |
|       - |  362 | `			 * nothing itself. */` |
|     126 |  363 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|      61 |  364 | `		}` |
|      75 |  365 | `	}` |
|     619 |  366 | `	SyBlobRelease(&sMsg);` |
|     619 |  367 | `	return rc;` |
|       5 |  368 | `}` |
|       - |  369 | `/*` |
|       - |  370 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|       - |  371 | ` *` |
|       - |  372 | ` * This is the single choke point for surfacing an allocation failure that would` |
|       - |  373 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|       - |  374 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|       - |  375 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|       - |  376 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|       - |  377 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|       - |  378 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|       - |  379 | ` * calling it from a VM op.` |
|       - |  380 | ` */` |
|     ! 0 |  381 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|     ! 0 |  382 | `{` |
|     ! 0 |  383 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|       - |  384 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|     ! 0 |  385 | `	pVm->iExitStatus = 255;` |
|     ! 0 |  386 | `	pVm->bHaltRequested = 1;` |
|     ! 0 |  387 | `	return PH7_ABORT;` |
|     ! 0 |  388 | `}` |
|       - |  389 | `/*` |
|       - |  390 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|       - |  391 | ` */` |
|     ! 0 |  392 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|     ! 0 |  393 | `{` |
|     ! 0 |  394 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|     ! 0 |  395 | `}` |
|       - |  396 | `/*` |
|       - |  397 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|       - |  398 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|       - |  399 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|       - |  400 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|       - |  401 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|       - |  402 | ` */` |
|       - |  403 | `/*` |
|       - |  404 | `` * TRUE when reading pVal as an `int` would lose what it holds -- the event php`` |
|       - |  405 | `` * 8.1 only DEPRECATES (`Implicit conversion from float 1.9 / float-string "1.9"`` |
|       - |  406 | `` * to int loses precision`) and §10 refuses outright.`` |
|       - |  407 | ` *` |
|       - |  408 | ` * Two kinds of value can lose something, and php treats them as one: a FLOAT,` |
|       - |  409 | ` * and a numeric STRING whose bytes spell a double. Which strings those are is` |
|       - |  410 | ` * not just the ones carrying a '.' or an exponent — an integer-shaped run too` |
|       - |  411 | ` * long for an int64 is a double in php too ("99999999999999999999"), and it` |
|       - |  412 | ` * loses digits exactly the same way. So the question is asked of the NUMBER the` |
|       - |  413 | ` * value converts to, whatever spelling it arrived in.` |
|       - |  414 | ` *` |
|       - |  415 | ` * A value is lossy when that number is not an exact int64: outside the range at` |
|       - |  416 | ` * all (NaN and the infinities included), or carrying a fraction. An integral` |
|       - |  417 | `` * float in range (`4.0 % 3`, `$o->i = 5.0`) loses nothing and is not lossy.`` |
|       - |  418 | ` */` |
|  105502 |  419 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|       5 |  420 | `{` |
|       - |  421 | `	ph7_real r;` |
|  105507 |  422 | `	if( pVal == 0 ){` |
|     ! 0 |  423 | `		return FALSE;` |
|       - |  424 | `	}` |
|  105507 |  425 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|      37 |  426 | `		r = pVal->rVal;` |
|  105512 |  427 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|       - |  428 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|       - |  429 | `		 * no, and a numeric conversion would replace it. */` |
|       - |  430 | `		ph7_value sProbe;` |
|       - |  431 | `		SyString sStr;` |
|       - |  432 | `		int bReal;` |
|  100163 |  433 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|  100163 |  434 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|       - |  435 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|       - |  436 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|       - |  437 | `		 * answers without building anything. Conservative on purpose: it may` |
|       - |  438 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|  100163 |  439 | `		if( n < 19 ){` |
|  300431 |  440 | `			for( i = 0 ; i < n ; ++i ){` |
|  200321 |  441 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|      23 |  442 | `					break;` |
|       - |  443 | `				}` |
|  100142 |  444 | `			}` |
|  100157 |  445 | `			if( i >= n ){` |
|  100117 |  446 | `				return FALSE;` |
|       - |  447 | `			}` |
|      21 |  448 | `		}` |
|      50 |  449 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|      50 |  450 | `		PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|      50 |  451 | `		PH7_MemObjToNumeric(&sProbe);` |
|      50 |  452 | `		bReal = (sProbe.iFlags & MEMOBJ_REAL) != 0;` |
|      50 |  453 | `		r = sProbe.rVal;` |
|      50 |  454 | `		PH7_MemObjRelease(&sProbe);` |
|      50 |  455 | `		if( !bReal ){` |
|       5 |  456 | `			return FALSE;` |
|       - |  457 | `		}` |
|      24 |  458 | `	}else{` |
|    5315 |  459 | `		return FALSE;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* The bounds are tested in DOUBLE space, BEFORE the cast: (sxi64)r is` |
|       - |  462 | `	 * undefined outside them, and a test of an undefined cast's result is one an` |
|       - |  463 | `	 * optimiser is entitled to delete -- which is exactly how the printf family's` |
|       - |  464 | `	 * PHP_INT_MIN guard disappeared (§2). NaN fails both comparisons and either` |
|       - |  465 | `	 * infinity fails one, so all three are lossy without a libm predicate. */` |
|      81 |  466 | `	if( !(r >= -9223372036854775808.0 && r < 9223372036854775808.0) ){` |
|      26 |  467 | `		return TRUE;` |
|       - |  468 | `	}` |
|      57 |  469 | `	return r != (ph7_real)(sxi64)r;` |
|   52756 |  470 | `}` |
|       - |  471 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|       - |  472 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|       - |  473 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|       - |  474 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|       - |  475 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|    5098 |  476 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|       5 |  477 | `{` |
|    5103 |  478 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|    5077 |  479 | `		return SXRET_OK;` |
|       - |  480 | `	}` |
|      27 |  481 | `	return VmThrowFixedError(pVm,"TypeError",` |
|      26 |  482 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|       - |  483 | `			? "Implicit conversion from float to int loses precision"` |
|       - |  484 | `			: "Implicit conversion from float-string to int loses precision");` |
|    2554 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|       - |  488 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|       - |  489 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|       - |  490 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|       - |  491 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|       - |  492 | ` */` |
| 2255445 |  493 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|       5 |  494 | `{` |
|       - |  495 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|       - |  496 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
| 2255450 |  497 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|       5 |  498 | `}` |
|       - |  499 | `/*` |
|       - |  500 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|       - |  501 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|       - |  502 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|       - |  503 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|       - |  504 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|       - |  505 | ` * keep native re-entries off a finite C stack).` |
|       - |  506 | ` */` |
| 3386867 |  507 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|       5 |  508 | `{` |
| 3386872 |  509 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|       5 |  510 | `}` |
|       - |  511 | `/*` |
|       - |  512 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|       - |  513 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|       - |  514 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|       - |  515 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|       - |  516 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|       - |  517 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|       - |  518 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|       - |  519 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|       - |  520 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|       - |  521 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|       - |  522 | ` * (VmNativeNestingFatal).` |
|       - |  523 | ` *` |
|       - |  524 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|       - |  525 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|       - |  526 | ` * re-enter and loop.` |
|       - |  527 | ` */` |
|       2 |  528 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|       1 |  529 | `{` |
|       3 |  530 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  531 | `		return PH7_ABORT;` |
|       - |  532 | `	}` |
|       3 |  533 | `	pVm->iExitStatus = 255;` |
|       3 |  534 | `	pVm->bHaltRequested = 1;` |
|       3 |  535 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|       3 |  536 | `	return PH7_ABORT;` |
|       2 |  537 | `}` |
|       - |  538 | `/*` |
|       - |  539 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|       - |  540 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|       - |  541 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|       - |  542 | ` */` |
|       4 |  543 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|       1 |  544 | `{` |
|       5 |  545 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  546 | `		return PH7_ABORT;` |
|       - |  547 | `	}` |
|       5 |  548 | `	pVm->iExitStatus = 255;` |
|       5 |  549 | `	pVm->bHaltRequested = 1;` |
|       5 |  550 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|       5 |  551 | `	return PH7_ABORT;` |
|       3 |  552 | `}` |
|       - |  553 | `/*` |
|       - |  554 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |  555 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - |  556 | ` * information.` |
|       - |  557 | ` */` |
|   26218 |  558 | `static sxi32 VmThrowErrorAp(` |
|       - |  559 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  560 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  561 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|       - |  562 | `	const char *zFormat, /* Format message */` |
|       - |  563 | `	va_list ap           /* Variable list of arguments */` |
|       - |  564 | `	)` |
|       5 |  565 | `{` |
|       - |  566 | `	SyBlob sMsg;` |
|       - |  567 | `	SyString *pFile;` |
|   26223 |  568 | `	sxi32 rc = SXRET_OK;` |
|       - |  569 | `	/* Peek the processed file if available */` |
|   26223 |  570 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - |  571 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|   26223 |  572 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|   26223 |  573 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|   26223 |  574 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - |  575 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|       - |  576 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|       - |  577 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|       - |  578 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|   26223 |  579 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|       - |  580 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|       - |  581 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|   25165 |  582 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|   25165 |  583 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|   25053 |  584 | `			SyBlobRelease(&sMsg);` |
|   25053 |  585 | `			return SXRET_OK;` |
|       - |  586 | `		}` |
|     172 |  587 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      56 |  588 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|      56 |  589 | `	}` |
|    1175 |  590 | `	SyBlobRelease(&sMsg);` |
|    1175 |  591 | `	return rc;` |
|   13114 |  592 | `}` |
|       - |  593 | `/*` |
|       - |  594 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|       - |  595 | ` * scope), or NULL when executing outside any class context.` |
|       - |  596 | ` */` |
|    8800 |  597 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|       5 |  598 | `{` |
|    8805 |  599 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|     139 |  600 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|     139 |  601 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|       - |  602 | `	}` |
|    8671 |  603 | `	return 0;` |
|    4405 |  604 | `}` |
|       - |  605 | `/*` |
|       - |  606 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|       - |  607 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|       - |  608 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|       - |  609 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|       - |  610 | ` * as an undefined function and therefore as another engine throw). Each such` |
|       - |  611 | ` * throw would construct another exception and recurse until the native-nesting` |
|       - |  612 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|       - |  613 | ` * (an engine throw from inside a user exception's constructor) working and` |
|       - |  614 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|       - |  615 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|       - |  616 | ` * after the call.` |
|       - |  617 | ` */` |
|       - |  618 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|  346356 |  619 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|       5 |  620 | `{` |
|  346361 |  621 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|     ! 0 |  622 | `		return 0;` |
|       - |  623 | `	}` |
|  346361 |  624 | `	pVm->nExcCtorDepth++;` |
|  346361 |  625 | `	return 1;` |
|  173183 |  626 | `}` |
|       - |  627 | `/*` |
|       - |  628 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|       - |  629 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|       - |  630 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|       - |  631 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|       - |  632 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|       - |  633 | ` */` |
|  205360 |  634 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|       5 |  635 | `{` |
|       - |  636 | `	ph7_class *pErrClass;` |
|       - |  637 | `	ph7_class_instance *pThis;` |
|       - |  638 | `	ph7_class_method *pCons;` |
|       - |  639 | `	VmFrame *pFrame;` |
|       - |  640 | `	sxi32 rc;` |
|  205365 |  641 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|  205365 |  642 | `	if( pErrClass == 0 ){` |
|     ! 0 |  643 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  644 | `		return PH7_ABORT;` |
|       - |  645 | `	}` |
|  205365 |  646 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|  205365 |  647 | `	if( pThis == 0 ){` |
|     ! 0 |  648 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  649 | `		return PH7_ABORT;` |
|       - |  650 | `	}` |
|  205365 |  651 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|  205365 |  652 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|       - |  653 | `		ph7_value sArg;` |
|       - |  654 | `		ph7_value *apArg[1];` |
|       - |  655 | `		SyString sMsgStr;` |
|  205365 |  656 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|  205365 |  657 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|  205365 |  658 | `		apArg[0] = &sArg;` |
|  205365 |  659 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|  205365 |  660 | `		PH7_MemObjRelease(&sArg);` |
|  205365 |  661 | `		pVm->nExcCtorDepth--;` |
|  102680 |  662 | `	}` |
|  205365 |  663 | `	SyBlobRelease(pMsg);` |
|  205365 |  664 | `	pFrame = pVm->pFrame;` |
|  205365 |  665 | `	if( pFrame ){` |
|  205365 |  666 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  205365 |  667 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|  102680 |  668 | `	}` |
|  205365 |  669 | `	rc = VmThrowException(&(*pVm),pThis);` |
|  205365 |  670 | `	PH7_ClassInstanceUnref(pThis);` |
|  205365 |  671 | `	if( rc == SXERR_ABORT ){` |
|      30 |  672 | `		return PH7_ABORT;` |
|       - |  673 | `	}` |
|  205339 |  674 | `	return PH7_EXCEPTION;` |
|  102685 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|       - |  678 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|       - |  679 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|       - |  680 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|       - |  681 | ` * result through its normal goto Exception / goto Abort.` |
|       - |  682 | ` */` |
|      50 |  683 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|       4 |  684 | `{` |
|       - |  685 | `	SyBlob sMsg;` |
|      54 |  686 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      54 |  687 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      54 |  688 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|       4 |  689 | `}` |
|       - |  690 | `/*` |
|       - |  691 | ` * Enum case singletons (PHP 8.1).` |
|       - |  692 | ` *` |
|       - |  693 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|       - |  694 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|       - |  695 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|       - |  696 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|       - |  697 | ` * backing-value type check and the duplicate-value check only fire when a` |
|       - |  698 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|       - |  699 | ` */` |
|       - |  700 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|       - |  701 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|       - |  702 | ` * readonly property" through the normal store path. */` |
|       - |  703 |  |
|       - |  704 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|       - |  705 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|     410 |  706 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|       2 |  707 | `{` |
|     412 |  708 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|       - |  709 | `	ph7_class_instance *pObj;` |
|       - |  710 | `	SyHashEntry *pEntry;` |
|     412 |  711 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      59 |  712 | `		return 0;` |
|       - |  713 | `	}` |
|     354 |  714 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|     354 |  715 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|     354 |  716 | `	if( pEntry == 0 ){` |
|     ! 0 |  717 | `		return 0;` |
|       - |  718 | `	}` |
|     354 |  719 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|     207 |  720 | `}` |
|       - |  721 | `/*` |
|       - |  722 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|       - |  723 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|       - |  724 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|       - |  725 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|       - |  726 | ` * raises it. Returns the throw status to park/route.` |
|       - |  727 | ` */` |
|       2 |  728 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|       1 |  729 | `{` |
|       - |  730 | `	SyBlob sMsg;` |
|       3 |  731 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|       3 |  732 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|       3 |  733 | `	pVm->pConstCycleAttr = 0;` |
|       3 |  734 | `	pVm->pConstCycleClass = 0;` |
|       3 |  735 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  736 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|       1 |  737 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|       3 |  738 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  739 | `}` |
|       - |  740 | `/*` |
|       - |  741 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|       - |  742 | ` * materialize lazily and individually on first access — the backing-value` |
|       - |  743 | ` * type check fires per case, and the duplicate-value check compares only` |
|       - |  744 | ` * against cases that have already materialized (a broken sibling case does` |
|       - |  745 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|       - |  746 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|       - |  747 | ` * (duplicate value / self-reference) — which the caller routes` |
|       - |  748 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|       - |  749 | ` */` |
|     482 |  750 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|       5 |  751 | `{` |
|       - |  752 | `	ph7_class_attr **apCase;` |
|       - |  753 | `	ph7_class_instance *pObj;` |
|       - |  754 | `	ph7_value *pSlot;` |
|       - |  755 | `	ph7_value sBacking,sPropVal;` |
|       - |  756 | `	sxu32 i;` |
|     487 |  757 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|     369 |  758 | `		return SXRET_OK;` |
|       - |  759 | `	}` |
|     119 |  760 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  761 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|       - |  762 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|     ! 0 |  763 | `		if( pVm->pConstCycleAttr == 0 ){` |
|     ! 0 |  764 | `			pVm->pConstCycleAttr = pCase;` |
|     ! 0 |  765 | `			pVm->pConstCycleClass = pClass;` |
|     ! 0 |  766 | `		}` |
|     ! 0 |  767 | `		return SXRET_OK;` |
|       - |  768 | `	}` |
|     119 |  769 | `	PH7_MemObjInit(pVm,&sBacking);` |
|     119 |  770 | `	if( pClass->nEnumBacking != 0 ){` |
|      90 |  771 | `		if( pCase->pNativeValue ){` |
|       - |  772 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|       - |  773 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|       - |  774 | `			 * is what that byte-code would have produced anyway. */` |
|       5 |  775 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|      88 |  776 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|       - |  777 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|      86 |  778 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       - |  779 | `			sxi32 rcExec;` |
|      86 |  780 | `			pVm->pConstEvalClass = pClass;` |
|      86 |  781 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      86 |  782 | `			pVm->nConstEvalDepth++;` |
|      86 |  783 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      86 |  784 | `			pVm->nConstEvalDepth--;` |
|      86 |  785 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      86 |  786 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      86 |  787 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  788 | `				/* The backing expression raised: abandon materialization and` |
|       - |  789 | `				 * hand the status to the caller to park/route. */` |
|       3 |  790 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  791 | `				return rcExec;` |
|       - |  792 | `			}` |
|      84 |  793 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|     ! 0 |  794 | `				PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  795 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  796 | `			}` |
|      40 |  797 | `		}` |
|      88 |  798 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|       - |  799 | `			/* php: TypeError, checked lazily at first case access */` |
|       - |  800 | `			SyBlob sMsg;` |
|       3 |  801 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|       3 |  802 | `			PH7_MemObjRelease(&sBacking);` |
|       3 |  803 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       2 |  804 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|       2 |  805 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|       3 |  806 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - |  807 | `		}` |
|      86 |  808 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       - |  809 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|       - |  810 | `			 * the typed-constant leniency) to a genuine int. */` |
|      17 |  811 | `			PH7_MemObjToInteger(&sBacking);` |
|       9 |  812 | `		}else{` |
|      70 |  813 | `			PH7_MemObjToString(&sBacking);` |
|       - |  814 | `		}` |
|       - |  815 | `		/* php: two cases sharing one backing value are an Error — compared` |
|       - |  816 | `		 * against already-materialized cases only (php registers values as` |
|       - |  817 | `		 * each case evaluates). */` |
|      86 |  818 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     278 |  819 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|       - |  820 | `			ph7_value *pPrev;` |
|     198 |  821 | `			int bDup = 0;` |
|     198 |  822 | `			if( apCase[i] == pCase ){` |
|      84 |  823 | `				continue;` |
|       - |  824 | `			}` |
|     115 |  825 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|     115 |  826 | `			if( pPrev ){` |
|      57 |  827 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       7 |  828 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|       4 |  829 | `				}else{` |
|      63 |  830 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|      50 |  831 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|      24 |  832 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|       - |  833 | `				}` |
|      28 |  834 | `			}` |
|     115 |  835 | `			if( bDup ){` |
|       - |  836 | `				/* php prints the two cases in DECLARATION order regardless of` |
|       - |  837 | `				 * which one is being evaluated. */` |
|       3 |  838 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|       - |  839 | `				SyBlob sMsg;` |
|       - |  840 | `				sxu32 j;` |
|       5 |  841 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|       5 |  842 | `					if( apCase[j] == pCase ){ break; }` |
|       2 |  843 | `				}` |
|       3 |  844 | `				if( j < i ){` |
|     ! 0 |  845 | `					pFirst = pCase;` |
|     ! 0 |  846 | `					pSecond = apCase[i];` |
|     ! 0 |  847 | `				}` |
|       3 |  848 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  849 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  850 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|       1 |  851 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|       3 |  852 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - |  853 | `			}` |
|      57 |  854 | `		}` |
|      40 |  855 | `	}` |
|       - |  856 | `	/* Create the singleton and fill its readonly props */` |
|     113 |  857 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|     113 |  858 | `	if( pObj == 0 ){` |
|     ! 0 |  859 | `		PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  860 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  861 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|     ! 0 |  862 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  863 | `		return PH7_ABORT;` |
|       - |  864 | `	}` |
|     113 |  865 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|     113 |  866 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|     113 |  867 | `	PH7_MemObjRelease(&sPropVal);` |
|     113 |  868 | `	if( pClass->nEnumBacking != 0 ){` |
|      84 |  869 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|      40 |  870 | `	}` |
|     113 |  871 | `	PH7_MemObjRelease(&sBacking);` |
|       - |  872 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|       - |  873 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|     113 |  874 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|     113 |  875 | `	if( pSlot == 0 ){` |
|     ! 0 |  876 | `		PH7_ClassInstanceUnref(pObj);` |
|     ! 0 |  877 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  878 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|     ! 0 |  879 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  880 | `		return PH7_ABORT;` |
|       - |  881 | `	}` |
|     113 |  882 | `	pSlot->x.pOther = pObj;` |
|     113 |  883 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|     113 |  884 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     113 |  885 | `	pCase->nIdx = pSlot->nIdx;` |
|     113 |  886 | `	return SXRET_OK;` |
|     246 |  887 | `}` |
|       - |  888 | `/*` |
|       - |  889 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|       - |  890 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|       - |  891 | ` * there, so a broken case surfaces its error at the same point).` |
|       - |  892 | ` */` |
|     246 |  893 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  894 | `{` |
|       - |  895 | `	ph7_class_attr **apCase;` |
|       - |  896 | `	sxu32 n;` |
|     251 |  897 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 |  898 | `		return SXRET_OK;` |
|       - |  899 | `	}` |
|     251 |  900 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     711 |  901 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     471 |  902 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|     471 |  903 | `		if( rc != SXRET_OK ){` |
|       8 |  904 | `			return rc;` |
|       - |  905 | `		}` |
|     235 |  906 | `	}` |
|     245 |  907 | `	return SXRET_OK;` |
|     128 |  908 | `}` |
|       - |  909 | `/*` |
|       - |  910 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|       - |  911 | ` * or 0 when the name does not name an enum.` |
|       - |  912 | ` */` |
|     204 |  913 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|       3 |  914 | `{` |
|       - |  915 | `	ph7_class *pClass;` |
|     207 |  916 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|     ! 0 |  917 | `		return 0;` |
|       - |  918 | `	}` |
|     309 |  919 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|     102 |  920 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|     209 |  921 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|       3 |  922 | `		pClass = pClass->pNextName;` |
|       1 |  923 | `	}` |
|     207 |  924 | `	return pClass;` |
|     105 |  925 | `}` |
|       - |  926 | `/*` |
|       - |  927 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|       - |  928 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|       - |  929 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|       - |  930 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|       - |  931 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|       - |  932 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|       - |  933 | ` */` |
|     428 |  934 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|       5 |  935 | `{` |
|     433 |  936 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     428 |  937 | `	if( pInner && pInner->pUserData` |
|     254 |  938 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|     ! 0 |  939 | `		return 0;` |
|       - |  940 | `	}` |
|     433 |  941 | `	return pVm->nCurLine;` |
|     219 |  942 | `}` |
|       - |  943 | `/*` |
|       - |  944 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|       - |  945 | ` * not the pointer: aMemObj is a by-value SySet, so any nested evaluation (an` |
|       - |  946 | ` * initializer, or the constructor of the very TypeError being raised) can grow` |
|       - |  947 | ` * and REALLOC the pool, leaving a pointer taken before it dangling — the rule` |
|       - |  948 | ` * VmLocalExecIntoObj is built around. The slot's contents are released first:` |
|       - |  949 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|       - |  950 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|       - |  951 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|       - |  952 | ` */` |
|      36 |  953 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|       3 |  954 | `{` |
|      39 |  955 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       - |  956 | `	VmSlot sSlot;` |
|      39 |  957 | `	if( pObj == 0 ){` |
|     ! 0 |  958 | `		return;` |
|       - |  959 | `	}` |
|      39 |  960 | `	PH7_MemObjRelease(pObj);` |
|      39 |  961 | `	sSlot.nIdx = nIdx;` |
|      39 |  962 | `	sSlot.pUserData = 0;` |
|      39 |  963 | `	SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      21 |  964 | `}` |
|       - |  965 | `/*` |
|       - |  966 | ` * Evaluate a class constant's initializer on demand.` |
|       - |  967 | ` *` |
|       - |  968 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|       - |  969 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|       - |  970 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|       - |  971 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|       - |  972 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|       - |  973 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|       - |  974 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|       - |  975 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|       - |  976 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|       - |  977 | ` * "Cannot declare self-referencing constant" Error.` |
|       - |  978 | ` */` |
|     482 |  979 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  980 | `{` |
|       - |  981 | `	ph7_value *pMemObj;` |
|     482 |  982 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     482 |  983 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     487 |  984 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|     ! 0 |  985 | `		return SXRET_OK;` |
|       - |  986 | `	}` |
|     487 |  987 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  988 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|       - |  989 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|       - |  990 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|       3 |  991 | `		if( pVm->pConstCycleAttr == 0 ){` |
|       3 |  992 | `			pVm->pConstCycleAttr = pAttr;` |
|       3 |  993 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 |  994 | `		}` |
|       3 |  995 | `		return SXRET_OK;` |
|       - |  996 | `	}` |
|     485 |  997 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     485 |  998 | `	if( pMemObj == 0 ){` |
|     ! 0 |  999 | `		return SXERR_MEM;` |
|       - | 1000 | `	}` |
|     485 | 1001 | `	if( pAttr->pNativeValue ){` |
|       - | 1002 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|       - | 1003 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|       - | 1004 | `		 * when they went lazy (17th session), so this path — the only one an` |
|       - | 1005 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|       - | 1006 | `		 * which is why every native class constant read NULL: nothing had declared` |
|       - | 1007 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|       - | 1008 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|       - | 1009 | `		 * failure path to mirror below. */` |
|     113 | 1010 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|     113 | 1011 | `		pAttr->nIdx = pMemObj->nIdx;` |
|     113 | 1012 | `		return SXRET_OK;` |
|       - | 1013 | `	}` |
|     373 | 1014 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     373 | 1015 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     373 | 1016 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|     373 | 1017 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|       - | 1018 | `		sxu32 nSaveLazyLine;` |
|       - | 1019 | `		sxi32 nSaveLazyDepth;` |
|       - | 1020 | `		sxu32 nSlot;` |
|       - | 1021 | `		sxi32 rcExec;` |
|     373 | 1022 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|     373 | 1023 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1024 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|       - | 1025 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|       - | 1026 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|     373 | 1027 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - | 1028 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|       - | 1029 | `		 * throw out of its own bytecode carries. */` |
|     373 | 1030 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|     373 | 1031 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|     373 | 1032 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|     373 | 1033 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|     373 | 1034 | `		pVm->nConstEvalDepth++;` |
|     373 | 1035 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     373 | 1036 | `		pVm->nConstEvalDepth--;` |
|     373 | 1037 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|     373 | 1038 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|     373 | 1039 | `		pVm->pConstEvalClass = pSaveCtx;` |
|     373 | 1040 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|     373 | 1041 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     373 | 1042 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|     368 | 1043 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|     344 | 1044 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|       - | 1045 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|       - | 1046 | `			 * class constant's expression at each access until one of them` |
|       - | 1047 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|       - | 1048 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|       - | 1049 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|       - | 1050 | `			 * the rest of the run — and made a static default that named it` |
|       - | 1051 | `			 * (whose own evaluation is deferred to first access) find it` |
|       - | 1052 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|       - | 1053 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|       - | 1054 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|       - | 1055 | `			 * it is found — an inner level only records it — but the value is` |
|       - | 1056 | `			 * unusable and the next access must be able to detect it again.` |
|       - | 1057 | `			 * No loop: each access runs the initializer once and raises. */` |
|      35 | 1058 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|      35 | 1059 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - | 1060 | `				/* Hand the status to the caller to park/route. */` |
|      32 | 1061 | `				return rcExec;` |
|       - | 1062 | `			}` |
|       3 | 1063 | `			if( pVm->nConstEvalDepth == 0 ){` |
|       - | 1064 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|       - | 1065 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|       3 | 1066 | `				return VmConstCycleThrow(&(*pVm));` |
|       - | 1067 | `			}` |
|     ! 0 | 1068 | `			return SXRET_OK;` |
|       - | 1069 | `		}` |
|     341 | 1070 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       - | 1071 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|       - | 1072 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|       - | 1073 | `			 * access raises again — php re-runs the whole materialization each` |
|       - | 1074 | `			 * time. A pass may widen int -> float in place, which is the value` |
|       - | 1075 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|       - | 1076 | `			 * runs php code that may grow (and realloc) aMemObj — so the slot is` |
|       - | 1077 | `			 * addressed by index from here on, never through pMemObj. */` |
|       5 | 1078 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|       5 | 1079 | `			if( rcType != SXRET_OK ){` |
|       5 | 1080 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|       5 | 1081 | `				return rcType;` |
|       - | 1082 | `			}` |
|     ! 0 | 1083 | `		}` |
|       - | 1084 | `		/* Memoize the value. */` |
|     337 | 1085 | `		pAttr->nIdx = nSlot;` |
|     337 | 1086 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|     337 | 1087 | `		return SXRET_OK;` |
|       - | 1088 | `	}` |
|     ! 0 | 1089 | `	pAttr->nIdx = pMemObj->nIdx;` |
|     ! 0 | 1090 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     ! 0 | 1091 | `	return SXRET_OK;` |
|     246 | 1092 | `}` |
|       - | 1093 | `/*` |
|       - | 1094 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|       - | 1095 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|       - | 1096 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|       - | 1097 | ` */` |
|     136 | 1098 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1099 | `{` |
|     140 | 1100 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      77 | 1101 | `		return SXRET_OK;` |
|       - | 1102 | `	}` |
|      64 | 1103 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|      15 | 1104 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       - | 1105 | `	}` |
|      50 | 1106 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      72 | 1107 | `}` |
|       - | 1108 | `/*` |
|       - | 1109 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|       - | 1110 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|       - | 1111 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|       - | 1112 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|       - | 1113 | ` */` |
|       6 | 1114 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|       1 | 1115 | `{` |
|       - | 1116 | `	SyBlob sMsg;` |
|       7 | 1117 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 | 1118 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       7 | 1119 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1120 | `}` |
|       - | 1121 | `/*` |
|       - | 1122 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|       - | 1123 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|       - | 1124 | ` * raised at the store site here with the same message and the same` |
|       - | 1125 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|       - | 1126 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|       - | 1127 | ` */` |
|       2 | 1128 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|       1 | 1129 | `{` |
|       3 | 1130 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|       3 | 1131 | `	pVm->iExitStatus = 255;` |
|       3 | 1132 | `	pVm->bHaltRequested = 1;` |
|       3 | 1133 | `	return PH7_ABORT;` |
|       1 | 1134 | `}` |
|       - | 1135 | `/*` |
|       - | 1136 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|       - | 1137 | ` * property assignment. Called from the STORE path when coercion is not` |
|       - | 1138 | ` * possible.` |
|       - | 1139 | ` */` |
|  100164 | 1140 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|       5 | 1141 | `{` |
|  100169 | 1142 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|  100169 | 1143 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 1144 | `	char zType[192];` |
|  150251 | 1145 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|   50082 | 1146 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));` |
|       - | 1147 | `	SyBlob sMsg;` |
|  100169 | 1148 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 1149 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|       - | 1150 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|  100169 | 1151 | `	if( pOwner ){` |
|  100169 | 1152 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|   50082 | 1153 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|   50087 | 1154 | `	}else{` |
|     ! 0 | 1155 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",` |
|     ! 0 | 1156 | `			zGiven,&pAttr->sName,zTypeText);` |
|       - | 1157 | `	}` |
|  100169 | 1158 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       5 | 1159 | `}` |
|       - | 1160 | `/*` |
|       - | 1161 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|       - | 1162 | ` */` |
|  100016 | 1163 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 | 1164 | `{` |
|  100021 | 1165 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|  100021 | 1166 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|       - | 1167 | `	SyBlob sMsg;` |
|  100021 | 1168 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|  100021 | 1169 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|   50008 | 1170 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|  100021 | 1171 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       5 | 1172 | `}` |
|       - | 1173 | `/*` |
|       - | 1174 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|       - | 1175 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|       - | 1176 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|       - | 1177 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|       - | 1178 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|       - | 1179 | ` */` |
|       - | 1180 | `/*` |
|       - | 1181 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|       - | 1182 | ` * property from a scope its set-visibility excludes:` |
|       - | 1183 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|       - | 1184 | ` */` |
|      14 | 1185 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1186 | `{` |
|      15 | 1187 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      15 | 1188 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|      15 | 1189 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|       - | 1190 | `	SyBlob sMsg;` |
|      15 | 1191 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 1192 | `	if( pActive ){` |
|       3 | 1193 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|       1 | 1194 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|       2 | 1195 | `	}else{` |
|      13 | 1196 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|       6 | 1197 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|       - | 1198 | `	}` |
|      15 | 1199 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1200 | `}` |
|       - | 1201 | `/*` |
|       - | 1202 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|       - | 1203 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|       - | 1204 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|       - | 1205 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|       - | 1206 | ` */` |
|      32 | 1207 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|       1 | 1208 | `{` |
|      33 | 1209 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|      33 | 1210 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1211 | `	int bOk;` |
|      33 | 1212 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      27 | 1213 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|      14 | 1214 | `	}else{` |
|       7 | 1215 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|       - | 1216 | `	}` |
|      33 | 1217 | `	if( !bOk ){` |
|      15 | 1218 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|       - | 1219 | `	}` |
|      19 | 1220 | `	return SXRET_OK;` |
|      17 | 1221 | `}` |
|      40 | 1222 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|       5 | 1223 | `{` |
|      45 | 1224 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1225 | `	SyBlob sMsg;` |
|      45 | 1226 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      45 | 1227 | `	if( bModify ){` |
|      41 | 1228 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|      23 | 1229 | `	}else{` |
|       6 | 1230 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       6 | 1231 | `		if( pActive ){` |
|     ! 0 | 1232 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|     ! 0 | 1233 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|     ! 0 | 1234 | `		}else{` |
|       6 | 1235 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|       2 | 1236 | `				&pOwner->sName,&pAttr->sName);` |
|       - | 1237 | `		}` |
|       - | 1238 | `	}` |
|      45 | 1239 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       5 | 1240 | `}` |
|       - | 1241 | `/*` |
|       - | 1242 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|       - | 1243 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|       - | 1244 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|       - | 1245 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|       - | 1246 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|       - | 1247 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|       - | 1248 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|       - | 1249 | ` */` |
|  696942 | 1250 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|       5 | 1251 | `{` |
|       - | 1252 | `	SyHashEntry *pSlot;` |
|       - | 1253 | `	VmClassAttr *pVmAttr;` |
|  696947 | 1254 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|  446092 | 1255 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|       - | 1256 | `	}` |
|  250860 | 1257 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  250860 | 1258 | `	if( pSlot == 0 ){` |
|  250766 | 1259 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1260 | `	}` |
|      98 | 1261 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      98 | 1262 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|      12 | 1263 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|       - | 1264 | `	}` |
|      84 | 1265 | `	if( pVmAttr->pAttr` |
|      87 | 1266 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|       - | 1267 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|       7 | 1268 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|       - | 1269 | `	}` |
|      81 | 1270 | `	return SXRET_OK;` |
|  348907 | 1271 | `}` |
|       - | 1272 | `/*` |
|       - | 1273 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|       - | 1274 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|       - | 1275 | ` * For class types, instanceof is verified.` |
|       - | 1276 | ` *` |
|       - | 1277 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|       - | 1278 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|       - | 1279 | ` */` |
|       - | 1280 |  |
|       - | 1281 | `/*` |
|       - | 1282 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|       - | 1283 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|       - | 1284 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|       - | 1285 | ` *   0 if it's not strictly numeric.` |
|       - | 1286 | ` */` |
|      38 | 1287 | `static int VmStringNumericKind(ph7_value *pValue)` |
|       3 | 1288 | `{` |
|       - | 1289 | `	const char *z, *zEnd, *zTail;` |
|       - | 1290 | `	sxu32 n;` |
|      41 | 1291 | `	sxu8 bReal = 0;` |
|       - | 1292 | `	sxi32 rc;` |
|      41 | 1293 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      24 | 1294 | `		return 0;` |
|       - | 1295 | `	}` |
|      18 | 1296 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|      18 | 1297 | `	n = SyBlobLength(&pValue->sBlob);` |
|      18 | 1298 | `	zEnd = z + n;` |
|      18 | 1299 | `	if( n == 0 ) return 0;` |
|      18 | 1300 | `	zTail = 0;` |
|      18 | 1301 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|      18 | 1302 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|      19 | 1303 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|      15 | 1304 | `	if( zTail != zEnd ) return 0;` |
|      15 | 1305 | `	return bReal ? 2 : 1;` |
|      22 | 1306 | `}` |
|       - | 1307 |  |
|       - | 1308 | `/*` |
|       - | 1309 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|       - | 1310 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|       - | 1311 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|       - | 1312 | ` * return, parameter, property, union alternative — would have to string-match` |
|       - | 1313 | ` * the name itself.` |
|       - | 1314 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|       - | 1315 | ` * to extend when another literal/pseudo type is added.` |
|       - | 1316 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|       - | 1317 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|       - | 1318 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|       - | 1319 | ` */` |
|    2968 | 1320 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|       5 | 1321 | `{` |
|    2973 | 1322 | `	const char *z = pClass->zString;` |
|    2973 | 1323 | `	sxu32 n = pClass->nByte;` |
|    2973 | 1324 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|     136 | 1325 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|       - | 1326 | `	}` |
|    2841 | 1327 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|      28 | 1328 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|       - | 1329 | `	}` |
|    2815 | 1330 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|      51 | 1331 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|       - | 1332 | `	}` |
|    2767 | 1333 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|       - | 1334 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|       - | 1335 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|       - | 1336 | `		 * object; scope-sensitive, so a private method is callable only from` |
|       - | 1337 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|       - | 1338 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|       - | 1339 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|       - | 1340 | `		 * php does.) */` |
|    2015 | 1341 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|       - | 1342 | `	}` |
|     757 | 1343 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|       - | 1344 | `		/* iterable === array \| Traversable */` |
|      69 | 1345 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      14 | 1346 | `			return 1;` |
|       - | 1347 | `		}` |
|      57 | 1348 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|      29 | 1349 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      29 | 1350 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|      13 | 1351 | `				return 1;` |
|       - | 1352 | `			}` |
|       7 | 1353 | `		}` |
|      45 | 1354 | `		return 0;` |
|       - | 1355 | `	}` |
|     691 | 1356 | `	return -1;` |
|    1489 | 1357 | `}` |
|       - | 1358 | `/*` |
|       - | 1359 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|       - | 1360 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|       - | 1361 | ` * through. php binds the keyword where the hint is written, so` |
|       - | 1362 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|       - | 1363 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|       - | 1364 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|       - | 1365 | ` * throw a TypeError over perfectly valid code.` |
|       - | 1366 | ` *` |
|       - | 1367 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|       - | 1368 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|       - | 1369 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|       - | 1370 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|       - | 1371 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|       - | 1372 | `` * VmResolveTypeClass applies to `parent`.`` |
|       - | 1373 | ` */` |
|  210946 | 1374 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|       5 | 1375 | `{` |
|  210951 | 1376 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  202271 | 1377 | `		return pDecl;` |
|       - | 1378 | `	}` |
|    8685 | 1379 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|  105478 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|       - | 1383 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|       - | 1384 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|       - | 1385 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|       - | 1386 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|       - | 1387 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|       - | 1388 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|       - | 1389 | ` * throw.` |
|       - | 1390 | ` *` |
|       - | 1391 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|       - | 1392 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|       - | 1393 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|       - | 1394 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|       - | 1395 | ` */` |
|       - | 1396 | `/*` |
|       - | 1397 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|       - | 1398 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|       - | 1399 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|       - | 1400 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|       - | 1401 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|       - | 1402 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|       - | 1403 | ` * iLoadable flag — lives in one place.` |
|       - | 1404 | ` *` |
|       - | 1405 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|       - | 1406 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|       - | 1407 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|       - | 1408 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|       - | 1409 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|       - | 1410 | ` *` |
|       - | 1411 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|       - | 1412 | ` * compatibility target, where the type may legitimately be an interface or` |
|       - | 1413 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|       - | 1414 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|       - | 1415 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|       - | 1416 | ` * with TRUE; it does not go through this helper.)` |
|       - | 1417 | ` */` |
|     762 | 1418 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|       5 | 1419 | `{` |
|     767 | 1420 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|     101 | 1421 | `		return pSelf;` |
|       - | 1422 | `	}` |
|     671 | 1423 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|       - | 1424 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|       - | 1425 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|       - | 1426 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|       - | 1427 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|       - | 1428 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|      40 | 1429 | `		return PH7_VmPeekTopClass(pVm);` |
|       - | 1430 | `	}` |
|     635 | 1431 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|       - | 1432 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|       - | 1433 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|      21 | 1434 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 | 1435 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|     ! 0 | 1436 | `		}` |
|      21 | 1437 | `		return pSelf ? pSelf->pBase : 0;` |
|       - | 1438 | `	}` |
|     617 | 1439 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|     386 | 1440 | `}` |
|       - | 1441 | `/*` |
|       - | 1442 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|       - | 1443 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|       - | 1444 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|       - | 1445 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|       - | 1446 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|       - | 1447 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|       - | 1448 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|       - | 1449 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|       - | 1450 | ` */` |
|  100530 | 1451 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|       5 | 1452 | `{` |
|  100546 | 1453 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|  100519 | 1454 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|  150795 | 1455 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|       5 | 1456 | `}` |
|       - | 1457 | `/*` |
|       - | 1458 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|       - | 1459 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|       - | 1460 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|       - | 1461 | ` * typed-default checks — each of which then formats its own message. The` |
|       - | 1462 | ` * resolved class is handed back through *ppResolved for the message builder` |
|       - | 1463 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|       - | 1464 | ` * nothing resolved).` |
|       - | 1465 | ` *` |
|       - | 1466 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|       - | 1467 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|       - | 1468 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|       - | 1469 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|       - | 1470 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|       - | 1471 | ` * resolve against there is no class to compare to — a position php rejects at` |
|       - | 1472 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|       - | 1473 | ` *` |
|       - | 1474 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|       - | 1475 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|       - | 1476 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|       - | 1477 | ` * autoloader can produce fails.` |
|       - | 1478 | ` */` |
|     570 | 1479 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|       - | 1480 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|       5 | 1481 | `{` |
|     575 | 1482 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     575 | 1483 | `	*ppResolved = pExpected;` |
|     575 | 1484 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      42 | 1485 | `		return 0;` |
|       - | 1486 | `	}` |
|     537 | 1487 | `	if( pExpected == 0 ){` |
|      13 | 1488 | `		return VmHintIsScopeKeyword(pName);` |
|       - | 1489 | `	}` |
|     525 | 1490 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|     290 | 1491 | `}` |
|       - | 1492 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|       - | 1493 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|  403166 | 1494 | `static int VmHintNameChar(int c)` |
|       5 | 1495 | `{` |
|  805959 | 1496 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|  402794 | 1497 | `		\|\| c == ' ' \|\| c == '\t');` |
|       5 | 1498 | `}` |
|       - | 1499 | `/*` |
|       - | 1500 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|       - | 1501 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|       - | 1502 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|       - | 1503 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|       - | 1504 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|       - | 1505 | ` * resolved until the call site are substituted.` |
|       - | 1506 | ` *` |
|       - | 1507 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|       - | 1508 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|       - | 1509 | ` * are only ever available as source: unions/intersections, and the property /` |
|       - | 1510 | ` * class-constant messages, which print the declared type whatever its shape.` |
|       - | 1511 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|       - | 1512 | ` *` |
|       - | 1513 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|       - | 1514 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|       - | 1515 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|       - | 1516 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|       - | 1517 | ` * names the two types it stands for. Only the standalone spellings can still` |
|       - | 1518 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|       - | 1519 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|       - | 1520 | ` */` |
|  100350 | 1521 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|       - | 1522 | `	char *zBuf,sxu32 nBuf)` |
|       5 | 1523 | `{` |
|       - | 1524 | `	const char *z;` |
|  100355 | 1525 | `	sxu32 n, i = 0, nAt = 0;` |
|  100355 | 1526 | `	if( nBuf == 0 ){` |
|     ! 0 | 1527 | `		return "";` |
|       - | 1528 | `	}` |
|  100355 | 1529 | `	z = pDeclared ? pDeclared->zString : 0;` |
|  100355 | 1530 | `	n = z ? pDeclared->nByte : 0;` |
|  100355 | 1531 | `	if( z ){` |
|  100355 | 1532 | `		const char *zIter = 0;` |
|  100355 | 1533 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|       8 | 1534 | `			zIter = "Traversable\|array";` |
|  100352 | 1535 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|       3 | 1536 | `			zIter = "Traversable\|array\|null";` |
|       1 | 1537 | `		}` |
|  100355 | 1538 | `		if( zIter ){` |
|      10 | 1539 | `			sxu32 nIter = SyStrlen(zIter);` |
|      10 | 1540 | `			if( nIter > nBuf - 1 ){` |
|     ! 0 | 1541 | `				nIter = nBuf - 1;` |
|     ! 0 | 1542 | `			}` |
|      10 | 1543 | `			SyMemcpy(zIter,zBuf,nIter);` |
|      10 | 1544 | `			zBuf[nIter] = 0;` |
|      10 | 1545 | `			return zBuf;` |
|       - | 1546 | `		}` |
|   50171 | 1547 | `	}` |
|  201067 | 1548 | `	while( i < n && nAt + 1 < nBuf ){` |
|       - | 1549 | `		sxu32 nStart, nCopy;` |
|       - | 1550 | `		SyString sTok;` |
|       - | 1551 | `		const SyString *pOut;` |
|  100725 | 1552 | `		if( !VmHintNameChar(z[i]) ){` |
|     207 | 1553 | `			zBuf[nAt++] = z[i++];` |
|     207 | 1554 | `			continue;` |
|       - | 1555 | `		}` |
|  100523 | 1556 | `		nStart = i;` |
|  402793 | 1557 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|  302275 | 1558 | `			i++;` |
|       5 | 1559 | `		}` |
|  100523 | 1560 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|  100523 | 1561 | `		pOut = &sTok;` |
|  100523 | 1562 | `		if( VmHintIsScopeKeyword(&sTok) ){` |
|      34 | 1563 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|      34 | 1564 | `			if( pRes ){` |
|      34 | 1565 | `				pOut = &pRes->sName;` |
|      15 | 1566 | `			}` |
|      15 | 1567 | `		}` |
|  100523 | 1568 | `		nCopy = pOut->nByte;` |
|  100523 | 1569 | `		if( nCopy > nBuf - nAt - 1 ){` |
|     ! 0 | 1570 | `			nCopy = nBuf - nAt - 1;` |
|     ! 0 | 1571 | `		}` |
|  100523 | 1572 | `		if( nCopy > 0 ){` |
|  100523 | 1573 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|  100523 | 1574 | `			nAt += nCopy;` |
|   50259 | 1575 | `		}` |
|       5 | 1576 | `	}` |
|  100347 | 1577 | `	zBuf[nAt] = 0;` |
|  100347 | 1578 | `	return zBuf;` |
|   50180 | 1579 | `}` |
|       - | 1580 | `/*` |
|       - | 1581 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|       - | 1582 | ` * float-identity leniency — see the typed-constant note above` |
|       - | 1583 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|       - | 1584 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|       - | 1585 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|       - | 1586 | ` * so an int-typed parameter / return / property / union member silently` |
|       - | 1587 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|       - | 1588 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|       - | 1589 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|       - | 1590 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|       - | 1591 | ` * present).` |
|       - | 1592 | ` *` |
|       - | 1593 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|       - | 1594 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|       - | 1595 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|       - | 1596 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|       - | 1597 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|       - | 1598 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|       - | 1599 | ` */` |
|   40299 | 1600 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|       5 | 1601 | `{` |
|   40299 | 1602 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|   21356 | 1603 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       - | 1604 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|       - | 1605 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|       - | 1606 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|       - | 1607 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|      55 | 1608 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|      55 | 1609 | `		SyBlobRelease(&pVal->sBlob);` |
|      55 | 1610 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|      25 | 1611 | `	}` |
|   40304 | 1612 | `}` |
|     336 | 1613 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|       - | 1614 | `	ph7_class *pSelf)` |
|       5 | 1615 | `{` |
|       - | 1616 | `	sxu32 i;` |
|       - | 1617 | `	sxu32 nAlts;` |
|       - | 1618 | `	ph7_type_alt *aAlts;` |
|       - | 1619 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|       - | 1620 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     341 | 1621 | `	int bHasIntersection = 0;` |
|       - | 1622 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     341 | 1623 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      20 | 1624 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|       - | 1625 | `	}` |
|     325 | 1626 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     325 | 1627 | `	nAlts = SySetUsed(pAlts);` |
|       - | 1628 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|       - | 1629 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|       - | 1630 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|       - | 1631 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|       - | 1632 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   10565 | 1633 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    1001 | 1634 | `	for( i = 0; i < nAlts; i++ ){` |
|     681 | 1635 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|      42 | 1636 | `			bHasIntersection = 1;` |
|      19 | 1637 | `		}` |
|     343 | 1638 | `	}` |
|       - | 1639 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|       - | 1640 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|       - | 1641 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|       - | 1642 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     325 | 1643 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|      36 | 1644 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1645 | `		sxu32 g;` |
|     422 | 1646 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|       - | 1647 | `			int bAll;` |
|     410 | 1648 | `			if( aGroupCount[g] < 2 ) continue;` |
|      36 | 1649 | `			bAll = 1;` |
|      88 | 1650 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1651 | `				ph7_class *pExpected;` |
|      68 | 1652 | `				if( aAlts[i].nGroup != g ) continue;` |
|      64 | 1653 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|      64 | 1654 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      64 | 1655 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      16 | 1656 | `					bAll = 0;` |
|      16 | 1657 | `					break;` |
|       - | 1658 | `				}` |
|      28 | 1659 | `			}` |
|      36 | 1660 | `			if( bAll ) return SXRET_OK;` |
|      10 | 1661 | `		}` |
|       6 | 1662 | `	}` |
|       - | 1663 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|       - | 1664 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|       - | 1665 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|       - | 1666 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     921 | 1667 | `	for( i = 0; i < nAlts; i++ ){` |
|     635 | 1668 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     594 | 1669 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     407 | 1670 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|      16 | 1671 | `			return SXRET_OK;` |
|       - | 1672 | `		}` |
|     295 | 1673 | `	}` |
|     291 | 1674 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     291 | 1675 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     895 | 1676 | `	for( i = 0; i < nAlts; i++ ){` |
|     609 | 1677 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     573 | 1678 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     389 | 1679 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     385 | 1680 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     379 | 1681 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     167 | 1682 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     127 | 1683 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|       5 | 1684 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     289 | 1685 | `	}` |
|       - | 1686 | `	/* Object handling */` |
|     291 | 1687 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      99 | 1688 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      99 | 1689 | `		if( bHasClassAlt ){` |
|      85 | 1690 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     217 | 1691 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1692 | `				ph7_class *pExpected;` |
|     161 | 1693 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     153 | 1694 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|     107 | 1695 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|     107 | 1696 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      28 | 1697 | `					return SXRET_OK;` |
|       - | 1698 | `				}` |
|      44 | 1699 | `			}` |
|      28 | 1700 | `		}` |
|      74 | 1701 | `		return SXERR_INVALID;` |
|       - | 1702 | `	}` |
|       - | 1703 | `	/* Array handling */` |
|     197 | 1704 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      16 | 1705 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|       - | 1706 | `	}` |
|       - | 1707 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|       - | 1708 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|       - | 1709 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|       - | 1710 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|       - | 1711 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|       - | 1712 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     185 | 1713 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      22 | 1714 | `		if( bHasFloat ) return SXRET_OK;` |
|       5 | 1715 | `	}` |
|     177 | 1716 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|     119 | 1717 | `		if( bHasInt ){` |
|      97 | 1718 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      97 | 1719 | `			return SXRET_OK;` |
|       - | 1720 | `		}` |
|      11 | 1721 | `	}` |
|      85 | 1722 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|      59 | 1723 | `		if( bHasString ) return SXRET_OK;` |
|       8 | 1724 | `	}` |
|      45 | 1725 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|       3 | 1726 | `		if( bHasBool ) return SXRET_OK;` |
|       1 | 1727 | `	}` |
|      45 | 1728 | `	if( bStrict ){` |
|       - | 1729 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|       5 | 1730 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|     ! 0 | 1731 | `			PH7_MemObjToReal(pValue);` |
|     ! 0 | 1732 | `			return SXRET_OK;` |
|       - | 1733 | `		}` |
|       5 | 1734 | `		return SXERR_INVALID;` |
|       - | 1735 | `	}` |
|       - | 1736 | `	/* Weak coercion preference order: int > float > string > bool.` |
|       - | 1737 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|       - | 1738 | `	 * to match PHP's union RFC. */` |
|       - | 1739 | `	{` |
|      41 | 1740 | `		int kind = VmStringNumericKind(pValue);` |
|      41 | 1741 | `		if( bHasInt ){` |
|       - | 1742 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|       - | 1743 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|      18 | 1744 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1745 | `				PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1746 | `				return SXRET_OK;` |
|       - | 1747 | `			}` |
|      18 | 1748 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1749 | `				ph7_real r = pValue->rVal;` |
|     ! 0 | 1750 | `				if( r == (ph7_real)(sxi64)r ){` |
|     ! 0 | 1751 | `					PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1752 | `					return SXRET_OK;` |
|       - | 1753 | `				}` |
|     ! 0 | 1754 | `			}` |
|      18 | 1755 | `			if( kind == 1 ){` |
|       9 | 1756 | `				PH7_MemObjToInteger(pValue);` |
|       9 | 1757 | `				return SXRET_OK;` |
|       - | 1758 | `			}` |
|       4 | 1759 | `		}` |
|      33 | 1760 | `		if( bHasFloat ){` |
|      10 | 1761 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|     ! 0 | 1762 | `				PH7_MemObjToReal(pValue);` |
|     ! 0 | 1763 | `				return SXRET_OK;` |
|       - | 1764 | `			}` |
|      10 | 1765 | `			if( kind == 1 \|\| kind == 2 ){` |
|       7 | 1766 | `				PH7_MemObjToReal(pValue);` |
|       7 | 1767 | `				return SXRET_OK;` |
|       - | 1768 | `			}` |
|       1 | 1769 | `		}` |
|      26 | 1770 | `		if( bHasString ){` |
|     ! 0 | 1771 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|     ! 0 | 1772 | `				PH7_MemObjToString(pValue);` |
|     ! 0 | 1773 | `				return SXRET_OK;` |
|       - | 1774 | `			}` |
|     ! 0 | 1775 | `		}` |
|      26 | 1776 | `		if( bHasBool ){` |
|       3 | 1777 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|       3 | 1778 | `				PH7_MemObjToBool(pValue);` |
|       3 | 1779 | `				return SXRET_OK;` |
|       - | 1780 | `			}` |
|     ! 0 | 1781 | `		}` |
|       - | 1782 | `	}` |
|      24 | 1783 | `	return SXERR_INVALID;` |
|     173 | 1784 | `}` |
|       - | 1785 |  |
|       - | 1786 | `/*` |
|       - | 1787 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|       - | 1788 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|       - | 1789 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|       - | 1790 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|       - | 1791 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|       - | 1792 | ` */` |
|     326 | 1793 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|       5 | 1794 | `{` |
|       - | 1795 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|       - | 1796 | `	 * null value satisfies it (and a null value matches via the flag test` |
|       - | 1797 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|       - | 1798 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|       - | 1799 | `	 * silently swallow any argument. */` |
|     331 | 1800 | `	if( nType == MEMOBJ_NULL ){` |
|       3 | 1801 | `		return SXERR_INVALID;` |
|       - | 1802 | `	}` |
|       - | 1803 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|       - | 1804 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|       - | 1805 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|       - | 1806 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|       - | 1807 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|       - | 1808 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|       - | 1809 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|       - | 1810 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|       - | 1811 | `	 * object value against an array type is caught here too (never valid);` |
|       - | 1812 | `	 * object->scalar stays a separate case handled by the callers. */` |
|     329 | 1813 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|      37 | 1814 | `		return SXERR_INVALID;` |
|       - | 1815 | `	}` |
|     295 | 1816 | `	if( bStrict ){` |
|       - | 1817 | `		/* Only int -> float widening is allowed implicitly. */` |
|      36 | 1818 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|       3 | 1819 | `			PH7_MemObjToReal(pVal);` |
|       3 | 1820 | `			return SXRET_OK;` |
|       - | 1821 | `		}` |
|      34 | 1822 | `		return SXERR_INVALID;` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|       - | 1825 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|       - | 1826 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|       - | 1827 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|       - | 1828 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|       - | 1829 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|       - | 1830 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     263 | 1831 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      20 | 1832 | `		return SXERR_INVALID;` |
|       - | 1833 | `	}` |
|       - | 1834 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|       - | 1835 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|       - | 1836 | `	 * is invoked by the string cast below). Every other scalar target —` |
|       - | 1837 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|       - | 1838 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|       - | 1839 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|       - | 1840 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|       - | 1841 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|       - | 1842 | `	 * is caught by the array guard.) */` |
|     245 | 1843 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      22 | 1844 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      27 | 1845 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      10 | 1846 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      13 | 1847 | `			return SXERR_INVALID;` |
|       - | 1848 | `		}` |
|       4 | 1849 | `	}` |
|     228 | 1850 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|     185 | 1851 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|     173 | 1852 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      49 | 1853 | `		return SXERR_INVALID;` |
|       - | 1854 | `	}` |
|     187 | 1855 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|       - | 1856 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|       - | 1857 | `		 * PHL rejects it (§10). SXERR_INVALID routes to the caller's TypeError,` |
|       - | 1858 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|       - | 1859 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|       - | 1860 | `		 * for the typed parameters and returns that reach here, for the typed` |
|       - | 1861 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|       - | 1862 | `		 * integer-only operators. */` |
|     ! 0 | 1863 | `		return SXERR_INVALID;` |
|       - | 1864 | `	}` |
|       - | 1865 | `	{` |
|     187 | 1866 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|     187 | 1867 | `		if( xCast ) xCast(pVal);` |
|       - | 1868 | `	}` |
|     187 | 1869 | `	return SXRET_OK;` |
|     168 | 1870 | `}` |
|       - | 1871 |  |
|       - | 1872 | `/*` |
|       - | 1873 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|       - | 1874 | ` * TypeError message. Prefers the declared textual form when available.` |
|       - | 1875 | ` *` |
|       - | 1876 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|       - | 1877 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|       - | 1878 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|       - | 1879 | ` * back to a static literal and ignore zBuf entirely.` |
|       - | 1880 | ` */` |
|     202 | 1881 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|       5 | 1882 | `{` |
|     207 | 1883 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     207 | 1884 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     207 | 1885 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     207 | 1886 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     207 | 1887 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|     101 | 1888 | `		}` |
|     207 | 1889 | `		zBuf[nCopy] = 0;` |
|     207 | 1890 | `		return zBuf;` |
|       - | 1891 | `	}` |
|     ! 0 | 1892 | `	switch( nType ){` |
|     ! 0 | 1893 | `		case MEMOBJ_INT:     return "int";` |
|     ! 0 | 1894 | `		case MEMOBJ_REAL:    return "float";` |
|     ! 0 | 1895 | `		case MEMOBJ_STRING:  return "string";` |
|     ! 0 | 1896 | `		case MEMOBJ_BOOL:    return "bool";` |
|     ! 0 | 1897 | `		case MEMOBJ_HASHMAP: return "array";` |
|     ! 0 | 1898 | `		case MEMOBJ_OBJ:     return "object";` |
|     ! 0 | 1899 | `		default:             return "scalar";` |
|       - | 1900 | `	}` |
|     106 | 1901 | `}` |
|       - | 1902 |  |
|       - | 1903 | `/*` |
|       - | 1904 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|       - | 1905 | ` * the way php writes it in a TypeError:` |
|       - | 1906 | ` *` |
|       - | 1907 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|       - | 1908 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|       - | 1909 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|       - | 1910 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|       - | 1911 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|       - | 1912 | ` *` |
|       - | 1913 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|       - | 1914 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|       - | 1915 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|       - | 1916 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|       - | 1917 | ` */` |
|     174 | 1918 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|       - | 1919 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|       5 | 1920 | `{` |
|     179 | 1921 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|       - | 1922 | `	sxu32 nCopy;` |
|     179 | 1923 | `	sxu32 nAt = 0;` |
|     179 | 1924 | `	if( nBuf == 0 ){` |
|     ! 0 | 1925 | `		return "";` |
|       - | 1926 | `	}` |
|     174 | 1927 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|      79 | 1928 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|      21 | 1929 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|      21 | 1930 | `		nCopy = SyStrlen(zIter);` |
|      21 | 1931 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      21 | 1932 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|      21 | 1933 | `		zBuf[nCopy] = 0;` |
|      21 | 1934 | `		return zBuf;` |
|       - | 1935 | `	}` |
|     161 | 1936 | `	if( bNullable && nBuf > 1 ){` |
|      23 | 1937 | `		zBuf[nAt++] = '?';` |
|      10 | 1938 | `	}` |
|     161 | 1939 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|     161 | 1940 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|     161 | 1941 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|     161 | 1942 | `	zBuf[nAt + nCopy] = 0;` |
|     161 | 1943 | `	return zBuf;` |
|      92 | 1944 | `}` |
|       - | 1945 |  |
|       - | 1946 | `/*` |
|       - | 1947 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|       - | 1948 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|       - | 1949 | ` */` |
|     142 | 1950 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|       5 | 1951 | `{` |
|     147 | 1952 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     218 | 1953 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|     142 | 1954 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|     147 | 1955 | `	return zBuf;` |
|       5 | 1956 | `}` |
|       - | 1957 |  |
|  108270 | 1958 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|       5 | 1959 | `{` |
|       - | 1960 | `	SyHashEntry *pSlot;` |
|       - | 1961 | `	VmClassAttr *pVmAttr;` |
|       - | 1962 | `	ph7_class_attr *pAttr;` |
|       - | 1963 | `	ph7_class *pHintScope;` |
|       - | 1964 | `	char zGivenBuf[128];` |
|       - | 1965 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|       - | 1966 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|       - | 1967 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|       - | 1968 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|  108275 | 1969 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|  108275 | 1970 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  108275 | 1971 | `	if( pSlot == 0 ){` |
|    7523 | 1972 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1973 | `	}` |
|  100757 | 1974 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|  100757 | 1975 | `	pAttr = pVmAttr->pAttr;` |
|  100757 | 1976 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1977 | `		return SXRET_OK;` |
|       - | 1978 | `	}` |
|       - | 1979 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|       - | 1980 | `	 * the property (a trait's members count as the composing class), not the` |
|       - | 1981 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|  100757 | 1982 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|       - | 1983 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|       - | 1984 | `	 * property may be written exactly once and only from within the declaring` |
|       - | 1985 | `	 * class scope (its set-scope is protected). */` |
|  100757 | 1986 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1987 | `		/* A readonly property is always typed and default-less, so it starts` |
|       - | 1988 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|       - | 1989 | `		 * write below — making it the write-once latch (a type-rejected write` |
|       - | 1990 | `		 * leaves it set, so a later valid initialization still works). */` |
|      99 | 1991 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|       - | 1992 | `			/* Already initialized: any further write is forbidden, any scope —` |
|       - | 1993 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|       - | 1994 | `			 * Exceptions that fall through to the set-scope check below:` |
|       - | 1995 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|       - | 1996 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|       - | 1997 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|      33 | 1998 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      33 | 1999 | `			if( !(pCloneFr && pCloneFr->pThis` |
|      16 | 2000 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|      31 | 2001 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|       - | 2002 | `			}` |
|       1 | 2003 | `		}` |
|      34 | 2004 | `	}` |
|  100731 | 2005 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|       - | 2006 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|       - | 2007 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|      27 | 2008 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|      27 | 2009 | `		if( rcVis != SXRET_OK ){` |
|      13 | 2010 | `			return rcVis;` |
|       1 | 2011 | `		}` |
|  100712 | 2012 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 2013 | `		/* First write (or a clone re-init) must come from within the declaring` |
|       - | 2014 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      71 | 2015 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      71 | 2016 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 2017 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|       - | 2018 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|       - | 2019 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|      71 | 2020 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|       5 | 2021 | `			pDecl = pVmAttr->pOwner;` |
|       2 | 2022 | `		}` |
|      71 | 2023 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|       6 | 2024 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|       - | 2025 | `		}` |
|      31 | 2026 | `	}` |
|       - | 2027 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|       - | 2028 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|       - | 2029 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|       - | 2030 | `	 * is its TypeError there), which this used to deny outright. */` |
|  100715 | 2031 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      92 | 2032 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|      58 | 2033 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|      29 | 2034 | `			bStrict,pHintScope);` |
|      63 | 2035 | `		if( rc == SXRET_OK ){` |
|      38 | 2036 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      38 | 2037 | `			return SXRET_OK;` |
|       - | 2038 | `		}` |
|      29 | 2039 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 2040 | `			char zBuf[128];` |
|      25 | 2041 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       7 | 2042 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2043 | `		}` |
|      13 | 2044 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2045 | `	}` |
|       - | 2046 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|       - | 2047 | `	 * includes null). */` |
|  100657 | 2048 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      30 | 2049 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|      24 | 2050 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       2 | 2051 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|      23 | 2052 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      23 | 2053 | `			return SXRET_OK;` |
|       - | 2054 | `		}` |
|      12 | 2055 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|       - | 2056 | `	}` |
|       - | 2057 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|       - | 2058 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|       - | 2059 | `	 * type error. */` |
|  100627 | 2060 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2061 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2062 | `	}` |
|       - | 2063 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|       - | 2064 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|       - | 2065 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|  100627 | 2066 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      12 | 2067 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       5 | 2068 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       5 | 2069 | `			return SXRET_OK;` |
|       - | 2070 | `		}` |
|       7 | 2071 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2072 | `	}` |
|       - | 2073 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|       - | 2074 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|       - | 2075 | `	 * handled by the nullable check above). Checked by value before the generic` |
|       - | 2076 | `	 * class-instanceof branch, which would resolve no such class and then` |
|       - | 2077 | `	 * wrongly accept any object / reject arrays. */` |
|  100617 | 2078 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      79 | 2079 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|      79 | 2080 | `		if( rcPseudo == 1 ){` |
|      13 | 2081 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      13 | 2082 | `			return SXRET_OK;` |
|       - | 2083 | `		}` |
|      67 | 2084 | `		if( rcPseudo == 0 ){` |
|       8 | 2085 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2086 | `		}` |
|       - | 2087 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|      28 | 2088 | `	}` |
|  100599 | 2089 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       - | 2090 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|       - | 2091 | `		 * class (pHintScope), not the instance's runtime class. */` |
|      61 | 2092 | `		ph7_class *pExpected = 0;` |
|      61 | 2093 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|       - | 2094 | `			char zBuf[128];` |
|      32 | 2095 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      18 | 2096 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      18 | 2097 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|     ! 0 | 2098 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2099 | `		}` |
|      43 | 2100 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      43 | 2101 | `		return SXRET_OK;` |
|       - | 2102 | `	}` |
|       - | 2103 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|       - | 2104 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|       - | 2105 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|       - | 2106 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|       - | 2107 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|  100543 | 2108 | `	if( bStrict ){` |
|       - | 2109 | `		int bOk;` |
|      29 | 2110 | `		if( ph7_value_is_bool(pValue) ){` |
|       5 | 2111 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|      27 | 2112 | `		}else if( ph7_value_is_float(pValue) ){` |
|       3 | 2113 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|      24 | 2114 | `		}else if( ph7_value_is_int(pValue) ){` |
|       9 | 2115 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|      19 | 2116 | `		}else if( ph7_value_is_string(pValue) ){` |
|      15 | 2117 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|       8 | 2118 | `		}else{` |
|       - | 2119 | `			/* array / resource / an object against a scalar type: no coercion in` |
|       - | 2120 | `			 * either mode, so the flag test is the whole answer (an object never` |
|       - | 2121 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|       - | 2122 | `			 * does not perform). */` |
|     ! 0 | 2123 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|       - | 2124 | `		}` |
|      29 | 2125 | `		if( !bOk ){` |
|       - | 2126 | `			char zObjBuf[128];` |
|      31 | 2127 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      20 | 2128 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|     ! 0 | 2129 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|      20 | 2130 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2131 | `		}` |
|       9 | 2132 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|       3 | 2133 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|       2 | 2134 | `		}else{` |
|       7 | 2135 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 2136 | `		}` |
|       9 | 2137 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       9 | 2138 | `		return SXRET_OK;` |
|       - | 2139 | `	}` |
|       - | 2140 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|       - | 2141 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|       - | 2142 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|       - | 2143 | `	 * via __toString, so fall through to the string cast below. */` |
|  100515 | 2144 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      16 | 2145 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      18 | 2146 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       4 | 2147 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       - | 2148 | `			char zBuf[128];` |
|      20 | 2149 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 2150 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2151 | `		}` |
|       1 | 2152 | `	}` |
|       - | 2153 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|       - | 2154 | `	 * take (VmCoerceScalarWeak's own rule, §10) -- and it had none of it, so this` |
|       - | 2155 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|       - | 2156 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|       - | 2157 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|       - | 2158 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|       - | 2159 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|       - | 2160 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|       - | 2161 | `	 * that arrives carrying a cached int representation is asked too. */` |
|  100503 | 2162 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|      38 | 2163 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      12 | 2164 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2165 | `	}` |
|  100479 | 2166 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|  100081 | 2167 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|  100081 | 2168 | `		if( xCast ){` |
|       - | 2169 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|  100081 | 2170 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       8 | 2171 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2172 | `			}` |
|  100075 | 2173 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 2174 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2175 | `			}` |
|       - | 2176 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|       - | 2177 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|       - | 2178 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|  100064 | 2179 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|  100060 | 2180 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|  100064 | 2181 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|  100037 | 2182 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|       - | 2183 | `			}` |
|      36 | 2184 | `			xCast(pValue);` |
|      16 | 2185 | `		}` |
|      20 | 2186 | `	}else{` |
|       - | 2187 | `		/* Mask matched — an int property accepting a whole-real must` |
|       - | 2188 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|     403 | 2189 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 2190 | `	}` |
|     435 | 2191 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     435 | 2192 | `	return SXRET_OK;` |
|   54140 | 2193 | `}` |
|       - | 2194 | `/*` |
|       - | 2195 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|       - | 2196 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|       - | 2197 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|       - | 2198 | ` *     a scope that could normally reach it — else a catchable Error),` |
|       - | 2199 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|       - | 2200 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|       - | 2201 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|       - | 2202 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|       - | 2203 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|       - | 2204 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|       - | 2205 | ` */` |
|      30 | 2206 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|       - | 2207 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|       1 | 2208 | `{` |
|      31 | 2209 | `	ph7_class *pClass = pClone->pClass;` |
|       - | 2210 | `	SyHashEntry *pEntry;` |
|       - | 2211 | `	VmClassAttr *pVmAttr;` |
|       - | 2212 | `	ph7_class_attr *pAttr;` |
|       - | 2213 | `	ph7_value *pSlot;` |
|       - | 2214 | `	sxi32 rc;` |
|      31 | 2215 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|      31 | 2216 | `	if( pEntry == 0 ){` |
|       - | 2217 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|       - | 2218 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|       - | 2219 | `		 * deferred residual). */` |
|     ! 0 | 2220 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|     ! 0 | 2221 | `		if( pSlot == 0 ){` |
|     ! 0 | 2222 | `			return PH7_VmMemoryError(pVm);` |
|       - | 2223 | `		}` |
|     ! 0 | 2224 | `		PH7_MemObjStore(pValue,pSlot);` |
|     ! 0 | 2225 | `		return SXRET_OK;` |
|       - | 2226 | `	}` |
|      31 | 2227 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      31 | 2228 | `	pAttr = pVmAttr->pAttr;` |
|       - | 2229 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|      31 | 2230 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - | 2231 | `		SyBlob sMsg;` |
|     ! 0 | 2232 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     ! 0 | 2233 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|     ! 0 | 2234 | `			&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2235 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2236 | `	}` |
|       - | 2237 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|       - | 2238 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|       - | 2239 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|      31 | 2240 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       5 | 2241 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|       5 | 2242 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 2243 | `		SyBlob sMsg;` |
|       5 | 2244 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       5 | 2245 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|       5 | 2246 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2247 | `	}` |
|       - | 2248 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|      27 | 2249 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|      27 | 2250 | `	if( rc != SXRET_OK ){` |
|       3 | 2251 | `		return rc;` |
|       - | 2252 | `	}` |
|       - | 2253 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|      25 | 2254 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|      25 | 2255 | `	if( pSlot ){` |
|      25 | 2256 | `		PH7_MemObjStore(pValue,pSlot);` |
|      12 | 2257 | `	}` |
|      25 | 2258 | `	return SXRET_OK;` |
|      16 | 2259 | `}` |
|       - | 2260 | `/*` |
|       - | 2261 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|       - | 2262 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|       - | 2263 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|       - | 2264 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|       - | 2265 | ` */` |
|      10 | 2266 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       3 | 2267 | `{` |
|      13 | 2268 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2269 | `	char zBuf[128],zType[192];` |
|       - | 2270 | `	const char *zGiven;` |
|      18 | 2271 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       5 | 2272 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|      13 | 2273 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2274 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2275 | `	}else{` |
|      13 | 2276 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2277 | `	}` |
|      13 | 2278 | `	if( bLazy ){` |
|       - | 2279 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|       - | 2280 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|       - | 2281 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|       - | 2282 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|       - | 2283 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|       - | 2284 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|       - | 2285 | `		 * re-evaluates and re-raises, as php's does. */` |
|       - | 2286 | `		SyBlob sMsg;` |
|       5 | 2287 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       5 | 2288 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|       2 | 2289 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       5 | 2290 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - | 2291 | `	}` |
|       - | 2292 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|       - | 2293 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|       - | 2294 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|       - | 2295 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|       - | 2296 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|       - | 2297 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|       - | 2298 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       9 | 2299 | `	if( pVm->sCodeGen.xErr ){` |
|       8 | 2300 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|       - | 2301 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       2 | 2302 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       4 | 2303 | `	}else{` |
|       4 | 2304 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 2305 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       1 | 2306 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       - | 2307 | `	}` |
|       9 | 2308 | `	pVm->iExitStatus = 255;` |
|       9 | 2309 | `	pVm->bHaltRequested = 1;` |
|       9 | 2310 | `	return SXERR_ABORT;` |
|       8 | 2311 | `}` |
|       - | 2312 | `/*` |
|       - | 2313 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|       - | 2314 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|       - | 2315 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|       - | 2316 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|       - | 2317 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|       - | 2318 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|       - | 2319 | ` */` |
|      42 | 2320 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       4 | 2321 | `{` |
|      46 | 2322 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|       - | 2323 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|      46 | 2324 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 2325 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       3 | 2326 | `			return SXRET_OK;` |
|       - | 2327 | `		}` |
|     ! 0 | 2328 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|     ! 0 | 2329 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 2330 | `			return SXRET_OK;` |
|       - | 2331 | `		}` |
|     ! 0 | 2332 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2333 | `	}` |
|       - | 2334 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|      44 | 2335 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       6 | 2336 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       5 | 2337 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       5 | 2338 | `			return SXRET_OK;` |
|       - | 2339 | `		}` |
|     ! 0 | 2340 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2341 | `	}` |
|       - | 2342 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|      40 | 2343 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2344 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2345 | `	}` |
|       - | 2346 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|      40 | 2347 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2348 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2349 | `			return SXRET_OK;` |
|       - | 2350 | `		}` |
|     ! 0 | 2351 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2352 | `	}` |
|       - | 2353 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|       - | 2354 | `	 * a real class/interface verified by instanceof. */` |
|      40 | 2355 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       3 | 2356 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       3 | 2357 | `		if( rcPseudo == 1 ){` |
|     ! 0 | 2358 | `			return SXRET_OK;` |
|       - | 2359 | `		}` |
|       3 | 2360 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2361 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2362 | `		}` |
|       - | 2363 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|       - | 2364 | `		 * self/parent resolve against the declaring class. */` |
|       - | 2365 | `		{` |
|       3 | 2366 | `			ph7_class *pExpected = 0;` |
|       4 | 2367 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|       1 | 2368 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|       3 | 2369 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2370 | `			}` |
|       - | 2371 | `		}` |
|     ! 0 | 2372 | `		return SXRET_OK;` |
|       - | 2373 | `	}` |
|       - | 2374 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|       - | 2375 | `	 * implicit widening. Everything else is a type error.` |
|       - | 2376 | `	 *` |
|       - | 2377 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|       - | 2378 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|       - | 2379 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|       - | 2380 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|       - | 2381 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|       - | 2382 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|       - | 2383 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|       - | 2384 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|       - | 2385 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|       - | 2386 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|       - | 2387 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|      37 | 2388 | `	if( pValue->iFlags & pAttr->nType ){` |
|      25 | 2389 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      25 | 2390 | `		return SXRET_OK;` |
|       - | 2391 | `	}` |
|      13 | 2392 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       3 | 2393 | `		PH7_MemObjToReal(pValue);` |
|       3 | 2394 | `		return SXRET_OK;` |
|       - | 2395 | `	}` |
|      10 | 2396 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|      25 | 2397 | `}` |
|       - | 2398 | `/*` |
|       - | 2399 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|       - | 2400 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|       - | 2401 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|       - | 2402 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|       - | 2403 | ` */` |
|      34 | 2404 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       2 | 2405 | `{` |
|      36 | 2406 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2407 | `	const char *zGiven;` |
|       - | 2408 | `	char zBuf[128],zType[192];` |
|      53 | 2409 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|      17 | 2410 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|       - | 2411 | `	SyBlob sMsg;` |
|      36 | 2412 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2413 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2414 | `	}else{` |
|      36 | 2415 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2416 | `	}` |
|      36 | 2417 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      36 | 2418 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|      17 | 2419 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|      36 | 2420 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       2 | 2421 | `}` |
|       - | 2422 | `/*` |
|       - | 2423 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|       - | 2424 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|       - | 2425 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|       - | 2426 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|       - | 2427 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|       - | 2428 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|       - | 2429 | ` * instance defaults per-instantiation. Matching structure of` |
|       - | 2430 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|       - | 2431 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|       - | 2432 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|       - | 2433 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|       - | 2434 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|       - | 2435 | ` */` |
|       - | 2436 | `/*` |
|       - | 2437 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|       - | 2438 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|       - | 2439 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|       - | 2440 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|       - | 2441 | ` * throwing, so the static-property mount path can defer the failure (php` |
|       - | 2442 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|       - | 2443 | ` * instance path throws immediately via the wrapper below.` |
|       - | 2444 | ` */` |
|     386 | 2445 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2446 | `{` |
|     391 | 2447 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|     391 | 2448 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      58 | 2449 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      54 | 2450 | `			return SXRET_OK;` |
|       - | 2451 | `		}` |
|       4 | 2452 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       4 | 2453 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|       3 | 2454 | `			return SXRET_OK;` |
|       - | 2455 | `		}` |
|       3 | 2456 | `		return SXERR_INVALID;` |
|       - | 2457 | `	}` |
|     337 | 2458 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      39 | 2459 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|      30 | 2460 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|      30 | 2461 | `			return SXRET_OK;` |
|       - | 2462 | `		}` |
|     ! 0 | 2463 | `		return SXERR_INVALID;` |
|       - | 2464 | `	}` |
|     311 | 2465 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2466 | `		return SXERR_INVALID;` |
|       - | 2467 | `	}` |
|     311 | 2468 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2469 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2470 | `			return SXRET_OK;` |
|       - | 2471 | `		}` |
|     ! 0 | 2472 | `		return SXERR_INVALID;` |
|       - | 2473 | `	}` |
|     311 | 2474 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       5 | 2475 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       5 | 2476 | `		if( rcPseudo == 1 ){` |
|       5 | 2477 | `			return SXRET_OK;` |
|       - | 2478 | `		}` |
|     ! 0 | 2479 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2480 | `			return SXERR_INVALID;` |
|       - | 2481 | `		}` |
|       - | 2482 | `		{` |
|       - | 2483 | `			/* self/parent in the hint resolve against the declaring class. */` |
|     ! 0 | 2484 | `			ph7_class *pExpected = 0;` |
|     ! 0 | 2485 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|     ! 0 | 2486 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|     ! 0 | 2487 | `				return SXERR_INVALID;` |
|       - | 2488 | `			}` |
|       - | 2489 | `		}` |
|     ! 0 | 2490 | `		return SXRET_OK;` |
|       - | 2491 | `	}` |
|     307 | 2492 | `	if( pValue->iFlags & pAttr->nType ){` |
|     271 | 2493 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|     271 | 2494 | `		return SXRET_OK;` |
|       - | 2495 | `	}` |
|      38 | 2496 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       6 | 2497 | `		PH7_MemObjToReal(pValue);` |
|       6 | 2498 | `		return SXRET_OK;` |
|       - | 2499 | `	}` |
|      34 | 2500 | `	return SXERR_INVALID;` |
|     198 | 2501 | `}` |
|     322 | 2502 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2503 | `{` |
|     327 | 2504 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|     315 | 2505 | `		return SXRET_OK;` |
|       - | 2506 | `	}` |
|      13 | 2507 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|     166 | 2508 | `}` |
|       - | 2509 | `/*` |
|       - | 2510 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|       - | 2511 | ` * class chain for a static typed slot whose mount-time default failed its` |
|       - | 2512 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|       - | 2513 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|       - | 2514 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|       - | 2515 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|       - | 2516 | ` * never-touched class stays silent, and the throw repeats on every access` |
|       - | 2517 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|       - | 2518 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|       - | 2519 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|       - | 2520 | ` */` |
|      26 | 2521 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2522 | `{` |
|       - | 2523 | `	ph7_class *pScan;` |
|      33 | 2524 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       - | 2525 | `		SyHashEntry *pEntry;` |
|      29 | 2526 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|      33 | 2527 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|      29 | 2528 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      26 | 2529 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|       - | 2530 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|      24 | 2531 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      25 | 2532 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|      24 | 2533 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      24 | 2534 | `				if( pSlot ){` |
|      24 | 2535 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      24 | 2536 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|      24 | 2537 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       - | 2538 | `						ph7_value sNull;` |
|      24 | 2539 | `						if( pValue == 0 ){` |
|     ! 0 | 2540 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|     ! 0 | 2541 | `							pValue = &sNull;` |
|     ! 0 | 2542 | `						}` |
|      24 | 2543 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|       - | 2544 | `					}` |
|     ! 0 | 2545 | `				}` |
|     ! 0 | 2546 | `			}` |
|       1 | 2547 | `		}` |
|       3 | 2548 | `	}` |
|       5 | 2549 | `	return SXRET_OK;` |
|      16 | 2550 | `}` |
|       - | 2551 | `/*` |
|       - | 2552 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|       - | 2553 | ` * materialization: an initializer that threw at mount and was deferred` |
|       - | 2554 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|       - | 2555 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|       - | 2556 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|       - | 2557 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|       - | 2558 | ` * a subclass can be mounted before the base whose default failed.` |
|       - | 2559 | ` */` |
| 2214360 | 2560 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|       5 | 2561 | `{` |
| 4432411 | 2562 | `	while( pClass ){` |
| 2218133 | 2563 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|      85 | 2564 | `			return 1;` |
|       - | 2565 | `		}` |
| 2218051 | 2566 | `		pClass = pClass->pBase;` |
|       5 | 2567 | `	}` |
| 2214283 | 2568 | `	return 0;` |
| 1107185 | 2569 | `}` |
|       - | 2570 | `/*` |
|       - | 2571 | ` * Re-run the initializers of the static properties whose evaluation was` |
|       - | 2572 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|       - | 2573 | ` *` |
|       - | 2574 | ` * php builds a class's static table on first use, evaluating each slot's` |
|       - | 2575 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|       - | 2576 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|       - | 2577 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|       - | 2578 | ` * the access: a constant define()d after the class declaration resolves.` |
|       - | 2579 | ` *` |
|       - | 2580 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|       - | 2581 | ` * the base's broken default, not its own), then declaration order within a` |
|       - | 2582 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|       - | 2583 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|       - | 2584 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|       - | 2585 | ` * re-raises on every access too.` |
|       - | 2586 | ` */` |
|      86 | 2587 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|       3 | 2588 | `{` |
|       - | 2589 | `	SyHashEntry *pEntry;` |
|       - | 2590 | `	sxi32 rc;` |
|      89 | 2591 | `	if( pClass->pBase ){` |
|       6 | 2592 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|       6 | 2593 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2594 | `			return rc;` |
|       - | 2595 | `		}` |
|       2 | 2596 | `	}` |
|      89 | 2597 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     209 | 2598 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     123 | 2599 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     123 | 2600 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|       - | 2601 | `			/* Not pending. An inherited slot the base pass already collected` |
|       - | 2602 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|       - | 2603 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|      61 | 2604 | `			continue;` |
|       - | 2605 | `		}` |
|      65 | 2606 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - | 2607 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|       - | 2608 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|       - | 2609 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|       - | 2610 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|       - | 2611 | `			 * class keeps its hint flag so a later access retries. */` |
|     ! 0 | 2612 | `			*pbLeft = 1;` |
|     ! 0 | 2613 | `			continue;` |
|       - | 2614 | `		}` |
|      65 | 2615 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|      65 | 2616 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2617 | `			return rc;` |
|       - | 2618 | `		}` |
|       3 | 2619 | `	}` |
|      89 | 2620 | `	return SXRET_OK;` |
|      46 | 2621 | `}` |
|       - | 2622 | `/*` |
|       - | 2623 | ` * Re-run the initializers of the static properties whose evaluation was` |
|       - | 2624 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|       - | 2625 | ` *` |
|       - | 2626 | ` * php builds a class's static table on first use, evaluating each slot's` |
|       - | 2627 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|       - | 2628 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|       - | 2629 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|       - | 2630 | ` * the access: a constant define()d after the class declaration resolves.` |
|       - | 2631 | ` *` |
|       - | 2632 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|       - | 2633 | ` * the base's broken default, not its own), then declaration order within a` |
|       - | 2634 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|       - | 2635 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|       - | 2636 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|       - | 2637 | ` * re-raises on every access too.` |
|       - | 2638 | ` *` |
|       - | 2639 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|       - | 2640 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|       - | 2641 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|       - | 2642 | ` * cut the outer one short.` |
|       - | 2643 | ` */` |
|      82 | 2644 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|       3 | 2645 | `{` |
|       - | 2646 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|       - | 2647 | `	ph7_class_attr **apPending;` |
|       - | 2648 | `	sxu32 n,nUsed;` |
|       - | 2649 | `	sxi32 rc;` |
|      85 | 2650 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|      85 | 2651 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|      85 | 2652 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|      85 | 2653 | `	nUsed = SySetUsed(&aPending);` |
|      89 | 2654 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|      63 | 2655 | `		ph7_class_attr *pAttr = apPending[n];` |
|      63 | 2656 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2657 | `		ph7_class *pSaveCtx;` |
|       - | 2658 | `		void *pSaveFrame;` |
|       - | 2659 | `		sxu32 nSaveLazyLine;` |
|       - | 2660 | `		sxi32 nSaveLazyDepth;` |
|       - | 2661 | `		ph7_value *pMemObj;` |
|       - | 2662 | `		sxi32 rcExec;` |
|      63 | 2663 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|     ! 0 | 2664 | `			continue; /* the base pass already ran this shared slot */` |
|       - | 2665 | `		}` |
|      63 | 2666 | `		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      63 | 2667 | `		if( pMemObj == 0 ){` |
|     ! 0 | 2668 | `			continue;` |
|       - | 2669 | `		}` |
|      63 | 2670 | `		pSaveCtx = pVm->pConstEvalClass;` |
|      63 | 2671 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|      63 | 2672 | `		pVm->pConstEvalClass = pOwner;` |
|       - | 2673 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|       - | 2674 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|       - | 2675 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|       - | 2676 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|       - | 2677 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|      63 | 2678 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - | 2679 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|       - | 2680 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|      63 | 2681 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      63 | 2682 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      63 | 2683 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      63 | 2684 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      63 | 2685 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|      63 | 2686 | `		pVm->nConstEvalDepth++;` |
|      63 | 2687 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      63 | 2688 | `		pVm->nConstEvalDepth--;` |
|      63 | 2689 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      63 | 2690 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      63 | 2691 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      63 | 2692 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      63 | 2693 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      63 | 2694 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - | 2695 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|       - | 2696 | `			 * caller to route (a catch here is the user's own). */` |
|      59 | 2697 | `			rc = rcExec;` |
|      59 | 2698 | `			break;` |
|       - | 2699 | `		}` |
|       5 | 2700 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|       5 | 2701 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|       - | 2702 | `			/* The initializer named a self-referencing constant. Like the mount` |
|       - | 2703 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|       - | 2704 | `			 * the access, where a catch can see it. */` |
|     ! 0 | 2705 | `			rc = VmConstCycleThrow(&(*pVm));` |
|     ! 0 | 2706 | `			break;` |
|       - | 2707 | `		}` |
|       4 | 2708 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       3 | 2709 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - | 2710 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|       - | 2711 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|       - | 2712 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|     ! 0 | 2713 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|     ! 0 | 2714 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|     ! 0 | 2715 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     ! 0 | 2716 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|     ! 0 | 2717 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|     ! 0 | 2718 | `				break;` |
|       - | 2719 | `			}` |
|     ! 0 | 2720 | `		}` |
|       3 | 2721 | `	}` |
|      85 | 2722 | `	if( rc != SXRET_OK ){` |
|      59 | 2723 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|      28 | 2724 | `	}` |
|      85 | 2725 | `	SySetRelease(&aPending);` |
|      85 | 2726 | `	return rc;` |
|       3 | 2727 | `}` |
|       - | 2728 | `/*` |
|       - | 2729 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|       - | 2730 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|       - | 2731 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|       - | 2732 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|       - | 2733 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|       - | 2734 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|       - | 2735 | ` */` |
|      82 | 2736 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2737 | `{` |
|      85 | 2738 | `	int bLeft = 0;` |
|      85 | 2739 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|      85 | 2740 | `	if( rc == SXRET_OK ){` |
|      29 | 2741 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|      13 | 2742 | `	}` |
|      85 | 2743 | `	if( rc == SXRET_OK && !bLeft ){` |
|       - | 2744 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|       - | 2745 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|       - | 2746 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|       - | 2747 | `		 * set — php's materialization keeps failing too. */` |
|       - | 2748 | `		ph7_class *pScan;` |
|       9 | 2749 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       5 | 2750 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|       3 | 2751 | `		}` |
|       2 | 2752 | `	}` |
|      85 | 2753 | `	return rc;` |
|       3 | 2754 | `}` |
|       - | 2755 |  |
|       - | 2756 | `/*` |
|       - | 2757 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 2758 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 2759 | ` * information.` |
|       - | 2760 | ` * ------------------------------------` |
|       - | 2761 | ` * Simple boring wrapper function.` |
|       - | 2762 | ` * ------------------------------------` |
|       - | 2763 | ` */` |
|     530 | 2764 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|       5 | 2765 | `{` |
|       - | 2766 | `	va_list ap;` |
|       - | 2767 | `	sxi32 rc;` |
|     535 | 2768 | `	va_start(ap,zFormat);` |
|     535 | 2769 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     535 | 2770 | `	va_end(ap);` |
|     535 | 2771 | `	return rc;` |
|       5 | 2772 | `}` |
|       - | 2773 | `/*` |
|       - | 2774 | ` * Throw a TypeError exception from within the VM execution loop.` |
|       - | 2775 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|       - | 2776 | ` */` |
|     380 | 2777 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|       5 | 2778 | `{` |
|       - | 2779 | `	ph7_class *pClass;` |
|       - | 2780 | `	ph7_class_instance *pThis;` |
|       - | 2781 | `	ph7_class_method *pCons;` |
|       - | 2782 | `	ph7_value sArg;` |
|       - | 2783 | `	ph7_value *apArg[1];` |
|       - | 2784 | `	SyBlob sMsg;` |
|       - | 2785 | `	SyString sMsgStr;` |
|     385 | 2786 | `	SyString *pFuncName = &pCallee->sName;` |
|       - | 2787 | `	VmFrame *pFrame;` |
|       - | 2788 | `	sxi32 rc;` |
|     385 | 2789 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     385 | 2790 | `	if( pClass == 0 ){` |
|     ! 0 | 2791 | `		return PH7_ABORT;` |
|       - | 2792 | `	}` |
|     385 | 2793 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     385 | 2794 | `	if( pThis == 0 ){` |
|     ! 0 | 2795 | `		return PH7_ABORT;` |
|       - | 2796 | `	}` |
|     385 | 2797 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2798 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|       - | 2799 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|       - | 2800 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|       - | 2801 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|       - | 2802 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|     573 | 2803 | `	if( pOwnerClass ){` |
|       - | 2804 | `		/* A property hook is named after its PROPERTY, never after the method` |
|       - | 2805 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|       - | 2806 | `		SyBlob sHook;` |
|      41 | 2807 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|      41 | 2808 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|       6 | 2809 | `			if( pArgName ){` |
|       6 | 2810 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|       4 | 2811 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|       2 | 2812 | `					nArg,pArgName,zExpected,zGiven);` |
|       4 | 2813 | `			}else{` |
|     ! 0 | 2814 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|     ! 0 | 2815 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|     ! 0 | 2816 | `					nArg,zExpected,zGiven);` |
|       - | 2817 | `			}` |
|       6 | 2818 | `			SyBlobRelease(&sHook);` |
|       6 | 2819 | `			goto ArgMsgBuilt;` |
|       - | 2820 | `		}` |
|      37 | 2821 | `		SyBlobRelease(&sHook);` |
|      37 | 2822 | `		if( pArgName ){` |
|      28 | 2823 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|      12 | 2824 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|      16 | 2825 | `		}else{` |
|      11 | 2826 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|       4 | 2827 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|       - | 2828 | `		}` |
|      21 | 2829 | `	}else{` |
|       - | 2830 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|     349 | 2831 | `		const char *zShow = 0;` |
|     349 | 2832 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     349 | 2833 | `		if( pArgName ){` |
|     281 | 2834 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|     138 | 2835 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|     143 | 2836 | `		}else{` |
|      72 | 2837 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      34 | 2838 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|       - | 2839 | `		}` |
|       - | 2840 | `	}` |
|     190 | 2841 | `ArgMsgBuilt:` |
|       - | 2842 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|       - | 2843 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|       - | 2844 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|     385 | 2845 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|     379 | 2846 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     379 | 2847 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|     379 | 2848 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|     187 | 2849 | `		}` |
|     187 | 2850 | `	}` |
|     385 | 2851 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     385 | 2852 | `	if( pCons ){` |
|     385 | 2853 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     385 | 2854 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     385 | 2855 | `		apArg[0] = &sArg;` |
|     385 | 2856 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     385 | 2857 | `		PH7_MemObjRelease(&sArg);` |
|     190 | 2858 | `	}` |
|     385 | 2859 | `	SyBlobRelease(&sMsg);` |
|     385 | 2860 | `	pFrame = pVm->pFrame;` |
|     385 | 2861 | `	if( pFrame ){` |
|     385 | 2862 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     385 | 2863 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     190 | 2864 | `	}` |
|     385 | 2865 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     385 | 2866 | `	PH7_ClassInstanceUnref(pThis);` |
|     385 | 2867 | `	if( rc == SXERR_ABORT ){` |
|       6 | 2868 | `		return PH7_ABORT;` |
|       - | 2869 | `	}` |
|     381 | 2870 | `	return PH7_EXCEPTION;` |
|     195 | 2871 | `}` |
|       - | 2872 | `/*` |
|       - | 2873 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|       - | 2874 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|       - | 2875 | ` * positional and the named-argument binding paths of OP_CALL.` |
|       - | 2876 | ` *` |
|       - | 2877 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|       - | 2878 | ` * element uses its overall call position; a NAMED element always reports` |
|       - | 2879 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|       - | 2880 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|       - | 2881 | ` * formal, so no single parameter name applies.` |
|       - | 2882 | ` *` |
|       - | 2883 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|       - | 2884 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|       - | 2885 | ` */` |
|    2620 | 2886 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|       - | 2887 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|       5 | 2888 | `{` |
|       - | 2889 | `	sxi32 rc;` |
|    2625 | 2890 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|      33 | 2891 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|      37 | 2892 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|       - | 2893 | `			const char *zGiven;` |
|      11 | 2894 | `			const char *zExpected = "union";` |
|       - | 2895 | `			char zBuf[128];` |
|       - | 2896 | `			char zTypeBuf[128];` |
|      11 | 2897 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 2898 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      10 | 2899 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2900 | `				zGiven = "null";` |
|     ! 0 | 2901 | `			}else{` |
|       9 | 2902 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 2903 | `			}` |
|      11 | 2904 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|      15 | 2905 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|       4 | 2906 | `					zTypeBuf,sizeof(zTypeBuf));` |
|       4 | 2907 | `			}` |
|      11 | 2908 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|      11 | 2909 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2910 | `		}` |
|      17 | 2911 | `		return SXRET_OK;` |
|       - | 2912 | `	}` |
|    2598 | 2913 | `	if( pFormal->nType < 1` |
|    1411 | 2914 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|    2401 | 2915 | `		return SXRET_OK;` |
|       - | 2916 | `	}` |
|     207 | 2917 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2918 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|       - | 2919 | `		 * per element exactly like the non-variadic paths. */` |
|      61 | 2920 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2921 | `		ph7_class *pClass;` |
|      61 | 2922 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      61 | 2923 | `		if( rcPseudo == 0 ){` |
|       - | 2924 | `			/* Recognised pseudo-type; value mismatches */` |
|       - | 2925 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      14 | 2926 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       6 | 2927 | `				VmClassHintTypeName(pName,0,` |
|       6 | 2928 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       3 | 2929 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       8 | 2930 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2931 | `		}` |
|       - | 2932 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|       - | 2933 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|       - | 2934 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|       - | 2935 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|       - | 2936 | `		 * so ANY non-object is a TypeError, matching php. */` |
|      55 | 2937 | `		pClass = 0;` |
|      55 | 2938 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 2939 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      43 | 2940 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      20 | 2941 | `				VmClassHintTypeName(pName,pClass,` |
|      20 | 2942 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 2943 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      23 | 2944 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2945 | `		}` |
|      34 | 2946 | `		return SXRET_OK;` |
|       - | 2947 | `	}` |
|     149 | 2948 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|      63 | 2949 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|       - | 2950 | `			char zGivenBuf[128];` |
|       8 | 2951 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       2 | 2952 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       6 | 2953 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2954 | `		}` |
|      59 | 2955 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|       - | 2956 | `			char zTypeBuf[128];` |
|       - | 2957 | `			char zGivenBuf[128];` |
|      60 | 2958 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      19 | 2959 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      19 | 2960 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      41 | 2961 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2962 | `		}` |
|      11 | 2963 | `	}else{` |
|       - | 2964 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|       - | 2965 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      89 | 2966 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2967 | `	}` |
|     107 | 2968 | `	return SXRET_OK;` |
|    1315 | 2969 | `}` |
|       - | 2970 | `/*` |
|       - | 2971 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|       - | 2972 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|       - | 2973 | ` * before a required parameter as implicitly required), excluding a trailing` |
|       - | 2974 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|       - | 2975 | ` * pick php's wording — "exactly N expected" when required == total,` |
|       - | 2976 | ` * "at least N" when trailing optionals exist.` |
|       - | 2977 | ` */` |
|    6795 | 2978 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|       5 | 2979 | `{` |
|    6800 | 2980 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    6800 | 2981 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|    6800 | 2982 | `	sxu32 nRequired = 0;` |
|       - | 2983 | `	sxu32 n;` |
|    6800 | 2984 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     735 | 2985 | `		nFormal--;` |
|     365 | 2986 | `	}` |
|   34763 | 2987 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|   27968 | 2988 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    8812 | 2989 | `			nRequired = n + 1;` |
|    4403 | 2990 | `		}` |
|   13985 | 2991 | `	}` |
|    6800 | 2992 | `	*pnNonVariadic = nFormal;` |
|    6800 | 2993 | `	return nRequired;` |
|       5 | 2994 | `}` |
|       - | 2995 | `/*` |
|       - | 2996 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|       - | 2997 | ` * with too few arguments:` |
|       - | 2998 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|       - | 2999 | ` *   {exactly\|at least} M expected` |
|       - | 3000 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|       - | 3001 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|       - | 3002 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|       - | 3003 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|       - | 3004 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|       - | 3005 | ` */` |
|      26 | 3006 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3007 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|       3 | 3008 | `{` |
|       - | 3009 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|       - | 3010 | `	SyBlob sMsg;` |
|      29 | 3011 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      29 | 3012 | `	if( pOwnerClass ){` |
|       5 | 3013 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|       2 | 3014 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|       3 | 3015 | `	}else{` |
|      25 | 3016 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|       - | 3017 | `	}` |
|      29 | 3018 | `	if( bCallSite ){` |
|      27 | 3019 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|      27 | 3020 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|      12 | 3021 | `	}` |
|      29 | 3022 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|      13 | 3023 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|       - | 3024 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      29 | 3025 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       3 | 3026 | `}` |
|       - | 3027 | `/*` |
|       - | 3028 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|       - | 3029 | ` * cannot be referenced:` |
|       - | 3030 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|       - | 3031 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|       - | 3032 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|       - | 3033 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|       - | 3034 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|       - | 3035 | `` * the bare `m()`.`` |
|       - | 3036 | ` */` |
|       - | 3037 | `/*` |
|       - | 3038 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|       - | 3039 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|       - | 3040 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|       - | 3041 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|       - | 3042 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|       - | 3043 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|       - | 3044 | `` * php's `{closure:file:line}`.`` |
|       - | 3045 | ` */` |
|      74 | 3046 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|       - | 3047 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|       1 | 3048 | `{` |
|      75 | 3049 | `	const char *zShow = 0;` |
|      75 | 3050 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|       - | 3051 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|       - | 3052 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|       - | 3053 | `	 * single name applies (the refusal message splits the same way). */` |
|      75 | 3054 | `	if( pOwnerClass && pArgName ){` |
|      34 | 3055 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 3056 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|      11 | 3057 | `			&pOwnerClass->sName,nShow,zShow,nArgPos,pArgName);` |
|      64 | 3058 | `	}else if( pOwnerClass ){` |
|     ! 0 | 3059 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 3060 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|     ! 0 | 3061 | `			&pOwnerClass->sName,nShow,zShow,nArgPos);` |
|      53 | 3062 | `	}else if( pArgName ){` |
|      73 | 3063 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 3064 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|      24 | 3065 | `			nShow,zShow,nArgPos,pArgName);` |
|      25 | 3066 | `	}else{` |
|       7 | 3067 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 3068 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|       2 | 3069 | `			nShow,zShow,nArgPos);` |
|       - | 3070 | `	}` |
|      75 | 3071 | `}` |
|    4026 | 3072 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3073 | `	sxu32 nArgPos,SyString *pArgName)` |
|       2 | 3074 | `{` |
|       - | 3075 | `	SyBlob sMsg;` |
|    4028 | 3076 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    4028 | 3077 | `	if( pOwnerClass ){` |
|       8 | 3078 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) could not be passed by reference",` |
|       3 | 3079 | `			&pOwnerClass->sName,pFuncName,nArgPos,pArgName);` |
|       5 | 3080 | `	}else{` |
|    4022 | 3081 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) could not be passed by reference",` |
|    2010 | 3082 | `			pFuncName,nArgPos,pArgName);` |
|       - | 3083 | `	}` |
|       - | 3084 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|    4028 | 3085 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       2 | 3086 | `}` |
|       - | 3087 | `/*` |
|       - | 3088 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|       - | 3089 | ` * called with too few arguments, in php's ZPP wording:` |
|       - | 3090 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|       - | 3091 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|       - | 3092 | ` * pluralized on the expected count).` |
|       - | 3093 | ` *` |
|       - | 3094 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|       - | 3095 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|       - | 3096 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|       - | 3097 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|       - | 3098 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|       - | 3099 | ` */` |
|      34 | 3100 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3101 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|       1 | 3102 | `{` |
|       - | 3103 | `	SyBlob sMsg;` |
|      35 | 3104 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|      35 | 3105 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|      35 | 3106 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      35 | 3107 | `	if( pOwnerClass ){` |
|     ! 0 | 3108 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 3109 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|     ! 0 | 3110 | `	}else{` |
|      35 | 3111 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      17 | 3112 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       - | 3113 | `	}` |
|       - | 3114 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      35 | 3115 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3116 | `}` |
|       - | 3117 | `/*` |
|       - | 3118 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|       - | 3119 | ` * MANY arguments, in php's ZPP wording:` |
|       - | 3120 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|       - | 3121 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|       - | 3122 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|       - | 3123 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|       - | 3124 | ` * so the caller must not route such a callee here.` |
|       - | 3125 | ` */` |
|      26 | 3126 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3127 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|       1 | 3128 | `{` |
|       - | 3129 | `	SyBlob sMsg;` |
|      27 | 3130 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|      27 | 3131 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|      27 | 3132 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      27 | 3133 | `	if( pOwnerClass ){` |
|     ! 0 | 3134 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 3135 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|     ! 0 | 3136 | `	}else{` |
|      27 | 3137 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      13 | 3138 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|       - | 3139 | `	}` |
|       - | 3140 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      27 | 3141 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3142 | `}` |
|       - | 3143 | `/*` |
|       - | 3144 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|       - | 3145 | ` * named or positional argument resolved to:` |
|       - | 3146 | ` *   C::f(): Argument #N ($x) not passed` |
|       - | 3147 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|       - | 3148 | ` */` |
|       2 | 3149 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3150 | `	sxu32 nArg,SyString *pArgName)` |
|       1 | 3151 | `{` |
|       - | 3152 | `	SyBlob sMsg;` |
|       3 | 3153 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 3154 | `	if( pOwnerClass ){` |
|     ! 0 | 3155 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|     ! 0 | 3156 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|     ! 0 | 3157 | `	}else{` |
|       3 | 3158 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|       - | 3159 | `	}` |
|       - | 3160 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       3 | 3161 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3162 | `}` |
|       - | 3163 | `/*` |
|       - | 3164 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|       - | 3165 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|       - | 3166 | ` */` |
|       - | 3167 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|       - | 3168 | ` * The message is copied into the instance by __construct, so the caller owns` |
|       - | 3169 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|     146 | 3170 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|       5 | 3171 | `{` |
|       - | 3172 | `	ph7_class *pClass;` |
|       - | 3173 | `	ph7_class_instance *pThis;` |
|       - | 3174 | `	ph7_class_method *pCons;` |
|       - | 3175 | `	ph7_value sArg;` |
|       - | 3176 | `	ph7_value *apArg[1];` |
|       - | 3177 | `	SyString sMsgStr;` |
|       - | 3178 | `	VmFrame *pFrame;` |
|       - | 3179 | `	sxi32 rc;` |
|     151 | 3180 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     151 | 3181 | `	if( pClass == 0 ){` |
|     ! 0 | 3182 | `		return PH7_ABORT;` |
|       - | 3183 | `	}` |
|     151 | 3184 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     151 | 3185 | `	if( pThis == 0 ){` |
|     ! 0 | 3186 | `		return PH7_ABORT;` |
|       - | 3187 | `	}` |
|     151 | 3188 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     151 | 3189 | `	if( pCons ){` |
|     151 | 3190 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|     151 | 3191 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     151 | 3192 | `		apArg[0] = &sArg;` |
|     151 | 3193 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     151 | 3194 | `		PH7_MemObjRelease(&sArg);` |
|      73 | 3195 | `	}` |
|     151 | 3196 | `	pFrame = pVm->pFrame;` |
|     151 | 3197 | `	if( pFrame ){` |
|     151 | 3198 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     151 | 3199 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      73 | 3200 | `	}` |
|     151 | 3201 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     151 | 3202 | `	PH7_ClassInstanceUnref(pThis);` |
|     151 | 3203 | `	if( rc == SXERR_ABORT ){` |
|       6 | 3204 | `		return PH7_ABORT;` |
|       - | 3205 | `	}` |
|     147 | 3206 | `	return PH7_EXCEPTION;` |
|      78 | 3207 | `}` |
|       - | 3208 | `/*` |
|       - | 3209 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|       - | 3210 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|       - | 3211 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|       - | 3212 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|       - | 3213 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|       - | 3214 | ` * other callee falls through to the ordinary Class::method rendering.` |
|       - | 3215 | ` */` |
|       - | 3216 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|  632166 | 3217 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|       5 | 3218 | `{` |
|  632171 | 3219 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|  632171 | 3220 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|  632049 | 3221 | `		return 0;` |
|       - | 3222 | `	}` |
|     127 | 3223 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      83 | 3224 | `		return 0;` |
|       - | 3225 | `	}` |
|      47 | 3226 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|      35 | 3227 | `		*pzKind = "get";` |
|      31 | 3228 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|      16 | 3229 | `		*pzKind = "set";` |
|      10 | 3230 | `	}else{` |
|     ! 0 | 3231 | `		return 0;` |
|       - | 3232 | `	}` |
|      47 | 3233 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|      47 | 3234 | `	return 1;` |
|  316088 | 3235 | `}` |
|     128 | 3236 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3237 | `{` |
|       - | 3238 | `	SyString sProp;` |
|       - | 3239 | `	const char *zKind;` |
|     133 | 3240 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|     123 | 3241 | `		return 0;` |
|       - | 3242 | `	}` |
|      13 | 3243 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|      13 | 3244 | `	return 1;` |
|      69 | 3245 | `}` |
|       - | 3246 | `/*` |
|       - | 3247 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|       - | 3248 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|       - | 3249 | ` * anything else uses its display name (which is also what strips a closure's` |
|       - | 3250 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|       - | 3251 | ` * instead — they are thrown from call sites that already resolved it.` |
|       - | 3252 | ` */` |
|     146 | 3253 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3254 | `{` |
|     151 | 3255 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      96 | 3256 | `		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){` |
|       7 | 3257 | `			return;` |
|       - | 3258 | `		}` |
|      90 | 3259 | `		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);` |
|      90 | 3260 | `		return;` |
|       - | 3261 | `	}` |
|       - | 3262 | `	{` |
|      59 | 3263 | `		const char *zShow = 0;` |
|      59 | 3264 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|      59 | 3265 | `		if( zShow && nShow > 0 ){` |
|      59 | 3266 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      27 | 3267 | `		}` |
|       - | 3268 | `	}` |
|      78 | 3269 | `}` |
|     142 | 3270 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|       5 | 3271 | `{` |
|       - | 3272 | `	SyBlob sMsg,sName;` |
|       - | 3273 | `	sxi32 rc;` |
|     147 | 3274 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     147 | 3275 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|     147 | 3276 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|     147 | 3277 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|     142 | 3278 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|     147 | 3279 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|     147 | 3280 | `	SyBlobRelease(&sName);` |
|     147 | 3281 | `	SyBlobRelease(&sMsg);` |
|     147 | 3282 | `	return rc;` |
|       5 | 3283 | `}` |
|       - | 3284 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|       - | 3285 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|       - | 3286 | ` * php calls it a "method" when it is one. */` |
|       4 | 3287 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|       2 | 3288 | `{` |
|       - | 3289 | `	SyBlob sMsg,sName;` |
|       - | 3290 | `	sxi32 rc;` |
|       6 | 3291 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       6 | 3292 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|       6 | 3293 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|       6 | 3294 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|       4 | 3295 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|       4 | 3296 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|       6 | 3297 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|       6 | 3298 | `	SyBlobRelease(&sName);` |
|       6 | 3299 | `	SyBlobRelease(&sMsg);` |
|       6 | 3300 | `	return rc;` |
|       2 | 3301 | `}` |
|       - | 3302 | `/*` |
|       - | 3303 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|       - | 3304 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|       - | 3305 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|       - | 3306 | ` */` |
|    1004 | 3307 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|       5 | 3308 | `{` |
|    1009 | 3309 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     109 | 3310 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 3311 | `	}` |
|     905 | 3312 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     103 | 3313 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     103 | 3314 | `		if( pThis && pThis->pClass ){` |
|     103 | 3315 | `			SyString *pName = &pThis->pClass->sName;` |
|     103 | 3316 | `			sxu32 n = pName->nByte;` |
|     103 | 3317 | `			if( n >= nBuf ){` |
|     ! 0 | 3318 | `				n = nBuf - 1;` |
|     ! 0 | 3319 | `			}` |
|     103 | 3320 | `			SyMemcpy(pName->zString,zBuf,n);` |
|     103 | 3321 | `			zBuf[n] = 0;` |
|     103 | 3322 | `			return zBuf;` |
|       - | 3323 | `		}` |
|     ! 0 | 3324 | `		return "object";` |
|       - | 3325 | `	}` |
|     807 | 3326 | `	return ph7_type_name(pVal);` |
|     507 | 3327 | `}` |
|       - | 3328 | `/*` |
|       - | 3329 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|       - | 3330 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|       - | 3331 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|       - | 3332 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|       - | 3333 | ` */` |
|      18 | 3334 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|       3 | 3335 | `{` |
|       - | 3336 | `	ph7_class *pClass;` |
|       - | 3337 | `	ph7_class_instance *pThis;` |
|       - | 3338 | `	ph7_class_method *pCons;` |
|       - | 3339 | `	ph7_value sArg;` |
|       - | 3340 | `	ph7_value *apArg[1];` |
|       - | 3341 | `	SyBlob sMsg;` |
|       - | 3342 | `	SyString sMsgStr;` |
|       - | 3343 | `	VmFrame *pFrame;` |
|       - | 3344 | `	sxi32 rc;` |
|      21 | 3345 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|       - | 3346 | `	char zNameBuf[64];` |
|      21 | 3347 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|      21 | 3348 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|      21 | 3349 | `	if( pClass == 0 ){` |
|     ! 0 | 3350 | `		return PH7_ABORT;` |
|       - | 3351 | `	}` |
|      21 | 3352 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      21 | 3353 | `	if( pThis == 0 ){` |
|     ! 0 | 3354 | `		return PH7_ABORT;` |
|       - | 3355 | `	}` |
|      21 | 3356 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      21 | 3357 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|      21 | 3358 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      21 | 3359 | `	if( pCons ){` |
|      21 | 3360 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      21 | 3361 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      21 | 3362 | `		apArg[0] = &sArg;` |
|      21 | 3363 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      21 | 3364 | `		PH7_MemObjRelease(&sArg);` |
|       9 | 3365 | `	}` |
|      21 | 3366 | `	SyBlobRelease(&sMsg);` |
|      21 | 3367 | `	pFrame = pVm->pFrame;` |
|      21 | 3368 | `	if( pFrame ){` |
|      21 | 3369 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      21 | 3370 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       9 | 3371 | `	}` |
|      21 | 3372 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      21 | 3373 | `	PH7_ClassInstanceUnref(pThis);` |
|      21 | 3374 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3375 | `		return PH7_ABORT;` |
|       - | 3376 | `	}` |
|      21 | 3377 | `	return PH7_EXCEPTION;` |
|      12 | 3378 | `}` |
|       - | 3379 | `/*` |
|       - | 3380 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|       - | 3381 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|       - | 3382 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|       - | 3383 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|       - | 3384 | ` */` |
|       - | 3385 | `/*` |
|       - | 3386 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|       - | 3387 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|       - | 3388 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|       - | 3389 | ` * type field.` |
|       - | 3390 | ` */` |
|  783204 | 3391 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|       5 | 3392 | `{` |
|  783209 | 3393 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|       5 | 3394 | `}` |
|   11848 | 3395 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|       5 | 3396 | `{` |
|   11853 | 3397 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|   11853 | 3398 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|       - | 3399 | `	const char *zGiven;` |
|       - | 3400 | `	ph7_class *pHintScope;` |
|       - | 3401 | `	char zBuf[128];` |
|       - | 3402 | `	char zTypeBuf[128];` |
|       - | 3403 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|   11853 | 3404 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|     ! 0 | 3405 | `		return SXRET_OK;` |
|       - | 3406 | `	}` |
|       - | 3407 | `	/* never return type: the function must not return at all. An explicit` |
|       - | 3408 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|       - | 3409 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|       - | 3410 | `	 * the call site). */` |
|   11853 | 3411 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       6 | 3412 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|       - | 3413 | `	}` |
|       - | 3414 | `	/* void return type: the function must not produce a value. */` |
|   11849 | 3415 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|    1749 | 3416 | `		if( pValue == 0 ){` |
|    1745 | 3417 | `			return SXRET_OK;` |
|       - | 3418 | `		}` |
|       - | 3419 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|       - | 3420 | `		 * store, not a return: php compiles no return statement there at all,` |
|       - | 3421 | `		 * and still reports the hook's return type as void. PHL carries the` |
|       - | 3422 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|       - | 3423 | `		 * so the one implicit value this arm must not reject is that one. */` |
|       6 | 3424 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|       6 | 3425 | `			return SXRET_OK;` |
|       - | 3426 | `		}` |
|       - | 3427 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|       - | 3428 | `		 * still counts as "returned a value" here. */` |
|     ! 0 | 3429 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|     ! 0 | 3430 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|       - | 3431 | `	}` |
|       - | 3432 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|       - | 3433 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|       - | 3434 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|   10105 | 3435 | `	if( pValue == 0 ){` |
|      33 | 3436 | `		const char *zExpected = "value";` |
|      33 | 3437 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      48 | 3438 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|      15 | 3439 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|      15 | 3440 | `		}` |
|       - | 3441 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|       - | 3442 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|      33 | 3443 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|       - | 3444 | `	}` |
|       - | 3445 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|       - | 3446 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|       - | 3447 | `	 * matching how every other typed return reports a missing value.) */` |
|   10075 | 3448 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|       5 | 3449 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 3450 | `			return SXRET_OK;` |
|       - | 3451 | `		}` |
|       4 | 3452 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|       1 | 3453 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3454 | `	}` |
|       - | 3455 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|       - | 3456 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|       - | 3457 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|   10071 | 3458 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|      40 | 3459 | `		return SXRET_OK;` |
|       - | 3460 | `	}` |
|       - | 3461 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|       - | 3462 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|       - | 3463 | `	 * Check by value before the real-class instanceof branch below. */` |
|   10035 | 3464 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     237 | 3465 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|     237 | 3466 | `		if( rcPseudo == 1 ){` |
|     121 | 3467 | `			return SXRET_OK;` |
|       - | 3468 | `		}` |
|     121 | 3469 | `		if( rcPseudo == 0 ){` |
|      18 | 3470 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       8 | 3471 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|       4 | 3472 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3473 | `		}` |
|       - | 3474 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      54 | 3475 | `	}` |
|       - | 3476 | `	/* The two branches below are the only ones that can name a class, so the` |
|       - | 3477 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|       - | 3478 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|       - | 3479 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|       - | 3480 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|       - | 3481 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|       - | 3482 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    9911 | 3483 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|       - | 3484 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|       - | 3485 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|       - | 3486 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    9911 | 3487 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       - | 3488 | `		sxi32 rcU;` |
|      41 | 3489 | `		const char *zExpected = "union";` |
|      41 | 3490 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|      41 | 3491 | `		if( rcU == SXRET_OK ){` |
|      30 | 3492 | `			return SXRET_OK;` |
|       - | 3493 | `		}` |
|      13 | 3494 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      11 | 3495 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       7 | 3496 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 3497 | `			zGiven = "null";` |
|     ! 0 | 3498 | `		}else{` |
|       3 | 3499 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3500 | `		}` |
|      13 | 3501 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      18 | 3502 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|       5 | 3503 | `				zTypeBuf,sizeof(zTypeBuf));` |
|       5 | 3504 | `		}` |
|      13 | 3505 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|       - | 3506 | `	}` |
|       - | 3507 | `	/* Class return type — instanceof check. The class name is a length-` |
|       - | 3508 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|       - | 3509 | `	 * it into the TypeError message. */` |
|    9875 | 3510 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     113 | 3511 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|     113 | 3512 | `		ph7_class *pExpected = 0;` |
|     113 | 3513 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|      34 | 3514 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      28 | 3515 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      16 | 3516 | `			}else{` |
|       8 | 3517 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3518 | `			}` |
|      49 | 3519 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      15 | 3520 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|       - | 3521 | `		}` |
|      83 | 3522 | `		return SXRET_OK;` |
|       - | 3523 | `	}` |
|       - | 3524 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|       - | 3525 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|       - | 3526 | `	 * non-nullable scalar return — a TypeError. */` |
|    9767 | 3527 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      26 | 3528 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       8 | 3529 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3530 | `			"null");` |
|       - | 3531 | `	}` |
|       - | 3532 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|       - | 3533 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    9751 | 3534 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    9629 | 3535 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    9629 | 3536 | `		return SXRET_OK;` |
|       - | 3537 | `	}` |
|       - | 3538 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|       - | 3539 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|       - | 3540 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|       - | 3541 | `	 * still rejects the object under strict_types. */` |
|     127 | 3542 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      22 | 3543 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      31 | 3544 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      18 | 3545 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      20 | 3546 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      29 | 3547 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       9 | 3548 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       9 | 3549 | `				zGiven);` |
|       - | 3550 | `		}` |
|       1 | 3551 | `	}` |
|       - | 3552 | `	/* Array <-> scalar is never compatible. */` |
|     109 | 3553 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      33 | 3554 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      10 | 3555 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 3556 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3557 | `	}` |
|       - | 3558 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|       - | 3559 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|       - | 3560 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|       - | 3561 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|      84 | 3562 | `	if( !bStrict` |
|      83 | 3563 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|      49 | 3564 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|      54 | 3565 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|      12 | 3566 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       3 | 3567 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3568 | `			"string");` |
|       - | 3569 | `	}` |
|      82 | 3570 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|      80 | 3571 | `		return SXRET_OK;` |
|       - | 3572 | `	}` |
|       4 | 3573 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       1 | 3574 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       1 | 3575 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    5929 | 3576 | `}` |
|       - | 3577 | `/*` |
|       - | 3578 | ` * Report a fatal named-argument error.` |
|       - | 3579 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|       - | 3580 | ` */` |
|      12 | 3581 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       3 | 3582 | `{` |
|       - | 3583 | `	SyBlob sMsg;` |
|       - | 3584 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|       - | 3585 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|       - | 3586 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|       - | 3587 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|       - | 3588 | `	 * unconditional fatal even inside try/catch. */` |
|      15 | 3589 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 3590 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|      15 | 3591 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 3592 | `}` |
|       - | 3593 | `/*` |
|       - | 3594 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 3595 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 3596 | ` * information.` |
|       - | 3597 | ` * ------------------------------------` |
|       - | 3598 | ` * Simple boring wrapper function.` |
|       - | 3599 | ` * ------------------------------------` |
|       - | 3600 | ` */` |
|   25688 | 3601 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|       5 | 3602 | `{` |
|       - | 3603 | `	sxi32 rc;` |
|   25693 | 3604 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|   25693 | 3605 | `	return rc;` |
|       5 | 3606 | `}` |
|       - | 3607 | `/*` |
|       - | 3608 | ` * Resolve function context from the current frame.` |
|       - | 3609 | ` */` |
|       - | 3610 | `/*` |
|       - | 3611 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|       - | 3612 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|       - | 3613 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|       - | 3614 | ` * straight at the function's own name otherwise.` |
|       - | 3615 | ` */` |
|  631984 | 3616 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|       5 | 3617 | `{` |
|  631989 | 3618 | `	const char *zName = pFunc->sName.zString;` |
|  631989 | 3619 | `	int nName = (int)pFunc->sName.nByte;` |
|  682448 | 3620 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|  684133 | 3621 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|       - | 3622 | `	/* A property hook is not a method in php and never shows the name PHL` |
|       - | 3623 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|       - | 3624 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|       - | 3625 | `	 * report from inside one, and what makes the trace line read` |
|       - | 3626 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|       - | 3627 | ``	 * `C::$p::get` for free. */`` |
|       - | 3628 | `	{` |
|       - | 3629 | `		SyString sProp;` |
|       - | 3630 | `		const char *zKind;` |
|  631989 | 3631 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      44 | 3632 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      13 | 3633 | `				"$%z::%s",&sProp,zKind);` |
|      31 | 3634 | `			*pzOut = pVm->zDisplayName;` |
|      31 | 3635 | `			return n;` |
|       - | 3636 | `		}` |
|       - | 3637 | `	}` |
|  631963 | 3638 | `	if( bClosure ){` |
|       - | 3639 | `		int n;` |
|    3455 | 3640 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|       - | 3641 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|       - | 3642 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|    3455 | 3643 | `			*pzOut = pFunc->sClosureName.zString;` |
|    3455 | 3644 | `			return (int)pFunc->sClosureName.nByte;` |
|       - | 3645 | `		}` |
|     ! 0 | 3646 | `		if( pFunc->sFile.nByte > 0 ){` |
|     ! 0 | 3647 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|     ! 0 | 3648 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|     ! 0 | 3649 | `		}else{` |
|     ! 0 | 3650 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|       - | 3651 | `		}` |
|     ! 0 | 3652 | `		*pzOut = pVm->zDisplayName;` |
|     ! 0 | 3653 | `		return n;` |
|       - | 3654 | `	}` |
|  628513 | 3655 | `	*pzOut = zName;` |
|  628513 | 3656 | `	return nName;` |
|  315997 | 3657 | `}` |
|    1148 | 3658 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|       4 | 3659 | `{` |
|       - | 3660 | `	VmFrame *pFrame;` |
|       - | 3661 | `	ph7_vm_func *pFunc;` |
|    1152 | 3662 | `	*pzFuncName = 0;` |
|    1152 | 3663 | `	*pnFuncLen = 0;` |
|    1152 | 3664 | `	pFrame = pVm->pFrame;` |
|    1152 | 3665 | `	if( pFrame == 0 ){` |
|     ! 0 | 3666 | `		return;` |
|       - | 3667 | `	}` |
|    1152 | 3668 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    1152 | 3669 | `	if( pFrame->pParent == 0 ){` |
|    1112 | 3670 | `		return;` |
|       - | 3671 | `	}` |
|      44 | 3672 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      44 | 3673 | `	if( pFunc == 0 ){` |
|     ! 0 | 3674 | `		return;` |
|       - | 3675 | `	}` |
|      44 | 3676 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|     578 | 3677 | `}` |
|       - | 3678 | `/*` |
|       - | 3679 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|       - | 3680 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|       - | 3681 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|       - | 3682 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|       - | 3683 | ` * built-in library and already produces php's exact byte format, which keeps` |
|       - | 3684 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|       - | 3685 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|       - | 3686 | ` * synthesize what it can.` |
|       - | 3687 | ` */` |
|     598 | 3688 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3689 | `{` |
|       - | 3690 | `	ph7_class_method *pGetTrace;` |
|       - | 3691 | `	ph7_value sTrace;` |
|       - | 3692 | `	const char *zTmp;` |
|       - | 3693 | `	int nTmp;` |
|     602 | 3694 | `	int bDone = 0;` |
|       - | 3695 | `	int bSaved;` |
|     602 | 3696 | `	if( pThis == 0 ){` |
|       5 | 3697 | `		return 0;` |
|       - | 3698 | `	}` |
|     598 | 3699 | `	if( pVm->bRenderingUncaught ){` |
|       - | 3700 | `		/* Already inside a report: do not run userland trace code again. */` |
|     ! 0 | 3701 | `		return 0;` |
|       - | 3702 | `	}` |
|     598 | 3703 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|     598 | 3704 | `	if( pGetTrace == 0 ){` |
|     ! 0 | 3705 | `		return 0;` |
|       - | 3706 | `	}` |
|     598 | 3707 | `	PH7_MemObjInit(pVm,&sTrace);` |
|       - | 3708 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|       - | 3709 | `	 * anything it calls) throws, the throw would be reported by this very` |
|       - | 3710 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|       - | 3711 | `	 * trace rather than re-entering here forever. */` |
|     598 | 3712 | `	bSaved = pVm->bRenderingUncaught;` |
|     598 | 3713 | `	pVm->bRenderingUncaught = 1;` |
|     598 | 3714 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|     598 | 3715 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|     598 | 3716 | `		if( zTmp && nTmp > 0 ){` |
|     598 | 3717 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     598 | 3718 | `			bDone = 1;` |
|     297 | 3719 | `		}` |
|     297 | 3720 | `	}` |
|     598 | 3721 | `	PH7_MemObjRelease(&sTrace);` |
|     598 | 3722 | `	pVm->bRenderingUncaught = bSaved;` |
|     598 | 3723 | `	return bDone;` |
|     303 | 3724 | `}` |
|       - | 3725 | `/*` |
|       - | 3726 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|       - | 3727 | ` *` |
|       - | 3728 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|       - | 3729 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|       - | 3730 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|       - | 3731 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|       - | 3732 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|       - | 3733 | ` *             trailer.` |
|       - | 3734 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|       - | 3735 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|       - | 3736 | ` * call; this routine only appends.` |
|       - | 3737 | ` */` |
|     598 | 3738 | `static void VmRenderUncaughtEntry(` |
|       - | 3739 | `	ph7_vm *pVm,SyBlob *pOut,` |
|       - | 3740 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|       - | 3741 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|       - | 3742 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|       - | 3743 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|       - | 3744 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|       4 | 3745 | `{` |
|       - | 3746 | `	SyString *pFile;` |
|     602 | 3747 | `	if( nThrowLine == 0 ){` |
|       5 | 3748 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|       2 | 3749 | `	}` |
|     602 | 3750 | `	if( nCallLine == 0 ){` |
|     602 | 3751 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     602 | 3752 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|     299 | 3753 | `	}` |
|     602 | 3754 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|     ! 0 | 3755 | `		zClass = "Exception";` |
|     ! 0 | 3756 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|     ! 0 | 3757 | `	}` |
|     602 | 3758 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     564 | 3759 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     280 | 3760 | `	}` |
|     602 | 3761 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     602 | 3762 | `	if( bFirst ){` |
|     596 | 3763 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|     300 | 3764 | `	}else{` |
|       8 | 3765 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       - | 3766 | `	}` |
|     602 | 3767 | `	SyBlobAppend(pOut,zClass,nClass);` |
|     602 | 3768 | `	if( zMsg && nMsg > 0 ){` |
|     602 | 3769 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|     602 | 3770 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|     299 | 3771 | `	}` |
|     602 | 3772 | `	if( pFile ){` |
|     602 | 3773 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     299 | 3774 | `	}` |
|     602 | 3775 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       - | 3776 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|       - | 3777 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|       - | 3778 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|       - | 3779 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|       - | 3780 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|       - | 3781 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|       - | 3782 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|     602 | 3783 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|       5 | 3784 | `		int bFrame = 0;` |
|       5 | 3785 | `		if( zFuncName && nFuncLen > 0 ){` |
|       3 | 3786 | `			if( pFile ){` |
|       - | 3787 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|       - | 3788 | `				 * running inside it. */` |
|       4 | 3789 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|       2 | 3790 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|       2 | 3791 | `			}else{` |
|     ! 0 | 3792 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|       - | 3793 | `			}` |
|       3 | 3794 | `			bFrame = 1;` |
|       1 | 3795 | `		}` |
|       - | 3796 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|       7 | 3797 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|       2 | 3798 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|       2 | 3799 | `	}` |
|     602 | 3800 | `	if( bLast && pFile ){` |
|     596 | 3801 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|     596 | 3802 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     296 | 3803 | `	}` |
|     602 | 3804 | `}` |
|       - | 3805 | `/*` |
|       - | 3806 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|       - | 3807 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|       - | 3808 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|       - | 3809 | ` */` |
|       4 | 3810 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|       1 | 3811 | `{` |
|       - | 3812 | `	SyBlob sOut;` |
|       - | 3813 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|       - | 3814 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|       5 | 3815 | `	pVm->iExitStatus = 255;` |
|       5 | 3816 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3817 | `		return PH7_OK;` |
|       - | 3818 | `	}` |
|       5 | 3819 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       5 | 3820 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|       5 | 3821 | `	VmCallErrorHandler(pVm,&sOut);` |
|       5 | 3822 | `	SyBlobRelease(&sOut);` |
|       5 | 3823 | `	return PH7_ABORT;` |
|       3 | 3824 | `}` |
|       - | 3825 | `/*` |
|       - | 3826 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|       - | 3827 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|       - | 3828 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|       - | 3829 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|       - | 3830 | ` */` |
|       - | 3831 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|     594 | 3832 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|       4 | 3833 | `{` |
|       - | 3834 | `	ph7_value *pValue;` |
|       - | 3835 | `	ph7_class_instance *pPrev;` |
|       - | 3836 | `	ph7_class *pThrowable;` |
|     598 | 3837 | `	if( pThis == 0 ){` |
|     ! 0 | 3838 | `		return 0;` |
|       - | 3839 | `	}` |
|     598 | 3840 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     598 | 3841 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     592 | 3842 | `		return 0;` |
|       - | 3843 | `	}` |
|       8 | 3844 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 3845 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|       - | 3846 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|       - | 3847 | `	 * never renders a stray object as an exception entry. */` |
|       8 | 3848 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       8 | 3849 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|     ! 0 | 3850 | `		return 0;` |
|       - | 3851 | `	}` |
|       8 | 3852 | `	return pPrev;` |
|     301 | 3853 | `}` |
|       - | 3854 | `/*` |
|       - | 3855 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|       - | 3856 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|       - | 3857 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|       - | 3858 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|       - | 3859 | ` */` |
|      16 | 3860 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|       3 | 3861 | `{` |
|       - | 3862 | `	ph7_value *pValue;` |
|      19 | 3863 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|     ! 0 | 3864 | `		return;` |
|       - | 3865 | `	}` |
|      19 | 3866 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      19 | 3867 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       3 | 3868 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|       - | 3869 | `	}` |
|      17 | 3870 | `	pPrev->iRef++;` |
|       - | 3871 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|       - | 3872 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|      17 | 3873 | `	PH7_MemObjRelease(pValue);` |
|      17 | 3874 | `	pValue->x.pOther = pPrev;` |
|      17 | 3875 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|      11 | 3876 | `}` |
|       - | 3877 | `/*` |
|       - | 3878 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|       - | 3879 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|       - | 3880 | ` * absent or yields an empty string.` |
|       - | 3881 | ` */` |
|       - | 3882 | `/*` |
|       - | 3883 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|       - | 3884 | ` * 0 when the class exposes no getLine().` |
|       - | 3885 | ` */` |
|     594 | 3886 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       4 | 3887 | `{` |
|       - | 3888 | `	ph7_class_method *pGetLine;` |
|       - | 3889 | `	ph7_value sLine;` |
|     598 | 3890 | `	sxu32 nLine = 0;` |
|     598 | 3891 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|     598 | 3892 | `	if( pGetLine == 0 ){` |
|     ! 0 | 3893 | `		return 0;` |
|       - | 3894 | `	}` |
|     598 | 3895 | `	PH7_MemObjInit(pVm,&sLine);` |
|     598 | 3896 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|     598 | 3897 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|     598 | 3898 | `		if( n > 0 ){` |
|     598 | 3899 | `			nLine = (sxu32)n;` |
|     297 | 3900 | `		}` |
|     297 | 3901 | `	}` |
|     598 | 3902 | `	PH7_MemObjRelease(&sLine);` |
|     598 | 3903 | `	return nLine;` |
|     301 | 3904 | `}` |
|     594 | 3905 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3906 | `{` |
|       - | 3907 | `	ph7_class_method *pGetMessage;` |
|       - | 3908 | `	ph7_value sMsg;` |
|       - | 3909 | `	const char *zTmp;` |
|       - | 3910 | `	int nTmp;` |
|     598 | 3911 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|     598 | 3912 | `	if( pGetMessage == 0 ){` |
|     ! 0 | 3913 | `		return;` |
|       - | 3914 | `	}` |
|     598 | 3915 | `	PH7_MemObjInit(pVm,&sMsg);` |
|     598 | 3916 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|     598 | 3917 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|     598 | 3918 | `		if( zTmp && nTmp > 0 ){` |
|     598 | 3919 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     297 | 3920 | `		}` |
|     297 | 3921 | `	}` |
|     598 | 3922 | `	PH7_MemObjRelease(&sMsg);` |
|     301 | 3923 | `}` |
|       - | 3924 | `/*` |
|       - | 3925 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|       - | 3926 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|       - | 3927 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|       - | 3928 | ` * outermost (the actually-uncaught) exception.` |
|       - | 3929 | ` *` |
|       - | 3930 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|       - | 3931 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|       - | 3932 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|       - | 3933 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|       - | 3934 | ` */` |
|       - | 3935 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|     588 | 3936 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|       4 | 3937 | `{` |
|       - | 3938 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|     592 | 3939 | `	int nChain = 0;` |
|       - | 3940 | `	int i;` |
|       - | 3941 | `	SyBlob sOut;` |
|       - | 3942 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|       - | 3943 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|       - | 3944 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|     592 | 3945 | `	pVm->iExitStatus = 255;` |
|     592 | 3946 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3947 | `		return PH7_OK;` |
|       - | 3948 | `	}` |
|       - | 3949 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|       - | 3950 | `	 * collected) or the hard cap. */` |
|    1186 | 3951 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|     606 | 3952 | `		for( i = 0 ; i < nChain ; ++i ){` |
|      10 | 3953 | `			if( apChain[i] == pThis ){` |
|     ! 0 | 3954 | `				pThis = 0; /* cycle: stop the walk */` |
|     ! 0 | 3955 | `				break;` |
|       - | 3956 | `			}` |
|       6 | 3957 | `		}` |
|     598 | 3958 | `		if( pThis == 0 ){` |
|     ! 0 | 3959 | `			break;` |
|       - | 3960 | `		}` |
|     598 | 3961 | `		apChain[nChain++] = pThis;` |
|     598 | 3962 | `		pThis = VmExceptionGetPrevious(pThis);` |
|       4 | 3963 | `	}` |
|     592 | 3964 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       - | 3965 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|       - | 3966 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|    1186 | 3967 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|     598 | 3968 | `		ph7_class_instance *pEnt = apChain[i];` |
|       - | 3969 | `		SyBlob sMsg;` |
|     598 | 3970 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     598 | 3971 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|     895 | 3972 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|     594 | 3973 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|     594 | 3974 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|     297 | 3975 | `			zFuncName,nFuncLen,` |
|     594 | 3976 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|     297 | 3977 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|     297 | 3978 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|     598 | 3979 | `		SyBlobRelease(&sMsg);` |
|     301 | 3980 | `	}` |
|     592 | 3981 | `	VmCallErrorHandler(pVm,&sOut);` |
|     592 | 3982 | `	SyBlobRelease(&sOut);` |
|     592 | 3983 | `	return PH7_ABORT;` |
|     298 | 3984 | `}` |
|       - | 3985 | `/*` |
|       - | 3986 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|       - | 3987 | ` *` |
|       - | 3988 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|       - | 3989 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|       - | 3990 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|       - | 3991 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|       - | 3992 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|       - | 3993 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|       - | 3994 | ` */` |
| 1556170 | 3995 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|       5 | 3996 | `{` |
| 1556175 | 3997 | `	if( pVm->bCoalesceArmed ){` |
|       8 | 3998 | `		if( pVm->pCoalesceObj ){` |
|       8 | 3999 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|       3 | 4000 | `		}` |
|       8 | 4001 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       8 | 4002 | `		pVm->pCoalesceObj = 0;` |
|       8 | 4003 | `		pVm->bCoalesceArmed = 0;` |
|       3 | 4004 | `	}` |
| 1556175 | 4005 | `}` |
|       - | 4006 | `/*` |
|       - | 4007 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|       - | 4008 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|       - | 4009 | ` * is a literal, non-formatted string; callers that need formatting should` |
|       - | 4010 | ` * build the SyBlob themselves and pass its data + length.` |
|       - | 4011 | ` *` |
|       - | 4012 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|       - | 4013 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|       - | 4014 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|       - | 4015 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|       - | 4016 | ` */` |
|  140996 | 4017 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|       - | 4018 | `	ph7_vm *pVm,` |
|       - | 4019 | `	const char *zClass,` |
|       - | 4020 | `	const char *zMsg,` |
|       - | 4021 | `	sxu32 nMsg` |
|       5 | 4022 | `){` |
|       - | 4023 | `	ph7_class *pClass;` |
|       - | 4024 | `	ph7_class_instance *pThis;` |
|       - | 4025 | `	ph7_class_method *pCons;` |
|       - | 4026 | `	VmFrame *pFrame;` |
|       - | 4027 | `	sxi32 rc;` |
|  141001 | 4028 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|  141001 | 4029 | `	if( pClass == 0 ){` |
|     ! 0 | 4030 | `		return SXERR_ABORT;` |
|       - | 4031 | `	}` |
|  141001 | 4032 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|  141001 | 4033 | `	if( pThis == 0 ){` |
|     ! 0 | 4034 | `		return SXERR_ABORT;` |
|       - | 4035 | `	}` |
|  141001 | 4036 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|  141001 | 4037 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|       - | 4038 | `		ph7_value sArg;` |
|       - | 4039 | `		ph7_value *apArg[1];` |
|       - | 4040 | `		SyString sMsgStr;` |
|  141001 | 4041 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|  141001 | 4042 | `		PH7_MemObjInit(pVm,&sArg);` |
|  141001 | 4043 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  141001 | 4044 | `		apArg[0] = &sArg;` |
|  141001 | 4045 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|  141001 | 4046 | `		PH7_MemObjRelease(&sArg);` |
|  141001 | 4047 | `		pVm->nExcCtorDepth--;` |
|   70498 | 4048 | `	}` |
|  141001 | 4049 | `	pFrame = pVm->pFrame;` |
|  141001 | 4050 | `	if( pFrame ){` |
|  141001 | 4051 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  141001 | 4052 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   70498 | 4053 | `	}` |
|  141001 | 4054 | `	rc = VmThrowException(pVm,pThis);` |
|  141001 | 4055 | `	PH7_ClassInstanceUnref(pThis);` |
|  141001 | 4056 | `	return rc;` |
|   70503 | 4057 | `}` |
|       - | 4058 | `/*` |
|       - | 4059 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|       - | 4060 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|       - | 4061 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|       - | 4062 | ` *` |
|       - | 4063 | ` *   int/float/bool/null      arithmetic proceeds` |
|       - | 4064 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|       - | 4065 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|       - | 4066 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|       - | 4067 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|       - | 4068 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|       - | 4069 | ` *   object/resource          TypeError, naming the object's CLASS` |
|       - | 4070 | ` *` |
|       - | 4071 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|       - | 4072 | ` */` |
|       - | 4073 | `/*` |
|       - | 4074 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|       - | 4075 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|       - | 4076 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|       - | 4077 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|       - | 4078 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|       - | 4079 | ` * to depth 1).` |
|       - | 4080 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|       - | 4081 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|       - | 4082 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|       - | 4083 | ` * frame shape is file/line/function[/class/type], matching the default` |
|       - | 4084 | ` * zend.exception_ignore_args=On.` |
|       - | 4085 | ` */` |
| 1456124 | 4086 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|       5 | 4087 | `{` |
|       - | 4088 | `	SyString *pFile;` |
|       - | 4089 | `	VmFrame *pFrame;` |
|       - | 4090 | `	ph7_value *pValue;` |
| 1456129 | 4091 | `	sxi32 nDone = 0;` |
| 1456129 | 4092 | `	pValue = ph7_new_scalar(&(*pVm));` |
| 1456129 | 4093 | `	if( pValue == 0 ){` |
|     ! 0 | 4094 | `		return;` |
|       - | 4095 | `	}` |
| 1456129 | 4096 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1456129 | 4097 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 2087551 | 4098 | `	while( pFrame ){` |
|       - | 4099 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|       - | 4100 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|       - | 4101 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|       - | 4102 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
| 2087551 | 4103 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|      39 | 4104 | `			break;` |
|       - | 4105 | `		}` |
| 2087513 | 4106 | `		nDone++;` |
| 2087513 | 4107 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 4108 | `		ph7_value *pEntry;` |
| 2087513 | 4109 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|       - | 4110 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|  728048 | 4111 | `			break;` |
|       - | 4112 | `		}` |
|  631427 | 4113 | `		pEntry = ph7_new_array(&(*pVm));` |
|  631427 | 4114 | `		if( pEntry == 0 ){` |
|     ! 0 | 4115 | `			break;` |
|       - | 4116 | `		}` |
|       - | 4117 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|       - | 4118 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|       - | 4119 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|       - | 4120 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|       - | 4121 | `		 * include-stack top for a call made at global scope. */` |
|       - | 4122 | `		{` |
|  631427 | 4123 | `			SyString *pFrameFile = pFile;` |
|  631427 | 4124 | `			if( pFrame->pParent->pUserData ){` |
|     443 | 4125 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|     443 | 4126 | `				if( pCaller->sFile.nByte > 0 ){` |
|     443 | 4127 | `					pFrameFile = &pCaller->sFile;` |
|     219 | 4128 | `				}` |
|     219 | 4129 | `			}` |
|  631427 | 4130 | `			if( pFrameFile ){` |
|  631427 | 4131 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|  631427 | 4132 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|  631427 | 4133 | `				ph7_value_reset_string_cursor(pValue);` |
|  315711 | 4134 | `			}` |
|       - | 4135 | `		}` |
|  631427 | 4136 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|  631427 | 4137 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|       - | 4138 | `		{` |
|  631427 | 4139 | `			const char *zDisp = 0;` |
|  631427 | 4140 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|  631427 | 4141 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|       - | 4142 | `		}` |
|  631427 | 4143 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|  631427 | 4144 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 4145 | `		{` |
|       - | 4146 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|       - | 4147 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|       - | 4148 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|       - | 4149 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|       - | 4150 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|       - | 4151 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|       - | 4152 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|  631427 | 4153 | `			SyString *pClsName = 0;` |
|  631427 | 4154 | `			const char *zType = "->";` |
|  631427 | 4155 | `			int bStatic = 0;` |
|  631427 | 4156 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       - | 4157 | `				/* php's separator says what the CALLEE is, not how the caller` |
|       - | 4158 | ``				 * happened to reach it: a static method is `::` even when the`` |
|       - | 4159 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|       - | 4160 | ``				 * instance method), which the pThis test reported as `->`.`` |
|       - | 4161 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|       - | 4162 | `				 * own flags are one cast away. */` |
|  500431 | 4163 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|  500431 | 4164 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|  500431 | 4165 | `				zType = bStatic ? "::" : "->";` |
|  381214 | 4166 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|     ! 0 | 4167 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|     ! 0 | 4168 | `			}` |
|  631427 | 4169 | `			if( pClsName ){` |
|  500431 | 4170 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|  500431 | 4171 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|  500431 | 4172 | `				ph7_value_reset_string_cursor(pValue);` |
|  500431 | 4173 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|  500431 | 4174 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|  500431 | 4175 | `				ph7_value_reset_string_cursor(pValue);` |
|  500426 | 4176 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis` |
|      24 | 4177 | `				 && !bStatic ){` |
|      19 | 4178 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      19 | 4179 | `					if( pObjVal ){` |
|      19 | 4180 | `						pFrame->pThis->iRef++;` |
|      19 | 4181 | `						pObjVal->x.pOther = pFrame->pThis;` |
|      19 | 4182 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      19 | 4183 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      19 | 4184 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       8 | 4185 | `					}` |
|       8 | 4186 | `				}` |
|  250213 | 4187 | `			}` |
|       - | 4188 | `		}` |
|  631427 | 4189 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|     117 | 4190 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|     117 | 4191 | `			if( pArg ){` |
|       - | 4192 | `				/* The arguments the caller actually PASSED, which is not the same` |
|       - | 4193 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|       - | 4194 | `				 * (shared with func_get_args()). */` |
|     117 | 4195 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg);` |
|     117 | 4196 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|     117 | 4197 | `				ph7_release_value(&(*pVm),pArg);` |
|      57 | 4198 | `			}` |
|      57 | 4199 | `		}` |
|  631427 | 4200 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|  631427 | 4201 | `		ph7_release_value(&(*pVm),pEntry);` |
|  631427 | 4202 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|       5 | 4203 | `	}` |
| 1456129 | 4204 | `	ph7_release_value(&(*pVm),pValue);` |
|  728067 | 4205 | `}` |
|       - | 4206 | `/*` |
|       - | 4207 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|       - | 4208 | ` *` |
|       - | 4209 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|       - | 4210 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|       - | 4211 | ` * calls parent::__construct still reports the right position. The embedded` |
|       - | 4212 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|       - | 4213 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|       - | 4214 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|       - | 4215 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|       - | 4216 | ` */` |
| 1576610 | 4217 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       5 | 4218 | `{` |
|       - | 4219 | `	static const char *azField[] = { "file", "line", "trace" };` |
|       - | 4220 | `	ph7_class *pThrowable;` |
|       - | 4221 | `	SyString *pFile;` |
|       - | 4222 | `	SyString *pSiteFile;` |
|       - | 4223 | `	sxu32 n;` |
| 1576615 | 4224 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|     ! 0 | 4225 | `		return;` |
|       - | 4226 | `	}` |
| 1576615 | 4227 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1576615 | 4228 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|  120587 | 4229 | `		return;` |
|       - | 4230 | `	}` |
| 1456033 | 4231 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1456033 | 4232 | `	pSiteFile = pFile;` |
|       - | 4233 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|       - | 4234 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|       - | 4235 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|       - | 4236 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|       - | 4237 | `	{` |
| 1456033 | 4238 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 1456033 | 4239 | `		if( pInner && pInner->pUserData ){` |
|  628025 | 4240 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|  628025 | 4241 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|  627933 | 4242 | `				pSiteFile = &pInnerFunc->sFile;` |
|  313964 | 4243 | `			}` |
|  314010 | 4244 | `		}` |
|       - | 4245 | `	}` |
| 5824117 | 4246 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|       - | 4247 | `		SyHashEntry *pEntry;` |
|       - | 4248 | `		VmClassAttr *pVmAttr;` |
|       - | 4249 | `		ph7_value *pAttrValue;` |
| 4368089 | 4250 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
| 4368089 | 4251 | `		if( pEntry == 0 ){` |
|     ! 0 | 4252 | `			continue;` |
|       - | 4253 | `		}` |
| 4368089 | 4254 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
| 4368089 | 4255 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 4368089 | 4256 | `		if( pAttrValue == 0 ){` |
|     ! 0 | 4257 | `			continue;` |
|       - | 4258 | `		}` |
| 4368089 | 4259 | `		if( n == 0 ){` |
| 1456033 | 4260 | `			if( pSiteFile ){` |
| 1456033 | 4261 | `				PH7_MemObjRelease(pAttrValue);` |
| 1456033 | 4262 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|  728019 | 4263 | `			}` |
| 3640075 | 4264 | `		}else if( n == 1 ){` |
|       - | 4265 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|       - | 4266 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|       - | 4267 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|       - | 4268 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|       - | 4269 | `			 * off everything the initializer calls — an autoloader, a nested` |
|       - | 4270 | `			 * constant's evaluation — which report their own lines in both engines.` |
|       - | 4271 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|       - | 4272 | `			 * PHL already matches.) */` |
| 1456034 | 4273 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|      40 | 4274 | `				? pVm->nLazyInitLine` |
| 1456029 | 4275 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
| 1456033 | 4276 | `			PH7_MemObjRelease(pAttrValue);` |
| 1456033 | 4277 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|  728019 | 4278 | `		}else{` |
|       - | 4279 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|       - | 4280 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|       - | 4281 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|       - | 4282 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
| 1456033 | 4283 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
| 1456033 | 4284 | `			if( pList == 0 ){` |
|     ! 0 | 4285 | `				continue;` |
|       - | 4286 | `			}` |
| 1456033 | 4287 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,0,pList);` |
|       - | 4288 | `			/* Building the trace reserves new memobjs, which may realloc` |
|       - | 4289 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|       - | 4290 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|       - | 4291 | `			 * AFTER the walk before releasing/storing into it. */` |
| 1456033 | 4292 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 1456033 | 4293 | `			if( pAttrValue ){` |
| 1456033 | 4294 | `				PH7_MemObjRelease(pAttrValue);` |
| 1456033 | 4295 | `				PH7_MemObjStore(pList,pAttrValue);` |
|  728014 | 4296 | `			}` |
| 1456033 | 4297 | `			ph7_release_value(&(*pVm),pList);` |
|       - | 4298 | `		}` |
| 2184047 | 4299 | `	}` |
|  788310 | 4300 | `}` |
|     362 | 4301 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|       3 | 4302 | `{` |
|     365 | 4303 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      33 | 4304 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      33 | 4305 | `		if( pInst && pInst->pClass ){` |
|      33 | 4306 | `			return pInst->pClass->sName.zString;` |
|       - | 4307 | `		}` |
|     ! 0 | 4308 | `	}` |
|     333 | 4309 | `	return ph7_type_name(pVal);` |
|     184 | 4310 | `}` |
|       - | 4311 | `/*` |
|       - | 4312 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|       - | 4313 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|       - | 4314 | ` * describes an operand it could not use as one ("on false", "false given").` |
|       - | 4315 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|       - | 4316 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|       - | 4317 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|       - | 4318 | ` * instead of replacing it.` |
|       - | 4319 | ` */` |
|     104 | 4320 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|       3 | 4321 | `{` |
|     107 | 4322 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|      19 | 4323 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 4324 | `	}` |
|      89 | 4325 | `	return VmArithTypeName(&(*pVal));` |
|      55 | 4326 | `}` |
|       - | 4327 | `/*` |
|       - | 4328 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|       - | 4329 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|       - | 4330 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|       - | 4331 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|       - | 4332 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|       - | 4333 | ` */` |
|      36 | 4334 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|       1 | 4335 | `{` |
|      37 | 4336 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|      37 | 4337 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      15 | 4338 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      15 | 4339 | `		if( pInst && pInst->pClass ){` |
|      15 | 4340 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|      15 | 4341 | `			return;` |
|       - | 4342 | `		}` |
|     ! 0 | 4343 | `	}` |
|      23 | 4344 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|      19 | 4345 | `}` |
|       - | 4346 | `/*` |
|       - | 4347 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|       - | 4348 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|       - | 4349 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|       - | 4350 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|       - | 4351 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|       - | 4352 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|       - | 4353 | ` */` |
|      40 | 4354 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|       1 | 4355 | `{` |
|      41 | 4356 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      41 | 4357 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      41 | 4358 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      41 | 4359 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      41 | 4360 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|       - | 4361 | `	sxu32 i;` |
|     109 | 4362 | `	for( i = 0 ; i < nMin ; ++i ){` |
|       - | 4363 | `		unsigned char c;` |
|      69 | 4364 | `		if( cOp == '\|' ){` |
|      25 | 4365 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      57 | 4366 | `		}else if( cOp == '^' ){` |
|      21 | 4367 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|      11 | 4368 | `		}else{` |
|      25 | 4369 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|       - | 4370 | `		}` |
|      69 | 4371 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      35 | 4372 | `	}` |
|      41 | 4373 | `	if( cOp == '\|' && nL != nR ){` |
|      11 | 4374 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|      11 | 4375 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|      11 | 4376 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       5 | 4377 | `	}` |
|      41 | 4378 | `}` |
|       - | 4379 | `/*` |
|       - | 4380 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|       - | 4381 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|       - | 4382 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|       - | 4383 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|       - | 4384 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|       - | 4385 | ` * null, which is what an undefined variable answers.` |
|       - | 4386 | ` */` |
|      24 | 4387 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|       1 | 4388 | `{` |
|      25 | 4389 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       9 | 4390 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       9 | 4391 | `		if( pInst && pInst->pClass ){` |
|       9 | 4392 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|       9 | 4393 | `			return;` |
|       - | 4394 | `		}` |
|     ! 0 | 4395 | `	}` |
|      17 | 4396 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       7 | 4397 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|       4 | 4398 | `			pVal->x.iVal ? "true" : "false");` |
|      15 | 4399 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|      11 | 4400 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|       6 | 4401 | `	}else{` |
|       3 | 4402 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|       - | 4403 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|       - | 4404 | `	}` |
|      13 | 4405 | `}` |
|       - | 4406 | `/*` |
|       - | 4407 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|       - | 4408 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|       - | 4409 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|       - | 4410 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|       - | 4411 | ` * computes with the prefix. Classify pVal against that contract.` |
|       - | 4412 | ` */` |
|   85790 | 4413 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|       5 | 4414 | `{` |
|       - | 4415 | `	ph7_value sInt;` |
|       - | 4416 | `	sxi32 rc;` |
|   85795 | 4417 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   85795 | 4418 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   85795 | 4419 | `	PH7_MemObjRelease(&sInt);` |
|   85795 | 4420 | `	return rc;` |
|       5 | 4421 | `}` |
|  168693 | 4422 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|       5 | 4423 | `{` |
|  168698 | 4424 | `	int bBadL = 0, bBadR = 0;` |
|       - | 4425 | `	int i;` |
|       - | 4426 | `	ph7_value *apOperand[2];` |
|  168698 | 4427 | `	apOperand[0] = pLeft;` |
|  168698 | 4428 | `	apOperand[1] = pRight;` |
|       - | 4429 | `	/* array + array is php's union operator, not arithmetic */` |
|  168693 | 4430 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   26427 | 4431 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|    5009 | 4432 | `		return SXRET_OK;` |
|       - | 4433 | `	}` |
|  491072 | 4434 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  327383 | 4435 | `		ph7_value *pVal = apOperand[i];` |
|  327383 | 4436 | `		int bBad = 0;` |
|  327383 | 4437 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      87 | 4438 | `			bBad = 1;` |
|  327340 | 4439 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     333 | 4440 | `			const char *zTail = 0;` |
|     333 | 4441 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     333 | 4442 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|       - | 4443 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|      49 | 4444 | `				bBad = 1;` |
|      25 | 4445 | `			}else{` |
|       - | 4446 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|       - | 4447 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|       - | 4448 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     313 | 4449 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|      30 | 4450 | `					zTail++;` |
|       2 | 4451 | `				}` |
|     285 | 4452 | `				if( zTail < zEnd ){` |
|      44 | 4453 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      21 | 4454 | `				}` |
|       - | 4455 | `			}` |
|     165 | 4456 | `		}` |
|  327383 | 4457 | `		if( bBad ){` |
|     135 | 4458 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|      67 | 4459 | `		}` |
|  164031 | 4460 | `	}` |
|  163694 | 4461 | `	if( bBadL \|\| bBadR ){` |
|       - | 4462 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|       - | 4463 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|       - | 4464 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     187 | 4465 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|      62 | 4466 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     125 | 4467 | `		return SXERR_INVALID;` |
|       - | 4468 | `	}` |
|  163570 | 4469 | `	return SXRET_OK;` |
|   84520 | 4470 | `}` |
|       - | 4471 | `/*` |
|       - | 4472 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|       - | 4473 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|       - | 4474 | ` * pass 0 for the engine errors that leave it at its default.` |
|       - | 4475 | ` */` |
|    8138 | 4476 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|       5 | 4477 | `{` |
|       - | 4478 | `	ph7_vm *pVm;` |
|       - | 4479 | `	ph7_class *pClass;` |
|       - | 4480 | `	ph7_class_instance *pThis;` |
|       - | 4481 | `	ph7_class_method *pCons;` |
|       - | 4482 | `	ph7_value sArg,sCode;` |
|       - | 4483 | `	ph7_value *apArg[2];` |
|       - | 4484 | `	SyBlob sMsg;` |
|       - | 4485 | `	SyString sMsgStr;` |
|       - | 4486 | `	VmFrame *pFrame;` |
|       - | 4487 | `	sxi32 rc;` |
|       - | 4488 |  |
|    8143 | 4489 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4490 | `		return PH7_ABORT;` |
|       - | 4491 | `	}` |
|    8143 | 4492 | `	pVm = pCtx->pVm;` |
|    8143 | 4493 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4494 | `		zClass = "Error";` |
|     ! 0 | 4495 | `	}` |
|       - | 4496 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|       - | 4497 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|       - | 4498 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|       - | 4499 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|       - | 4500 | `	 * reported error (VmHostFuncThrowRc). */` |
|    8143 | 4501 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    8143 | 4502 | `	if( pClass == 0 ){` |
|     ! 0 | 4503 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4504 | `			"Cannot throw internal exception, class '%s' is not available",` |
|     ! 0 | 4505 | `			zClass` |
|       - | 4506 | `			);` |
|     ! 0 | 4507 | `		return pCtx->nThrowRc;` |
|       - | 4508 | `	}` |
|    8143 | 4509 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    8143 | 4510 | `	if( pThis == 0 ){` |
|     ! 0 | 4511 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4512 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|       - | 4513 | `			);` |
|     ! 0 | 4514 | `		return pCtx->nThrowRc;` |
|       - | 4515 | `	}` |
|       - | 4516 |  |
|    8143 | 4517 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    8143 | 4518 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - | 4519 |  |
|    8143 | 4520 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    8143 | 4521 | `	if( pCons ){` |
|    8143 | 4522 | `		int nArg = 1;` |
|    8143 | 4523 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    8143 | 4524 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    8143 | 4525 | `		apArg[0] = &sArg;` |
|    8143 | 4526 | `		if( iCode != 0 ){` |
|      14 | 4527 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      14 | 4528 | `			apArg[1] = &sCode;` |
|      14 | 4529 | `			nArg = 2;` |
|       6 | 4530 | `		}` |
|    8143 | 4531 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    8143 | 4532 | `		if( iCode != 0 ){` |
|      14 | 4533 | `			PH7_MemObjRelease(&sCode);` |
|       6 | 4534 | `		}` |
|    8143 | 4535 | `		PH7_MemObjRelease(&sArg);` |
|    4069 | 4536 | `	}` |
|    8143 | 4537 | `	SyBlobRelease(&sMsg);` |
|       - | 4538 |  |
|    8143 | 4539 | `	pFrame = pVm->pFrame;` |
|    8143 | 4540 | `	if( pFrame ){` |
|    8143 | 4541 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    8143 | 4542 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    4069 | 4543 | `	}` |
|    8143 | 4544 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    8143 | 4545 | `	PH7_ClassInstanceUnref(pThis);` |
|    8143 | 4546 | `	if( rc == SXERR_ABORT ){` |
|     524 | 4547 | `		pCtx->nThrowRc = PH7_ABORT;` |
|     524 | 4548 | `		return PH7_ABORT;` |
|       - | 4549 | `	}` |
|       - | 4550 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|       - | 4551 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|       - | 4552 | `	 * argument-validation helper whose callers have no status channel — would` |
|       - | 4553 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|       - | 4554 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|       - | 4555 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|       - | 4556 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|       - | 4557 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|       - | 4558 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    7623 | 4559 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    7623 | 4560 | `	return PH7_EXCEPTION;` |
|    4074 | 4561 | `}` |
|    8126 | 4562 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|       5 | 4563 | `{` |
|       - | 4564 | `	va_list ap;` |
|       - | 4565 | `	sxi32 rc;` |
|    8131 | 4566 | `	va_start(ap,zFormat);` |
|    8131 | 4567 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    8131 | 4568 | `	va_end(ap);` |
|    8131 | 4569 | `	return rc;` |
|       5 | 4570 | `}` |
|       - | 4571 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      12 | 4572 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|       2 | 4573 | `{` |
|       - | 4574 | `	va_list ap;` |
|       - | 4575 | `	sxi32 rc;` |
|      14 | 4576 | `	va_start(ap,zFormat);` |
|      14 | 4577 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      14 | 4578 | `	va_end(ap);` |
|      14 | 4579 | `	return rc;` |
|       2 | 4580 | `}` |
|       - | 4581 | `/*` |
|       - | 4582 | ` * The status a host function's own throw should have returned. Consulted at the` |
|       - | 4583 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|       - | 4584 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|       - | 4585 | ` * and is the identity when the routine never threw or already reported it.` |
|       - | 4586 | ` *` |
|       - | 4587 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|       - | 4588 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|       - | 4589 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|       - | 4590 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|       - | 4591 | ` */` |
| 2864982 | 4592 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|       5 | 4593 | `{` |
| 2864982 | 4594 | `	if( pCtx->nThrowRc == 0` |
| 1436594 | 4595 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
| 2860887 | 4596 | `		return rc;` |
|       - | 4597 | `	}` |
|    4104 | 4598 | `	return pCtx->nThrowRc;` |
| 1433321 | 4599 | `}` |
|       - | 4600 | `/*` |
|       - | 4601 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|       - | 4602 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|       - | 4603 | ` */` |
|     ! 0 | 4604 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|     ! 0 | 4605 | `{` |
|       - | 4606 | `	ph7_vm *pVm;` |
|       - | 4607 | `	SyBlob sMsg;` |
|     ! 0 | 4608 | `	const char *zFuncName = 0;` |
|     ! 0 | 4609 | `	int nFuncLen = 0;` |
|       - | 4610 | `	va_list ap;` |
|       - | 4611 | `	sxi32 rc;` |
|       - | 4612 |  |
|     ! 0 | 4613 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4614 | `		return PH7_OK;` |
|       - | 4615 | `	}` |
|     ! 0 | 4616 | `	pVm = pCtx->pVm;` |
|     ! 0 | 4617 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4618 | `		zClass = "Error";` |
|     ! 0 | 4619 | `	}` |
|       - | 4620 |  |
|     ! 0 | 4621 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 4622 |  |
|     ! 0 | 4623 | `	va_start(ap,zFormat);` |
|     ! 0 | 4624 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     ! 0 | 4625 | `	va_end(ap);` |
|       - | 4626 |  |
|     ! 0 | 4627 | `	if( pCtx->pFunc ){` |
|     ! 0 | 4628 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|     ! 0 | 4629 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|     ! 0 | 4630 | `	}` |
|     ! 0 | 4631 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     ! 0 | 4632 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     ! 0 | 4633 | `	}` |
|     ! 0 | 4634 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|     ! 0 | 4635 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|     ! 0 | 4636 | `	SyBlobRelease(&sMsg);` |
|     ! 0 | 4637 | `	return rc;` |
|     ! 0 | 4638 | `}` |
|       - | 4639 | `/*` |
|       - | 4640 | ` * The following routine is invoked by the engine when an uncaught` |
|       - | 4641 | ` * exception is triggered.` |
|       - | 4642 | ` */` |
|     590 | 4643 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|       - | 4644 | `	ph7_vm *pVm, /* Target VM */` |
|       - | 4645 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4646 | `	)` |
|       4 | 4647 | `{` |
|       - | 4648 | `	ph7_value *apArg[2],sArg;` |
|     594 | 4649 | `	int nArg = 1;` |
|       - | 4650 | `	sxi32 rc;` |
|     594 | 4651 | `	if( pVm->nMuteThrow > 0 ){` |
|       - | 4652 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|       - | 4653 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|       - | 4654 | `		 * observable: no exception handler runs, no report is printed and the` |
|       - | 4655 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|       - | 4656 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|     ! 0 | 4657 | `		return SXERR_ABORT;` |
|       - | 4658 | `	}` |
|     594 | 4659 | `	if( pVm->nExceptDepth > 15 ){` |
|       - | 4660 | `		/* Nesting limit reached */` |
|     ! 0 | 4661 | `		return SXRET_OK;` |
|       - | 4662 | `	}` |
|       - | 4663 | `	/* Call any exception handler if available */` |
|     594 | 4664 | `	PH7_MemObjInit(pVm,&sArg);` |
|     594 | 4665 | `	if( pThis ){` |
|       - | 4666 | `		/* Load the exception instance */` |
|     594 | 4667 | `		sArg.x.pOther = pThis;` |
|     594 | 4668 | `		pThis->iRef++;` |
|     594 | 4669 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|     299 | 4670 | `	}else{` |
|     ! 0 | 4671 | `		nArg = 0;` |
|       - | 4672 | `	}` |
|     594 | 4673 | `	apArg[0] = &sArg;` |
|       - | 4674 | `	/* Call the exception handler if available */` |
|     594 | 4675 | `	pVm->nExceptDepth++;` |
|       - | 4676 | `	{` |
|       - | 4677 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|       - | 4678 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|       - | 4679 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|       - | 4680 | `		ph7_value sRunning;` |
|     594 | 4681 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     594 | 4682 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|     594 | 4683 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|     594 | 4684 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|     594 | 4685 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|     594 | 4686 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|     594 | 4687 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|     295 | 4688 | `		}` |
|     594 | 4689 | `		PH7_MemObjRelease(&sRunning);` |
|       - | 4690 | `	}` |
|     594 | 4691 | `	pVm->nExceptDepth--;` |
|     594 | 4692 | `	if( rc != SXRET_OK ){` |
|       - | 4693 | `		const char *zFuncName;` |
|       - | 4694 | `		int nFuncLen;` |
|     592 | 4695 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       - | 4696 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|     592 | 4697 | `		if( pThis ){` |
|       - | 4698 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|       - | 4699 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|       - | 4700 | `			 * renders byte-identically to the historical single-entry report. */` |
|     592 | 4701 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|     298 | 4702 | `		}else{` |
|       - | 4703 | `			/* No instance (internal report path) — default-class single entry. */` |
|     ! 0 | 4704 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|       - | 4705 | `		}` |
|       - | 4706 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|     592 | 4707 | `		rc = SXERR_ABORT;` |
|     294 | 4708 | `	}` |
|     594 | 4709 | `	PH7_MemObjRelease(&sArg);` |
|     594 | 4710 | `	return rc;` |
|     299 | 4711 | `}` |
|       - | 4712 | `/*` |
|       - | 4713 | ` * Throw a user exception.` |
|       - | 4714 | ` *` |
|       - | 4715 | ` * Exception dispatch follows this sequence:` |
|       - | 4716 | ` *` |
|       - | 4717 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|       - | 4718 | ` *    try/catch whose catch block matches the exception class.` |
|       - | 4719 | ` *` |
|       - | 4720 | ` * 2. If NO catch matches:` |
|       - | 4721 | ` *    a. Run finally (if present) for the current try block.` |
|       - | 4722 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|       - | 4723 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|       - | 4724 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|       - | 4725 | ` *       exception in pVm->pPendingException instead of reporting it` |
|       - | 4726 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|       - | 4727 | ` *    d. Otherwise, report as truly uncaught.` |
|       - | 4728 | ` *` |
|       - | 4729 | ` * 3. If a catch DOES match:` |
|       - | 4730 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|       - | 4731 | ` *       aException stack and resetting it. This prevents a re-throw` |
|       - | 4732 | ` *       inside the catch body from immediately propagating past our` |
|       - | 4733 | ` *       finally block.` |
|       - | 4734 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|       - | 4735 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|       - | 4736 | ` *       no handlers (they're hidden), so the exception is deferred` |
|       - | 4737 | ` *       in pPendingException (step 2c).` |
|       - | 4738 | ` *    c. Restore outer handlers from the saved copy.` |
|       - | 4739 | ` *    d. Run finally (if present).` |
|       - | 4740 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|       - | 4741 | ` *       that handlers are restored and finally has run.` |
|       - | 4742 | ` */` |
|       - | 4743 | `/*` |
|       - | 4744 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|       - | 4745 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|       - | 4746 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|       - | 4747 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|       - | 4748 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|       - | 4749 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|       - | 4750 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|       - | 4751 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|       - | 4752 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|       - | 4753 | ` */` |
|     136 | 4754 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|       5 | 4755 | `{` |
|     149 | 4756 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|      45 | 4757 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      45 | 4758 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|      45 | 4759 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|     ! 0 | 4760 | `			break; /* reached an outer exec's / legacy handler */` |
|       - | 4761 | `		}` |
|      45 | 4762 | `		(void)SySetPop(&pVm->aException);` |
|      45 | 4763 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|      45 | 4764 | `		if( pT->iHasFinally ){` |
|      37 | 4765 | `			*pPc = pT->iFinallyPc;` |
|      37 | 4766 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|      37 | 4767 | `			return 1;` |
|       - | 4768 | `		}` |
|       - | 4769 | `		/* No finally: tear the try's transparent frame down now. */` |
|      10 | 4770 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       6 | 4771 | `			VmLeaveFrame(&(*pVm));` |
|       2 | 4772 | `		}` |
|      10 | 4773 | `		VmExcRelease(&(*pVm),pT);` |
|       2 | 4774 | `	}` |
|     107 | 4775 | `	return 0;` |
|      73 | 4776 | `}` |
|       - | 4777 | `/*` |
|       - | 4778 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|       - | 4779 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|       - | 4780 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|       - | 4781 | ` *` |
|       - | 4782 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|       - | 4783 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|       - | 4784 | ` *    and redirect to the catch body (iHandlerPc).` |
|       - | 4785 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|       - | 4786 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|       - | 4787 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|       - | 4788 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|       - | 4789 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|       - | 4790 | ` */` |
|       - | 4791 | `/*` |
|       - | 4792 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|       - | 4793 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|       - | 4794 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|       - | 4795 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|       - | 4796 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|       - | 4797 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|       - | 4798 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|       - | 4799 | ` * case) is unchanged: no wrapper.` |
|       - | 4800 | ` */` |
|   20252 | 4801 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|       5 | 4802 | `{` |
|   20257 | 4803 | `	VmFrame *pWrap = 0;` |
|       - | 4804 | `	VmFrame *pThrowSite;` |
|       - | 4805 | `	sxi32 rc;` |
|   20257 | 4806 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|   20153 | 4807 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4808 | `	}` |
|     107 | 4809 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|       - | 4810 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|     ! 0 | 4811 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4812 | `	}` |
|     107 | 4813 | `	pThrowSite = pWrap->pParent;` |
|     107 | 4814 | `	pWrap->pParent = pOwner;` |
|     107 | 4815 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|     107 | 4816 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4817 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|       - | 4818 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|       - | 4819 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|       - | 4820 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|     107 | 4821 | `	if( pVm->pFrame == pWrap ){` |
|     107 | 4822 | `		VmLeaveFrame(&(*pVm));` |
|      52 | 4823 | `	}` |
|     107 | 4824 | `	pVm->pFrame = pThrowSite;` |
|     107 | 4825 | `	return rc;` |
|   10131 | 4826 | `}` |
|       - | 4827 | `/*` |
|       - | 4828 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|       - | 4829 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|       - | 4830 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|       - | 4831 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|       - | 4832 | ` */` |
|       - | 4833 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      92 | 4834 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|       - | 4835 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|       5 | 4836 | `{` |
|      97 | 4837 | `	if( pCatch ){` |
|      81 | 4838 | `		pException->iInCatch = 1;` |
|      81 | 4839 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|      81 | 4840 | `		if( pThis ){ pThis->iRef++; }` |
|      81 | 4841 | `		pException->pInflight = pThis;` |
|      81 | 4842 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      81 | 4843 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|      81 | 4844 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      81 | 4845 | `		return SXRET_OK;` |
|       - | 4846 | `	}` |
|      20 | 4847 | `	if( pException->iHasFinally ){` |
|       - | 4848 | `		VmFinallyAction sAct;` |
|      15 | 4849 | `		SyZero(&sAct,sizeof(sAct));` |
|      15 | 4850 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      15 | 4851 | `		if( pThis ){ pThis->iRef++; }` |
|      15 | 4852 | `		sAct.pExc = pThis;` |
|      15 | 4853 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      15 | 4854 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      15 | 4855 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      15 | 4856 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      15 | 4857 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      15 | 4858 | `		return SXRET_OK;` |
|       - | 4859 | `	}` |
|       - | 4860 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|       - | 4861 | `	 * flat native stack instead of mutual recursion. */` |
|       6 | 4862 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     ! 0 | 4863 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 | 4864 | `	}` |
|       6 | 4865 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       6 | 4866 | `	return VM_THROW_KEEP_UNWINDING;` |
|      51 | 4867 | `}` |
| 1455954 | 4868 | `PH7_PRIVATE sxi32 VmThrowException(` |
|       - | 4869 | `	ph7_vm *pVm,              /* Target VM */` |
|       - | 4870 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4871 | `	)` |
|       5 | 4872 | `{` |
|       - | 4873 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|       - | 4874 | `	ph7_exception **apException;` |
|  727977 | 4875 | `	ph7_exception *pException;` |
|   50102 | 4876 | `Rethrow:` |
|       - | 4877 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|       - | 4878 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|       - | 4879 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|       - | 4880 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|       - | 4881 | `	 * so the throw path must be too). */` |
|       - | 4882 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|       - | 4883 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|       - | 4884 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
| 1556163 | 4885 | `	VmCoalesceDisarm(pVm);` |
|       - | 4886 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|       - | 4887 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|       - | 4888 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|       - | 4889 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|       - | 4890 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|       - | 4891 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|       - | 4892 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|       - | 4893 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
| 1556158 | 4894 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|      25 | 4895 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|      19 | 4896 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|       8 | 4897 | `	}` |
|       - | 4898 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|       - | 4899 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|       - | 4900 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|       - | 4901 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|       - | 4902 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|       - | 4903 | `	 * that owns the pending return, so it must leave that return intact. */` |
|       - | 4904 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|       - | 4905 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|       - | 4906 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|       - | 4907 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
| 1556163 | 4908 | `	pVm->pResumeFrame = 0;` |
|       - | 4909 | `	/* Point to the stack of loaded exceptions */` |
| 1556163 | 4910 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
| 1556163 | 4911 | `	pException = 0;` |
| 1556163 | 4912 | `	pCatch = 0;` |
| 1556163 | 4913 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 4914 | `		ph7_exception_block *aCatch;` |
|       - | 4915 | `		ph7_class *pClass;` |
|       - | 4916 | `		SyString *aNames;` |
|       - | 4917 | `		sxu32 nNames;` |
|       - | 4918 | `		int matched;` |
|       - | 4919 | `		sxu32 j,k;` |
|       - | 4920 | `		/* Locate the appropriate block to execute */` |
| 1455453 | 4921 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
| 1455453 | 4922 | `		(void)SySetPop(&pVm->aException);` |
| 1455453 | 4923 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       - | 4924 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|       - | 4925 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|       - | 4926 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
| 1455471 | 4927 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       - | 4928 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
| 1435283 | 4929 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
| 1435283 | 4930 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
| 1435283 | 4931 | `			matched = 0;` |
| 1435327 | 4932 | `			for( k = 0 ; k < nNames ; ++k ){` |
|       - | 4933 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|       - | 4934 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|       - | 4935 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
| 1435309 | 4936 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
| 1435309 | 4937 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 4938 | `					/* No such class, or trait — cannot match */` |
|     ! 0 | 4939 | `					continue;` |
|       - | 4940 | `				}` |
| 1435309 | 4941 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
| 1435265 | 4942 | `					matched = 1;` |
| 1435265 | 4943 | `					break;` |
|       - | 4944 | `				}` |
|      26 | 4945 | `			}` |
| 1435283 | 4946 | `			if( matched ){` |
|       - | 4947 | `				/* Catch block found,break immediately */` |
| 1435265 | 4948 | `				pCatch = &aCatch[j];` |
| 1435265 | 4949 | `				break;` |
|       - | 4950 | `			}` |
|      12 | 4951 | `		}` |
|  727724 | 4952 | `	}` |
|       - | 4953 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|       - | 4954 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|       - | 4955 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|       - | 4956 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|       - | 4957 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|       - | 4958 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|       - | 4959 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
| 1556163 | 4960 | `	if( pException ){` |
| 1455453 | 4961 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|  727724 | 4962 | `	}` |
|       - | 4963 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|       - | 4964 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|       - | 4965 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
| 1556163 | 4966 | `	if( pException && pException->iInlined ){` |
|      97 | 4967 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      97 | 4968 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|       - | 4969 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|       6 | 4970 | `			goto Rethrow;` |
|       - | 4971 | `		}` |
|      93 | 4972 | `		return rcInline;` |
|       - | 4973 | `	}` |
|       - | 4974 | `	/* Execute the cached block if available */` |
| 1556071 | 4975 | `	if( pCatch == 0 ){` |
|       - | 4976 | `		sxi32 rc;` |
|       - | 4977 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|  120887 | 4978 | `		if( pException && pException->iHasFinally ){` |
|   20173 | 4979 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|   20173 | 4980 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|   20173 | 4981 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|   20173 | 4982 | `			pException->iFinallyDone = 1;` |
|       - | 4983 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|       - | 4984 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|   20173 | 4985 | `			pVm->pInflightException = pThis;` |
|   20173 | 4986 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 4987 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|       - | 4988 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|   20173 | 4989 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|   20173 | 4990 | `			pVm->pInflightException = pSaveInflight;` |
|   20173 | 4991 | `			pVm->nInflightExcBase = nSaveBase;` |
|   20173 | 4992 | `			if( rc == SXERR_ABORT ){` |
|       3 | 4993 | `				VmExcRelease(&(*pVm),pException);` |
|       3 | 4994 | `				return SXERR_ABORT;` |
|       - | 4995 | `			}` |
|       - | 4996 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|       - | 4997 | `			 * semantics). The finally stored it on the body frame it returns from` |
|       - | 4998 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|       - | 4999 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|       - | 5000 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|       - | 5001 | `			 * takes the value instead of unwinding) and resume in place.` |
|       - | 5002 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|       - | 5003 | `			 * the same transport an in-place catch uses — and unwind as an` |
|       - | 5004 | `			 * exception; the owner's activation consumes the resume` |
|       - | 5005 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|       - | 5006 | `			 * its bHasRet tail materializes the return. */` |
|       - | 5007 | `			{` |
|   20171 | 5008 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   20171 | 5009 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|   20171 | 5010 | `				if( pOwnerFrame->bHasRet ){` |
|   20029 | 5011 | `					if( pOwnerFrame == pThrowFrame ){` |
|   20026 | 5012 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|       - | 5013 | `						/* Record the landing pad like the cross-frame case below.` |
|       - | 5014 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|       - | 5015 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|       - | 5016 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|       - | 5017 | `						 * its router unwound as an exception and the Unwind discard` |
|       - | 5018 | ``						 * dropped the parked return — `function f(){ try {`` |
|       - | 5019 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|       - | 5020 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|       - | 5021 | `						 * try frame down and its bHasRet tail materializes the` |
|       - | 5022 | `						 * return, same as the in-place-catch landing. */` |
|   20026 | 5023 | `						pVm->pResumeFrame = pOwnerFrame;` |
|   20026 | 5024 | `						pVm->iResumePc = pException->iLandingPc;` |
|   20026 | 5025 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|   20026 | 5026 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|   20026 | 5027 | `						VmExcRelease(&(*pVm),pException);` |
|   20026 | 5028 | `						return SXRET_OK;` |
|       - | 5029 | `					}` |
|       3 | 5030 | `					pVm->pResumeFrame = pOwnerFrame;` |
|       3 | 5031 | `					pVm->iResumePc = pException->iLandingPc;` |
|       3 | 5032 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|       3 | 5033 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|       3 | 5034 | `					VmExcRelease(&(*pVm),pException);` |
|       3 | 5035 | `					return PH7_EXCEPTION;` |
|       - | 5036 | `				}` |
|       - | 5037 | `			}` |
|       - | 5038 | `			/* The finally threw an exception that superseded pThis — it either` |
|       - | 5039 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|       - | 5040 | `			 * (which consumed an entry from the exception stack). Either way the` |
|       - | 5041 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|       - | 5042 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|     144 | 5043 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      17 | 5044 | `				VmExcRelease(&(*pVm),pException);` |
|      17 | 5045 | `				return PH7_EXCEPTION;` |
|       - | 5046 | `			}` |
|      63 | 5047 | `		}` |
|       - | 5048 | `		/* Check if there is an outer exception handler on the stack */` |
|  100845 | 5049 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 5050 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|       - | 5051 | `			 * iteration per unwound level instead of one native frame. */` |
|     133 | 5052 | `			VmExcRelease(&(*pVm),pException);` |
|     133 | 5053 | `			goto Rethrow;` |
|       - | 5054 | `		}` |
|  100715 | 5055 | `		if( pVm->nMuteThrow > 0 ){` |
|       - | 5056 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|       - | 5057 | `			 * default at class mount, which php would not have evaluated yet).` |
|       - | 5058 | `			 * Nothing outside the initializer may observe this throw: no` |
|       - | 5059 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|       - | 5060 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|       - | 5061 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|       - | 5062 | `			 * the mount path rolls the whole attempt back. */` |
|      50 | 5063 | `			VmExcRelease(&(*pVm),pException);` |
|      50 | 5064 | `			return SXERR_ABORT;` |
|       - | 5065 | `		}` |
|       - | 5066 | `		/* No outer handler. If the handlers were temporarily hidden` |
|       - | 5067 | `		 * (catch body re-throw with finally pending), defer the` |
|       - | 5068 | `		 * exception instead of reporting it uncaught.` |
|       - | 5069 | `		 */` |
|  100669 | 5070 | `		if( pVm->pPendingException == 0 && pThis ){` |
|       - | 5071 | `			/* Check if we are inside a catch execution with hidden handlers` |
|       - | 5072 | `			 * by looking for a catch frame on the stack.` |
|       - | 5073 | `			 */` |
|  100669 | 5074 | `			VmFrame *pF = pVm->pFrame;` |
|  100669 | 5075 | `			int inCatch = 0;` |
|  101305 | 5076 | `			while( pF ){` |
|  100715 | 5077 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|  100078 | 5078 | `					inCatch = 1;` |
|  100078 | 5079 | `					break;` |
|       - | 5080 | `				}` |
|     640 | 5081 | `				pF = pF->pParent;` |
|       4 | 5082 | `			}` |
|  100669 | 5083 | `			if( inCatch ){` |
|       - | 5084 | `				/* Defer — will be re-thrown after finally runs */` |
|  100078 | 5085 | `				pThis->iRef++;` |
|  100078 | 5086 | `				pVm->pPendingException = pThis;` |
|  100078 | 5087 | `				VmExcRelease(&(*pVm),pException);` |
|  100078 | 5088 | `				return SXRET_OK;` |
|       - | 5089 | `			}` |
|     295 | 5090 | `		}` |
|       - | 5091 | `		/* Truly uncaught */` |
|     594 | 5092 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|     594 | 5093 | `		if( rc == SXRET_OK && pException ){` |
|     ! 0 | 5094 | `			VmFrame *pFrame = pVm->pFrame;` |
|     ! 0 | 5095 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|     ! 0 | 5096 | `			if( pException->pFrame == pFrame ){` |
|     ! 0 | 5097 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|     ! 0 | 5098 | `			}` |
|     ! 0 | 5099 | `		}` |
|     594 | 5100 | `		VmExcRelease(&(*pVm),pException);` |
|     594 | 5101 | `		return rc;` |
|     ! 0 | 5102 | `	}else{` |
| 1435189 | 5103 | `		VmFrame *pFrame = pVm->pFrame;` |
| 1435189 | 5104 | `		ph7_exception **apSaved = 0;` |
|       - | 5105 | `		sxu32 nSavedCount;` |
|       - | 5106 | `		sxi32 rc;` |
|       - | 5107 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|       - | 5108 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|       - | 5109 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|       - | 5110 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|       - | 5111 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
| 1435189 | 5112 | `		VmFrame *pCatchBody = pException->pFrame;` |
| 1435189 | 5113 | `		sxu32 iCatchPc = pException->iLandingPc;` |
| 1435189 | 5114 | `		void *pCatchInstr = pException->pOwnerInstr;` |
| 1435189 | 5115 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
| 1435189 | 5116 | `		if( pException->pFrame == pFrame ){` |
|  827583 | 5117 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|  413789 | 5118 | `		}` |
|       - | 5119 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|       - | 5120 | `		 * body re-throws, the exception does not immediately propagate past` |
|       - | 5121 | `		 * our finally block. We save the stack contents and restore after.` |
|       - | 5122 | `		 */` |
| 1435189 | 5123 | `		nSavedCount = SySetUsed(&pVm->aException);` |
| 1435189 | 5124 | `		if( nSavedCount > 0 ){` |
|  150275 | 5125 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   50090 | 5126 | `				nSavedCount * sizeof(ph7_exception *));` |
|  100185 | 5127 | `			if( apSaved ){` |
|  150275 | 5128 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|   50090 | 5129 | `					nSavedCount * sizeof(ph7_exception *));` |
|  100185 | 5130 | `				SySetReset(&pVm->aException);` |
|   50090 | 5131 | `			}` |
|   50090 | 5132 | `		}` |
|       - | 5133 | `		/* Create the catch frame (made transparent below) */` |
| 1435189 | 5134 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
| 1435189 | 5135 | `		if( rc == SXRET_OK ){` |
|       - | 5136 | `			ph7_value *pObj;` |
|       - | 5137 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|       - | 5138 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|       - | 5139 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|       - | 5140 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|       - | 5141 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|       - | 5142 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|       - | 5143 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|       - | 5144 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|       - | 5145 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
| 1435189 | 5146 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|       - | 5147 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|       - | 5148 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|       - | 5149 | `			 * against the live current scope rather than a freed frame. */` |
| 1435189 | 5150 | `			if( pCatchBody ){` |
| 1435189 | 5151 | `				pFrame->pParent = pCatchBody;` |
|  717592 | 5152 | `			}` |
|       - | 5153 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|       - | 5154 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|       - | 5155 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|       - | 5156 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|       - | 5157 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|       - | 5158 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|       - | 5159 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|       - | 5160 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
| 1435189 | 5161 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|       - | 5162 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
| 2152781 | 5163 | `			pObj = (pCatch->sThis.nByte > 0)` |
| 1435182 | 5164 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
| 1435189 | 5165 | `			if( pObj ){` |
|       - | 5166 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|       - | 5167 | `				 * so it may already hold a value from a prior catch or assignment.` |
|       - | 5168 | `				 * Pin the new instance, then release the slot's prior contents` |
|       - | 5169 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|       - | 5170 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|       - | 5171 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
| 1435185 | 5172 | `				pThis->iRef++;` |
| 1435185 | 5173 | `				PH7_MemObjRelease(pObj);` |
| 1435185 | 5174 | `				pObj->x.pOther = pThis;` |
| 1435185 | 5175 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|  717590 | 5176 | `			}` |
|       - | 5177 | `			/* Execute the catch block */` |
| 1435189 | 5178 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|       - | 5179 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|       - | 5180 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|       - | 5181 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|       - | 5182 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|       - | 5183 | `			 * unbalanced — never pop somebody else's frame. */` |
| 1435189 | 5184 | `			if( pVm->pFrame == pFrame ){` |
| 1435189 | 5185 | `				VmLeaveFrame(&(*pVm));` |
|  717592 | 5186 | `			}` |
| 1435189 | 5187 | `			pVm->pFrame = pThrowSite;` |
|  717592 | 5188 | `		}` |
|       - | 5189 | `		/* Restore the outer exception handlers */` |
| 1435189 | 5190 | `		if( apSaved ){` |
|       - | 5191 | `			sxu32 k;` |
|       - | 5192 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|       - | 5193 | `			 * the catch body) are normally already consumed; on an abnormal` |
|       - | 5194 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|       - | 5195 | `			 * linger — release those activations before discarding the set. */` |
|  100185 | 5196 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|  100185 | 5197 | `			SySetReset(&pVm->aException);` |
|  201017 | 5198 | `			for(k = 0; k < nSavedCount; k++){` |
|  100837 | 5199 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|   50421 | 5200 | `			}` |
|  100185 | 5201 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|   50090 | 5202 | `		}` |
|       - | 5203 | `		/* Execute the finally block after catch */` |
| 1435189 | 5204 | `		if( pException->iHasFinally ){` |
|       - | 5205 | `			sxi32 rcf;` |
|       - | 5206 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|       - | 5207 | `			 * from, its pending-return write generation (set if the catch above` |
|       - | 5208 | `			 * returned), and the exception-stack depth. After the finally we use` |
|       - | 5209 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|       - | 5210 | `			 * catch-return. */` |
|       - | 5211 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|       - | 5212 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|       - | 5213 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|       - | 5214 | `			 * supersede decision belong to the owner, not the thrower. */` |
|      89 | 5215 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|      89 | 5216 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|      89 | 5217 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|       - | 5218 | `			/* The exception in flight while this finally runs is the catch body's` |
|       - | 5219 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|       - | 5220 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|       - | 5221 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|       - | 5222 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|      89 | 5223 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|      89 | 5224 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|      89 | 5225 | `			pException->iFinallyDone = 1;` |
|      89 | 5226 | `			pVm->pInflightException = pVm->pPendingException;` |
|      89 | 5227 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 5228 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|      89 | 5229 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|      89 | 5230 | `			pVm->pInflightException = pSaveInflight;` |
|      89 | 5231 | `			pVm->nInflightExcBase = nSaveBase;` |
|      89 | 5232 | `			if( rcf == SXERR_ABORT ){` |
|     ! 0 | 5233 | `				VmExcRelease(&(*pVm),pException);` |
|     ! 0 | 5234 | `				return SXERR_ABORT;` |
|       - | 5235 | `			}` |
|       - | 5236 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|       - | 5237 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|       - | 5238 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|       - | 5239 | `			 * either case that exception supersedes this try's catch-return — but` |
|       - | 5240 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|       - | 5241 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|       - | 5242 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|      89 | 5243 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      20 | 5244 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|      13 | 5245 | `					VmClearFramePending(pBody);` |
|       5 | 5246 | `				}` |
|       - | 5247 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|       - | 5248 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|       - | 5249 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|       - | 5250 | `				 * legitimately have re-armed it during the finally. */` |
|      20 | 5251 | `				pBody->nCatchJmpPc = 0;` |
|       8 | 5252 | `			}` |
|      89 | 5253 | `			if( rcf == PH7_EXCEPTION ){` |
|       - | 5254 | `				/* The finally's exception propagated past this try; drop any deferred` |
|       - | 5255 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|       - | 5256 | `				 * reaches the frame that caught the finally's throw. */` |
|      20 | 5257 | `				if( pVm->pPendingException ){` |
|     ! 0 | 5258 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|     ! 0 | 5259 | `					pVm->pPendingException = 0;` |
|     ! 0 | 5260 | `				}` |
|      20 | 5261 | `				VmExcRelease(&(*pVm),pException);` |
|      20 | 5262 | `				return PH7_EXCEPTION;` |
|       - | 5263 | `			}` |
|      34 | 5264 | `		}` |
| 1435173 | 5265 | `		if( rc == SXERR_ABORT ){` |
|       5 | 5266 | `			VmExcRelease(&(*pVm),pException);` |
|       5 | 5267 | `			return SXERR_ABORT;` |
|       - | 5268 | `		}` |
|       - | 5269 | `		/* If the catch body re-threw, the exception was deferred in` |
|       - | 5270 | `		 * pPendingException (because outer handlers were hidden).` |
|       - | 5271 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|       - | 5272 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|       - | 5273 | `		 * the catch frame having been left above), which swallows the in-flight` |
|       - | 5274 | `		 * exception (PHP semantics).` |
|       - | 5275 | `		 */` |
| 1435169 | 5276 | `		if( pVm->pPendingException ){` |
|       - | 5277 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|  100078 | 5278 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|  100078 | 5279 | `			if( !pOwner->bHasRet ){` |
|  100074 | 5280 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|       - | 5281 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|       - | 5282 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|       - | 5283 | `				 * than leave it armed for an unrelated later landing. */` |
|  100074 | 5284 | `				pOwner->nCatchJmpPc = 0;` |
|  100074 | 5285 | `				pVm->pPendingException = 0;` |
|  100074 | 5286 | `				VmExcRelease(&(*pVm),pException);` |
|       - | 5287 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|  100074 | 5288 | `				pThis = pReThrow;` |
|  100074 | 5289 | `				goto Rethrow;` |
|       - | 5290 | `			}` |
|       - | 5291 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|       6 | 5292 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|       6 | 5293 | `			pVm->pPendingException = 0;` |
|       2 | 5294 | `		}` |
|       - | 5295 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|       - | 5296 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|       - | 5297 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|       - | 5298 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
| 1335099 | 5299 | `		pVm->pResumeFrame = pCatchBody;` |
| 1335099 | 5300 | `		pVm->iResumePc = iCatchPc;` |
| 1335099 | 5301 | `		pVm->pResumeInstr = pCatchInstr;` |
| 1335099 | 5302 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|       - | 5303 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|       - | 5304 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|       - | 5305 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
| 1335099 | 5306 | `		VmExcRelease(&(*pVm),pException);` |
|       - | 5307 | `	}` |
| 1335099 | 5308 | `	return SXRET_OK;` |
|  727982 | 5309 | `}` |
|       - | 5310 |  |
