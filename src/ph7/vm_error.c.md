# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2540/2829 lines (89.78%)

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
|    26378 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|        5 |   25 | `{` |
|    26383 |   26 | `	pVm->nLastErrType = iErr;` |
|    26383 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|    26383 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    26383 |   29 | `	if( zMsg && nMsg > 0 ){` |
|    26383 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|    13189 |   31 | `	}` |
|    26383 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    26383 |   33 | `	if( pFile ){` |
|    26383 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|    13189 |   35 | `	}` |
|    26383 |   36 | `}` |
|        - |   37 | `/*` |
|        - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|        - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|        - |   40 | ` * diagnostic is never silently swallowed.` |
|        - |   41 | ` */` |
|      804 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|        4 |   43 | `{` |
|      808 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|        4 |   45 | `}` |
|        - |   46 | `/*` |
|        - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|        - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|        - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|        - |   50 | ` * headers_sent()/output accounting).` |
|        - |   51 | ` */` |
|      830 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|        4 |   53 | `{` |
|        - |   54 | `	sxi32 rc;` |
|        - |   55 | `	/* Append a new line */` |
|        - |   56 | `#ifdef __WINNT__` |
|        4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |   58 | `#else` |
|      830 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |   60 | `#endif` |
|        - |   61 | `	/* Invoke the output consumer callback */` |
|      834 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      834 |   63 | `	if( bTrack ){` |
|       30 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|       13 |   65 | `	}` |
|      834 |   66 | `	return rc;` |
|        4 |   67 | `}` |
|        - |   68 | `/*` |
|        - |   69 | ` * Route an already-formatted diagnostic blob (the uncaught-exception path builds` |
|        - |   70 | `` * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream`` |
|        - |   71 | ` * when log_errors is on, else to the program-output stream when display_errors` |
|        - |   72 | ` * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).` |
|        - |   73 | ` */` |
|      592 |   74 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |   75 | `{` |
|      596 |   76 | `	if( pVm->bLogErrors ){` |
|      596 |   77 | `		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);` |
|        - |   78 | `	}` |
|      ! 0 |   79 | `	if( pVm->bDisplayErrors ){` |
|      ! 0 |   80 | `		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);` |
|        - |   81 | `	}` |
|      ! 0 |   82 | `	return SXRET_OK;` |
|      300 |   83 | `}` |
|        - |   84 | `/*` |
|        - |   85 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |   86 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |   87 | ` * information.` |
|        - |   88 | ` */` |
|    28655 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |   90 | `{` |
|        - |   91 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|        - |   92 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|        - |   93 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|        - |   94 | `	 * straight through to the engine's own reporting, which is what returning` |
|        - |   95 | `	 * TRUE below means. */` |
|    28655 |   96 | `	if( ph7_value_is_callable(&pVm->sErrCB)` |
|    15481 |   97 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
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
|     2286 |  108 | `		if( iErr == PH7_CTX_NOTICE ){` |
|      323 |  109 | `			iErr = 8; /* E_NOTICE */` |
|      159 |  110 | `		}` |
|        - |  111 | `		/* Prepare arguments */` |
|     2286 |  112 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  113 | `			/* use explicit message length to avoid reading past buffer */` |
|     2286 |  114 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     2286 |  115 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     2286 |  116 | `		if( pFile ){` |
|     2286 |  117 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     2286 |  118 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     1141 |  119 | `		}else{` |
|      ! 0 |  120 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  121 | `		}` |
|     2286 |  122 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     2286 |  123 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  124 | `		/* Set up pointer array */` |
|     2286 |  125 | `		apArgPtr[0] = &apArg[0];` |
|     2286 |  126 | `		apArgPtr[1] = &apArg[1];` |
|     2286 |  127 | `		apArgPtr[2] = &apArg[2];` |
|     2286 |  128 | `		apArgPtr[3] = &apArg[3];` |
|        - |  129 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|        - |  130 | `		 * handler itself raises reaches the engine's reporting instead of` |
|        - |  131 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|        - |  132 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|        - |  133 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|        - |  134 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|        - |  135 | `		 * the handler installed itself STAYS. */` |
|     2286 |  136 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     2286 |  137 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|     2286 |  138 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|     2286 |  139 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|        - |  140 | `		/* Call the handler */` |
|        - |  141 | `		{` |
|     2286 |  142 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|     2286 |  143 | `			if( !ph7_value_is_callable(&pVm->sErrCB) ){` |
|     2284 |  144 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     1135 |  145 | `			}` |
|     2286 |  146 | `			PH7_MemObjRelease(&sRunning);` |
|     2286 |  147 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
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
|     2282 |  162 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  163 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  164 | `		}` |
|        - |  165 | `		/* Release */` |
|     2282 |  166 | `		PH7_MemObjRelease(&apArg[0]);` |
|     2282 |  167 | `		PH7_MemObjRelease(&apArg[1]);` |
|     2282 |  168 | `		PH7_MemObjRelease(&apArg[2]);` |
|     2282 |  169 | `		PH7_MemObjRelease(&apArg[3]);` |
|     2282 |  170 | `		PH7_MemObjRelease(&sResult);` |
|        - |  171 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  172 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     2282 |  173 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  174 | `	}` |
|        - |  175 | `	/* No handler, always call error handler */` |
|    26379 |  176 | `	return TRUE;` |
|    14328 |  177 | `}` |
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
|    24389 |  195 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|        5 |  196 | `{` |
|    24394 |  197 | `	switch( iErr ){` |
|    11845 |  198 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|    23686 |  199 | `		return 2;` |
|       21 |  200 | `	case 512  /* E_USER_WARNING */:` |
|       44 |  201 | `		return 512;` |
|      199 |  202 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|        - |  203 | `	case 8    /* E_NOTICE */:` |
|      403 |  204 | `		return 8;` |
|       35 |  205 | `	case 1024 /* E_USER_NOTICE */:` |
|       74 |  206 | `		return 1024;` |
|       64 |  207 | `	case 8192 /* E_DEPRECATED */:` |
|      132 |  208 | `		return 8192;` |
|       21 |  209 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       43 |  210 | `		return 16384;` |
|      ! 0 |  211 | `	case 256  /* E_USER_ERROR */:` |
|      ! 0 |  212 | `		return 256;` |
|       14 |  213 | `	default:` |
|       32 |  214 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|        - |  215 | `	}` |
|    12195 |  216 | `}` |
|    26378 |  217 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|        5 |  218 | `{` |
|    26383 |  219 | `	if( !pVm->bErrReport ){` |
|     4289 |  220 | `		return 0;` |
|        - |  221 | `	}` |
|    22097 |  222 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|    13194 |  223 | `}` |
|      238 |  224 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|        4 |  225 | `{` |
|      242 |  226 | `	switch(iErr){` |
|       86 |  227 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|        - |  228 | `	case 512  /* E_USER_WARNING */:` |
|      176 |  229 | `		return "Warning";` |
|       19 |  230 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|        - |  231 | `	case 8    /* E_NOTICE */:` |
|        - |  232 | `	case 1024 /* E_USER_NOTICE */:` |
|       42 |  233 | `		return "Notice";` |
|      ! 0 |  234 | `	case 8192  /* E_DEPRECATED */:` |
|        - |  235 | `	case 16384 /* E_USER_DEPRECATED */:` |
|      ! 0 |  236 | `		return "Deprecated";` |
|      ! 0 |  237 | `	case 256 /* E_USER_ERROR */:` |
|      ! 0 |  238 | `		return "Fatal error";` |
|       14 |  239 | `	default:` |
|       32 |  240 | `		return "Error";` |
|        - |  241 | `	}` |
|      123 |  242 | `}` |
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
|      238 |  254 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|        4 |  255 | `{` |
|      242 |  256 | `	if( pFile ){` |
|      361 |  257 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|      119 |  258 | `			nLine ? nLine : 1);` |
|      119 |  259 | `	}` |
|      242 |  260 | `}` |
|        - |  261 | `/*` |
|        - |  262 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|        - |  263 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|        - |  264 | ` * (compile.c) and stock CLI's stderr log copy.` |
|        - |  265 | ` */` |
|      212 |  266 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  267 | `{` |
|      216 |  268 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      216 |  269 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      216 |  270 | `}` |
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
|    28029 |  283 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|        5 |  284 | `{` |
|    28034 |  285 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     1044 |  286 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     1044 |  287 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      515 |  288 | `	}` |
|    28034 |  289 | `}` |
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
|      234 |  303 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|        - |  304 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|        4 |  305 | `{` |
|      238 |  306 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      238 |  307 | `	sxi32 rc = SXRET_OK;` |
|      238 |  308 | `	if( pVm->bLogErrors ){` |
|      216 |  309 | `		SyBlobReset(pWorker);` |
|      216 |  310 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      216 |  311 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      216 |  312 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      216 |  313 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      106 |  314 | `	}` |
|      238 |  315 | `	if( pVm->bDisplayErrors ){` |
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
|      238 |  330 | `	return rc;` |
|        4 |  331 | `}` |
|     1021 |  332 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  333 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  334 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  335 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  336 | `	const char *zMessage /* Null terminated error message */` |
|        - |  337 | `	)` |
|        5 |  338 | `{` |
|        - |  339 | `	SyBlob sMsg;` |
|        - |  340 | `	SyString *pFile;` |
|     1026 |  341 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     1026 |  342 | `	sxi32 rc = SXRET_OK;` |
|        - |  343 | `	/* Peek the processed file if available */` |
|     1026 |  344 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1026 |  345 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     1026 |  346 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|        - |  347 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|        - |  348 | `		 * out-of-memory fatal through this path with none, and must not need` |
|        - |  349 | `		 * an allocation to say so. */` |
|      400 |  350 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      400 |  351 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      400 |  352 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      400 |  353 | `		nMsg = SyBlobLength(&sMsg);` |
|      193 |  354 | `	}` |
|        - |  355 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|        - |  356 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     1026 |  357 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|      155 |  358 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|      155 |  359 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|        - |  360 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|        - |  361 | `			 * '@' php still runs the handler (done just above) but prints` |
|        - |  362 | `			 * nothing itself. */` |
|      126 |  363 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|       61 |  364 | `		}` |
|       75 |  365 | `	}` |
|     1026 |  366 | `	SyBlobRelease(&sMsg);` |
|     1026 |  367 | `	return rc;` |
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
|   106238 |  419 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|        5 |  420 | `{` |
|        - |  421 | `	ph7_real r;` |
|   106243 |  422 | `	if( pVal == 0 ){` |
|      ! 0 |  423 | `		return FALSE;` |
|        - |  424 | `	}` |
|   106243 |  425 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|       37 |  426 | `		r = pVal->rVal;` |
|   106248 |  427 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|        - |  428 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|        - |  429 | `		 * no, and a numeric conversion would replace it. */` |
|        - |  430 | `		ph7_value sProbe;` |
|        - |  431 | `		SyString sStr;` |
|        - |  432 | `		int bReal;` |
|   100163 |  433 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|   100163 |  434 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|        - |  435 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|        - |  436 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|        - |  437 | `		 * answers without building anything. Conservative on purpose: it may` |
|        - |  438 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|   100163 |  439 | `		if( n < 19 ){` |
|   300431 |  440 | `			for( i = 0 ; i < n ; ++i ){` |
|   200321 |  441 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|       23 |  442 | `					break;` |
|        - |  443 | `				}` |
|   100142 |  444 | `			}` |
|   100157 |  445 | `			if( i >= n ){` |
|   100117 |  446 | `				return FALSE;` |
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
|     6051 |  459 | `		return FALSE;` |
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
|    53123 |  474 | `}` |
|        - |  475 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|        - |  476 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|        - |  477 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|        - |  478 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|        - |  479 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|     5834 |  480 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|        5 |  481 | `{` |
|     5839 |  482 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|     5813 |  483 | `		return SXRET_OK;` |
|        - |  484 | `	}` |
|       27 |  485 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       26 |  486 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|        - |  487 | `			? "Implicit conversion from float to int loses precision"` |
|        - |  488 | `			: "Implicit conversion from float-string to int loses precision");` |
|     2921 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|        - |  492 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|        - |  493 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|        - |  494 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|        - |  495 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|        - |  496 | ` */` |
|  2286080 |  497 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  498 | `{` |
|        - |  499 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|        - |  500 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  2286085 |  501 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  502 | `}` |
|        - |  503 | `/*` |
|        - |  504 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|        - |  505 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|        - |  506 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|        - |  507 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|        - |  508 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|        - |  509 | ` * keep native re-entries off a finite C stack).` |
|        - |  510 | ` */` |
|  3403349 |  511 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|        5 |  512 | `{` |
|  3403354 |  513 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
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
|        1 |  548 | `{` |
|        5 |  549 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  550 | `		return PH7_ABORT;` |
|        - |  551 | `	}` |
|        5 |  552 | `	pVm->iExitStatus = 255;` |
|        5 |  553 | `	pVm->bHaltRequested = 1;` |
|        5 |  554 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|        5 |  555 | `	return PH7_ABORT;` |
|        3 |  556 | `}` |
|        - |  557 | `/*` |
|        - |  558 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  559 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  560 | ` * information.` |
|        - |  561 | ` */` |
|    27634 |  562 | `static sxi32 VmThrowErrorAp(` |
|        - |  563 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  564 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  565 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  566 | `	const char *zFormat, /* Format message */` |
|        - |  567 | `	va_list ap           /* Variable list of arguments */` |
|        - |  568 | `	)` |
|        5 |  569 | `{` |
|        - |  570 | `	SyBlob sMsg;` |
|        - |  571 | `	SyString *pFile;` |
|    27639 |  572 | `	sxi32 rc = SXRET_OK;` |
|        - |  573 | `	/* Peek the processed file if available */` |
|    27639 |  574 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  575 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|    27639 |  576 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|    27639 |  577 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|    27639 |  578 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  579 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|        - |  580 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|        - |  581 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|        - |  582 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|    27639 |  583 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|        - |  584 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|        - |  585 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|    26233 |  586 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|    26233 |  587 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|    26121 |  588 | `			SyBlobRelease(&sMsg);` |
|    26121 |  589 | `			return SXRET_OK;` |
|        - |  590 | `		}` |
|      172 |  591 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|       56 |  592 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|       56 |  593 | `	}` |
|     1523 |  594 | `	SyBlobRelease(&sMsg);` |
|     1523 |  595 | `	return rc;` |
|    13822 |  596 | `}` |
|        - |  597 | `/*` |
|        - |  598 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - |  599 | ` * scope), or NULL when executing outside any class context.` |
|        - |  600 | ` */` |
|     9398 |  601 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 |  602 | `{` |
|     9403 |  603 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      139 |  604 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      139 |  605 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - |  606 | `	}` |
|     9269 |  607 | `	return 0;` |
|     4701 |  608 | `}` |
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
|   346480 |  623 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|        5 |  624 | `{` |
|   346485 |  625 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|      ! 0 |  626 | `		return 0;` |
|        - |  627 | `	}` |
|   346485 |  628 | `	pVm->nExcCtorDepth++;` |
|   346485 |  629 | `	return 1;` |
|   173245 |  630 | `}` |
|        - |  631 | `/*` |
|        - |  632 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - |  633 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - |  634 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - |  635 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - |  636 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - |  637 | ` */` |
|   205410 |  638 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 |  639 | `{` |
|        - |  640 | `	ph7_class *pErrClass;` |
|        - |  641 | `	ph7_class_instance *pThis;` |
|        - |  642 | `	ph7_class_method *pCons;` |
|        - |  643 | `	VmFrame *pFrame;` |
|        - |  644 | `	sxi32 rc;` |
|   205415 |  645 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|   205415 |  646 | `	if( pErrClass == 0 ){` |
|      ! 0 |  647 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  648 | `		return PH7_ABORT;` |
|        - |  649 | `	}` |
|   205415 |  650 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|   205415 |  651 | `	if( pThis == 0 ){` |
|      ! 0 |  652 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  653 | `		return PH7_ABORT;` |
|        - |  654 | `	}` |
|   205415 |  655 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|   205415 |  656 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|        - |  657 | `		ph7_value sArg;` |
|        - |  658 | `		ph7_value *apArg[1];` |
|        - |  659 | `		SyString sMsgStr;` |
|   205415 |  660 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|   205415 |  661 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   205415 |  662 | `		apArg[0] = &sArg;` |
|   205415 |  663 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   205415 |  664 | `		PH7_MemObjRelease(&sArg);` |
|   205415 |  665 | `		pVm->nExcCtorDepth--;` |
|   102705 |  666 | `	}` |
|   205415 |  667 | `	SyBlobRelease(pMsg);` |
|   205415 |  668 | `	pFrame = pVm->pFrame;` |
|   205415 |  669 | `	if( pFrame ){` |
|   205415 |  670 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   205415 |  671 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   102705 |  672 | `	}` |
|   205415 |  673 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   205415 |  674 | `	PH7_ClassInstanceUnref(pThis);` |
|   205415 |  675 | `	if( rc == SXERR_ABORT ){` |
|       30 |  676 | `		return PH7_ABORT;` |
|        - |  677 | `	}` |
|   205389 |  678 | `	return PH7_EXCEPTION;` |
|   102710 |  679 | `}` |
|        - |  680 | `/*` |
|        - |  681 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|        - |  682 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|        - |  683 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|        - |  684 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|        - |  685 | ` * result through its normal goto Exception / goto Abort.` |
|        - |  686 | ` */` |
|       54 |  687 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|        4 |  688 | `{` |
|        - |  689 | `	SyBlob sMsg;` |
|       58 |  690 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       58 |  691 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|       58 |  692 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|        4 |  693 | `}` |
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
|      482 |  754 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|        5 |  755 | `{` |
|        - |  756 | `	ph7_class_attr **apCase;` |
|        - |  757 | `	ph7_class_instance *pObj;` |
|        - |  758 | `	ph7_value *pSlot;` |
|        - |  759 | `	ph7_value sBacking,sPropVal;` |
|        - |  760 | `	sxu32 i;` |
|      487 |  761 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      369 |  762 | `		return SXRET_OK;` |
|        - |  763 | `	}` |
|      119 |  764 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - |  765 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|        - |  766 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|      ! 0 |  767 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      ! 0 |  768 | `			pVm->pConstCycleAttr = pCase;` |
|      ! 0 |  769 | `			pVm->pConstCycleClass = pClass;` |
|      ! 0 |  770 | `		}` |
|      ! 0 |  771 | `		return SXRET_OK;` |
|        - |  772 | `	}` |
|      119 |  773 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      119 |  774 | `	if( pClass->nEnumBacking != 0 ){` |
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
|       85 |  797 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      ! 0 |  798 | `				PH7_MemObjRelease(&sBacking);` |
|      ! 0 |  799 | `				return VmConstCycleThrow(&(*pVm));` |
|        - |  800 | `			}` |
|       40 |  801 | `		}` |
|       89 |  802 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|        - |  803 | `			/* php: TypeError, checked lazily at first case access */` |
|        - |  804 | `			SyBlob sMsg;` |
|        3 |  805 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|        3 |  806 | `			PH7_MemObjRelease(&sBacking);` |
|        3 |  807 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        2 |  808 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|        2 |  809 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|        3 |  810 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - |  811 | `		}` |
|       87 |  812 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  813 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|        - |  814 | `			 * the typed-constant leniency) to a genuine int. */` |
|       17 |  815 | `			PH7_MemObjToInteger(&sBacking);` |
|        9 |  816 | `		}else{` |
|       71 |  817 | `			PH7_MemObjToString(&sBacking);` |
|        - |  818 | `		}` |
|        - |  819 | `		/* php: two cases sharing one backing value are an Error — compared` |
|        - |  820 | `		 * against already-materialized cases only (php registers values as` |
|        - |  821 | `		 * each case evaluates). */` |
|       87 |  822 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      279 |  823 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|        - |  824 | `			ph7_value *pPrev;` |
|      199 |  825 | `			int bDup = 0;` |
|      199 |  826 | `			if( apCase[i] == pCase ){` |
|       85 |  827 | `				continue;` |
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
|      113 |  861 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      113 |  862 | `	if( pObj == 0 ){` |
|      ! 0 |  863 | `		PH7_MemObjRelease(&sBacking);` |
|      ! 0 |  864 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  865 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|      ! 0 |  866 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 |  867 | `		return PH7_ABORT;` |
|        - |  868 | `	}` |
|      113 |  869 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      113 |  870 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      113 |  871 | `	PH7_MemObjRelease(&sPropVal);` |
|      113 |  872 | `	if( pClass->nEnumBacking != 0 ){` |
|       85 |  873 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|       40 |  874 | `	}` |
|      113 |  875 | `	PH7_MemObjRelease(&sBacking);` |
|        - |  876 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|        - |  877 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      113 |  878 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      113 |  879 | `	if( pSlot == 0 ){` |
|      ! 0 |  880 | `		PH7_ClassInstanceUnref(pObj);` |
|      ! 0 |  881 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  882 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|      ! 0 |  883 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 |  884 | `		return PH7_ABORT;` |
|        - |  885 | `	}` |
|      113 |  886 | `	pSlot->x.pOther = pObj;` |
|      113 |  887 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      113 |  888 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      113 |  889 | `	pCase->nIdx = pSlot->nIdx;` |
|      113 |  890 | `	return SXRET_OK;` |
|      246 |  891 | `}` |
|        - |  892 | `/*` |
|        - |  893 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|        - |  894 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|        - |  895 | ` * there, so a broken case surfaces its error at the same point).` |
|        - |  896 | ` */` |
|      246 |  897 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|        5 |  898 | `{` |
|        - |  899 | `	ph7_class_attr **apCase;` |
|        - |  900 | `	sxu32 n;` |
|      251 |  901 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 |  902 | `		return SXRET_OK;` |
|        - |  903 | `	}` |
|      251 |  904 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      711 |  905 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      471 |  906 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|      471 |  907 | `		if( rc != SXRET_OK ){` |
|        8 |  908 | `			return rc;` |
|        - |  909 | `		}` |
|      235 |  910 | `	}` |
|      245 |  911 | `	return SXRET_OK;` |
|      128 |  912 | `}` |
|        - |  913 | `/*` |
|        - |  914 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|        - |  915 | ` * or 0 when the name does not name an enum.` |
|        - |  916 | ` */` |
|      204 |  917 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|        3 |  918 | `{` |
|        - |  919 | `	ph7_class *pClass;` |
|      207 |  920 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|      ! 0 |  921 | `		return 0;` |
|        - |  922 | `	}` |
|      309 |  923 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      102 |  924 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      209 |  925 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|        3 |  926 | `		pClass = pClass->pNextName;` |
|        1 |  927 | `	}` |
|      207 |  928 | `	return pClass;` |
|      105 |  929 | `}` |
|        - |  930 | `/*` |
|        - |  931 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|        - |  932 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|        - |  933 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|        - |  934 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|        - |  935 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|        - |  936 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|        - |  937 | ` */` |
|      428 |  938 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|        5 |  939 | `{` |
|      433 |  940 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      428 |  941 | `	if( pInner && pInner->pUserData` |
|      254 |  942 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      ! 0 |  943 | `		return 0;` |
|        - |  944 | `	}` |
|      433 |  945 | `	return pVm->nCurLine;` |
|      219 |  946 | `}` |
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
|      652 |  983 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  984 | `{` |
|        - |  985 | `	ph7_value *pMemObj;` |
|      652 |  986 | `	if( pAttr->nIdx != SXU32_HIGH` |
|      652 |  987 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      657 |  988 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|      ! 0 |  989 | `		return SXRET_OK;` |
|        - |  990 | `	}` |
|      657 |  991 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - |  992 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|        - |  993 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|        - |  994 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|        3 |  995 | `		if( pVm->pConstCycleAttr == 0 ){` |
|        3 |  996 | `			pVm->pConstCycleAttr = pAttr;` |
|        3 |  997 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        1 |  998 | `		}` |
|        3 |  999 | `		return SXRET_OK;` |
|        - | 1000 | `	}` |
|      655 | 1001 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      655 | 1002 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1003 | `		return SXERR_MEM;` |
|        - | 1004 | `	}` |
|      655 | 1005 | `	if( pAttr->pNativeValue ){` |
|        - | 1006 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|        - | 1007 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|        - | 1008 | `		 * when they went lazy (17th session), so this path — the only one an` |
|        - | 1009 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|        - | 1010 | `		 * which is why every native class constant read NULL: nothing had declared` |
|        - | 1011 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|        - | 1012 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|        - | 1013 | `		 * failure path to mirror below. */` |
|      286 | 1014 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      286 | 1015 | `		pAttr->nIdx = pMemObj->nIdx;` |
|      286 | 1016 | `		return SXRET_OK;` |
|        - | 1017 | `	}` |
|      373 | 1018 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      373 | 1019 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      373 | 1020 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      373 | 1021 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|        - | 1022 | `		sxu32 nSaveLazyLine;` |
|        - | 1023 | `		sxi32 nSaveLazyDepth;` |
|        - | 1024 | `		sxu32 nSlot;` |
|        - | 1025 | `		sxi32 rcExec;` |
|      373 | 1026 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      373 | 1027 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 1028 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|        - | 1029 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|        - | 1030 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|      373 | 1031 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 1032 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|        - | 1033 | `		 * throw out of its own bytecode carries. */` |
|      373 | 1034 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      373 | 1035 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      373 | 1036 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      373 | 1037 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      373 | 1038 | `		pVm->nConstEvalDepth++;` |
|      373 | 1039 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      373 | 1040 | `		pVm->nConstEvalDepth--;` |
|      373 | 1041 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      373 | 1042 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      373 | 1043 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      373 | 1044 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      373 | 1045 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      373 | 1046 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|      368 | 1047 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|      344 | 1048 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
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
|      341 | 1074 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|      337 | 1089 | `		pAttr->nIdx = nSlot;` |
|      337 | 1090 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|      337 | 1091 | `		return SXRET_OK;` |
|        - | 1092 | `	}` |
|      ! 0 | 1093 | `	pAttr->nIdx = pMemObj->nIdx;` |
|      ! 0 | 1094 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      ! 0 | 1095 | `	return SXRET_OK;` |
|      331 | 1096 | `}` |
|        - | 1097 | `/*` |
|        - | 1098 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|        - | 1099 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|        - | 1100 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|        - | 1101 | ` */` |
|      148 | 1102 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        4 | 1103 | `{` |
|      152 | 1104 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       77 | 1105 | `		return SXRET_OK;` |
|        - | 1106 | `	}` |
|       76 | 1107 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       15 | 1108 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|        - | 1109 | `	}` |
|       62 | 1110 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       78 | 1111 | `}` |
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
|   100166 | 1144 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        5 | 1145 | `{` |
|   100171 | 1146 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|   100171 | 1147 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - | 1148 | `	char zType[192];` |
|   150254 | 1149 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|    50083 | 1150 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));` |
|        - | 1151 | `	SyBlob sMsg;` |
|   100171 | 1152 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1153 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - | 1154 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|   100171 | 1155 | `	if( pOwner ){` |
|   100171 | 1156 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|    50083 | 1157 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|    50088 | 1158 | `	}else{` |
|      ! 0 | 1159 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",` |
|      ! 0 | 1160 | `			zGiven,&pAttr->sName,zTypeText);` |
|        - | 1161 | `	}` |
|   100171 | 1162 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - | 1166 | ` */` |
|   100016 | 1167 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1168 | `{` |
|   100021 | 1169 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   100021 | 1170 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - | 1171 | `	SyBlob sMsg;` |
|   100021 | 1172 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   100021 | 1173 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|    50008 | 1174 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|   100021 | 1175 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
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
|       42 | 1226 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        5 | 1227 | `{` |
|       47 | 1228 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 1229 | `	SyBlob sMsg;` |
|       47 | 1230 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       47 | 1231 | `	if( bModify ){` |
|       43 | 1232 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       24 | 1233 | `	}else{` |
|        6 | 1234 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        6 | 1235 | `		if( pActive ){` |
|      ! 0 | 1236 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 | 1237 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|      ! 0 | 1238 | `		}else{` |
|        6 | 1239 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        2 | 1240 | `				&pOwner->sName,&pAttr->sName);` |
|        - | 1241 | `		}` |
|        - | 1242 | `	}` |
|       47 | 1243 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1244 | `}` |
|        - | 1245 | `/*` |
|        - | 1246 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - | 1247 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - | 1248 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - | 1249 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - | 1250 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - | 1251 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - | 1252 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - | 1253 | ` */` |
|   697579 | 1254 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1255 | `{` |
|        - | 1256 | `	SyHashEntry *pSlot;` |
|        - | 1257 | `	VmClassAttr *pVmAttr;` |
|   697584 | 1258 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|   446235 | 1259 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - | 1260 | `	}` |
|   251354 | 1261 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   251354 | 1262 | `	if( pSlot == 0 ){` |
|   251254 | 1263 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 1264 | `	}` |
|      104 | 1265 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      104 | 1266 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 | 1267 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|        - | 1268 | `	}` |
|       90 | 1269 | `	if( pVmAttr->pAttr` |
|       93 | 1270 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|        - | 1271 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|        7 | 1272 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|        - | 1273 | `	}` |
|       87 | 1274 | `	return SXRET_OK;` |
|   349224 | 1275 | `}` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - | 1278 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - | 1279 | ` * For class types, instanceof is verified.` |
|        - | 1280 | ` *` |
|        - | 1281 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - | 1282 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - | 1283 | ` */` |
|        - | 1284 |  |
|        - | 1285 | `/*` |
|        - | 1286 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - | 1287 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - | 1288 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - | 1289 | ` *   0 if it's not strictly numeric.` |
|        - | 1290 | ` */` |
|       38 | 1291 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        3 | 1292 | `{` |
|        - | 1293 | `	const char *z, *zEnd, *zTail;` |
|        - | 1294 | `	sxu32 n;` |
|       41 | 1295 | `	sxu8 bReal = 0;` |
|        - | 1296 | `	sxi32 rc;` |
|       41 | 1297 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       24 | 1298 | `		return 0;` |
|        - | 1299 | `	}` |
|       18 | 1300 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 | 1301 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 | 1302 | `	zEnd = z + n;` |
|       18 | 1303 | `	if( n == 0 ) return 0;` |
|       18 | 1304 | `	zTail = 0;` |
|       18 | 1305 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 | 1306 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 | 1307 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 | 1308 | `	if( zTail != zEnd ) return 0;` |
|       15 | 1309 | `	return bReal ? 2 : 1;` |
|       22 | 1310 | `}` |
|        - | 1311 |  |
|        - | 1312 | `/*` |
|        - | 1313 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - | 1314 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|        - | 1315 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|        - | 1316 | ` * return, parameter, property, union alternative — would have to string-match` |
|        - | 1317 | ` * the name itself.` |
|        - | 1318 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - | 1319 | ` * to extend when another literal/pseudo type is added.` |
|        - | 1320 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - | 1321 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - | 1322 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - | 1323 | ` */` |
|     4264 | 1324 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        5 | 1325 | `{` |
|     4269 | 1326 | `	const char *z = pClass->zString;` |
|     4269 | 1327 | `	sxu32 n = pClass->nByte;` |
|     4269 | 1328 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      216 | 1329 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - | 1330 | `	}` |
|     4057 | 1331 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       28 | 1332 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - | 1333 | `	}` |
|     4031 | 1334 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       51 | 1335 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - | 1336 | `	}` |
|     3983 | 1337 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|        - | 1338 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|        - | 1339 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|        - | 1340 | `		 * object; scope-sensitive, so a private method is callable only from` |
|        - | 1341 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|        - | 1342 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|        - | 1343 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|        - | 1344 | `		 * php does.) */` |
|     2705 | 1345 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|        - | 1346 | `	}` |
|     1283 | 1347 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - | 1348 | `		/* iterable === array \| Traversable */` |
|       69 | 1349 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1350 | `			return 1;` |
|        - | 1351 | `		}` |
|       57 | 1352 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       29 | 1353 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       29 | 1354 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       13 | 1355 | `				return 1;` |
|        - | 1356 | `			}` |
|        7 | 1357 | `		}` |
|       45 | 1358 | `		return 0;` |
|        - | 1359 | `	}` |
|     1217 | 1360 | `	return -1;` |
|     2137 | 1361 | `}` |
|        - | 1362 | `/*` |
|        - | 1363 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|        - | 1364 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|        - | 1365 | ` * through. php binds the keyword where the hint is written, so` |
|        - | 1366 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|        - | 1367 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|        - | 1368 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|        - | 1369 | ` * throw a TypeError over perfectly valid code.` |
|        - | 1370 | ` *` |
|        - | 1371 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|        - | 1372 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|        - | 1373 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|        - | 1374 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|        - | 1375 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|        - | 1376 | `` * VmResolveTypeClass applies to `parent`.`` |
|        - | 1377 | ` */` |
|   211872 | 1378 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|        5 | 1379 | `{` |
|   211877 | 1380 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   202599 | 1381 | `		return pDecl;` |
|        - | 1382 | `	}` |
|     9283 | 1383 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|   105938 | 1384 | `}` |
|        - | 1385 | `/*` |
|        - | 1386 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - | 1387 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - | 1388 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - | 1389 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - | 1390 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - | 1391 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - | 1392 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - | 1393 | ` * throw.` |
|        - | 1394 | ` *` |
|        - | 1395 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|        - | 1396 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|        - | 1397 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|        - | 1398 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|        - | 1399 | ` */` |
|        - | 1400 | `/*` |
|        - | 1401 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|        - | 1402 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|        - | 1403 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|        - | 1404 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|        - | 1405 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|        - | 1406 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|        - | 1407 | ` * iLoadable flag — lives in one place.` |
|        - | 1408 | ` *` |
|        - | 1409 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|        - | 1410 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|        - | 1411 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|        - | 1412 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|        - | 1413 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|        - | 1414 | ` *` |
|        - | 1415 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|        - | 1416 | ` * compatibility target, where the type may legitimately be an interface or` |
|        - | 1417 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|        - | 1418 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|        - | 1419 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|        - | 1420 | ` * with TRUE; it does not go through this helper.)` |
|        - | 1421 | ` */` |
|     1288 | 1422 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|        5 | 1423 | `{` |
|     1293 | 1424 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      101 | 1425 | `		return pSelf;` |
|        - | 1426 | `	}` |
|     1197 | 1427 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|        - | 1428 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|        - | 1429 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|        - | 1430 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|        - | 1431 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|        - | 1432 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|       40 | 1433 | `		return PH7_VmPeekTopClass(pVm);` |
|        - | 1434 | `	}` |
|     1161 | 1435 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|        - | 1436 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|        - | 1437 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|       21 | 1438 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      ! 0 | 1439 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|      ! 0 | 1440 | `		}` |
|       21 | 1441 | `		return pSelf ? pSelf->pBase : 0;` |
|        - | 1442 | `	}` |
|     1143 | 1443 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|      649 | 1444 | `}` |
|        - | 1445 | `/*` |
|        - | 1446 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|        - | 1447 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|        - | 1448 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|        - | 1449 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|        - | 1450 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|        - | 1451 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|        - | 1452 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|        - | 1453 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|        - | 1454 | ` */` |
|   100532 | 1455 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|        5 | 1456 | `{` |
|   100549 | 1457 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|   100521 | 1458 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|   150798 | 1459 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|        5 | 1460 | `}` |
|        - | 1461 | `/*` |
|        - | 1462 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|        - | 1463 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|        - | 1464 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|        - | 1465 | ` * typed-default checks — each of which then formats its own message. The` |
|        - | 1466 | ` * resolved class is handed back through *ppResolved for the message builder` |
|        - | 1467 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|        - | 1468 | ` * nothing resolved).` |
|        - | 1469 | ` *` |
|        - | 1470 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|        - | 1471 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|        - | 1472 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|        - | 1473 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|        - | 1474 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|        - | 1475 | ` * resolve against there is no class to compare to — a position php rejects at` |
|        - | 1476 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|        - | 1477 | ` *` |
|        - | 1478 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|        - | 1479 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|        - | 1480 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|        - | 1481 | ` * autoloader can produce fails.` |
|        - | 1482 | ` */` |
|     1096 | 1483 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|        - | 1484 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|        5 | 1485 | `{` |
|     1101 | 1486 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     1101 | 1487 | `	*ppResolved = pExpected;` |
|     1101 | 1488 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       42 | 1489 | `		return 0;` |
|        - | 1490 | `	}` |
|     1063 | 1491 | `	if( pExpected == 0 ){` |
|       13 | 1492 | `		return VmHintIsScopeKeyword(pName);` |
|        - | 1493 | `	}` |
|     1051 | 1494 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|      553 | 1495 | `}` |
|        - | 1496 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|        - | 1497 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|   403176 | 1498 | `static int VmHintNameChar(int c)` |
|        5 | 1499 | `{` |
|   805979 | 1500 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|   402804 | 1501 | `		\|\| c == ' ' \|\| c == '\t');` |
|        5 | 1502 | `}` |
|        - | 1503 | `/*` |
|        - | 1504 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|        - | 1505 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|        - | 1506 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|        - | 1507 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|        - | 1508 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|        - | 1509 | ` * resolved until the call site are substituted.` |
|        - | 1510 | ` *` |
|        - | 1511 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|        - | 1512 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|        - | 1513 | ` * are only ever available as source: unions/intersections, and the property /` |
|        - | 1514 | ` * class-constant messages, which print the declared type whatever its shape.` |
|        - | 1515 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|        - | 1516 | ` *` |
|        - | 1517 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|        - | 1518 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|        - | 1519 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|        - | 1520 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|        - | 1521 | ` * names the two types it stands for. Only the standalone spellings can still` |
|        - | 1522 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|        - | 1523 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|        - | 1524 | ` */` |
|   100352 | 1525 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 1526 | `	char *zBuf,sxu32 nBuf)` |
|        5 | 1527 | `{` |
|        - | 1528 | `	const char *z;` |
|   100357 | 1529 | `	sxu32 n, i = 0, nAt = 0;` |
|   100357 | 1530 | `	if( nBuf == 0 ){` |
|      ! 0 | 1531 | `		return "";` |
|        - | 1532 | `	}` |
|   100357 | 1533 | `	z = pDeclared ? pDeclared->zString : 0;` |
|   100357 | 1534 | `	n = z ? pDeclared->nByte : 0;` |
|   100357 | 1535 | `	if( z ){` |
|   100357 | 1536 | `		const char *zIter = 0;` |
|   100357 | 1537 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        8 | 1538 | `			zIter = "Traversable\|array";` |
|   100354 | 1539 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        3 | 1540 | `			zIter = "Traversable\|array\|null";` |
|        1 | 1541 | `		}` |
|   100357 | 1542 | `		if( zIter ){` |
|       10 | 1543 | `			sxu32 nIter = SyStrlen(zIter);` |
|       10 | 1544 | `			if( nIter > nBuf - 1 ){` |
|      ! 0 | 1545 | `				nIter = nBuf - 1;` |
|      ! 0 | 1546 | `			}` |
|       10 | 1547 | `			SyMemcpy(zIter,zBuf,nIter);` |
|       10 | 1548 | `			zBuf[nIter] = 0;` |
|       10 | 1549 | `			return zBuf;` |
|        - | 1550 | `		}` |
|    50172 | 1551 | `	}` |
|   201071 | 1552 | `	while( i < n && nAt + 1 < nBuf ){` |
|        - | 1553 | `		sxu32 nStart, nCopy;` |
|        - | 1554 | `		SyString sTok;` |
|        - | 1555 | `		const SyString *pOut;` |
|   100727 | 1556 | `		if( !VmHintNameChar(z[i]) ){` |
|      207 | 1557 | `			zBuf[nAt++] = z[i++];` |
|      207 | 1558 | `			continue;` |
|        - | 1559 | `		}` |
|   100525 | 1560 | `		nStart = i;` |
|   402803 | 1561 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|   302283 | 1562 | `			i++;` |
|        5 | 1563 | `		}` |
|   100525 | 1564 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|   100525 | 1565 | `		pOut = &sTok;` |
|   100525 | 1566 | `		if( VmHintIsScopeKeyword(&sTok) ){` |
|       34 | 1567 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|       34 | 1568 | `			if( pRes ){` |
|       34 | 1569 | `				pOut = &pRes->sName;` |
|       15 | 1570 | `			}` |
|       15 | 1571 | `		}` |
|   100525 | 1572 | `		nCopy = pOut->nByte;` |
|   100525 | 1573 | `		if( nCopy > nBuf - nAt - 1 ){` |
|      ! 0 | 1574 | `			nCopy = nBuf - nAt - 1;` |
|      ! 0 | 1575 | `		}` |
|   100525 | 1576 | `		if( nCopy > 0 ){` |
|   100525 | 1577 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|   100525 | 1578 | `			nAt += nCopy;` |
|    50260 | 1579 | `		}` |
|        5 | 1580 | `	}` |
|   100349 | 1581 | `	zBuf[nAt] = 0;` |
|   100349 | 1582 | `	return zBuf;` |
|    50181 | 1583 | `}` |
|        - | 1584 | `/*` |
|        - | 1585 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|        - | 1586 | ` * float-identity leniency — see the typed-constant note above` |
|        - | 1587 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|        - | 1588 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|        - | 1589 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|        - | 1590 | ` * so an int-typed parameter / return / property / union member silently` |
|        - | 1591 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|        - | 1592 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|        - | 1593 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|        - | 1594 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|        - | 1595 | ` * present).` |
|        - | 1596 | ` *` |
|        - | 1597 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|        - | 1598 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|        - | 1599 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|        - | 1600 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|        - | 1601 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|        - | 1602 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|        - | 1603 | ` */` |
|    43164 | 1604 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|        5 | 1605 | `{` |
|    43164 | 1606 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|    21449 | 1607 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - | 1608 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|        - | 1609 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|        - | 1610 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|        - | 1611 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|       55 | 1612 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|       55 | 1613 | `		SyBlobRelease(&pVal->sBlob);` |
|       55 | 1614 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|       25 | 1615 | `	}` |
|    43169 | 1616 | `}` |
|      336 | 1617 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|        - | 1618 | `	ph7_class *pSelf)` |
|        5 | 1619 | `{` |
|        - | 1620 | `	sxu32 i;` |
|        - | 1621 | `	sxu32 nAlts;` |
|        - | 1622 | `	ph7_type_alt *aAlts;` |
|        - | 1623 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - | 1624 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      341 | 1625 | `	int bHasIntersection = 0;` |
|        - | 1626 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|      341 | 1627 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       20 | 1628 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - | 1629 | `	}` |
|      325 | 1630 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|      325 | 1631 | `	nAlts = SySetUsed(pAlts);` |
|        - | 1632 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|        - | 1633 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|        - | 1634 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|        - | 1635 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|        - | 1636 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|    10565 | 1637 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     1001 | 1638 | `	for( i = 0; i < nAlts; i++ ){` |
|      681 | 1639 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|       42 | 1640 | `			bHasIntersection = 1;` |
|       19 | 1641 | `		}` |
|      343 | 1642 | `	}` |
|        - | 1643 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|        - | 1644 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|        - | 1645 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|        - | 1646 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|      325 | 1647 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|       36 | 1648 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 1649 | `		sxu32 g;` |
|      422 | 1650 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|        - | 1651 | `			int bAll;` |
|      410 | 1652 | `			if( aGroupCount[g] < 2 ) continue;` |
|       36 | 1653 | `			bAll = 1;` |
|       88 | 1654 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 1655 | `				ph7_class *pExpected;` |
|       68 | 1656 | `				if( aAlts[i].nGroup != g ) continue;` |
|       64 | 1657 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|       64 | 1658 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|       64 | 1659 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       16 | 1660 | `					bAll = 0;` |
|       16 | 1661 | `					break;` |
|        - | 1662 | `				}` |
|       28 | 1663 | `			}` |
|       36 | 1664 | `			if( bAll ) return SXRET_OK;` |
|       10 | 1665 | `		}` |
|        6 | 1666 | `	}` |
|        - | 1667 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - | 1668 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - | 1669 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - | 1670 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|      921 | 1671 | `	for( i = 0; i < nAlts; i++ ){` |
|      635 | 1672 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      594 | 1673 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      407 | 1674 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|       16 | 1675 | `			return SXRET_OK;` |
|        - | 1676 | `		}` |
|      295 | 1677 | `	}` |
|      291 | 1678 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|      291 | 1679 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      895 | 1680 | `	for( i = 0; i < nAlts; i++ ){` |
|      609 | 1681 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      573 | 1682 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      389 | 1683 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      385 | 1684 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      379 | 1685 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|      167 | 1686 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|      127 | 1687 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|        5 | 1688 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|      289 | 1689 | `	}` |
|        - | 1690 | `	/* Object handling */` |
|      291 | 1691 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       99 | 1692 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       99 | 1693 | `		if( bHasClassAlt ){` |
|       85 | 1694 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      217 | 1695 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 1696 | `				ph7_class *pExpected;` |
|      161 | 1697 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      153 | 1698 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      107 | 1699 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      107 | 1700 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       28 | 1701 | `					return SXRET_OK;` |
|        - | 1702 | `				}` |
|       44 | 1703 | `			}` |
|       28 | 1704 | `		}` |
|       74 | 1705 | `		return SXERR_INVALID;` |
|        - | 1706 | `	}` |
|        - | 1707 | `	/* Array handling */` |
|      197 | 1708 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1709 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - | 1710 | `	}` |
|        - | 1711 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|        - | 1712 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|        - | 1713 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|        - | 1714 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|        - | 1715 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|        - | 1716 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|      185 | 1717 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|       22 | 1718 | `		if( bHasFloat ) return SXRET_OK;` |
|        5 | 1719 | `	}` |
|      177 | 1720 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      119 | 1721 | `		if( bHasInt ){` |
|       97 | 1722 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|       97 | 1723 | `			return SXRET_OK;` |
|        - | 1724 | `		}` |
|       11 | 1725 | `	}` |
|       85 | 1726 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       59 | 1727 | `		if( bHasString ) return SXRET_OK;` |
|        8 | 1728 | `	}` |
|       45 | 1729 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|        3 | 1730 | `		if( bHasBool ) return SXRET_OK;` |
|        1 | 1731 | `	}` |
|       45 | 1732 | `	if( bStrict ){` |
|        - | 1733 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|        5 | 1734 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 | 1735 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 | 1736 | `			return SXRET_OK;` |
|        - | 1737 | `		}` |
|        5 | 1738 | `		return SXERR_INVALID;` |
|        - | 1739 | `	}` |
|        - | 1740 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - | 1741 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - | 1742 | `	 * to match PHP's union RFC. */` |
|        - | 1743 | `	{` |
|       41 | 1744 | `		int kind = VmStringNumericKind(pValue);` |
|       41 | 1745 | `		if( bHasInt ){` |
|        - | 1746 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - | 1747 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 | 1748 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1749 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 | 1750 | `				return SXRET_OK;` |
|        - | 1751 | `			}` |
|       18 | 1752 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 1753 | `				ph7_real r = pValue->rVal;` |
|        - | 1754 | ``				/* Range first: `(sxi64)r` is undefined outside it (§2), and NaN`` |
|        - | 1755 | `				 * and the infinities are not exact ints either way. */` |
|        - | 1756 | `				/* (double)r: see VmValueIsLossyToInt -- a no-op where ph7_real` |
|        - | 1757 | `				 * is double, and the narrowing MSVC turns into an error under` |
|        - | 1758 | `				 * PH7_OMIT_FLOATING_POINT otherwise. */` |
|      ! 0 | 1759 | `				if( PH7_RealFitsInt64((double)r) && r == (ph7_real)(sxi64)r ){` |
|      ! 0 | 1760 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 | 1761 | `					return SXRET_OK;` |
|        - | 1762 | `				}` |
|      ! 0 | 1763 | `			}` |
|       18 | 1764 | `			if( kind == 1 ){` |
|        9 | 1765 | `				PH7_MemObjToInteger(pValue);` |
|        9 | 1766 | `				return SXRET_OK;` |
|        - | 1767 | `			}` |
|        4 | 1768 | `		}` |
|       33 | 1769 | `		if( bHasFloat ){` |
|       10 | 1770 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 | 1771 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 | 1772 | `				return SXRET_OK;` |
|        - | 1773 | `			}` |
|       10 | 1774 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 | 1775 | `				PH7_MemObjToReal(pValue);` |
|        7 | 1776 | `				return SXRET_OK;` |
|        - | 1777 | `			}` |
|        1 | 1778 | `		}` |
|       26 | 1779 | `		if( bHasString ){` |
|      ! 0 | 1780 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 | 1781 | `				PH7_MemObjToString(pValue);` |
|      ! 0 | 1782 | `				return SXRET_OK;` |
|        - | 1783 | `			}` |
|      ! 0 | 1784 | `		}` |
|       26 | 1785 | `		if( bHasBool ){` |
|        3 | 1786 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|        3 | 1787 | `				PH7_MemObjToBool(pValue);` |
|        3 | 1788 | `				return SXRET_OK;` |
|        - | 1789 | `			}` |
|      ! 0 | 1790 | `		}` |
|        - | 1791 | `	}` |
|       24 | 1792 | `	return SXERR_INVALID;` |
|      173 | 1793 | `}` |
|        - | 1794 |  |
|        - | 1795 | `/*` |
|        - | 1796 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - | 1797 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - | 1798 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - | 1799 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - | 1800 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - | 1801 | ` */` |
|      334 | 1802 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        5 | 1803 | `{` |
|        - | 1804 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - | 1805 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - | 1806 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - | 1807 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - | 1808 | `	 * silently swallow any argument. */` |
|      339 | 1809 | `	if( nType == MEMOBJ_NULL ){` |
|        3 | 1810 | `		return SXERR_INVALID;` |
|        - | 1811 | `	}` |
|        - | 1812 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|        - | 1813 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|        - | 1814 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|        - | 1815 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|        - | 1816 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|        - | 1817 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|        - | 1818 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|        - | 1819 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|        - | 1820 | `	 * object value against an array type is caught here too (never valid);` |
|        - | 1821 | `	 * object->scalar stays a separate case handled by the callers. */` |
|      337 | 1822 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|       37 | 1823 | `		return SXERR_INVALID;` |
|        - | 1824 | `	}` |
|      303 | 1825 | `	if( bStrict ){` |
|        - | 1826 | `		/* Only int -> float widening is allowed implicitly. */` |
|       36 | 1827 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 | 1828 | `			PH7_MemObjToReal(pVal);` |
|        3 | 1829 | `			return SXRET_OK;` |
|        - | 1830 | `		}` |
|       34 | 1831 | `		return SXERR_INVALID;` |
|        - | 1832 | `	}` |
|        - | 1833 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|        - | 1834 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|        - | 1835 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|        - | 1836 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|        - | 1837 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|        - | 1838 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|        - | 1839 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|      271 | 1840 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       20 | 1841 | `		return SXERR_INVALID;` |
|        - | 1842 | `	}` |
|        - | 1843 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|        - | 1844 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|        - | 1845 | `	 * is invoked by the string cast below). Every other scalar target —` |
|        - | 1846 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|        - | 1847 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|        - | 1848 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|        - | 1849 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|        - | 1850 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|        - | 1851 | `	 * is caught by the array guard.) */` |
|      253 | 1852 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       22 | 1853 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       27 | 1854 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       10 | 1855 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       13 | 1856 | `			return SXERR_INVALID;` |
|        - | 1857 | `		}` |
|        4 | 1858 | `	}` |
|      236 | 1859 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      191 | 1860 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      177 | 1861 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|       49 | 1862 | `		return SXERR_INVALID;` |
|        - | 1863 | `	}` |
|      195 | 1864 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|        - | 1865 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|        - | 1866 | `		 * PHL rejects it (§10). SXERR_INVALID routes to the caller's TypeError,` |
|        - | 1867 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|        - | 1868 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|        - | 1869 | `		 * for the typed parameters and returns that reach here, for the typed` |
|        - | 1870 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|        - | 1871 | `		 * integer-only operators. */` |
|      ! 0 | 1872 | `		return SXERR_INVALID;` |
|        - | 1873 | `	}` |
|      190 | 1874 | `	if( nType == MEMOBJ_STRING && (pVal->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       56 | 1875 | `	 && pVal->pVm && PH7_IS_NAN(pVal->rVal) ){` |
|        - | 1876 | ``		/* A userland `string` parameter, return or property taking a NaN: php's`` |
|        - | 1877 | `		 * weak coercion warns there exactly as its ZPP does for an internal one` |
|        - | 1878 | ``		 * (`unexpected NAN value was coerced to string`). The cast below is the`` |
|        - | 1879 | `		 * silent conversion -- it is shared with the engine's own -- so the` |
|        - | 1880 | `		 * diagnostic is raised here, where the DECLARED type is known. */` |
|        3 | 1881 | `		VmErrorFormat(pVal->pVm,PH7_CTX_WARNING,` |
|        - | 1882 | `			"unexpected NAN value was coerced to string");` |
|        1 | 1883 | `	}` |
|        - | 1884 | `	{` |
|      195 | 1885 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      195 | 1886 | `		if( xCast ) xCast(pVal);` |
|        - | 1887 | `	}` |
|      195 | 1888 | `	return SXRET_OK;` |
|      172 | 1889 | `}` |
|        - | 1890 |  |
|        - | 1891 | `/*` |
|        - | 1892 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - | 1893 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - | 1894 | ` *` |
|        - | 1895 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - | 1896 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - | 1897 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - | 1898 | ` * back to a static literal and ignore zBuf entirely.` |
|        - | 1899 | ` */` |
|      202 | 1900 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        5 | 1901 | `{` |
|      207 | 1902 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|      207 | 1903 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|      207 | 1904 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      207 | 1905 | `		if( pDeclared->zString && nCopy > 0 ){` |
|      207 | 1906 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      101 | 1907 | `		}` |
|      207 | 1908 | `		zBuf[nCopy] = 0;` |
|      207 | 1909 | `		return zBuf;` |
|        - | 1910 | `	}` |
|      ! 0 | 1911 | `	switch( nType ){` |
|      ! 0 | 1912 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 | 1913 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 | 1914 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 | 1915 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 | 1916 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 | 1917 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 | 1918 | `		default:             return "scalar";` |
|        - | 1919 | `	}` |
|      106 | 1920 | `}` |
|        - | 1921 |  |
|        - | 1922 | `/*` |
|        - | 1923 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|        - | 1924 | ` * the way php writes it in a TypeError:` |
|        - | 1925 | ` *` |
|        - | 1926 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|        - | 1927 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|        - | 1928 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|        - | 1929 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|        - | 1930 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|        - | 1931 | ` *` |
|        - | 1932 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|        - | 1933 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|        - | 1934 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|        - | 1935 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|        - | 1936 | ` */` |
|      174 | 1937 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|        - | 1938 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|        5 | 1939 | `{` |
|      179 | 1940 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|        - | 1941 | `	sxu32 nCopy;` |
|      179 | 1942 | `	sxu32 nAt = 0;` |
|      179 | 1943 | `	if( nBuf == 0 ){` |
|      ! 0 | 1944 | `		return "";` |
|        - | 1945 | `	}` |
|      174 | 1946 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|       79 | 1947 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|       21 | 1948 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|       21 | 1949 | `		nCopy = SyStrlen(zIter);` |
|       21 | 1950 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       21 | 1951 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|       21 | 1952 | `		zBuf[nCopy] = 0;` |
|       21 | 1953 | `		return zBuf;` |
|        - | 1954 | `	}` |
|      161 | 1955 | `	if( bNullable && nBuf > 1 ){` |
|       23 | 1956 | `		zBuf[nAt++] = '?';` |
|       10 | 1957 | `	}` |
|      161 | 1958 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|      161 | 1959 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|      161 | 1960 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|      161 | 1961 | `	zBuf[nAt + nCopy] = 0;` |
|      161 | 1962 | `	return zBuf;` |
|       92 | 1963 | `}` |
|        - | 1964 |  |
|        - | 1965 | `/*` |
|        - | 1966 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - | 1967 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - | 1968 | ` */` |
|      142 | 1969 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        5 | 1970 | `{` |
|      147 | 1971 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      218 | 1972 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      142 | 1973 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      147 | 1974 | `	return zBuf;` |
|        5 | 1975 | `}` |
|        - | 1976 |  |
|        - | 1977 | `/*` |
|        - | 1978 | ` * php's write_property handler (ph7_class::xSet): a native class whose properties` |
|        - | 1979 | ` * are its own C struct converts the incoming value the way that struct demands —` |
|        - | 1980 | ` * and may refuse the write outright. The value is rewritten IN PLACE, so what the` |
|        - | 1981 | ` * caller goes on to store is what the hook left behind.` |
|        - | 1982 | ` */` |
|      612 | 1983 | `static sxi32 VmRunNativeSet(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pValue)` |
|        2 | 1984 | `{` |
|        - | 1985 | `	PH7_NativeSetCtx sSet;` |
|      614 | 1986 | `	if( pVmAttr->pInst == 0 ){` |
|      ! 0 | 1987 | `		return SXRET_OK;   /* a class static: no object for a handler to run on */` |
|        - | 1988 | `	}` |
|      614 | 1989 | `	sSet.pName = &pVmAttr->pAttr->sName;` |
|      614 | 1990 | `	sSet.pValue = pValue;` |
|      614 | 1991 | `	sSet.zThrowClass = 0;` |
|      614 | 1992 | `	sSet.zThrowMsg[0] = 0;` |
|      614 | 1993 | `	if( PH7_ClassNativeSet(pVmAttr->pInst,&sSet) && sSet.zThrowClass ){` |
|        5 | 1994 | `		return VmThrowFixedError(pVm,sSet.zThrowClass,sSet.zThrowMsg);` |
|        - | 1995 | `	}` |
|      610 | 1996 | `	return SXRET_OK;` |
|      308 | 1997 | `}` |
|        - | 1998 | `/*` |
|        - | 1999 | ` * The same handler, asked of a SLOT that an opcode has already mutated in place.` |
|        - | 2000 | `` * `$i->f++` and `$i->f--` never pass a value through the store filter — they`` |
|        - | 2001 | ` * increment the slot where it lies — so the conversion has to be applied after` |
|        - | 2002 | ` * the fact, which is exactly what php does (it reads, increments, and writes` |
|        - | 2003 | `` * back through the handler: `$i->f = 1.456008; ++$i->f` leaves the property at`` |
|        - | 2004 | ` * 2.456007, the microsecond truncation of the sum). Answers SXRET_OK when the` |
|        - | 2005 | ` * slot is not a native one.` |
|        - | 2006 | ` */` |
|        - | 2007 | `/*` |
|        - | 2008 | ` * Register a property slot with the store filter, and drop it again. These two` |
|        - | 2009 | ` * are the ONLY writers of pVm->hTypedSlot: the predicate that decides membership` |
|        - | 2010 | ` * lives here once (a declared type, a native write handler, or both), and the` |
|        - | 2011 | ` * handler COUNT that lets the mutation opcodes skip the table entirely is kept` |
|        - | 2012 | ` * beside it -- registering in one place and forgetting to drop in another is` |
|        - | 2013 | ` * exactly how a recycled memobj index would inherit a stale entry.` |
|        - | 2014 | ` */` |
| 10297141 | 2015 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        5 | 2016 | `{` |
| 10297146 | 2017 | `	if( !PH7_ATTR_STORE_FILTERED(pVmAttr->pAttr) ){` |
|  2990237 | 2018 | `		return SXRET_OK;` |
|        - | 2019 | `	}` |
|  7306914 | 2020 | `	if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 2021 | `		return SXERR_MEM;` |
|        - | 2022 | `	}` |
|  7306914 | 2023 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|     2026 | 2024 | `		pVm->nNativeSetSlot++;` |
|     1012 | 2025 | `	}` |
|  7306914 | 2026 | `	return SXRET_OK;` |
|  5148572 | 2027 | `}` |
|  9531411 | 2028 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx)` |
|        5 | 2029 | `{` |
|  9531416 | 2030 | `	if( pAttr == 0 \|\| !PH7_ATTR_STORE_FILTERED(pAttr) ){` |
|  2739927 | 2031 | `		return;` |
|        - | 2032 | `	}` |
|  6791489 | 2033 | `	if( SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32),0) == SXRET_OK` |
|  6791494 | 2034 | `	 && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) && pVm->nNativeSetSlot > 0 ){` |
|     1959 | 2035 | `		pVm->nNativeSetSlot--;` |
|      979 | 2036 | `	}` |
|  4765707 | 2037 | `}` |
|   697485 | 2038 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 | 2039 | `{` |
|        - | 2040 | `	SyHashEntry *pSlot;` |
|        - | 2041 | `	VmClassAttr *pVmAttr;` |
|   697490 | 2042 | `	if( nIdx == SXU32_HIGH \|\| pVm->nNativeSetSlot == 0 ){` |
|   697460 | 2043 | `		return SXRET_OK;` |
|        - | 2044 | `	}` |
|       31 | 2045 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|       31 | 2046 | `	if( pSlot == 0 ){` |
|       25 | 2047 | `		return SXRET_OK;` |
|        - | 2048 | `	}` |
|        7 | 2049 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|        7 | 2050 | `	if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) == 0 ){` |
|      ! 0 | 2051 | `		return SXRET_OK;` |
|        - | 2052 | `	}` |
|        7 | 2053 | `	return VmRunNativeSet(pVm,pVmAttr,pValue);` |
|   349177 | 2054 | `}` |
|   109788 | 2055 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|        5 | 2056 | `{` |
|        - | 2057 | `	SyHashEntry *pSlot;` |
|        - | 2058 | `	VmClassAttr *pVmAttr;` |
|        - | 2059 | `	ph7_class_attr *pAttr;` |
|        - | 2060 | `	ph7_class *pHintScope;` |
|        - | 2061 | `	char zGivenBuf[128];` |
|        - | 2062 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|        - | 2063 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|        - | 2064 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|        - | 2065 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|   109793 | 2066 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|   109793 | 2067 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   109793 | 2068 | `	if( pSlot == 0 ){` |
|     8125 | 2069 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2070 | `	}` |
|   101673 | 2071 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   101673 | 2072 | `	pAttr = pVmAttr->pAttr;` |
|   101673 | 2073 | `	if( pAttr == 0 ){` |
|      ! 0 | 2074 | `		return SXRET_OK;` |
|        - | 2075 | `	}` |
|   101673 | 2076 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|      608 | 2077 | `		sxi32 rcNat = VmRunNativeSet(pVm,pVmAttr,pValue);` |
|      608 | 2078 | `		if( rcNat != SXRET_OK ){` |
|        5 | 2079 | `			return rcNat;` |
|        - | 2080 | `		}` |
|      301 | 2081 | `	}` |
|   101669 | 2082 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      604 | 2083 | `		return SXRET_OK;` |
|        - | 2084 | `	}` |
|        - | 2085 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|        - | 2086 | `	 * the property (a trait's members count as the composing class), not the` |
|        - | 2087 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|   101067 | 2088 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 2089 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - | 2090 | `	 * property may be written exactly once and only from within the declaring` |
|        - | 2091 | `	 * class scope (its set-scope is protected). */` |
|   101067 | 2092 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2093 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - | 2094 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - | 2095 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - | 2096 | `		 * leaves it set, so a later valid initialization still works). */` |
|      101 | 2097 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - | 2098 | `			/* Already initialized: any further write is forbidden, any scope —` |
|        - | 2099 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|        - | 2100 | `			 * Exceptions that fall through to the set-scope check below:` |
|        - | 2101 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|        - | 2102 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|        - | 2103 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|       34 | 2104 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       34 | 2105 | `			if( !(pCloneFr && pCloneFr->pThis` |
|       17 | 2106 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|       32 | 2107 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|        - | 2108 | `			}` |
|        1 | 2109 | `		}` |
|       34 | 2110 | `	}` |
|   101039 | 2111 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 2112 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|        - | 2113 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|       27 | 2114 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|       27 | 2115 | `		if( rcVis != SXRET_OK ){` |
|       13 | 2116 | `			return rcVis;` |
|        1 | 2117 | `		}` |
|   101020 | 2118 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2119 | `		/* First write (or a clone re-init) must come from within the declaring` |
|        - | 2120 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|       71 | 2121 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       71 | 2122 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 2123 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|        - | 2124 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|        - | 2125 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|       71 | 2126 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|        5 | 2127 | `			pDecl = pVmAttr->pOwner;` |
|        2 | 2128 | `		}` |
|       71 | 2129 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        6 | 2130 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|        - | 2131 | `		}` |
|       31 | 2132 | `	}` |
|        - | 2133 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|        - | 2134 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|        - | 2135 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|        - | 2136 | `	 * is its TypeError there), which this used to deny outright. */` |
|   101023 | 2137 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       92 | 2138 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       58 | 2139 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       29 | 2140 | `			bStrict,pHintScope);` |
|       63 | 2141 | `		if( rc == SXRET_OK ){` |
|       38 | 2142 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       38 | 2143 | `			return SXRET_OK;` |
|        - | 2144 | `		}` |
|       29 | 2145 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 2146 | `			char zBuf[128];` |
|       25 | 2147 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        7 | 2148 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 2149 | `		}` |
|       13 | 2150 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2151 | `	}` |
|        - | 2152 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - | 2153 | `	 * includes null). */` |
|   100965 | 2154 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       32 | 2155 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       26 | 2156 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 | 2157 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       23 | 2158 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       23 | 2159 | `			return SXRET_OK;` |
|        - | 2160 | `		}` |
|       15 | 2161 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - | 2162 | `	}` |
|        - | 2163 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - | 2164 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - | 2165 | `	 * type error. */` |
|   100933 | 2166 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2167 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2168 | `	}` |
|        - | 2169 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - | 2170 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - | 2171 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|   100933 | 2172 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 | 2173 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 | 2174 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        5 | 2175 | `			return SXRET_OK;` |
|        - | 2176 | `		}` |
|        7 | 2177 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2178 | `	}` |
|        - | 2179 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - | 2180 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - | 2181 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - | 2182 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - | 2183 | `	 * wrongly accept any object / reject arrays. */` |
|   100923 | 2184 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       79 | 2185 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       79 | 2186 | `		if( rcPseudo == 1 ){` |
|       13 | 2187 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       13 | 2188 | `			return SXRET_OK;` |
|        - | 2189 | `		}` |
|       67 | 2190 | `		if( rcPseudo == 0 ){` |
|        8 | 2191 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2192 | `		}` |
|        - | 2193 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       28 | 2194 | `	}` |
|   100905 | 2195 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - | 2196 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|        - | 2197 | `		 * class (pHintScope), not the instance's runtime class. */` |
|       61 | 2198 | `		ph7_class *pExpected = 0;` |
|       61 | 2199 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|        - | 2200 | `			char zBuf[128];` |
|       32 | 2201 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       18 | 2202 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|       18 | 2203 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|      ! 0 | 2204 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2205 | `		}` |
|       43 | 2206 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       43 | 2207 | `		return SXRET_OK;` |
|        - | 2208 | `	}` |
|        - | 2209 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|        - | 2210 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|        - | 2211 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|        - | 2212 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|        - | 2213 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|   100849 | 2214 | `	if( bStrict ){` |
|        - | 2215 | `		int bOk;` |
|       29 | 2216 | `		if( ph7_value_is_bool(pValue) ){` |
|        5 | 2217 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|       27 | 2218 | `		}else if( ph7_value_is_float(pValue) ){` |
|        3 | 2219 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|       24 | 2220 | `		}else if( ph7_value_is_int(pValue) ){` |
|        9 | 2221 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|       19 | 2222 | `		}else if( ph7_value_is_string(pValue) ){` |
|       15 | 2223 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|        8 | 2224 | `		}else{` |
|        - | 2225 | `			/* array / resource / an object against a scalar type: no coercion in` |
|        - | 2226 | `			 * either mode, so the flag test is the whole answer (an object never` |
|        - | 2227 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|        - | 2228 | `			 * does not perform). */` |
|      ! 0 | 2229 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|        - | 2230 | `		}` |
|       29 | 2231 | `		if( !bOk ){` |
|        - | 2232 | `			char zObjBuf[128];` |
|       31 | 2233 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       20 | 2234 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      ! 0 | 2235 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|       20 | 2236 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2237 | `		}` |
|        9 | 2238 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|        3 | 2239 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|        2 | 2240 | `		}else{` |
|        7 | 2241 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2242 | `		}` |
|        9 | 2243 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 2244 | `		return SXRET_OK;` |
|        - | 2245 | `	}` |
|        - | 2246 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - | 2247 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|        - | 2248 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|        - | 2249 | `	 * via __toString, so fall through to the string cast below. */` |
|   100821 | 2250 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       16 | 2251 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       18 | 2252 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|        4 | 2253 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|        - | 2254 | `			char zBuf[128];` |
|       20 | 2255 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        6 | 2256 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 2257 | `		}` |
|        1 | 2258 | `	}` |
|        - | 2259 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|        - | 2260 | `	 * take (VmCoerceScalarWeak's own rule, §10) -- and it had none of it, so this` |
|        - | 2261 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|        - | 2262 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|        - | 2263 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|        - | 2264 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|        - | 2265 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|        - | 2266 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|        - | 2267 | `	 * that arrives carrying a cached int representation is asked too. */` |
|   100809 | 2268 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|       38 | 2269 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       12 | 2270 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2271 | `	}` |
|   100785 | 2272 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|   100083 | 2273 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|   100083 | 2274 | `		if( xCast ){` |
|        - | 2275 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|   100083 | 2276 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        8 | 2277 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2278 | `			}` |
|   100077 | 2279 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 | 2280 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|        - | 2281 | `			}` |
|        - | 2282 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - | 2283 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - | 2284 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|   100066 | 2285 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|   100061 | 2286 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|   100065 | 2287 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|   100037 | 2288 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - | 2289 | `			}` |
|       38 | 2290 | `			xCast(pValue);` |
|       17 | 2291 | `		}` |
|       21 | 2292 | `	}else{` |
|        - | 2293 | `		/* Mask matched — an int property accepting a whole-real must` |
|        - | 2294 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|      707 | 2295 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2296 | `	}` |
|      741 | 2297 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      741 | 2298 | `	return SXRET_OK;` |
|    54899 | 2299 | `}` |
|        - | 2300 | `/*` |
|        - | 2301 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|        - | 2302 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|        - | 2303 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|        - | 2304 | ` *     a scope that could normally reach it — else a catchable Error),` |
|        - | 2305 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|        - | 2306 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|        - | 2307 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|        - | 2308 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|        - | 2309 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|        - | 2310 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|        - | 2311 | ` */` |
|       30 | 2312 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|        - | 2313 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|        1 | 2314 | `{` |
|       31 | 2315 | `	ph7_class *pClass = pClone->pClass;` |
|        - | 2316 | `	SyHashEntry *pEntry;` |
|        - | 2317 | `	VmClassAttr *pVmAttr;` |
|        - | 2318 | `	ph7_class_attr *pAttr;` |
|        - | 2319 | `	ph7_value *pSlot;` |
|        - | 2320 | `	sxi32 rc;` |
|       31 | 2321 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|       31 | 2322 | `	if( pEntry == 0 ){` |
|        - | 2323 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|        - | 2324 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|        - | 2325 | `		 * deferred residual). */` |
|      ! 0 | 2326 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|      ! 0 | 2327 | `		if( pSlot == 0 ){` |
|      ! 0 | 2328 | `			return PH7_VmMemoryError(pVm);` |
|        - | 2329 | `		}` |
|      ! 0 | 2330 | `		PH7_MemObjStore(pValue,pSlot);` |
|      ! 0 | 2331 | `		return SXRET_OK;` |
|        - | 2332 | `	}` |
|       31 | 2333 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       31 | 2334 | `	pAttr = pVmAttr->pAttr;` |
|        - | 2335 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|       31 | 2336 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 2337 | `		SyBlob sMsg;` |
|      ! 0 | 2338 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 2339 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|      ! 0 | 2340 | `			&pClass->sName,&pAttr->sName);` |
|      ! 0 | 2341 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2342 | `	}` |
|        - | 2343 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|        - | 2344 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|        - | 2345 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|       31 | 2346 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 | 2347 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|        5 | 2348 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|        - | 2349 | `		SyBlob sMsg;` |
|        5 | 2350 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2351 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|        5 | 2352 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2353 | `	}` |
|        - | 2354 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|       27 | 2355 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|       27 | 2356 | `	if( rc != SXRET_OK ){` |
|        3 | 2357 | `		return rc;` |
|        - | 2358 | `	}` |
|        - | 2359 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|       25 | 2360 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       25 | 2361 | `	if( pSlot ){` |
|       25 | 2362 | `		PH7_MemObjStore(pValue,pSlot);` |
|       12 | 2363 | `	}` |
|       25 | 2364 | `	return SXRET_OK;` |
|       16 | 2365 | `}` |
|        - | 2366 | `/*` |
|        - | 2367 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - | 2368 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - | 2369 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - | 2370 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - | 2371 | ` */` |
|       10 | 2372 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        3 | 2373 | `{` |
|       13 | 2374 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2375 | `	char zBuf[128],zType[192];` |
|        - | 2376 | `	const char *zGiven;` |
|       18 | 2377 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        5 | 2378 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|       13 | 2379 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2380 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 2381 | `	}else{` |
|       13 | 2382 | `		zGiven = ph7_type_name(pValue);` |
|        - | 2383 | `	}` |
|       13 | 2384 | `	if( bLazy ){` |
|        - | 2385 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|        - | 2386 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|        - | 2387 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|        - | 2388 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|        - | 2389 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|        - | 2390 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|        - | 2391 | `		 * re-evaluates and re-raises, as php's does. */` |
|        - | 2392 | `		SyBlob sMsg;` |
|        5 | 2393 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2394 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|        2 | 2395 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        5 | 2396 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 2397 | `	}` |
|        - | 2398 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - | 2399 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - | 2400 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - | 2401 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - | 2402 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - | 2403 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - | 2404 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|        9 | 2405 | `	if( pVm->sCodeGen.xErr ){` |
|        8 | 2406 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - | 2407 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        2 | 2408 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        4 | 2409 | `	}else{` |
|        4 | 2410 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 2411 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        1 | 2412 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        - | 2413 | `	}` |
|        9 | 2414 | `	pVm->iExitStatus = 255;` |
|        9 | 2415 | `	pVm->bHaltRequested = 1;` |
|        9 | 2416 | `	return SXERR_ABORT;` |
|        8 | 2417 | `}` |
|        - | 2418 | `/*` |
|        - | 2419 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - | 2420 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - | 2421 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - | 2422 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - | 2423 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - | 2424 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - | 2425 | ` */` |
|       42 | 2426 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        4 | 2427 | `{` |
|       46 | 2428 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - | 2429 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|       46 | 2430 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 2431 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 | 2432 | `			return SXRET_OK;` |
|        - | 2433 | `		}` |
|      ! 0 | 2434 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 | 2435 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 2436 | `			return SXRET_OK;` |
|        - | 2437 | `		}` |
|      ! 0 | 2438 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2439 | `	}` |
|        - | 2440 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|       44 | 2441 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        6 | 2442 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|        5 | 2443 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|        5 | 2444 | `			return SXRET_OK;` |
|        - | 2445 | `		}` |
|      ! 0 | 2446 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2447 | `	}` |
|        - | 2448 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|       40 | 2449 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2450 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2451 | `	}` |
|        - | 2452 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|       40 | 2453 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 2454 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2455 | `			return SXRET_OK;` |
|        - | 2456 | `		}` |
|      ! 0 | 2457 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2458 | `	}` |
|        - | 2459 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - | 2460 | `	 * a real class/interface verified by instanceof. */` |
|       40 | 2461 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 2462 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        3 | 2463 | `		if( rcPseudo == 1 ){` |
|      ! 0 | 2464 | `			return SXRET_OK;` |
|        - | 2465 | `		}` |
|        3 | 2466 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 2467 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2468 | `		}` |
|        - | 2469 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|        - | 2470 | `		 * self/parent resolve against the declaring class. */` |
|        - | 2471 | `		{` |
|        3 | 2472 | `			ph7_class *pExpected = 0;` |
|        4 | 2473 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|        1 | 2474 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|        3 | 2475 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2476 | `			}` |
|        - | 2477 | `		}` |
|      ! 0 | 2478 | `		return SXRET_OK;` |
|        - | 2479 | `	}` |
|        - | 2480 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - | 2481 | `	 * implicit widening. Everything else is a type error.` |
|        - | 2482 | `	 *` |
|        - | 2483 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - | 2484 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|        - | 2485 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|        - | 2486 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|        - | 2487 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|        - | 2488 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|        - | 2489 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|        - | 2490 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|        - | 2491 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|        - | 2492 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|        - | 2493 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|       37 | 2494 | `	if( pValue->iFlags & pAttr->nType ){` |
|       25 | 2495 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       25 | 2496 | `		return SXRET_OK;` |
|        - | 2497 | `	}` |
|       13 | 2498 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 | 2499 | `		PH7_MemObjToReal(pValue);` |
|        3 | 2500 | `		return SXRET_OK;` |
|        - | 2501 | `	}` |
|       10 | 2502 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       25 | 2503 | `}` |
|        - | 2504 | `/*` |
|        - | 2505 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|        - | 2506 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|        - | 2507 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|        - | 2508 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|        - | 2509 | ` */` |
|       34 | 2510 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        2 | 2511 | `{` |
|       36 | 2512 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2513 | `	const char *zGiven;` |
|        - | 2514 | `	char zBuf[128],zType[192];` |
|       53 | 2515 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       17 | 2516 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|        - | 2517 | `	SyBlob sMsg;` |
|       36 | 2518 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2519 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 2520 | `	}else{` |
|       36 | 2521 | `		zGiven = ph7_type_name(pValue);` |
|        - | 2522 | `	}` |
|       36 | 2523 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       36 | 2524 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|       17 | 2525 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       36 | 2526 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        2 | 2527 | `}` |
|        - | 2528 | `/*` |
|        - | 2529 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|        - | 2530 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|        - | 2531 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|        - | 2532 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|        - | 2533 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|        - | 2534 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|        - | 2535 | ` * instance defaults per-instantiation. Matching structure of` |
|        - | 2536 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|        - | 2537 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|        - | 2538 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|        - | 2539 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|        - | 2540 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|        - | 2541 | ` */` |
|        - | 2542 | `/*` |
|        - | 2543 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|        - | 2544 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|        - | 2545 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|        - | 2546 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|        - | 2547 | ` * throwing, so the static-property mount path can defer the failure (php` |
|        - | 2548 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|        - | 2549 | ` * instance path throws immediately via the wrapper below.` |
|        - | 2550 | ` */` |
|      388 | 2551 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 2552 | `{` |
|      393 | 2553 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|      393 | 2554 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       58 | 2555 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       54 | 2556 | `			return SXRET_OK;` |
|        - | 2557 | `		}` |
|        4 | 2558 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 2559 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 2560 | `			return SXRET_OK;` |
|        - | 2561 | `		}` |
|        3 | 2562 | `		return SXERR_INVALID;` |
|        - | 2563 | `	}` |
|      339 | 2564 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       39 | 2565 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       30 | 2566 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       30 | 2567 | `			return SXRET_OK;` |
|        - | 2568 | `		}` |
|      ! 0 | 2569 | `		return SXERR_INVALID;` |
|        - | 2570 | `	}` |
|      313 | 2571 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2572 | `		return SXERR_INVALID;` |
|        - | 2573 | `	}` |
|      313 | 2574 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 2575 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2576 | `			return SXRET_OK;` |
|        - | 2577 | `		}` |
|      ! 0 | 2578 | `		return SXERR_INVALID;` |
|        - | 2579 | `	}` |
|      313 | 2580 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        5 | 2581 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        5 | 2582 | `		if( rcPseudo == 1 ){` |
|        5 | 2583 | `			return SXRET_OK;` |
|        - | 2584 | `		}` |
|      ! 0 | 2585 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 2586 | `			return SXERR_INVALID;` |
|        - | 2587 | `		}` |
|        - | 2588 | `		{` |
|        - | 2589 | `			/* self/parent in the hint resolve against the declaring class. */` |
|      ! 0 | 2590 | `			ph7_class *pExpected = 0;` |
|      ! 0 | 2591 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|      ! 0 | 2592 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|      ! 0 | 2593 | `				return SXERR_INVALID;` |
|        - | 2594 | `			}` |
|        - | 2595 | `		}` |
|      ! 0 | 2596 | `		return SXRET_OK;` |
|        - | 2597 | `	}` |
|      309 | 2598 | `	if( pValue->iFlags & pAttr->nType ){` |
|      273 | 2599 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      273 | 2600 | `		return SXRET_OK;` |
|        - | 2601 | `	}` |
|       38 | 2602 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        6 | 2603 | `		PH7_MemObjToReal(pValue);` |
|        6 | 2604 | `		return SXRET_OK;` |
|        - | 2605 | `	}` |
|       34 | 2606 | `	return SXERR_INVALID;` |
|      199 | 2607 | `}` |
|      324 | 2608 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 2609 | `{` |
|      329 | 2610 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|      317 | 2611 | `		return SXRET_OK;` |
|        - | 2612 | `	}` |
|       13 | 2613 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      167 | 2614 | `}` |
|        - | 2615 | `/*` |
|        - | 2616 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|        - | 2617 | ` * class chain for a static typed slot whose mount-time default failed its` |
|        - | 2618 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|        - | 2619 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|        - | 2620 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|        - | 2621 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|        - | 2622 | ` * never-touched class stays silent, and the throw repeats on every access` |
|        - | 2623 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|        - | 2624 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|        - | 2625 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|        - | 2626 | ` */` |
|       26 | 2627 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 2628 | `{` |
|        - | 2629 | `	ph7_class *pScan;` |
|       33 | 2630 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        - | 2631 | `		SyHashEntry *pEntry;` |
|       29 | 2632 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|       33 | 2633 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|       29 | 2634 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       26 | 2635 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|        - | 2636 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|       24 | 2637 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       25 | 2638 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|       24 | 2639 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 | 2640 | `				if( pSlot ){` |
|       24 | 2641 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       24 | 2642 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|       24 | 2643 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        - | 2644 | `						ph7_value sNull;` |
|       24 | 2645 | `						if( pValue == 0 ){` |
|      ! 0 | 2646 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|      ! 0 | 2647 | `							pValue = &sNull;` |
|      ! 0 | 2648 | `						}` |
|       24 | 2649 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|        - | 2650 | `					}` |
|      ! 0 | 2651 | `				}` |
|      ! 0 | 2652 | `			}` |
|        1 | 2653 | `		}` |
|        3 | 2654 | `	}` |
|        5 | 2655 | `	return SXRET_OK;` |
|       16 | 2656 | `}` |
|        - | 2657 | `/*` |
|        - | 2658 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|        - | 2659 | ` * materialization: an initializer that threw at mount and was deferred` |
|        - | 2660 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|        - | 2661 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|        - | 2662 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|        - | 2663 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|        - | 2664 | ` * a subclass can be mounted before the base whose default failed.` |
|        - | 2665 | ` */` |
|  2217934 | 2666 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|        5 | 2667 | `{` |
|  4442219 | 2668 | `	while( pClass ){` |
|  2224367 | 2669 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|       85 | 2670 | `			return 1;` |
|        - | 2671 | `		}` |
|  2224285 | 2672 | `		pClass = pClass->pBase;` |
|        5 | 2673 | `	}` |
|  2217857 | 2674 | `	return 0;` |
|  1108972 | 2675 | `}` |
|        - | 2676 | `/*` |
|        - | 2677 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 2678 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 2679 | ` *` |
|        - | 2680 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 2681 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 2682 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 2683 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 2684 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 2685 | ` *` |
|        - | 2686 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 2687 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 2688 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 2689 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 2690 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 2691 | ` * re-raises on every access too.` |
|        - | 2692 | ` */` |
|       86 | 2693 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|        3 | 2694 | `{` |
|        - | 2695 | `	SyHashEntry *pEntry;` |
|        - | 2696 | `	sxi32 rc;` |
|       89 | 2697 | `	if( pClass->pBase ){` |
|        6 | 2698 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|        6 | 2699 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2700 | `			return rc;` |
|        - | 2701 | `		}` |
|        2 | 2702 | `	}` |
|       89 | 2703 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      209 | 2704 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      123 | 2705 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      123 | 2706 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - | 2707 | `			/* Not pending. An inherited slot the base pass already collected` |
|        - | 2708 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|        - | 2709 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|       61 | 2710 | `			continue;` |
|        - | 2711 | `		}` |
|       65 | 2712 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 2713 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|        - | 2714 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|        - | 2715 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|        - | 2716 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|        - | 2717 | `			 * class keeps its hint flag so a later access retries. */` |
|      ! 0 | 2718 | `			*pbLeft = 1;` |
|      ! 0 | 2719 | `			continue;` |
|        - | 2720 | `		}` |
|       65 | 2721 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|       65 | 2722 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2723 | `			return rc;` |
|        - | 2724 | `		}` |
|        3 | 2725 | `	}` |
|       89 | 2726 | `	return SXRET_OK;` |
|       46 | 2727 | `}` |
|        - | 2728 | `/*` |
|        - | 2729 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 2730 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 2731 | ` *` |
|        - | 2732 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 2733 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 2734 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 2735 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 2736 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 2737 | ` *` |
|        - | 2738 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 2739 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 2740 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 2741 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 2742 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 2743 | ` * re-raises on every access too.` |
|        - | 2744 | ` *` |
|        - | 2745 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|        - | 2746 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|        - | 2747 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|        - | 2748 | ` * cut the outer one short.` |
|        - | 2749 | ` */` |
|       82 | 2750 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|        3 | 2751 | `{` |
|        - | 2752 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|        - | 2753 | `	ph7_class_attr **apPending;` |
|        - | 2754 | `	sxu32 n,nUsed;` |
|        - | 2755 | `	sxi32 rc;` |
|       85 | 2756 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       85 | 2757 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|       85 | 2758 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|       85 | 2759 | `	nUsed = SySetUsed(&aPending);` |
|       89 | 2760 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|       63 | 2761 | `		ph7_class_attr *pAttr = apPending[n];` |
|       63 | 2762 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - | 2763 | `		ph7_class *pSaveCtx;` |
|        - | 2764 | `		void *pSaveFrame;` |
|        - | 2765 | `		sxu32 nSaveLazyLine;` |
|        - | 2766 | `		sxi32 nSaveLazyDepth;` |
|        - | 2767 | `		ph7_value *pMemObj;` |
|        - | 2768 | `		sxi32 rcExec;` |
|       63 | 2769 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|      ! 0 | 2770 | `			continue; /* the base pass already ran this shared slot */` |
|        - | 2771 | `		}` |
|       63 | 2772 | `		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       63 | 2773 | `		if( pMemObj == 0 ){` |
|      ! 0 | 2774 | `			continue;` |
|        - | 2775 | `		}` |
|       63 | 2776 | `		pSaveCtx = pVm->pConstEvalClass;` |
|       63 | 2777 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|       63 | 2778 | `		pVm->pConstEvalClass = pOwner;` |
|        - | 2779 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|        - | 2780 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|        - | 2781 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|        - | 2782 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|        - | 2783 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|       63 | 2784 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 2785 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|        - | 2786 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|       63 | 2787 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|       63 | 2788 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|       63 | 2789 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|       63 | 2790 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|       63 | 2791 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|       63 | 2792 | `		pVm->nConstEvalDepth++;` |
|       63 | 2793 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|       63 | 2794 | `		pVm->nConstEvalDepth--;` |
|       63 | 2795 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|       63 | 2796 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|       63 | 2797 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       63 | 2798 | `		pVm->pConstEvalClass = pSaveCtx;` |
|       63 | 2799 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|       63 | 2800 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 2801 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|        - | 2802 | `			 * caller to route (a catch here is the user's own). */` |
|       59 | 2803 | `			rc = rcExec;` |
|       59 | 2804 | `			break;` |
|        - | 2805 | `		}` |
|        5 | 2806 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|        5 | 2807 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 2808 | `			/* The initializer named a self-referencing constant. Like the mount` |
|        - | 2809 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|        - | 2810 | `			 * the access, where a catch can see it. */` |
|      ! 0 | 2811 | `			rc = VmConstCycleThrow(&(*pVm));` |
|      ! 0 | 2812 | `			break;` |
|        - | 2813 | `		}` |
|        4 | 2814 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|        3 | 2815 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 2816 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|        - | 2817 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|        - | 2818 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|      ! 0 | 2819 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      ! 0 | 2820 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|      ! 0 | 2821 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      ! 0 | 2822 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|      ! 0 | 2823 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|      ! 0 | 2824 | `				break;` |
|        - | 2825 | `			}` |
|      ! 0 | 2826 | `		}` |
|        3 | 2827 | `	}` |
|       85 | 2828 | `	if( rc != SXRET_OK ){` |
|       59 | 2829 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|       28 | 2830 | `	}` |
|       85 | 2831 | `	SySetRelease(&aPending);` |
|       85 | 2832 | `	return rc;` |
|        3 | 2833 | `}` |
|        - | 2834 | `/*` |
|        - | 2835 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|        - | 2836 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|        - | 2837 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|        - | 2838 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|        - | 2839 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|        - | 2840 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|        - | 2841 | ` */` |
|       82 | 2842 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 2843 | `{` |
|       85 | 2844 | `	int bLeft = 0;` |
|       85 | 2845 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|       85 | 2846 | `	if( rc == SXRET_OK ){` |
|       29 | 2847 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|       13 | 2848 | `	}` |
|       85 | 2849 | `	if( rc == SXRET_OK && !bLeft ){` |
|        - | 2850 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|        - | 2851 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|        - | 2852 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|        - | 2853 | `		 * set — php's materialization keeps failing too. */` |
|        - | 2854 | `		ph7_class *pScan;` |
|        9 | 2855 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        5 | 2856 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|        3 | 2857 | `		}` |
|        2 | 2858 | `	}` |
|       85 | 2859 | `	return rc;` |
|        3 | 2860 | `}` |
|        - | 2861 |  |
|        - | 2862 | `/*` |
|        - | 2863 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 2864 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 2865 | ` * information.` |
|        - | 2866 | ` * ------------------------------------` |
|        - | 2867 | ` * Simple boring wrapper function.` |
|        - | 2868 | ` * ------------------------------------` |
|        - | 2869 | ` */` |
|      782 | 2870 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        5 | 2871 | `{` |
|        - | 2872 | `	va_list ap;` |
|        - | 2873 | `	sxi32 rc;` |
|      787 | 2874 | `	va_start(ap,zFormat);` |
|      787 | 2875 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|      787 | 2876 | `	va_end(ap);` |
|      787 | 2877 | `	return rc;` |
|        5 | 2878 | `}` |
|        - | 2879 | `/*` |
|        - | 2880 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - | 2881 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - | 2882 | ` */` |
|      380 | 2883 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 | 2884 | `{` |
|        - | 2885 | `	ph7_class *pClass;` |
|        - | 2886 | `	ph7_class_instance *pThis;` |
|        - | 2887 | `	ph7_class_method *pCons;` |
|        - | 2888 | `	ph7_value sArg;` |
|        - | 2889 | `	ph7_value *apArg[1];` |
|        - | 2890 | `	SyBlob sMsg;` |
|        - | 2891 | `	SyString sMsgStr;` |
|      385 | 2892 | `	SyString *pFuncName = &pCallee->sName;` |
|        - | 2893 | `	VmFrame *pFrame;` |
|        - | 2894 | `	sxi32 rc;` |
|      385 | 2895 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      385 | 2896 | `	if( pClass == 0 ){` |
|      ! 0 | 2897 | `		return PH7_ABORT;` |
|        - | 2898 | `	}` |
|      385 | 2899 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      385 | 2900 | `	if( pThis == 0 ){` |
|      ! 0 | 2901 | `		return PH7_ABORT;` |
|        - | 2902 | `	}` |
|      385 | 2903 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 2904 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|        - | 2905 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|        - | 2906 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|        - | 2907 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|        - | 2908 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|      573 | 2909 | `	if( pOwnerClass ){` |
|        - | 2910 | `		/* A property hook is named after its PROPERTY, never after the method` |
|        - | 2911 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|        - | 2912 | `		SyBlob sHook;` |
|       41 | 2913 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|       41 | 2914 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|        6 | 2915 | `			if( pArgName ){` |
|        6 | 2916 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|        4 | 2917 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|        2 | 2918 | `					nArg,pArgName,zExpected,zGiven);` |
|        4 | 2919 | `			}else{` |
|      ! 0 | 2920 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      ! 0 | 2921 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|      ! 0 | 2922 | `					nArg,zExpected,zGiven);` |
|        - | 2923 | `			}` |
|        6 | 2924 | `			SyBlobRelease(&sHook);` |
|        6 | 2925 | `			goto ArgMsgBuilt;` |
|        - | 2926 | `		}` |
|       37 | 2927 | `		SyBlobRelease(&sHook);` |
|       37 | 2928 | `		if( pArgName ){` |
|       28 | 2929 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       12 | 2930 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       16 | 2931 | `		}else{` |
|       11 | 2932 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|        4 | 2933 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|        - | 2934 | `		}` |
|       21 | 2935 | `	}else{` |
|        - | 2936 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|      349 | 2937 | `		const char *zShow = 0;` |
|      349 | 2938 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|      349 | 2939 | `		if( pArgName ){` |
|      281 | 2940 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|      138 | 2941 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|      143 | 2942 | `		}else{` |
|       72 | 2943 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|       34 | 2944 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|        - | 2945 | `		}` |
|        - | 2946 | `	}` |
|      190 | 2947 | `ArgMsgBuilt:` |
|        - | 2948 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|        - | 2949 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|        - | 2950 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|      385 | 2951 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|      379 | 2952 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      379 | 2953 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|      379 | 2954 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|      187 | 2955 | `		}` |
|      187 | 2956 | `	}` |
|      385 | 2957 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      385 | 2958 | `	if( pCons ){` |
|      385 | 2959 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      385 | 2960 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      385 | 2961 | `		apArg[0] = &sArg;` |
|      385 | 2962 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      385 | 2963 | `		PH7_MemObjRelease(&sArg);` |
|      190 | 2964 | `	}` |
|      385 | 2965 | `	SyBlobRelease(&sMsg);` |
|      385 | 2966 | `	pFrame = pVm->pFrame;` |
|      385 | 2967 | `	if( pFrame ){` |
|      385 | 2968 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      385 | 2969 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      190 | 2970 | `	}` |
|      385 | 2971 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      385 | 2972 | `	PH7_ClassInstanceUnref(pThis);` |
|      385 | 2973 | `	if( rc == SXERR_ABORT ){` |
|        6 | 2974 | `		return PH7_ABORT;` |
|        - | 2975 | `	}` |
|      381 | 2976 | `	return PH7_EXCEPTION;` |
|      195 | 2977 | `}` |
|        - | 2978 | `/*` |
|        - | 2979 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|        - | 2980 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|        - | 2981 | ` * positional and the named-argument binding paths of OP_CALL.` |
|        - | 2982 | ` *` |
|        - | 2983 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|        - | 2984 | ` * element uses its overall call position; a NAMED element always reports` |
|        - | 2985 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|        - | 2986 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|        - | 2987 | ` * formal, so no single parameter name applies.` |
|        - | 2988 | ` *` |
|        - | 2989 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|        - | 2990 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|        - | 2991 | ` */` |
|     2768 | 2992 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|        - | 2993 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|        5 | 2994 | `{` |
|        - | 2995 | `	sxi32 rc;` |
|     2773 | 2996 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|       33 | 2997 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|       37 | 2998 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|        - | 2999 | `			const char *zGiven;` |
|       11 | 3000 | `			const char *zExpected = "union";` |
|        - | 3001 | `			char zBuf[128];` |
|        - | 3002 | `			char zTypeBuf[128];` |
|       11 | 3003 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 | 3004 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       10 | 3005 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 3006 | `				zGiven = "null";` |
|      ! 0 | 3007 | `			}else{` |
|        9 | 3008 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 3009 | `			}` |
|       11 | 3010 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       15 | 3011 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|        4 | 3012 | `					zTypeBuf,sizeof(zTypeBuf));` |
|        4 | 3013 | `			}` |
|       11 | 3014 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       11 | 3015 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3016 | `		}` |
|       17 | 3017 | `		return SXRET_OK;` |
|        - | 3018 | `	}` |
|     2746 | 3019 | `	if( pFormal->nType < 1` |
|     1485 | 3020 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|     2549 | 3021 | `		return SXRET_OK;` |
|        - | 3022 | `	}` |
|      207 | 3023 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 3024 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|        - | 3025 | `		 * per element exactly like the non-variadic paths. */` |
|       62 | 3026 | `		SyString *pName = &pFormal->sClass;` |
|        - | 3027 | `		ph7_class *pClass;` |
|       62 | 3028 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       62 | 3029 | `		if( rcPseudo == 0 ){` |
|        - | 3030 | `			/* Recognised pseudo-type; value mismatches */` |
|        - | 3031 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       14 | 3032 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        6 | 3033 | `				VmClassHintTypeName(pName,0,` |
|        6 | 3034 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|        3 | 3035 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        8 | 3036 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3037 | `		}` |
|        - | 3038 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|        - | 3039 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|        - | 3040 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|        - | 3041 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|        - | 3042 | `		 * so ANY non-object is a TypeError, matching php. */` |
|       56 | 3043 | `		pClass = 0;` |
|       56 | 3044 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 3045 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       43 | 3046 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 3047 | `				VmClassHintTypeName(pName,pClass,` |
|       20 | 3048 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 3049 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       23 | 3050 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3051 | `		}` |
|       34 | 3052 | `		return SXRET_OK;` |
|        - | 3053 | `	}` |
|      149 | 3054 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       63 | 3055 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|        - | 3056 | `			char zGivenBuf[128];` |
|        8 | 3057 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        2 | 3058 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        6 | 3059 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3060 | `		}` |
|       59 | 3061 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 3062 | `			char zTypeBuf[128];` |
|        - | 3063 | `			char zGivenBuf[128];` |
|       60 | 3064 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       19 | 3065 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       19 | 3066 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       41 | 3067 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3068 | `		}` |
|       11 | 3069 | `	}else{` |
|        - | 3070 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|        - | 3071 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|       89 | 3072 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 3073 | `	}` |
|      107 | 3074 | `	return SXRET_OK;` |
|     1389 | 3075 | `}` |
|        - | 3076 | `/*` |
|        - | 3077 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|        - | 3078 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|        - | 3079 | ` * before a required parameter as implicitly required), excluding a trailing` |
|        - | 3080 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|        - | 3081 | ` * pick php's wording — "exactly N expected" when required == total,` |
|        - | 3082 | ` * "at least N" when trailing optionals exist.` |
|        - | 3083 | ` */` |
|     7518 | 3084 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|        5 | 3085 | `{` |
|     7523 | 3086 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     7523 | 3087 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|     7523 | 3088 | `	sxu32 nRequired = 0;` |
|        - | 3089 | `	sxu32 n;` |
|     7523 | 3090 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      769 | 3091 | `		nFormal--;` |
|      382 | 3092 | `	}` |
|    37501 | 3093 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|    29983 | 3094 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|     9643 | 3095 | `			nRequired = n + 1;` |
|     4818 | 3096 | `		}` |
|    14991 | 3097 | `	}` |
|     7523 | 3098 | `	*pnNonVariadic = nFormal;` |
|     7523 | 3099 | `	return nRequired;` |
|        5 | 3100 | `}` |
|        - | 3101 | `/*` |
|        - | 3102 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|        - | 3103 | ` * with too few arguments:` |
|        - | 3104 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|        - | 3105 | ` *   {exactly\|at least} M expected` |
|        - | 3106 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|        - | 3107 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|        - | 3108 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|        - | 3109 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|        - | 3110 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|        - | 3111 | ` */` |
|       26 | 3112 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3113 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|        3 | 3114 | `{` |
|        - | 3115 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|        - | 3116 | `	SyBlob sMsg;` |
|       29 | 3117 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       29 | 3118 | `	if( pOwnerClass ){` |
|        5 | 3119 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|        2 | 3120 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|        3 | 3121 | `	}else{` |
|       25 | 3122 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|        - | 3123 | `	}` |
|       29 | 3124 | `	if( bCallSite ){` |
|       27 | 3125 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|       27 | 3126 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|       12 | 3127 | `	}` |
|       29 | 3128 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|       13 | 3129 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|        - | 3130 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       29 | 3131 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        3 | 3132 | `}` |
|        - | 3133 | `/*` |
|        - | 3134 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|        - | 3135 | ` * cannot be referenced:` |
|        - | 3136 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|        - | 3137 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|        - | 3138 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|        - | 3139 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|        - | 3140 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|        - | 3141 | `` * the bare `m()`.`` |
|        - | 3142 | ` */` |
|        - | 3143 | `/*` |
|        - | 3144 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|        - | 3145 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|        - | 3146 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|        - | 3147 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|        - | 3148 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|        - | 3149 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|        - | 3150 | `` * php's `{closure:file:line}`.`` |
|        - | 3151 | ` */` |
|       74 | 3152 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|        - | 3153 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        1 | 3154 | `{` |
|       75 | 3155 | `	const char *zShow = 0;` |
|       75 | 3156 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|        - | 3157 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|        - | 3158 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|        - | 3159 | `	 * single name applies (the refusal message splits the same way). */` |
|       75 | 3160 | `	if( pOwnerClass && pArgName ){` |
|       34 | 3161 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3162 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       11 | 3163 | `			&pOwnerClass->sName,nShow,zShow,nArgPos,pArgName);` |
|       64 | 3164 | `	}else if( pOwnerClass ){` |
|      ! 0 | 3165 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3166 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|      ! 0 | 3167 | `			&pOwnerClass->sName,nShow,zShow,nArgPos);` |
|       53 | 3168 | `	}else if( pArgName ){` |
|       73 | 3169 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3170 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       24 | 3171 | `			nShow,zShow,nArgPos,pArgName);` |
|       25 | 3172 | `	}else{` |
|        7 | 3173 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3174 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|        2 | 3175 | `			nShow,zShow,nArgPos);` |
|        - | 3176 | `	}` |
|       75 | 3177 | `}` |
|     4026 | 3178 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3179 | `	sxu32 nArgPos,SyString *pArgName)` |
|        2 | 3180 | `{` |
|        - | 3181 | `	SyBlob sMsg;` |
|     4028 | 3182 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     4028 | 3183 | `	if( pOwnerClass ){` |
|        8 | 3184 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) could not be passed by reference",` |
|        3 | 3185 | `			&pOwnerClass->sName,pFuncName,nArgPos,pArgName);` |
|        5 | 3186 | `	}else{` |
|     4022 | 3187 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) could not be passed by reference",` |
|     2010 | 3188 | `			pFuncName,nArgPos,pArgName);` |
|        - | 3189 | `	}` |
|        - | 3190 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     4028 | 3191 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 3192 | `}` |
|        - | 3193 | `/*` |
|        - | 3194 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|        - | 3195 | ` * called with too few arguments, in php's ZPP wording:` |
|        - | 3196 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|        - | 3197 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|        - | 3198 | ` * pluralized on the expected count).` |
|        - | 3199 | ` *` |
|        - | 3200 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|        - | 3201 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|        - | 3202 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|        - | 3203 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|        - | 3204 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|        - | 3205 | ` */` |
|       34 | 3206 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3207 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|        1 | 3208 | `{` |
|        - | 3209 | `	SyBlob sMsg;` |
|       35 | 3210 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|       35 | 3211 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       35 | 3212 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 | 3213 | `	if( pOwnerClass ){` |
|      ! 0 | 3214 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3215 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      ! 0 | 3216 | `	}else{` |
|       35 | 3217 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       17 | 3218 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|        - | 3219 | `	}` |
|        - | 3220 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       35 | 3221 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3222 | `}` |
|        - | 3223 | `/*` |
|        - | 3224 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|        - | 3225 | ` * MANY arguments, in php's ZPP wording:` |
|        - | 3226 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|        - | 3227 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|        - | 3228 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|        - | 3229 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|        - | 3230 | ` * so the caller must not route such a callee here.` |
|        - | 3231 | ` */` |
|       26 | 3232 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3233 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|        1 | 3234 | `{` |
|        - | 3235 | `	SyBlob sMsg;` |
|       27 | 3236 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|       27 | 3237 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|       27 | 3238 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       27 | 3239 | `	if( pOwnerClass ){` |
|      ! 0 | 3240 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3241 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|      ! 0 | 3242 | `	}else{` |
|       27 | 3243 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       13 | 3244 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|        - | 3245 | `	}` |
|        - | 3246 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       27 | 3247 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3248 | `}` |
|        - | 3249 | `/*` |
|        - | 3250 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|        - | 3251 | ` * named or positional argument resolved to:` |
|        - | 3252 | ` *   C::f(): Argument #N ($x) not passed` |
|        - | 3253 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|        - | 3254 | ` */` |
|        2 | 3255 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3256 | `	sxu32 nArg,SyString *pArgName)` |
|        1 | 3257 | `{` |
|        - | 3258 | `	SyBlob sMsg;` |
|        3 | 3259 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 3260 | `	if( pOwnerClass ){` |
|      ! 0 | 3261 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|      ! 0 | 3262 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|      ! 0 | 3263 | `	}else{` |
|        3 | 3264 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|        - | 3265 | `	}` |
|        - | 3266 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|        3 | 3267 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3268 | `}` |
|        - | 3269 | `/*` |
|        - | 3270 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - | 3271 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - | 3272 | ` */` |
|        - | 3273 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|        - | 3274 | ` * The message is copied into the instance by __construct, so the caller owns` |
|        - | 3275 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|      146 | 3276 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|        5 | 3277 | `{` |
|        - | 3278 | `	ph7_class *pClass;` |
|        - | 3279 | `	ph7_class_instance *pThis;` |
|        - | 3280 | `	ph7_class_method *pCons;` |
|        - | 3281 | `	ph7_value sArg;` |
|        - | 3282 | `	ph7_value *apArg[1];` |
|        - | 3283 | `	SyString sMsgStr;` |
|        - | 3284 | `	VmFrame *pFrame;` |
|        - | 3285 | `	sxi32 rc;` |
|      151 | 3286 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      151 | 3287 | `	if( pClass == 0 ){` |
|      ! 0 | 3288 | `		return PH7_ABORT;` |
|        - | 3289 | `	}` |
|      151 | 3290 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      151 | 3291 | `	if( pThis == 0 ){` |
|      ! 0 | 3292 | `		return PH7_ABORT;` |
|        - | 3293 | `	}` |
|      151 | 3294 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      151 | 3295 | `	if( pCons ){` |
|      151 | 3296 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      151 | 3297 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      151 | 3298 | `		apArg[0] = &sArg;` |
|      151 | 3299 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      151 | 3300 | `		PH7_MemObjRelease(&sArg);` |
|       73 | 3301 | `	}` |
|      151 | 3302 | `	pFrame = pVm->pFrame;` |
|      151 | 3303 | `	if( pFrame ){` |
|      151 | 3304 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      151 | 3305 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       73 | 3306 | `	}` |
|      151 | 3307 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      151 | 3308 | `	PH7_ClassInstanceUnref(pThis);` |
|      151 | 3309 | `	if( rc == SXERR_ABORT ){` |
|        6 | 3310 | `		return PH7_ABORT;` |
|        - | 3311 | `	}` |
|      147 | 3312 | `	return PH7_EXCEPTION;` |
|       78 | 3313 | `}` |
|        - | 3314 | `/*` |
|        - | 3315 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|        - | 3316 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|        - | 3317 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|        - | 3318 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|        - | 3319 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|        - | 3320 | ` * other callee falls through to the ordinary Class::method rendering.` |
|        - | 3321 | ` */` |
|        - | 3322 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|   633500 | 3323 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|        5 | 3324 | `{` |
|   633505 | 3325 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|   633505 | 3326 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|   633379 | 3327 | `		return 0;` |
|        - | 3328 | `	}` |
|      131 | 3329 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|       87 | 3330 | `		return 0;` |
|        - | 3331 | `	}` |
|       47 | 3332 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|       35 | 3333 | `		*pzKind = "get";` |
|       31 | 3334 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|       16 | 3335 | `		*pzKind = "set";` |
|       10 | 3336 | `	}else{` |
|      ! 0 | 3337 | `		return 0;` |
|        - | 3338 | `	}` |
|       47 | 3339 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|       47 | 3340 | `	return 1;` |
|   316755 | 3341 | `}` |
|      128 | 3342 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3343 | `{` |
|        - | 3344 | `	SyString sProp;` |
|        - | 3345 | `	const char *zKind;` |
|      133 | 3346 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      123 | 3347 | `		return 0;` |
|        - | 3348 | `	}` |
|       13 | 3349 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|       13 | 3350 | `	return 1;` |
|       69 | 3351 | `}` |
|        - | 3352 | `/*` |
|        - | 3353 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|        - | 3354 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|        - | 3355 | ` * anything else uses its display name (which is also what strips a closure's` |
|        - | 3356 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|        - | 3357 | ` * instead — they are thrown from call sites that already resolved it.` |
|        - | 3358 | ` */` |
|      162 | 3359 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3360 | `{` |
|      167 | 3361 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       96 | 3362 | `		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){` |
|        7 | 3363 | `			return;` |
|        - | 3364 | `		}` |
|       90 | 3365 | `		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);` |
|       90 | 3366 | `		return;` |
|        - | 3367 | `	}` |
|        - | 3368 | `	{` |
|       75 | 3369 | `		const char *zShow = 0;` |
|       75 | 3370 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|       75 | 3371 | `		if( zShow && nShow > 0 ){` |
|       75 | 3372 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       35 | 3373 | `		}` |
|        - | 3374 | `	}` |
|       86 | 3375 | `}` |
|      142 | 3376 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|        5 | 3377 | `{` |
|        - | 3378 | `	SyBlob sMsg,sName;` |
|        - | 3379 | `	sxi32 rc;` |
|      147 | 3380 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      147 | 3381 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|      147 | 3382 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|      147 | 3383 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|      142 | 3384 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|      147 | 3385 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      147 | 3386 | `	SyBlobRelease(&sName);` |
|      147 | 3387 | `	SyBlobRelease(&sMsg);` |
|      147 | 3388 | `	return rc;` |
|        5 | 3389 | `}` |
|        - | 3390 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|        - | 3391 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|        - | 3392 | ` * php calls it a "method" when it is one. */` |
|        4 | 3393 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        2 | 3394 | `{` |
|        - | 3395 | `	SyBlob sMsg,sName;` |
|        - | 3396 | `	sxi32 rc;` |
|        6 | 3397 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 3398 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|        6 | 3399 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|        6 | 3400 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|        4 | 3401 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|        4 | 3402 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|        6 | 3403 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|        6 | 3404 | `	SyBlobRelease(&sName);` |
|        6 | 3405 | `	SyBlobRelease(&sMsg);` |
|        6 | 3406 | `	return rc;` |
|        2 | 3407 | `}` |
|        - | 3408 | `/*` |
|        - | 3409 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - | 3410 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - | 3411 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - | 3412 | ` */` |
|     1016 | 3413 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        5 | 3414 | `{` |
|     1021 | 3415 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 3416 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 3417 | `	}` |
|      917 | 3418 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      103 | 3419 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      103 | 3420 | `		if( pThis && pThis->pClass ){` |
|      103 | 3421 | `			SyString *pName = &pThis->pClass->sName;` |
|      103 | 3422 | `			sxu32 n = pName->nByte;` |
|      103 | 3423 | `			if( n >= nBuf ){` |
|      ! 0 | 3424 | `				n = nBuf - 1;` |
|      ! 0 | 3425 | `			}` |
|      103 | 3426 | `			SyMemcpy(pName->zString,zBuf,n);` |
|      103 | 3427 | `			zBuf[n] = 0;` |
|      103 | 3428 | `			return zBuf;` |
|        - | 3429 | `		}` |
|      ! 0 | 3430 | `		return "object";` |
|        - | 3431 | `	}` |
|      819 | 3432 | `	return ph7_type_name(pVal);` |
|      513 | 3433 | `}` |
|        - | 3434 | `/*` |
|        - | 3435 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - | 3436 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - | 3437 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - | 3438 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - | 3439 | ` */` |
|       18 | 3440 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        2 | 3441 | `{` |
|        - | 3442 | `	ph7_class *pClass;` |
|        - | 3443 | `	ph7_class_instance *pThis;` |
|        - | 3444 | `	ph7_class_method *pCons;` |
|        - | 3445 | `	ph7_value sArg;` |
|        - | 3446 | `	ph7_value *apArg[1];` |
|        - | 3447 | `	SyBlob sMsg;` |
|        - | 3448 | `	SyString sMsgStr;` |
|        - | 3449 | `	VmFrame *pFrame;` |
|        - | 3450 | `	sxi32 rc;` |
|       20 | 3451 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - | 3452 | `	char zNameBuf[64];` |
|       20 | 3453 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       20 | 3454 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       20 | 3455 | `	if( pClass == 0 ){` |
|      ! 0 | 3456 | `		return PH7_ABORT;` |
|        - | 3457 | `	}` |
|       20 | 3458 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       20 | 3459 | `	if( pThis == 0 ){` |
|      ! 0 | 3460 | `		return PH7_ABORT;` |
|        - | 3461 | `	}` |
|       20 | 3462 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       20 | 3463 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       20 | 3464 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       20 | 3465 | `	if( pCons ){` |
|       20 | 3466 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       20 | 3467 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       20 | 3468 | `		apArg[0] = &sArg;` |
|       20 | 3469 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       20 | 3470 | `		PH7_MemObjRelease(&sArg);` |
|        9 | 3471 | `	}` |
|       20 | 3472 | `	SyBlobRelease(&sMsg);` |
|       20 | 3473 | `	pFrame = pVm->pFrame;` |
|       20 | 3474 | `	if( pFrame ){` |
|       20 | 3475 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       20 | 3476 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 | 3477 | `	}` |
|       20 | 3478 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       20 | 3479 | `	PH7_ClassInstanceUnref(pThis);` |
|       20 | 3480 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3481 | `		return PH7_ABORT;` |
|        - | 3482 | `	}` |
|       20 | 3483 | `	return PH7_EXCEPTION;` |
|       11 | 3484 | `}` |
|        - | 3485 | `/*` |
|        - | 3486 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - | 3487 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - | 3488 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - | 3489 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - | 3490 | ` */` |
|        - | 3491 | `/*` |
|        - | 3492 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|        - | 3493 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|        - | 3494 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|        - | 3495 | ` * type field.` |
|        - | 3496 | ` */` |
|   792104 | 3497 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|        5 | 3498 | `{` |
|   792109 | 3499 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|        5 | 3500 | `}` |
|    12702 | 3501 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 3502 | `{` |
|    12707 | 3503 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    12707 | 3504 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|        - | 3505 | `	const char *zGiven;` |
|        - | 3506 | `	ph7_class *pHintScope;` |
|        - | 3507 | `	char zBuf[128];` |
|        - | 3508 | `	char zTypeBuf[128];` |
|        - | 3509 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    12707 | 3510 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|      ! 0 | 3511 | `		return SXRET_OK;` |
|        - | 3512 | `	}` |
|        - | 3513 | `	/* never return type: the function must not return at all. An explicit` |
|        - | 3514 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|        - | 3515 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|        - | 3516 | `	 * the call site). */` |
|    12707 | 3517 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        6 | 3518 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|        - | 3519 | `	}` |
|        - | 3520 | `	/* void return type: the function must not produce a value. */` |
|    12703 | 3521 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     1953 | 3522 | `		if( pValue == 0 ){` |
|     1949 | 3523 | `			return SXRET_OK;` |
|        - | 3524 | `		}` |
|        - | 3525 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|        - | 3526 | `		 * store, not a return: php compiles no return statement there at all,` |
|        - | 3527 | `		 * and still reports the hook's return type as void. PHL carries the` |
|        - | 3528 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|        - | 3529 | `		 * so the one implicit value this arm must not reject is that one. */` |
|        6 | 3530 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|        6 | 3531 | `			return SXRET_OK;` |
|        - | 3532 | `		}` |
|        - | 3533 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - | 3534 | `		 * still counts as "returned a value" here. */` |
|      ! 0 | 3535 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 | 3536 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|        - | 3537 | `	}` |
|        - | 3538 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|        - | 3539 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|        - | 3540 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    10755 | 3541 | `	if( pValue == 0 ){` |
|       33 | 3542 | `		const char *zExpected = "value";` |
|       33 | 3543 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       48 | 3544 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|       15 | 3545 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|       15 | 3546 | `		}` |
|        - | 3547 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|        - | 3548 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|       33 | 3549 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|        - | 3550 | `	}` |
|        - | 3551 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - | 3552 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - | 3553 | `	 * matching how every other typed return reports a missing value.) */` |
|    10725 | 3554 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 | 3555 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 3556 | `			return SXRET_OK;` |
|        - | 3557 | `		}` |
|        4 | 3558 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|        1 | 3559 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3560 | `	}` |
|        - | 3561 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|        - | 3562 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|        - | 3563 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    10721 | 3564 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|       41 | 3565 | `		return SXRET_OK;` |
|        - | 3566 | `	}` |
|        - | 3567 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - | 3568 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - | 3569 | `	 * Check by value before the real-class instanceof branch below. */` |
|    10685 | 3570 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      707 | 3571 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|      707 | 3572 | `		if( rcPseudo == 1 ){` |
|      156 | 3573 | `			return SXRET_OK;` |
|        - | 3574 | `		}` |
|      555 | 3575 | `		if( rcPseudo == 0 ){` |
|       19 | 3576 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 3577 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|        4 | 3578 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3579 | `		}` |
|        - | 3580 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      271 | 3581 | `	}` |
|        - | 3582 | `	/* The two branches below are the only ones that can name a class, so the` |
|        - | 3583 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|        - | 3584 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|        - | 3585 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|        - | 3586 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|        - | 3587 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|        - | 3588 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    10525 | 3589 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|        - | 3590 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|        - | 3591 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|        - | 3592 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    10525 | 3593 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - | 3594 | `		sxi32 rcU;` |
|       41 | 3595 | `		const char *zExpected = "union";` |
|       41 | 3596 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|       41 | 3597 | `		if( rcU == SXRET_OK ){` |
|       31 | 3598 | `			return SXRET_OK;` |
|        - | 3599 | `		}` |
|       12 | 3600 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       10 | 3601 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|        7 | 3602 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 3603 | `			zGiven = "null";` |
|      ! 0 | 3604 | `		}else{` |
|        3 | 3605 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 3606 | `		}` |
|       12 | 3607 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       17 | 3608 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|        5 | 3609 | `				zTypeBuf,sizeof(zTypeBuf));` |
|        5 | 3610 | `		}` |
|       12 | 3611 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|        - | 3612 | `	}` |
|        - | 3613 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - | 3614 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - | 3615 | `	 * it into the TypeError message. */` |
|    10489 | 3616 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      547 | 3617 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      547 | 3618 | `		ph7_class *pExpected = 0;` |
|      547 | 3619 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|       34 | 3620 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       28 | 3621 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       16 | 3622 | `			}else{` |
|        8 | 3623 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 3624 | `			}` |
|       49 | 3625 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       15 | 3626 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|        - | 3627 | `		}` |
|      517 | 3628 | `		return SXRET_OK;` |
|        - | 3629 | `	}` |
|        - | 3630 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|        - | 3631 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|        - | 3632 | `	 * non-nullable scalar return — a TypeError. */` |
|     9947 | 3633 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       26 | 3634 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 3635 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 3636 | `			"null");` |
|        - | 3637 | `	}` |
|        - | 3638 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|        - | 3639 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|     9931 | 3640 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|     9809 | 3641 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|     9809 | 3642 | `		return SXRET_OK;` |
|        - | 3643 | `	}` |
|        - | 3644 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|        - | 3645 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|        - | 3646 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|        - | 3647 | `	 * still rejects the object under strict_types. */` |
|      127 | 3648 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       22 | 3649 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       31 | 3650 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       18 | 3651 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       20 | 3652 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       29 | 3653 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        9 | 3654 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        9 | 3655 | `				zGiven);` |
|        - | 3656 | `		}` |
|        1 | 3657 | `	}` |
|        - | 3658 | `	/* Array <-> scalar is never compatible. */` |
|      109 | 3659 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       33 | 3660 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       10 | 3661 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 3662 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 3663 | `	}` |
|        - | 3664 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - | 3665 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - | 3666 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - | 3667 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       84 | 3668 | `	if( !bStrict` |
|       83 | 3669 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       49 | 3670 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       54 | 3671 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       12 | 3672 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        3 | 3673 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 3674 | `			"string");` |
|        - | 3675 | `	}` |
|       82 | 3676 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       80 | 3677 | `		return SXRET_OK;` |
|        - | 3678 | `	}` |
|        4 | 3679 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        1 | 3680 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 3681 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|     6353 | 3682 | `}` |
|        - | 3683 | `/*` |
|        - | 3684 | ` * Report a fatal named-argument error.` |
|        - | 3685 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - | 3686 | ` */` |
|       12 | 3687 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        3 | 3688 | `{` |
|        - | 3689 | `	SyBlob sMsg;` |
|        - | 3690 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|        - | 3691 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|        - | 3692 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|        - | 3693 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|        - | 3694 | `	 * unconditional fatal even inside try/catch. */` |
|       15 | 3695 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 | 3696 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|       15 | 3697 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 3698 | `}` |
|        - | 3699 | `/*` |
|        - | 3700 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3701 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 3702 | ` * information.` |
|        - | 3703 | ` * ------------------------------------` |
|        - | 3704 | ` * Simple boring wrapper function.` |
|        - | 3705 | ` * ------------------------------------` |
|        - | 3706 | ` */` |
|    26852 | 3707 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        5 | 3708 | `{` |
|        - | 3709 | `	sxi32 rc;` |
|    26857 | 3710 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|    26857 | 3711 | `	return rc;` |
|        5 | 3712 | `}` |
|        - | 3713 | `/*` |
|        - | 3714 | ` * Resolve function context from the current frame.` |
|        - | 3715 | ` */` |
|        - | 3716 | `/*` |
|        - | 3717 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|        - | 3718 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|        - | 3719 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|        - | 3720 | ` * straight at the function's own name otherwise.` |
|        - | 3721 | ` */` |
|   633318 | 3722 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|        5 | 3723 | `{` |
|   633323 | 3724 | `	const char *zName = pFunc->sName.zString;` |
|   633323 | 3725 | `	int nName = (int)pFunc->sName.nByte;` |
|   683929 | 3726 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   686027 | 3727 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|        - | 3728 | `	/* A property hook is not a method in php and never shows the name PHL` |
|        - | 3729 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|        - | 3730 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|        - | 3731 | `	 * report from inside one, and what makes the trace line read` |
|        - | 3732 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|        - | 3733 | ``	 * `C::$p::get` for free. */`` |
|        - | 3734 | `	{` |
|        - | 3735 | `		SyString sProp;` |
|        - | 3736 | `		const char *zKind;` |
|   633323 | 3737 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|       44 | 3738 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       13 | 3739 | `				"$%z::%s",&sProp,zKind);` |
|       31 | 3740 | `			*pzOut = pVm->zDisplayName;` |
|       31 | 3741 | `			return n;` |
|        - | 3742 | `		}` |
|        - | 3743 | `	}` |
|   633297 | 3744 | `	if( bClosure ){` |
|        - | 3745 | `		int n;` |
|     4473 | 3746 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|        - | 3747 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|        - | 3748 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|     4473 | 3749 | `			*pzOut = pFunc->sClosureName.zString;` |
|     4473 | 3750 | `			return (int)pFunc->sClosureName.nByte;` |
|        - | 3751 | `		}` |
|      ! 0 | 3752 | `		if( pFunc->sFile.nByte > 0 ){` |
|      ! 0 | 3753 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      ! 0 | 3754 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|      ! 0 | 3755 | `		}else{` |
|      ! 0 | 3756 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|        - | 3757 | `		}` |
|      ! 0 | 3758 | `		*pzOut = pVm->zDisplayName;` |
|      ! 0 | 3759 | `		return n;` |
|        - | 3760 | `	}` |
|   628829 | 3761 | `	*pzOut = zName;` |
|   628829 | 3762 | `	return nName;` |
|   316664 | 3763 | `}` |
|        - | 3764 | `/*` |
|        - | 3765 | `` * php's name for the ACTIVE function -- the one its `name(): ` diagnostic`` |
|        - | 3766 | ` * qualifier prints. A method is rendered with its declaring class ("W::go"),` |
|        - | 3767 | `` * a closure with php's `{closure:file:line}`, and the global scope with php's`` |
|        - | 3768 | ` * own "main". VmGetFrameContext answers the same question in the shape the` |
|        - | 3769 | ` * uncaught-exception reporter wants (a bare display name, nothing at global` |
|        - | 3770 | ` * scope); this one is for a diagnostic raised from INSIDE an internal` |
|        - | 3771 | ` * function on the caller's behalf, which is how php attributes libxml's` |
|        - | 3772 | `` * errors -- `$el->nodeValue = 'a&b'` warns under the caller's name, not under`` |
|        - | 3773 | ` * the accessor's.` |
|        - | 3774 | ` */` |
|       76 | 3775 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut)` |
|        2 | 3776 | `{` |
|       78 | 3777 | `	VmFrame *pFrame = pVm->pFrame;` |
|       78 | 3778 | `	ph7_vm_func *pFunc = 0;` |
|       78 | 3779 | `	if( pFrame ){` |
|       78 | 3780 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       78 | 3781 | `		if( pFrame->pParent ){` |
|       17 | 3782 | `			pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        8 | 3783 | `		}` |
|       38 | 3784 | `	}` |
|       78 | 3785 | `	if( pFunc ){` |
|       17 | 3786 | `		VmReturnFuncName(&(*pVm),pFunc,pOut);` |
|        9 | 3787 | `	}else{` |
|       62 | 3788 | `		SyBlobAppend(pOut,"main",sizeof("main")-1);` |
|        - | 3789 | `	}` |
|       78 | 3790 | `	SyBlobNullAppend(pOut);` |
|       78 | 3791 | `}` |
|     1148 | 3792 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 | 3793 | `{` |
|        - | 3794 | `	VmFrame *pFrame;` |
|        - | 3795 | `	ph7_vm_func *pFunc;` |
|     1152 | 3796 | `	*pzFuncName = 0;` |
|     1152 | 3797 | `	*pnFuncLen = 0;` |
|     1152 | 3798 | `	pFrame = pVm->pFrame;` |
|     1152 | 3799 | `	if( pFrame == 0 ){` |
|      ! 0 | 3800 | `		return;` |
|        - | 3801 | `	}` |
|     1152 | 3802 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1152 | 3803 | `	if( pFrame->pParent == 0 ){` |
|     1112 | 3804 | `		return;` |
|        - | 3805 | `	}` |
|       44 | 3806 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       44 | 3807 | `	if( pFunc == 0 ){` |
|      ! 0 | 3808 | `		return;` |
|        - | 3809 | `	}` |
|       44 | 3810 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|      578 | 3811 | `}` |
|        - | 3812 | `/*` |
|        - | 3813 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|        - | 3814 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|        - | 3815 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|        - | 3816 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|        - | 3817 | ` * built-in library and already produces php's exact byte format, which keeps` |
|        - | 3818 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|        - | 3819 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|        - | 3820 | ` * synthesize what it can.` |
|        - | 3821 | ` */` |
|      598 | 3822 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 3823 | `{` |
|        - | 3824 | `	ph7_class_method *pGetTrace;` |
|        - | 3825 | `	ph7_value sTrace;` |
|        - | 3826 | `	const char *zTmp;` |
|        - | 3827 | `	int nTmp;` |
|      602 | 3828 | `	int bDone = 0;` |
|        - | 3829 | `	int bSaved;` |
|      602 | 3830 | `	if( pThis == 0 ){` |
|        5 | 3831 | `		return 0;` |
|        - | 3832 | `	}` |
|      598 | 3833 | `	if( pVm->bRenderingUncaught ){` |
|        - | 3834 | `		/* Already inside a report: do not run userland trace code again. */` |
|      ! 0 | 3835 | `		return 0;` |
|        - | 3836 | `	}` |
|      598 | 3837 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|      598 | 3838 | `	if( pGetTrace == 0 ){` |
|      ! 0 | 3839 | `		return 0;` |
|        - | 3840 | `	}` |
|      598 | 3841 | `	PH7_MemObjInit(pVm,&sTrace);` |
|        - | 3842 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|        - | 3843 | `	 * anything it calls) throws, the throw would be reported by this very` |
|        - | 3844 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|        - | 3845 | `	 * trace rather than re-entering here forever. */` |
|      598 | 3846 | `	bSaved = pVm->bRenderingUncaught;` |
|      598 | 3847 | `	pVm->bRenderingUncaught = 1;` |
|      598 | 3848 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|      598 | 3849 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|      598 | 3850 | `		if( zTmp && nTmp > 0 ){` |
|      598 | 3851 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      598 | 3852 | `			bDone = 1;` |
|      297 | 3853 | `		}` |
|      297 | 3854 | `	}` |
|      598 | 3855 | `	PH7_MemObjRelease(&sTrace);` |
|      598 | 3856 | `	pVm->bRenderingUncaught = bSaved;` |
|      598 | 3857 | `	return bDone;` |
|      303 | 3858 | `}` |
|        - | 3859 | `/*` |
|        - | 3860 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|        - | 3861 | ` *` |
|        - | 3862 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|        - | 3863 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|        - | 3864 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|        - | 3865 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|        - | 3866 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|        - | 3867 | ` *             trailer.` |
|        - | 3868 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|        - | 3869 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|        - | 3870 | ` * call; this routine only appends.` |
|        - | 3871 | ` */` |
|      598 | 3872 | `static void VmRenderUncaughtEntry(` |
|        - | 3873 | `	ph7_vm *pVm,SyBlob *pOut,` |
|        - | 3874 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|        - | 3875 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|        - | 3876 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|        - | 3877 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|        - | 3878 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|        4 | 3879 | `{` |
|        - | 3880 | `	SyString *pFile;` |
|      602 | 3881 | `	if( nThrowLine == 0 ){` |
|        5 | 3882 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|        2 | 3883 | `	}` |
|      602 | 3884 | `	if( nCallLine == 0 ){` |
|      602 | 3885 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      602 | 3886 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|      299 | 3887 | `	}` |
|      602 | 3888 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 | 3889 | `		zClass = "Exception";` |
|      ! 0 | 3890 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 | 3891 | `	}` |
|      602 | 3892 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      564 | 3893 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      280 | 3894 | `	}` |
|      602 | 3895 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      602 | 3896 | `	if( bFirst ){` |
|      596 | 3897 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      300 | 3898 | `	}else{` |
|        8 | 3899 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|        - | 3900 | `	}` |
|      602 | 3901 | `	SyBlobAppend(pOut,zClass,nClass);` |
|      602 | 3902 | `	if( zMsg && nMsg > 0 ){` |
|      602 | 3903 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|      602 | 3904 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|      299 | 3905 | `	}` |
|      602 | 3906 | `	if( pFile ){` |
|      602 | 3907 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      299 | 3908 | `	}` |
|      602 | 3909 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        - | 3910 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|        - | 3911 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|        - | 3912 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|        - | 3913 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|        - | 3914 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|        - | 3915 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|        - | 3916 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|      602 | 3917 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|        5 | 3918 | `		int bFrame = 0;` |
|        5 | 3919 | `		if( zFuncName && nFuncLen > 0 ){` |
|        3 | 3920 | `			if( pFile ){` |
|        - | 3921 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|        - | 3922 | `				 * running inside it. */` |
|        4 | 3923 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|        2 | 3924 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|        2 | 3925 | `			}else{` |
|      ! 0 | 3926 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|        - | 3927 | `			}` |
|        3 | 3928 | `			bFrame = 1;` |
|        1 | 3929 | `		}` |
|        - | 3930 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|        7 | 3931 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|        2 | 3932 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|        2 | 3933 | `	}` |
|      602 | 3934 | `	if( bLast && pFile ){` |
|      596 | 3935 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|      596 | 3936 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      296 | 3937 | `	}` |
|      602 | 3938 | `}` |
|        - | 3939 | `/*` |
|        - | 3940 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|        - | 3941 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|        - | 3942 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|        - | 3943 | ` */` |
|        4 | 3944 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 | 3945 | `{` |
|        - | 3946 | `	SyBlob sOut;` |
|        - | 3947 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|        - | 3948 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|        5 | 3949 | `	pVm->iExitStatus = 255;` |
|        5 | 3950 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 3951 | `		return PH7_OK;` |
|        - | 3952 | `	}` |
|        5 | 3953 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        5 | 3954 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|        5 | 3955 | `	VmCallErrorHandler(pVm,&sOut);` |
|        5 | 3956 | `	SyBlobRelease(&sOut);` |
|        5 | 3957 | `	return PH7_ABORT;` |
|        3 | 3958 | `}` |
|        - | 3959 | `/*` |
|        - | 3960 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|        - | 3961 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|        - | 3962 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|        - | 3963 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|        - | 3964 | ` */` |
|        - | 3965 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|      594 | 3966 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|        4 | 3967 | `{` |
|        - | 3968 | `	ph7_value *pValue;` |
|        - | 3969 | `	ph7_class_instance *pPrev;` |
|        - | 3970 | `	ph7_class *pThrowable;` |
|      598 | 3971 | `	if( pThis == 0 ){` |
|      ! 0 | 3972 | `		return 0;` |
|        - | 3973 | `	}` |
|      598 | 3974 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      598 | 3975 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      592 | 3976 | `		return 0;` |
|        - | 3977 | `	}` |
|        8 | 3978 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 3979 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|        - | 3980 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|        - | 3981 | `	 * never renders a stray object as an exception entry. */` |
|        8 | 3982 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|        8 | 3983 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|      ! 0 | 3984 | `		return 0;` |
|        - | 3985 | `	}` |
|        8 | 3986 | `	return pPrev;` |
|      301 | 3987 | `}` |
|        - | 3988 | `/*` |
|        - | 3989 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|        - | 3990 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|        - | 3991 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|        - | 3992 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|        - | 3993 | ` */` |
|       16 | 3994 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|        3 | 3995 | `{` |
|        - | 3996 | `	ph7_value *pValue;` |
|       19 | 3997 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|      ! 0 | 3998 | `		return;` |
|        - | 3999 | `	}` |
|       19 | 4000 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|       19 | 4001 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        3 | 4002 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|        - | 4003 | `	}` |
|       17 | 4004 | `	pPrev->iRef++;` |
|        - | 4005 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|        - | 4006 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|       17 | 4007 | `	PH7_MemObjRelease(pValue);` |
|       17 | 4008 | `	pValue->x.pOther = pPrev;` |
|       17 | 4009 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|       11 | 4010 | `}` |
|        - | 4011 | `/*` |
|        - | 4012 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|        - | 4013 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|        - | 4014 | ` * absent or yields an empty string.` |
|        - | 4015 | ` */` |
|        - | 4016 | `/*` |
|        - | 4017 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|        - | 4018 | ` * 0 when the class exposes no getLine().` |
|        - | 4019 | ` */` |
|      594 | 4020 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        4 | 4021 | `{` |
|        - | 4022 | `	ph7_class_method *pGetLine;` |
|        - | 4023 | `	ph7_value sLine;` |
|      598 | 4024 | `	sxu32 nLine = 0;` |
|      598 | 4025 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|      598 | 4026 | `	if( pGetLine == 0 ){` |
|      ! 0 | 4027 | `		return 0;` |
|        - | 4028 | `	}` |
|      598 | 4029 | `	PH7_MemObjInit(pVm,&sLine);` |
|      598 | 4030 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|      598 | 4031 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|      598 | 4032 | `		if( n > 0 ){` |
|      598 | 4033 | `			nLine = (sxu32)n;` |
|      297 | 4034 | `		}` |
|      297 | 4035 | `	}` |
|      598 | 4036 | `	PH7_MemObjRelease(&sLine);` |
|      598 | 4037 | `	return nLine;` |
|      301 | 4038 | `}` |
|      594 | 4039 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 4040 | `{` |
|        - | 4041 | `	ph7_class_method *pGetMessage;` |
|        - | 4042 | `	ph7_value sMsg;` |
|        - | 4043 | `	const char *zTmp;` |
|        - | 4044 | `	int nTmp;` |
|      598 | 4045 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      598 | 4046 | `	if( pGetMessage == 0 ){` |
|      ! 0 | 4047 | `		return;` |
|        - | 4048 | `	}` |
|      598 | 4049 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      598 | 4050 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      598 | 4051 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      598 | 4052 | `		if( zTmp && nTmp > 0 ){` |
|      598 | 4053 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      297 | 4054 | `		}` |
|      297 | 4055 | `	}` |
|      598 | 4056 | `	PH7_MemObjRelease(&sMsg);` |
|      301 | 4057 | `}` |
|        - | 4058 | `/*` |
|        - | 4059 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|        - | 4060 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|        - | 4061 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|        - | 4062 | ` * outermost (the actually-uncaught) exception.` |
|        - | 4063 | ` *` |
|        - | 4064 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|        - | 4065 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|        - | 4066 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|        - | 4067 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|        - | 4068 | ` */` |
|        - | 4069 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|      588 | 4070 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|        4 | 4071 | `{` |
|        - | 4072 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|      592 | 4073 | `	int nChain = 0;` |
|        - | 4074 | `	int i;` |
|        - | 4075 | `	SyBlob sOut;` |
|        - | 4076 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|        - | 4077 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|        - | 4078 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|      592 | 4079 | `	pVm->iExitStatus = 255;` |
|      592 | 4080 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 4081 | `		return PH7_OK;` |
|        - | 4082 | `	}` |
|        - | 4083 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|        - | 4084 | `	 * collected) or the hard cap. */` |
|     1186 | 4085 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|      606 | 4086 | `		for( i = 0 ; i < nChain ; ++i ){` |
|       10 | 4087 | `			if( apChain[i] == pThis ){` |
|      ! 0 | 4088 | `				pThis = 0; /* cycle: stop the walk */` |
|      ! 0 | 4089 | `				break;` |
|        - | 4090 | `			}` |
|        6 | 4091 | `		}` |
|      598 | 4092 | `		if( pThis == 0 ){` |
|      ! 0 | 4093 | `			break;` |
|        - | 4094 | `		}` |
|      598 | 4095 | `		apChain[nChain++] = pThis;` |
|      598 | 4096 | `		pThis = VmExceptionGetPrevious(pThis);` |
|        4 | 4097 | `	}` |
|      592 | 4098 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        - | 4099 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|        - | 4100 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|     1186 | 4101 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|      598 | 4102 | `		ph7_class_instance *pEnt = apChain[i];` |
|        - | 4103 | `		SyBlob sMsg;` |
|      598 | 4104 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      598 | 4105 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|      895 | 4106 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|      594 | 4107 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|      594 | 4108 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|      297 | 4109 | `			zFuncName,nFuncLen,` |
|      594 | 4110 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|      297 | 4111 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|      297 | 4112 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|      598 | 4113 | `		SyBlobRelease(&sMsg);` |
|      301 | 4114 | `	}` |
|      592 | 4115 | `	VmCallErrorHandler(pVm,&sOut);` |
|      592 | 4116 | `	SyBlobRelease(&sOut);` |
|      592 | 4117 | `	return PH7_ABORT;` |
|      298 | 4118 | `}` |
|        - | 4119 | `/*` |
|        - | 4120 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - | 4121 | ` *` |
|        - | 4122 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - | 4123 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - | 4124 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - | 4125 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - | 4126 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - | 4127 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - | 4128 | ` */` |
|  1557467 | 4129 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 | 4130 | `{` |
|  1557472 | 4131 | `	if( pVm->bCoalesceArmed ){` |
|       13 | 4132 | `		if( pVm->pCoalesceObj ){` |
|       13 | 4133 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        5 | 4134 | `		}` |
|       13 | 4135 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       13 | 4136 | `		pVm->pCoalesceObj = 0;` |
|       13 | 4137 | `		pVm->bCoalesceArmed = 0;` |
|        5 | 4138 | `	}` |
|  1557472 | 4139 | `}` |
|        - | 4140 | `/*` |
|        - | 4141 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - | 4142 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - | 4143 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - | 4144 | ` * build the SyBlob themselves and pass its data + length.` |
|        - | 4145 | ` *` |
|        - | 4146 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - | 4147 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - | 4148 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - | 4149 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - | 4150 | ` */` |
|   141070 | 4151 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|        - | 4152 | `	ph7_vm *pVm,` |
|        - | 4153 | `	const char *zClass,` |
|        - | 4154 | `	const char *zMsg,` |
|        - | 4155 | `	sxu32 nMsg` |
|        5 | 4156 | `){` |
|        - | 4157 | `	ph7_class *pClass;` |
|        - | 4158 | `	ph7_class_instance *pThis;` |
|        - | 4159 | `	ph7_class_method *pCons;` |
|        - | 4160 | `	VmFrame *pFrame;` |
|        - | 4161 | `	sxi32 rc;` |
|   141075 | 4162 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|   141075 | 4163 | `	if( pClass == 0 ){` |
|      ! 0 | 4164 | `		return SXERR_ABORT;` |
|        - | 4165 | `	}` |
|   141075 | 4166 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|   141075 | 4167 | `	if( pThis == 0 ){` |
|      ! 0 | 4168 | `		return SXERR_ABORT;` |
|        - | 4169 | `	}` |
|   141075 | 4170 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   141075 | 4171 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|        - | 4172 | `		ph7_value sArg;` |
|        - | 4173 | `		ph7_value *apArg[1];` |
|        - | 4174 | `		SyString sMsgStr;` |
|   141075 | 4175 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|   141075 | 4176 | `		PH7_MemObjInit(pVm,&sArg);` |
|   141075 | 4177 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|   141075 | 4178 | `		apArg[0] = &sArg;` |
|   141075 | 4179 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|   141075 | 4180 | `		PH7_MemObjRelease(&sArg);` |
|   141075 | 4181 | `		pVm->nExcCtorDepth--;` |
|    70535 | 4182 | `	}` |
|   141075 | 4183 | `	pFrame = pVm->pFrame;` |
|   141075 | 4184 | `	if( pFrame ){` |
|   141075 | 4185 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   141075 | 4186 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    70535 | 4187 | `	}` |
|   141075 | 4188 | `	rc = VmThrowException(pVm,pThis);` |
|   141075 | 4189 | `	PH7_ClassInstanceUnref(pThis);` |
|   141075 | 4190 | `	return rc;` |
|    70540 | 4191 | `}` |
|        - | 4192 | `/*` |
|        - | 4193 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|        - | 4194 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|        - | 4195 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|        - | 4196 | ` *` |
|        - | 4197 | ` *   int/float/bool/null      arithmetic proceeds` |
|        - | 4198 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|        - | 4199 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|        - | 4200 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|        - | 4201 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|        - | 4202 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|        - | 4203 | ` *   object/resource          TypeError, naming the object's CLASS` |
|        - | 4204 | ` *` |
|        - | 4205 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|        - | 4206 | ` */` |
|        - | 4207 | `/*` |
|        - | 4208 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|        - | 4209 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|        - | 4210 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|        - | 4211 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|        - | 4212 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|        - | 4213 | ` * to depth 1).` |
|        - | 4214 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|        - | 4215 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|        - | 4216 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|        - | 4217 | ` * frame shape is file/line/function[/class/type], matching the default` |
|        - | 4218 | ` * zend.exception_ignore_args=On.` |
|        - | 4219 | ` */` |
|  1457387 | 4220 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|        5 | 4221 | `{` |
|        - | 4222 | `	SyString *pFile;` |
|        - | 4223 | `	VmFrame *pFrame;` |
|        - | 4224 | `	ph7_value *pValue;` |
|  1457392 | 4225 | `	sxi32 nDone = 0;` |
|  1457392 | 4226 | `	pValue = ph7_new_scalar(&(*pVm));` |
|  1457392 | 4227 | `	if( pValue == 0 ){` |
|      ! 0 | 4228 | `		return;` |
|        - | 4229 | `	}` |
|  1457392 | 4230 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  1457392 | 4231 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  2090132 | 4232 | `	while( pFrame ){` |
|        - | 4233 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|        - | 4234 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|        - | 4235 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|        - | 4236 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
|  2090132 | 4237 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|       39 | 4238 | `			break;` |
|        - | 4239 | `		}` |
|  2090094 | 4240 | `		nDone++;` |
|  2090094 | 4241 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        - | 4242 | `		ph7_value *pEntry;` |
|  2090094 | 4243 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|        - | 4244 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   728679 | 4245 | `			break;` |
|        - | 4246 | `		}` |
|   632745 | 4247 | `		pEntry = ph7_new_array(&(*pVm));` |
|   632745 | 4248 | `		if( pEntry == 0 ){` |
|      ! 0 | 4249 | `			break;` |
|        - | 4250 | `		}` |
|        - | 4251 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|        - | 4252 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|        - | 4253 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|        - | 4254 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|        - | 4255 | `		 * include-stack top for a call made at global scope. */` |
|        - | 4256 | `		{` |
|   632745 | 4257 | `			SyString *pFrameFile = pFile;` |
|   632745 | 4258 | `			if( pFrame->pParent->pUserData ){` |
|      445 | 4259 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|      445 | 4260 | `				if( pCaller->sFile.nByte > 0 ){` |
|      445 | 4261 | `					pFrameFile = &pCaller->sFile;` |
|      220 | 4262 | `				}` |
|      220 | 4263 | `			}` |
|   632745 | 4264 | `			if( pFrameFile ){` |
|   632745 | 4265 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   632745 | 4266 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   632745 | 4267 | `				ph7_value_reset_string_cursor(pValue);` |
|   316370 | 4268 | `			}` |
|        - | 4269 | `		}` |
|   632745 | 4270 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   632745 | 4271 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|        - | 4272 | `		{` |
|   632745 | 4273 | `			const char *zDisp = 0;` |
|   632745 | 4274 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   632745 | 4275 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|        - | 4276 | `		}` |
|   632745 | 4277 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   632745 | 4278 | `		ph7_value_reset_string_cursor(pValue);` |
|        - | 4279 | `		{` |
|        - | 4280 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|        - | 4281 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|        - | 4282 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|        - | 4283 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|        - | 4284 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|        - | 4285 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|        - | 4286 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   632745 | 4287 | `			SyString *pClsName = 0;` |
|   632745 | 4288 | `			const char *zType = "->";` |
|   632745 | 4289 | `			int bStatic = 0;` |
|   632745 | 4290 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 4291 | `				/* php's separator says what the CALLEE is, not how the caller` |
|        - | 4292 | ``				 * happened to reach it: a static method is `::` even when the`` |
|        - | 4293 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|        - | 4294 | ``				 * instance method), which the pThis test reported as `->`.`` |
|        - | 4295 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|        - | 4296 | `				 * own flags are one cast away. */` |
|   500433 | 4297 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   500433 | 4298 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|   500433 | 4299 | `				zType = bStatic ? "::" : "->";` |
|   382531 | 4300 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|      ! 0 | 4301 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|      ! 0 | 4302 | `			}` |
|   632745 | 4303 | `			if( pClsName ){` |
|   500433 | 4304 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|   500433 | 4305 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|   500433 | 4306 | `				ph7_value_reset_string_cursor(pValue);` |
|   500433 | 4307 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|   500433 | 4308 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|   500433 | 4309 | `				ph7_value_reset_string_cursor(pValue);` |
|   500428 | 4310 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis` |
|       24 | 4311 | `				 && !bStatic ){` |
|       18 | 4312 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       18 | 4313 | `					if( pObjVal ){` |
|       18 | 4314 | `						pFrame->pThis->iRef++;` |
|       18 | 4315 | `						pObjVal->x.pOther = pFrame->pThis;` |
|       18 | 4316 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       18 | 4317 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|       18 | 4318 | `						ph7_release_value(&(*pVm),pObjVal);` |
|        8 | 4319 | `					}` |
|        8 | 4320 | `				}` |
|   250214 | 4321 | `			}` |
|        - | 4322 | `		}` |
|   632745 | 4323 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      116 | 4324 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      116 | 4325 | `			if( pArg ){` |
|        - | 4326 | `				/* The arguments the caller actually PASSED, which is not the same` |
|        - | 4327 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|        - | 4328 | `				 * (shared with func_get_args()). */` |
|      116 | 4329 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg);` |
|      116 | 4330 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      116 | 4331 | `				ph7_release_value(&(*pVm),pArg);` |
|       57 | 4332 | `			}` |
|       57 | 4333 | `		}` |
|   632745 | 4334 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   632745 | 4335 | `		ph7_release_value(&(*pVm),pEntry);` |
|   632745 | 4336 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|        5 | 4337 | `	}` |
|  1457392 | 4338 | `	ph7_release_value(&(*pVm),pValue);` |
|   728698 | 4339 | `}` |
|        - | 4340 | `/*` |
|        - | 4341 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|        - | 4342 | ` *` |
|        - | 4343 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|        - | 4344 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|        - | 4345 | ` * calls parent::__construct still reports the right position. The embedded` |
|        - | 4346 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|        - | 4347 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|        - | 4348 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|        - | 4349 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|        - | 4350 | ` */` |
|  1587215 | 4351 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        5 | 4352 | `{` |
|        - | 4353 | `	static const char *azField[] = { "file", "line", "trace" };` |
|        - | 4354 | `	ph7_class *pThrowable;` |
|        - | 4355 | `	SyString *pFile;` |
|        - | 4356 | `	SyString *pSiteFile;` |
|        - | 4357 | `	sxu32 n;` |
|  1587220 | 4358 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|      ! 0 | 4359 | `		return;` |
|        - | 4360 | `	}` |
|  1587220 | 4361 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|  1587220 | 4362 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   129929 | 4363 | `		return;` |
|        - | 4364 | `	}` |
|  1457296 | 4365 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  1457296 | 4366 | `	pSiteFile = pFile;` |
|        - | 4367 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|        - | 4368 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|        - | 4369 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|        - | 4370 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|        - | 4371 | `	{` |
|  1457296 | 4372 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  1457296 | 4373 | `		if( pInner && pInner->pUserData ){` |
|   628797 | 4374 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|   628797 | 4375 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|   628705 | 4376 | `				pSiteFile = &pInnerFunc->sFile;` |
|   314350 | 4377 | `			}` |
|   314396 | 4378 | `		}` |
|        - | 4379 | `	}` |
|  5829169 | 4380 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|        - | 4381 | `		SyHashEntry *pEntry;` |
|        - | 4382 | `		VmClassAttr *pVmAttr;` |
|        - | 4383 | `		ph7_value *pAttrValue;` |
|  4371878 | 4384 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|  4371878 | 4385 | `		if( pEntry == 0 ){` |
|      ! 0 | 4386 | `			continue;` |
|        - | 4387 | `		}` |
|  4371878 | 4388 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  4371878 | 4389 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  4371878 | 4390 | `		if( pAttrValue == 0 ){` |
|      ! 0 | 4391 | `			continue;` |
|        - | 4392 | `		}` |
|  4371878 | 4393 | `		if( n == 0 ){` |
|  1457296 | 4394 | `			if( pSiteFile ){` |
|  1457296 | 4395 | `				PH7_MemObjRelease(pAttrValue);` |
|  1457296 | 4396 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   728650 | 4397 | `			}` |
|  3643232 | 4398 | `		}else if( n == 1 ){` |
|        - | 4399 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|        - | 4400 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|        - | 4401 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|        - | 4402 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|        - | 4403 | `			 * off everything the initializer calls — an autoloader, a nested` |
|        - | 4404 | `			 * constant's evaluation — which report their own lines in both engines.` |
|        - | 4405 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|        - | 4406 | `			 * PHL already matches.) */` |
|  1457296 | 4407 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|       40 | 4408 | `				? pVm->nLazyInitLine` |
|  1457292 | 4409 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
|  1457296 | 4410 | `			PH7_MemObjRelease(pAttrValue);` |
|  1457296 | 4411 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|   728650 | 4412 | `		}else{` |
|        - | 4413 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|        - | 4414 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|        - | 4415 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|        - | 4416 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|  1457296 | 4417 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|  1457296 | 4418 | `			if( pList == 0 ){` |
|      ! 0 | 4419 | `				continue;` |
|        - | 4420 | `			}` |
|  1457296 | 4421 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,0,pList);` |
|        - | 4422 | `			/* Building the trace reserves new memobjs, which may realloc` |
|        - | 4423 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|        - | 4424 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|        - | 4425 | `			 * AFTER the walk before releasing/storing into it. */` |
|  1457296 | 4426 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  1457296 | 4427 | `			if( pAttrValue ){` |
|  1457296 | 4428 | `				PH7_MemObjRelease(pAttrValue);` |
|  1457296 | 4429 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   728645 | 4430 | `			}` |
|  1457296 | 4431 | `			ph7_release_value(&(*pVm),pList);` |
|        - | 4432 | `		}` |
|  2185940 | 4433 | `	}` |
|   793612 | 4434 | `}` |
|      362 | 4435 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|        3 | 4436 | `{` |
|      365 | 4437 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       33 | 4438 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       33 | 4439 | `		if( pInst && pInst->pClass ){` |
|       33 | 4440 | `			return pInst->pClass->sName.zString;` |
|        - | 4441 | `		}` |
|      ! 0 | 4442 | `	}` |
|      333 | 4443 | `	return ph7_type_name(pVal);` |
|      184 | 4444 | `}` |
|        - | 4445 | `/*` |
|        - | 4446 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|        - | 4447 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|        - | 4448 | ` * describes an operand it could not use as one ("on false", "false given").` |
|        - | 4449 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|        - | 4450 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|        - | 4451 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|        - | 4452 | ` * instead of replacing it.` |
|        - | 4453 | ` */` |
|      104 | 4454 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|        3 | 4455 | `{` |
|      107 | 4456 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|       19 | 4457 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 4458 | `	}` |
|       89 | 4459 | `	return VmArithTypeName(&(*pVal));` |
|       55 | 4460 | `}` |
|        - | 4461 | `/*` |
|        - | 4462 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|        - | 4463 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|        - | 4464 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|        - | 4465 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|        - | 4466 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|        - | 4467 | ` */` |
|       38 | 4468 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|        1 | 4469 | `{` |
|       39 | 4470 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|       39 | 4471 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       17 | 4472 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       17 | 4473 | `		if( pInst && pInst->pClass ){` |
|       17 | 4474 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|       17 | 4475 | `			return;` |
|        - | 4476 | `		}` |
|      ! 0 | 4477 | `	}` |
|       23 | 4478 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|       20 | 4479 | `}` |
|        - | 4480 | `/*` |
|        - | 4481 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|        - | 4482 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|        - | 4483 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|        - | 4484 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|        - | 4485 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|        - | 4486 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|        - | 4487 | ` */` |
|       40 | 4488 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|        1 | 4489 | `{` |
|       41 | 4490 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|       41 | 4491 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|       41 | 4492 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|       41 | 4493 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|       41 | 4494 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|        - | 4495 | `	sxu32 i;` |
|      109 | 4496 | `	for( i = 0 ; i < nMin ; ++i ){` |
|        - | 4497 | `		unsigned char c;` |
|       69 | 4498 | `		if( cOp == '\|' ){` |
|       25 | 4499 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|       57 | 4500 | `		}else if( cOp == '^' ){` |
|       21 | 4501 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|       11 | 4502 | `		}else{` |
|       25 | 4503 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|        - | 4504 | `		}` |
|       69 | 4505 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|       35 | 4506 | `	}` |
|       41 | 4507 | `	if( cOp == '\|' && nL != nR ){` |
|       11 | 4508 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|       11 | 4509 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|       11 | 4510 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|        5 | 4511 | `	}` |
|       41 | 4512 | `}` |
|        - | 4513 | `/*` |
|        - | 4514 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|        - | 4515 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|        - | 4516 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|        - | 4517 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|        - | 4518 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|        - | 4519 | ` * null, which is what an undefined variable answers.` |
|        - | 4520 | ` */` |
|       24 | 4521 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|        1 | 4522 | `{` |
|       25 | 4523 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        9 | 4524 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|        9 | 4525 | `		if( pInst && pInst->pClass ){` |
|        9 | 4526 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|        9 | 4527 | `			return;` |
|        - | 4528 | `		}` |
|      ! 0 | 4529 | `	}` |
|       17 | 4530 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 | 4531 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|        4 | 4532 | `			pVal->x.iVal ? "true" : "false");` |
|       15 | 4533 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|       11 | 4534 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|        6 | 4535 | `	}else{` |
|        3 | 4536 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|        - | 4537 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|        - | 4538 | `	}` |
|       13 | 4539 | `}` |
|        - | 4540 | `/*` |
|        - | 4541 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|        - | 4542 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|        - | 4543 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|        - | 4544 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|        - | 4545 | ` * computes with the prefix. Classify pVal against that contract.` |
|        - | 4546 | ` */` |
|    88768 | 4547 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|        5 | 4548 | `{` |
|        - | 4549 | `	ph7_value sInt;` |
|        - | 4550 | `	sxi32 rc;` |
|    88773 | 4551 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|    88773 | 4552 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|    88773 | 4553 | `	PH7_MemObjRelease(&sInt);` |
|    88773 | 4554 | `	return rc;` |
|        5 | 4555 | `}` |
|   173182 | 4556 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|        5 | 4557 | `{` |
|   173187 | 4558 | `	int bBadL = 0, bBadR = 0;` |
|        - | 4559 | `	int i;` |
|        - | 4560 | `	ph7_value *apOperand[2];` |
|   173187 | 4561 | `	apOperand[0] = pLeft;` |
|   173187 | 4562 | `	apOperand[1] = pRight;` |
|        - | 4563 | `	/* array + array is php's union operator, not arithmetic */` |
|   173182 | 4564 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|    27151 | 4565 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|     5117 | 4566 | `		return SXRET_OK;` |
|        - | 4567 | `	}` |
|   504215 | 4568 | `	for( i = 0 ; i < 2 ; ++i ){` |
|   336145 | 4569 | `		ph7_value *pVal = apOperand[i];` |
|   336145 | 4570 | `		int bBad = 0;` |
|   336145 | 4571 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       87 | 4572 | `			bBad = 1;` |
|   336102 | 4573 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      333 | 4574 | `			const char *zTail = 0;` |
|      333 | 4575 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|      333 | 4576 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|        - | 4577 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|       49 | 4578 | `				bBad = 1;` |
|       25 | 4579 | `			}else{` |
|        - | 4580 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|        - | 4581 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|        - | 4582 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|      313 | 4583 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       30 | 4584 | `					zTail++;` |
|        2 | 4585 | `				}` |
|      285 | 4586 | `				if( zTail < zEnd ){` |
|       44 | 4587 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|       21 | 4588 | `				}` |
|        - | 4589 | `			}` |
|      165 | 4590 | `		}` |
|   336145 | 4591 | `		if( bBad ){` |
|      135 | 4592 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|       67 | 4593 | `		}` |
|   168411 | 4594 | `	}` |
|   168075 | 4595 | `	if( bBadL \|\| bBadR ){` |
|        - | 4596 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|        - | 4597 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|        - | 4598 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|      187 | 4599 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|       62 | 4600 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|      125 | 4601 | `		return SXERR_INVALID;` |
|        - | 4602 | `	}` |
|   167951 | 4603 | `	return SXRET_OK;` |
|    86764 | 4604 | `}` |
|        - | 4605 | `/*` |
|        - | 4606 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - | 4607 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|        - | 4608 | ` * pass 0 for the engine errors that leave it at its default.` |
|        - | 4609 | ` */` |
|     9191 | 4610 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|        5 | 4611 | `{` |
|        - | 4612 | `	ph7_vm *pVm;` |
|        - | 4613 | `	ph7_class *pClass;` |
|        - | 4614 | `	ph7_class_instance *pThis;` |
|        - | 4615 | `	ph7_class_method *pCons;` |
|        - | 4616 | `	ph7_value sArg,sCode;` |
|        - | 4617 | `	ph7_value *apArg[2];` |
|        - | 4618 | `	SyBlob sMsg;` |
|        - | 4619 | `	SyString sMsgStr;` |
|        - | 4620 | `	VmFrame *pFrame;` |
|        - | 4621 | `	sxi32 rc;` |
|        - | 4622 |  |
|     9196 | 4623 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 4624 | `		return PH7_ABORT;` |
|        - | 4625 | `	}` |
|     9196 | 4626 | `	pVm = pCtx->pVm;` |
|     9196 | 4627 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 4628 | `		zClass = "Error";` |
|      ! 0 | 4629 | `	}` |
|        - | 4630 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|        - | 4631 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|        - | 4632 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|        - | 4633 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|        - | 4634 | `	 * reported error (VmHostFuncThrowRc). */` |
|     9196 | 4635 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|     9196 | 4636 | `	if( pClass == 0 ){` |
|      ! 0 | 4637 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 4638 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 | 4639 | `			zClass` |
|        - | 4640 | `			);` |
|      ! 0 | 4641 | `		return pCtx->nThrowRc;` |
|        - | 4642 | `	}` |
|     9196 | 4643 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     9196 | 4644 | `	if( pThis == 0 ){` |
|      ! 0 | 4645 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 4646 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - | 4647 | `			);` |
|      ! 0 | 4648 | `		return pCtx->nThrowRc;` |
|        - | 4649 | `	}` |
|        - | 4650 |  |
|     9196 | 4651 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     9196 | 4652 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - | 4653 |  |
|     9196 | 4654 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     9196 | 4655 | `	if( pCons ){` |
|     9196 | 4656 | `		int nArg = 1;` |
|     9196 | 4657 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     9196 | 4658 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     9196 | 4659 | `		apArg[0] = &sArg;` |
|     9196 | 4660 | `		if( iCode != 0 ){` |
|      422 | 4661 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      422 | 4662 | `			apArg[1] = &sCode;` |
|      422 | 4663 | `			nArg = 2;` |
|      210 | 4664 | `		}` |
|     9196 | 4665 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|     9196 | 4666 | `		if( iCode != 0 ){` |
|      422 | 4667 | `			PH7_MemObjRelease(&sCode);` |
|      210 | 4668 | `		}` |
|     9196 | 4669 | `		PH7_MemObjRelease(&sArg);` |
|     4595 | 4670 | `	}` |
|     9196 | 4671 | `	SyBlobRelease(&sMsg);` |
|        - | 4672 |  |
|     9196 | 4673 | `	pFrame = pVm->pFrame;` |
|     9196 | 4674 | `	if( pFrame ){` |
|     9196 | 4675 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     9196 | 4676 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     4595 | 4677 | `	}` |
|     9196 | 4678 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     9196 | 4679 | `	PH7_ClassInstanceUnref(pThis);` |
|     9196 | 4680 | `	if( rc == SXERR_ABORT ){` |
|      524 | 4681 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      524 | 4682 | `		return PH7_ABORT;` |
|        - | 4683 | `	}` |
|        - | 4684 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|        - | 4685 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|        - | 4686 | `	 * argument-validation helper whose callers have no status channel — would` |
|        - | 4687 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|        - | 4688 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|        - | 4689 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|        - | 4690 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|        - | 4691 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|        - | 4692 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|     8676 | 4693 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|     8676 | 4694 | `	return PH7_EXCEPTION;` |
|     4600 | 4695 | `}` |
|     8771 | 4696 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 | 4697 | `{` |
|        - | 4698 | `	va_list ap;` |
|        - | 4699 | `	sxi32 rc;` |
|     8776 | 4700 | `	va_start(ap,zFormat);` |
|     8776 | 4701 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|     8776 | 4702 | `	va_end(ap);` |
|     8776 | 4703 | `	return rc;` |
|        5 | 4704 | `}` |
|        - | 4705 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      420 | 4706 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|        2 | 4707 | `{` |
|        - | 4708 | `	va_list ap;` |
|        - | 4709 | `	sxi32 rc;` |
|      422 | 4710 | `	va_start(ap,zFormat);` |
|      422 | 4711 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      422 | 4712 | `	va_end(ap);` |
|      422 | 4713 | `	return rc;` |
|        2 | 4714 | `}` |
|        - | 4715 | `/*` |
|        - | 4716 | ` * The status a host function's own throw should have returned. Consulted at the` |
|        - | 4717 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|        - | 4718 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|        - | 4719 | ` * and is the identity when the routine never threw or already reported it.` |
|        - | 4720 | ` *` |
|        - | 4721 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|        - | 4722 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|        - | 4723 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|        - | 4724 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|        - | 4725 | ` */` |
|  2968542 | 4726 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|        5 | 4727 | `{` |
|  2968542 | 4728 | `	if( pCtx->nThrowRc == 0` |
|  1488899 | 4729 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
|  2964409 | 4730 | `		return rc;` |
|        - | 4731 | `	}` |
|     4141 | 4732 | `	return pCtx->nThrowRc;` |
|  1485095 | 4733 | `}` |
|        - | 4734 | `/*` |
|        - | 4735 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - | 4736 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - | 4737 | ` */` |
|      ! 0 | 4738 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 | 4739 | `{` |
|        - | 4740 | `	ph7_vm *pVm;` |
|        - | 4741 | `	SyBlob sMsg;` |
|      ! 0 | 4742 | `	const char *zFuncName = 0;` |
|      ! 0 | 4743 | `	int nFuncLen = 0;` |
|        - | 4744 | `	va_list ap;` |
|        - | 4745 | `	sxi32 rc;` |
|        - | 4746 |  |
|      ! 0 | 4747 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 4748 | `		return PH7_OK;` |
|        - | 4749 | `	}` |
|      ! 0 | 4750 | `	pVm = pCtx->pVm;` |
|      ! 0 | 4751 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 4752 | `		zClass = "Error";` |
|      ! 0 | 4753 | `	}` |
|        - | 4754 |  |
|      ! 0 | 4755 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 4756 |  |
|      ! 0 | 4757 | `	va_start(ap,zFormat);` |
|      ! 0 | 4758 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 | 4759 | `	va_end(ap);` |
|        - | 4760 |  |
|      ! 0 | 4761 | `	if( pCtx->pFunc ){` |
|      ! 0 | 4762 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 | 4763 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 | 4764 | `	}` |
|      ! 0 | 4765 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 | 4766 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 | 4767 | `	}` |
|      ! 0 | 4768 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 | 4769 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 | 4770 | `	SyBlobRelease(&sMsg);` |
|      ! 0 | 4771 | `	return rc;` |
|      ! 0 | 4772 | `}` |
|        - | 4773 | `/*` |
|        - | 4774 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 4775 | ` * exception is triggered.` |
|        - | 4776 | ` */` |
|      590 | 4777 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|        - | 4778 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 4779 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 4780 | `	)` |
|        4 | 4781 | `{` |
|        - | 4782 | `	ph7_value *apArg[2],sArg;` |
|      594 | 4783 | `	int nArg = 1;` |
|        - | 4784 | `	sxi32 rc;` |
|      594 | 4785 | `	if( pVm->nMuteThrow > 0 ){` |
|        - | 4786 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|        - | 4787 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|        - | 4788 | `		 * observable: no exception handler runs, no report is printed and the` |
|        - | 4789 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|        - | 4790 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|      ! 0 | 4791 | `		return SXERR_ABORT;` |
|        - | 4792 | `	}` |
|      594 | 4793 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 4794 | `		/* Nesting limit reached */` |
|      ! 0 | 4795 | `		return SXRET_OK;` |
|        - | 4796 | `	}` |
|        - | 4797 | `	/* Call any exception handler if available */` |
|      594 | 4798 | `	PH7_MemObjInit(pVm,&sArg);` |
|      594 | 4799 | `	if( pThis ){` |
|        - | 4800 | `		/* Load the exception instance */` |
|      594 | 4801 | `		sArg.x.pOther = pThis;` |
|      594 | 4802 | `		pThis->iRef++;` |
|      594 | 4803 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      299 | 4804 | `	}else{` |
|      ! 0 | 4805 | `		nArg = 0;` |
|        - | 4806 | `	}` |
|      594 | 4807 | `	apArg[0] = &sArg;` |
|        - | 4808 | `	/* Call the exception handler if available */` |
|      594 | 4809 | `	pVm->nExceptDepth++;` |
|        - | 4810 | `	{` |
|        - | 4811 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|        - | 4812 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|        - | 4813 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|        - | 4814 | `		ph7_value sRunning;` |
|      594 | 4815 | `		PH7_MemObjInit(pVm,&sRunning);` |
|      594 | 4816 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|      594 | 4817 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|      594 | 4818 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|      594 | 4819 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|      594 | 4820 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|      594 | 4821 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|      295 | 4822 | `		}` |
|      594 | 4823 | `		PH7_MemObjRelease(&sRunning);` |
|        - | 4824 | `	}` |
|      594 | 4825 | `	pVm->nExceptDepth--;` |
|      594 | 4826 | `	if( rc != SXRET_OK ){` |
|        - | 4827 | `		const char *zFuncName;` |
|        - | 4828 | `		int nFuncLen;` |
|      592 | 4829 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        - | 4830 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|      592 | 4831 | `		if( pThis ){` |
|        - | 4832 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|        - | 4833 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|        - | 4834 | `			 * renders byte-identically to the historical single-entry report. */` |
|      592 | 4835 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|      298 | 4836 | `		}else{` |
|        - | 4837 | `			/* No instance (internal report path) — default-class single entry. */` |
|      ! 0 | 4838 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|        - | 4839 | `		}` |
|        - | 4840 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      592 | 4841 | `		rc = SXERR_ABORT;` |
|      294 | 4842 | `	}` |
|      594 | 4843 | `	PH7_MemObjRelease(&sArg);` |
|      594 | 4844 | `	return rc;` |
|      299 | 4845 | `}` |
|        - | 4846 | `/*` |
|        - | 4847 | ` * Throw a user exception.` |
|        - | 4848 | ` *` |
|        - | 4849 | ` * Exception dispatch follows this sequence:` |
|        - | 4850 | ` *` |
|        - | 4851 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 4852 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 4853 | ` *` |
|        - | 4854 | ` * 2. If NO catch matches:` |
|        - | 4855 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 4856 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 4857 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 4858 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 4859 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 4860 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 4861 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 4862 | ` *` |
|        - | 4863 | ` * 3. If a catch DOES match:` |
|        - | 4864 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 4865 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 4866 | ` *       inside the catch body from immediately propagating past our` |
|        - | 4867 | ` *       finally block.` |
|        - | 4868 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 4869 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 4870 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 4871 | ` *       in pPendingException (step 2c).` |
|        - | 4872 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 4873 | ` *    d. Run finally (if present).` |
|        - | 4874 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 4875 | ` *       that handlers are restored and finally has run.` |
|        - | 4876 | ` */` |
|        - | 4877 | `/*` |
|        - | 4878 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|        - | 4879 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|        - | 4880 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|        - | 4881 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|        - | 4882 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|        - | 4883 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|        - | 4884 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|        - | 4885 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|        - | 4886 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|        - | 4887 | ` */` |
|      140 | 4888 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|        5 | 4889 | `{` |
|      153 | 4890 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|       45 | 4891 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       45 | 4892 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|       45 | 4893 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|      ! 0 | 4894 | `			break; /* reached an outer exec's / legacy handler */` |
|        - | 4895 | `		}` |
|       45 | 4896 | `		(void)SySetPop(&pVm->aException);` |
|       45 | 4897 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|       45 | 4898 | `		if( pT->iHasFinally ){` |
|       37 | 4899 | `			*pPc = pT->iFinallyPc;` |
|       37 | 4900 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|       37 | 4901 | `			return 1;` |
|        - | 4902 | `		}` |
|        - | 4903 | `		/* No finally: tear the try's transparent frame down now. */` |
|       11 | 4904 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        6 | 4905 | `			VmLeaveFrame(&(*pVm));` |
|        2 | 4906 | `		}` |
|       11 | 4907 | `		VmExcRelease(&(*pVm),pT);` |
|        3 | 4908 | `	}` |
|      111 | 4909 | `	return 0;` |
|       75 | 4910 | `}` |
|        - | 4911 | `/*` |
|        - | 4912 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|        - | 4913 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|        - | 4914 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|        - | 4915 | ` *` |
|        - | 4916 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|        - | 4917 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|        - | 4918 | ` *    and redirect to the catch body (iHandlerPc).` |
|        - | 4919 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|        - | 4920 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|        - | 4921 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|        - | 4922 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|        - | 4923 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|        - | 4924 | ` */` |
|        - | 4925 | `/*` |
|        - | 4926 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|        - | 4927 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|        - | 4928 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|        - | 4929 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|        - | 4930 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|        - | 4931 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|        - | 4932 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|        - | 4933 | ` * case) is unchanged: no wrapper.` |
|        - | 4934 | ` */` |
|    20252 | 4935 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|        5 | 4936 | `{` |
|    20257 | 4937 | `	VmFrame *pWrap = 0;` |
|        - | 4938 | `	VmFrame *pThrowSite;` |
|        - | 4939 | `	sxi32 rc;` |
|    20257 | 4940 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    20153 | 4941 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 4942 | `	}` |
|      107 | 4943 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|        - | 4944 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|      ! 0 | 4945 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 4946 | `	}` |
|      107 | 4947 | `	pThrowSite = pWrap->pParent;` |
|      107 | 4948 | `	pWrap->pParent = pOwner;` |
|      107 | 4949 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|      107 | 4950 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 4951 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|        - | 4952 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|        - | 4953 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|        - | 4954 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|      107 | 4955 | `	if( pVm->pFrame == pWrap ){` |
|      107 | 4956 | `		VmLeaveFrame(&(*pVm));` |
|       52 | 4957 | `	}` |
|      107 | 4958 | `	pVm->pFrame = pThrowSite;` |
|      107 | 4959 | `	return rc;` |
|    10131 | 4960 | `}` |
|        - | 4961 | `/*` |
|        - | 4962 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|        - | 4963 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|        - | 4964 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|        - | 4965 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|        - | 4966 | ` */` |
|        - | 4967 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|       92 | 4968 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - | 4969 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|        5 | 4970 | `{` |
|       97 | 4971 | `	if( pCatch ){` |
|       81 | 4972 | `		pException->iInCatch = 1;` |
|       81 | 4973 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|       81 | 4974 | `		if( pThis ){ pThis->iRef++; }` |
|       81 | 4975 | `		pException->pInflight = pThis;` |
|       81 | 4976 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       81 | 4977 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|       81 | 4978 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       81 | 4979 | `		return SXRET_OK;` |
|        - | 4980 | `	}` |
|       20 | 4981 | `	if( pException->iHasFinally ){` |
|        - | 4982 | `		VmFinallyAction sAct;` |
|       15 | 4983 | `		SyZero(&sAct,sizeof(sAct));` |
|       15 | 4984 | `		sAct.eKind = PH7_FA_RETHROW;` |
|       15 | 4985 | `		if( pThis ){ pThis->iRef++; }` |
|       15 | 4986 | `		sAct.pExc = pThis;` |
|       15 | 4987 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       15 | 4988 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       15 | 4989 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|       15 | 4990 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       15 | 4991 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       15 | 4992 | `		return SXRET_OK;` |
|        - | 4993 | `	}` |
|        - | 4994 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|        - | 4995 | `	 * flat native stack instead of mutual recursion. */` |
|        6 | 4996 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      ! 0 | 4997 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 4998 | `	}` |
|        6 | 4999 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|        6 | 5000 | `	return VM_THROW_KEEP_UNWINDING;` |
|       51 | 5001 | `}` |
|  1457209 | 5002 | `PH7_PRIVATE sxi32 VmThrowException(` |
|        - | 5003 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 5004 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 5005 | `	)` |
|        5 | 5006 | `{` |
|        - | 5007 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 5008 | `	ph7_exception **apException;` |
|   728604 | 5009 | `	ph7_exception *pException;` |
|    50103 | 5010 | `Rethrow:` |
|        - | 5011 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|        - | 5012 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|        - | 5013 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|        - | 5014 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|        - | 5015 | `	 * so the throw path must be too). */` |
|        - | 5016 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 5017 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 5018 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|  1557420 | 5019 | `	VmCoalesceDisarm(pVm);` |
|        - | 5020 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|        - | 5021 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|        - | 5022 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|        - | 5023 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|        - | 5024 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|        - | 5025 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|        - | 5026 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|        - | 5027 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|  1557415 | 5028 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|       25 | 5029 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|       19 | 5030 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|        8 | 5031 | `	}` |
|        - | 5032 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|        - | 5033 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|        - | 5034 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|        - | 5035 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|        - | 5036 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|        - | 5037 | `	 * that owns the pending return, so it must leave that return intact. */` |
|        - | 5038 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|        - | 5039 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|        - | 5040 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|        - | 5041 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|  1557420 | 5042 | `	pVm->pResumeFrame = 0;` |
|        - | 5043 | `	/* Point to the stack of loaded exceptions */` |
|  1557420 | 5044 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|  1557420 | 5045 | `	pException = 0;` |
|  1557420 | 5046 | `	pCatch = 0;` |
|  1557420 | 5047 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 5048 | `		ph7_exception_block *aCatch;` |
|        - | 5049 | `		ph7_class *pClass;` |
|        - | 5050 | `		SyString *aNames;` |
|        - | 5051 | `		sxu32 nNames;` |
|        - | 5052 | `		int matched;` |
|        - | 5053 | `		sxu32 j,k;` |
|        - | 5054 | `		/* Locate the appropriate block to execute */` |
|  1456710 | 5055 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|  1456710 | 5056 | `		(void)SySetPop(&pVm->aException);` |
|  1456710 | 5057 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|        - | 5058 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|        - | 5059 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|        - | 5060 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|  1456730 | 5061 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 5062 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|  1436540 | 5063 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|  1436540 | 5064 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|  1436540 | 5065 | `			matched = 0;` |
|  1436586 | 5066 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 5067 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 5068 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 5069 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|  1436566 | 5070 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|  1436566 | 5071 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 5072 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 5073 | `					continue;` |
|        - | 5074 | `				}` |
|  1436566 | 5075 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|  1436520 | 5076 | `					matched = 1;` |
|  1436520 | 5077 | `					break;` |
|        - | 5078 | `				}` |
|       27 | 5079 | `			}` |
|  1436540 | 5080 | `			if( matched ){` |
|        - | 5081 | `				/* Catch block found,break immediately */` |
|  1436520 | 5082 | `				pCatch = &aCatch[j];` |
|  1436520 | 5083 | `				break;` |
|        - | 5084 | `			}` |
|       13 | 5085 | `		}` |
|   728352 | 5086 | `	}` |
|        - | 5087 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|        - | 5088 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|        - | 5089 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|        - | 5090 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|        - | 5091 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|        - | 5092 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|        - | 5093 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|  1557420 | 5094 | `	if( pException ){` |
|  1456710 | 5095 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   728352 | 5096 | `	}` |
|        - | 5097 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|        - | 5098 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|        - | 5099 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|  1557420 | 5100 | `	if( pException && pException->iInlined ){` |
|       97 | 5101 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|       97 | 5102 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|        - | 5103 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|        6 | 5104 | `			goto Rethrow;` |
|        - | 5105 | `		}` |
|       93 | 5106 | `		return rcInline;` |
|        - | 5107 | `	}` |
|        - | 5108 | `	/* Execute the cached block if available */` |
|  1557328 | 5109 | `	if( pCatch == 0 ){` |
|        - | 5110 | `		sxi32 rc;` |
|        - | 5111 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|   120889 | 5112 | `		if( pException && pException->iHasFinally ){` |
|    20173 | 5113 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    20173 | 5114 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    20173 | 5115 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|    20173 | 5116 | `			pException->iFinallyDone = 1;` |
|        - | 5117 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|        - | 5118 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    20173 | 5119 | `			pVm->pInflightException = pThis;` |
|    20173 | 5120 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 5121 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|        - | 5122 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    20173 | 5123 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    20173 | 5124 | `			pVm->pInflightException = pSaveInflight;` |
|    20173 | 5125 | `			pVm->nInflightExcBase = nSaveBase;` |
|    20173 | 5126 | `			if( rc == SXERR_ABORT ){` |
|        3 | 5127 | `				VmExcRelease(&(*pVm),pException);` |
|        3 | 5128 | `				return SXERR_ABORT;` |
|        - | 5129 | `			}` |
|        - | 5130 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|        - | 5131 | `			 * semantics). The finally stored it on the body frame it returns from` |
|        - | 5132 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|        - | 5133 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|        - | 5134 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|        - | 5135 | `			 * takes the value instead of unwinding) and resume in place.` |
|        - | 5136 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|        - | 5137 | `			 * the same transport an in-place catch uses — and unwind as an` |
|        - | 5138 | `			 * exception; the owner's activation consumes the resume` |
|        - | 5139 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|        - | 5140 | `			 * its bHasRet tail materializes the return. */` |
|        - | 5141 | `			{` |
|    20171 | 5142 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    20171 | 5143 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    20171 | 5144 | `				if( pOwnerFrame->bHasRet ){` |
|    20029 | 5145 | `					if( pOwnerFrame == pThrowFrame ){` |
|    20026 | 5146 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|        - | 5147 | `						/* Record the landing pad like the cross-frame case below.` |
|        - | 5148 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|        - | 5149 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|        - | 5150 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|        - | 5151 | `						 * its router unwound as an exception and the Unwind discard` |
|        - | 5152 | ``						 * dropped the parked return — `function f(){ try {`` |
|        - | 5153 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|        - | 5154 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|        - | 5155 | `						 * try frame down and its bHasRet tail materializes the` |
|        - | 5156 | `						 * return, same as the in-place-catch landing. */` |
|    20026 | 5157 | `						pVm->pResumeFrame = pOwnerFrame;` |
|    20026 | 5158 | `						pVm->iResumePc = pException->iLandingPc;` |
|    20026 | 5159 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|    20026 | 5160 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|    20026 | 5161 | `						VmExcRelease(&(*pVm),pException);` |
|    20026 | 5162 | `						return SXRET_OK;` |
|        - | 5163 | `					}` |
|        3 | 5164 | `					pVm->pResumeFrame = pOwnerFrame;` |
|        3 | 5165 | `					pVm->iResumePc = pException->iLandingPc;` |
|        3 | 5166 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|        3 | 5167 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|        3 | 5168 | `					VmExcRelease(&(*pVm),pException);` |
|        3 | 5169 | `					return PH7_EXCEPTION;` |
|        - | 5170 | `				}` |
|        - | 5171 | `			}` |
|        - | 5172 | `			/* The finally threw an exception that superseded pThis — it either` |
|        - | 5173 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|        - | 5174 | `			 * (which consumed an entry from the exception stack). Either way the` |
|        - | 5175 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|        - | 5176 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|      145 | 5177 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       17 | 5178 | `				VmExcRelease(&(*pVm),pException);` |
|       17 | 5179 | `				return PH7_EXCEPTION;` |
|        - | 5180 | `			}` |
|       63 | 5181 | `		}` |
|        - | 5182 | `		/* Check if there is an outer exception handler on the stack */` |
|   100847 | 5183 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 5184 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|        - | 5185 | `			 * iteration per unwound level instead of one native frame. */` |
|      137 | 5186 | `			VmExcRelease(&(*pVm),pException);` |
|      137 | 5187 | `			goto Rethrow;` |
|        - | 5188 | `		}` |
|   100715 | 5189 | `		if( pVm->nMuteThrow > 0 ){` |
|        - | 5190 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|        - | 5191 | `			 * default at class mount, which php would not have evaluated yet).` |
|        - | 5192 | `			 * Nothing outside the initializer may observe this throw: no` |
|        - | 5193 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|        - | 5194 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|        - | 5195 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|        - | 5196 | `			 * the mount path rolls the whole attempt back. */` |
|       50 | 5197 | `			VmExcRelease(&(*pVm),pException);` |
|       50 | 5198 | `			return SXERR_ABORT;` |
|        - | 5199 | `		}` |
|        - | 5200 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 5201 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 5202 | `		 * exception instead of reporting it uncaught.` |
|        - | 5203 | `		 */` |
|   100669 | 5204 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 5205 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 5206 | `			 * by looking for a catch frame on the stack.` |
|        - | 5207 | `			 */` |
|   100669 | 5208 | `			VmFrame *pF = pVm->pFrame;` |
|   100669 | 5209 | `			int inCatch = 0;` |
|   101305 | 5210 | `			while( pF ){` |
|   100715 | 5211 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|   100078 | 5212 | `					inCatch = 1;` |
|   100078 | 5213 | `					break;` |
|        - | 5214 | `				}` |
|      640 | 5215 | `				pF = pF->pParent;` |
|        4 | 5216 | `			}` |
|   100669 | 5217 | `			if( inCatch ){` |
|        - | 5218 | `				/* Defer — will be re-thrown after finally runs */` |
|   100078 | 5219 | `				pThis->iRef++;` |
|   100078 | 5220 | `				pVm->pPendingException = pThis;` |
|   100078 | 5221 | `				VmExcRelease(&(*pVm),pException);` |
|   100078 | 5222 | `				return SXRET_OK;` |
|        - | 5223 | `			}` |
|      295 | 5224 | `		}` |
|        - | 5225 | `		/* Truly uncaught */` |
|      594 | 5226 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      594 | 5227 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 5228 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 5229 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 5230 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 5231 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 5232 | `			}` |
|      ! 0 | 5233 | `		}` |
|      594 | 5234 | `		VmExcRelease(&(*pVm),pException);` |
|      594 | 5235 | `		return rc;` |
|      ! 0 | 5236 | `	}else{` |
|  1436444 | 5237 | `		VmFrame *pFrame = pVm->pFrame;` |
|  1436444 | 5238 | `		ph7_exception **apSaved = 0;` |
|        - | 5239 | `		sxu32 nSavedCount;` |
|        - | 5240 | `		sxi32 rc;` |
|        - | 5241 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|        - | 5242 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|        - | 5243 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|        - | 5244 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|        - | 5245 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|  1436444 | 5246 | `		VmFrame *pCatchBody = pException->pFrame;` |
|  1436444 | 5247 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|  1436444 | 5248 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|  1436444 | 5249 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  1436444 | 5250 | `		if( pException->pFrame == pFrame ){` |
|   828236 | 5251 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|   414115 | 5252 | `		}` |
|        - | 5253 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 5254 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 5255 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 5256 | `		 */` |
|  1436444 | 5257 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|  1436444 | 5258 | `		if( nSavedCount > 0 ){` |
|   150305 | 5259 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    50100 | 5260 | `				nSavedCount * sizeof(ph7_exception *));` |
|   100205 | 5261 | `			if( apSaved ){` |
|   150305 | 5262 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|    50100 | 5263 | `					nSavedCount * sizeof(ph7_exception *));` |
|   100205 | 5264 | `				SySetReset(&pVm->aException);` |
|    50100 | 5265 | `			}` |
|    50100 | 5266 | `		}` |
|        - | 5267 | `		/* Create the catch frame (made transparent below) */` |
|  1436444 | 5268 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|  1436444 | 5269 | `		if( rc == SXRET_OK ){` |
|        - | 5270 | `			ph7_value *pObj;` |
|        - | 5271 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|        - | 5272 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|        - | 5273 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|        - | 5274 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|        - | 5275 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|        - | 5276 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|        - | 5277 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|        - | 5278 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|        - | 5279 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|  1436444 | 5280 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|        - | 5281 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|        - | 5282 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|        - | 5283 | `			 * against the live current scope rather than a freed frame. */` |
|  1436444 | 5284 | `			if( pCatchBody ){` |
|  1436444 | 5285 | `				pFrame->pParent = pCatchBody;` |
|   718219 | 5286 | `			}` |
|        - | 5287 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 5288 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 5289 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 5290 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 5291 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 5292 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 5293 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 5294 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|  1436444 | 5295 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|        - | 5296 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|  2154664 | 5297 | `			pObj = (pCatch->sThis.nByte > 0)` |
|  1436437 | 5298 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|  1436444 | 5299 | `			if( pObj ){` |
|        - | 5300 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 5301 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 5302 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 5303 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 5304 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 5305 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|  1436440 | 5306 | `				pThis->iRef++;` |
|  1436440 | 5307 | `				PH7_MemObjRelease(pObj);` |
|  1436440 | 5308 | `				pObj->x.pOther = pThis;` |
|  1436440 | 5309 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   718217 | 5310 | `			}` |
|        - | 5311 | `			/* Execute the catch block */` |
|  1436444 | 5312 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|        - | 5313 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|        - | 5314 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|        - | 5315 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|        - | 5316 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|        - | 5317 | `			 * unbalanced — never pop somebody else's frame. */` |
|  1436444 | 5318 | `			if( pVm->pFrame == pFrame ){` |
|  1436444 | 5319 | `				VmLeaveFrame(&(*pVm));` |
|   718219 | 5320 | `			}` |
|  1436444 | 5321 | `			pVm->pFrame = pThrowSite;` |
|   718219 | 5322 | `		}` |
|        - | 5323 | `		/* Restore the outer exception handlers */` |
|  1436444 | 5324 | `		if( apSaved ){` |
|        - | 5325 | `			sxu32 k;` |
|        - | 5326 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|        - | 5327 | `			 * the catch body) are normally already consumed; on an abnormal` |
|        - | 5328 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|        - | 5329 | `			 * linger — release those activations before discarding the set. */` |
|   100205 | 5330 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|   100205 | 5331 | `			SySetReset(&pVm->aException);` |
|   201057 | 5332 | `			for(k = 0; k < nSavedCount; k++){` |
|   100857 | 5333 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    50431 | 5334 | `			}` |
|   100205 | 5335 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|    50100 | 5336 | `		}` |
|        - | 5337 | `		/* Execute the finally block after catch */` |
|  1436444 | 5338 | `		if( pException->iHasFinally ){` |
|        - | 5339 | `			sxi32 rcf;` |
|        - | 5340 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|        - | 5341 | `			 * from, its pending-return write generation (set if the catch above` |
|        - | 5342 | `			 * returned), and the exception-stack depth. After the finally we use` |
|        - | 5343 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|        - | 5344 | `			 * catch-return. */` |
|        - | 5345 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|        - | 5346 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|        - | 5347 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|        - | 5348 | `			 * supersede decision belong to the owner, not the thrower. */` |
|       89 | 5349 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|       89 | 5350 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|       89 | 5351 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|        - | 5352 | `			/* The exception in flight while this finally runs is the catch body's` |
|        - | 5353 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|        - | 5354 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|        - | 5355 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|        - | 5356 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|       89 | 5357 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|       89 | 5358 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|       89 | 5359 | `			pException->iFinallyDone = 1;` |
|       89 | 5360 | `			pVm->pInflightException = pVm->pPendingException;` |
|       89 | 5361 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 5362 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|       89 | 5363 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|       89 | 5364 | `			pVm->pInflightException = pSaveInflight;` |
|       89 | 5365 | `			pVm->nInflightExcBase = nSaveBase;` |
|       89 | 5366 | `			if( rcf == SXERR_ABORT ){` |
|      ! 0 | 5367 | `				VmExcRelease(&(*pVm),pException);` |
|      ! 0 | 5368 | `				return SXERR_ABORT;` |
|        - | 5369 | `			}` |
|        - | 5370 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|        - | 5371 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|        - | 5372 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|        - | 5373 | `			 * either case that exception supersedes this try's catch-return — but` |
|        - | 5374 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|        - | 5375 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|        - | 5376 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|       89 | 5377 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       19 | 5378 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|       12 | 5379 | `					VmClearFramePending(pBody);` |
|        5 | 5380 | `				}` |
|        - | 5381 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|        - | 5382 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|        - | 5383 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|        - | 5384 | `				 * legitimately have re-armed it during the finally. */` |
|       19 | 5385 | `				pBody->nCatchJmpPc = 0;` |
|        8 | 5386 | `			}` |
|       89 | 5387 | `			if( rcf == PH7_EXCEPTION ){` |
|        - | 5388 | `				/* The finally's exception propagated past this try; drop any deferred` |
|        - | 5389 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|        - | 5390 | `				 * reaches the frame that caught the finally's throw. */` |
|       19 | 5391 | `				if( pVm->pPendingException ){` |
|      ! 0 | 5392 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      ! 0 | 5393 | `					pVm->pPendingException = 0;` |
|      ! 0 | 5394 | `				}` |
|       19 | 5395 | `				VmExcRelease(&(*pVm),pException);` |
|       19 | 5396 | `				return PH7_EXCEPTION;` |
|        - | 5397 | `			}` |
|       34 | 5398 | `		}` |
|  1436428 | 5399 | `		if( rc == SXERR_ABORT ){` |
|        5 | 5400 | `			VmExcRelease(&(*pVm),pException);` |
|        5 | 5401 | `			return SXERR_ABORT;` |
|        - | 5402 | `		}` |
|        - | 5403 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 5404 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 5405 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 5406 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|        - | 5407 | `		 * the catch frame having been left above), which swallows the in-flight` |
|        - | 5408 | `		 * exception (PHP semantics).` |
|        - | 5409 | `		 */` |
|  1436424 | 5410 | `		if( pVm->pPendingException ){` |
|        - | 5411 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|   100078 | 5412 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|   100078 | 5413 | `			if( !pOwner->bHasRet ){` |
|   100074 | 5414 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        - | 5415 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|        - | 5416 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|        - | 5417 | `				 * than leave it armed for an unrelated later landing. */` |
|   100074 | 5418 | `				pOwner->nCatchJmpPc = 0;` |
|   100074 | 5419 | `				pVm->pPendingException = 0;` |
|   100074 | 5420 | `				VmExcRelease(&(*pVm),pException);` |
|        - | 5421 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|   100074 | 5422 | `				pThis = pReThrow;` |
|   100074 | 5423 | `				goto Rethrow;` |
|        - | 5424 | `			}` |
|        - | 5425 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|        6 | 5426 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        6 | 5427 | `			pVm->pPendingException = 0;` |
|        2 | 5428 | `		}` |
|        - | 5429 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|        - | 5430 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|        - | 5431 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|        - | 5432 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|  1336354 | 5433 | `		pVm->pResumeFrame = pCatchBody;` |
|  1336354 | 5434 | `		pVm->iResumePc = iCatchPc;` |
|  1336354 | 5435 | `		pVm->pResumeInstr = pCatchInstr;` |
|  1336354 | 5436 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|        - | 5437 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|        - | 5438 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|        - | 5439 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|  1336354 | 5440 | `		VmExcRelease(&(*pVm),pException);` |
|        - | 5441 | `	}` |
|  1336354 | 5442 | `	return SXRET_OK;` |
|   728609 | 5443 | `}` |
|        - | 5444 |  |
