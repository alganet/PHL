# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2685/2974 lines (90.28%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * Section:` |
|        - |    9 | ` *    Error, diagnostics and type-enforcement machinery: PH7_VmThrowError` |
|        - |   10 | ` *    and the error-handler invocation path, enum materialization and` |
|        - |   11 | ` *    on-demand class constants, scalar/union/property/constant/return` |
|        - |   12 | ` *    type enforcement, the TypeError/ArgumentCountError throwers,` |
|        - |   13 | ` *    uncaught-exception rendering, VmBuildBacktrace, and the exception` |
|        - |   14 | ` *    core VmUncaughtException/VmThrowException.` |
|        - |   15 | ` * Status:` |
|        - |   16 | ` *    Stable.` |
|        - |   17 | ` */` |
|        - |   18 | `/*` |
|        - |   19 | ` * Remember a diagnostic for error_get_last(). php records the last error that reached` |
|        - |   20 | ` * DEFAULT processing: one hidden by '@' or by error_reporting() still counts, but one a` |
|        - |   21 | ` * user handler claimed (by returning true) does not -- so this is called only on the` |
|        - |   22 | ` * default-processing path.` |
|        - |   23 | ` */` |
|    27956 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|        5 |   25 | `{` |
|    27961 |   26 | `	pVm->nLastErrType = iErr;` |
|    27961 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|    27961 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    27961 |   29 | `	if( zMsg && nMsg > 0 ){` |
|    27961 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|    13978 |   31 | `	}` |
|    27961 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    27961 |   33 | `	if( pFile ){` |
|    27961 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|    13978 |   35 | `	}` |
|    27961 |   36 | `}` |
|        - |   37 | `/*` |
|        - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|        - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|        - |   40 | ` * diagnostic is never silently swallowed.` |
|        - |   41 | ` */` |
|      862 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|        4 |   43 | `{` |
|      866 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|        4 |   45 | `}` |
|        - |   46 | `/*` |
|        - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|        - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|        - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|        - |   50 | ` * headers_sent()/output accounting).` |
|        - |   51 | ` */` |
|      888 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|        4 |   53 | `{` |
|        - |   54 | `	sxi32 rc;` |
|        - |   55 | `	/* Append a new line */` |
|        - |   56 | `#ifdef __WINNT__` |
|        4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |   58 | `#else` |
|      888 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |   60 | `#endif` |
|        - |   61 | `	/* Invoke the output consumer callback */` |
|      892 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      892 |   63 | `	if( bTrack ){` |
|       30 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|       13 |   65 | `	}` |
|      892 |   66 | `	return rc;` |
|        4 |   67 | `}` |
|        - |   68 | `/*` |
|        - |   69 | ` * Route an already-formatted diagnostic blob (the uncaught-exception path builds` |
|        - |   70 | `` * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream`` |
|        - |   71 | ` * when log_errors is on, else to the program-output stream when display_errors` |
|        - |   72 | ` * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).` |
|        - |   73 | ` */` |
|      594 |   74 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |   75 | `{` |
|      598 |   76 | `	if( pVm->bLogErrors ){` |
|      598 |   77 | `		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);` |
|        - |   78 | `	}` |
|      ! 0 |   79 | `	if( pVm->bDisplayErrors ){` |
|      ! 0 |   80 | `		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);` |
|        - |   81 | `	}` |
|      ! 0 |   82 | `	return SXRET_OK;` |
|      301 |   83 | `}` |
|        - |   84 | `/*` |
|        - |   85 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |   86 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |   87 | ` * information.` |
|        - |   88 | ` */` |
|    31817 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |   90 | `{` |
|        - |   91 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|        - |   92 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|        - |   93 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|        - |   94 | `	 * straight through to the engine's own reporting, which is what returning` |
|        - |   95 | `	 * TRUE below means. */` |
|    31817 |   96 | `	if( ph7_value_is_callable(&pVm->sErrCB)` |
|    17854 |   97 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
|        - |   98 | `		ph7_value apArg[4];` |
|        - |   99 | `		ph7_value *apArgPtr[4];` |
|        - |  100 | `		ph7_value sResult;` |
|        - |  101 | `		ph7_value sRunning;` |
|        - |  102 | `		SyString sErr;` |
|        - |  103 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|        - |  104 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|        - |  105 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|        - |  106 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|        - |  107 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|     3870 |  108 | `		if( iErr == PH7_CTX_NOTICE ){` |
|      387 |  109 | `			iErr = 8; /* E_NOTICE */` |
|      191 |  110 | `		}` |
|        - |  111 | `		/* Prepare arguments */` |
|     3870 |  112 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  113 | `			/* use explicit message length to avoid reading past buffer */` |
|     3870 |  114 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     3870 |  115 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     3870 |  116 | `		if( pFile ){` |
|     3870 |  117 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     3870 |  118 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     1933 |  119 | `		}else{` |
|      ! 0 |  120 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  121 | `		}` |
|     3870 |  122 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     3870 |  123 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  124 | `		/* Set up pointer array */` |
|     3870 |  125 | `		apArgPtr[0] = &apArg[0];` |
|     3870 |  126 | `		apArgPtr[1] = &apArg[1];` |
|     3870 |  127 | `		apArgPtr[2] = &apArg[2];` |
|     3870 |  128 | `		apArgPtr[3] = &apArg[3];` |
|        - |  129 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|        - |  130 | `		 * handler itself raises reaches the engine's reporting instead of` |
|        - |  131 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|        - |  132 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|        - |  133 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|        - |  134 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|        - |  135 | `		 * the handler installed itself STAYS. */` |
|     3870 |  136 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     3870 |  137 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|     3870 |  138 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|     3870 |  139 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|        - |  140 | `		/* Call the handler */` |
|        - |  141 | `		{` |
|     3870 |  142 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|     3870 |  143 | `			if( !ph7_value_is_callable(&pVm->sErrCB) ){` |
|     3868 |  144 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     1927 |  145 | `			}` |
|     3870 |  146 | `			PH7_MemObjRelease(&sRunning);` |
|     3870 |  147 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|        - |  148 | `				/* The handler threw (or aborted) instead of returning: php never` |
|        - |  149 | `				 * reports the original diagnostic then — the exception supersedes` |
|        - |  150 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|        - |  151 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|        - |  152 | `				 * user's catch already ran. */` |
|        5 |  153 | `				PH7_MemObjRelease(&apArg[0]);` |
|        5 |  154 | `				PH7_MemObjRelease(&apArg[1]);` |
|        5 |  155 | `				PH7_MemObjRelease(&apArg[2]);` |
|        5 |  156 | `				PH7_MemObjRelease(&apArg[3]);` |
|        5 |  157 | `				PH7_MemObjRelease(&sResult);` |
|        5 |  158 | `				return FALSE;` |
|        - |  159 | `			}` |
|        - |  160 | `		}` |
|        - |  161 | `		/* Check return value */` |
|     3866 |  162 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  163 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  164 | `		}` |
|        - |  165 | `		/* Release */` |
|     3866 |  166 | `		PH7_MemObjRelease(&apArg[0]);` |
|     3866 |  167 | `		PH7_MemObjRelease(&apArg[1]);` |
|     3866 |  168 | `		PH7_MemObjRelease(&apArg[2]);` |
|     3866 |  169 | `		PH7_MemObjRelease(&apArg[3]);` |
|     3866 |  170 | `		PH7_MemObjRelease(&sResult);` |
|        - |  171 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  172 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     3866 |  173 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  174 | `	}` |
|        - |  175 | `	/* No handler, always call error handler */` |
|    27957 |  176 | `	return TRUE;` |
|    15909 |  177 | `}` |
|        - |  178 | `/*` |
|        - |  179 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|        - |  180 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|        - |  181 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|        - |  182 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|        - |  183 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|        - |  184 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|        - |  185 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|        - |  186 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|        - |  187 | ` * only picks the DISPLAY label.` |
|        - |  188 | ` */` |
|        - |  189 | `/*` |
|        - |  190 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|        - |  191 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|        - |  192 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|        - |  193 | ` * every deprecation.` |
|        - |  194 | ` */` |
|    27399 |  195 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|        5 |  196 | `{` |
|    27404 |  197 | `	switch( iErr ){` |
|    13287 |  198 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|    26570 |  199 | `		return 2;` |
|       49 |  200 | `	case 512  /* E_USER_WARNING */:` |
|      101 |  201 | `		return 512;` |
|      231 |  202 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|        - |  203 | `	case 8    /* E_NOTICE */:` |
|      467 |  204 | `		return 8;` |
|       35 |  205 | `	case 1024 /* E_USER_NOTICE */:` |
|       74 |  206 | `		return 1024;` |
|       65 |  207 | `	case 8192 /* E_DEPRECATED */:` |
|      135 |  208 | `		return 8192;` |
|       23 |  209 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       48 |  210 | `		return 16384;` |
|      ! 0 |  211 | `	case 256  /* E_USER_ERROR */:` |
|      ! 0 |  212 | `		return 256;` |
|       14 |  213 | `	default:` |
|       32 |  214 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|        - |  215 | `	}` |
|    13700 |  216 | `}` |
|    27956 |  217 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|        5 |  218 | `{` |
|    27961 |  219 | `	if( !pVm->bErrReport ){` |
|     4441 |  220 | `		return 0;` |
|        - |  221 | `	}` |
|    23523 |  222 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|    13983 |  223 | `}` |
|      294 |  224 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|        4 |  225 | `{` |
|      298 |  226 | `	switch(iErr){` |
|      112 |  227 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|        - |  228 | `	case 512  /* E_USER_WARNING */:` |
|      228 |  229 | `		return "Warning";` |
|       19 |  230 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|        - |  231 | `	case 8    /* E_NOTICE */:` |
|        - |  232 | `	case 1024 /* E_USER_NOTICE */:` |
|       41 |  233 | `		return "Notice";` |
|        2 |  234 | `	case 8192  /* E_DEPRECATED */:` |
|        - |  235 | `	case 16384 /* E_USER_DEPRECATED */:` |
|        5 |  236 | `		return "Deprecated";` |
|      ! 0 |  237 | `	case 256 /* E_USER_ERROR */:` |
|      ! 0 |  238 | `		return "Fatal error";` |
|       14 |  239 | `	default:` |
|       32 |  240 | `		return "Error";` |
|        - |  241 | `	}` |
|      151 |  242 | `}` |
|        - |  243 | `/*` |
|        - |  244 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|        - |  245 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|        - |  246 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|        - |  247 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|        - |  248 | ` * be authored cross-engine with --EXPECTF--.` |
|        - |  249 | ` */` |
|       26 |  250 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  251 | `{` |
|       30 |  252 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|       30 |  253 | `}` |
|      294 |  254 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|        4 |  255 | `{` |
|      298 |  256 | `	if( pFile ){` |
|      445 |  257 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|      147 |  258 | `			nLine ? nLine : 1);` |
|      147 |  259 | `	}` |
|      298 |  260 | `}` |
|        - |  261 | `/*` |
|        - |  262 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|        - |  263 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|        - |  264 | ` * (compile.c) and stock CLI's stderr log copy.` |
|        - |  265 | ` */` |
|      268 |  266 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  267 | `{` |
|      272 |  268 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      272 |  269 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      272 |  270 | `}` |
|        - |  271 | `/*` |
|        - |  272 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|        - |  273 | ` *` |
|        - |  274 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|        - |  275 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|        - |  276 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|        - |  277 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|        - |  278 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|        - |  279 | ` * php never produces. Building it into the message here is the single place that` |
|        - |  280 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|        - |  281 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|        - |  282 | ` */` |
|    30993 |  283 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|        5 |  284 | `{` |
|    30998 |  285 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     1138 |  286 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     1138 |  287 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      562 |  288 | `	}` |
|    30998 |  289 | `}` |
|        - |  290 | `/*` |
|        - |  291 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|        - |  292 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|        - |  293 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|        - |  294 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|        - |  295 | ` *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:` |
|        - |  296 | ``  *                     `\nLABEL: BODY in FILE on line N` `` |
|        - |  297 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|        - |  298 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|        - |  299 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|        - |  300 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|        - |  301 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|        - |  302 | ` */` |
|      290 |  303 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|        - |  304 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|        4 |  305 | `{` |
|      294 |  306 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      294 |  307 | `	sxi32 rc = SXRET_OK;` |
|      294 |  308 | `	if( pVm->bLogErrors ){` |
|      272 |  309 | `		SyBlobReset(pWorker);` |
|      272 |  310 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      272 |  311 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      272 |  312 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      272 |  313 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      134 |  314 | `	}` |
|      294 |  315 | `	if( pVm->bDisplayErrors ){` |
|        - |  316 | `		sxi32 rc2;` |
|       30 |  317 | `		SyBlobReset(pWorker);` |
|        - |  318 | `		/* php's text-mode display copy is prefixed with a blank line */` |
|       30 |  319 | `		SyBlobAppend(pWorker,"\n",sizeof(char));` |
|       30 |  320 | `		VmDiagnosticHeader(pWorker,iErr);` |
|       30 |  321 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|       30 |  322 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|       30 |  323 | `		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);` |
|        - |  324 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|        - |  325 | `		 * than letting a later successful write mask it */` |
|       30 |  326 | `		if( rc == SXRET_OK ){` |
|       30 |  327 | `			rc = rc2;` |
|       13 |  328 | `		}` |
|       13 |  329 | `	}` |
|      294 |  330 | `	return rc;` |
|        4 |  331 | `}` |
|     1227 |  332 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  333 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  334 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  335 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  336 | `	const char *zMessage /* Null terminated error message */` |
|        - |  337 | `	)` |
|        5 |  338 | `{` |
|        - |  339 | `	SyBlob sMsg;` |
|        - |  340 | `	SyString *pFile;` |
|     1232 |  341 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     1232 |  342 | `	sxi32 rc = SXRET_OK;` |
|        - |  343 | `	/* Peek the processed file if available */` |
|     1232 |  344 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1232 |  345 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     1232 |  346 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|        - |  347 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|        - |  348 | `		 * out-of-memory fatal through this path with none, and must not need` |
|        - |  349 | `		 * an allocation to say so. */` |
|      407 |  350 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      407 |  351 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      407 |  352 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      407 |  353 | `		nMsg = SyBlobLength(&sMsg);` |
|      197 |  354 | `	}` |
|        - |  355 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|        - |  356 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     1232 |  357 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|      155 |  358 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|      155 |  359 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|        - |  360 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|        - |  361 | `			 * '@' php still runs the handler (done just above) but prints` |
|        - |  362 | `			 * nothing itself. */` |
|      126 |  363 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|       61 |  364 | `		}` |
|       75 |  365 | `	}` |
|     1232 |  366 | `	SyBlobRelease(&sMsg);` |
|     1232 |  367 | `	return rc;` |
|        5 |  368 | `}` |
|        - |  369 | `/*` |
|        - |  370 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  371 | ` *` |
|        - |  372 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  373 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  374 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  375 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  376 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  377 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  378 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  379 | ` * calling it from a VM op.` |
|        - |  380 | ` */` |
|      ! 0 |  381 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  382 | `{` |
|      ! 0 |  383 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  384 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  385 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  386 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  387 | `	return PH7_ABORT;` |
|      ! 0 |  388 | `}` |
|        - |  389 | `/*` |
|        - |  390 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  391 | ` */` |
|      ! 0 |  392 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  393 | `{` |
|      ! 0 |  394 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  395 | `}` |
|        - |  396 | `/*` |
|        - |  397 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|        - |  398 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|        - |  399 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|        - |  400 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|        - |  401 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|        - |  402 | ` */` |
|        - |  403 | `/*` |
|        - |  404 | `` * TRUE when reading pVal as an `int` would lose what it holds -- the event php`` |
|        - |  405 | `` * 8.1 only DEPRECATES (`Implicit conversion from float 1.9 / float-string "1.9"`` |
|        - |  406 | `` * to int loses precision`) and §10 refuses outright.`` |
|        - |  407 | ` *` |
|        - |  408 | ` * Two kinds of value can lose something, and php treats them as one: a FLOAT,` |
|        - |  409 | ` * and a numeric STRING whose bytes spell a double. Which strings those are is` |
|        - |  410 | ` * not just the ones carrying a '.' or an exponent — an integer-shaped run too` |
|        - |  411 | ` * long for an int64 is a double in php too ("99999999999999999999"), and it` |
|        - |  412 | ` * loses digits exactly the same way. So the question is asked of the NUMBER the` |
|        - |  413 | ` * value converts to, whatever spelling it arrived in.` |
|        - |  414 | ` *` |
|        - |  415 | ` * A value is lossy when that number is not an exact int64: outside the range at` |
|        - |  416 | ` * all (NaN and the infinities included), or carrying a fraction. An integral` |
|        - |  417 | `` * float in range (`4.0 % 3`, `$o->i = 5.0`) loses nothing and is not lossy.`` |
|        - |  418 | ` */` |
|   118582 |  419 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|        5 |  420 | `{` |
|        - |  421 | `	ph7_real r;` |
|   118587 |  422 | `	if( pVal == 0 ){` |
|      ! 0 |  423 | `		return FALSE;` |
|        - |  424 | `	}` |
|   118587 |  425 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|       37 |  426 | `		r = pVal->rVal;` |
|   118592 |  427 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|        - |  428 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|        - |  429 | `		 * no, and a numeric conversion would replace it. */` |
|        - |  430 | `		ph7_value sProbe;` |
|        - |  431 | `		SyString sStr;` |
|        - |  432 | `		int bReal;` |
|   101159 |  433 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|   101159 |  434 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|        - |  435 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|        - |  436 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|        - |  437 | `		 * answers without building anything. Conservative on purpose: it may` |
|        - |  438 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|   101159 |  439 | `		if( n < 19 ){` |
|   302939 |  440 | `			for( i = 0 ; i < n ; ++i ){` |
|   201833 |  441 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|       23 |  442 | `					break;` |
|        - |  443 | `				}` |
|   100898 |  444 | `			}` |
|   101153 |  445 | `			if( i >= n ){` |
|   101113 |  446 | `				return FALSE;` |
|        - |  447 | `			}` |
|       21 |  448 | `		}` |
|       50 |  449 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       50 |  450 | `		PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|       50 |  451 | `		PH7_MemObjToNumeric(&sProbe);` |
|       50 |  452 | `		bReal = (sProbe.iFlags & MEMOBJ_REAL) != 0;` |
|       50 |  453 | `		r = sProbe.rVal;` |
|       50 |  454 | `		PH7_MemObjRelease(&sProbe);` |
|       50 |  455 | `		if( !bReal ){` |
|        5 |  456 | `			return FALSE;` |
|        - |  457 | `		}` |
|       24 |  458 | `	}else{` |
|    17399 |  459 | `		return FALSE;` |
|        - |  460 | `	}` |
|        - |  461 | `	/* The bounds are tested in DOUBLE space, BEFORE the cast: (sxi64)r is` |
|        - |  462 | `	 * undefined outside them, and a test of an undefined cast's result is one an` |
|        - |  463 | `	 * optimiser is entitled to delete -- which is exactly how the printf family's` |
|        - |  464 | `	 * PHP_INT_MIN guard disappeared (§2). NaN fails both comparisons and either` |
|        - |  465 | `	 * infinity fails one, so all three are lossy without a libm predicate. */` |
|        - |  466 | `	/* The cast is a no-op wherever ph7_real is the double this screen is written` |
|        - |  467 | `	 * for. Under PH7_OMIT_FLOATING_POINT ph7_real is sxi64, and handing an` |
|        - |  468 | `	 * integer to a double parameter is a narrowing MSVC reports as C4244 --` |
|        - |  469 | `	 * which /WX makes a build error, so the tiny build is where it bites. */` |
|       81 |  470 | `	if( !PH7_RealFitsInt64((double)r) ){` |
|       26 |  471 | `		return TRUE;` |
|        - |  472 | `	}` |
|       57 |  473 | `	return r != (ph7_real)(sxi64)r;` |
|    59296 |  474 | `}` |
|        - |  475 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|        - |  476 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|        - |  477 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|        - |  478 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|        - |  479 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|    18054 |  480 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|        5 |  481 | `{` |
|    18059 |  482 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|    18033 |  483 | `		return SXRET_OK;` |
|        - |  484 | `	}` |
|       27 |  485 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       26 |  486 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|        - |  487 | `			? "Implicit conversion from float to int loses precision"` |
|        - |  488 | `			: "Implicit conversion from float-string to int loses precision");` |
|     9032 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|        - |  492 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|        - |  493 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|        - |  494 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|        - |  495 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|        - |  496 | ` */` |
|  2345642 |  497 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  498 | `{` |
|        - |  499 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|        - |  500 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  2345647 |  501 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  502 | `}` |
|        - |  503 | `/*` |
|        - |  504 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|        - |  505 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|        - |  506 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|        - |  507 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|        - |  508 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|        - |  509 | ` * keep native re-entries off a finite C stack).` |
|        - |  510 | ` */` |
|  4616686 |  511 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|        5 |  512 | `{` |
|  4616691 |  513 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|        5 |  514 | `}` |
|        - |  515 | `/*` |
|        - |  516 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  517 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  518 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  519 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  520 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  521 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  522 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  523 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  524 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|        - |  525 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|        - |  526 | ` * (VmNativeNestingFatal).` |
|        - |  527 | ` *` |
|        - |  528 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  529 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  530 | ` * re-enter and loop.` |
|        - |  531 | ` */` |
|        2 |  532 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  533 | `{` |
|        3 |  534 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  535 | `		return PH7_ABORT;` |
|        - |  536 | `	}` |
|        3 |  537 | `	pVm->iExitStatus = 255;` |
|        3 |  538 | `	pVm->bHaltRequested = 1;` |
|        3 |  539 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        3 |  540 | `	return PH7_ABORT;` |
|        2 |  541 | `}` |
|        - |  542 | `/*` |
|        - |  543 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|        - |  544 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|        - |  545 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|        - |  546 | ` */` |
|        4 |  547 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|        2 |  548 | `{` |
|        6 |  549 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  550 | `		return PH7_ABORT;` |
|        - |  551 | `	}` |
|        6 |  552 | `	pVm->iExitStatus = 255;` |
|        6 |  553 | `	pVm->bHaltRequested = 1;` |
|        6 |  554 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|        6 |  555 | `	return PH7_ABORT;` |
|        4 |  556 | `}` |
|        - |  557 | `/*` |
|        - |  558 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  559 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  560 | ` * information.` |
|        - |  561 | ` */` |
|    30590 |  562 | `static sxi32 VmThrowErrorAp(` |
|        - |  563 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  564 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  565 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  566 | `	const char *zFormat, /* Format message */` |
|        - |  567 | `	va_list ap           /* Variable list of arguments */` |
|        - |  568 | `	)` |
|        5 |  569 | `{` |
|        - |  570 | `	SyBlob sMsg;` |
|        - |  571 | `	SyString *pFile;` |
|    30595 |  572 | `	sxi32 rc = SXRET_OK;` |
|        - |  573 | `	/* Peek the processed file if available */` |
|    30595 |  574 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  575 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|    30595 |  576 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|    30595 |  577 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|    30595 |  578 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  579 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|        - |  580 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|        - |  581 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|        - |  582 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|    30595 |  583 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|        - |  584 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|        - |  585 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|    27811 |  586 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|    27811 |  587 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|    27643 |  588 | `			SyBlobRelease(&sMsg);` |
|    27643 |  589 | `			return SXRET_OK;` |
|        - |  590 | `		}` |
|      256 |  591 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|       84 |  592 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|       84 |  593 | `	}` |
|     2957 |  594 | `	SyBlobRelease(&sMsg);` |
|     2957 |  595 | `	return rc;` |
|    15300 |  596 | `}` |
|        - |  597 | `/*` |
|        - |  598 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - |  599 | ` * scope), or NULL when executing outside any class context.` |
|        - |  600 | ` */` |
|     9614 |  601 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 |  602 | `{` |
|     9619 |  603 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      169 |  604 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      169 |  605 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - |  606 | `	}` |
|     9455 |  607 | `	return 0;` |
|     4809 |  608 | `}` |
|        - |  609 | `/*` |
|        - |  610 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|        - |  611 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|        - |  612 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|        - |  613 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|        - |  614 | ` * as an undefined function and therefore as another engine throw). Each such` |
|        - |  615 | ` * throw would construct another exception and recurse until the native-nesting` |
|        - |  616 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|        - |  617 | ` * (an engine throw from inside a user exception's constructor) working and` |
|        - |  618 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|        - |  619 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|        - |  620 | ` * after the call.` |
|        - |  621 | ` */` |
|        - |  622 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|   350460 |  623 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|        5 |  624 | `{` |
|   350465 |  625 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|      ! 0 |  626 | `		return 0;` |
|        - |  627 | `	}` |
|   350465 |  628 | `	pVm->nExcCtorDepth++;` |
|   350465 |  629 | `	return 1;` |
|   175235 |  630 | `}` |
|        - |  631 | `/*` |
|        - |  632 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - |  633 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - |  634 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - |  635 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - |  636 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - |  637 | ` */` |
|   205900 |  638 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 |  639 | `{` |
|        - |  640 | `	ph7_class *pErrClass;` |
|        - |  641 | `	ph7_class_instance *pThis;` |
|        - |  642 | `	ph7_class_method *pCons;` |
|        - |  643 | `	VmFrame *pFrame;` |
|        - |  644 | `	sxi32 rc;` |
|   205905 |  645 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|   205905 |  646 | `	if( pErrClass == 0 ){` |
|      ! 0 |  647 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  648 | `		return PH7_ABORT;` |
|        - |  649 | `	}` |
|   205905 |  650 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|   205905 |  651 | `	if( pThis == 0 ){` |
|      ! 0 |  652 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  653 | `		return PH7_ABORT;` |
|        - |  654 | `	}` |
|   205905 |  655 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|   205905 |  656 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|        - |  657 | `		ph7_value sArg;` |
|        - |  658 | `		ph7_value *apArg[1];` |
|        - |  659 | `		SyString sMsgStr;` |
|   205905 |  660 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|   205905 |  661 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   205905 |  662 | `		apArg[0] = &sArg;` |
|   205905 |  663 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   205905 |  664 | `		PH7_MemObjRelease(&sArg);` |
|   205905 |  665 | `		pVm->nExcCtorDepth--;` |
|   102950 |  666 | `	}` |
|   205905 |  667 | `	SyBlobRelease(pMsg);` |
|   205905 |  668 | `	pFrame = pVm->pFrame;` |
|   205905 |  669 | `	if( pFrame ){` |
|   205905 |  670 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   205905 |  671 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   102950 |  672 | `	}` |
|   205905 |  673 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   205905 |  674 | `	PH7_ClassInstanceUnref(pThis);` |
|   205905 |  675 | `	if( rc == SXERR_ABORT ){` |
|       30 |  676 | `		return PH7_ABORT;` |
|        - |  677 | `	}` |
|   205879 |  678 | `	return PH7_EXCEPTION;` |
|   102955 |  679 | `}` |
|        - |  680 | `/*` |
|        - |  681 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|        - |  682 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|        - |  683 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|        - |  684 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|        - |  685 | ` * result through its normal goto Exception / goto Abort.` |
|        - |  686 | ` */` |
|      334 |  687 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|        3 |  688 | `{` |
|        - |  689 | `	SyBlob sMsg;` |
|      337 |  690 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      337 |  691 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      337 |  692 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|        3 |  693 | `}` |
|        - |  694 | `/*` |
|        - |  695 | ` * Enum case singletons (PHP 8.1).` |
|        - |  696 | ` *` |
|        - |  697 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|        - |  698 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|        - |  699 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|        - |  700 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|        - |  701 | ` * backing-value type check and the duplicate-value check only fire when a` |
|        - |  702 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|        - |  703 | ` */` |
|        - |  704 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|        - |  705 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|        - |  706 | ` * readonly property" through the normal store path. */` |
|        - |  707 |  |
|        - |  708 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|        - |  709 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|      410 |  710 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|        2 |  711 | `{` |
|      412 |  712 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|        - |  713 | `	ph7_class_instance *pObj;` |
|        - |  714 | `	SyHashEntry *pEntry;` |
|      412 |  715 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       59 |  716 | `		return 0;` |
|        - |  717 | `	}` |
|      354 |  718 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      354 |  719 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      354 |  720 | `	if( pEntry == 0 ){` |
|      ! 0 |  721 | `		return 0;` |
|        - |  722 | `	}` |
|      354 |  723 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      207 |  724 | `}` |
|        - |  725 | `/*` |
|        - |  726 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|        - |  727 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|        - |  728 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|        - |  729 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|        - |  730 | ` * raises it. Returns the throw status to park/route.` |
|        - |  731 | ` */` |
|        2 |  732 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|        1 |  733 | `{` |
|        - |  734 | `	SyBlob sMsg;` |
|        3 |  735 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|        3 |  736 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|        3 |  737 | `	pVm->pConstCycleAttr = 0;` |
|        3 |  738 | `	pVm->pConstCycleClass = 0;` |
|        3 |  739 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  740 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|        1 |  741 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|        3 |  742 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 |  743 | `}` |
|        - |  744 | `/*` |
|        - |  745 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|        - |  746 | ` * materialize lazily and individually on first access — the backing-value` |
|        - |  747 | ` * type check fires per case, and the duplicate-value check compares only` |
|        - |  748 | ` * against cases that have already materialized (a broken sibling case does` |
|        - |  749 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|        - |  750 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|        - |  751 | ` * (duplicate value / self-reference) — which the caller routes` |
|        - |  752 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|        - |  753 | ` */` |
|      546 |  754 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|        5 |  755 | `{` |
|        - |  756 | `	ph7_class_attr **apCase;` |
|        - |  757 | `	ph7_class_instance *pObj;` |
|        - |  758 | `	ph7_value *pSlot;` |
|        - |  759 | `	ph7_value sBacking,sPropVal;` |
|        - |  760 | `	sxu32 i;` |
|      551 |  761 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      401 |  762 | `		return SXRET_OK;` |
|        - |  763 | `	}` |
|      151 |  764 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - |  765 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|        - |  766 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|      ! 0 |  767 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      ! 0 |  768 | `			pVm->pConstCycleAttr = pCase;` |
|      ! 0 |  769 | `			pVm->pConstCycleClass = pClass;` |
|      ! 0 |  770 | `		}` |
|      ! 0 |  771 | `		return SXRET_OK;` |
|        - |  772 | `	}` |
|      151 |  773 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      151 |  774 | `	if( pClass->nEnumBacking != 0 ){` |
|       91 |  775 | `		if( pCase->pNativeValue ){` |
|        - |  776 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|        - |  777 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|        - |  778 | `			 * is what that byte-code would have produced anyway. */` |
|        5 |  779 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|       89 |  780 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|        - |  781 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|       87 |  782 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|        - |  783 | `			sxi32 rcExec;` |
|       87 |  784 | `			pVm->pConstEvalClass = pClass;` |
|       87 |  785 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|       87 |  786 | `			pVm->nConstEvalDepth++;` |
|       87 |  787 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|       87 |  788 | `			pVm->nConstEvalDepth--;` |
|       87 |  789 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       87 |  790 | `			pVm->pConstEvalClass = pSaveCtx;` |
|       87 |  791 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - |  792 | `				/* The backing expression raised: abandon materialization and` |
|        - |  793 | `				 * hand the status to the caller to park/route. */` |
|        3 |  794 | `				PH7_MemObjRelease(&sBacking);` |
|        3 |  795 | `				return rcExec;` |
|        - |  796 | `			}` |
|       84 |  797 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      ! 0 |  798 | `				PH7_MemObjRelease(&sBacking);` |
|      ! 0 |  799 | `				return VmConstCycleThrow(&(*pVm));` |
|        - |  800 | `			}` |
|       40 |  801 | `		}` |
|       88 |  802 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|        - |  803 | `			/* php: TypeError, checked lazily at first case access */` |
|        - |  804 | `			SyBlob sMsg;` |
|        3 |  805 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|        3 |  806 | `			PH7_MemObjRelease(&sBacking);` |
|        3 |  807 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        2 |  808 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|        2 |  809 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|        3 |  810 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - |  811 | `		}` |
|       86 |  812 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  813 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|        - |  814 | `			 * the typed-constant leniency) to a genuine int. */` |
|       17 |  815 | `			PH7_MemObjToInteger(&sBacking);` |
|        9 |  816 | `		}else{` |
|       70 |  817 | `			PH7_MemObjToString(&sBacking);` |
|        - |  818 | `		}` |
|        - |  819 | `		/* php: two cases sharing one backing value are an Error — compared` |
|        - |  820 | `		 * against already-materialized cases only (php registers values as` |
|        - |  821 | `		 * each case evaluates). */` |
|       86 |  822 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      278 |  823 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|        - |  824 | `			ph7_value *pPrev;` |
|      198 |  825 | `			int bDup = 0;` |
|      198 |  826 | `			if( apCase[i] == pCase ){` |
|       84 |  827 | `				continue;` |
|        - |  828 | `			}` |
|      115 |  829 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      115 |  830 | `			if( pPrev ){` |
|       57 |  831 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  832 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|        4 |  833 | `				}else{` |
|       63 |  834 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|       50 |  835 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|       24 |  836 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|        - |  837 | `				}` |
|       28 |  838 | `			}` |
|      115 |  839 | `			if( bDup ){` |
|        - |  840 | `				/* php prints the two cases in DECLARATION order regardless of` |
|        - |  841 | `				 * which one is being evaluated. */` |
|        3 |  842 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|        - |  843 | `				SyBlob sMsg;` |
|        - |  844 | `				sxu32 j;` |
|        5 |  845 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|        5 |  846 | `					if( apCase[j] == pCase ){ break; }` |
|        2 |  847 | `				}` |
|        3 |  848 | `				if( j < i ){` |
|      ! 0 |  849 | `					pFirst = pCase;` |
|      ! 0 |  850 | `					pSecond = apCase[i];` |
|      ! 0 |  851 | `				}` |
|        3 |  852 | `				PH7_MemObjRelease(&sBacking);` |
|        3 |  853 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  854 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|        1 |  855 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|        3 |  856 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - |  857 | `			}` |
|       57 |  858 | `		}` |
|       40 |  859 | `	}` |
|        - |  860 | `	/* Create the singleton and fill its readonly props */` |
|      144 |  861 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      144 |  862 | `	if( pObj == 0 ){` |
|      ! 0 |  863 | `		PH7_MemObjRelease(&sBacking);` |
|      ! 0 |  864 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  865 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|      ! 0 |  866 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 |  867 | `		return PH7_ABORT;` |
|        - |  868 | `	}` |
|      144 |  869 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      144 |  870 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      144 |  871 | `	PH7_MemObjRelease(&sPropVal);` |
|      144 |  872 | `	if( pClass->nEnumBacking != 0 ){` |
|       84 |  873 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|       40 |  874 | `	}` |
|      144 |  875 | `	PH7_MemObjRelease(&sBacking);` |
|        - |  876 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|        - |  877 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      144 |  878 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      144 |  879 | `	if( pSlot == 0 ){` |
|      ! 0 |  880 | `		PH7_ClassInstanceUnref(pObj);` |
|      ! 0 |  881 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  882 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|      ! 0 |  883 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 |  884 | `		return PH7_ABORT;` |
|        - |  885 | `	}` |
|      144 |  886 | `	pSlot->x.pOther = pObj;` |
|      144 |  887 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      144 |  888 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      144 |  889 | `	pCase->nIdx = pSlot->nIdx;` |
|      144 |  890 | `	return SXRET_OK;` |
|      278 |  891 | `}` |
|        - |  892 | `/*` |
|        - |  893 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|        - |  894 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|        - |  895 | ` * there, so a broken case surfaces its error at the same point).` |
|        - |  896 | ` */` |
|      256 |  897 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|        5 |  898 | `{` |
|        - |  899 | `	ph7_class_attr **apCase;` |
|        - |  900 | `	sxu32 n;` |
|      261 |  901 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 |  902 | `		return SXRET_OK;` |
|        - |  903 | `	}` |
|      261 |  904 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      785 |  905 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      535 |  906 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|      535 |  907 | `		if( rc != SXRET_OK ){` |
|        8 |  908 | `			return rc;` |
|        - |  909 | `		}` |
|      266 |  910 | `	}` |
|      254 |  911 | `	return SXRET_OK;` |
|      133 |  912 | `}` |
|        - |  913 | `/*` |
|        - |  914 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|        - |  915 | ` * or 0 when the name does not name an enum.` |
|        - |  916 | ` */` |
|      212 |  917 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|        3 |  918 | `{` |
|        - |  919 | `	ph7_class *pClass;` |
|      215 |  920 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|      ! 0 |  921 | `		return 0;` |
|        - |  922 | `	}` |
|      321 |  923 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      106 |  924 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      217 |  925 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|        3 |  926 | `		pClass = pClass->pNextName;` |
|        1 |  927 | `	}` |
|      215 |  928 | `	return pClass;` |
|      109 |  929 | `}` |
|        - |  930 | `/*` |
|        - |  931 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|        - |  932 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|        - |  933 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|        - |  934 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|        - |  935 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|        - |  936 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|        - |  937 | ` */` |
|      432 |  938 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|        5 |  939 | `{` |
|      437 |  940 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      432 |  941 | `	if( pInner && pInner->pUserData` |
|      256 |  942 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      ! 0 |  943 | `		return 0;` |
|        - |  944 | `	}` |
|      437 |  945 | `	return pVm->nCurLine;` |
|      221 |  946 | `}` |
|        - |  947 | `/*` |
|        - |  948 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|        - |  949 | ` * not the pointer: aMemObj is a by-value SySet, so any nested evaluation (an` |
|        - |  950 | ` * initializer, or the constructor of the very TypeError being raised) can grow` |
|        - |  951 | ` * and REALLOC the pool, leaving a pointer taken before it dangling — the rule` |
|        - |  952 | ` * VmLocalExecIntoObj is built around. The slot's contents are released first:` |
|        - |  953 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|        - |  954 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|        - |  955 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|        - |  956 | ` */` |
|       36 |  957 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|        3 |  958 | `{` |
|       39 |  959 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  960 | `	VmSlot sSlot;` |
|       39 |  961 | `	if( pObj == 0 ){` |
|      ! 0 |  962 | `		return;` |
|        - |  963 | `	}` |
|       39 |  964 | `	PH7_MemObjRelease(pObj);` |
|       39 |  965 | `	sSlot.nIdx = nIdx;` |
|       39 |  966 | `	sSlot.pUserData = 0;` |
|       39 |  967 | `	SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       21 |  968 | `}` |
|        - |  969 | `/*` |
|        - |  970 | ` * Evaluate a class constant's initializer on demand.` |
|        - |  971 | ` *` |
|        - |  972 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|        - |  973 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|        - |  974 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|        - |  975 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|        - |  976 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|        - |  977 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|        - |  978 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|        - |  979 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|        - |  980 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|        - |  981 | ` * "Cannot declare self-referencing constant" Error.` |
|        - |  982 | ` */` |
|      718 |  983 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  984 | `{` |
|        - |  985 | `	ph7_value *pMemObj;` |
|      718 |  986 | `	if( pAttr->nIdx != SXU32_HIGH` |
|      718 |  987 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      723 |  988 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|      ! 0 |  989 | `		return SXRET_OK;` |
|        - |  990 | `	}` |
|      723 |  991 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - |  992 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|        - |  993 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|        - |  994 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|        3 |  995 | `		if( pVm->pConstCycleAttr == 0 ){` |
|        3 |  996 | `			pVm->pConstCycleAttr = pAttr;` |
|        3 |  997 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        1 |  998 | `		}` |
|        3 |  999 | `		return SXRET_OK;` |
|        - | 1000 | `	}` |
|      721 | 1001 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      721 | 1002 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1003 | `		return SXERR_MEM;` |
|        - | 1004 | `	}` |
|      721 | 1005 | `	if( pAttr->pNativeValue ){` |
|        - | 1006 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|        - | 1007 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|        - | 1008 | `		 * when they went lazy (17th session), so this path — the only one an` |
|        - | 1009 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|        - | 1010 | `		 * which is why every native class constant read NULL: nothing had declared` |
|        - | 1011 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|        - | 1012 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|        - | 1013 | `		 * failure path to mirror below. */` |
|      349 | 1014 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      349 | 1015 | `		pAttr->nIdx = pMemObj->nIdx;` |
|      349 | 1016 | `		return SXRET_OK;` |
|        - | 1017 | `	}` |
|      377 | 1018 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      377 | 1019 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      377 | 1020 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      377 | 1021 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|        - | 1022 | `		sxu32 nSaveLazyLine;` |
|        - | 1023 | `		sxi32 nSaveLazyDepth;` |
|        - | 1024 | `		sxu32 nSlot;` |
|        - | 1025 | `		sxi32 rcExec;` |
|      377 | 1026 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      377 | 1027 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 1028 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|        - | 1029 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|        - | 1030 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|      377 | 1031 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 1032 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|        - | 1033 | `		 * throw out of its own bytecode carries. */` |
|      377 | 1034 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      377 | 1035 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      377 | 1036 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      377 | 1037 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      377 | 1038 | `		pVm->nConstEvalDepth++;` |
|      377 | 1039 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      377 | 1040 | `		pVm->nConstEvalDepth--;` |
|      377 | 1041 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      377 | 1042 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      377 | 1043 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      377 | 1044 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      377 | 1045 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      377 | 1046 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|      372 | 1047 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|      348 | 1048 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|        - | 1049 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|        - | 1050 | `			 * class constant's expression at each access until one of them` |
|        - | 1051 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|        - | 1052 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|        - | 1053 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|        - | 1054 | `			 * the rest of the run — and made a static default that named it` |
|        - | 1055 | `			 * (whose own evaluation is deferred to first access) find it` |
|        - | 1056 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|        - | 1057 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|        - | 1058 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|        - | 1059 | `			 * it is found — an inner level only records it — but the value is` |
|        - | 1060 | `			 * unusable and the next access must be able to detect it again.` |
|        - | 1061 | `			 * No loop: each access runs the initializer once and raises. */` |
|       35 | 1062 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|       35 | 1063 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1064 | `				/* Hand the status to the caller to park/route. */` |
|       32 | 1065 | `				return rcExec;` |
|        - | 1066 | `			}` |
|        3 | 1067 | `			if( pVm->nConstEvalDepth == 0 ){` |
|        - | 1068 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|        - | 1069 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|        3 | 1070 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1071 | `			}` |
|      ! 0 | 1072 | `			return SXRET_OK;` |
|        - | 1073 | `		}` |
|      345 | 1074 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1075 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|        - | 1076 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|        - | 1077 | `			 * access raises again — php re-runs the whole materialization each` |
|        - | 1078 | `			 * time. A pass may widen int -> float in place, which is the value` |
|        - | 1079 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|        - | 1080 | `			 * runs php code that may grow (and realloc) aMemObj — so the slot is` |
|        - | 1081 | `			 * addressed by index from here on, never through pMemObj. */` |
|        5 | 1082 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|        5 | 1083 | `			if( rcType != SXRET_OK ){` |
|        5 | 1084 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|        5 | 1085 | `				return rcType;` |
|        - | 1086 | `			}` |
|      ! 0 | 1087 | `		}` |
|        - | 1088 | `		/* Memoize the value. */` |
|      341 | 1089 | `		pAttr->nIdx = nSlot;` |
|      341 | 1090 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|      341 | 1091 | `		return SXRET_OK;` |
|        - | 1092 | `	}` |
|      ! 0 | 1093 | `	pAttr->nIdx = pMemObj->nIdx;` |
|      ! 0 | 1094 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      ! 0 | 1095 | `	return SXRET_OK;` |
|      364 | 1096 | `}` |
|        - | 1097 | `/*` |
|        - | 1098 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|        - | 1099 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|        - | 1100 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|        - | 1101 | ` */` |
|      152 | 1102 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        3 | 1103 | `{` |
|      155 | 1104 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       81 | 1105 | `		return SXRET_OK;` |
|        - | 1106 | `	}` |
|       76 | 1107 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       15 | 1108 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|        - | 1109 | `	}` |
|       62 | 1110 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       79 | 1111 | `}` |
|        - | 1112 | `/*` |
|        - | 1113 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|        - | 1114 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|        - | 1115 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|        - | 1116 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|        - | 1117 | ` */` |
|        6 | 1118 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|        1 | 1119 | `{` |
|        - | 1120 | `	SyBlob sMsg;` |
|        7 | 1121 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 | 1122 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|        7 | 1123 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1124 | `}` |
|        - | 1125 | `/*` |
|        - | 1126 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|        - | 1127 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|        - | 1128 | ` * raised at the store site here with the same message and the same` |
|        - | 1129 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|        - | 1130 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|        - | 1131 | ` */` |
|        2 | 1132 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|        1 | 1133 | `{` |
|        3 | 1134 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|        3 | 1135 | `	pVm->iExitStatus = 255;` |
|        3 | 1136 | `	pVm->bHaltRequested = 1;` |
|        3 | 1137 | `	return PH7_ABORT;` |
|        1 | 1138 | `}` |
|        - | 1139 | `/*` |
|        - | 1140 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - | 1141 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - | 1142 | ` * possible.` |
|        - | 1143 | ` */` |
|   100168 | 1144 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        5 | 1145 | `{` |
|   100173 | 1146 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|   100173 | 1147 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - | 1148 | `	char zType[192];` |
|   150257 | 1149 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|    50084 | 1150 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));` |
|        - | 1151 | `	SyBlob sMsg;` |
|   100173 | 1152 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1153 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - | 1154 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|   100173 | 1155 | `	if( pOwner ){` |
|   100173 | 1156 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|    50084 | 1157 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|    50089 | 1158 | `	}else{` |
|      ! 0 | 1159 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",` |
|      ! 0 | 1160 | `			zGiven,&pAttr->sName,zTypeText);` |
|        - | 1161 | `	}` |
|   100173 | 1162 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - | 1166 | ` */` |
|   100020 | 1167 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1168 | `{` |
|   100025 | 1169 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   100025 | 1170 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - | 1171 | `	SyBlob sMsg;` |
|   100025 | 1172 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   100025 | 1173 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|    50010 | 1174 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|   100025 | 1175 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1176 | `}` |
|        - | 1177 | `/*` |
|        - | 1178 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|        - | 1179 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|        - | 1180 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|        - | 1181 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|        - | 1182 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|        - | 1183 | ` */` |
|        - | 1184 | `/*` |
|        - | 1185 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|        - | 1186 | ` * property from a scope its set-visibility excludes:` |
|        - | 1187 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|        - | 1188 | ` */` |
|       14 | 1189 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1190 | `{` |
|       15 | 1191 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       15 | 1192 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       15 | 1193 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|        - | 1194 | `	SyBlob sMsg;` |
|       15 | 1195 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 | 1196 | `	if( pActive ){` |
|        3 | 1197 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|        1 | 1198 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|        2 | 1199 | `	}else{` |
|       13 | 1200 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|        6 | 1201 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|        - | 1202 | `	}` |
|       15 | 1203 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1204 | `}` |
|        - | 1205 | `/*` |
|        - | 1206 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|        - | 1207 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|        - | 1208 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|        - | 1209 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|        - | 1210 | ` */` |
|       32 | 1211 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|        1 | 1212 | `{` |
|       33 | 1213 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|       33 | 1214 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 1215 | `	int bOk;` |
|       33 | 1216 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|       27 | 1217 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|       14 | 1218 | `	}else{` |
|        7 | 1219 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        - | 1220 | `	}` |
|       33 | 1221 | `	if( !bOk ){` |
|       15 | 1222 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|        - | 1223 | `	}` |
|       19 | 1224 | `	return SXRET_OK;` |
|       17 | 1225 | `}` |
|        - | 1226 | `/*` |
|        - | 1227 | ` * php's write refusal for a native property whose handler takes NO write` |
|        - | 1228 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE). The sentence is the readonly one -- php's` |
|        - | 1229 | ` * date_period_write_property says exactly that -- but the property carries no` |
|        - | 1230 | ` * readonly FLAG, so this is spelled apart from VmThrowReadonlyError rather than` |
|        - | 1231 | ` * reached through it: Reflection reports isReadOnly() false for DatePeriod's` |
|        - | 1232 | ` * seven in both engines, and the readonly rules (write-once, set-scope, the` |
|        - | 1233 | ` * __clone re-initialization window) do not apply to a handler that never` |
|        - | 1234 | ` * accepts one.` |
|        - | 1235 | ` */` |
|       40 | 1236 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1237 | `{` |
|       41 | 1238 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 1239 | `	SyBlob sMsg;` |
|       41 | 1240 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       41 | 1241 | `	SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       41 | 1242 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1243 | `}` |
|        - | 1244 | `/*` |
|        - | 1245 | `` * And its unset half, which php words differently: `Cannot unset C::$p`, with`` |
|        - | 1246 | ` * neither "readonly" nor "property" in it.` |
|        - | 1247 | ` */` |
|       18 | 1248 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1249 | `{` |
|        - | 1250 | `	SyBlob sMsg;` |
|       19 | 1251 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1252 | `	/* The OBJECT's class, not the declaring one -- php's two refusals disagree` |
|        - | 1253 | `	 * about which to print, and a subclass of DatePeriod shows it: the write says` |
|        - | 1254 | ``	 * `DatePeriod::$interval` and the unset says `SubDp::$interval`. */`` |
|       19 | 1255 | `	SyBlobFormat(&sMsg,"Cannot unset %z::$%z",&pClass->sName,&pAttr->sName);` |
|       19 | 1256 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1257 | `}` |
|        - | 1258 | `/*` |
|        - | 1259 | ` * And php's THIRD refusal for a property its handler will not have written:` |
|        - | 1260 | `` * `Property p is read only`, which names neither the class nor the `$`.`` |
|        - | 1261 | `` * PDOStatement's `queryString` is php's case, and the shapes it applies to are`` |
|        - | 1262 | ` * the plain store and the unset alone -- see PH7_CLASS_ATTR_NATIVE_RDONLY.` |
|        - | 1263 | ` */` |
|       10 | 1264 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr)` |
|        1 | 1265 | `{` |
|        - | 1266 | `	SyBlob sMsg;` |
|       11 | 1267 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 | 1268 | `	SyBlobFormat(&sMsg,"Property %z is read only",&pAttr->sName);` |
|       11 | 1269 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1270 | `}` |
|        - | 1271 | `/*` |
|        - | 1272 | `` * php's answer to `unset($o->p)` where p is READONLY. Two of the three cases`` |
|        - | 1273 | ` * refuse, and the sentences are not the write ones:` |
|        - | 1274 | ` *` |
|        - | 1275 | ` *   * an INITIALIZED one refuses from every scope, its own included --` |
|        - | 1276 | `` *     `Cannot unset readonly property C::$p`. Destroying it would re-arm the`` |
|        - | 1277 | ` *     write-once latch, which is exactly what readonly exists to prevent;` |
|        - | 1278 | ` *` |
|        - | 1279 | ` *   * an UNINITIALIZED one is a WRITE-shaped act, so it takes the set-visibility` |
|        - | 1280 | ` *     rules: allowed from the declaring class or a subclass (php lets a lazy` |
|        - | 1281 | ` *     proxy re-arm one that way), and otherwise the asymmetric-visibility` |
|        - | 1282 | `` *     refusal. php words that one two ways -- an EXPLICIT `private(set)` gets the`` |
|        - | 1283 | ` *     ordinary asymmetric sentence with no "readonly" in it, and everything else` |
|        - | 1284 | `` *     gets readonly's own implicit `protected(set) readonly`.`` |
|        - | 1285 | ` *` |
|        - | 1286 | ` * Answers SXRET_OK when the unset may proceed, else the throw status.` |
|        - | 1287 | ` */` |
|       24 | 1288 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr)` |
|        1 | 1289 | `{` |
|       25 | 1290 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - | 1291 | `	ph7_class *pOwner;` |
|        - | 1292 | `	ph7_class *pActive;` |
|        - | 1293 | `	SyBlob sMsg;` |
|        - | 1294 | `	int bInit,bScope;` |
|       25 | 1295 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) == 0 ){` |
|      ! 0 | 1296 | `		return SXRET_OK;` |
|        - | 1297 | `	}` |
|       25 | 1298 | `	pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       25 | 1299 | `	pActive = VmCurrentSelf(pVm);` |
|       25 | 1300 | `	bInit = (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0;` |
|       25 | 1301 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        7 | 1302 | `		bScope = (pActive != 0 && pActive == pOwner);` |
|        4 | 1303 | `	}else{` |
|       19 | 1304 | `		bScope = (pActive != 0 && pOwner != 0 && PH7_VmInstanceOf(pActive,pOwner));` |
|        - | 1305 | `	}` |
|       25 | 1306 | `	if( !bInit && bScope ){` |
|        9 | 1307 | `		return SXRET_OK;` |
|        - | 1308 | `	}` |
|       17 | 1309 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 | 1310 | `	if( bInit ){` |
|        9 | 1311 | `		SyBlobFormat(&sMsg,"Cannot unset readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|        5 | 1312 | `	}else{` |
|       13 | 1313 | `		const char *zWhat = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        4 | 1314 | `			? "private(set)" : "protected(set) readonly";` |
|        9 | 1315 | `		if( pActive ){` |
|        3 | 1316 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from scope %z",` |
|        1 | 1317 | `				zWhat,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|        2 | 1318 | `		}else{` |
|        7 | 1319 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from global scope",` |
|        3 | 1320 | `				zWhat,&pOwner->sName,&pAttr->sName);` |
|        - | 1321 | `		}` |
|        - | 1322 | `	}` |
|       17 | 1323 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       13 | 1324 | `}` |
|       50 | 1325 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        5 | 1326 | `{` |
|       55 | 1327 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 1328 | `	SyBlob sMsg;` |
|       55 | 1329 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       55 | 1330 | `	if( bModify ){` |
|       51 | 1331 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       28 | 1332 | `	}else{` |
|        6 | 1333 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        6 | 1334 | `		if( pActive ){` |
|      ! 0 | 1335 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 | 1336 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|      ! 0 | 1337 | `		}else{` |
|        6 | 1338 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        2 | 1339 | `				&pOwner->sName,&pAttr->sName);` |
|        - | 1340 | `		}` |
|        - | 1341 | `	}` |
|       55 | 1342 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1343 | `}` |
|        - | 1344 | `/*` |
|        - | 1345 | ` * INDIRECT modification: this SLOT is about to be reached as something other than` |
|        - | 1346 | `` * a plain store -- aliased by `=&`, handed to a by-reference parameter, walked by`` |
|        - | 1347 | ` * a by-reference foreach, or used as the BASE of a subscript write. php screens` |
|        - | 1348 | ` * every one of them where it screens a store, because the alias outlives the` |
|        - | 1349 | ` * statement and the next write through it would reach the property with no` |
|        - | 1350 | ` * handler and no readonly latch in the way.` |
|        - | 1351 | ` *` |
|        - | 1352 | `` * Two sentences. A php-readonly property gets its own: `Cannot indirectly modify`` |
|        - | 1353 | `` * readonly property C::$p`, raised whatever the scope and whether or not the`` |
|        - | 1354 | ` * property has been initialized -- the reference is refused before the` |
|        - | 1355 | ` * uninitialized read is. A NATIVE class whose handler refuses every write` |
|        - | 1356 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE) gets that handler's own sentence, the one a` |
|        - | 1357 | ` * plain store to it gets.` |
|        - | 1358 | ` *` |
|        - | 1359 | ` * Answers SXRET_OK to proceed, or the throw status. Asked by the sites that reach` |
|        - | 1360 | ` * a property through its memobj index rather than through its declaration.` |
|        - | 1361 | ` */` |
|    11452 | 1362 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1363 | `{` |
|        - | 1364 | `	SyHashEntry *pSlot;` |
|        - | 1365 | `	VmClassAttr *pVmAttr;` |
|        - | 1366 | `	ph7_class_attr *pAttr;` |
|    11457 | 1367 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|     5141 | 1368 | `		return SXRET_OK;` |
|        - | 1369 | `	}` |
|     6321 | 1370 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6321 | 1371 | `	if( pSlot == 0 ){` |
|     6249 | 1372 | `		return SXRET_OK;` |
|        - | 1373 | `	}` |
|       75 | 1374 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       75 | 1375 | `	pAttr = pVmAttr->pAttr;` |
|       75 | 1376 | `	if( pAttr == 0 ){` |
|      ! 0 | 1377 | `		return SXRET_OK;` |
|        - | 1378 | `	}` |
|       75 | 1379 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       13 | 1380 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pAttr);` |
|        - | 1381 | `	}` |
|       63 | 1382 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       29 | 1383 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - | 1384 | `		SyBlob sMsg;` |
|       29 | 1385 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       29 | 1386 | `		SyBlobFormat(&sMsg,"Cannot indirectly modify readonly property %z::$%z",` |
|       14 | 1387 | `			&pOwner->sName,&pAttr->sName);` |
|       29 | 1388 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1389 | `	}` |
|       35 | 1390 | `	return SXRET_OK;` |
|     5731 | 1391 | `}` |
|        - | 1392 | `/*` |
|        - | 1393 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - | 1394 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - | 1395 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - | 1396 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - | 1397 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - | 1398 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - | 1399 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - | 1400 | ` */` |
|   729739 | 1401 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1402 | `{` |
|        - | 1403 | `	SyHashEntry *pSlot;` |
|        - | 1404 | `	VmClassAttr *pVmAttr;` |
|   729744 | 1405 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|   452741 | 1406 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - | 1407 | `	}` |
|   277008 | 1408 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   277008 | 1409 | `	if( pSlot == 0 ){` |
|   276696 | 1410 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 1411 | `	}` |
|      316 | 1412 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      316 | 1413 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|        5 | 1414 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|        - | 1415 | `	}` |
|      312 | 1416 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 | 1417 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|        - | 1418 | `	}` |
|      298 | 1419 | `	if( pVmAttr->pAttr` |
|      300 | 1420 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|        - | 1421 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|        7 | 1422 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|        - | 1423 | `	}` |
|      294 | 1424 | `	return SXRET_OK;` |
|   365435 | 1425 | `}` |
|        - | 1426 | `/*` |
|        - | 1427 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - | 1428 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - | 1429 | ` * For class types, instanceof is verified.` |
|        - | 1430 | ` *` |
|        - | 1431 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - | 1432 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - | 1433 | ` */` |
|        - | 1434 |  |
|        - | 1435 | `/*` |
|        - | 1436 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - | 1437 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - | 1438 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - | 1439 | ` *   0 if it's not strictly numeric.` |
|        - | 1440 | ` */` |
|       38 | 1441 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        3 | 1442 | `{` |
|        - | 1443 | `	const char *z, *zEnd, *zTail;` |
|        - | 1444 | `	sxu32 n;` |
|       41 | 1445 | `	sxu8 bReal = 0;` |
|        - | 1446 | `	sxi32 rc;` |
|       41 | 1447 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       24 | 1448 | `		return 0;` |
|        - | 1449 | `	}` |
|       18 | 1450 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 | 1451 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 | 1452 | `	zEnd = z + n;` |
|       18 | 1453 | `	if( n == 0 ) return 0;` |
|       18 | 1454 | `	zTail = 0;` |
|       18 | 1455 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 | 1456 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 | 1457 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 | 1458 | `	if( zTail != zEnd ) return 0;` |
|       15 | 1459 | `	return bReal ? 2 : 1;` |
|       22 | 1460 | `}` |
|        - | 1461 |  |
|        - | 1462 | `/*` |
|        - | 1463 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - | 1464 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|        - | 1465 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|        - | 1466 | ` * return, parameter, property, union alternative — would have to string-match` |
|        - | 1467 | ` * the name itself.` |
|        - | 1468 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - | 1469 | ` * to extend when another literal/pseudo type is added.` |
|        - | 1470 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - | 1471 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - | 1472 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - | 1473 | ` */` |
|     4506 | 1474 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        5 | 1475 | `{` |
|     4511 | 1476 | `	const char *z = pClass->zString;` |
|     4511 | 1477 | `	sxu32 n = pClass->nByte;` |
|     4511 | 1478 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      216 | 1479 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - | 1480 | `	}` |
|     4299 | 1481 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       28 | 1482 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - | 1483 | `	}` |
|     4273 | 1484 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       51 | 1485 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - | 1486 | `	}` |
|     4225 | 1487 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|        - | 1488 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|        - | 1489 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|        - | 1490 | `		 * object; scope-sensitive, so a private method is callable only from` |
|        - | 1491 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|        - | 1492 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|        - | 1493 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|        - | 1494 | `		 * php does.) */` |
|     2865 | 1495 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|        - | 1496 | `	}` |
|     1365 | 1497 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - | 1498 | `		/* iterable === array \| Traversable */` |
|       69 | 1499 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1500 | `			return 1;` |
|        - | 1501 | `		}` |
|       57 | 1502 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       29 | 1503 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       29 | 1504 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       13 | 1505 | `				return 1;` |
|        - | 1506 | `			}` |
|        7 | 1507 | `		}` |
|       45 | 1508 | `		return 0;` |
|        - | 1509 | `	}` |
|     1299 | 1510 | `	return -1;` |
|     2258 | 1511 | `}` |
|        - | 1512 | `/*` |
|        - | 1513 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|        - | 1514 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|        - | 1515 | ` * through. php binds the keyword where the hint is written, so` |
|        - | 1516 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|        - | 1517 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|        - | 1518 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|        - | 1519 | ` * throw a TypeError over perfectly valid code.` |
|        - | 1520 | ` *` |
|        - | 1521 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|        - | 1522 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|        - | 1523 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|        - | 1524 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|        - | 1525 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|        - | 1526 | `` * VmResolveTypeClass applies to `parent`.`` |
|        - | 1527 | ` */` |
|   212700 | 1528 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|        5 | 1529 | `{` |
|   212705 | 1530 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   203259 | 1531 | `		return pDecl;` |
|        - | 1532 | `	}` |
|     9451 | 1533 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|   106352 | 1534 | `}` |
|        - | 1535 | `/*` |
|        - | 1536 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - | 1537 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - | 1538 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - | 1539 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - | 1540 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - | 1541 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - | 1542 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - | 1543 | ` * throw.` |
|        - | 1544 | ` *` |
|        - | 1545 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|        - | 1546 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|        - | 1547 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|        - | 1548 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|        - | 1549 | ` */` |
|        - | 1550 | `/*` |
|        - | 1551 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|        - | 1552 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|        - | 1553 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|        - | 1554 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|        - | 1555 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|        - | 1556 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|        - | 1557 | ` * iLoadable flag — lives in one place.` |
|        - | 1558 | ` *` |
|        - | 1559 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|        - | 1560 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|        - | 1561 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|        - | 1562 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|        - | 1563 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|        - | 1564 | ` *` |
|        - | 1565 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|        - | 1566 | ` * compatibility target, where the type may legitimately be an interface or` |
|        - | 1567 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|        - | 1568 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|        - | 1569 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|        - | 1570 | ` * with TRUE; it does not go through this helper.)` |
|        - | 1571 | ` */` |
|     1370 | 1572 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|        5 | 1573 | `{` |
|     1375 | 1574 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      100 | 1575 | `		return pSelf;` |
|        - | 1576 | `	}` |
|     1279 | 1577 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|        - | 1578 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|        - | 1579 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|        - | 1580 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|        - | 1581 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|        - | 1582 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|       40 | 1583 | `		return PH7_VmPeekTopClass(pVm);` |
|        - | 1584 | `	}` |
|     1243 | 1585 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|        - | 1586 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|        - | 1587 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|       21 | 1588 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      ! 0 | 1589 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|      ! 0 | 1590 | `		}` |
|       21 | 1591 | `		return pSelf ? pSelf->pBase : 0;` |
|        - | 1592 | `	}` |
|     1225 | 1593 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|      690 | 1594 | `}` |
|        - | 1595 | `/*` |
|        - | 1596 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|        - | 1597 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|        - | 1598 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|        - | 1599 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|        - | 1600 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|        - | 1601 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|        - | 1602 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|        - | 1603 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|        - | 1604 | ` */` |
|   100542 | 1605 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|        5 | 1606 | `{` |
|   100559 | 1607 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|   100531 | 1608 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|   150813 | 1609 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|        5 | 1610 | `}` |
|        - | 1611 | `/*` |
|        - | 1612 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|        - | 1613 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|        - | 1614 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|        - | 1615 | ` * typed-default checks — each of which then formats its own message. The` |
|        - | 1616 | ` * resolved class is handed back through *ppResolved for the message builder` |
|        - | 1617 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|        - | 1618 | ` * nothing resolved).` |
|        - | 1619 | ` *` |
|        - | 1620 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|        - | 1621 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|        - | 1622 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|        - | 1623 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|        - | 1624 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|        - | 1625 | ` * resolve against there is no class to compare to — a position php rejects at` |
|        - | 1626 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|        - | 1627 | ` *` |
|        - | 1628 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|        - | 1629 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|        - | 1630 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|        - | 1631 | ` * autoloader can produce fails.` |
|        - | 1632 | ` */` |
|     1178 | 1633 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|        - | 1634 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|        5 | 1635 | `{` |
|     1183 | 1636 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     1183 | 1637 | `	*ppResolved = pExpected;` |
|     1183 | 1638 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       43 | 1639 | `		return 0;` |
|        - | 1640 | `	}` |
|     1145 | 1641 | `	if( pExpected == 0 ){` |
|       13 | 1642 | `		return VmHintIsScopeKeyword(pName);` |
|        - | 1643 | `	}` |
|     1133 | 1644 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|      594 | 1645 | `}` |
|        - | 1646 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|        - | 1647 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|   403248 | 1648 | `static int VmHintNameChar(int c)` |
|        5 | 1649 | `{` |
|   806115 | 1650 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|   402868 | 1651 | `		\|\| c == ' ' \|\| c == '\t');` |
|        5 | 1652 | `}` |
|        - | 1653 | `/*` |
|        - | 1654 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|        - | 1655 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|        - | 1656 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|        - | 1657 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|        - | 1658 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|        - | 1659 | ` * resolved until the call site are substituted.` |
|        - | 1660 | ` *` |
|        - | 1661 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|        - | 1662 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|        - | 1663 | ` * are only ever available as source: unions/intersections, and the property /` |
|        - | 1664 | ` * class-constant messages, which print the declared type whatever its shape.` |
|        - | 1665 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|        - | 1666 | ` *` |
|        - | 1667 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|        - | 1668 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|        - | 1669 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|        - | 1670 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|        - | 1671 | ` * names the two types it stands for. Only the standalone spellings can still` |
|        - | 1672 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|        - | 1673 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|        - | 1674 | ` */` |
|   100362 | 1675 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 1676 | `	char *zBuf,sxu32 nBuf)` |
|        5 | 1677 | `{` |
|        - | 1678 | `	const char *z;` |
|   100367 | 1679 | `	sxu32 n, i = 0, nAt = 0;` |
|   100367 | 1680 | `	if( nBuf == 0 ){` |
|      ! 0 | 1681 | `		return "";` |
|        - | 1682 | `	}` |
|   100367 | 1683 | `	z = pDeclared ? pDeclared->zString : 0;` |
|   100367 | 1684 | `	n = z ? pDeclared->nByte : 0;` |
|   100367 | 1685 | `	if( z ){` |
|   100367 | 1686 | `		const char *zIter = 0;` |
|   100367 | 1687 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        9 | 1688 | `			zIter = "Traversable\|array";` |
|   100364 | 1689 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        3 | 1690 | `			zIter = "Traversable\|array\|null";` |
|        1 | 1691 | `		}` |
|   100367 | 1692 | `		if( zIter ){` |
|       11 | 1693 | `			sxu32 nIter = SyStrlen(zIter);` |
|       11 | 1694 | `			if( nIter > nBuf - 1 ){` |
|      ! 0 | 1695 | `				nIter = nBuf - 1;` |
|      ! 0 | 1696 | `			}` |
|       11 | 1697 | `			SyMemcpy(zIter,zBuf,nIter);` |
|       11 | 1698 | `			zBuf[nIter] = 0;` |
|       11 | 1699 | `			return zBuf;` |
|        - | 1700 | `		}` |
|    50177 | 1701 | `	}` |
|   201099 | 1702 | `	while( i < n && nAt + 1 < nBuf ){` |
|        - | 1703 | `		sxu32 nStart, nCopy;` |
|        - | 1704 | `		SyString sTok;` |
|        - | 1705 | `		const SyString *pOut;` |
|   100745 | 1706 | `		if( !VmHintNameChar(z[i]) ){` |
|      215 | 1707 | `			zBuf[nAt++] = z[i++];` |
|      215 | 1708 | `			continue;` |
|        - | 1709 | `		}` |
|   100535 | 1710 | `		nStart = i;` |
|   402867 | 1711 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|   302337 | 1712 | `			i++;` |
|        5 | 1713 | `		}` |
|   100535 | 1714 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|   100535 | 1715 | `		pOut = &sTok;` |
|   100535 | 1716 | `		if( VmHintIsScopeKeyword(&sTok) ){` |
|       33 | 1717 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|       33 | 1718 | `			if( pRes ){` |
|       33 | 1719 | `				pOut = &pRes->sName;` |
|       15 | 1720 | `			}` |
|       15 | 1721 | `		}` |
|   100535 | 1722 | `		nCopy = pOut->nByte;` |
|   100535 | 1723 | `		if( nCopy > nBuf - nAt - 1 ){` |
|      ! 0 | 1724 | `			nCopy = nBuf - nAt - 1;` |
|      ! 0 | 1725 | `		}` |
|   100535 | 1726 | `		if( nCopy > 0 ){` |
|   100535 | 1727 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|   100535 | 1728 | `			nAt += nCopy;` |
|    50265 | 1729 | `		}` |
|        5 | 1730 | `	}` |
|   100359 | 1731 | `	zBuf[nAt] = 0;` |
|   100359 | 1732 | `	return zBuf;` |
|    50186 | 1733 | `}` |
|        - | 1734 | `/*` |
|        - | 1735 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|        - | 1736 | ` * float-identity leniency — see the typed-constant note above` |
|        - | 1737 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|        - | 1738 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|        - | 1739 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|        - | 1740 | ` * so an int-typed parameter / return / property / union member silently` |
|        - | 1741 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|        - | 1742 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|        - | 1743 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|        - | 1744 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|        - | 1745 | ` * present).` |
|        - | 1746 | ` *` |
|        - | 1747 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|        - | 1748 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|        - | 1749 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|        - | 1750 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|        - | 1751 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|        - | 1752 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|        - | 1753 | ` */` |
|    47222 | 1754 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|        5 | 1755 | `{` |
|    47222 | 1756 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|    22637 | 1757 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - | 1758 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|        - | 1759 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|        - | 1760 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|        - | 1761 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|       54 | 1762 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|       54 | 1763 | `		SyBlobRelease(&pVal->sBlob);` |
|       54 | 1764 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|       25 | 1765 | `	}` |
|    47227 | 1766 | `}` |
|      336 | 1767 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|        - | 1768 | `	ph7_class *pSelf)` |
|        5 | 1769 | `{` |
|        - | 1770 | `	sxu32 i;` |
|        - | 1771 | `	sxu32 nAlts;` |
|        - | 1772 | `	ph7_type_alt *aAlts;` |
|        - | 1773 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - | 1774 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      341 | 1775 | `	int bHasIntersection = 0;` |
|        - | 1776 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|      341 | 1777 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       20 | 1778 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - | 1779 | `	}` |
|      325 | 1780 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|      325 | 1781 | `	nAlts = SySetUsed(pAlts);` |
|        - | 1782 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|        - | 1783 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|        - | 1784 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|        - | 1785 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|        - | 1786 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|    10565 | 1787 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     1001 | 1788 | `	for( i = 0; i < nAlts; i++ ){` |
|      681 | 1789 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|       41 | 1790 | `			bHasIntersection = 1;` |
|       19 | 1791 | `		}` |
|      343 | 1792 | `	}` |
|        - | 1793 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|        - | 1794 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|        - | 1795 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|        - | 1796 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|      325 | 1797 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|       35 | 1798 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 1799 | `		sxu32 g;` |
|      421 | 1800 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|        - | 1801 | `			int bAll;` |
|      409 | 1802 | `			if( aGroupCount[g] < 2 ) continue;` |
|       35 | 1803 | `			bAll = 1;` |
|       87 | 1804 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 1805 | `				ph7_class *pExpected;` |
|       67 | 1806 | `				if( aAlts[i].nGroup != g ) continue;` |
|       63 | 1807 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|       63 | 1808 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|       63 | 1809 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       15 | 1810 | `					bAll = 0;` |
|       15 | 1811 | `					break;` |
|        - | 1812 | `				}` |
|       27 | 1813 | `			}` |
|       35 | 1814 | `			if( bAll ) return SXRET_OK;` |
|        9 | 1815 | `		}` |
|        6 | 1816 | `	}` |
|        - | 1817 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - | 1818 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - | 1819 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - | 1820 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|      921 | 1821 | `	for( i = 0; i < nAlts; i++ ){` |
|      635 | 1822 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      594 | 1823 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      407 | 1824 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|       16 | 1825 | `			return SXRET_OK;` |
|        - | 1826 | `		}` |
|      295 | 1827 | `	}` |
|      291 | 1828 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|      291 | 1829 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      895 | 1830 | `	for( i = 0; i < nAlts; i++ ){` |
|      609 | 1831 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      573 | 1832 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      389 | 1833 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      385 | 1834 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      379 | 1835 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|      167 | 1836 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|      127 | 1837 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|        5 | 1838 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|      289 | 1839 | `	}` |
|        - | 1840 | `	/* Object handling */` |
|      291 | 1841 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       99 | 1842 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       99 | 1843 | `		if( bHasClassAlt ){` |
|       85 | 1844 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      217 | 1845 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 1846 | `				ph7_class *pExpected;` |
|      161 | 1847 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      153 | 1848 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      107 | 1849 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      107 | 1850 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       28 | 1851 | `					return SXRET_OK;` |
|        - | 1852 | `				}` |
|       44 | 1853 | `			}` |
|       28 | 1854 | `		}` |
|       74 | 1855 | `		return SXERR_INVALID;` |
|        - | 1856 | `	}` |
|        - | 1857 | `	/* Array handling */` |
|      197 | 1858 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1859 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - | 1860 | `	}` |
|        - | 1861 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|        - | 1862 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|        - | 1863 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|        - | 1864 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|        - | 1865 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|        - | 1866 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|      185 | 1867 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|       22 | 1868 | `		if( bHasFloat ) return SXRET_OK;` |
|        5 | 1869 | `	}` |
|      177 | 1870 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      119 | 1871 | `		if( bHasInt ){` |
|       97 | 1872 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|       97 | 1873 | `			return SXRET_OK;` |
|        - | 1874 | `		}` |
|       11 | 1875 | `	}` |
|       85 | 1876 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       59 | 1877 | `		if( bHasString ) return SXRET_OK;` |
|        8 | 1878 | `	}` |
|       45 | 1879 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|        3 | 1880 | `		if( bHasBool ) return SXRET_OK;` |
|        1 | 1881 | `	}` |
|       45 | 1882 | `	if( bStrict ){` |
|        - | 1883 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|        5 | 1884 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 | 1885 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 | 1886 | `			return SXRET_OK;` |
|        - | 1887 | `		}` |
|        5 | 1888 | `		return SXERR_INVALID;` |
|        - | 1889 | `	}` |
|        - | 1890 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - | 1891 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - | 1892 | `	 * to match PHP's union RFC. */` |
|        - | 1893 | `	{` |
|       41 | 1894 | `		int kind = VmStringNumericKind(pValue);` |
|       41 | 1895 | `		if( bHasInt ){` |
|        - | 1896 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - | 1897 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 | 1898 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1899 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 | 1900 | `				return SXRET_OK;` |
|        - | 1901 | `			}` |
|       18 | 1902 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1903 | `				ph7_real r = pValue->rVal;` |
|        - | 1904 | ``				/* Range first: `(sxi64)r` is undefined outside it (§2), and NaN`` |
|        - | 1905 | `				 * and the infinities are not exact ints either way. */` |
|        - | 1906 | `				/* (double)r: see VmValueIsLossyToInt -- a no-op where ph7_real` |
|        - | 1907 | `				 * is double, and the narrowing MSVC turns into an error under` |
|        - | 1908 | `				 * PH7_OMIT_FLOATING_POINT otherwise. */` |
|      ! 0 | 1909 | `				if( PH7_RealFitsInt64((double)r) && r == (ph7_real)(sxi64)r ){` |
|      ! 0 | 1910 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 | 1911 | `					return SXRET_OK;` |
|        - | 1912 | `				}` |
|      ! 0 | 1913 | `			}` |
|       18 | 1914 | `			if( kind == 1 ){` |
|        9 | 1915 | `				PH7_MemObjToInteger(pValue);` |
|        9 | 1916 | `				return SXRET_OK;` |
|        - | 1917 | `			}` |
|        4 | 1918 | `		}` |
|       33 | 1919 | `		if( bHasFloat ){` |
|       10 | 1920 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 | 1921 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 | 1922 | `				return SXRET_OK;` |
|        - | 1923 | `			}` |
|       10 | 1924 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 | 1925 | `				PH7_MemObjToReal(pValue);` |
|        7 | 1926 | `				return SXRET_OK;` |
|        - | 1927 | `			}` |
|        1 | 1928 | `		}` |
|       26 | 1929 | `		if( bHasString ){` |
|      ! 0 | 1930 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 | 1931 | `				PH7_MemObjToString(pValue);` |
|      ! 0 | 1932 | `				return SXRET_OK;` |
|        - | 1933 | `			}` |
|      ! 0 | 1934 | `		}` |
|       26 | 1935 | `		if( bHasBool ){` |
|        3 | 1936 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|        3 | 1937 | `				PH7_MemObjToBool(pValue);` |
|        3 | 1938 | `				return SXRET_OK;` |
|        - | 1939 | `			}` |
|      ! 0 | 1940 | `		}` |
|        - | 1941 | `	}` |
|       24 | 1942 | `	return SXERR_INVALID;` |
|      173 | 1943 | `}` |
|        - | 1944 |  |
|        - | 1945 | `/*` |
|        - | 1946 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - | 1947 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - | 1948 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - | 1949 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - | 1950 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - | 1951 | ` */` |
|      334 | 1952 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        5 | 1953 | `{` |
|        - | 1954 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - | 1955 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - | 1956 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - | 1957 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - | 1958 | `	 * silently swallow any argument. */` |
|      339 | 1959 | `	if( nType == MEMOBJ_NULL ){` |
|        3 | 1960 | `		return SXERR_INVALID;` |
|        - | 1961 | `	}` |
|        - | 1962 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|        - | 1963 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|        - | 1964 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|        - | 1965 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|        - | 1966 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|        - | 1967 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|        - | 1968 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|        - | 1969 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|        - | 1970 | `	 * object value against an array type is caught here too (never valid);` |
|        - | 1971 | `	 * object->scalar stays a separate case handled by the callers. */` |
|      337 | 1972 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|       37 | 1973 | `		return SXERR_INVALID;` |
|        - | 1974 | `	}` |
|      303 | 1975 | `	if( bStrict ){` |
|        - | 1976 | `		/* Only int -> float widening is allowed implicitly. */` |
|       36 | 1977 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 | 1978 | `			PH7_MemObjToReal(pVal);` |
|        3 | 1979 | `			return SXRET_OK;` |
|        - | 1980 | `		}` |
|       34 | 1981 | `		return SXERR_INVALID;` |
|        - | 1982 | `	}` |
|        - | 1983 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|        - | 1984 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|        - | 1985 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|        - | 1986 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|        - | 1987 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|        - | 1988 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|        - | 1989 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|      271 | 1990 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       20 | 1991 | `		return SXERR_INVALID;` |
|        - | 1992 | `	}` |
|        - | 1993 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|        - | 1994 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|        - | 1995 | `	 * is invoked by the string cast below). Every other scalar target —` |
|        - | 1996 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|        - | 1997 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|        - | 1998 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|        - | 1999 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|        - | 2000 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|        - | 2001 | `	 * is caught by the array guard.) */` |
|      253 | 2002 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       23 | 2003 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       28 | 2004 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       10 | 2005 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       13 | 2006 | `			return SXERR_INVALID;` |
|        - | 2007 | `		}` |
|        4 | 2008 | `	}` |
|      236 | 2009 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      191 | 2010 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      177 | 2011 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|       49 | 2012 | `		return SXERR_INVALID;` |
|        - | 2013 | `	}` |
|      195 | 2014 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|        - | 2015 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|        - | 2016 | `		 * PHL rejects it (§10). SXERR_INVALID routes to the caller's TypeError,` |
|        - | 2017 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|        - | 2018 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|        - | 2019 | `		 * for the typed parameters and returns that reach here, for the typed` |
|        - | 2020 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|        - | 2021 | `		 * integer-only operators. */` |
|      ! 0 | 2022 | `		return SXERR_INVALID;` |
|        - | 2023 | `	}` |
|      190 | 2024 | `	if( nType == MEMOBJ_STRING && (pVal->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       56 | 2025 | `	 && pVal->pVm && PH7_IS_NAN(pVal->rVal) ){` |
|        - | 2026 | ``		/* A userland `string` parameter, return or property taking a NaN: php's`` |
|        - | 2027 | `		 * weak coercion warns there exactly as its ZPP does for an internal one` |
|        - | 2028 | ``		 * (`unexpected NAN value was coerced to string`). The cast below is the`` |
|        - | 2029 | `		 * silent conversion -- it is shared with the engine's own -- so the` |
|        - | 2030 | `		 * diagnostic is raised here, where the DECLARED type is known. */` |
|        3 | 2031 | `		VmErrorFormat(pVal->pVm,PH7_CTX_WARNING,` |
|        - | 2032 | `			"unexpected NAN value was coerced to string");` |
|        1 | 2033 | `	}` |
|        - | 2034 | `	{` |
|      195 | 2035 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      195 | 2036 | `		if( xCast ) xCast(pVal);` |
|        - | 2037 | `	}` |
|      195 | 2038 | `	return SXRET_OK;` |
|      172 | 2039 | `}` |
|        - | 2040 |  |
|        - | 2041 | `/*` |
|        - | 2042 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - | 2043 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - | 2044 | ` *` |
|        - | 2045 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - | 2046 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - | 2047 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - | 2048 | ` * back to a static literal and ignore zBuf entirely.` |
|        - | 2049 | ` */` |
|      202 | 2050 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        5 | 2051 | `{` |
|      207 | 2052 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|      207 | 2053 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|      207 | 2054 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      207 | 2055 | `		if( pDeclared->zString && nCopy > 0 ){` |
|      207 | 2056 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      101 | 2057 | `		}` |
|      207 | 2058 | `		zBuf[nCopy] = 0;` |
|      207 | 2059 | `		return zBuf;` |
|        - | 2060 | `	}` |
|      ! 0 | 2061 | `	switch( nType ){` |
|      ! 0 | 2062 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 | 2063 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 | 2064 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 | 2065 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 | 2066 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 | 2067 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 | 2068 | `		default:             return "scalar";` |
|        - | 2069 | `	}` |
|      106 | 2070 | `}` |
|        - | 2071 |  |
|        - | 2072 | `/*` |
|        - | 2073 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|        - | 2074 | ` * the way php writes it in a TypeError:` |
|        - | 2075 | ` *` |
|        - | 2076 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|        - | 2077 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|        - | 2078 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|        - | 2079 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|        - | 2080 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|        - | 2081 | ` *` |
|        - | 2082 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|        - | 2083 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|        - | 2084 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|        - | 2085 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|        - | 2086 | ` */` |
|      174 | 2087 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|        - | 2088 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|        5 | 2089 | `{` |
|      179 | 2090 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|        - | 2091 | `	sxu32 nCopy;` |
|      179 | 2092 | `	sxu32 nAt = 0;` |
|      179 | 2093 | `	if( nBuf == 0 ){` |
|      ! 0 | 2094 | `		return "";` |
|        - | 2095 | `	}` |
|      174 | 2096 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|       79 | 2097 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|       21 | 2098 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|       21 | 2099 | `		nCopy = SyStrlen(zIter);` |
|       21 | 2100 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       21 | 2101 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|       21 | 2102 | `		zBuf[nCopy] = 0;` |
|       21 | 2103 | `		return zBuf;` |
|        - | 2104 | `	}` |
|      161 | 2105 | `	if( bNullable && nBuf > 1 ){` |
|       23 | 2106 | `		zBuf[nAt++] = '?';` |
|       10 | 2107 | `	}` |
|      161 | 2108 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|      161 | 2109 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|      161 | 2110 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|      161 | 2111 | `	zBuf[nAt + nCopy] = 0;` |
|      161 | 2112 | `	return zBuf;` |
|       92 | 2113 | `}` |
|        - | 2114 |  |
|        - | 2115 | `/*` |
|        - | 2116 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - | 2117 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - | 2118 | ` */` |
|      142 | 2119 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        5 | 2120 | `{` |
|      147 | 2121 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      218 | 2122 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      142 | 2123 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      147 | 2124 | `	return zBuf;` |
|        5 | 2125 | `}` |
|        - | 2126 |  |
|        - | 2127 | `/*` |
|        - | 2128 | ` * php's write_property handler (ph7_class::xSet): a native class whose properties` |
|        - | 2129 | ` * are its own C struct converts the incoming value the way that struct demands —` |
|        - | 2130 | ` * and may refuse the write outright. The value is rewritten IN PLACE, so what the` |
|        - | 2131 | ` * caller goes on to store is what the hook left behind.` |
|        - | 2132 | ` */` |
|      616 | 2133 | `static sxi32 VmRunNativeSet(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pValue)` |
|        3 | 2134 | `{` |
|        - | 2135 | `	PH7_NativeSetCtx sSet;` |
|      619 | 2136 | `	if( pVmAttr->pInst == 0 ){` |
|      ! 0 | 2137 | `		return SXRET_OK;   /* a class static: no object for a handler to run on */` |
|        - | 2138 | `	}` |
|      619 | 2139 | `	sSet.pName = &pVmAttr->pAttr->sName;` |
|      619 | 2140 | `	sSet.pValue = pValue;` |
|      619 | 2141 | `	sSet.zThrowClass = 0;` |
|      619 | 2142 | `	sSet.zThrowMsg[0] = 0;` |
|      619 | 2143 | `	if( PH7_ClassNativeSet(pVmAttr->pInst,&sSet) && sSet.zThrowClass ){` |
|        5 | 2144 | `		return VmThrowFixedError(pVm,sSet.zThrowClass,sSet.zThrowMsg);` |
|        - | 2145 | `	}` |
|      615 | 2146 | `	return SXRET_OK;` |
|      311 | 2147 | `}` |
|        - | 2148 | `/*` |
|        - | 2149 | ` * The same handler, asked of a SLOT that an opcode has already mutated in place.` |
|        - | 2150 | `` * `$i->f++` and `$i->f--` never pass a value through the store filter — they`` |
|        - | 2151 | ` * increment the slot where it lies — so the conversion has to be applied after` |
|        - | 2152 | ` * the fact, which is exactly what php does (it reads, increments, and writes` |
|        - | 2153 | `` * back through the handler: `$i->f = 1.456008; ++$i->f` leaves the property at`` |
|        - | 2154 | ` * 2.456007, the microsecond truncation of the sum). Answers SXRET_OK when the` |
|        - | 2155 | ` * slot is not a native one.` |
|        - | 2156 | ` */` |
|        - | 2157 | `/*` |
|        - | 2158 | ` * Register a property slot with the store filter, and drop it again. These two` |
|        - | 2159 | ` * are the ONLY writers of pVm->hTypedSlot: the predicate that decides membership` |
|        - | 2160 | ` * lives here once (a declared type, a native write handler, or both), and the` |
|        - | 2161 | ` * handler COUNT that lets the mutation opcodes skip the table entirely is kept` |
|        - | 2162 | ` * beside it -- registering in one place and forgetting to drop in another is` |
|        - | 2163 | ` * exactly how a recycled memobj index would inherit a stale entry.` |
|        - | 2164 | ` */` |
| 10394473 | 2165 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        5 | 2166 | `{` |
| 10394478 | 2167 | `	if( !PH7_ATTR_STORE_FILTERED(pVmAttr->pAttr) ){` |
|  3047651 | 2168 | `		return SXRET_OK;` |
|        - | 2169 | `	}` |
|  7346832 | 2170 | `	if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 2171 | `		return SXERR_MEM;` |
|        - | 2172 | `	}` |
|  7346832 | 2173 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|     5691 | 2174 | `		pVm->nNativeSetSlot++;` |
|     2844 | 2175 | `	}` |
|  7346832 | 2176 | `	return SXRET_OK;` |
|  5197238 | 2177 | `}` |
|  9617665 | 2178 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx)` |
|        5 | 2179 | `{` |
|  9617670 | 2180 | `	if( pAttr == 0 \|\| !PH7_ATTR_STORE_FILTERED(pAttr) ){` |
|  2787457 | 2181 | `		return;` |
|        - | 2182 | `	}` |
|  6830213 | 2183 | `	if( SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32),0) == SXRET_OK` |
|  6830218 | 2184 | `	 && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) && pVm->nNativeSetSlot > 0 ){` |
|     5002 | 2185 | `		pVm->nNativeSetSlot--;` |
|     2500 | 2186 | `	}` |
|  4808834 | 2187 | `}` |
|   729627 | 2188 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 | 2189 | `{` |
|        - | 2190 | `	SyHashEntry *pSlot;` |
|        - | 2191 | `	VmClassAttr *pVmAttr;` |
|   729632 | 2192 | `	if( nIdx == SXU32_HIGH \|\| pVm->nNativeSetSlot == 0 ){` |
|   592626 | 2193 | `		return SXRET_OK;` |
|        - | 2194 | `	}` |
|   137007 | 2195 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   137007 | 2196 | `	if( pSlot == 0 ){` |
|   136991 | 2197 | `		return SXRET_OK;` |
|        - | 2198 | `	}` |
|       17 | 2199 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       17 | 2200 | `	if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) == 0 ){` |
|       11 | 2201 | `		return SXRET_OK;` |
|        - | 2202 | `	}` |
|        7 | 2203 | `	return VmRunNativeSet(pVm,pVmAttr,pValue);` |
|   365379 | 2204 | `}` |
|   534024 | 2205 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|        5 | 2206 | `{` |
|        - | 2207 | `	SyHashEntry *pSlot;` |
|        - | 2208 | `	VmClassAttr *pVmAttr;` |
|        - | 2209 | `	ph7_class_attr *pAttr;` |
|        - | 2210 | `	ph7_class *pHintScope;` |
|        - | 2211 | `	char zGivenBuf[128];` |
|        - | 2212 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|        - | 2213 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|        - | 2214 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|        - | 2215 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|   534029 | 2216 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|   534029 | 2217 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   534029 | 2218 | `	if( pSlot == 0 ){` |
|   432183 | 2219 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2220 | `	}` |
|   101851 | 2221 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   101851 | 2222 | `	pAttr = pVmAttr->pAttr;` |
|   101851 | 2223 | `	if( pAttr == 0 ){` |
|      ! 0 | 2224 | `		return SXRET_OK;` |
|        - | 2225 | `	}` |
|   101851 | 2226 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - | 2227 | `		/* php's write_property handler for this class refuses outright, and its` |
|        - | 2228 | `		 * sentence is the readonly one -- without the readonly FLAG, which is why` |
|        - | 2229 | `		 * Reflection still reports isReadOnly() false for DatePeriod's seven. The` |
|        - | 2230 | `		 * C bodies that fill them write the slot directly and never come here. */` |
|       23 | 2231 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pAttr);` |
|        - | 2232 | `	}` |
|   101829 | 2233 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|      613 | 2234 | `		sxi32 rcNat = VmRunNativeSet(pVm,pVmAttr,pValue);` |
|      613 | 2235 | `		if( rcNat != SXRET_OK ){` |
|        5 | 2236 | `			return rcNat;` |
|        - | 2237 | `		}` |
|      303 | 2238 | `	}` |
|   101825 | 2239 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      609 | 2240 | `		return SXRET_OK;` |
|        - | 2241 | `	}` |
|        - | 2242 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|        - | 2243 | `	 * the property (a trait's members count as the composing class), not the` |
|        - | 2244 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|   101219 | 2245 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 2246 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - | 2247 | `	 * property may be written exactly once and only from within the declaring` |
|        - | 2248 | `	 * class scope (its set-scope is protected). */` |
|   101219 | 2249 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2250 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - | 2251 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - | 2252 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - | 2253 | `		 * leaves it set, so a later valid initialization still works). */` |
|      123 | 2254 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - | 2255 | `			/* Already initialized: any further write is forbidden, any scope —` |
|        - | 2256 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|        - | 2257 | `			 * Exceptions that fall through to the set-scope check below:` |
|        - | 2258 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|        - | 2259 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|        - | 2260 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|       43 | 2261 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       43 | 2262 | `			if( !(pCloneFr && pCloneFr->pThis` |
|       21 | 2263 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|       41 | 2264 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|        - | 2265 | `			}` |
|        1 | 2266 | `		}` |
|       41 | 2267 | `	}` |
|   101183 | 2268 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 2269 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|        - | 2270 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|       27 | 2271 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|       27 | 2272 | `		if( rcVis != SXRET_OK ){` |
|       13 | 2273 | `			return rcVis;` |
|        1 | 2274 | `		}` |
|   101164 | 2275 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2276 | `		/* First write (or a clone re-init) must come from within the declaring` |
|        - | 2277 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|       85 | 2278 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       85 | 2279 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 2280 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|        - | 2281 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|        - | 2282 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|       85 | 2283 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|        5 | 2284 | `			pDecl = pVmAttr->pOwner;` |
|        2 | 2285 | `		}` |
|       85 | 2286 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        6 | 2287 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|        - | 2288 | `		}` |
|       38 | 2289 | `	}` |
|        - | 2290 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|        - | 2291 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|        - | 2292 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|        - | 2293 | `	 * is its TypeError there), which this used to deny outright. */` |
|   101167 | 2294 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       92 | 2295 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       58 | 2296 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       29 | 2297 | `			bStrict,pHintScope);` |
|       63 | 2298 | `		if( rc == SXRET_OK ){` |
|       38 | 2299 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       38 | 2300 | `			return SXRET_OK;` |
|        - | 2301 | `		}` |
|       28 | 2302 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 2303 | `			char zBuf[128];` |
|       24 | 2304 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        7 | 2305 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 2306 | `		}` |
|       13 | 2307 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2308 | `	}` |
|        - | 2309 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - | 2310 | `	 * includes null). */` |
|   101109 | 2311 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       32 | 2312 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       26 | 2313 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 | 2314 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       23 | 2315 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       23 | 2316 | `			return SXRET_OK;` |
|        - | 2317 | `		}` |
|       15 | 2318 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - | 2319 | `	}` |
|        - | 2320 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - | 2321 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - | 2322 | `	 * type error. */` |
|   101077 | 2323 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2324 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2325 | `	}` |
|        - | 2326 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - | 2327 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - | 2328 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|   101077 | 2329 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 | 2330 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 | 2331 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        5 | 2332 | `			return SXRET_OK;` |
|        - | 2333 | `		}` |
|        7 | 2334 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2335 | `	}` |
|        - | 2336 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - | 2337 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - | 2338 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - | 2339 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - | 2340 | `	 * wrongly accept any object / reject arrays. */` |
|   101067 | 2341 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       79 | 2342 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       79 | 2343 | `		if( rcPseudo == 1 ){` |
|       13 | 2344 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       13 | 2345 | `			return SXRET_OK;` |
|        - | 2346 | `		}` |
|       67 | 2347 | `		if( rcPseudo == 0 ){` |
|        8 | 2348 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2349 | `		}` |
|        - | 2350 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       28 | 2351 | `	}` |
|   101049 | 2352 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - | 2353 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|        - | 2354 | `		 * class (pHintScope), not the instance's runtime class. */` |
|       61 | 2355 | `		ph7_class *pExpected = 0;` |
|       61 | 2356 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|        - | 2357 | `			char zBuf[128];` |
|       31 | 2358 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       18 | 2359 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|       18 | 2360 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|      ! 0 | 2361 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2362 | `		}` |
|       43 | 2363 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       43 | 2364 | `		return SXRET_OK;` |
|        - | 2365 | `	}` |
|        - | 2366 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|        - | 2367 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|        - | 2368 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|        - | 2369 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|        - | 2370 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|   100993 | 2371 | `	if( bStrict ){` |
|        - | 2372 | `		int bOk;` |
|       29 | 2373 | `		if( ph7_value_is_bool(pValue) ){` |
|        5 | 2374 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|       27 | 2375 | `		}else if( ph7_value_is_float(pValue) ){` |
|        3 | 2376 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|       24 | 2377 | `		}else if( ph7_value_is_int(pValue) ){` |
|        9 | 2378 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|       19 | 2379 | `		}else if( ph7_value_is_string(pValue) ){` |
|       15 | 2380 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|        8 | 2381 | `		}else{` |
|        - | 2382 | `			/* array / resource / an object against a scalar type: no coercion in` |
|        - | 2383 | `			 * either mode, so the flag test is the whole answer (an object never` |
|        - | 2384 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|        - | 2385 | `			 * does not perform). */` |
|      ! 0 | 2386 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|        - | 2387 | `		}` |
|       29 | 2388 | `		if( !bOk ){` |
|        - | 2389 | `			char zObjBuf[128];` |
|       31 | 2390 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       20 | 2391 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      ! 0 | 2392 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|       20 | 2393 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2394 | `		}` |
|        9 | 2395 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|        3 | 2396 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|        2 | 2397 | `		}else{` |
|        7 | 2398 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2399 | `		}` |
|        9 | 2400 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 2401 | `		return SXRET_OK;` |
|        - | 2402 | `	}` |
|        - | 2403 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - | 2404 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|        - | 2405 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|        - | 2406 | `	 * via __toString, so fall through to the string cast below. */` |
|   100965 | 2407 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       16 | 2408 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       18 | 2409 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|        4 | 2410 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|        - | 2411 | `			char zBuf[128];` |
|       20 | 2412 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        6 | 2413 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 2414 | `		}` |
|        1 | 2415 | `	}` |
|        - | 2416 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|        - | 2417 | `	 * take (VmCoerceScalarWeak's own rule, §10) -- and it had none of it, so this` |
|        - | 2418 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|        - | 2419 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|        - | 2420 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|        - | 2421 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|        - | 2422 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|        - | 2423 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|        - | 2424 | `	 * that arrives carrying a cached int representation is asked too. */` |
|   100953 | 2425 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|       38 | 2426 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       12 | 2427 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2428 | `	}` |
|   100929 | 2429 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|   100085 | 2430 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|   100085 | 2431 | `		if( xCast ){` |
|        - | 2432 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|   100085 | 2433 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        8 | 2434 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2435 | `			}` |
|   100079 | 2436 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       11 | 2437 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2438 | `			}` |
|        - | 2439 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - | 2440 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - | 2441 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|   100066 | 2442 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|   100061 | 2443 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|   100065 | 2444 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|   100037 | 2445 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - | 2446 | `			}` |
|       39 | 2447 | `			xCast(pValue);` |
|       17 | 2448 | `		}` |
|       22 | 2449 | `	}else{` |
|        - | 2450 | `		/* Mask matched — an int property accepting a whole-real must` |
|        - | 2451 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|      849 | 2452 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2453 | `	}` |
|      883 | 2454 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      883 | 2455 | `	return SXRET_OK;` |
|   267194 | 2456 | `}` |
|        - | 2457 | `/*` |
|        - | 2458 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|        - | 2459 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|        - | 2460 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|        - | 2461 | ` *     a scope that could normally reach it — else a catchable Error),` |
|        - | 2462 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|        - | 2463 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|        - | 2464 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|        - | 2465 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|        - | 2466 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|        - | 2467 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|        - | 2468 | ` */` |
|       30 | 2469 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|        - | 2470 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|        1 | 2471 | `{` |
|       31 | 2472 | `	ph7_class *pClass = pClone->pClass;` |
|        - | 2473 | `	SyHashEntry *pEntry;` |
|        - | 2474 | `	VmClassAttr *pVmAttr;` |
|        - | 2475 | `	ph7_class_attr *pAttr;` |
|        - | 2476 | `	ph7_value *pSlot;` |
|        - | 2477 | `	sxi32 rc;` |
|       31 | 2478 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|       31 | 2479 | `	if( pEntry == 0 ){` |
|        - | 2480 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|        - | 2481 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|        - | 2482 | `		 * deferred residual). */` |
|      ! 0 | 2483 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|      ! 0 | 2484 | `		if( pSlot == 0 ){` |
|      ! 0 | 2485 | `			return PH7_VmMemoryError(pVm);` |
|        - | 2486 | `		}` |
|      ! 0 | 2487 | `		PH7_MemObjStore(pValue,pSlot);` |
|      ! 0 | 2488 | `		return SXRET_OK;` |
|        - | 2489 | `	}` |
|       31 | 2490 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       31 | 2491 | `	pAttr = pVmAttr->pAttr;` |
|        - | 2492 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|       31 | 2493 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 2494 | `		SyBlob sMsg;` |
|      ! 0 | 2495 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 2496 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|      ! 0 | 2497 | `			&pClass->sName,&pAttr->sName);` |
|      ! 0 | 2498 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2499 | `	}` |
|        - | 2500 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|        - | 2501 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|        - | 2502 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|       31 | 2503 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 | 2504 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|        5 | 2505 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - | 2506 | `		SyBlob sMsg;` |
|        5 | 2507 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2508 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|        5 | 2509 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2510 | `	}` |
|        - | 2511 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|       27 | 2512 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|       27 | 2513 | `	if( rc != SXRET_OK ){` |
|        3 | 2514 | `		return rc;` |
|        - | 2515 | `	}` |
|        - | 2516 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|       25 | 2517 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       25 | 2518 | `	if( pSlot ){` |
|       25 | 2519 | `		PH7_MemObjStore(pValue,pSlot);` |
|       12 | 2520 | `	}` |
|       25 | 2521 | `	return SXRET_OK;` |
|       16 | 2522 | `}` |
|        - | 2523 | `/*` |
|        - | 2524 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - | 2525 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - | 2526 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - | 2527 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - | 2528 | ` */` |
|       10 | 2529 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        3 | 2530 | `{` |
|       13 | 2531 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2532 | `	char zBuf[128],zType[192];` |
|        - | 2533 | `	const char *zGiven;` |
|       18 | 2534 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        5 | 2535 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|       13 | 2536 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2537 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 2538 | `	}else{` |
|       13 | 2539 | `		zGiven = ph7_type_name(pValue);` |
|        - | 2540 | `	}` |
|       13 | 2541 | `	if( bLazy ){` |
|        - | 2542 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|        - | 2543 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|        - | 2544 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|        - | 2545 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|        - | 2546 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|        - | 2547 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|        - | 2548 | `		 * re-evaluates and re-raises, as php's does. */` |
|        - | 2549 | `		SyBlob sMsg;` |
|        5 | 2550 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2551 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|        2 | 2552 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        5 | 2553 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 2554 | `	}` |
|        - | 2555 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - | 2556 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - | 2557 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - | 2558 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - | 2559 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - | 2560 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - | 2561 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|        9 | 2562 | `	if( pVm->sCodeGen.xErr ){` |
|        8 | 2563 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - | 2564 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        2 | 2565 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        4 | 2566 | `	}else{` |
|        4 | 2567 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 2568 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        1 | 2569 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        - | 2570 | `	}` |
|        9 | 2571 | `	pVm->iExitStatus = 255;` |
|        9 | 2572 | `	pVm->bHaltRequested = 1;` |
|        9 | 2573 | `	return SXERR_ABORT;` |
|        8 | 2574 | `}` |
|        - | 2575 | `/*` |
|        - | 2576 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - | 2577 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - | 2578 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - | 2579 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - | 2580 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - | 2581 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - | 2582 | ` */` |
|       42 | 2583 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        4 | 2584 | `{` |
|       46 | 2585 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - | 2586 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|       46 | 2587 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 2588 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 | 2589 | `			return SXRET_OK;` |
|        - | 2590 | `		}` |
|      ! 0 | 2591 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 | 2592 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 2593 | `			return SXRET_OK;` |
|        - | 2594 | `		}` |
|      ! 0 | 2595 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2596 | `	}` |
|        - | 2597 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|       44 | 2598 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        6 | 2599 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|        5 | 2600 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|        5 | 2601 | `			return SXRET_OK;` |
|        - | 2602 | `		}` |
|      ! 0 | 2603 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2604 | `	}` |
|        - | 2605 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|       40 | 2606 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2607 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2608 | `	}` |
|        - | 2609 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|       40 | 2610 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 2611 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2612 | `			return SXRET_OK;` |
|        - | 2613 | `		}` |
|      ! 0 | 2614 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2615 | `	}` |
|        - | 2616 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - | 2617 | `	 * a real class/interface verified by instanceof. */` |
|       40 | 2618 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 2619 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        3 | 2620 | `		if( rcPseudo == 1 ){` |
|      ! 0 | 2621 | `			return SXRET_OK;` |
|        - | 2622 | `		}` |
|        3 | 2623 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 2624 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2625 | `		}` |
|        - | 2626 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|        - | 2627 | `		 * self/parent resolve against the declaring class. */` |
|        - | 2628 | `		{` |
|        3 | 2629 | `			ph7_class *pExpected = 0;` |
|        4 | 2630 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|        1 | 2631 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|        3 | 2632 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2633 | `			}` |
|        - | 2634 | `		}` |
|      ! 0 | 2635 | `		return SXRET_OK;` |
|        - | 2636 | `	}` |
|        - | 2637 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - | 2638 | `	 * implicit widening. Everything else is a type error.` |
|        - | 2639 | `	 *` |
|        - | 2640 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - | 2641 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|        - | 2642 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|        - | 2643 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|        - | 2644 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|        - | 2645 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|        - | 2646 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|        - | 2647 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|        - | 2648 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|        - | 2649 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|        - | 2650 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|       37 | 2651 | `	if( pValue->iFlags & pAttr->nType ){` |
|       25 | 2652 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       25 | 2653 | `		return SXRET_OK;` |
|        - | 2654 | `	}` |
|       13 | 2655 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 | 2656 | `		PH7_MemObjToReal(pValue);` |
|        3 | 2657 | `		return SXRET_OK;` |
|        - | 2658 | `	}` |
|       10 | 2659 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       25 | 2660 | `}` |
|        - | 2661 | `/*` |
|        - | 2662 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|        - | 2663 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|        - | 2664 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|        - | 2665 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|        - | 2666 | ` */` |
|       34 | 2667 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        3 | 2668 | `{` |
|       37 | 2669 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2670 | `	const char *zGiven;` |
|        - | 2671 | `	char zBuf[128],zType[192];` |
|       54 | 2672 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       17 | 2673 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|        - | 2674 | `	SyBlob sMsg;` |
|       37 | 2675 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2676 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 2677 | `	}else{` |
|       37 | 2678 | `		zGiven = ph7_type_name(pValue);` |
|        - | 2679 | `	}` |
|       37 | 2680 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       37 | 2681 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|       17 | 2682 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       37 | 2683 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        3 | 2684 | `}` |
|        - | 2685 | `/*` |
|        - | 2686 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|        - | 2687 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|        - | 2688 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|        - | 2689 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|        - | 2690 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|        - | 2691 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|        - | 2692 | ` * instance defaults per-instantiation. Matching structure of` |
|        - | 2693 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|        - | 2694 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|        - | 2695 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|        - | 2696 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|        - | 2697 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|        - | 2698 | ` */` |
|        - | 2699 | `/*` |
|        - | 2700 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|        - | 2701 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|        - | 2702 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|        - | 2703 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|        - | 2704 | ` * throwing, so the static-property mount path can defer the failure (php` |
|        - | 2705 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|        - | 2706 | ` * instance path throws immediately via the wrapper below.` |
|        - | 2707 | ` */` |
|      426 | 2708 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 2709 | `{` |
|      431 | 2710 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|      431 | 2711 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       58 | 2712 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       54 | 2713 | `			return SXRET_OK;` |
|        - | 2714 | `		}` |
|        4 | 2715 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 2716 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 2717 | `			return SXRET_OK;` |
|        - | 2718 | `		}` |
|        3 | 2719 | `		return SXERR_INVALID;` |
|        - | 2720 | `	}` |
|      377 | 2721 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       39 | 2722 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       30 | 2723 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       30 | 2724 | `			return SXRET_OK;` |
|        - | 2725 | `		}` |
|      ! 0 | 2726 | `		return SXERR_INVALID;` |
|        - | 2727 | `	}` |
|      351 | 2728 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2729 | `		return SXERR_INVALID;` |
|        - | 2730 | `	}` |
|      351 | 2731 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 2732 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2733 | `			return SXRET_OK;` |
|        - | 2734 | `		}` |
|      ! 0 | 2735 | `		return SXERR_INVALID;` |
|        - | 2736 | `	}` |
|      351 | 2737 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        5 | 2738 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        5 | 2739 | `		if( rcPseudo == 1 ){` |
|        5 | 2740 | `			return SXRET_OK;` |
|        - | 2741 | `		}` |
|      ! 0 | 2742 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 2743 | `			return SXERR_INVALID;` |
|        - | 2744 | `		}` |
|        - | 2745 | `		{` |
|        - | 2746 | `			/* self/parent in the hint resolve against the declaring class. */` |
|      ! 0 | 2747 | `			ph7_class *pExpected = 0;` |
|      ! 0 | 2748 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|      ! 0 | 2749 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|      ! 0 | 2750 | `				return SXERR_INVALID;` |
|        - | 2751 | `			}` |
|        - | 2752 | `		}` |
|      ! 0 | 2753 | `		return SXRET_OK;` |
|        - | 2754 | `	}` |
|      347 | 2755 | `	if( pValue->iFlags & pAttr->nType ){` |
|      311 | 2756 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      311 | 2757 | `		return SXRET_OK;` |
|        - | 2758 | `	}` |
|       39 | 2759 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        6 | 2760 | `		PH7_MemObjToReal(pValue);` |
|        6 | 2761 | `		return SXRET_OK;` |
|        - | 2762 | `	}` |
|       35 | 2763 | `	return SXERR_INVALID;` |
|      218 | 2764 | `}` |
|      362 | 2765 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 2766 | `{` |
|      367 | 2767 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|      355 | 2768 | `		return SXRET_OK;` |
|        - | 2769 | `	}` |
|       13 | 2770 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      186 | 2771 | `}` |
|        - | 2772 | `/*` |
|        - | 2773 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|        - | 2774 | ` * class chain for a static typed slot whose mount-time default failed its` |
|        - | 2775 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|        - | 2776 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|        - | 2777 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|        - | 2778 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|        - | 2779 | ` * never-touched class stays silent, and the throw repeats on every access` |
|        - | 2780 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|        - | 2781 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|        - | 2782 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|        - | 2783 | ` */` |
|       26 | 2784 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 2785 | `{` |
|        - | 2786 | `	ph7_class *pScan;` |
|       33 | 2787 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        - | 2788 | `		SyHashEntry *pEntry;` |
|       29 | 2789 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|       33 | 2790 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|       29 | 2791 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       26 | 2792 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|        - | 2793 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|       24 | 2794 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       25 | 2795 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|       25 | 2796 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       25 | 2797 | `				if( pSlot ){` |
|       25 | 2798 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       25 | 2799 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|       25 | 2800 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        - | 2801 | `						ph7_value sNull;` |
|       25 | 2802 | `						if( pValue == 0 ){` |
|      ! 0 | 2803 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|      ! 0 | 2804 | `							pValue = &sNull;` |
|      ! 0 | 2805 | `						}` |
|       25 | 2806 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|        - | 2807 | `					}` |
|      ! 0 | 2808 | `				}` |
|      ! 0 | 2809 | `			}` |
|        1 | 2810 | `		}` |
|        3 | 2811 | `	}` |
|        5 | 2812 | `	return SXRET_OK;` |
|       16 | 2813 | `}` |
|        - | 2814 | `/*` |
|        - | 2815 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|        - | 2816 | ` * materialization: an initializer that threw at mount and was deferred` |
|        - | 2817 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|        - | 2818 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|        - | 2819 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|        - | 2820 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|        - | 2821 | ` * a subclass can be mounted before the base whose default failed.` |
|        - | 2822 | ` */` |
|  2229164 | 2823 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|        5 | 2824 | `{` |
|  4466469 | 2825 | `	while( pClass ){` |
|  2237387 | 2826 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|       85 | 2827 | `			return 1;` |
|        - | 2828 | `		}` |
|  2237305 | 2829 | `		pClass = pClass->pBase;` |
|        5 | 2830 | `	}` |
|  2229087 | 2831 | `	return 0;` |
|  1114587 | 2832 | `}` |
|        - | 2833 | `/*` |
|        - | 2834 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 2835 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 2836 | ` *` |
|        - | 2837 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 2838 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 2839 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 2840 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 2841 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 2842 | ` *` |
|        - | 2843 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 2844 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 2845 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 2846 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 2847 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 2848 | ` * re-raises on every access too.` |
|        - | 2849 | ` */` |
|       86 | 2850 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|        3 | 2851 | `{` |
|        - | 2852 | `	SyHashEntry *pEntry;` |
|        - | 2853 | `	sxi32 rc;` |
|       89 | 2854 | `	if( pClass->pBase ){` |
|        6 | 2855 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|        6 | 2856 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2857 | `			return rc;` |
|        - | 2858 | `		}` |
|        2 | 2859 | `	}` |
|       89 | 2860 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      209 | 2861 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      123 | 2862 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      123 | 2863 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - | 2864 | `			/* Not pending. An inherited slot the base pass already collected` |
|        - | 2865 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|        - | 2866 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|       61 | 2867 | `			continue;` |
|        - | 2868 | `		}` |
|       64 | 2869 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 2870 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|        - | 2871 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|        - | 2872 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|        - | 2873 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|        - | 2874 | `			 * class keeps its hint flag so a later access retries. */` |
|      ! 0 | 2875 | `			*pbLeft = 1;` |
|      ! 0 | 2876 | `			continue;` |
|        - | 2877 | `		}` |
|       64 | 2878 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|       64 | 2879 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2880 | `			return rc;` |
|        - | 2881 | `		}` |
|        2 | 2882 | `	}` |
|       89 | 2883 | `	return SXRET_OK;` |
|       46 | 2884 | `}` |
|        - | 2885 | `/*` |
|        - | 2886 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 2887 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 2888 | ` *` |
|        - | 2889 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 2890 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 2891 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 2892 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 2893 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 2894 | ` *` |
|        - | 2895 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 2896 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 2897 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 2898 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 2899 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 2900 | ` * re-raises on every access too.` |
|        - | 2901 | ` *` |
|        - | 2902 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|        - | 2903 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|        - | 2904 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|        - | 2905 | ` * cut the outer one short.` |
|        - | 2906 | ` */` |
|       82 | 2907 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|        3 | 2908 | `{` |
|        - | 2909 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|        - | 2910 | `	ph7_class_attr **apPending;` |
|        - | 2911 | `	sxu32 n,nUsed;` |
|        - | 2912 | `	sxi32 rc;` |
|       85 | 2913 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       85 | 2914 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|       85 | 2915 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|       85 | 2916 | `	nUsed = SySetUsed(&aPending);` |
|       89 | 2917 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|       62 | 2918 | `		ph7_class_attr *pAttr = apPending[n];` |
|       62 | 2919 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2920 | `		ph7_class *pSaveCtx;` |
|        - | 2921 | `		void *pSaveFrame;` |
|        - | 2922 | `		sxu32 nSaveLazyLine;` |
|        - | 2923 | `		sxi32 nSaveLazyDepth;` |
|        - | 2924 | `		ph7_value *pMemObj;` |
|        - | 2925 | `		sxi32 rcExec;` |
|       62 | 2926 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|      ! 0 | 2927 | `			continue; /* the base pass already ran this shared slot */` |
|        - | 2928 | `		}` |
|       62 | 2929 | `		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       62 | 2930 | `		if( pMemObj == 0 ){` |
|      ! 0 | 2931 | `			continue;` |
|        - | 2932 | `		}` |
|       62 | 2933 | `		pSaveCtx = pVm->pConstEvalClass;` |
|       62 | 2934 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|       62 | 2935 | `		pVm->pConstEvalClass = pOwner;` |
|        - | 2936 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|        - | 2937 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|        - | 2938 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|        - | 2939 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|        - | 2940 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|       62 | 2941 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 2942 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|        - | 2943 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|       62 | 2944 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|       62 | 2945 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|       62 | 2946 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|       62 | 2947 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|       62 | 2948 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|       62 | 2949 | `		pVm->nConstEvalDepth++;` |
|       62 | 2950 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|       62 | 2951 | `		pVm->nConstEvalDepth--;` |
|       62 | 2952 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|       62 | 2953 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|       62 | 2954 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       62 | 2955 | `		pVm->pConstEvalClass = pSaveCtx;` |
|       62 | 2956 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|       62 | 2957 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 2958 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|        - | 2959 | `			 * caller to route (a catch here is the user's own). */` |
|       58 | 2960 | `			rc = rcExec;` |
|       58 | 2961 | `			break;` |
|        - | 2962 | `		}` |
|        5 | 2963 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|        5 | 2964 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 2965 | `			/* The initializer named a self-referencing constant. Like the mount` |
|        - | 2966 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|        - | 2967 | `			 * the access, where a catch can see it. */` |
|      ! 0 | 2968 | `			rc = VmConstCycleThrow(&(*pVm));` |
|      ! 0 | 2969 | `			break;` |
|        - | 2970 | `		}` |
|        4 | 2971 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|        3 | 2972 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 2973 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|        - | 2974 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|        - | 2975 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|      ! 0 | 2976 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      ! 0 | 2977 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|      ! 0 | 2978 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      ! 0 | 2979 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|      ! 0 | 2980 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|      ! 0 | 2981 | `				break;` |
|        - | 2982 | `			}` |
|      ! 0 | 2983 | `		}` |
|        3 | 2984 | `	}` |
|       85 | 2985 | `	if( rc != SXRET_OK ){` |
|       58 | 2986 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|       28 | 2987 | `	}` |
|       85 | 2988 | `	SySetRelease(&aPending);` |
|       85 | 2989 | `	return rc;` |
|        3 | 2990 | `}` |
|        - | 2991 | `/*` |
|        - | 2992 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|        - | 2993 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|        - | 2994 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|        - | 2995 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|        - | 2996 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|        - | 2997 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|        - | 2998 | ` */` |
|       82 | 2999 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 3000 | `{` |
|       85 | 3001 | `	int bLeft = 0;` |
|       85 | 3002 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|       85 | 3003 | `	if( rc == SXRET_OK ){` |
|       29 | 3004 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|       13 | 3005 | `	}` |
|       85 | 3006 | `	if( rc == SXRET_OK && !bLeft ){` |
|        - | 3007 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|        - | 3008 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|        - | 3009 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|        - | 3010 | `		 * set — php's materialization keeps failing too. */` |
|        - | 3011 | `		ph7_class *pScan;` |
|        9 | 3012 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        5 | 3013 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|        3 | 3014 | `		}` |
|        2 | 3015 | `	}` |
|       85 | 3016 | `	return rc;` |
|        3 | 3017 | `}` |
|        - | 3018 |  |
|        - | 3019 | `/*` |
|        - | 3020 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3021 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 3022 | ` * information.` |
|        - | 3023 | ` * ------------------------------------` |
|        - | 3024 | ` * Simple boring wrapper function.` |
|        - | 3025 | ` * ------------------------------------` |
|        - | 3026 | ` */` |
|     1950 | 3027 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        5 | 3028 | `{` |
|        - | 3029 | `	va_list ap;` |
|        - | 3030 | `	sxi32 rc;` |
|     1955 | 3031 | `	va_start(ap,zFormat);` |
|     1955 | 3032 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     1955 | 3033 | `	va_end(ap);` |
|     1955 | 3034 | `	return rc;` |
|        5 | 3035 | `}` |
|        - | 3036 | `/*` |
|        - | 3037 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - | 3038 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - | 3039 | ` */` |
|      380 | 3040 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 | 3041 | `{` |
|        - | 3042 | `	ph7_class *pClass;` |
|        - | 3043 | `	ph7_class_instance *pThis;` |
|        - | 3044 | `	ph7_class_method *pCons;` |
|        - | 3045 | `	ph7_value sArg;` |
|        - | 3046 | `	ph7_value *apArg[1];` |
|        - | 3047 | `	SyBlob sMsg;` |
|        - | 3048 | `	SyString sMsgStr;` |
|      385 | 3049 | `	SyString *pFuncName = &pCallee->sName;` |
|        - | 3050 | `	VmFrame *pFrame;` |
|        - | 3051 | `	sxi32 rc;` |
|      385 | 3052 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      385 | 3053 | `	if( pClass == 0 ){` |
|      ! 0 | 3054 | `		return PH7_ABORT;` |
|        - | 3055 | `	}` |
|      385 | 3056 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      385 | 3057 | `	if( pThis == 0 ){` |
|      ! 0 | 3058 | `		return PH7_ABORT;` |
|        - | 3059 | `	}` |
|      385 | 3060 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 3061 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|        - | 3062 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|        - | 3063 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|        - | 3064 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|        - | 3065 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|      573 | 3066 | `	if( pOwnerClass ){` |
|        - | 3067 | `		/* A property hook is named after its PROPERTY, never after the method` |
|        - | 3068 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|        - | 3069 | `		SyBlob sHook;` |
|       41 | 3070 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|       41 | 3071 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|        6 | 3072 | `			if( pArgName ){` |
|        6 | 3073 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|        4 | 3074 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|        2 | 3075 | `					nArg,pArgName,zExpected,zGiven);` |
|        4 | 3076 | `			}else{` |
|      ! 0 | 3077 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      ! 0 | 3078 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|      ! 0 | 3079 | `					nArg,zExpected,zGiven);` |
|        - | 3080 | `			}` |
|        6 | 3081 | `			SyBlobRelease(&sHook);` |
|        6 | 3082 | `			goto ArgMsgBuilt;` |
|        - | 3083 | `		}` |
|       37 | 3084 | `		SyBlobRelease(&sHook);` |
|       37 | 3085 | `		if( pArgName ){` |
|       29 | 3086 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       12 | 3087 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       17 | 3088 | `		}else{` |
|       11 | 3089 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|        4 | 3090 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|        - | 3091 | `		}` |
|       21 | 3092 | `	}else{` |
|        - | 3093 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|      349 | 3094 | `		const char *zShow = 0;` |
|      349 | 3095 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|      349 | 3096 | `		if( pArgName ){` |
|      281 | 3097 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|      138 | 3098 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|      143 | 3099 | `		}else{` |
|       72 | 3100 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|       34 | 3101 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|        - | 3102 | `		}` |
|        - | 3103 | `	}` |
|      190 | 3104 | `ArgMsgBuilt:` |
|        - | 3105 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|        - | 3106 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|        - | 3107 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|      385 | 3108 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|      379 | 3109 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      379 | 3110 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|      379 | 3111 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|      187 | 3112 | `		}` |
|      187 | 3113 | `	}` |
|      385 | 3114 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      385 | 3115 | `	if( pCons ){` |
|      385 | 3116 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      385 | 3117 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      385 | 3118 | `		apArg[0] = &sArg;` |
|      385 | 3119 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      385 | 3120 | `		PH7_MemObjRelease(&sArg);` |
|      190 | 3121 | `	}` |
|      385 | 3122 | `	SyBlobRelease(&sMsg);` |
|      385 | 3123 | `	pFrame = pVm->pFrame;` |
|      385 | 3124 | `	if( pFrame ){` |
|      385 | 3125 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      385 | 3126 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      190 | 3127 | `	}` |
|      385 | 3128 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      385 | 3129 | `	PH7_ClassInstanceUnref(pThis);` |
|      385 | 3130 | `	if( rc == SXERR_ABORT ){` |
|        6 | 3131 | `		return PH7_ABORT;` |
|        - | 3132 | `	}` |
|      381 | 3133 | `	return PH7_EXCEPTION;` |
|      195 | 3134 | `}` |
|        - | 3135 | `/*` |
|        - | 3136 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|        - | 3137 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|        - | 3138 | ` * positional and the named-argument binding paths of OP_CALL.` |
|        - | 3139 | ` *` |
|        - | 3140 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|        - | 3141 | ` * element uses its overall call position; a NAMED element always reports` |
|        - | 3142 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|        - | 3143 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|        - | 3144 | ` * formal, so no single parameter name applies.` |
|        - | 3145 | ` *` |
|        - | 3146 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|        - | 3147 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|        - | 3148 | ` */` |
|     2800 | 3149 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|        - | 3150 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|        5 | 3151 | `{` |
|        - | 3152 | `	sxi32 rc;` |
|     2805 | 3153 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|       33 | 3154 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|       37 | 3155 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|        - | 3156 | `			const char *zGiven;` |
|       11 | 3157 | `			const char *zExpected = "union";` |
|        - | 3158 | `			char zBuf[128];` |
|        - | 3159 | `			char zTypeBuf[128];` |
|       11 | 3160 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 | 3161 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       10 | 3162 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 3163 | `				zGiven = "null";` |
|      ! 0 | 3164 | `			}else{` |
|        9 | 3165 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 3166 | `			}` |
|       11 | 3167 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       15 | 3168 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|        4 | 3169 | `					zTypeBuf,sizeof(zTypeBuf));` |
|        4 | 3170 | `			}` |
|       11 | 3171 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       11 | 3172 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3173 | `		}` |
|       17 | 3174 | `		return SXRET_OK;` |
|        - | 3175 | `	}` |
|     2778 | 3176 | `	if( pFormal->nType < 1` |
|     1501 | 3177 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|     2581 | 3178 | `		return SXRET_OK;` |
|        - | 3179 | `	}` |
|      207 | 3180 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 3181 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|        - | 3182 | `		 * per element exactly like the non-variadic paths. */` |
|       61 | 3183 | `		SyString *pName = &pFormal->sClass;` |
|        - | 3184 | `		ph7_class *pClass;` |
|       61 | 3185 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       61 | 3186 | `		if( rcPseudo == 0 ){` |
|        - | 3187 | `			/* Recognised pseudo-type; value mismatches */` |
|        - | 3188 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       14 | 3189 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        6 | 3190 | `				VmClassHintTypeName(pName,0,` |
|        6 | 3191 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|        3 | 3192 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        8 | 3193 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3194 | `		}` |
|        - | 3195 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|        - | 3196 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|        - | 3197 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|        - | 3198 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|        - | 3199 | `		 * so ANY non-object is a TypeError, matching php. */` |
|       55 | 3200 | `		pClass = 0;` |
|       55 | 3201 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 3202 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       43 | 3203 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 3204 | `				VmClassHintTypeName(pName,pClass,` |
|       20 | 3205 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 3206 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       23 | 3207 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3208 | `		}` |
|       34 | 3209 | `		return SXRET_OK;` |
|        - | 3210 | `	}` |
|      149 | 3211 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       63 | 3212 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|        - | 3213 | `			char zGivenBuf[128];` |
|        8 | 3214 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        2 | 3215 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        6 | 3216 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3217 | `		}` |
|       59 | 3218 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 3219 | `			char zTypeBuf[128];` |
|        - | 3220 | `			char zGivenBuf[128];` |
|       60 | 3221 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       19 | 3222 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       19 | 3223 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       41 | 3224 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3225 | `		}` |
|       11 | 3226 | `	}else{` |
|        - | 3227 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|        - | 3228 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|       89 | 3229 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 3230 | `	}` |
|      107 | 3231 | `	return SXRET_OK;` |
|     1405 | 3232 | `}` |
|        - | 3233 | `/*` |
|        - | 3234 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|        - | 3235 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|        - | 3236 | ` * before a required parameter as implicitly required), excluding a trailing` |
|        - | 3237 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|        - | 3238 | ` * pick php's wording — "exactly N expected" when required == total,` |
|        - | 3239 | ` * "at least N" when trailing optionals exist.` |
|        - | 3240 | ` */` |
|     9114 | 3241 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|        5 | 3242 | `{` |
|     9119 | 3243 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     9119 | 3244 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|     9119 | 3245 | `	sxu32 nRequired = 0;` |
|        - | 3246 | `	sxu32 n;` |
|     9119 | 3247 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      817 | 3248 | `		nFormal--;` |
|      406 | 3249 | `	}` |
|    42233 | 3250 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|    33119 | 3251 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    11571 | 3252 | `			nRequired = n + 1;` |
|     5783 | 3253 | `		}` |
|    16562 | 3254 | `	}` |
|     9119 | 3255 | `	*pnNonVariadic = nFormal;` |
|     9119 | 3256 | `	return nRequired;` |
|        5 | 3257 | `}` |
|        - | 3258 | `/*` |
|        - | 3259 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|        - | 3260 | ` * with too few arguments:` |
|        - | 3261 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|        - | 3262 | ` *   {exactly\|at least} M expected` |
|        - | 3263 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|        - | 3264 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|        - | 3265 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|        - | 3266 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|        - | 3267 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|        - | 3268 | ` */` |
|       26 | 3269 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3270 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|        3 | 3271 | `{` |
|        - | 3272 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|        - | 3273 | `	SyBlob sMsg;` |
|       29 | 3274 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       29 | 3275 | `	if( pOwnerClass ){` |
|        5 | 3276 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|        2 | 3277 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|        3 | 3278 | `	}else{` |
|       25 | 3279 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|        - | 3280 | `	}` |
|       29 | 3281 | `	if( bCallSite ){` |
|       27 | 3282 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|       27 | 3283 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|       12 | 3284 | `	}` |
|       29 | 3285 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|       13 | 3286 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|        - | 3287 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       29 | 3288 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        3 | 3289 | `}` |
|        - | 3290 | `/*` |
|        - | 3291 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|        - | 3292 | ` * cannot be referenced:` |
|        - | 3293 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|        - | 3294 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|        - | 3295 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|        - | 3296 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|        - | 3297 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|        - | 3298 | `` * the bare `m()`.`` |
|        - | 3299 | ` */` |
|        - | 3300 | `/*` |
|        - | 3301 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|        - | 3302 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|        - | 3303 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|        - | 3304 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|        - | 3305 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|        - | 3306 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|        - | 3307 | `` * php's `{closure:file:line}`.`` |
|        - | 3308 | ` */` |
|       74 | 3309 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|        - | 3310 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        1 | 3311 | `{` |
|       75 | 3312 | `	const char *zShow = 0;` |
|       75 | 3313 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|        - | 3314 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|        - | 3315 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|        - | 3316 | `	 * single name applies (the refusal message splits the same way). */` |
|       75 | 3317 | `	if( pOwnerClass && pArgName ){` |
|       34 | 3318 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3319 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       11 | 3320 | `			&pOwnerClass->sName,nShow,zShow,nArgPos,pArgName);` |
|       64 | 3321 | `	}else if( pOwnerClass ){` |
|      ! 0 | 3322 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3323 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|      ! 0 | 3324 | `			&pOwnerClass->sName,nShow,zShow,nArgPos);` |
|       53 | 3325 | `	}else if( pArgName ){` |
|       73 | 3326 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3327 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       24 | 3328 | `			nShow,zShow,nArgPos,pArgName);` |
|       25 | 3329 | `	}else{` |
|        7 | 3330 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3331 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|        2 | 3332 | `			nShow,zShow,nArgPos);` |
|        - | 3333 | `	}` |
|       75 | 3334 | `}` |
|     4026 | 3335 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3336 | `	sxu32 nArgPos,SyString *pArgName)` |
|        2 | 3337 | `{` |
|        - | 3338 | `	SyBlob sMsg;` |
|     4028 | 3339 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     4028 | 3340 | `	if( pOwnerClass ){` |
|        8 | 3341 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) could not be passed by reference",` |
|        3 | 3342 | `			&pOwnerClass->sName,pFuncName,nArgPos,pArgName);` |
|        5 | 3343 | `	}else{` |
|     4022 | 3344 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) could not be passed by reference",` |
|     2010 | 3345 | `			pFuncName,nArgPos,pArgName);` |
|        - | 3346 | `	}` |
|        - | 3347 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     4028 | 3348 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 3349 | `}` |
|        - | 3350 | `/*` |
|        - | 3351 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|        - | 3352 | ` * called with too few arguments, in php's ZPP wording:` |
|        - | 3353 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|        - | 3354 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|        - | 3355 | ` * pluralized on the expected count).` |
|        - | 3356 | ` *` |
|        - | 3357 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|        - | 3358 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|        - | 3359 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|        - | 3360 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|        - | 3361 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|        - | 3362 | ` */` |
|       34 | 3363 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3364 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|        1 | 3365 | `{` |
|        - | 3366 | `	SyBlob sMsg;` |
|       35 | 3367 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|       35 | 3368 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       35 | 3369 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 | 3370 | `	if( pOwnerClass ){` |
|      ! 0 | 3371 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3372 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      ! 0 | 3373 | `	}else{` |
|       35 | 3374 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       17 | 3375 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|        - | 3376 | `	}` |
|        - | 3377 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       35 | 3378 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3379 | `}` |
|        - | 3380 | `/*` |
|        - | 3381 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|        - | 3382 | ` * MANY arguments, in php's ZPP wording:` |
|        - | 3383 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|        - | 3384 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|        - | 3385 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|        - | 3386 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|        - | 3387 | ` * so the caller must not route such a callee here.` |
|        - | 3388 | ` */` |
|       26 | 3389 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3390 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|        1 | 3391 | `{` |
|        - | 3392 | `	SyBlob sMsg;` |
|       27 | 3393 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|       27 | 3394 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|       27 | 3395 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       27 | 3396 | `	if( pOwnerClass ){` |
|      ! 0 | 3397 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3398 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|      ! 0 | 3399 | `	}else{` |
|       27 | 3400 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       13 | 3401 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|        - | 3402 | `	}` |
|        - | 3403 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       27 | 3404 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3405 | `}` |
|        - | 3406 | `/*` |
|        - | 3407 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|        - | 3408 | ` * named or positional argument resolved to:` |
|        - | 3409 | ` *   C::f(): Argument #N ($x) not passed` |
|        - | 3410 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|        - | 3411 | ` */` |
|        2 | 3412 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3413 | `	sxu32 nArg,SyString *pArgName)` |
|        1 | 3414 | `{` |
|        - | 3415 | `	SyBlob sMsg;` |
|        3 | 3416 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 3417 | `	if( pOwnerClass ){` |
|      ! 0 | 3418 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|      ! 0 | 3419 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|      ! 0 | 3420 | `	}else{` |
|        3 | 3421 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|        - | 3422 | `	}` |
|        - | 3423 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|        3 | 3424 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3425 | `}` |
|        - | 3426 | `/*` |
|        - | 3427 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - | 3428 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - | 3429 | ` */` |
|        - | 3430 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|        - | 3431 | ` * The message is copied into the instance by __construct, so the caller owns` |
|        - | 3432 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|      146 | 3433 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|        5 | 3434 | `{` |
|        - | 3435 | `	ph7_class *pClass;` |
|        - | 3436 | `	ph7_class_instance *pThis;` |
|        - | 3437 | `	ph7_class_method *pCons;` |
|        - | 3438 | `	ph7_value sArg;` |
|        - | 3439 | `	ph7_value *apArg[1];` |
|        - | 3440 | `	SyString sMsgStr;` |
|        - | 3441 | `	VmFrame *pFrame;` |
|        - | 3442 | `	sxi32 rc;` |
|      151 | 3443 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      151 | 3444 | `	if( pClass == 0 ){` |
|      ! 0 | 3445 | `		return PH7_ABORT;` |
|        - | 3446 | `	}` |
|      151 | 3447 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      151 | 3448 | `	if( pThis == 0 ){` |
|      ! 0 | 3449 | `		return PH7_ABORT;` |
|        - | 3450 | `	}` |
|      151 | 3451 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      151 | 3452 | `	if( pCons ){` |
|      151 | 3453 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      151 | 3454 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      151 | 3455 | `		apArg[0] = &sArg;` |
|      151 | 3456 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      151 | 3457 | `		PH7_MemObjRelease(&sArg);` |
|       73 | 3458 | `	}` |
|      151 | 3459 | `	pFrame = pVm->pFrame;` |
|      151 | 3460 | `	if( pFrame ){` |
|      151 | 3461 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      151 | 3462 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       73 | 3463 | `	}` |
|      151 | 3464 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      151 | 3465 | `	PH7_ClassInstanceUnref(pThis);` |
|      151 | 3466 | `	if( rc == SXERR_ABORT ){` |
|        6 | 3467 | `		return PH7_ABORT;` |
|        - | 3468 | `	}` |
|      147 | 3469 | `	return PH7_EXCEPTION;` |
|       78 | 3470 | `}` |
|        - | 3471 | `/*` |
|        - | 3472 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|        - | 3473 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|        - | 3474 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|        - | 3475 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|        - | 3476 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|        - | 3477 | ` * other callee falls through to the ordinary Class::method rendering.` |
|        - | 3478 | ` */` |
|        - | 3479 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|   639272 | 3480 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|        5 | 3481 | `{` |
|   639277 | 3482 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|   639277 | 3483 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|   638859 | 3484 | `		return 0;` |
|        - | 3485 | `	}` |
|      423 | 3486 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      378 | 3487 | `		return 0;` |
|        - | 3488 | `	}` |
|       47 | 3489 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|       35 | 3490 | `		*pzKind = "get";` |
|       31 | 3491 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|       16 | 3492 | `		*pzKind = "set";` |
|       10 | 3493 | `	}else{` |
|      ! 0 | 3494 | `		return 0;` |
|        - | 3495 | `	}` |
|       47 | 3496 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|       47 | 3497 | `	return 1;` |
|   319641 | 3498 | `}` |
|      128 | 3499 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3500 | `{` |
|        - | 3501 | `	SyString sProp;` |
|        - | 3502 | `	const char *zKind;` |
|      133 | 3503 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      123 | 3504 | `		return 0;` |
|        - | 3505 | `	}` |
|       13 | 3506 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|       13 | 3507 | `	return 1;` |
|       69 | 3508 | `}` |
|        - | 3509 | `/*` |
|        - | 3510 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|        - | 3511 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|        - | 3512 | ` * anything else uses its display name (which is also what strips a closure's` |
|        - | 3513 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|        - | 3514 | ` * instead — they are thrown from call sites that already resolved it.` |
|        - | 3515 | ` */` |
|      162 | 3516 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3517 | `{` |
|      167 | 3518 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       95 | 3519 | `		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){` |
|        7 | 3520 | `			return;` |
|        - | 3521 | `		}` |
|       89 | 3522 | `		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);` |
|       89 | 3523 | `		return;` |
|        - | 3524 | `	}` |
|        - | 3525 | `	{` |
|       75 | 3526 | `		const char *zShow = 0;` |
|       75 | 3527 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|       75 | 3528 | `		if( zShow && nShow > 0 ){` |
|       75 | 3529 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       35 | 3530 | `		}` |
|        - | 3531 | `	}` |
|       86 | 3532 | `}` |
|      142 | 3533 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|        5 | 3534 | `{` |
|        - | 3535 | `	SyBlob sMsg,sName;` |
|        - | 3536 | `	sxi32 rc;` |
|      147 | 3537 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      147 | 3538 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|      147 | 3539 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|      147 | 3540 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|      142 | 3541 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|      147 | 3542 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      147 | 3543 | `	SyBlobRelease(&sName);` |
|      147 | 3544 | `	SyBlobRelease(&sMsg);` |
|      147 | 3545 | `	return rc;` |
|        5 | 3546 | `}` |
|        - | 3547 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|        - | 3548 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|        - | 3549 | ` * php calls it a "method" when it is one. */` |
|        4 | 3550 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        2 | 3551 | `{` |
|        - | 3552 | `	SyBlob sMsg,sName;` |
|        - | 3553 | `	sxi32 rc;` |
|        6 | 3554 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 3555 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|        6 | 3556 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|        6 | 3557 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|        4 | 3558 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|        4 | 3559 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|        6 | 3560 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|        6 | 3561 | `	SyBlobRelease(&sName);` |
|        6 | 3562 | `	SyBlobRelease(&sMsg);` |
|        6 | 3563 | `	return rc;` |
|        2 | 3564 | `}` |
|        - | 3565 | `/*` |
|        - | 3566 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - | 3567 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - | 3568 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - | 3569 | ` */` |
|     1138 | 3570 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        5 | 3571 | `{` |
|     1143 | 3572 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      111 | 3573 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 3574 | `	}` |
|     1037 | 3575 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      116 | 3576 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      116 | 3577 | `		if( pThis && pThis->pClass ){` |
|      116 | 3578 | `			SyString *pName = &pThis->pClass->sName;` |
|      116 | 3579 | `			sxu32 n = pName->nByte;` |
|      116 | 3580 | `			if( n >= nBuf ){` |
|      ! 0 | 3581 | `				n = nBuf - 1;` |
|      ! 0 | 3582 | `			}` |
|      116 | 3583 | `			SyMemcpy(pName->zString,zBuf,n);` |
|      116 | 3584 | `			zBuf[n] = 0;` |
|      116 | 3585 | `			return zBuf;` |
|        - | 3586 | `		}` |
|      ! 0 | 3587 | `		return "object";` |
|        - | 3588 | `	}` |
|      925 | 3589 | `	return ph7_type_name(pVal);` |
|      574 | 3590 | `}` |
|        - | 3591 | `/*` |
|        - | 3592 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - | 3593 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - | 3594 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - | 3595 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - | 3596 | ` */` |
|       18 | 3597 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        3 | 3598 | `{` |
|        - | 3599 | `	ph7_class *pClass;` |
|        - | 3600 | `	ph7_class_instance *pThis;` |
|        - | 3601 | `	ph7_class_method *pCons;` |
|        - | 3602 | `	ph7_value sArg;` |
|        - | 3603 | `	ph7_value *apArg[1];` |
|        - | 3604 | `	SyBlob sMsg;` |
|        - | 3605 | `	SyString sMsgStr;` |
|        - | 3606 | `	VmFrame *pFrame;` |
|        - | 3607 | `	sxi32 rc;` |
|       21 | 3608 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - | 3609 | `	char zNameBuf[64];` |
|       21 | 3610 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       21 | 3611 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       21 | 3612 | `	if( pClass == 0 ){` |
|      ! 0 | 3613 | `		return PH7_ABORT;` |
|        - | 3614 | `	}` |
|       21 | 3615 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       21 | 3616 | `	if( pThis == 0 ){` |
|      ! 0 | 3617 | `		return PH7_ABORT;` |
|        - | 3618 | `	}` |
|       21 | 3619 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       21 | 3620 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       21 | 3621 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       21 | 3622 | `	if( pCons ){` |
|       21 | 3623 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       21 | 3624 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       21 | 3625 | `		apArg[0] = &sArg;` |
|       21 | 3626 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       21 | 3627 | `		PH7_MemObjRelease(&sArg);` |
|        9 | 3628 | `	}` |
|       21 | 3629 | `	SyBlobRelease(&sMsg);` |
|       21 | 3630 | `	pFrame = pVm->pFrame;` |
|       21 | 3631 | `	if( pFrame ){` |
|       21 | 3632 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       21 | 3633 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 | 3634 | `	}` |
|       21 | 3635 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       21 | 3636 | `	PH7_ClassInstanceUnref(pThis);` |
|       21 | 3637 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3638 | `		return PH7_ABORT;` |
|        - | 3639 | `	}` |
|       21 | 3640 | `	return PH7_EXCEPTION;` |
|       12 | 3641 | `}` |
|        - | 3642 | `/*` |
|        - | 3643 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - | 3644 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - | 3645 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - | 3646 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - | 3647 | ` */` |
|        - | 3648 | `/*` |
|        - | 3649 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|        - | 3650 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|        - | 3651 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|        - | 3652 | ` * type field.` |
|        - | 3653 | ` */` |
|   817066 | 3654 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|        5 | 3655 | `{` |
|   817071 | 3656 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|        5 | 3657 | `}` |
|    13822 | 3658 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 3659 | `{` |
|    13827 | 3660 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    13827 | 3661 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|        - | 3662 | `	const char *zGiven;` |
|        - | 3663 | `	ph7_class *pHintScope;` |
|        - | 3664 | `	char zBuf[128];` |
|        - | 3665 | `	char zTypeBuf[128];` |
|        - | 3666 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    13827 | 3667 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|      ! 0 | 3668 | `		return SXRET_OK;` |
|        - | 3669 | `	}` |
|        - | 3670 | `	/* never return type: the function must not return at all. An explicit` |
|        - | 3671 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|        - | 3672 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|        - | 3673 | `	 * the call site). */` |
|    13827 | 3674 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        6 | 3675 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|        - | 3676 | `	}` |
|        - | 3677 | `	/* void return type: the function must not produce a value. */` |
|    13823 | 3678 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     2407 | 3679 | `		if( pValue == 0 ){` |
|     2403 | 3680 | `			return SXRET_OK;` |
|        - | 3681 | `		}` |
|        - | 3682 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|        - | 3683 | `		 * store, not a return: php compiles no return statement there at all,` |
|        - | 3684 | `		 * and still reports the hook's return type as void. PHL carries the` |
|        - | 3685 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|        - | 3686 | `		 * so the one implicit value this arm must not reject is that one. */` |
|        6 | 3687 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|        6 | 3688 | `			return SXRET_OK;` |
|        - | 3689 | `		}` |
|        - | 3690 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - | 3691 | `		 * still counts as "returned a value" here. */` |
|      ! 0 | 3692 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 | 3693 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|        - | 3694 | `	}` |
|        - | 3695 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|        - | 3696 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|        - | 3697 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    11421 | 3698 | `	if( pValue == 0 ){` |
|       32 | 3699 | `		const char *zExpected = "value";` |
|       32 | 3700 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       47 | 3701 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|       15 | 3702 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|       15 | 3703 | `		}` |
|        - | 3704 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|        - | 3705 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|       32 | 3706 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|        - | 3707 | `	}` |
|        - | 3708 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - | 3709 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - | 3710 | `	 * matching how every other typed return reports a missing value.) */` |
|    11391 | 3711 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 | 3712 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 3713 | `			return SXRET_OK;` |
|        - | 3714 | `		}` |
|        4 | 3715 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|        1 | 3716 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3717 | `	}` |
|        - | 3718 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|        - | 3719 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|        - | 3720 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    11387 | 3721 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|       40 | 3722 | `		return SXRET_OK;` |
|        - | 3723 | `	}` |
|        - | 3724 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - | 3725 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - | 3726 | `	 * Check by value before the real-class instanceof branch below. */` |
|    11351 | 3727 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      709 | 3728 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|      709 | 3729 | `		if( rcPseudo == 1 ){` |
|      157 | 3730 | `			return SXRET_OK;` |
|        - | 3731 | `		}` |
|      557 | 3732 | `		if( rcPseudo == 0 ){` |
|       19 | 3733 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 3734 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|        4 | 3735 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3736 | `		}` |
|        - | 3737 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      272 | 3738 | `	}` |
|        - | 3739 | `	/* The two branches below are the only ones that can name a class, so the` |
|        - | 3740 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|        - | 3741 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|        - | 3742 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|        - | 3743 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|        - | 3744 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|        - | 3745 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    11191 | 3746 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|        - | 3747 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|        - | 3748 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|        - | 3749 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    11191 | 3750 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - | 3751 | `		sxi32 rcU;` |
|       40 | 3752 | `		const char *zExpected = "union";` |
|       40 | 3753 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|       40 | 3754 | `		if( rcU == SXRET_OK ){` |
|       30 | 3755 | `			return SXRET_OK;` |
|        - | 3756 | `		}` |
|       12 | 3757 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       10 | 3758 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|        7 | 3759 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 3760 | `			zGiven = "null";` |
|      ! 0 | 3761 | `		}else{` |
|        3 | 3762 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 3763 | `		}` |
|       12 | 3764 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       17 | 3765 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|        5 | 3766 | `				zTypeBuf,sizeof(zTypeBuf));` |
|        5 | 3767 | `		}` |
|       12 | 3768 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|        - | 3769 | `	}` |
|        - | 3770 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - | 3771 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - | 3772 | `	 * it into the TypeError message. */` |
|    11155 | 3773 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      549 | 3774 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      549 | 3775 | `		ph7_class *pExpected = 0;` |
|      549 | 3776 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|       35 | 3777 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       28 | 3778 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       16 | 3779 | `			}else{` |
|        8 | 3780 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 3781 | `			}` |
|       50 | 3782 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       15 | 3783 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|        - | 3784 | `		}` |
|      519 | 3785 | `		return SXRET_OK;` |
|        - | 3786 | `	}` |
|        - | 3787 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|        - | 3788 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|        - | 3789 | `	 * non-nullable scalar return — a TypeError. */` |
|    10611 | 3790 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       25 | 3791 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 3792 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 3793 | `			"null");` |
|        - | 3794 | `	}` |
|        - | 3795 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|        - | 3796 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    10595 | 3797 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    10473 | 3798 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    10473 | 3799 | `		return SXRET_OK;` |
|        - | 3800 | `	}` |
|        - | 3801 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|        - | 3802 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|        - | 3803 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|        - | 3804 | `	 * still rejects the object under strict_types. */` |
|      127 | 3805 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       22 | 3806 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       31 | 3807 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       18 | 3808 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       20 | 3809 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       29 | 3810 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        9 | 3811 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        9 | 3812 | `				zGiven);` |
|        - | 3813 | `		}` |
|        1 | 3814 | `	}` |
|        - | 3815 | `	/* Array <-> scalar is never compatible. */` |
|      109 | 3816 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       33 | 3817 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       10 | 3818 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 3819 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3820 | `	}` |
|        - | 3821 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - | 3822 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - | 3823 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - | 3824 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       84 | 3825 | `	if( !bStrict` |
|       83 | 3826 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       49 | 3827 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       54 | 3828 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       12 | 3829 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        3 | 3830 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 3831 | `			"string");` |
|        - | 3832 | `	}` |
|       82 | 3833 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       80 | 3834 | `		return SXRET_OK;` |
|        - | 3835 | `	}` |
|        4 | 3836 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        1 | 3837 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 3838 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|     6913 | 3839 | `}` |
|        - | 3840 | `/*` |
|        - | 3841 | ` * Report a fatal named-argument error.` |
|        - | 3842 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - | 3843 | ` */` |
|       12 | 3844 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        3 | 3845 | `{` |
|        - | 3846 | `	SyBlob sMsg;` |
|        - | 3847 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|        - | 3848 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|        - | 3849 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|        - | 3850 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|        - | 3851 | `	 * unconditional fatal even inside try/catch. */` |
|       15 | 3852 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 | 3853 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|       15 | 3854 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 3855 | `}` |
|        - | 3856 | `/*` |
|        - | 3857 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3858 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 3859 | ` * information.` |
|        - | 3860 | ` * ------------------------------------` |
|        - | 3861 | ` * Simple boring wrapper function.` |
|        - | 3862 | ` * ------------------------------------` |
|        - | 3863 | ` */` |
|    28640 | 3864 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        5 | 3865 | `{` |
|        - | 3866 | `	sxi32 rc;` |
|    28645 | 3867 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|    28645 | 3868 | `	return rc;` |
|        5 | 3869 | `}` |
|        - | 3870 | `/*` |
|        - | 3871 | ` * Resolve function context from the current frame.` |
|        - | 3872 | ` */` |
|        - | 3873 | `/*` |
|        - | 3874 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|        - | 3875 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|        - | 3876 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|        - | 3877 | ` * straight at the function's own name otherwise.` |
|        - | 3878 | ` */` |
|   639090 | 3879 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|        5 | 3880 | `{` |
|   639095 | 3881 | `	const char *zName = pFunc->sName.zString;` |
|   639095 | 3882 | `	int nName = (int)pFunc->sName.nByte;` |
|   690067 | 3883 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   692668 | 3884 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|        - | 3885 | `	/* A property hook is not a method in php and never shows the name PHL` |
|        - | 3886 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|        - | 3887 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|        - | 3888 | `	 * report from inside one, and what makes the trace line read` |
|        - | 3889 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|        - | 3890 | ``	 * `C::$p::get` for free. */`` |
|        - | 3891 | `	{` |
|        - | 3892 | `		SyString sProp;` |
|        - | 3893 | `		const char *zKind;` |
|   639095 | 3894 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|       44 | 3895 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       13 | 3896 | `				"$%z::%s",&sProp,zKind);` |
|       31 | 3897 | `			*pzOut = pVm->zDisplayName;` |
|       31 | 3898 | `			return n;` |
|        - | 3899 | `		}` |
|        - | 3900 | `	}` |
|   639069 | 3901 | `	if( bClosure ){` |
|        - | 3902 | `		int n;` |
|     5541 | 3903 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|        - | 3904 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|        - | 3905 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|     5541 | 3906 | `			*pzOut = pFunc->sClosureName.zString;` |
|     5541 | 3907 | `			return (int)pFunc->sClosureName.nByte;` |
|        - | 3908 | `		}` |
|      ! 0 | 3909 | `		if( pFunc->sFile.nByte > 0 ){` |
|      ! 0 | 3910 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      ! 0 | 3911 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|      ! 0 | 3912 | `		}else{` |
|      ! 0 | 3913 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|        - | 3914 | `		}` |
|      ! 0 | 3915 | `		*pzOut = pVm->zDisplayName;` |
|      ! 0 | 3916 | `		return n;` |
|        - | 3917 | `	}` |
|   633533 | 3918 | `	*pzOut = zName;` |
|   633533 | 3919 | `	return nName;` |
|   319550 | 3920 | `}` |
|        - | 3921 | `/*` |
|        - | 3922 | `` * php's name for the ACTIVE function -- the one its `name(): ` diagnostic`` |
|        - | 3923 | ` * qualifier prints. A method is rendered with its declaring class ("W::go"),` |
|        - | 3924 | `` * a closure with php's `{closure:file:line}`, and the global scope with php's`` |
|        - | 3925 | ` * own "main". VmGetFrameContext answers the same question in the shape the` |
|        - | 3926 | ` * uncaught-exception reporter wants (a bare display name, nothing at global` |
|        - | 3927 | ` * scope); this one is for a diagnostic raised from INSIDE an internal` |
|        - | 3928 | ` * function on the caller's behalf, which is how php attributes libxml's` |
|        - | 3929 | `` * errors -- `$el->nodeValue = 'a&b'` warns under the caller's name, not under`` |
|        - | 3930 | ` * the accessor's.` |
|        - | 3931 | ` */` |
|       76 | 3932 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut)` |
|        2 | 3933 | `{` |
|       78 | 3934 | `	VmFrame *pFrame = pVm->pFrame;` |
|       78 | 3935 | `	ph7_vm_func *pFunc = 0;` |
|       78 | 3936 | `	if( pFrame ){` |
|       78 | 3937 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       78 | 3938 | `		if( pFrame->pParent ){` |
|       17 | 3939 | `			pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        8 | 3940 | `		}` |
|       38 | 3941 | `	}` |
|       78 | 3942 | `	if( pFunc ){` |
|       17 | 3943 | `		VmReturnFuncName(&(*pVm),pFunc,pOut);` |
|        9 | 3944 | `	}else{` |
|       62 | 3945 | `		SyBlobAppend(pOut,"main",sizeof("main")-1);` |
|        - | 3946 | `	}` |
|       78 | 3947 | `	SyBlobNullAppend(pOut);` |
|       78 | 3948 | `}` |
|     1152 | 3949 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 | 3950 | `{` |
|        - | 3951 | `	VmFrame *pFrame;` |
|        - | 3952 | `	ph7_vm_func *pFunc;` |
|     1156 | 3953 | `	*pzFuncName = 0;` |
|     1156 | 3954 | `	*pnFuncLen = 0;` |
|     1156 | 3955 | `	pFrame = pVm->pFrame;` |
|     1156 | 3956 | `	if( pFrame == 0 ){` |
|      ! 0 | 3957 | `		return;` |
|        - | 3958 | `	}` |
|     1156 | 3959 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1156 | 3960 | `	if( pFrame->pParent == 0 ){` |
|     1116 | 3961 | `		return;` |
|        - | 3962 | `	}` |
|       44 | 3963 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       44 | 3964 | `	if( pFunc == 0 ){` |
|      ! 0 | 3965 | `		return;` |
|        - | 3966 | `	}` |
|       44 | 3967 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|      580 | 3968 | `}` |
|        - | 3969 | `/*` |
|        - | 3970 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|        - | 3971 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|        - | 3972 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|        - | 3973 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|        - | 3974 | ` * built-in library and already produces php's exact byte format, which keeps` |
|        - | 3975 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|        - | 3976 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|        - | 3977 | ` * synthesize what it can.` |
|        - | 3978 | ` */` |
|      600 | 3979 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 3980 | `{` |
|        - | 3981 | `	ph7_class_method *pGetTrace;` |
|        - | 3982 | `	ph7_value sTrace;` |
|        - | 3983 | `	const char *zTmp;` |
|        - | 3984 | `	int nTmp;` |
|      604 | 3985 | `	int bDone = 0;` |
|        - | 3986 | `	int bSaved;` |
|      604 | 3987 | `	if( pThis == 0 ){` |
|        5 | 3988 | `		return 0;` |
|        - | 3989 | `	}` |
|      600 | 3990 | `	if( pVm->bRenderingUncaught ){` |
|        - | 3991 | `		/* Already inside a report: do not run userland trace code again. */` |
|      ! 0 | 3992 | `		return 0;` |
|        - | 3993 | `	}` |
|      600 | 3994 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|      600 | 3995 | `	if( pGetTrace == 0 ){` |
|      ! 0 | 3996 | `		return 0;` |
|        - | 3997 | `	}` |
|      600 | 3998 | `	PH7_MemObjInit(pVm,&sTrace);` |
|        - | 3999 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|        - | 4000 | `	 * anything it calls) throws, the throw would be reported by this very` |
|        - | 4001 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|        - | 4002 | `	 * trace rather than re-entering here forever. */` |
|      600 | 4003 | `	bSaved = pVm->bRenderingUncaught;` |
|      600 | 4004 | `	pVm->bRenderingUncaught = 1;` |
|      600 | 4005 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|      600 | 4006 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|      600 | 4007 | `		if( zTmp && nTmp > 0 ){` |
|      600 | 4008 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      600 | 4009 | `			bDone = 1;` |
|      298 | 4010 | `		}` |
|      298 | 4011 | `	}` |
|      600 | 4012 | `	PH7_MemObjRelease(&sTrace);` |
|      600 | 4013 | `	pVm->bRenderingUncaught = bSaved;` |
|      600 | 4014 | `	return bDone;` |
|      304 | 4015 | `}` |
|        - | 4016 | `/*` |
|        - | 4017 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|        - | 4018 | ` *` |
|        - | 4019 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|        - | 4020 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|        - | 4021 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|        - | 4022 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|        - | 4023 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|        - | 4024 | ` *             trailer.` |
|        - | 4025 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|        - | 4026 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|        - | 4027 | ` * call; this routine only appends.` |
|        - | 4028 | ` */` |
|      600 | 4029 | `static void VmRenderUncaughtEntry(` |
|        - | 4030 | `	ph7_vm *pVm,SyBlob *pOut,` |
|        - | 4031 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|        - | 4032 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|        - | 4033 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|        - | 4034 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|        - | 4035 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|        4 | 4036 | `{` |
|        - | 4037 | `	SyString *pFile;` |
|      604 | 4038 | `	if( nThrowLine == 0 ){` |
|        5 | 4039 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|        2 | 4040 | `	}` |
|      604 | 4041 | `	if( nCallLine == 0 ){` |
|      604 | 4042 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      604 | 4043 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|      300 | 4044 | `	}` |
|      604 | 4045 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 | 4046 | `		zClass = "Exception";` |
|      ! 0 | 4047 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 | 4048 | `	}` |
|      604 | 4049 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      566 | 4050 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      281 | 4051 | `	}` |
|      604 | 4052 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      604 | 4053 | `	if( bFirst ){` |
|      598 | 4054 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      301 | 4055 | `	}else{` |
|        8 | 4056 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|        - | 4057 | `	}` |
|      604 | 4058 | `	SyBlobAppend(pOut,zClass,nClass);` |
|      604 | 4059 | `	if( zMsg && nMsg > 0 ){` |
|      604 | 4060 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|      604 | 4061 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|      300 | 4062 | `	}` |
|      604 | 4063 | `	if( pFile ){` |
|      604 | 4064 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      300 | 4065 | `	}` |
|      604 | 4066 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        - | 4067 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|        - | 4068 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|        - | 4069 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|        - | 4070 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|        - | 4071 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|        - | 4072 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|        - | 4073 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|      604 | 4074 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|        5 | 4075 | `		int bFrame = 0;` |
|        5 | 4076 | `		if( zFuncName && nFuncLen > 0 ){` |
|        3 | 4077 | `			if( pFile ){` |
|        - | 4078 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|        - | 4079 | `				 * running inside it. */` |
|        4 | 4080 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|        2 | 4081 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|        2 | 4082 | `			}else{` |
|      ! 0 | 4083 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|        - | 4084 | `			}` |
|        3 | 4085 | `			bFrame = 1;` |
|        1 | 4086 | `		}` |
|        - | 4087 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|        7 | 4088 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|        2 | 4089 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|        2 | 4090 | `	}` |
|      604 | 4091 | `	if( bLast && pFile ){` |
|      598 | 4092 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|      598 | 4093 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      297 | 4094 | `	}` |
|      604 | 4095 | `}` |
|        - | 4096 | `/*` |
|        - | 4097 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|        - | 4098 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|        - | 4099 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|        - | 4100 | ` */` |
|        4 | 4101 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 | 4102 | `{` |
|        - | 4103 | `	SyBlob sOut;` |
|        - | 4104 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|        - | 4105 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|        5 | 4106 | `	pVm->iExitStatus = 255;` |
|        5 | 4107 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 4108 | `		return PH7_OK;` |
|        - | 4109 | `	}` |
|        5 | 4110 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        5 | 4111 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|        5 | 4112 | `	VmCallErrorHandler(pVm,&sOut);` |
|        5 | 4113 | `	SyBlobRelease(&sOut);` |
|        5 | 4114 | `	return PH7_ABORT;` |
|        3 | 4115 | `}` |
|        - | 4116 | `/*` |
|        - | 4117 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|        - | 4118 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|        - | 4119 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|        - | 4120 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|        - | 4121 | ` */` |
|        - | 4122 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|      596 | 4123 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|        4 | 4124 | `{` |
|        - | 4125 | `	ph7_value *pValue;` |
|        - | 4126 | `	ph7_class_instance *pPrev;` |
|        - | 4127 | `	ph7_class *pThrowable;` |
|      600 | 4128 | `	if( pThis == 0 ){` |
|      ! 0 | 4129 | `		return 0;` |
|        - | 4130 | `	}` |
|      600 | 4131 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      600 | 4132 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      594 | 4133 | `		return 0;` |
|        - | 4134 | `	}` |
|        8 | 4135 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 4136 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|        - | 4137 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|        - | 4138 | `	 * never renders a stray object as an exception entry. */` |
|        8 | 4139 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|        8 | 4140 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|      ! 0 | 4141 | `		return 0;` |
|        - | 4142 | `	}` |
|        8 | 4143 | `	return pPrev;` |
|      302 | 4144 | `}` |
|        - | 4145 | `/*` |
|        - | 4146 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|        - | 4147 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|        - | 4148 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|        - | 4149 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|        - | 4150 | ` */` |
|       16 | 4151 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|        3 | 4152 | `{` |
|        - | 4153 | `	ph7_value *pValue;` |
|       19 | 4154 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|      ! 0 | 4155 | `		return;` |
|        - | 4156 | `	}` |
|       19 | 4157 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|       19 | 4158 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        3 | 4159 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|        - | 4160 | `	}` |
|       17 | 4161 | `	pPrev->iRef++;` |
|        - | 4162 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|        - | 4163 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|       17 | 4164 | `	PH7_MemObjRelease(pValue);` |
|       17 | 4165 | `	pValue->x.pOther = pPrev;` |
|       17 | 4166 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|       11 | 4167 | `}` |
|        - | 4168 | `/*` |
|        - | 4169 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|        - | 4170 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|        - | 4171 | ` * absent or yields an empty string.` |
|        - | 4172 | ` */` |
|        - | 4173 | `/*` |
|        - | 4174 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|        - | 4175 | ` * 0 when the class exposes no getLine().` |
|        - | 4176 | ` */` |
|      596 | 4177 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        4 | 4178 | `{` |
|        - | 4179 | `	ph7_class_method *pGetLine;` |
|        - | 4180 | `	ph7_value sLine;` |
|      600 | 4181 | `	sxu32 nLine = 0;` |
|      600 | 4182 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|      600 | 4183 | `	if( pGetLine == 0 ){` |
|      ! 0 | 4184 | `		return 0;` |
|        - | 4185 | `	}` |
|      600 | 4186 | `	PH7_MemObjInit(pVm,&sLine);` |
|      600 | 4187 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|      600 | 4188 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|      600 | 4189 | `		if( n > 0 ){` |
|      600 | 4190 | `			nLine = (sxu32)n;` |
|      298 | 4191 | `		}` |
|      298 | 4192 | `	}` |
|      600 | 4193 | `	PH7_MemObjRelease(&sLine);` |
|      600 | 4194 | `	return nLine;` |
|      302 | 4195 | `}` |
|      596 | 4196 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 4197 | `{` |
|        - | 4198 | `	ph7_class_method *pGetMessage;` |
|        - | 4199 | `	ph7_value sMsg;` |
|        - | 4200 | `	const char *zTmp;` |
|        - | 4201 | `	int nTmp;` |
|      600 | 4202 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      600 | 4203 | `	if( pGetMessage == 0 ){` |
|      ! 0 | 4204 | `		return;` |
|        - | 4205 | `	}` |
|      600 | 4206 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      600 | 4207 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      600 | 4208 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      600 | 4209 | `		if( zTmp && nTmp > 0 ){` |
|      600 | 4210 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      298 | 4211 | `		}` |
|      298 | 4212 | `	}` |
|      600 | 4213 | `	PH7_MemObjRelease(&sMsg);` |
|      302 | 4214 | `}` |
|        - | 4215 | `/*` |
|        - | 4216 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|        - | 4217 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|        - | 4218 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|        - | 4219 | ` * outermost (the actually-uncaught) exception.` |
|        - | 4220 | ` *` |
|        - | 4221 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|        - | 4222 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|        - | 4223 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|        - | 4224 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|        - | 4225 | ` */` |
|        - | 4226 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|      590 | 4227 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|        4 | 4228 | `{` |
|        - | 4229 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|      594 | 4230 | `	int nChain = 0;` |
|        - | 4231 | `	int i;` |
|        - | 4232 | `	SyBlob sOut;` |
|        - | 4233 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|        - | 4234 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|        - | 4235 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|      594 | 4236 | `	pVm->iExitStatus = 255;` |
|      594 | 4237 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 4238 | `		return PH7_OK;` |
|        - | 4239 | `	}` |
|        - | 4240 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|        - | 4241 | `	 * collected) or the hard cap. */` |
|     1190 | 4242 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|      608 | 4243 | `		for( i = 0 ; i < nChain ; ++i ){` |
|       10 | 4244 | `			if( apChain[i] == pThis ){` |
|      ! 0 | 4245 | `				pThis = 0; /* cycle: stop the walk */` |
|      ! 0 | 4246 | `				break;` |
|        - | 4247 | `			}` |
|        6 | 4248 | `		}` |
|      600 | 4249 | `		if( pThis == 0 ){` |
|      ! 0 | 4250 | `			break;` |
|        - | 4251 | `		}` |
|      600 | 4252 | `		apChain[nChain++] = pThis;` |
|      600 | 4253 | `		pThis = VmExceptionGetPrevious(pThis);` |
|        4 | 4254 | `	}` |
|      594 | 4255 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        - | 4256 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|        - | 4257 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|     1190 | 4258 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|      600 | 4259 | `		ph7_class_instance *pEnt = apChain[i];` |
|        - | 4260 | `		SyBlob sMsg;` |
|      600 | 4261 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      600 | 4262 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|      898 | 4263 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|      596 | 4264 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|      596 | 4265 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|      298 | 4266 | `			zFuncName,nFuncLen,` |
|      596 | 4267 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|      298 | 4268 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|      298 | 4269 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|      600 | 4270 | `		SyBlobRelease(&sMsg);` |
|      302 | 4271 | `	}` |
|      594 | 4272 | `	VmCallErrorHandler(pVm,&sOut);` |
|      594 | 4273 | `	SyBlobRelease(&sOut);` |
|      594 | 4274 | `	return PH7_ABORT;` |
|      299 | 4275 | `}` |
|        - | 4276 | `/*` |
|        - | 4277 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - | 4278 | ` *` |
|        - | 4279 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - | 4280 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - | 4281 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - | 4282 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - | 4283 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - | 4284 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - | 4285 | ` */` |
|  1563861 | 4286 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 | 4287 | `{` |
|  1563866 | 4288 | `	if( pVm->bCoalesceArmed ){` |
|       13 | 4289 | `		if( pVm->pCoalesceObj ){` |
|       13 | 4290 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        5 | 4291 | `		}` |
|       13 | 4292 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       13 | 4293 | `		pVm->pCoalesceObj = 0;` |
|       13 | 4294 | `		pVm->bCoalesceArmed = 0;` |
|        5 | 4295 | `	}` |
|  1563866 | 4296 | `}` |
|        - | 4297 | `/*` |
|        - | 4298 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - | 4299 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - | 4300 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - | 4301 | ` * build the SyBlob themselves and pass its data + length.` |
|        - | 4302 | ` *` |
|        - | 4303 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - | 4304 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - | 4305 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - | 4306 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - | 4307 | ` */` |
|   144560 | 4308 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|        - | 4309 | `	ph7_vm *pVm,` |
|        - | 4310 | `	const char *zClass,` |
|        - | 4311 | `	const char *zMsg,` |
|        - | 4312 | `	sxu32 nMsg` |
|        5 | 4313 | `){` |
|        - | 4314 | `	ph7_class *pClass;` |
|        - | 4315 | `	ph7_class_instance *pThis;` |
|        - | 4316 | `	ph7_class_method *pCons;` |
|        - | 4317 | `	VmFrame *pFrame;` |
|        - | 4318 | `	sxi32 rc;` |
|   144565 | 4319 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|   144565 | 4320 | `	if( pClass == 0 ){` |
|      ! 0 | 4321 | `		return SXERR_ABORT;` |
|        - | 4322 | `	}` |
|   144565 | 4323 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|   144565 | 4324 | `	if( pThis == 0 ){` |
|      ! 0 | 4325 | `		return SXERR_ABORT;` |
|        - | 4326 | `	}` |
|   144565 | 4327 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   144565 | 4328 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|        - | 4329 | `		ph7_value sArg;` |
|        - | 4330 | `		ph7_value *apArg[1];` |
|        - | 4331 | `		SyString sMsgStr;` |
|   144565 | 4332 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|   144565 | 4333 | `		PH7_MemObjInit(pVm,&sArg);` |
|   144565 | 4334 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|   144565 | 4335 | `		apArg[0] = &sArg;` |
|   144565 | 4336 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|   144565 | 4337 | `		PH7_MemObjRelease(&sArg);` |
|   144565 | 4338 | `		pVm->nExcCtorDepth--;` |
|    72280 | 4339 | `	}` |
|   144565 | 4340 | `	pFrame = pVm->pFrame;` |
|   144565 | 4341 | `	if( pFrame ){` |
|   144565 | 4342 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   144565 | 4343 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    72280 | 4344 | `	}` |
|   144565 | 4345 | `	rc = VmThrowException(pVm,pThis);` |
|   144565 | 4346 | `	PH7_ClassInstanceUnref(pThis);` |
|   144565 | 4347 | `	return rc;` |
|    72285 | 4348 | `}` |
|        - | 4349 | `/*` |
|        - | 4350 | ` * A native compare handler REFUSED the pair (ph7_class::xCmp wrote a class name` |
|        - | 4351 | ` * into its context, which PH7_ClassNativeCmp parked on the VM). php raises that` |
|        - | 4352 | ` * exception out of the comparison itself; PH7_MemObjCmp cannot, because it is` |
|        - | 4353 | ` * also the comparator sort(), in_array(), max() and switch drive, none of which` |
|        - | 4354 | ` * is a throw boundary. So the refusal waits here until a site that CAN route a` |
|        - | 4355 | ` * throw asks for it — the comparison opcodes and the switch arm raise it where` |
|        - | 4356 | ` * the expression's value would have landed, and the host-call boundary raises it` |
|        - | 4357 | ` * on the builtin's own context, which is where every other builtin throw is` |
|        - | 4358 | ` * reported from. Both doors clear it first: a raise that itself unwinds must not` |
|        - | 4359 | ` * leave the record standing for the next comparison to fire again.` |
|        - | 4360 | ` */` |
|  7358405 | 4361 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm)` |
|        5 | 4362 | `{` |
|  7358410 | 4363 | `	return pVm->zCmpRefusalClass != 0;` |
|        5 | 4364 | `}` |
|     5808 | 4365 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm)` |
|        5 | 4366 | `{` |
|     5813 | 4367 | `	pVm->zCmpRefusalClass = 0;` |
|     5813 | 4368 | `	pVm->zCmpRefusalMsg[0] = 0;` |
|     5813 | 4369 | `}` |
|       36 | 4370 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm)` |
|        1 | 4371 | `{` |
|       37 | 4372 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 4373 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       37 | 4374 | `	if( zClass == 0 ){` |
|      ! 0 | 4375 | `		return SXRET_OK;` |
|        - | 4376 | `	}` |
|       37 | 4377 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       37 | 4378 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       37 | 4379 | `	return VmThrowFromVm(&(*pVm),zClass,zMsg,(sxu32)SyStrlen(zMsg));` |
|       19 | 4380 | `}` |
|       16 | 4381 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx)` |
|        1 | 4382 | `{` |
|       17 | 4383 | `	ph7_vm *pVm = pCtx->pVm;` |
|       17 | 4384 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 4385 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       17 | 4386 | `	if( zClass == 0 ){` |
|      ! 0 | 4387 | `		return SXRET_OK;` |
|        - | 4388 | `	}` |
|       17 | 4389 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       17 | 4390 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       17 | 4391 | `	return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|        9 | 4392 | `}` |
|        - | 4393 | `/*` |
|        - | 4394 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|        - | 4395 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|        - | 4396 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|        - | 4397 | ` *` |
|        - | 4398 | ` *   int/float/bool/null      arithmetic proceeds` |
|        - | 4399 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|        - | 4400 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|        - | 4401 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|        - | 4402 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|        - | 4403 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|        - | 4404 | ` *   object/resource          TypeError, naming the object's CLASS` |
|        - | 4405 | ` *` |
|        - | 4406 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|        - | 4407 | ` */` |
|        - | 4408 | `/*` |
|        - | 4409 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|        - | 4410 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|        - | 4411 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|        - | 4412 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|        - | 4413 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|        - | 4414 | ` * to depth 1).` |
|        - | 4415 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|        - | 4416 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|        - | 4417 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|        - | 4418 | ` * frame shape is file/line/function[/class/type], matching the default` |
|        - | 4419 | ` * zend.exception_ignore_args=On.` |
|        - | 4420 | ` */` |
|  1463779 | 4421 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|        5 | 4422 | `{` |
|        - | 4423 | `	SyString *pFile;` |
|        - | 4424 | `	VmFrame *pFrame;` |
|        - | 4425 | `	ph7_value *pValue;` |
|  1463784 | 4426 | `	sxi32 nDone = 0;` |
|  1463784 | 4427 | `	pValue = ph7_new_scalar(&(*pVm));` |
|  1463784 | 4428 | `	if( pValue == 0 ){` |
|      ! 0 | 4429 | `		return;` |
|        - | 4430 | `	}` |
|  1463784 | 4431 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  1463784 | 4432 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  2102296 | 4433 | `	while( pFrame ){` |
|        - | 4434 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|        - | 4435 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|        - | 4436 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|        - | 4437 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
|  2102296 | 4438 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|       39 | 4439 | `			break;` |
|        - | 4440 | `		}` |
|  2102258 | 4441 | `		nDone++;` |
|  2102258 | 4442 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        - | 4443 | `		ph7_value *pEntry;` |
|  2102258 | 4444 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|        - | 4445 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   731875 | 4446 | `			break;` |
|        - | 4447 | `		}` |
|   638517 | 4448 | `		pEntry = ph7_new_array(&(*pVm));` |
|   638517 | 4449 | `		if( pEntry == 0 ){` |
|      ! 0 | 4450 | `			break;` |
|        - | 4451 | `		}` |
|        - | 4452 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|        - | 4453 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|        - | 4454 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|        - | 4455 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|        - | 4456 | `		 * include-stack top for a call made at global scope. */` |
|        - | 4457 | `		{` |
|   638517 | 4458 | `			SyString *pFrameFile = pFile;` |
|   638517 | 4459 | `			if( pFrame->pParent->pUserData ){` |
|      483 | 4460 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|      483 | 4461 | `				if( pCaller->sFile.nByte > 0 ){` |
|      483 | 4462 | `					pFrameFile = &pCaller->sFile;` |
|      239 | 4463 | `				}` |
|      239 | 4464 | `			}` |
|   638517 | 4465 | `			if( pFrameFile ){` |
|   638517 | 4466 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   638517 | 4467 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   638517 | 4468 | `				ph7_value_reset_string_cursor(pValue);` |
|   319256 | 4469 | `			}` |
|        - | 4470 | `		}` |
|   638517 | 4471 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   638517 | 4472 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|        - | 4473 | `		{` |
|   638517 | 4474 | `			const char *zDisp = 0;` |
|   638517 | 4475 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   638517 | 4476 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|        - | 4477 | `		}` |
|   638517 | 4478 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   638517 | 4479 | `		ph7_value_reset_string_cursor(pValue);` |
|        - | 4480 | `		{` |
|        - | 4481 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|        - | 4482 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|        - | 4483 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|        - | 4484 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|        - | 4485 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|        - | 4486 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|        - | 4487 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   638517 | 4488 | `			SyString *pClsName = 0;` |
|   638517 | 4489 | `			const char *zType = "->";` |
|   638517 | 4490 | `			int bStatic = 0;` |
|   638517 | 4491 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 4492 | `				/* php's separator says what the CALLEE is, not how the caller` |
|        - | 4493 | ``				 * happened to reach it: a static method is `::` even when the`` |
|        - | 4494 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|        - | 4495 | ``				 * instance method), which the pThis test reported as `->`.`` |
|        - | 4496 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|        - | 4497 | `				 * own flags are one cast away. */` |
|   500455 | 4498 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   500455 | 4499 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|   500455 | 4500 | `				zType = bStatic ? "::" : "->";` |
|   388292 | 4501 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|      ! 0 | 4502 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|      ! 0 | 4503 | `			}` |
|   638517 | 4504 | `			if( pClsName ){` |
|   500455 | 4505 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|   500455 | 4506 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|   500455 | 4507 | `				ph7_value_reset_string_cursor(pValue);` |
|   500455 | 4508 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|   500455 | 4509 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|   500455 | 4510 | `				ph7_value_reset_string_cursor(pValue);` |
|   500450 | 4511 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis` |
|       24 | 4512 | `				 && !bStatic ){` |
|       19 | 4513 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       19 | 4514 | `					if( pObjVal ){` |
|       19 | 4515 | `						pFrame->pThis->iRef++;` |
|       19 | 4516 | `						pObjVal->x.pOther = pFrame->pThis;` |
|       19 | 4517 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       19 | 4518 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|       19 | 4519 | `						ph7_release_value(&(*pVm),pObjVal);` |
|        8 | 4520 | `					}` |
|        8 | 4521 | `				}` |
|   250225 | 4522 | `			}` |
|        - | 4523 | `		}` |
|   638517 | 4524 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      117 | 4525 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      117 | 4526 | `			if( pArg ){` |
|        - | 4527 | `				/* The arguments the caller actually PASSED, which is not the same` |
|        - | 4528 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|        - | 4529 | `				 * (shared with func_get_args()). */` |
|      117 | 4530 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg);` |
|      117 | 4531 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      117 | 4532 | `				ph7_release_value(&(*pVm),pArg);` |
|       57 | 4533 | `			}` |
|       57 | 4534 | `		}` |
|   638517 | 4535 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   638517 | 4536 | `		ph7_release_value(&(*pVm),pEntry);` |
|   638517 | 4537 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|        5 | 4538 | `	}` |
|  1463784 | 4539 | `	ph7_release_value(&(*pVm),pValue);` |
|   731894 | 4540 | `}` |
|        - | 4541 | `/*` |
|        - | 4542 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|        - | 4543 | ` *` |
|        - | 4544 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|        - | 4545 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|        - | 4546 | ` * calls parent::__construct still reports the right position. The embedded` |
|        - | 4547 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|        - | 4548 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|        - | 4549 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|        - | 4550 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|        - | 4551 | ` */` |
|  1605215 | 4552 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        5 | 4553 | `{` |
|        - | 4554 | `	static const char *azField[] = { "file", "line", "trace" };` |
|        - | 4555 | `	ph7_class *pThrowable;` |
|        - | 4556 | `	SyString *pFile;` |
|        - | 4557 | `	SyString *pSiteFile;` |
|        - | 4558 | `	sxu32 n;` |
|  1605220 | 4559 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|      ! 0 | 4560 | `		return;` |
|        - | 4561 | `	}` |
|  1605220 | 4562 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|  1605220 | 4563 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   141537 | 4564 | `		return;` |
|        - | 4565 | `	}` |
|  1463688 | 4566 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  1463688 | 4567 | `	pSiteFile = pFile;` |
|        - | 4568 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|        - | 4569 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|        - | 4570 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|        - | 4571 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|        - | 4572 | `	{` |
|  1463688 | 4573 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  1463688 | 4574 | `		if( pInner && pInner->pUserData ){` |
|   633703 | 4575 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|   633703 | 4576 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|   633611 | 4577 | `				pSiteFile = &pInnerFunc->sFile;` |
|   316803 | 4578 | `			}` |
|   316849 | 4579 | `		}` |
|        - | 4580 | `	}` |
|  5854737 | 4581 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|        - | 4582 | `		SyHashEntry *pEntry;` |
|        - | 4583 | `		VmClassAttr *pVmAttr;` |
|        - | 4584 | `		ph7_value *pAttrValue;` |
|  4391054 | 4585 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|  4391054 | 4586 | `		if( pEntry == 0 ){` |
|      ! 0 | 4587 | `			continue;` |
|        - | 4588 | `		}` |
|  4391054 | 4589 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  4391054 | 4590 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  4391054 | 4591 | `		if( pAttrValue == 0 ){` |
|      ! 0 | 4592 | `			continue;` |
|        - | 4593 | `		}` |
|  4391054 | 4594 | `		if( n == 0 ){` |
|  1463688 | 4595 | `			if( pSiteFile ){` |
|  1463688 | 4596 | `				PH7_MemObjRelease(pAttrValue);` |
|  1463688 | 4597 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   731846 | 4598 | `			}` |
|  3659212 | 4599 | `		}else if( n == 1 ){` |
|        - | 4600 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|        - | 4601 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|        - | 4602 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|        - | 4603 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|        - | 4604 | `			 * off everything the initializer calls — an autoloader, a nested` |
|        - | 4605 | `			 * constant's evaluation — which report their own lines in both engines.` |
|        - | 4606 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|        - | 4607 | `			 * PHL already matches.) */` |
|  1463688 | 4608 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|       40 | 4609 | `				? pVm->nLazyInitLine` |
|  1463684 | 4610 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
|  1463688 | 4611 | `			PH7_MemObjRelease(pAttrValue);` |
|  1463688 | 4612 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|   731846 | 4613 | `		}else{` |
|        - | 4614 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|        - | 4615 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|        - | 4616 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|        - | 4617 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|  1463688 | 4618 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|  1463688 | 4619 | `			if( pList == 0 ){` |
|      ! 0 | 4620 | `				continue;` |
|        - | 4621 | `			}` |
|  1463688 | 4622 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,0,pList);` |
|        - | 4623 | `			/* Building the trace reserves new memobjs, which may realloc` |
|        - | 4624 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|        - | 4625 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|        - | 4626 | `			 * AFTER the walk before releasing/storing into it. */` |
|  1463688 | 4627 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  1463688 | 4628 | `			if( pAttrValue ){` |
|  1463688 | 4629 | `				PH7_MemObjRelease(pAttrValue);` |
|  1463688 | 4630 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   731841 | 4631 | `			}` |
|  1463688 | 4632 | `			ph7_release_value(&(*pVm),pList);` |
|        - | 4633 | `		}` |
|  2195528 | 4634 | `	}` |
|   802612 | 4635 | `}` |
|     6886 | 4636 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|        3 | 4637 | `{` |
|     6889 | 4638 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       54 | 4639 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       54 | 4640 | `		if( pInst && pInst->pClass ){` |
|       54 | 4641 | `			return pInst->pClass->sName.zString;` |
|        - | 4642 | `		}` |
|      ! 0 | 4643 | `	}` |
|     6837 | 4644 | `	return ph7_type_name(pVal);` |
|     3446 | 4645 | `}` |
|        - | 4646 | `/*` |
|        - | 4647 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|        - | 4648 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|        - | 4649 | ` * describes an operand it could not use as one ("on false", "false given").` |
|        - | 4650 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|        - | 4651 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|        - | 4652 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|        - | 4653 | ` * instead of replacing it.` |
|        - | 4654 | ` */` |
|      120 | 4655 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|        3 | 4656 | `{` |
|      123 | 4657 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|       19 | 4658 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 4659 | `	}` |
|      105 | 4660 | `	return VmArithTypeName(&(*pVal));` |
|       63 | 4661 | `}` |
|        - | 4662 | `/*` |
|        - | 4663 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|        - | 4664 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|        - | 4665 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|        - | 4666 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|        - | 4667 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|        - | 4668 | ` */` |
|       38 | 4669 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|        1 | 4670 | `{` |
|       39 | 4671 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|       39 | 4672 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       17 | 4673 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       17 | 4674 | `		if( pInst && pInst->pClass ){` |
|       17 | 4675 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|       17 | 4676 | `			return;` |
|        - | 4677 | `		}` |
|      ! 0 | 4678 | `	}` |
|       23 | 4679 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|       20 | 4680 | `}` |
|        - | 4681 | `/*` |
|        - | 4682 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|        - | 4683 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|        - | 4684 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|        - | 4685 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|        - | 4686 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|        - | 4687 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|        - | 4688 | ` */` |
|      250 | 4689 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|        1 | 4690 | `{` |
|      251 | 4691 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      251 | 4692 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      251 | 4693 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      251 | 4694 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      251 | 4695 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|        - | 4696 | `	sxu32 i;` |
|      523 | 4697 | `	for( i = 0 ; i < nMin ; ++i ){` |
|        - | 4698 | `		unsigned char c;` |
|      273 | 4699 | `		if( cOp == '\|' ){` |
|       93 | 4700 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      227 | 4701 | `		}else if( cOp == '^' ){` |
|       89 | 4702 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|       45 | 4703 | `		}else{` |
|       93 | 4704 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|        - | 4705 | `		}` |
|      273 | 4706 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      137 | 4707 | `	}` |
|      251 | 4708 | `	if( cOp == '\|' && nL != nR ){` |
|       63 | 4709 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|       63 | 4710 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|       63 | 4711 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       31 | 4712 | `	}` |
|      251 | 4713 | `}` |
|        - | 4714 | `/*` |
|        - | 4715 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|        - | 4716 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|        - | 4717 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|        - | 4718 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|        - | 4719 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|        - | 4720 | ` * null, which is what an undefined variable answers.` |
|        - | 4721 | ` */` |
|       26 | 4722 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|        1 | 4723 | `{` |
|       27 | 4724 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       11 | 4725 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       11 | 4726 | `		if( pInst && pInst->pClass ){` |
|       11 | 4727 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|       11 | 4728 | `			return;` |
|        - | 4729 | `		}` |
|      ! 0 | 4730 | `	}` |
|       17 | 4731 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 | 4732 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|        4 | 4733 | `			pVal->x.iVal ? "true" : "false");` |
|       15 | 4734 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|       11 | 4735 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|        6 | 4736 | `	}else{` |
|        3 | 4737 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|        - | 4738 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|        - | 4739 | `	}` |
|       14 | 4740 | `}` |
|        - | 4741 | `/*` |
|        - | 4742 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|        - | 4743 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|        - | 4744 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|        - | 4745 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|        - | 4746 | ` * computes with the prefix. Classify pVal against that contract.` |
|        - | 4747 | ` */` |
|    95080 | 4748 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|        5 | 4749 | `{` |
|        - | 4750 | `	ph7_value sInt;` |
|        - | 4751 | `	sxi32 rc;` |
|    95085 | 4752 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|    95085 | 4753 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|    95085 | 4754 | `	PH7_MemObjRelease(&sInt);` |
|    95085 | 4755 | `	return rc;` |
|        5 | 4756 | `}` |
|        - | 4757 | `/*` |
|        - | 4758 | ` * One arithmetic operator's whole prologue: php's do_operation handler first,` |
|        - | 4759 | ` * then the ordinary operand contract.` |
|        - | 4760 | ` *` |
|        - | 4761 | ` * php asks the LEFT operand's class for a handler and falls back to the RIGHT` |
|        - | 4762 | `` * one's, which is why an `int + Number` works as well as a `Number + int`. A`` |
|        - | 4763 | ` * handler that answers writes into pDest (the slot the opcode was going to` |
|        - | 4764 | ` * leave its result in), so the caller's only job is to skip the numeric` |
|        - | 4765 | ` * arithmetic. A handler that REFUSES hands back an exception class and a` |
|        - | 4766 | ` * message, and the caller throws them where it would have thrown the TypeError` |
|        - | 4767 | ` * -- after settling the operand stack.` |
|        - | 4768 | ` */` |
|        - | 4769 | ``/* Does this value's class declare php's do_operation? `++`/`--` ask before they`` |
|        - | 4770 | ` * refuse an object, since everything below that refusal is numeric. */` |
|   824783 | 4771 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal)` |
|        5 | 4772 | `{` |
|        - | 4773 | `	ph7_class_instance *pInst;` |
|   824788 | 4774 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|   824754 | 4775 | `		return 0;` |
|        - | 4776 | `	}` |
|       35 | 4777 | `	pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       35 | 4778 | `	return pInst->pClass != 0 && pInst->pClass->xArith != 0;` |
|   412957 | 4779 | `}` |
|   526408 | 4780 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|        - | 4781 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut)` |
|        5 | 4782 | `{` |
|   526413 | 4783 | `	ph7_class_instance *pInst = 0;` |
|        - | 4784 | `	int i;` |
|   526413 | 4785 | `	*pzClass = "TypeError";` |
|  1579099 | 4786 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1052759 | 4787 | `		ph7_value *pSide = i == 0 ? pLeft : pRight;` |
|  1052759 | 4788 | `		if( (pSide->iFlags & MEMOBJ_OBJ) != 0 && pSide->x.pOther ){` |
|       72 | 4789 | `			ph7_class_instance *pCand = (ph7_class_instance *)pSide->x.pOther;` |
|       72 | 4790 | `			if( pCand->pClass && pCand->pClass->xArith ){` |
|       70 | 4791 | `				pInst = pCand;` |
|       70 | 4792 | `				break;` |
|        - | 4793 | `			}` |
|        1 | 4794 | `		}` |
|   527034 | 4795 | `	}` |
|   526413 | 4796 | `	if( pInst ){` |
|        - | 4797 | `		PH7_NativeArithCtx sCtx;` |
|        - | 4798 | `		ph7_value sRes;` |
|       70 | 4799 | `		PH7_MemObjInit(&(*pVm),&sRes);` |
|       70 | 4800 | `		sCtx.zOp = zOp;` |
|       70 | 4801 | `		sCtx.pLeft = pLeft;` |
|       70 | 4802 | `		sCtx.pRight = pRight;` |
|       70 | 4803 | `		sCtx.pResult = &sRes;` |
|       70 | 4804 | `		sCtx.bHandled = 0;` |
|       70 | 4805 | `		sCtx.zThrowClass = 0;` |
|       70 | 4806 | `		sCtx.zThrowMsg[0] = 0;` |
|       70 | 4807 | `		pInst->pClass->xArith(&(*pVm),pInst,&sCtx);` |
|       70 | 4808 | `		if( sCtx.zThrowClass ){` |
|       11 | 4809 | `			PH7_MemObjRelease(&sRes);` |
|       11 | 4810 | `			*pzClass = sCtx.zThrowClass;` |
|       11 | 4811 | `			SyBlobAppend(pMsgOut,sCtx.zThrowMsg,(sxu32)SyStrlen(sCtx.zThrowMsg));` |
|       32 | 4812 | `			return PH7_ARITH_REFUSED;` |
|        - | 4813 | `		}` |
|       60 | 4814 | `		if( sCtx.bHandled ){` |
|       44 | 4815 | `			PH7_MemObjStore(&sRes,pDest);` |
|       44 | 4816 | `			PH7_MemObjRelease(&sRes);` |
|       44 | 4817 | `			return PH7_ARITH_HANDLED;` |
|        - | 4818 | `		}` |
|       18 | 4819 | `		PH7_MemObjRelease(&sRes);` |
|        8 | 4820 | `	}` |
|   526361 | 4821 | `	if( VmArithOperandCheck(&(*pVm),pLeft,pRight,zOp,pMsgOut) != SXRET_OK ){` |
|     1888 | 4822 | `		return PH7_ARITH_REFUSED;` |
|        - | 4823 | `	}` |
|   524475 | 4824 | `	return PH7_ARITH_ORDINARY;` |
|   263552 | 4825 | `}` |
|   629430 | 4826 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|        5 | 4827 | `{` |
|   629435 | 4828 | `	int bBadL = 0, bBadR = 0;` |
|        - | 4829 | `	int i;` |
|        - | 4830 | `	ph7_value *apOperand[2];` |
|   629435 | 4831 | `	apOperand[0] = pLeft;` |
|   629435 | 4832 | `	apOperand[1] = pRight;` |
|        - | 4833 | `	/* array + array is php's union operator, not arithmetic */` |
|   629430 | 4834 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   461605 | 4835 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|     5293 | 4836 | `		return SXRET_OK;` |
|        - | 4837 | `	}` |
|  1867159 | 4838 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1246395 | 4839 | `		ph7_value *pVal = apOperand[i];` |
|  1246395 | 4840 | `		int bBad = 0;` |
|  1246395 | 4841 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     1246 | 4842 | `			bBad = 1;` |
|  1245773 | 4843 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     4623 | 4844 | `			const char *zTail = 0;` |
|     4623 | 4845 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     4623 | 4846 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|        - | 4847 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     2135 | 4848 | `				bBad = 1;` |
|     1068 | 4849 | `			}else{` |
|        - | 4850 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|        - | 4851 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|        - | 4852 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     2517 | 4853 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       30 | 4854 | `					zTail++;` |
|        2 | 4855 | `				}` |
|     2489 | 4856 | `				if( zTail < zEnd ){` |
|     1187 | 4857 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      592 | 4858 | `				}` |
|        - | 4859 | `			}` |
|     2310 | 4860 | `		}` |
|  1246395 | 4861 | `		if( bBad ){` |
|     3380 | 4862 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|        - | 4863 | `			/* php converts the operands one at a time and STOPS at the first one it` |
|        - | 4864 | `			 * refuses — the second is never looked at, so it never says anything about` |
|        - | 4865 | ``			 * it. `"abc" + "5x"` is the TypeError alone, where walking both operands`` |
|        - | 4866 | ``			 * first announced `A non-numeric value encountered` for the "5x" php never`` |
|        - | 4867 | ``			 * reached. The other order is unaffected: `"5x" + "abc"` warns for the left`` |
|        - | 4868 | `			 * operand and then throws, in both engines. */` |
|     3380 | 4869 | `			break;` |
|        - | 4870 | `		}` |
|   622197 | 4871 | `	}` |
|   624147 | 4872 | `	if( bBadL \|\| bBadR ){` |
|        - | 4873 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|        - | 4874 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|        - | 4875 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     5069 | 4876 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|     1689 | 4877 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     3380 | 4878 | `		return SXERR_INVALID;` |
|        - | 4879 | `	}` |
|   620769 | 4880 | `	return SXRET_OK;` |
|   315063 | 4881 | `}` |
|        - | 4882 | `/*` |
|        - | 4883 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - | 4884 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|        - | 4885 | ` * pass 0 for the engine errors that leave it at its default.` |
|        - | 4886 | ` */` |
|    11573 | 4887 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|        5 | 4888 | `{` |
|        - | 4889 | `	ph7_vm *pVm;` |
|        - | 4890 | `	ph7_class *pClass;` |
|        - | 4891 | `	ph7_class_instance *pThis;` |
|        - | 4892 | `	ph7_class_method *pCons;` |
|        - | 4893 | `	ph7_value sArg,sCode;` |
|        - | 4894 | `	ph7_value *apArg[2];` |
|        - | 4895 | `	SyBlob sMsg;` |
|        - | 4896 | `	SyString sMsgStr;` |
|        - | 4897 | `	VmFrame *pFrame;` |
|        - | 4898 | `	sxi32 rc;` |
|        - | 4899 |  |
|    11578 | 4900 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 4901 | `		return PH7_ABORT;` |
|        - | 4902 | `	}` |
|    11578 | 4903 | `	pVm = pCtx->pVm;` |
|    11578 | 4904 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 4905 | `		zClass = "Error";` |
|      ! 0 | 4906 | `	}` |
|        - | 4907 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|        - | 4908 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|        - | 4909 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|        - | 4910 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|        - | 4911 | `	 * reported error (VmHostFuncThrowRc). */` |
|    11578 | 4912 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    11578 | 4913 | `	if( pClass == 0 ){` |
|      ! 0 | 4914 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 4915 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 | 4916 | `			zClass` |
|        - | 4917 | `			);` |
|      ! 0 | 4918 | `		return pCtx->nThrowRc;` |
|        - | 4919 | `	}` |
|    11578 | 4920 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    11578 | 4921 | `	if( pThis == 0 ){` |
|      ! 0 | 4922 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 4923 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - | 4924 | `			);` |
|      ! 0 | 4925 | `		return pCtx->nThrowRc;` |
|        - | 4926 | `	}` |
|        - | 4927 |  |
|    11578 | 4928 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    11578 | 4929 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - | 4930 |  |
|    11578 | 4931 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    11578 | 4932 | `	if( pCons ){` |
|    11578 | 4933 | `		int nArg = 1;` |
|    11578 | 4934 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    11578 | 4935 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    11578 | 4936 | `		apArg[0] = &sArg;` |
|    11578 | 4937 | `		if( iCode != 0 ){` |
|      422 | 4938 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      422 | 4939 | `			apArg[1] = &sCode;` |
|      422 | 4940 | `			nArg = 2;` |
|      210 | 4941 | `		}` |
|    11578 | 4942 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    11578 | 4943 | `		if( iCode != 0 ){` |
|      422 | 4944 | `			PH7_MemObjRelease(&sCode);` |
|      210 | 4945 | `		}` |
|    11578 | 4946 | `		PH7_MemObjRelease(&sArg);` |
|     5786 | 4947 | `	}` |
|    11578 | 4948 | `	SyBlobRelease(&sMsg);` |
|        - | 4949 |  |
|    11578 | 4950 | `	pFrame = pVm->pFrame;` |
|    11578 | 4951 | `	if( pFrame ){` |
|    11578 | 4952 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    11578 | 4953 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     5786 | 4954 | `	}` |
|    11578 | 4955 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    11578 | 4956 | `	PH7_ClassInstanceUnref(pThis);` |
|    11578 | 4957 | `	if( rc == SXERR_ABORT ){` |
|      529 | 4958 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      529 | 4959 | `		return PH7_ABORT;` |
|        - | 4960 | `	}` |
|        - | 4961 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|        - | 4962 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|        - | 4963 | `	 * argument-validation helper whose callers have no status channel — would` |
|        - | 4964 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|        - | 4965 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|        - | 4966 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|        - | 4967 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|        - | 4968 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|        - | 4969 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    11054 | 4970 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    11054 | 4971 | `	return PH7_EXCEPTION;` |
|     5791 | 4972 | `}` |
|    11153 | 4973 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 | 4974 | `{` |
|        - | 4975 | `	va_list ap;` |
|        - | 4976 | `	sxi32 rc;` |
|    11158 | 4977 | `	va_start(ap,zFormat);` |
|    11158 | 4978 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    11158 | 4979 | `	va_end(ap);` |
|    11158 | 4980 | `	return rc;` |
|        5 | 4981 | `}` |
|        - | 4982 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      420 | 4983 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|        2 | 4984 | `{` |
|        - | 4985 | `	va_list ap;` |
|        - | 4986 | `	sxi32 rc;` |
|      422 | 4987 | `	va_start(ap,zFormat);` |
|      422 | 4988 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      422 | 4989 | `	va_end(ap);` |
|      422 | 4990 | `	return rc;` |
|        2 | 4991 | `}` |
|        - | 4992 | `/*` |
|        - | 4993 | ` * The status a host function's own throw should have returned. Consulted at the` |
|        - | 4994 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|        - | 4995 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|        - | 4996 | ` * and is the identity when the routine never threw or already reported it.` |
|        - | 4997 | ` *` |
|        - | 4998 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|        - | 4999 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|        - | 5000 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|        - | 5001 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|        - | 5002 | ` */` |
|  6043519 | 5003 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|        5 | 5004 | `{` |
|  6043519 | 5005 | `	if( pCtx->nThrowRc == 0` |
|  3028216 | 5006 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
|  6039048 | 5007 | `		return rc;` |
|        - | 5008 | `	}` |
|     4480 | 5009 | `	return pCtx->nThrowRc;` |
|  3023298 | 5010 | `}` |
|        - | 5011 | `/*` |
|        - | 5012 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - | 5013 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - | 5014 | ` */` |
|      ! 0 | 5015 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 | 5016 | `{` |
|        - | 5017 | `	ph7_vm *pVm;` |
|        - | 5018 | `	SyBlob sMsg;` |
|      ! 0 | 5019 | `	const char *zFuncName = 0;` |
|      ! 0 | 5020 | `	int nFuncLen = 0;` |
|        - | 5021 | `	va_list ap;` |
|        - | 5022 | `	sxi32 rc;` |
|        - | 5023 |  |
|      ! 0 | 5024 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 5025 | `		return PH7_OK;` |
|        - | 5026 | `	}` |
|      ! 0 | 5027 | `	pVm = pCtx->pVm;` |
|      ! 0 | 5028 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 5029 | `		zClass = "Error";` |
|      ! 0 | 5030 | `	}` |
|        - | 5031 |  |
|      ! 0 | 5032 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 5033 |  |
|      ! 0 | 5034 | `	va_start(ap,zFormat);` |
|      ! 0 | 5035 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 | 5036 | `	va_end(ap);` |
|        - | 5037 |  |
|      ! 0 | 5038 | `	if( pCtx->pFunc ){` |
|      ! 0 | 5039 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 | 5040 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 | 5041 | `	}` |
|      ! 0 | 5042 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 | 5043 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 | 5044 | `	}` |
|      ! 0 | 5045 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 | 5046 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 | 5047 | `	SyBlobRelease(&sMsg);` |
|      ! 0 | 5048 | `	return rc;` |
|      ! 0 | 5049 | `}` |
|        - | 5050 | `/*` |
|        - | 5051 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 5052 | ` * exception is triggered.` |
|        - | 5053 | ` */` |
|      592 | 5054 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|        - | 5055 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 5056 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 5057 | `	)` |
|        4 | 5058 | `{` |
|        - | 5059 | `	ph7_value *apArg[2],sArg;` |
|      596 | 5060 | `	int nArg = 1;` |
|        - | 5061 | `	sxi32 rc;` |
|      596 | 5062 | `	if( pVm->nMuteThrow > 0 ){` |
|        - | 5063 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|        - | 5064 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|        - | 5065 | `		 * observable: no exception handler runs, no report is printed and the` |
|        - | 5066 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|        - | 5067 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|      ! 0 | 5068 | `		return SXERR_ABORT;` |
|        - | 5069 | `	}` |
|      596 | 5070 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 5071 | `		/* Nesting limit reached */` |
|      ! 0 | 5072 | `		return SXRET_OK;` |
|        - | 5073 | `	}` |
|        - | 5074 | `	/* Call any exception handler if available */` |
|      596 | 5075 | `	PH7_MemObjInit(pVm,&sArg);` |
|      596 | 5076 | `	if( pThis ){` |
|        - | 5077 | `		/* Load the exception instance */` |
|      596 | 5078 | `		sArg.x.pOther = pThis;` |
|      596 | 5079 | `		pThis->iRef++;` |
|      596 | 5080 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      300 | 5081 | `	}else{` |
|      ! 0 | 5082 | `		nArg = 0;` |
|        - | 5083 | `	}` |
|      596 | 5084 | `	apArg[0] = &sArg;` |
|        - | 5085 | `	/* Call the exception handler if available */` |
|      596 | 5086 | `	pVm->nExceptDepth++;` |
|        - | 5087 | `	{` |
|        - | 5088 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|        - | 5089 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|        - | 5090 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|        - | 5091 | `		ph7_value sRunning;` |
|      596 | 5092 | `		PH7_MemObjInit(pVm,&sRunning);` |
|      596 | 5093 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|      596 | 5094 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|      596 | 5095 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|      596 | 5096 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|      596 | 5097 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|      596 | 5098 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|      296 | 5099 | `		}` |
|      596 | 5100 | `		PH7_MemObjRelease(&sRunning);` |
|        - | 5101 | `	}` |
|      596 | 5102 | `	pVm->nExceptDepth--;` |
|      596 | 5103 | `	if( rc != SXRET_OK ){` |
|        - | 5104 | `		const char *zFuncName;` |
|        - | 5105 | `		int nFuncLen;` |
|      594 | 5106 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        - | 5107 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|      594 | 5108 | `		if( pThis ){` |
|        - | 5109 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|        - | 5110 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|        - | 5111 | `			 * renders byte-identically to the historical single-entry report. */` |
|      594 | 5112 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|      299 | 5113 | `		}else{` |
|        - | 5114 | `			/* No instance (internal report path) — default-class single entry. */` |
|      ! 0 | 5115 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|        - | 5116 | `		}` |
|        - | 5117 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      594 | 5118 | `		rc = SXERR_ABORT;` |
|      295 | 5119 | `	}` |
|      596 | 5120 | `	PH7_MemObjRelease(&sArg);` |
|      596 | 5121 | `	return rc;` |
|      300 | 5122 | `}` |
|        - | 5123 | `/*` |
|        - | 5124 | ` * Throw a user exception.` |
|        - | 5125 | ` *` |
|        - | 5126 | ` * Exception dispatch follows this sequence:` |
|        - | 5127 | ` *` |
|        - | 5128 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 5129 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 5130 | ` *` |
|        - | 5131 | ` * 2. If NO catch matches:` |
|        - | 5132 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 5133 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 5134 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 5135 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 5136 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 5137 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 5138 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 5139 | ` *` |
|        - | 5140 | ` * 3. If a catch DOES match:` |
|        - | 5141 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 5142 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 5143 | ` *       inside the catch body from immediately propagating past our` |
|        - | 5144 | ` *       finally block.` |
|        - | 5145 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 5146 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 5147 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 5148 | ` *       in pPendingException (step 2c).` |
|        - | 5149 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 5150 | ` *    d. Run finally (if present).` |
|        - | 5151 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 5152 | ` *       that handlers are restored and finally has run.` |
|        - | 5153 | ` */` |
|        - | 5154 | `/*` |
|        - | 5155 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|        - | 5156 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|        - | 5157 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|        - | 5158 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|        - | 5159 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|        - | 5160 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|        - | 5161 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|        - | 5162 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|        - | 5163 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|        - | 5164 | ` */` |
|      172 | 5165 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|        5 | 5166 | `{` |
|      187 | 5167 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|       74 | 5168 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       74 | 5169 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|       74 | 5170 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|       14 | 5171 | `			break; /* reached an outer exec's / legacy handler */` |
|        - | 5172 | `		}` |
|       48 | 5173 | `		(void)SySetPop(&pVm->aException);` |
|       48 | 5174 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|       48 | 5175 | `		if( pT->iHasFinally ){` |
|       37 | 5176 | `			*pPc = pT->iFinallyPc;` |
|       37 | 5177 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|       37 | 5178 | `			return 1;` |
|        - | 5179 | `		}` |
|        - | 5180 | `		/* No finally: tear the try's transparent frame down now. */` |
|       14 | 5181 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        6 | 5182 | `			VmLeaveFrame(&(*pVm));` |
|        2 | 5183 | `		}` |
|       14 | 5184 | `		VmExcRelease(&(*pVm),pT);` |
|        4 | 5185 | `	}` |
|      143 | 5186 | `	return 0;` |
|       91 | 5187 | `}` |
|        - | 5188 | `/*` |
|        - | 5189 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|        - | 5190 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|        - | 5191 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|        - | 5192 | ` *` |
|        - | 5193 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|        - | 5194 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|        - | 5195 | ` *    and redirect to the catch body (iHandlerPc).` |
|        - | 5196 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|        - | 5197 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|        - | 5198 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|        - | 5199 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|        - | 5200 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|        - | 5201 | ` */` |
|        - | 5202 | `/*` |
|        - | 5203 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|        - | 5204 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|        - | 5205 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|        - | 5206 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|        - | 5207 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|        - | 5208 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|        - | 5209 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|        - | 5210 | ` * case) is unchanged: no wrapper.` |
|        - | 5211 | ` */` |
|    20254 | 5212 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|        5 | 5213 | `{` |
|    20259 | 5214 | `	VmFrame *pWrap = 0;` |
|        - | 5215 | `	VmFrame *pThrowSite;` |
|        - | 5216 | `	sxi32 rc;` |
|    20259 | 5217 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    20155 | 5218 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5219 | `	}` |
|      107 | 5220 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|        - | 5221 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|      ! 0 | 5222 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5223 | `	}` |
|      107 | 5224 | `	pThrowSite = pWrap->pParent;` |
|      107 | 5225 | `	pWrap->pParent = pOwner;` |
|      107 | 5226 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|      107 | 5227 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5228 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|        - | 5229 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|        - | 5230 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|        - | 5231 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|      107 | 5232 | `	if( pVm->pFrame == pWrap ){` |
|      107 | 5233 | `		VmLeaveFrame(&(*pVm));` |
|       52 | 5234 | `	}` |
|      107 | 5235 | `	pVm->pFrame = pThrowSite;` |
|      107 | 5236 | `	return rc;` |
|    10132 | 5237 | `}` |
|        - | 5238 | `/*` |
|        - | 5239 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|        - | 5240 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|        - | 5241 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|        - | 5242 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|        - | 5243 | ` */` |
|        - | 5244 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|       94 | 5245 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - | 5246 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|        5 | 5247 | `{` |
|       99 | 5248 | `	if( pCatch ){` |
|       83 | 5249 | `		pException->iInCatch = 1;` |
|       83 | 5250 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|       83 | 5251 | `		if( pThis ){ pThis->iRef++; }` |
|       83 | 5252 | `		pException->pInflight = pThis;` |
|       83 | 5253 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       83 | 5254 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|       83 | 5255 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       83 | 5256 | `		return SXRET_OK;` |
|        - | 5257 | `	}` |
|       20 | 5258 | `	if( pException->iHasFinally ){` |
|        - | 5259 | `		VmFinallyAction sAct;` |
|       15 | 5260 | `		SyZero(&sAct,sizeof(sAct));` |
|       15 | 5261 | `		sAct.eKind = PH7_FA_RETHROW;` |
|       15 | 5262 | `		if( pThis ){ pThis->iRef++; }` |
|       15 | 5263 | `		sAct.pExc = pThis;` |
|       15 | 5264 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       15 | 5265 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       15 | 5266 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|       15 | 5267 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       15 | 5268 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       15 | 5269 | `		return SXRET_OK;` |
|        - | 5270 | `	}` |
|        - | 5271 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|        - | 5272 | `	 * flat native stack instead of mutual recursion. */` |
|        6 | 5273 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      ! 0 | 5274 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 5275 | `	}` |
|        6 | 5276 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|        6 | 5277 | `	return VM_THROW_KEEP_UNWINDING;` |
|       52 | 5278 | `}` |
|  1463601 | 5279 | `PH7_PRIVATE sxi32 VmThrowException(` |
|        - | 5280 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 5281 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 5282 | `	)` |
|        5 | 5283 | `{` |
|        - | 5284 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 5285 | `	ph7_exception **apException;` |
|   731800 | 5286 | `	ph7_exception *pException;` |
|    50104 | 5287 | `Rethrow:` |
|        - | 5288 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|        - | 5289 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|        - | 5290 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|        - | 5291 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|        - | 5292 | `	 * so the throw path must be too). */` |
|        - | 5293 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 5294 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 5295 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|  1563814 | 5296 | `	VmCoalesceDisarm(pVm);` |
|        - | 5297 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|        - | 5298 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|        - | 5299 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|        - | 5300 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|        - | 5301 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|        - | 5302 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|        - | 5303 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|        - | 5304 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|  1563809 | 5305 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|       25 | 5306 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|       19 | 5307 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|        8 | 5308 | `	}` |
|        - | 5309 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|        - | 5310 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|        - | 5311 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|        - | 5312 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|        - | 5313 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|        - | 5314 | `	 * that owns the pending return, so it must leave that return intact. */` |
|        - | 5315 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|        - | 5316 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|        - | 5317 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|        - | 5318 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|  1563814 | 5319 | `	pVm->pResumeFrame = 0;` |
|        - | 5320 | `	/* Point to the stack of loaded exceptions */` |
|  1563814 | 5321 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|  1563814 | 5322 | `	pException = 0;` |
|  1563814 | 5323 | `	pCatch = 0;` |
|  1563814 | 5324 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 5325 | `		ph7_exception_block *aCatch;` |
|        - | 5326 | `		ph7_class *pClass;` |
|        - | 5327 | `		SyString *aNames;` |
|        - | 5328 | `		sxu32 nNames;` |
|        - | 5329 | `		int matched;` |
|        - | 5330 | `		sxu32 j,k;` |
|        - | 5331 | `		/* Locate the appropriate block to execute */` |
|  1463098 | 5332 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|  1463098 | 5333 | `		(void)SySetPop(&pVm->aException);` |
|  1463098 | 5334 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|        - | 5335 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|        - | 5336 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|        - | 5337 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|  1463118 | 5338 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 5339 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|  1442926 | 5340 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|  1442926 | 5341 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|  1442926 | 5342 | `			matched = 0;` |
|  1442972 | 5343 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 5344 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 5345 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 5346 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|  1442952 | 5347 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|  1442952 | 5348 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 5349 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 5350 | `					continue;` |
|        - | 5351 | `				}` |
|  1442952 | 5352 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|  1442906 | 5353 | `					matched = 1;` |
|  1442906 | 5354 | `					break;` |
|        - | 5355 | `				}` |
|       27 | 5356 | `			}` |
|  1442926 | 5357 | `			if( matched ){` |
|        - | 5358 | `				/* Catch block found,break immediately */` |
|  1442906 | 5359 | `				pCatch = &aCatch[j];` |
|  1442906 | 5360 | `				break;` |
|        - | 5361 | `			}` |
|       13 | 5362 | `		}` |
|   731546 | 5363 | `	}` |
|        - | 5364 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|        - | 5365 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|        - | 5366 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|        - | 5367 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|        - | 5368 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|        - | 5369 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|        - | 5370 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|  1563814 | 5371 | `	if( pException ){` |
|  1463098 | 5372 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   731546 | 5373 | `	}` |
|        - | 5374 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|        - | 5375 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|        - | 5376 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|  1563814 | 5377 | `	if( pException && pException->iInlined ){` |
|       99 | 5378 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|       99 | 5379 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|        - | 5380 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|        6 | 5381 | `			goto Rethrow;` |
|        - | 5382 | `		}` |
|       95 | 5383 | `		return rcInline;` |
|        - | 5384 | `	}` |
|        - | 5385 | `	/* Execute the cached block if available */` |
|  1563720 | 5386 | `	if( pCatch == 0 ){` |
|        - | 5387 | `		sxi32 rc;` |
|        - | 5388 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|   120897 | 5389 | `		if( pException && pException->iHasFinally ){` |
|    20175 | 5390 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    20175 | 5391 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    20175 | 5392 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|    20175 | 5393 | `			pException->iFinallyDone = 1;` |
|        - | 5394 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|        - | 5395 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    20175 | 5396 | `			pVm->pInflightException = pThis;` |
|    20175 | 5397 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 5398 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|        - | 5399 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    20175 | 5400 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    20175 | 5401 | `			pVm->pInflightException = pSaveInflight;` |
|    20175 | 5402 | `			pVm->nInflightExcBase = nSaveBase;` |
|    20175 | 5403 | `			if( rc == SXERR_ABORT ){` |
|        3 | 5404 | `				VmExcRelease(&(*pVm),pException);` |
|        3 | 5405 | `				return SXERR_ABORT;` |
|        - | 5406 | `			}` |
|        - | 5407 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|        - | 5408 | `			 * semantics). The finally stored it on the body frame it returns from` |
|        - | 5409 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|        - | 5410 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|        - | 5411 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|        - | 5412 | `			 * takes the value instead of unwinding) and resume in place.` |
|        - | 5413 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|        - | 5414 | `			 * the same transport an in-place catch uses — and unwind as an` |
|        - | 5415 | `			 * exception; the owner's activation consumes the resume` |
|        - | 5416 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|        - | 5417 | `			 * its bHasRet tail materializes the return. */` |
|        - | 5418 | `			{` |
|    20173 | 5419 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    20173 | 5420 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    20173 | 5421 | `				if( pOwnerFrame->bHasRet ){` |
|    20029 | 5422 | `					if( pOwnerFrame == pThrowFrame ){` |
|    20026 | 5423 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|        - | 5424 | `						/* Record the landing pad like the cross-frame case below.` |
|        - | 5425 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|        - | 5426 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|        - | 5427 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|        - | 5428 | `						 * its router unwound as an exception and the Unwind discard` |
|        - | 5429 | ``						 * dropped the parked return — `function f(){ try {`` |
|        - | 5430 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|        - | 5431 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|        - | 5432 | `						 * try frame down and its bHasRet tail materializes the` |
|        - | 5433 | `						 * return, same as the in-place-catch landing. */` |
|    20026 | 5434 | `						pVm->pResumeFrame = pOwnerFrame;` |
|    20026 | 5435 | `						pVm->iResumePc = pException->iLandingPc;` |
|    20026 | 5436 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|    20026 | 5437 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|    20026 | 5438 | `						VmExcRelease(&(*pVm),pException);` |
|    20026 | 5439 | `						return SXRET_OK;` |
|        - | 5440 | `					}` |
|        3 | 5441 | `					pVm->pResumeFrame = pOwnerFrame;` |
|        3 | 5442 | `					pVm->iResumePc = pException->iLandingPc;` |
|        3 | 5443 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|        3 | 5444 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|        3 | 5445 | `					VmExcRelease(&(*pVm),pException);` |
|        3 | 5446 | `					return PH7_EXCEPTION;` |
|        - | 5447 | `				}` |
|        - | 5448 | `			}` |
|        - | 5449 | `			/* The finally threw an exception that superseded pThis — it either` |
|        - | 5450 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|        - | 5451 | `			 * (which consumed an entry from the exception stack). Either way the` |
|        - | 5452 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|        - | 5453 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|      147 | 5454 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       17 | 5455 | `				VmExcRelease(&(*pVm),pException);` |
|       17 | 5456 | `				return PH7_EXCEPTION;` |
|        - | 5457 | `			}` |
|       64 | 5458 | `		}` |
|        - | 5459 | `		/* Check if there is an outer exception handler on the stack */` |
|   100855 | 5460 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 5461 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|        - | 5462 | `			 * iteration per unwound level instead of one native frame. */` |
|      139 | 5463 | `			VmExcRelease(&(*pVm),pException);` |
|      139 | 5464 | `			goto Rethrow;` |
|        - | 5465 | `		}` |
|   100721 | 5466 | `		if( pVm->nMuteThrow > 0 ){` |
|        - | 5467 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|        - | 5468 | `			 * default at class mount, which php would not have evaluated yet).` |
|        - | 5469 | `			 * Nothing outside the initializer may observe this throw: no` |
|        - | 5470 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|        - | 5471 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|        - | 5472 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|        - | 5473 | `			 * the mount path rolls the whole attempt back. */` |
|       53 | 5474 | `			VmExcRelease(&(*pVm),pException);` |
|       53 | 5475 | `			return SXERR_ABORT;` |
|        - | 5476 | `		}` |
|        - | 5477 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 5478 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 5479 | `		 * exception instead of reporting it uncaught.` |
|        - | 5480 | `		 */` |
|   100671 | 5481 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 5482 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 5483 | `			 * by looking for a catch frame on the stack.` |
|        - | 5484 | `			 */` |
|   100671 | 5485 | `			VmFrame *pF = pVm->pFrame;` |
|   100671 | 5486 | `			int inCatch = 0;` |
|   101309 | 5487 | `			while( pF ){` |
|   100717 | 5488 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|   100078 | 5489 | `					inCatch = 1;` |
|   100078 | 5490 | `					break;` |
|        - | 5491 | `				}` |
|      642 | 5492 | `				pF = pF->pParent;` |
|        4 | 5493 | `			}` |
|   100671 | 5494 | `			if( inCatch ){` |
|        - | 5495 | `				/* Defer — will be re-thrown after finally runs */` |
|   100078 | 5496 | `				pThis->iRef++;` |
|   100078 | 5497 | `				pVm->pPendingException = pThis;` |
|   100078 | 5498 | `				VmExcRelease(&(*pVm),pException);` |
|   100078 | 5499 | `				return SXRET_OK;` |
|        - | 5500 | `			}` |
|      296 | 5501 | `		}` |
|        - | 5502 | `		/* Truly uncaught */` |
|      596 | 5503 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      596 | 5504 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 5505 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 5506 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 5507 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 5508 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 5509 | `			}` |
|      ! 0 | 5510 | `		}` |
|      596 | 5511 | `		VmExcRelease(&(*pVm),pException);` |
|      596 | 5512 | `		return rc;` |
|      ! 0 | 5513 | `	}else{` |
|  1442828 | 5514 | `		VmFrame *pFrame = pVm->pFrame;` |
|  1442828 | 5515 | `		ph7_exception **apSaved = 0;` |
|        - | 5516 | `		sxu32 nSavedCount;` |
|        - | 5517 | `		sxi32 rc;` |
|        - | 5518 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|        - | 5519 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|        - | 5520 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|        - | 5521 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|        - | 5522 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|  1442828 | 5523 | `		VmFrame *pCatchBody = pException->pFrame;` |
|  1442828 | 5524 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|  1442828 | 5525 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|  1442828 | 5526 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  1442828 | 5527 | `		if( pException->pFrame == pFrame ){` |
|   833678 | 5528 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|   416836 | 5529 | `		}` |
|        - | 5530 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 5531 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 5532 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 5533 | `		 */` |
|  1442828 | 5534 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|  1442828 | 5535 | `		if( nSavedCount > 0 ){` |
|   150443 | 5536 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    50146 | 5537 | `				nSavedCount * sizeof(ph7_exception *));` |
|   100297 | 5538 | `			if( apSaved ){` |
|   150443 | 5539 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|    50146 | 5540 | `					nSavedCount * sizeof(ph7_exception *));` |
|   100297 | 5541 | `				SySetReset(&pVm->aException);` |
|    50146 | 5542 | `			}` |
|    50146 | 5543 | `		}` |
|        - | 5544 | `		/* Create the catch frame (made transparent below) */` |
|  1442828 | 5545 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|  1442828 | 5546 | `		if( rc == SXRET_OK ){` |
|        - | 5547 | `			ph7_value *pObj;` |
|        - | 5548 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|        - | 5549 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|        - | 5550 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|        - | 5551 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|        - | 5552 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|        - | 5553 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|        - | 5554 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|        - | 5555 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|        - | 5556 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|  1442828 | 5557 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|        - | 5558 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|        - | 5559 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|        - | 5560 | `			 * against the live current scope rather than a freed frame. */` |
|  1442828 | 5561 | `			if( pCatchBody ){` |
|  1442828 | 5562 | `				pFrame->pParent = pCatchBody;` |
|   721411 | 5563 | `			}` |
|        - | 5564 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 5565 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 5566 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 5567 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 5568 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 5569 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 5570 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 5571 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|  1442828 | 5572 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|        - | 5573 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|  2164240 | 5574 | `			pObj = (pCatch->sThis.nByte > 0)` |
|  1442821 | 5575 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|  1442828 | 5576 | `			if( pObj ){` |
|        - | 5577 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 5578 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 5579 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 5580 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 5581 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 5582 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|  1442824 | 5583 | `				pThis->iRef++;` |
|  1442824 | 5584 | `				PH7_MemObjRelease(pObj);` |
|  1442824 | 5585 | `				pObj->x.pOther = pThis;` |
|  1442824 | 5586 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   721409 | 5587 | `			}` |
|        - | 5588 | `			/* Execute the catch block */` |
|  1442828 | 5589 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|        - | 5590 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|        - | 5591 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|        - | 5592 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|        - | 5593 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|        - | 5594 | `			 * unbalanced — never pop somebody else's frame. */` |
|  1442828 | 5595 | `			if( pVm->pFrame == pFrame ){` |
|  1442828 | 5596 | `				VmLeaveFrame(&(*pVm));` |
|   721411 | 5597 | `			}` |
|  1442828 | 5598 | `			pVm->pFrame = pThrowSite;` |
|   721411 | 5599 | `		}` |
|        - | 5600 | `		/* Restore the outer exception handlers */` |
|  1442828 | 5601 | `		if( apSaved ){` |
|        - | 5602 | `			sxu32 k;` |
|        - | 5603 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|        - | 5604 | `			 * the catch body) are normally already consumed; on an abnormal` |
|        - | 5605 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|        - | 5606 | `			 * linger — release those activations before discarding the set. */` |
|   100297 | 5607 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|   100297 | 5608 | `			SySetReset(&pVm->aException);` |
|   201243 | 5609 | `			for(k = 0; k < nSavedCount; k++){` |
|   100951 | 5610 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    50478 | 5611 | `			}` |
|   100297 | 5612 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|    50146 | 5613 | `		}` |
|        - | 5614 | `		/* Execute the finally block after catch */` |
|  1442828 | 5615 | `		if( pException->iHasFinally ){` |
|        - | 5616 | `			sxi32 rcf;` |
|        - | 5617 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|        - | 5618 | `			 * from, its pending-return write generation (set if the catch above` |
|        - | 5619 | `			 * returned), and the exception-stack depth. After the finally we use` |
|        - | 5620 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|        - | 5621 | `			 * catch-return. */` |
|        - | 5622 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|        - | 5623 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|        - | 5624 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|        - | 5625 | `			 * supersede decision belong to the owner, not the thrower. */` |
|       89 | 5626 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|       89 | 5627 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|       89 | 5628 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|        - | 5629 | `			/* The exception in flight while this finally runs is the catch body's` |
|        - | 5630 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|        - | 5631 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|        - | 5632 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|        - | 5633 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|       89 | 5634 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|       89 | 5635 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|       89 | 5636 | `			pException->iFinallyDone = 1;` |
|       89 | 5637 | `			pVm->pInflightException = pVm->pPendingException;` |
|       89 | 5638 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 5639 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|       89 | 5640 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|       89 | 5641 | `			pVm->pInflightException = pSaveInflight;` |
|       89 | 5642 | `			pVm->nInflightExcBase = nSaveBase;` |
|       89 | 5643 | `			if( rcf == SXERR_ABORT ){` |
|      ! 0 | 5644 | `				VmExcRelease(&(*pVm),pException);` |
|      ! 0 | 5645 | `				return SXERR_ABORT;` |
|        - | 5646 | `			}` |
|        - | 5647 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|        - | 5648 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|        - | 5649 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|        - | 5650 | `			 * either case that exception supersedes this try's catch-return — but` |
|        - | 5651 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|        - | 5652 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|        - | 5653 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|       89 | 5654 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       19 | 5655 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|       12 | 5656 | `					VmClearFramePending(pBody);` |
|        5 | 5657 | `				}` |
|        - | 5658 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|        - | 5659 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|        - | 5660 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|        - | 5661 | `				 * legitimately have re-armed it during the finally. */` |
|       19 | 5662 | `				pBody->nCatchJmpPc = 0;` |
|        8 | 5663 | `			}` |
|       89 | 5664 | `			if( rcf == PH7_EXCEPTION ){` |
|        - | 5665 | `				/* The finally's exception propagated past this try; drop any deferred` |
|        - | 5666 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|        - | 5667 | `				 * reaches the frame that caught the finally's throw. */` |
|       19 | 5668 | `				if( pVm->pPendingException ){` |
|      ! 0 | 5669 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      ! 0 | 5670 | `					pVm->pPendingException = 0;` |
|      ! 0 | 5671 | `				}` |
|       19 | 5672 | `				VmExcRelease(&(*pVm),pException);` |
|       19 | 5673 | `				return PH7_EXCEPTION;` |
|        - | 5674 | `			}` |
|       34 | 5675 | `		}` |
|  1442812 | 5676 | `		if( rc == SXERR_ABORT ){` |
|        5 | 5677 | `			VmExcRelease(&(*pVm),pException);` |
|        5 | 5678 | `			return SXERR_ABORT;` |
|        - | 5679 | `		}` |
|        - | 5680 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 5681 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 5682 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 5683 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|        - | 5684 | `		 * the catch frame having been left above), which swallows the in-flight` |
|        - | 5685 | `		 * exception (PHP semantics).` |
|        - | 5686 | `		 */` |
|  1442808 | 5687 | `		if( pVm->pPendingException ){` |
|        - | 5688 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|   100078 | 5689 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|   100078 | 5690 | `			if( !pOwner->bHasRet ){` |
|   100074 | 5691 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        - | 5692 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|        - | 5693 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|        - | 5694 | `				 * than leave it armed for an unrelated later landing. */` |
|   100074 | 5695 | `				pOwner->nCatchJmpPc = 0;` |
|   100074 | 5696 | `				pVm->pPendingException = 0;` |
|   100074 | 5697 | `				VmExcRelease(&(*pVm),pException);` |
|        - | 5698 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|   100074 | 5699 | `				pThis = pReThrow;` |
|   100074 | 5700 | `				goto Rethrow;` |
|        - | 5701 | `			}` |
|        - | 5702 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|        6 | 5703 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        6 | 5704 | `			pVm->pPendingException = 0;` |
|        2 | 5705 | `		}` |
|        - | 5706 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|        - | 5707 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|        - | 5708 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|        - | 5709 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|  1342738 | 5710 | `		pVm->pResumeFrame = pCatchBody;` |
|  1342738 | 5711 | `		pVm->iResumePc = iCatchPc;` |
|  1342738 | 5712 | `		pVm->pResumeInstr = pCatchInstr;` |
|  1342738 | 5713 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|        - | 5714 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|        - | 5715 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|        - | 5716 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|  1342738 | 5717 | `		VmExcRelease(&(*pVm),pException);` |
|        - | 5718 | `	}` |
|  1342738 | 5719 | `	return SXRET_OK;` |
|   731805 | 5720 | `}` |
|        - | 5721 |  |
