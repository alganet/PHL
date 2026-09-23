# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2412/2704 lines (89.20%)

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
|   24016 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|       5 |   25 | `{` |
|   24021 |   26 | `	pVm->nLastErrType = iErr;` |
|   24021 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|   24021 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|   24021 |   29 | `	if( zMsg && nMsg > 0 ){` |
|   24021 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   12008 |   31 | `	}` |
|   24021 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|   24021 |   33 | `	if( pFile ){` |
|   24021 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   12008 |   35 | `	}` |
|   24021 |   36 | `}` |
|       - |   37 | `/*` |
|       - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|       - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|       - |   40 | ` * diagnostic is never silently swallowed.` |
|       - |   41 | ` */` |
|     754 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|       4 |   43 | `{` |
|     758 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|       4 |   45 | `}` |
|       - |   46 | `/*` |
|       - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|       - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|       - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|       - |   50 | ` * headers_sent()/output accounting).` |
|       - |   51 | ` */` |
|     780 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|       4 |   53 | `{` |
|       - |   54 | `	sxi32 rc;` |
|       - |   55 | `	/* Append a new line */` |
|       - |   56 | `#ifdef __WINNT__` |
|       4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|       - |   58 | `#else` |
|     780 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|       - |   60 | `#endif` |
|       - |   61 | `	/* Invoke the output consumer callback */` |
|     784 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     784 |   63 | `	if( bTrack ){` |
|      29 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      13 |   65 | `	}` |
|     784 |   66 | `	return rc;` |
|       4 |   67 | `}` |
|       - |   68 | `/*` |
|       - |   69 | ` * Route an already-formatted diagnostic blob (the uncaught-exception path builds` |
|       - |   70 | `` * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream`` |
|       - |   71 | ` * when log_errors is on, else to the program-output stream when display_errors` |
|       - |   72 | ` * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).` |
|       - |   73 | ` */` |
|     580 |   74 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|       4 |   75 | `{` |
|     584 |   76 | `	if( pVm->bLogErrors ){` |
|     584 |   77 | `		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);` |
|       - |   78 | `	}` |
|     ! 0 |   79 | `	if( pVm->bDisplayErrors ){` |
|     ! 0 |   80 | `		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);` |
|       - |   81 | `	}` |
|     ! 0 |   82 | `	return SXRET_OK;` |
|     294 |   83 | `}` |
|       - |   84 | `/*` |
|       - |   85 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |   86 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|       - |   87 | ` * information.` |
|       - |   88 | ` */` |
|   24972 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|       5 |   90 | `{` |
|   24977 |   91 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|       - |   92 | `		ph7_value apArg[4];` |
|       - |   93 | `		ph7_value *apArgPtr[4];` |
|       - |   94 | `		ph7_value sResult;` |
|       - |   95 | `		SyString sErr;` |
|       - |   96 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|       - |   97 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|       - |   98 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|       - |   99 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|       - |  100 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|     965 |  101 | `		if( iErr == PH7_CTX_NOTICE ){` |
|     105 |  102 | `			iErr = 8; /* E_NOTICE */` |
|      51 |  103 | `		}` |
|       - |  104 | `		/* Prepare arguments */` |
|     965 |  105 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|       - |  106 | `			/* use explicit message length to avoid reading past buffer */` |
|     965 |  107 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     965 |  108 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     965 |  109 | `		if( pFile ){` |
|     965 |  110 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     965 |  111 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     485 |  112 | `		}else{` |
|     ! 0 |  113 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|       - |  114 | `		}` |
|     965 |  115 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     965 |  116 | `		PH7_MemObjInit(pVm,&sResult);` |
|       - |  117 | `		/* Set up pointer array */` |
|     965 |  118 | `		apArgPtr[0] = &apArg[0];` |
|     965 |  119 | `		apArgPtr[1] = &apArg[1];` |
|     965 |  120 | `		apArgPtr[2] = &apArg[2];` |
|     965 |  121 | `		apArgPtr[3] = &apArg[3];` |
|       - |  122 | `		/* Call the handler */` |
|       - |  123 | `		{` |
|     965 |  124 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|     965 |  125 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|       - |  126 | `				/* The handler threw (or aborted) instead of returning: php never` |
|       - |  127 | `				 * reports the original diagnostic then — the exception supersedes` |
|       - |  128 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|       - |  129 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|       - |  130 | `				 * user's catch already ran. */` |
|       3 |  131 | `				PH7_MemObjRelease(&apArg[0]);` |
|       3 |  132 | `				PH7_MemObjRelease(&apArg[1]);` |
|       3 |  133 | `				PH7_MemObjRelease(&apArg[2]);` |
|       3 |  134 | `				PH7_MemObjRelease(&apArg[3]);` |
|       3 |  135 | `				PH7_MemObjRelease(&sResult);` |
|       3 |  136 | `				return FALSE;` |
|       - |  137 | `			}` |
|       - |  138 | `		}` |
|       - |  139 | `		/* Check return value */` |
|     963 |  140 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 |  141 | `			PH7_MemObjToBool(&sResult);` |
|     ! 0 |  142 | `		}` |
|       - |  143 | `		/* Release */` |
|     963 |  144 | `		PH7_MemObjRelease(&apArg[0]);` |
|     963 |  145 | `		PH7_MemObjRelease(&apArg[1]);` |
|     963 |  146 | `		PH7_MemObjRelease(&apArg[2]);` |
|     963 |  147 | `		PH7_MemObjRelease(&apArg[3]);` |
|     963 |  148 | `		PH7_MemObjRelease(&sResult);` |
|       - |  149 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|       - |  150 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     963 |  151 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|       - |  152 | `	}` |
|       - |  153 | `	/* No handler, always call error handler */` |
|   24017 |  154 | `	return TRUE;` |
|   12491 |  155 | `}` |
|       - |  156 | `/*` |
|       - |  157 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|       - |  158 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|       - |  159 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|       - |  160 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|       - |  161 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|       - |  162 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|       - |  163 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|       - |  164 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|       - |  165 | ` * only picks the DISPLAY label.` |
|       - |  166 | ` */` |
|       - |  167 | `/*` |
|       - |  168 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|       - |  169 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|       - |  170 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|       - |  171 | ` * every deprecation.` |
|       - |  172 | ` */` |
|   24016 |  173 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|       5 |  174 | `{` |
|       - |  175 | `	sxi32 iBit;` |
|   24021 |  176 | `	if( !pVm->bErrReport ){` |
|    4131 |  177 | `		return 0;` |
|       - |  178 | `	}` |
|   19893 |  179 | `	switch( iErr ){` |
|    9912 |  180 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|   19829 |  181 | `		iBit = 2; break;` |
|       3 |  182 | `	case 512  /* E_USER_WARNING */:` |
|       8 |  183 | `		iBit = 512; break;` |
|      13 |  184 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|       - |  185 | `	case 8    /* E_NOTICE */:` |
|      28 |  186 | `		iBit = 8; break;` |
|       4 |  187 | `	case 1024 /* E_USER_NOTICE */:` |
|      11 |  188 | `		iBit = 1024; break;` |
|     ! 0 |  189 | `	case 8192 /* E_DEPRECATED */:` |
|     ! 0 |  190 | `		iBit = 8192; break;` |
|     ! 0 |  191 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  192 | `		iBit = 16384; break;` |
|     ! 0 |  193 | `	case 256  /* E_USER_ERROR */:` |
|     ! 0 |  194 | `		iBit = 256; break;` |
|      12 |  195 | `	default:` |
|      28 |  196 | `		iBit = 1; /* E_ERROR and everything else fatal-ish */` |
|      24 |  197 | `		break;` |
|       - |  198 | `	}` |
|   19893 |  199 | `	return (pVm->iErrMask & iBit) != 0;` |
|   12013 |  200 | `}` |
|     200 |  201 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|       4 |  202 | `{` |
|     204 |  203 | `	switch(iErr){` |
|      71 |  204 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|       - |  205 | `	case 512  /* E_USER_WARNING */:` |
|     146 |  206 | `		return "Warning";` |
|      17 |  207 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|       - |  208 | `	case 8    /* E_NOTICE */:` |
|       - |  209 | `	case 1024 /* E_USER_NOTICE */:` |
|      38 |  210 | `		return "Notice";` |
|     ! 0 |  211 | `	case 8192  /* E_DEPRECATED */:` |
|       - |  212 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  213 | `		return "Deprecated";` |
|     ! 0 |  214 | `	case 256 /* E_USER_ERROR */:` |
|     ! 0 |  215 | `		return "Fatal error";` |
|      12 |  216 | `	default:` |
|      28 |  217 | `		return "Error";` |
|       - |  218 | `	}` |
|     104 |  219 | `}` |
|       - |  220 | `/*` |
|       - |  221 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|       - |  222 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|       - |  223 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|       - |  224 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|       - |  225 | ` * be authored cross-engine with --EXPECTF--.` |
|       - |  226 | ` */` |
|      26 |  227 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|       3 |  228 | `{` |
|      29 |  229 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|      29 |  230 | `}` |
|     200 |  231 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|       4 |  232 | `{` |
|     204 |  233 | `	if( pFile ){` |
|     304 |  234 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|     100 |  235 | `			nLine ? nLine : 1);` |
|     100 |  236 | `	}` |
|     204 |  237 | `}` |
|       - |  238 | `/*` |
|       - |  239 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|       - |  240 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|       - |  241 | ` * (compile.c) and stock CLI's stderr log copy.` |
|       - |  242 | ` */` |
|     174 |  243 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|       4 |  244 | `{` |
|     178 |  245 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|     178 |  246 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|     178 |  247 | `}` |
|       - |  248 | `/*` |
|       - |  249 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|       - |  250 | ` *` |
|       - |  251 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|       - |  252 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|       - |  253 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|       - |  254 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|       - |  255 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|       - |  256 | ` * php never produces. Building it into the message here is the single place that` |
|       - |  257 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|       - |  258 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|       - |  259 | ` */` |
|   24628 |  260 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|       5 |  261 | `{` |
|   24633 |  262 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     294 |  263 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     294 |  264 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|     145 |  265 | `	}` |
|   24633 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|       - |  269 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|       - |  270 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|       - |  271 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|       - |  272 | ` *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:` |
|       - |  273 | ``  *                     `\nLABEL: BODY in FILE on line N` `` |
|       - |  274 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|       - |  275 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|       - |  276 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|       - |  277 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|       - |  278 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|       - |  279 | ` */` |
|     196 |  280 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|       - |  281 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|       4 |  282 | `{` |
|     200 |  283 | `	SyBlob *pWorker = &pVm->sWorker;` |
|     200 |  284 | `	sxi32 rc = SXRET_OK;` |
|     200 |  285 | `	if( pVm->bLogErrors ){` |
|     178 |  286 | `		SyBlobReset(pWorker);` |
|     178 |  287 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|     178 |  288 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|     178 |  289 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|     178 |  290 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      87 |  291 | `	}` |
|     200 |  292 | `	if( pVm->bDisplayErrors ){` |
|       - |  293 | `		sxi32 rc2;` |
|      29 |  294 | `		SyBlobReset(pWorker);` |
|       - |  295 | `		/* php's text-mode display copy is prefixed with a blank line */` |
|      29 |  296 | `		SyBlobAppend(pWorker,"\n",sizeof(char));` |
|      29 |  297 | `		VmDiagnosticHeader(pWorker,iErr);` |
|      29 |  298 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      29 |  299 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      29 |  300 | `		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);` |
|       - |  301 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|       - |  302 | `		 * than letting a later successful write mask it */` |
|      29 |  303 | `		if( rc == SXRET_OK ){` |
|      29 |  304 | `			rc = rc2;` |
|      13 |  305 | `		}` |
|      13 |  306 | `	}` |
|     200 |  307 | `	return rc;` |
|       4 |  308 | `}` |
|     378 |  309 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|       - |  310 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  311 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  312 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|       - |  313 | `	const char *zMessage /* Null terminated error message */` |
|       - |  314 | `	)` |
|       5 |  315 | `{` |
|       - |  316 | `	SyBlob sMsg;` |
|       - |  317 | `	SyString *pFile;` |
|     383 |  318 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     383 |  319 | `	sxi32 rc = SXRET_OK;` |
|       - |  320 | `	/* Peek the processed file if available */` |
|     383 |  321 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     383 |  322 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     383 |  323 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|       - |  324 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|       - |  325 | `		 * out-of-memory fatal through this path with none, and must not need` |
|       - |  326 | `		 * an allocation to say so. */` |
|      37 |  327 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      37 |  328 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      37 |  329 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      37 |  330 | `		nMsg = SyBlobLength(&sMsg);` |
|      17 |  331 | `	}` |
|       - |  332 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|       - |  333 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     383 |  334 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|     128 |  335 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|     128 |  336 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|       - |  337 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|       - |  338 | `			 * '@' php still runs the handler (done just above) but prints` |
|       - |  339 | `			 * nothing itself. */` |
|     124 |  340 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|      60 |  341 | `		}` |
|      62 |  342 | `	}` |
|     383 |  343 | `	SyBlobRelease(&sMsg);` |
|     383 |  344 | `	return rc;` |
|       5 |  345 | `}` |
|       - |  346 | `/*` |
|       - |  347 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|       - |  348 | ` *` |
|       - |  349 | ` * This is the single choke point for surfacing an allocation failure that would` |
|       - |  350 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|       - |  351 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|       - |  352 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|       - |  353 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|       - |  354 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|       - |  355 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|       - |  356 | ` * calling it from a VM op.` |
|       - |  357 | ` */` |
|     ! 0 |  358 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|     ! 0 |  359 | `{` |
|     ! 0 |  360 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|       - |  361 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|     ! 0 |  362 | `	pVm->iExitStatus = 255;` |
|     ! 0 |  363 | `	pVm->bHaltRequested = 1;` |
|     ! 0 |  364 | `	return PH7_ABORT;` |
|     ! 0 |  365 | `}` |
|       - |  366 | `/*` |
|       - |  367 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|       - |  368 | ` */` |
|     ! 0 |  369 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|     ! 0 |  370 | `{` |
|     ! 0 |  371 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|     ! 0 |  372 | `}` |
|       - |  373 | `/*` |
|       - |  374 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|       - |  375 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|       - |  376 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|       - |  377 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|       - |  378 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|       - |  379 | ` */` |
|       - |  380 | ``/* php only DEPRECATES a lossy float->int operand (`5 % 2.7`, `3 \| 1.5`); PHL targets`` |
|       - |  381 | ` * php's non-deprecated surface and rejects it with a TypeError. An INTEGRAL float` |
|       - |  382 | `` * (`4.0 % 3`) loses nothing and is accepted. Returns SXRET_OK to continue, or the`` |
|       - |  383 | ` * throw status for the caller to route via PH7_DISPATCH_ENFORCE_RC. */` |
|    4030 |  384 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|       5 |  385 | `{` |
|       - |  386 | `	double r;` |
|    4035 |  387 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_REAL) == 0 ){` |
|    4027 |  388 | `		return SXRET_OK;` |
|       - |  389 | `	}` |
|       9 |  390 | `	r = (double)pVal->rVal;` |
|       9 |  391 | `	if( r == (double)(sxi64)r ){` |
|       9 |  392 | `		return SXRET_OK;` |
|       - |  393 | `	}` |
|     ! 0 |  394 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       - |  395 | `		"Implicit conversion from float to int loses precision");` |
|    2020 |  396 | `}` |
|       - |  397 | `/*` |
|       - |  398 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|       - |  399 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|       - |  400 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|       - |  401 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|       - |  402 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|       - |  403 | ` */` |
| 2238891 |  404 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|       5 |  405 | `{` |
|       - |  406 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|       - |  407 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
| 2238896 |  408 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|       5 |  409 | `}` |
|       - |  410 | `/*` |
|       - |  411 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|       - |  412 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|       - |  413 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|       - |  414 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|       - |  415 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|       - |  416 | ` * keep native re-entries off a finite C stack).` |
|       - |  417 | ` */` |
| 3475138 |  418 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|       5 |  419 | `{` |
| 3475143 |  420 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|       5 |  421 | `}` |
|       - |  422 | `/*` |
|       - |  423 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|       - |  424 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|       - |  425 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|       - |  426 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|       - |  427 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|       - |  428 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|       - |  429 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|       - |  430 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|       - |  431 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|       - |  432 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|       - |  433 | ` * (VmNativeNestingFatal).` |
|       - |  434 | ` *` |
|       - |  435 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|       - |  436 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|       - |  437 | ` * re-enter and loop.` |
|       - |  438 | ` */` |
|       2 |  439 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|       1 |  440 | `{` |
|       3 |  441 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  442 | `		return PH7_ABORT;` |
|       - |  443 | `	}` |
|       3 |  444 | `	pVm->iExitStatus = 255;` |
|       3 |  445 | `	pVm->bHaltRequested = 1;` |
|       3 |  446 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|       3 |  447 | `	return PH7_ABORT;` |
|       2 |  448 | `}` |
|       - |  449 | `/*` |
|       - |  450 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|       - |  451 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|       - |  452 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|       - |  453 | ` */` |
|       4 |  454 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|       2 |  455 | `{` |
|       6 |  456 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  457 | `		return PH7_ABORT;` |
|       - |  458 | `	}` |
|       6 |  459 | `	pVm->iExitStatus = 255;` |
|       6 |  460 | `	pVm->bHaltRequested = 1;` |
|       6 |  461 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|       6 |  462 | `	return PH7_ABORT;` |
|       4 |  463 | `}` |
|       - |  464 | `/*` |
|       - |  465 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |  466 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - |  467 | ` * information.` |
|       - |  468 | ` */` |
|   24594 |  469 | `static sxi32 VmThrowErrorAp(` |
|       - |  470 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  471 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  472 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|       - |  473 | `	const char *zFormat, /* Format message */` |
|       - |  474 | `	va_list ap           /* Variable list of arguments */` |
|       - |  475 | `	)` |
|       5 |  476 | `{` |
|       - |  477 | `	SyBlob sMsg;` |
|       - |  478 | `	SyString *pFile;` |
|   24599 |  479 | `	sxi32 rc = SXRET_OK;` |
|       - |  480 | `	/* Peek the processed file if available */` |
|   24599 |  481 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - |  482 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|   24599 |  483 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|   24599 |  484 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|   24599 |  485 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - |  486 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|       - |  487 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|       - |  488 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|       - |  489 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|   24599 |  490 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|       - |  491 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|       - |  492 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|   23897 |  493 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|   23897 |  494 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|   23821 |  495 | `			SyBlobRelease(&sMsg);` |
|   23821 |  496 | `			return SXRET_OK;` |
|       - |  497 | `		}` |
|     118 |  498 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      38 |  499 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|      38 |  500 | `	}` |
|     783 |  501 | `	SyBlobRelease(&sMsg);` |
|     783 |  502 | `	return rc;` |
|   12302 |  503 | `}` |
|       - |  504 | `/*` |
|       - |  505 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|       - |  506 | ` * scope), or NULL when executing outside any class context.` |
|       - |  507 | ` */` |
|    8770 |  508 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|       5 |  509 | `{` |
|    8775 |  510 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|     129 |  511 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|     129 |  512 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|       - |  513 | `	}` |
|    8651 |  514 | `	return 0;` |
|    4390 |  515 | `}` |
|       - |  516 | `/*` |
|       - |  517 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|       - |  518 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|       - |  519 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|       - |  520 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|       - |  521 | ` * as an undefined function and therefore as another engine throw). Each such` |
|       - |  522 | ` * throw would construct another exception and recurse until the native-nesting` |
|       - |  523 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|       - |  524 | ` * (an engine throw from inside a user exception's constructor) working and` |
|       - |  525 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|       - |  526 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|       - |  527 | ` * after the call.` |
|       - |  528 | ` */` |
|       - |  529 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|  346248 |  530 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|       5 |  531 | `{` |
|  346253 |  532 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|     ! 0 |  533 | `		return 0;` |
|       - |  534 | `	}` |
|  346253 |  535 | `	pVm->nExcCtorDepth++;` |
|  346253 |  536 | `	return 1;` |
|  173129 |  537 | `}` |
|       - |  538 | `/*` |
|       - |  539 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|       - |  540 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|       - |  541 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|       - |  542 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|       - |  543 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|       - |  544 | ` */` |
|  205278 |  545 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|       5 |  546 | `{` |
|       - |  547 | `	ph7_class *pErrClass;` |
|       - |  548 | `	ph7_class_instance *pThis;` |
|       - |  549 | `	ph7_class_method *pCons;` |
|       - |  550 | `	VmFrame *pFrame;` |
|       - |  551 | `	sxi32 rc;` |
|  205283 |  552 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|  205283 |  553 | `	if( pErrClass == 0 ){` |
|     ! 0 |  554 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  555 | `		return PH7_ABORT;` |
|       - |  556 | `	}` |
|  205283 |  557 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|  205283 |  558 | `	if( pThis == 0 ){` |
|     ! 0 |  559 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  560 | `		return PH7_ABORT;` |
|       - |  561 | `	}` |
|  205283 |  562 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|  205283 |  563 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|       - |  564 | `		ph7_value sArg;` |
|       - |  565 | `		ph7_value *apArg[1];` |
|       - |  566 | `		SyString sMsgStr;` |
|  205283 |  567 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|  205283 |  568 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|  205283 |  569 | `		apArg[0] = &sArg;` |
|  205283 |  570 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|  205283 |  571 | `		PH7_MemObjRelease(&sArg);` |
|  205283 |  572 | `		pVm->nExcCtorDepth--;` |
|  102639 |  573 | `	}` |
|  205283 |  574 | `	SyBlobRelease(pMsg);` |
|  205283 |  575 | `	pFrame = pVm->pFrame;` |
|  205283 |  576 | `	if( pFrame ){` |
|  205283 |  577 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  205283 |  578 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|  102639 |  579 | `	}` |
|  205283 |  580 | `	rc = VmThrowException(&(*pVm),pThis);` |
|  205283 |  581 | `	PH7_ClassInstanceUnref(pThis);` |
|  205283 |  582 | `	if( rc == SXERR_ABORT ){` |
|      30 |  583 | `		return PH7_ABORT;` |
|       - |  584 | `	}` |
|  205257 |  585 | `	return PH7_EXCEPTION;` |
|  102644 |  586 | `}` |
|       - |  587 | `/*` |
|       - |  588 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|       - |  589 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|       - |  590 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|       - |  591 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|       - |  592 | ` * result through its normal goto Exception / goto Abort.` |
|       - |  593 | ` */` |
|      24 |  594 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|       3 |  595 | `{` |
|       - |  596 | `	SyBlob sMsg;` |
|      27 |  597 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      27 |  598 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      27 |  599 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|       3 |  600 | `}` |
|       - |  601 | `/*` |
|       - |  602 | ` * Enum case singletons (PHP 8.1).` |
|       - |  603 | ` *` |
|       - |  604 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|       - |  605 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|       - |  606 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|       - |  607 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|       - |  608 | ` * backing-value type check and the duplicate-value check only fire when a` |
|       - |  609 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|       - |  610 | ` */` |
|       - |  611 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|       - |  612 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|       - |  613 | ` * readonly property" through the normal store path. */` |
|       - |  614 |  |
|       - |  615 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|       - |  616 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|     410 |  617 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|       2 |  618 | `{` |
|     412 |  619 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|       - |  620 | `	ph7_class_instance *pObj;` |
|       - |  621 | `	SyHashEntry *pEntry;` |
|     412 |  622 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      59 |  623 | `		return 0;` |
|       - |  624 | `	}` |
|     354 |  625 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|     354 |  626 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|     354 |  627 | `	if( pEntry == 0 ){` |
|     ! 0 |  628 | `		return 0;` |
|       - |  629 | `	}` |
|     354 |  630 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|     207 |  631 | `}` |
|       - |  632 | `/*` |
|       - |  633 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|       - |  634 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|       - |  635 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|       - |  636 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|       - |  637 | ` * raises it. Returns the throw status to park/route.` |
|       - |  638 | ` */` |
|       2 |  639 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|       1 |  640 | `{` |
|       - |  641 | `	SyBlob sMsg;` |
|       3 |  642 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|       3 |  643 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|       3 |  644 | `	pVm->pConstCycleAttr = 0;` |
|       3 |  645 | `	pVm->pConstCycleClass = 0;` |
|       3 |  646 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  647 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|       1 |  648 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|       3 |  649 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  650 | `}` |
|       - |  651 | `/*` |
|       - |  652 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|       - |  653 | ` * materialize lazily and individually on first access — the backing-value` |
|       - |  654 | ` * type check fires per case, and the duplicate-value check compares only` |
|       - |  655 | ` * against cases that have already materialized (a broken sibling case does` |
|       - |  656 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|       - |  657 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|       - |  658 | ` * (duplicate value / self-reference) — which the caller routes` |
|       - |  659 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|       - |  660 | ` */` |
|     480 |  661 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|       4 |  662 | `{` |
|       - |  663 | `	ph7_class_attr **apCase;` |
|       - |  664 | `	ph7_class_instance *pObj;` |
|       - |  665 | `	ph7_value *pSlot;` |
|       - |  666 | `	ph7_value sBacking,sPropVal;` |
|       - |  667 | `	sxu32 i;` |
|     484 |  668 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|     369 |  669 | `		return SXRET_OK;` |
|       - |  670 | `	}` |
|     116 |  671 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  672 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|       - |  673 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|     ! 0 |  674 | `		if( pVm->pConstCycleAttr == 0 ){` |
|     ! 0 |  675 | `			pVm->pConstCycleAttr = pCase;` |
|     ! 0 |  676 | `			pVm->pConstCycleClass = pClass;` |
|     ! 0 |  677 | `		}` |
|     ! 0 |  678 | `		return SXRET_OK;` |
|       - |  679 | `	}` |
|     116 |  680 | `	PH7_MemObjInit(pVm,&sBacking);` |
|     116 |  681 | `	if( pClass->nEnumBacking != 0 ){` |
|      88 |  682 | `		if( pCase->pNativeValue ){` |
|       - |  683 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|       - |  684 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|       - |  685 | `			 * is what that byte-code would have produced anyway. */` |
|       5 |  686 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|      86 |  687 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|       - |  688 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|      84 |  689 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       - |  690 | `			sxi32 rcExec;` |
|      84 |  691 | `			pVm->pConstEvalClass = pClass;` |
|      84 |  692 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      84 |  693 | `			pVm->nConstEvalDepth++;` |
|      84 |  694 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      84 |  695 | `			pVm->nConstEvalDepth--;` |
|      84 |  696 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      84 |  697 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      84 |  698 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  699 | `				/* The backing expression raised: abandon materialization and` |
|       - |  700 | `				 * hand the status to the caller to park/route. */` |
|       3 |  701 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  702 | `				return rcExec;` |
|       - |  703 | `			}` |
|      82 |  704 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|     ! 0 |  705 | `				PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  706 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  707 | `			}` |
|      39 |  708 | `		}` |
|      86 |  709 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|       - |  710 | `			/* php: TypeError, checked lazily at first case access */` |
|       - |  711 | `			SyBlob sMsg;` |
|       3 |  712 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|       3 |  713 | `			PH7_MemObjRelease(&sBacking);` |
|       3 |  714 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       2 |  715 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|       2 |  716 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|       3 |  717 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - |  718 | `		}` |
|      84 |  719 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       - |  720 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|       - |  721 | `			 * the typed-constant leniency) to a genuine int. */` |
|      17 |  722 | `			PH7_MemObjToInteger(&sBacking);` |
|       9 |  723 | `		}else{` |
|      68 |  724 | `			PH7_MemObjToString(&sBacking);` |
|       - |  725 | `		}` |
|       - |  726 | `		/* php: two cases sharing one backing value are an Error — compared` |
|       - |  727 | `		 * against already-materialized cases only (php registers values as` |
|       - |  728 | `		 * each case evaluates). */` |
|      84 |  729 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     274 |  730 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|       - |  731 | `			ph7_value *pPrev;` |
|     196 |  732 | `			int bDup = 0;` |
|     196 |  733 | `			if( apCase[i] == pCase ){` |
|      82 |  734 | `				continue;` |
|       - |  735 | `			}` |
|     115 |  736 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|     115 |  737 | `			if( pPrev ){` |
|      57 |  738 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       7 |  739 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|       4 |  740 | `				}else{` |
|      63 |  741 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|      50 |  742 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|      24 |  743 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|       - |  744 | `				}` |
|      28 |  745 | `			}` |
|     115 |  746 | `			if( bDup ){` |
|       - |  747 | `				/* php prints the two cases in DECLARATION order regardless of` |
|       - |  748 | `				 * which one is being evaluated. */` |
|       3 |  749 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|       - |  750 | `				SyBlob sMsg;` |
|       - |  751 | `				sxu32 j;` |
|       5 |  752 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|       5 |  753 | `					if( apCase[j] == pCase ){ break; }` |
|       2 |  754 | `				}` |
|       3 |  755 | `				if( j < i ){` |
|     ! 0 |  756 | `					pFirst = pCase;` |
|     ! 0 |  757 | `					pSecond = apCase[i];` |
|     ! 0 |  758 | `				}` |
|       3 |  759 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  760 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  761 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|       1 |  762 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|       3 |  763 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - |  764 | `			}` |
|      57 |  765 | `		}` |
|      39 |  766 | `	}` |
|       - |  767 | `	/* Create the singleton and fill its readonly props */` |
|     110 |  768 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|     110 |  769 | `	if( pObj == 0 ){` |
|     ! 0 |  770 | `		PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  771 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  772 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|     ! 0 |  773 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  774 | `		return PH7_ABORT;` |
|       - |  775 | `	}` |
|     110 |  776 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|     110 |  777 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|     110 |  778 | `	PH7_MemObjRelease(&sPropVal);` |
|     110 |  779 | `	if( pClass->nEnumBacking != 0 ){` |
|      82 |  780 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|      39 |  781 | `	}` |
|     110 |  782 | `	PH7_MemObjRelease(&sBacking);` |
|       - |  783 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|       - |  784 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|     110 |  785 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|     110 |  786 | `	if( pSlot == 0 ){` |
|     ! 0 |  787 | `		PH7_ClassInstanceUnref(pObj);` |
|     ! 0 |  788 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  789 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|     ! 0 |  790 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  791 | `		return PH7_ABORT;` |
|       - |  792 | `	}` |
|     110 |  793 | `	pSlot->x.pOther = pObj;` |
|     110 |  794 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|     110 |  795 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     110 |  796 | `	pCase->nIdx = pSlot->nIdx;` |
|     110 |  797 | `	return SXRET_OK;` |
|     244 |  798 | `}` |
|       - |  799 | `/*` |
|       - |  800 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|       - |  801 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|       - |  802 | ` * there, so a broken case surfaces its error at the same point).` |
|       - |  803 | ` */` |
|     244 |  804 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|       4 |  805 | `{` |
|       - |  806 | `	ph7_class_attr **apCase;` |
|       - |  807 | `	sxu32 n;` |
|     248 |  808 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 |  809 | `		return SXRET_OK;` |
|       - |  810 | `	}` |
|     248 |  811 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     706 |  812 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     468 |  813 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|     468 |  814 | `		if( rc != SXRET_OK ){` |
|       8 |  815 | `			return rc;` |
|       - |  816 | `		}` |
|     233 |  817 | `	}` |
|     242 |  818 | `	return SXRET_OK;` |
|     126 |  819 | `}` |
|       - |  820 | `/*` |
|       - |  821 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|       - |  822 | ` * or 0 when the name does not name an enum.` |
|       - |  823 | ` */` |
|     204 |  824 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|       3 |  825 | `{` |
|       - |  826 | `	ph7_class *pClass;` |
|     207 |  827 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|     ! 0 |  828 | `		return 0;` |
|       - |  829 | `	}` |
|     309 |  830 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|     102 |  831 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|     209 |  832 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|       3 |  833 | `		pClass = pClass->pNextName;` |
|       1 |  834 | `	}` |
|     207 |  835 | `	return pClass;` |
|     105 |  836 | `}` |
|       - |  837 | `/*` |
|       - |  838 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|       - |  839 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|       - |  840 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|       - |  841 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|       - |  842 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|       - |  843 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|       - |  844 | ` */` |
|     426 |  845 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|       5 |  846 | `{` |
|     431 |  847 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     426 |  848 | `	if( pInner && pInner->pUserData` |
|     253 |  849 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|     ! 0 |  850 | `		return 0;` |
|       - |  851 | `	}` |
|     431 |  852 | `	return pVm->nCurLine;` |
|     218 |  853 | `}` |
|       - |  854 | `/*` |
|       - |  855 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|       - |  856 | ` * not the pointer: aMemObj is a by-value SySet, so any nested evaluation (an` |
|       - |  857 | ` * initializer, or the constructor of the very TypeError being raised) can grow` |
|       - |  858 | ` * and REALLOC the pool, leaving a pointer taken before it dangling — the rule` |
|       - |  859 | ` * VmLocalExecIntoObj is built around. The slot's contents are released first:` |
|       - |  860 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|       - |  861 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|       - |  862 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|       - |  863 | ` */` |
|      36 |  864 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|       3 |  865 | `{` |
|      39 |  866 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       - |  867 | `	VmSlot sSlot;` |
|      39 |  868 | `	if( pObj == 0 ){` |
|     ! 0 |  869 | `		return;` |
|       - |  870 | `	}` |
|      39 |  871 | `	PH7_MemObjRelease(pObj);` |
|      39 |  872 | `	sSlot.nIdx = nIdx;` |
|      39 |  873 | `	sSlot.pUserData = 0;` |
|      39 |  874 | `	SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      21 |  875 | `}` |
|       - |  876 | `/*` |
|       - |  877 | ` * Evaluate a class constant's initializer on demand.` |
|       - |  878 | ` *` |
|       - |  879 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|       - |  880 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|       - |  881 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|       - |  882 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|       - |  883 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|       - |  884 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|       - |  885 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|       - |  886 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|       - |  887 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|       - |  888 | ` * "Cannot declare self-referencing constant" Error.` |
|       - |  889 | ` */` |
|     480 |  890 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  891 | `{` |
|       - |  892 | `	ph7_value *pMemObj;` |
|     480 |  893 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     480 |  894 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     485 |  895 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|     ! 0 |  896 | `		return SXRET_OK;` |
|       - |  897 | `	}` |
|     485 |  898 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  899 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|       - |  900 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|       - |  901 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|       3 |  902 | `		if( pVm->pConstCycleAttr == 0 ){` |
|       3 |  903 | `			pVm->pConstCycleAttr = pAttr;` |
|       3 |  904 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 |  905 | `		}` |
|       3 |  906 | `		return SXRET_OK;` |
|       - |  907 | `	}` |
|     483 |  908 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     483 |  909 | `	if( pMemObj == 0 ){` |
|     ! 0 |  910 | `		return SXERR_MEM;` |
|       - |  911 | `	}` |
|     483 |  912 | `	if( pAttr->pNativeValue ){` |
|       - |  913 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|       - |  914 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|       - |  915 | `		 * when they went lazy (17th session), so this path — the only one an` |
|       - |  916 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|       - |  917 | `		 * which is why every native class constant read NULL: nothing had declared` |
|       - |  918 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|       - |  919 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|       - |  920 | `		 * failure path to mirror below. */` |
|     113 |  921 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|     113 |  922 | `		pAttr->nIdx = pMemObj->nIdx;` |
|     113 |  923 | `		return SXRET_OK;` |
|       - |  924 | `	}` |
|     371 |  925 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     371 |  926 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     371 |  927 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|     371 |  928 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|       - |  929 | `		sxu32 nSaveLazyLine;` |
|       - |  930 | `		sxi32 nSaveLazyDepth;` |
|       - |  931 | `		sxu32 nSlot;` |
|       - |  932 | `		sxi32 rcExec;` |
|     371 |  933 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|     371 |  934 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - |  935 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|       - |  936 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|       - |  937 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|     371 |  938 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - |  939 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|       - |  940 | `		 * throw out of its own bytecode carries. */` |
|     371 |  941 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|     371 |  942 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|     371 |  943 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|     371 |  944 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|     371 |  945 | `		pVm->nConstEvalDepth++;` |
|     371 |  946 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     371 |  947 | `		pVm->nConstEvalDepth--;` |
|     371 |  948 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|     371 |  949 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|     371 |  950 | `		pVm->pConstEvalClass = pSaveCtx;` |
|     371 |  951 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|     371 |  952 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     371 |  953 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|     366 |  954 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|     342 |  955 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|       - |  956 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|       - |  957 | `			 * class constant's expression at each access until one of them` |
|       - |  958 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|       - |  959 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|       - |  960 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|       - |  961 | `			 * the rest of the run — and made a static default that named it` |
|       - |  962 | `			 * (whose own evaluation is deferred to first access) find it` |
|       - |  963 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|       - |  964 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|       - |  965 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|       - |  966 | `			 * it is found — an inner level only records it — but the value is` |
|       - |  967 | `			 * unusable and the next access must be able to detect it again.` |
|       - |  968 | `			 * No loop: each access runs the initializer once and raises. */` |
|      35 |  969 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|      35 |  970 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  971 | `				/* Hand the status to the caller to park/route. */` |
|      32 |  972 | `				return rcExec;` |
|       - |  973 | `			}` |
|       3 |  974 | `			if( pVm->nConstEvalDepth == 0 ){` |
|       - |  975 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|       - |  976 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|       3 |  977 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  978 | `			}` |
|     ! 0 |  979 | `			return SXRET_OK;` |
|       - |  980 | `		}` |
|     339 |  981 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       - |  982 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|       - |  983 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|       - |  984 | `			 * access raises again — php re-runs the whole materialization each` |
|       - |  985 | `			 * time. A pass may widen int -> float in place, which is the value` |
|       - |  986 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|       - |  987 | `			 * runs php code that may grow (and realloc) aMemObj — so the slot is` |
|       - |  988 | `			 * addressed by index from here on, never through pMemObj. */` |
|       5 |  989 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|       5 |  990 | `			if( rcType != SXRET_OK ){` |
|       5 |  991 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|       5 |  992 | `				return rcType;` |
|       - |  993 | `			}` |
|     ! 0 |  994 | `		}` |
|       - |  995 | `		/* Memoize the value. */` |
|     335 |  996 | `		pAttr->nIdx = nSlot;` |
|     335 |  997 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|     335 |  998 | `		return SXRET_OK;` |
|       - |  999 | `	}` |
|     ! 0 | 1000 | `	pAttr->nIdx = pMemObj->nIdx;` |
|     ! 0 | 1001 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     ! 0 | 1002 | `	return SXRET_OK;` |
|     245 | 1003 | `}` |
|       - | 1004 | `/*` |
|       - | 1005 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|       - | 1006 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|       - | 1007 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|       - | 1008 | ` */` |
|     136 | 1009 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1010 | `{` |
|     140 | 1011 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      77 | 1012 | `		return SXRET_OK;` |
|       - | 1013 | `	}` |
|      64 | 1014 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|      15 | 1015 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       - | 1016 | `	}` |
|      50 | 1017 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      72 | 1018 | `}` |
|       - | 1019 | `/*` |
|       - | 1020 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|       - | 1021 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|       - | 1022 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|       - | 1023 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|       - | 1024 | ` */` |
|       6 | 1025 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|       1 | 1026 | `{` |
|       - | 1027 | `	SyBlob sMsg;` |
|       7 | 1028 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 | 1029 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       7 | 1030 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1031 | `}` |
|       - | 1032 | `/*` |
|       - | 1033 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|       - | 1034 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|       - | 1035 | ` * raised at the store site here with the same message and the same` |
|       - | 1036 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|       - | 1037 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|       - | 1038 | ` */` |
|       2 | 1039 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|       1 | 1040 | `{` |
|       3 | 1041 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|       3 | 1042 | `	pVm->iExitStatus = 255;` |
|       3 | 1043 | `	pVm->bHaltRequested = 1;` |
|       3 | 1044 | `	return PH7_ABORT;` |
|       1 | 1045 | `}` |
|       - | 1046 | `/*` |
|       - | 1047 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|       - | 1048 | ` * property assignment. Called from the STORE path when coercion is not` |
|       - | 1049 | ` * possible.` |
|       - | 1050 | ` */` |
|  100134 | 1051 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|       5 | 1052 | `{` |
|  100139 | 1053 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|  100139 | 1054 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 1055 | `	char zType[192];` |
|  150206 | 1056 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|   50067 | 1057 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));` |
|       - | 1058 | `	SyBlob sMsg;` |
|  100139 | 1059 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 1060 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|       - | 1061 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|  100139 | 1062 | `	if( pOwner ){` |
|  100139 | 1063 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|   50067 | 1064 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|   50072 | 1065 | `	}else{` |
|     ! 0 | 1066 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",` |
|     ! 0 | 1067 | `			zGiven,&pAttr->sName,zTypeText);` |
|       - | 1068 | `	}` |
|  100139 | 1069 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       5 | 1070 | `}` |
|       - | 1071 | `/*` |
|       - | 1072 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|       - | 1073 | ` */` |
|  100010 | 1074 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1075 | `{` |
|  100014 | 1076 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|  100014 | 1077 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|       - | 1078 | `	SyBlob sMsg;` |
|  100014 | 1079 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|  100014 | 1080 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|   50005 | 1081 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|  100014 | 1082 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       4 | 1083 | `}` |
|       - | 1084 | `/*` |
|       - | 1085 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|       - | 1086 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|       - | 1087 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|       - | 1088 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|       - | 1089 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|       - | 1090 | ` */` |
|       - | 1091 | `/*` |
|       - | 1092 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|       - | 1093 | ` * property from a scope its set-visibility excludes:` |
|       - | 1094 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|       - | 1095 | ` */` |
|      14 | 1096 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1097 | `{` |
|      15 | 1098 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      15 | 1099 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|      15 | 1100 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|       - | 1101 | `	SyBlob sMsg;` |
|      15 | 1102 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 1103 | `	if( pActive ){` |
|       3 | 1104 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|       1 | 1105 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|       2 | 1106 | `	}else{` |
|      13 | 1107 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|       6 | 1108 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|       - | 1109 | `	}` |
|      15 | 1110 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1111 | `}` |
|       - | 1112 | `/*` |
|       - | 1113 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|       - | 1114 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|       - | 1115 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|       - | 1116 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|       - | 1117 | ` */` |
|      32 | 1118 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|       1 | 1119 | `{` |
|      33 | 1120 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|      33 | 1121 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1122 | `	int bOk;` |
|      33 | 1123 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      27 | 1124 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|      14 | 1125 | `	}else{` |
|       7 | 1126 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|       - | 1127 | `	}` |
|      33 | 1128 | `	if( !bOk ){` |
|      15 | 1129 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|       - | 1130 | `	}` |
|      19 | 1131 | `	return SXRET_OK;` |
|      17 | 1132 | `}` |
|      40 | 1133 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|       5 | 1134 | `{` |
|      45 | 1135 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1136 | `	SyBlob sMsg;` |
|      45 | 1137 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      45 | 1138 | `	if( bModify ){` |
|      41 | 1139 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|      23 | 1140 | `	}else{` |
|       6 | 1141 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       6 | 1142 | `		if( pActive ){` |
|     ! 0 | 1143 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|     ! 0 | 1144 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|     ! 0 | 1145 | `		}else{` |
|       6 | 1146 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|       2 | 1147 | `				&pOwner->sName,&pAttr->sName);` |
|       - | 1148 | `		}` |
|       - | 1149 | `	}` |
|      45 | 1150 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       5 | 1151 | `}` |
|       - | 1152 | `/*` |
|       - | 1153 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|       - | 1154 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|       - | 1155 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|       - | 1156 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|       - | 1157 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|       - | 1158 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|       - | 1159 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|       - | 1160 | ` */` |
|  667473 | 1161 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|       5 | 1162 | `{` |
|       - | 1163 | `	SyHashEntry *pSlot;` |
|       - | 1164 | `	VmClassAttr *pVmAttr;` |
|  667478 | 1165 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|  417807 | 1166 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|       - | 1167 | `	}` |
|  249676 | 1168 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  249676 | 1169 | `	if( pSlot == 0 ){` |
|  249592 | 1170 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1171 | `	}` |
|      87 | 1172 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      87 | 1173 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|      12 | 1174 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|       - | 1175 | `	}` |
|      74 | 1176 | `	if( pVmAttr->pAttr` |
|      76 | 1177 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|       - | 1178 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|       7 | 1179 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|       - | 1180 | `	}` |
|      70 | 1181 | `	return SXRET_OK;` |
|  334160 | 1182 | `}` |
|       - | 1183 | `/*` |
|       - | 1184 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|       - | 1185 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|       - | 1186 | ` * For class types, instanceof is verified.` |
|       - | 1187 | ` *` |
|       - | 1188 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|       - | 1189 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|       - | 1190 | ` */` |
|       - | 1191 |  |
|       - | 1192 | `/*` |
|       - | 1193 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|       - | 1194 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|       - | 1195 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|       - | 1196 | ` *   0 if it's not strictly numeric.` |
|       - | 1197 | ` */` |
|      38 | 1198 | `static int VmStringNumericKind(ph7_value *pValue)` |
|       3 | 1199 | `{` |
|       - | 1200 | `	const char *z, *zEnd, *zTail;` |
|       - | 1201 | `	sxu32 n;` |
|      41 | 1202 | `	sxu8 bReal = 0;` |
|       - | 1203 | `	sxi32 rc;` |
|      41 | 1204 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      24 | 1205 | `		return 0;` |
|       - | 1206 | `	}` |
|      18 | 1207 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|      18 | 1208 | `	n = SyBlobLength(&pValue->sBlob);` |
|      18 | 1209 | `	zEnd = z + n;` |
|      18 | 1210 | `	if( n == 0 ) return 0;` |
|      18 | 1211 | `	zTail = 0;` |
|      18 | 1212 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|      18 | 1213 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|      19 | 1214 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|      15 | 1215 | `	if( zTail != zEnd ) return 0;` |
|      15 | 1216 | `	return bReal ? 2 : 1;` |
|      22 | 1217 | `}` |
|       - | 1218 |  |
|       - | 1219 | `/*` |
|       - | 1220 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|       - | 1221 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|       - | 1222 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|       - | 1223 | ` * return, parameter, property, union alternative — would have to string-match` |
|       - | 1224 | ` * the name itself.` |
|       - | 1225 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|       - | 1226 | ` * to extend when another literal/pseudo type is added.` |
|       - | 1227 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|       - | 1228 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|       - | 1229 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|       - | 1230 | ` */` |
|    2722 | 1231 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|       5 | 1232 | `{` |
|    2727 | 1233 | `	const char *z = pClass->zString;` |
|    2727 | 1234 | `	sxu32 n = pClass->nByte;` |
|    2727 | 1235 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|     136 | 1236 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|       - | 1237 | `	}` |
|    2595 | 1238 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|      28 | 1239 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|       - | 1240 | `	}` |
|    2569 | 1241 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|      51 | 1242 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|       - | 1243 | `	}` |
|    2521 | 1244 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|       - | 1245 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|       - | 1246 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|       - | 1247 | `		 * object; scope-sensitive, so a private method is callable only from` |
|       - | 1248 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|       - | 1249 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|       - | 1250 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|       - | 1251 | `		 * php does.) */` |
|    1787 | 1252 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|       - | 1253 | `	}` |
|     739 | 1254 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|       - | 1255 | `		/* iterable === array \| Traversable */` |
|      57 | 1256 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      14 | 1257 | `			return 1;` |
|       - | 1258 | `		}` |
|      45 | 1259 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|      23 | 1260 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      23 | 1261 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|      13 | 1262 | `				return 1;` |
|       - | 1263 | `			}` |
|       4 | 1264 | `		}` |
|      33 | 1265 | `		return 0;` |
|       - | 1266 | `	}` |
|     685 | 1267 | `	return -1;` |
|    1366 | 1268 | `}` |
|       - | 1269 | `/*` |
|       - | 1270 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|       - | 1271 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|       - | 1272 | ` * through. php binds the keyword where the hint is written, so` |
|       - | 1273 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|       - | 1274 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|       - | 1275 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|       - | 1276 | ` * throw a TypeError over perfectly valid code.` |
|       - | 1277 | ` *` |
|       - | 1278 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|       - | 1279 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|       - | 1280 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|       - | 1281 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|       - | 1282 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|       - | 1283 | `` * VmResolveTypeClass applies to `parent`.`` |
|       - | 1284 | ` */` |
|  210662 | 1285 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|       5 | 1286 | `{` |
|  210667 | 1287 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  202017 | 1288 | `		return pDecl;` |
|       - | 1289 | `	}` |
|    8655 | 1290 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|  105336 | 1291 | `}` |
|       - | 1292 | `/*` |
|       - | 1293 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|       - | 1294 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|       - | 1295 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|       - | 1296 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|       - | 1297 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|       - | 1298 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|       - | 1299 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|       - | 1300 | ` * throw.` |
|       - | 1301 | ` *` |
|       - | 1302 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|       - | 1303 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|       - | 1304 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|       - | 1305 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|       - | 1306 | ` */` |
|       - | 1307 | `/*` |
|       - | 1308 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|       - | 1309 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|       - | 1310 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|       - | 1311 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|       - | 1312 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|       - | 1313 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|       - | 1314 | ` * iLoadable flag — lives in one place.` |
|       - | 1315 | ` *` |
|       - | 1316 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|       - | 1317 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|       - | 1318 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|       - | 1319 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|       - | 1320 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|       - | 1321 | ` *` |
|       - | 1322 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|       - | 1323 | ` * compatibility target, where the type may legitimately be an interface or` |
|       - | 1324 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|       - | 1325 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|       - | 1326 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|       - | 1327 | ` * with TRUE; it does not go through this helper.)` |
|       - | 1328 | ` */` |
|     752 | 1329 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|       5 | 1330 | `{` |
|     757 | 1331 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      98 | 1332 | `		return pSelf;` |
|       - | 1333 | `	}` |
|     663 | 1334 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|       - | 1335 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|       - | 1336 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|       - | 1337 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|       - | 1338 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|       - | 1339 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|      39 | 1340 | `		return PH7_VmPeekTopClass(pVm);` |
|       - | 1341 | `	}` |
|     627 | 1342 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|       - | 1343 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|       - | 1344 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|      21 | 1345 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 | 1346 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|     ! 0 | 1347 | `		}` |
|      21 | 1348 | `		return pSelf ? pSelf->pBase : 0;` |
|       - | 1349 | `	}` |
|     609 | 1350 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|     381 | 1351 | `}` |
|       - | 1352 | `/*` |
|       - | 1353 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|       - | 1354 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|       - | 1355 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|       - | 1356 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|       - | 1357 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|       - | 1358 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|       - | 1359 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|       - | 1360 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|       - | 1361 | ` */` |
|  100482 | 1362 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|       5 | 1363 | `{` |
|  100496 | 1364 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|  100472 | 1365 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|  150723 | 1366 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|       5 | 1367 | `}` |
|       - | 1368 | `/*` |
|       - | 1369 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|       - | 1370 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|       - | 1371 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|       - | 1372 | ` * typed-default checks — each of which then formats its own message. The` |
|       - | 1373 | ` * resolved class is handed back through *ppResolved for the message builder` |
|       - | 1374 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|       - | 1375 | ` * nothing resolved).` |
|       - | 1376 | ` *` |
|       - | 1377 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|       - | 1378 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|       - | 1379 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|       - | 1380 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|       - | 1381 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|       - | 1382 | ` * resolve against there is no class to compare to — a position php rejects at` |
|       - | 1383 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|       - | 1384 | ` *` |
|       - | 1385 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|       - | 1386 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|       - | 1387 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|       - | 1388 | ` * autoloader can produce fails.` |
|       - | 1389 | ` */` |
|     564 | 1390 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|       - | 1391 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|       5 | 1392 | `{` |
|     569 | 1393 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     569 | 1394 | `	*ppResolved = pExpected;` |
|     569 | 1395 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      42 | 1396 | `		return 0;` |
|       - | 1397 | `	}` |
|     531 | 1398 | `	if( pExpected == 0 ){` |
|      13 | 1399 | `		return VmHintIsScopeKeyword(pName);` |
|       - | 1400 | `	}` |
|     519 | 1401 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|     287 | 1402 | `}` |
|       - | 1403 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|       - | 1404 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|  402914 | 1405 | `static int VmHintNameChar(int c)` |
|       5 | 1406 | `{` |
|  805475 | 1407 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|  402562 | 1408 | `		\|\| c == ' ' \|\| c == '\t');` |
|       5 | 1409 | `}` |
|       - | 1410 | `/*` |
|       - | 1411 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|       - | 1412 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|       - | 1413 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|       - | 1414 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|       - | 1415 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|       - | 1416 | ` * resolved until the call site are substituted.` |
|       - | 1417 | ` *` |
|       - | 1418 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|       - | 1419 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|       - | 1420 | ` * are only ever available as source: unions/intersections, and the property /` |
|       - | 1421 | ` * class-constant messages, which print the declared type whatever its shape.` |
|       - | 1422 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|       - | 1423 | ` */` |
|  100302 | 1424 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|       - | 1425 | `	char *zBuf,sxu32 nBuf)` |
|       5 | 1426 | `{` |
|       - | 1427 | `	const char *z;` |
|  100307 | 1428 | `	sxu32 n, i = 0, nAt = 0;` |
|  100307 | 1429 | `	if( nBuf == 0 ){` |
|     ! 0 | 1430 | `		return "";` |
|       - | 1431 | `	}` |
|  100307 | 1432 | `	z = pDeclared ? pDeclared->zString : 0;` |
|  100307 | 1433 | `	n = z ? pDeclared->nByte : 0;` |
|  200967 | 1434 | `	while( i < n && nAt + 1 < nBuf ){` |
|       - | 1435 | `		sxu32 nStart, nCopy;` |
|       - | 1436 | `		SyString sTok;` |
|       - | 1437 | `		const SyString *pOut;` |
|  100665 | 1438 | `		if( !VmHintNameChar(z[i]) ){` |
|     195 | 1439 | `			zBuf[nAt++] = z[i++];` |
|     195 | 1440 | `			continue;` |
|       - | 1441 | `		}` |
|  100475 | 1442 | `		nStart = i;` |
|  402561 | 1443 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|  302091 | 1444 | `			i++;` |
|       5 | 1445 | `		}` |
|  100475 | 1446 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|  100475 | 1447 | `		pOut = &sTok;` |
|  100475 | 1448 | `		if( VmHintIsScopeKeyword(&sTok) ){` |
|      30 | 1449 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|      30 | 1450 | `			if( pRes ){` |
|      30 | 1451 | `				pOut = &pRes->sName;` |
|      14 | 1452 | `			}` |
|      14 | 1453 | `		}` |
|  100475 | 1454 | `		nCopy = pOut->nByte;` |
|  100475 | 1455 | `		if( nCopy > nBuf - nAt - 1 ){` |
|     ! 0 | 1456 | `			nCopy = nBuf - nAt - 1;` |
|     ! 0 | 1457 | `		}` |
|  100475 | 1458 | `		if( nCopy > 0 ){` |
|  100475 | 1459 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|  100475 | 1460 | `			nAt += nCopy;` |
|   50235 | 1461 | `		}` |
|       5 | 1462 | `	}` |
|  100307 | 1463 | `	zBuf[nAt] = 0;` |
|  100307 | 1464 | `	return zBuf;` |
|   50156 | 1465 | `}` |
|       - | 1466 | `/*` |
|       - | 1467 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|       - | 1468 | ` * float-identity leniency — see the typed-constant note above` |
|       - | 1469 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|       - | 1470 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|       - | 1471 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|       - | 1472 | ` * so an int-typed parameter / return / property / union member silently` |
|       - | 1473 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|       - | 1474 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|       - | 1475 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|       - | 1476 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|       - | 1477 | ` * present).` |
|       - | 1478 | ` *` |
|       - | 1479 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|       - | 1480 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|       - | 1481 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|       - | 1482 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|       - | 1483 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|       - | 1484 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|       - | 1485 | ` */` |
|   37392 | 1486 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|       5 | 1487 | `{` |
|   37392 | 1488 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|   21075 | 1489 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       - | 1490 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|       - | 1491 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|       - | 1492 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|       - | 1493 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|      51 | 1494 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|      51 | 1495 | `		SyBlobRelease(&pVal->sBlob);` |
|      51 | 1496 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|      23 | 1497 | `	}` |
|   37397 | 1498 | `}` |
|     334 | 1499 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|       - | 1500 | `	ph7_class *pSelf)` |
|       5 | 1501 | `{` |
|       - | 1502 | `	sxu32 i;` |
|       - | 1503 | `	sxu32 nAlts;` |
|       - | 1504 | `	ph7_type_alt *aAlts;` |
|       - | 1505 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|       - | 1506 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     339 | 1507 | `	int bHasIntersection = 0;` |
|       - | 1508 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     339 | 1509 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      20 | 1510 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|       - | 1511 | `	}` |
|     323 | 1512 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     323 | 1513 | `	nAlts = SySetUsed(pAlts);` |
|       - | 1514 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|       - | 1515 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|       - | 1516 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|       - | 1517 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|       - | 1518 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   10499 | 1519 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     995 | 1520 | `	for( i = 0; i < nAlts; i++ ){` |
|     677 | 1521 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|      42 | 1522 | `			bHasIntersection = 1;` |
|      19 | 1523 | `		}` |
|     341 | 1524 | `	}` |
|       - | 1525 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|       - | 1526 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|       - | 1527 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|       - | 1528 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     323 | 1529 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|      35 | 1530 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1531 | `		sxu32 g;` |
|     421 | 1532 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|       - | 1533 | `			int bAll;` |
|     409 | 1534 | `			if( aGroupCount[g] < 2 ) continue;` |
|      35 | 1535 | `			bAll = 1;` |
|      87 | 1536 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1537 | `				ph7_class *pExpected;` |
|      67 | 1538 | `				if( aAlts[i].nGroup != g ) continue;` |
|      63 | 1539 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|      63 | 1540 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      63 | 1541 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      15 | 1542 | `					bAll = 0;` |
|      15 | 1543 | `					break;` |
|       - | 1544 | `				}` |
|      27 | 1545 | `			}` |
|      35 | 1546 | `			if( bAll ) return SXRET_OK;` |
|       9 | 1547 | `		}` |
|       6 | 1548 | `	}` |
|       - | 1549 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|       - | 1550 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|       - | 1551 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|       - | 1552 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     915 | 1553 | `	for( i = 0; i < nAlts; i++ ){` |
|     631 | 1554 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     590 | 1555 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     404 | 1556 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|      16 | 1557 | `			return SXRET_OK;` |
|       - | 1558 | `		}` |
|     293 | 1559 | `	}` |
|     289 | 1560 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     289 | 1561 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     889 | 1562 | `	for( i = 0; i < nAlts; i++ ){` |
|     605 | 1563 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     569 | 1564 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     387 | 1565 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     383 | 1566 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     377 | 1567 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     167 | 1568 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     127 | 1569 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|       5 | 1570 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     287 | 1571 | `	}` |
|       - | 1572 | `	/* Object handling */` |
|     289 | 1573 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      97 | 1574 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      97 | 1575 | `		if( bHasClassAlt ){` |
|      83 | 1576 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     211 | 1577 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1578 | `				ph7_class *pExpected;` |
|     157 | 1579 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     149 | 1580 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|     105 | 1581 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|     105 | 1582 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      28 | 1583 | `					return SXRET_OK;` |
|       - | 1584 | `				}` |
|      42 | 1585 | `			}` |
|      27 | 1586 | `		}` |
|      72 | 1587 | `		return SXERR_INVALID;` |
|       - | 1588 | `	}` |
|       - | 1589 | `	/* Array handling */` |
|     197 | 1590 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      15 | 1591 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|       - | 1592 | `	}` |
|       - | 1593 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|       - | 1594 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|       - | 1595 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|       - | 1596 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|       - | 1597 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|       - | 1598 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     185 | 1599 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      22 | 1600 | `		if( bHasFloat ) return SXRET_OK;` |
|       5 | 1601 | `	}` |
|     177 | 1602 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|     119 | 1603 | `		if( bHasInt ){` |
|      96 | 1604 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      96 | 1605 | `			return SXRET_OK;` |
|       - | 1606 | `		}` |
|      11 | 1607 | `	}` |
|      85 | 1608 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|      58 | 1609 | `		if( bHasString ) return SXRET_OK;` |
|       8 | 1610 | `	}` |
|      45 | 1611 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|       3 | 1612 | `		if( bHasBool ) return SXRET_OK;` |
|       1 | 1613 | `	}` |
|      45 | 1614 | `	if( bStrict ){` |
|       - | 1615 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|       5 | 1616 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|     ! 0 | 1617 | `			PH7_MemObjToReal(pValue);` |
|     ! 0 | 1618 | `			return SXRET_OK;` |
|       - | 1619 | `		}` |
|       5 | 1620 | `		return SXERR_INVALID;` |
|       - | 1621 | `	}` |
|       - | 1622 | `	/* Weak coercion preference order: int > float > string > bool.` |
|       - | 1623 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|       - | 1624 | `	 * to match PHP's union RFC. */` |
|       - | 1625 | `	{` |
|      41 | 1626 | `		int kind = VmStringNumericKind(pValue);` |
|      41 | 1627 | `		if( bHasInt ){` |
|       - | 1628 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|       - | 1629 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|      18 | 1630 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1631 | `				PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1632 | `				return SXRET_OK;` |
|       - | 1633 | `			}` |
|      18 | 1634 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1635 | `				ph7_real r = pValue->rVal;` |
|     ! 0 | 1636 | `				if( r == (ph7_real)(sxi64)r ){` |
|     ! 0 | 1637 | `					PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1638 | `					return SXRET_OK;` |
|       - | 1639 | `				}` |
|     ! 0 | 1640 | `			}` |
|      18 | 1641 | `			if( kind == 1 ){` |
|       9 | 1642 | `				PH7_MemObjToInteger(pValue);` |
|       9 | 1643 | `				return SXRET_OK;` |
|       - | 1644 | `			}` |
|       4 | 1645 | `		}` |
|      33 | 1646 | `		if( bHasFloat ){` |
|      10 | 1647 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|     ! 0 | 1648 | `				PH7_MemObjToReal(pValue);` |
|     ! 0 | 1649 | `				return SXRET_OK;` |
|       - | 1650 | `			}` |
|      10 | 1651 | `			if( kind == 1 \|\| kind == 2 ){` |
|       7 | 1652 | `				PH7_MemObjToReal(pValue);` |
|       7 | 1653 | `				return SXRET_OK;` |
|       - | 1654 | `			}` |
|       1 | 1655 | `		}` |
|      26 | 1656 | `		if( bHasString ){` |
|     ! 0 | 1657 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|     ! 0 | 1658 | `				PH7_MemObjToString(pValue);` |
|     ! 0 | 1659 | `				return SXRET_OK;` |
|       - | 1660 | `			}` |
|     ! 0 | 1661 | `		}` |
|      26 | 1662 | `		if( bHasBool ){` |
|       3 | 1663 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|       3 | 1664 | `				PH7_MemObjToBool(pValue);` |
|       3 | 1665 | `				return SXRET_OK;` |
|       - | 1666 | `			}` |
|     ! 0 | 1667 | `		}` |
|       - | 1668 | `	}` |
|      24 | 1669 | `	return SXERR_INVALID;` |
|     172 | 1670 | `}` |
|       - | 1671 |  |
|       - | 1672 | `/*` |
|       - | 1673 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|       - | 1674 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|       - | 1675 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|       - | 1676 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|       - | 1677 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|       - | 1678 | ` */` |
|     326 | 1679 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|       5 | 1680 | `{` |
|       - | 1681 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|       - | 1682 | `	 * null value satisfies it (and a null value matches via the flag test` |
|       - | 1683 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|       - | 1684 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|       - | 1685 | `	 * silently swallow any argument. */` |
|     331 | 1686 | `	if( nType == MEMOBJ_NULL ){` |
|       3 | 1687 | `		return SXERR_INVALID;` |
|       - | 1688 | `	}` |
|       - | 1689 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|       - | 1690 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|       - | 1691 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|       - | 1692 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|       - | 1693 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|       - | 1694 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|       - | 1695 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|       - | 1696 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|       - | 1697 | `	 * object value against an array type is caught here too (never valid);` |
|       - | 1698 | `	 * object->scalar stays a separate case handled by the callers. */` |
|     329 | 1699 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|      37 | 1700 | `		return SXERR_INVALID;` |
|       - | 1701 | `	}` |
|     295 | 1702 | `	if( bStrict ){` |
|       - | 1703 | `		/* Only int -> float widening is allowed implicitly. */` |
|      36 | 1704 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|       3 | 1705 | `			PH7_MemObjToReal(pVal);` |
|       3 | 1706 | `			return SXRET_OK;` |
|       - | 1707 | `		}` |
|      34 | 1708 | `		return SXERR_INVALID;` |
|       - | 1709 | `	}` |
|       - | 1710 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|       - | 1711 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|       - | 1712 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|       - | 1713 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|       - | 1714 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|       - | 1715 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|       - | 1716 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     263 | 1717 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      20 | 1718 | `		return SXERR_INVALID;` |
|       - | 1719 | `	}` |
|       - | 1720 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|       - | 1721 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|       - | 1722 | `	 * is invoked by the string cast below). Every other scalar target —` |
|       - | 1723 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|       - | 1724 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|       - | 1725 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|       - | 1726 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|       - | 1727 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|       - | 1728 | `	 * is caught by the array guard.) */` |
|     245 | 1729 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      23 | 1730 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      28 | 1731 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      10 | 1732 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      13 | 1733 | `			return SXERR_INVALID;` |
|       - | 1734 | `		}` |
|       4 | 1735 | `	}` |
|     228 | 1736 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|     185 | 1737 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|     173 | 1738 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      50 | 1739 | `		return SXERR_INVALID;` |
|       - | 1740 | `	}` |
|     187 | 1741 | `	if( nType == MEMOBJ_INT && pVal->pVm ){` |
|       - | 1742 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion` |
|       - | 1743 | `		 * (typed params, returns, typed property stores all funnel through here);` |
|       - | 1744 | `		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly` |
|       - | 1745 | `		 * like the null / non-numeric-string cases above. An INTEGRAL float loses` |
|       - | 1746 | `		 * nothing and coerces normally. */` |
|      59 | 1747 | `		if( pVal->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1748 | `			ph7_real r = pVal->rVal;` |
|     ! 0 | 1749 | `			if( r != (ph7_real)(sxi64)r ){` |
|     ! 0 | 1750 | `				return SXERR_INVALID;` |
|     ! 0 | 1751 | `			}` |
|      59 | 1752 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       - | 1753 | `			SyString sStr;` |
|       - | 1754 | `			ph7_value sProbe;` |
|       - | 1755 | `			int bLossy;` |
|      55 | 1756 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|      55 | 1757 | `			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|      55 | 1758 | `			PH7_MemObjToNumeric(&sProbe);` |
|      55 | 1759 | `			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;` |
|      55 | 1760 | `			PH7_MemObjRelease(&sProbe);` |
|      55 | 1761 | `			if( bLossy ){` |
|     ! 0 | 1762 | `				return SXERR_INVALID;` |
|       - | 1763 | `			}` |
|      25 | 1764 | `		}` |
|      27 | 1765 | `	}` |
|       - | 1766 | `	{` |
|     187 | 1767 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|     187 | 1768 | `		if( xCast ) xCast(pVal);` |
|       - | 1769 | `	}` |
|     187 | 1770 | `	return SXRET_OK;` |
|     168 | 1771 | `}` |
|       - | 1772 |  |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|       - | 1775 | ` * TypeError message. Prefers the declared textual form when available.` |
|       - | 1776 | ` *` |
|       - | 1777 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|       - | 1778 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|       - | 1779 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|       - | 1780 | ` * back to a static literal and ignore zBuf entirely.` |
|       - | 1781 | ` */` |
|     202 | 1782 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|       5 | 1783 | `{` |
|     207 | 1784 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     207 | 1785 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     207 | 1786 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     207 | 1787 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     207 | 1788 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|     101 | 1789 | `		}` |
|     207 | 1790 | `		zBuf[nCopy] = 0;` |
|     207 | 1791 | `		return zBuf;` |
|       - | 1792 | `	}` |
|     ! 0 | 1793 | `	switch( nType ){` |
|     ! 0 | 1794 | `		case MEMOBJ_INT:     return "int";` |
|     ! 0 | 1795 | `		case MEMOBJ_REAL:    return "float";` |
|     ! 0 | 1796 | `		case MEMOBJ_STRING:  return "string";` |
|     ! 0 | 1797 | `		case MEMOBJ_BOOL:    return "bool";` |
|     ! 0 | 1798 | `		case MEMOBJ_HASHMAP: return "array";` |
|     ! 0 | 1799 | `		case MEMOBJ_OBJ:     return "object";` |
|     ! 0 | 1800 | `		default:             return "scalar";` |
|       - | 1801 | `	}` |
|     106 | 1802 | `}` |
|       - | 1803 |  |
|       - | 1804 | `/*` |
|       - | 1805 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|       - | 1806 | ` * the way php writes it in a TypeError:` |
|       - | 1807 | ` *` |
|       - | 1808 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|       - | 1809 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|       - | 1810 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|       - | 1811 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|       - | 1812 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|       - | 1813 | ` *` |
|       - | 1814 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|       - | 1815 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|       - | 1816 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|       - | 1817 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|       - | 1818 | ` */` |
|     168 | 1819 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|       - | 1820 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|       5 | 1821 | `{` |
|     173 | 1822 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|       - | 1823 | `	sxu32 nCopy;` |
|     173 | 1824 | `	sxu32 nAt = 0;` |
|     173 | 1825 | `	if( nBuf == 0 ){` |
|     ! 0 | 1826 | `		return "";` |
|       - | 1827 | `	}` |
|     168 | 1828 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|      73 | 1829 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|      15 | 1830 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|      15 | 1831 | `		nCopy = SyStrlen(zIter);` |
|      15 | 1832 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      15 | 1833 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|      15 | 1834 | `		zBuf[nCopy] = 0;` |
|      15 | 1835 | `		return zBuf;` |
|       - | 1836 | `	}` |
|     161 | 1837 | `	if( bNullable && nBuf > 1 ){` |
|      23 | 1838 | `		zBuf[nAt++] = '?';` |
|      10 | 1839 | `	}` |
|     161 | 1840 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|     161 | 1841 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|     161 | 1842 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|     161 | 1843 | `	zBuf[nAt + nCopy] = 0;` |
|     161 | 1844 | `	return zBuf;` |
|      89 | 1845 | `}` |
|       - | 1846 |  |
|       - | 1847 | `/*` |
|       - | 1848 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|       - | 1849 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|       - | 1850 | ` */` |
|     140 | 1851 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|       5 | 1852 | `{` |
|     145 | 1853 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     215 | 1854 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|     140 | 1855 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|     145 | 1856 | `	return zBuf;` |
|       5 | 1857 | `}` |
|       - | 1858 |  |
|  108042 | 1859 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|       5 | 1860 | `{` |
|       - | 1861 | `	SyHashEntry *pSlot;` |
|       - | 1862 | `	VmClassAttr *pVmAttr;` |
|       - | 1863 | `	ph7_class_attr *pAttr;` |
|       - | 1864 | `	ph7_class *pHintScope;` |
|       - | 1865 | `	char zGivenBuf[128];` |
|       - | 1866 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|       - | 1867 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|       - | 1868 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|       - | 1869 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|  108047 | 1870 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|  108047 | 1871 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  108047 | 1872 | `	if( pSlot == 0 ){` |
|    7355 | 1873 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1874 | `	}` |
|  100697 | 1875 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|  100697 | 1876 | `	pAttr = pVmAttr->pAttr;` |
|  100697 | 1877 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1878 | `		return SXRET_OK;` |
|       - | 1879 | `	}` |
|       - | 1880 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|       - | 1881 | `	 * the property (a trait's members count as the composing class), not the` |
|       - | 1882 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|  100697 | 1883 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|       - | 1884 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|       - | 1885 | `	 * property may be written exactly once and only from within the declaring` |
|       - | 1886 | `	 * class scope (its set-scope is protected). */` |
|  100697 | 1887 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1888 | `		/* A readonly property is always typed and default-less, so it starts` |
|       - | 1889 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|       - | 1890 | `		 * write below — making it the write-once latch (a type-rejected write` |
|       - | 1891 | `		 * leaves it set, so a later valid initialization still works). */` |
|      99 | 1892 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|       - | 1893 | `			/* Already initialized: any further write is forbidden, any scope —` |
|       - | 1894 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|       - | 1895 | `			 * Exceptions that fall through to the set-scope check below:` |
|       - | 1896 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|       - | 1897 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|       - | 1898 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|      33 | 1899 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      33 | 1900 | `			if( !(pCloneFr && pCloneFr->pThis` |
|      16 | 1901 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|      31 | 1902 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|       - | 1903 | `			}` |
|       1 | 1904 | `		}` |
|      34 | 1905 | `	}` |
|  100671 | 1906 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|       - | 1907 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|       - | 1908 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|      27 | 1909 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|      27 | 1910 | `		if( rcVis != SXRET_OK ){` |
|      13 | 1911 | `			return rcVis;` |
|       1 | 1912 | `		}` |
|  100652 | 1913 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1914 | `		/* First write (or a clone re-init) must come from within the declaring` |
|       - | 1915 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      71 | 1916 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      71 | 1917 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1918 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|       - | 1919 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|       - | 1920 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|      71 | 1921 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|       5 | 1922 | `			pDecl = pVmAttr->pOwner;` |
|       2 | 1923 | `		}` |
|      71 | 1924 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|       6 | 1925 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|       - | 1926 | `		}` |
|      31 | 1927 | `	}` |
|       - | 1928 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|       - | 1929 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|       - | 1930 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|       - | 1931 | `	 * is its TypeError there), which this used to deny outright. */` |
|  100655 | 1932 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      88 | 1933 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|      56 | 1934 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|      28 | 1935 | `			bStrict,pHintScope);` |
|      60 | 1936 | `		if( rc == SXRET_OK ){` |
|      38 | 1937 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      38 | 1938 | `			return SXRET_OK;` |
|       - | 1939 | `		}` |
|      26 | 1940 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1941 | `			char zBuf[128];` |
|      21 | 1942 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 1943 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 1944 | `		}` |
|      14 | 1945 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1946 | `	}` |
|       - | 1947 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|       - | 1948 | `	 * includes null). */` |
|  100599 | 1949 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      30 | 1950 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|      25 | 1951 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       2 | 1952 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|      24 | 1953 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      24 | 1954 | `			return SXRET_OK;` |
|       - | 1955 | `		}` |
|      12 | 1956 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|       - | 1957 | `	}` |
|       - | 1958 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|       - | 1959 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|       - | 1960 | `	 * type error. */` |
|  100569 | 1961 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 1962 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1963 | `	}` |
|       - | 1964 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|       - | 1965 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|       - | 1966 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|  100569 | 1967 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      12 | 1968 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       5 | 1969 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       5 | 1970 | `			return SXRET_OK;` |
|       - | 1971 | `		}` |
|       7 | 1972 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1973 | `	}` |
|       - | 1974 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|       - | 1975 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|       - | 1976 | `	 * handled by the nullable check above). Checked by value before the generic` |
|       - | 1977 | `	 * class-instanceof branch, which would resolve no such class and then` |
|       - | 1978 | `	 * wrongly accept any object / reject arrays. */` |
|  100559 | 1979 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      73 | 1980 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|      73 | 1981 | `		if( rcPseudo == 1 ){` |
|      13 | 1982 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      13 | 1983 | `			return SXRET_OK;` |
|       - | 1984 | `		}` |
|      61 | 1985 | `		if( rcPseudo == 0 ){` |
|       3 | 1986 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1987 | `		}` |
|       - | 1988 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|      27 | 1989 | `	}` |
|  100545 | 1990 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       - | 1991 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|       - | 1992 | `		 * class (pHintScope), not the instance's runtime class. */` |
|      59 | 1993 | `		ph7_class *pExpected = 0;` |
|      59 | 1994 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|       - | 1995 | `			char zBuf[128];` |
|      32 | 1996 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      18 | 1997 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      18 | 1998 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|     ! 0 | 1999 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2000 | `		}` |
|      40 | 2001 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      40 | 2002 | `		return SXRET_OK;` |
|       - | 2003 | `	}` |
|       - | 2004 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|       - | 2005 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|       - | 2006 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|       - | 2007 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|       - | 2008 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|  100491 | 2009 | `	if( bStrict ){` |
|       - | 2010 | `		int bOk;` |
|      29 | 2011 | `		if( ph7_value_is_bool(pValue) ){` |
|       5 | 2012 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|      27 | 2013 | `		}else if( ph7_value_is_float(pValue) ){` |
|       3 | 2014 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|      24 | 2015 | `		}else if( ph7_value_is_int(pValue) ){` |
|       9 | 2016 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|      19 | 2017 | `		}else if( ph7_value_is_string(pValue) ){` |
|      15 | 2018 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|       8 | 2019 | `		}else{` |
|       - | 2020 | `			/* array / resource / an object against a scalar type: no coercion in` |
|       - | 2021 | `			 * either mode, so the flag test is the whole answer (an object never` |
|       - | 2022 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|       - | 2023 | `			 * does not perform). */` |
|     ! 0 | 2024 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|       - | 2025 | `		}` |
|      29 | 2026 | `		if( !bOk ){` |
|       - | 2027 | `			char zObjBuf[128];` |
|      31 | 2028 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      20 | 2029 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|     ! 0 | 2030 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|      20 | 2031 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2032 | `		}` |
|       9 | 2033 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|       3 | 2034 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|       2 | 2035 | `		}else{` |
|       7 | 2036 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 2037 | `		}` |
|       9 | 2038 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       9 | 2039 | `		return SXRET_OK;` |
|       - | 2040 | `	}` |
|       - | 2041 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|       - | 2042 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|       - | 2043 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|       - | 2044 | `	 * via __toString, so fall through to the string cast below. */` |
|  100463 | 2045 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      18 | 2046 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      20 | 2047 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       4 | 2048 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       - | 2049 | `			char zBuf[128];` |
|      22 | 2050 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 2051 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2052 | `		}` |
|       1 | 2053 | `	}` |
|  100451 | 2054 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|  100071 | 2055 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|  100071 | 2056 | `		if( xCast ){` |
|       - | 2057 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|  100071 | 2058 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       8 | 2059 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2060 | `			}` |
|  100065 | 2061 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 2062 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2063 | `			}` |
|       - | 2064 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|       - | 2065 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|       - | 2066 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|  100054 | 2067 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|  100050 | 2068 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|  100055 | 2069 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|  100037 | 2070 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|       - | 2071 | `			}` |
|      25 | 2072 | `			xCast(pValue);` |
|      11 | 2073 | `		}` |
|      14 | 2074 | `	}else{` |
|       - | 2075 | `		/* Mask matched — an int property accepting a whole-real must` |
|       - | 2076 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|     385 | 2077 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 2078 | `	}` |
|     407 | 2079 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     407 | 2080 | `	return SXRET_OK;` |
|   54026 | 2081 | `}` |
|       - | 2082 | `/*` |
|       - | 2083 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|       - | 2084 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|       - | 2085 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|       - | 2086 | ` *     a scope that could normally reach it — else a catchable Error),` |
|       - | 2087 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|       - | 2088 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|       - | 2089 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|       - | 2090 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|       - | 2091 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|       - | 2092 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|       - | 2093 | ` */` |
|      30 | 2094 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|       - | 2095 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|       1 | 2096 | `{` |
|      31 | 2097 | `	ph7_class *pClass = pClone->pClass;` |
|       - | 2098 | `	SyHashEntry *pEntry;` |
|       - | 2099 | `	VmClassAttr *pVmAttr;` |
|       - | 2100 | `	ph7_class_attr *pAttr;` |
|       - | 2101 | `	ph7_value *pSlot;` |
|       - | 2102 | `	sxi32 rc;` |
|      31 | 2103 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|      31 | 2104 | `	if( pEntry == 0 ){` |
|       - | 2105 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|       - | 2106 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|       - | 2107 | `		 * deferred residual). */` |
|     ! 0 | 2108 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|     ! 0 | 2109 | `		if( pSlot == 0 ){` |
|     ! 0 | 2110 | `			return PH7_VmMemoryError(pVm);` |
|       - | 2111 | `		}` |
|     ! 0 | 2112 | `		PH7_MemObjStore(pValue,pSlot);` |
|     ! 0 | 2113 | `		return SXRET_OK;` |
|       - | 2114 | `	}` |
|      31 | 2115 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      31 | 2116 | `	pAttr = pVmAttr->pAttr;` |
|       - | 2117 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|      31 | 2118 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - | 2119 | `		SyBlob sMsg;` |
|     ! 0 | 2120 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     ! 0 | 2121 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|     ! 0 | 2122 | `			&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2123 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2124 | `	}` |
|       - | 2125 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|       - | 2126 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|       - | 2127 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|      31 | 2128 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       5 | 2129 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|       5 | 2130 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 2131 | `		SyBlob sMsg;` |
|       5 | 2132 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       5 | 2133 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|       5 | 2134 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2135 | `	}` |
|       - | 2136 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|      27 | 2137 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|      27 | 2138 | `	if( rc != SXRET_OK ){` |
|       3 | 2139 | `		return rc;` |
|       - | 2140 | `	}` |
|       - | 2141 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|      25 | 2142 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|      25 | 2143 | `	if( pSlot ){` |
|      25 | 2144 | `		PH7_MemObjStore(pValue,pSlot);` |
|      12 | 2145 | `	}` |
|      25 | 2146 | `	return SXRET_OK;` |
|      16 | 2147 | `}` |
|       - | 2148 | `/*` |
|       - | 2149 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|       - | 2150 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|       - | 2151 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|       - | 2152 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|       - | 2153 | ` */` |
|      10 | 2154 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       2 | 2155 | `{` |
|      12 | 2156 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2157 | `	char zBuf[128],zType[192];` |
|       - | 2158 | `	const char *zGiven;` |
|      17 | 2159 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       5 | 2160 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|      12 | 2161 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2162 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2163 | `	}else{` |
|      12 | 2164 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2165 | `	}` |
|      12 | 2166 | `	if( bLazy ){` |
|       - | 2167 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|       - | 2168 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|       - | 2169 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|       - | 2170 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|       - | 2171 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|       - | 2172 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|       - | 2173 | `		 * re-evaluates and re-raises, as php's does. */` |
|       - | 2174 | `		SyBlob sMsg;` |
|       5 | 2175 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       5 | 2176 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|       2 | 2177 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       5 | 2178 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - | 2179 | `	}` |
|       - | 2180 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|       - | 2181 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|       - | 2182 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|       - | 2183 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|       - | 2184 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|       - | 2185 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|       - | 2186 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       8 | 2187 | `	if( pVm->sCodeGen.xErr ){` |
|       7 | 2188 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|       - | 2189 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       2 | 2190 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       3 | 2191 | `	}else{` |
|       4 | 2192 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 2193 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       1 | 2194 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       - | 2195 | `	}` |
|       8 | 2196 | `	pVm->iExitStatus = 255;` |
|       8 | 2197 | `	pVm->bHaltRequested = 1;` |
|       8 | 2198 | `	return SXERR_ABORT;` |
|       7 | 2199 | `}` |
|       - | 2200 | `/*` |
|       - | 2201 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|       - | 2202 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|       - | 2203 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|       - | 2204 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|       - | 2205 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|       - | 2206 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|       - | 2207 | ` */` |
|      42 | 2208 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       3 | 2209 | `{` |
|      45 | 2210 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|       - | 2211 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|      45 | 2212 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 2213 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       3 | 2214 | `			return SXRET_OK;` |
|       - | 2215 | `		}` |
|     ! 0 | 2216 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|     ! 0 | 2217 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 2218 | `			return SXRET_OK;` |
|       - | 2219 | `		}` |
|     ! 0 | 2220 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2221 | `	}` |
|       - | 2222 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|      43 | 2223 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       6 | 2224 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       5 | 2225 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       5 | 2226 | `			return SXRET_OK;` |
|       - | 2227 | `		}` |
|     ! 0 | 2228 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2229 | `	}` |
|       - | 2230 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|      39 | 2231 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2232 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2233 | `	}` |
|       - | 2234 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|      39 | 2235 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2236 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2237 | `			return SXRET_OK;` |
|       - | 2238 | `		}` |
|     ! 0 | 2239 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2240 | `	}` |
|       - | 2241 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|       - | 2242 | `	 * a real class/interface verified by instanceof. */` |
|      39 | 2243 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       3 | 2244 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       3 | 2245 | `		if( rcPseudo == 1 ){` |
|     ! 0 | 2246 | `			return SXRET_OK;` |
|       - | 2247 | `		}` |
|       3 | 2248 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2249 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2250 | `		}` |
|       - | 2251 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|       - | 2252 | `		 * self/parent resolve against the declaring class. */` |
|       - | 2253 | `		{` |
|       3 | 2254 | `			ph7_class *pExpected = 0;` |
|       4 | 2255 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|       1 | 2256 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|       3 | 2257 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2258 | `			}` |
|       - | 2259 | `		}` |
|     ! 0 | 2260 | `		return SXRET_OK;` |
|       - | 2261 | `	}` |
|       - | 2262 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|       - | 2263 | `	 * implicit widening. Everything else is a type error.` |
|       - | 2264 | `	 *` |
|       - | 2265 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|       - | 2266 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|       - | 2267 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|       - | 2268 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|       - | 2269 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|       - | 2270 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|       - | 2271 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|       - | 2272 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|       - | 2273 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|       - | 2274 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|       - | 2275 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|      37 | 2276 | `	if( pValue->iFlags & pAttr->nType ){` |
|      25 | 2277 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      25 | 2278 | `		return SXRET_OK;` |
|       - | 2279 | `	}` |
|      13 | 2280 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       3 | 2281 | `		PH7_MemObjToReal(pValue);` |
|       3 | 2282 | `		return SXRET_OK;` |
|       - | 2283 | `	}` |
|      10 | 2284 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|      24 | 2285 | `}` |
|       - | 2286 | `/*` |
|       - | 2287 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|       - | 2288 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|       - | 2289 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|       - | 2290 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|       - | 2291 | ` */` |
|      34 | 2292 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       2 | 2293 | `{` |
|      36 | 2294 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2295 | `	const char *zGiven;` |
|       - | 2296 | `	char zBuf[128],zType[192];` |
|      53 | 2297 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|      17 | 2298 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|       - | 2299 | `	SyBlob sMsg;` |
|      36 | 2300 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2301 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2302 | `	}else{` |
|      36 | 2303 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2304 | `	}` |
|      36 | 2305 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      36 | 2306 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|      17 | 2307 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|      36 | 2308 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       2 | 2309 | `}` |
|       - | 2310 | `/*` |
|       - | 2311 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|       - | 2312 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|       - | 2313 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|       - | 2314 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|       - | 2315 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|       - | 2316 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|       - | 2317 | ` * instance defaults per-instantiation. Matching structure of` |
|       - | 2318 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|       - | 2319 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|       - | 2320 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|       - | 2321 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|       - | 2322 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|       - | 2323 | ` */` |
|       - | 2324 | `/*` |
|       - | 2325 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|       - | 2326 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|       - | 2327 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|       - | 2328 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|       - | 2329 | ` * throwing, so the static-property mount path can defer the failure (php` |
|       - | 2330 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|       - | 2331 | ` * instance path throws immediately via the wrapper below.` |
|       - | 2332 | ` */` |
|     340 | 2333 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2334 | `{` |
|     345 | 2335 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|     345 | 2336 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      55 | 2337 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      51 | 2338 | `			return SXRET_OK;` |
|       - | 2339 | `		}` |
|       4 | 2340 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       4 | 2341 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|       3 | 2342 | `			return SXRET_OK;` |
|       - | 2343 | `		}` |
|       3 | 2344 | `		return SXERR_INVALID;` |
|       - | 2345 | `	}` |
|     293 | 2346 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      39 | 2347 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|      31 | 2348 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|      31 | 2349 | `			return SXRET_OK;` |
|       - | 2350 | `		}` |
|     ! 0 | 2351 | `		return SXERR_INVALID;` |
|       - | 2352 | `	}` |
|     267 | 2353 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2354 | `		return SXERR_INVALID;` |
|       - | 2355 | `	}` |
|     267 | 2356 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2357 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2358 | `			return SXRET_OK;` |
|       - | 2359 | `		}` |
|     ! 0 | 2360 | `		return SXERR_INVALID;` |
|       - | 2361 | `	}` |
|     267 | 2362 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       5 | 2363 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       5 | 2364 | `		if( rcPseudo == 1 ){` |
|       5 | 2365 | `			return SXRET_OK;` |
|       - | 2366 | `		}` |
|     ! 0 | 2367 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2368 | `			return SXERR_INVALID;` |
|       - | 2369 | `		}` |
|       - | 2370 | `		{` |
|       - | 2371 | `			/* self/parent in the hint resolve against the declaring class. */` |
|     ! 0 | 2372 | `			ph7_class *pExpected = 0;` |
|     ! 0 | 2373 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|     ! 0 | 2374 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|     ! 0 | 2375 | `				return SXERR_INVALID;` |
|       - | 2376 | `			}` |
|       - | 2377 | `		}` |
|     ! 0 | 2378 | `		return SXRET_OK;` |
|       - | 2379 | `	}` |
|     263 | 2380 | `	if( pValue->iFlags & pAttr->nType ){` |
|     227 | 2381 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|     227 | 2382 | `		return SXRET_OK;` |
|       - | 2383 | `	}` |
|      38 | 2384 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       6 | 2385 | `		PH7_MemObjToReal(pValue);` |
|       6 | 2386 | `		return SXRET_OK;` |
|       - | 2387 | `	}` |
|      34 | 2388 | `	return SXERR_INVALID;` |
|     175 | 2389 | `}` |
|     290 | 2390 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2391 | `{` |
|     295 | 2392 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|     283 | 2393 | `		return SXRET_OK;` |
|       - | 2394 | `	}` |
|      13 | 2395 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|     150 | 2396 | `}` |
|       - | 2397 | `/*` |
|       - | 2398 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|       - | 2399 | ` * class chain for a static typed slot whose mount-time default failed its` |
|       - | 2400 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|       - | 2401 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|       - | 2402 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|       - | 2403 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|       - | 2404 | ` * never-touched class stays silent, and the throw repeats on every access` |
|       - | 2405 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|       - | 2406 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|       - | 2407 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|       - | 2408 | ` */` |
|      26 | 2409 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2410 | `{` |
|       - | 2411 | `	ph7_class *pScan;` |
|      33 | 2412 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       - | 2413 | `		SyHashEntry *pEntry;` |
|      29 | 2414 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|      33 | 2415 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|      29 | 2416 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      26 | 2417 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|       - | 2418 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|      24 | 2419 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      25 | 2420 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|      24 | 2421 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      24 | 2422 | `				if( pSlot ){` |
|      24 | 2423 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      24 | 2424 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|      24 | 2425 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       - | 2426 | `						ph7_value sNull;` |
|      24 | 2427 | `						if( pValue == 0 ){` |
|     ! 0 | 2428 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|     ! 0 | 2429 | `							pValue = &sNull;` |
|     ! 0 | 2430 | `						}` |
|      24 | 2431 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|       - | 2432 | `					}` |
|     ! 0 | 2433 | `				}` |
|     ! 0 | 2434 | `			}` |
|       1 | 2435 | `		}` |
|       3 | 2436 | `	}` |
|       5 | 2437 | `	return SXRET_OK;` |
|      16 | 2438 | `}` |
|       - | 2439 | `/*` |
|       - | 2440 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|       - | 2441 | ` * materialization: an initializer that threw at mount and was deferred` |
|       - | 2442 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|       - | 2443 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|       - | 2444 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|       - | 2445 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|       - | 2446 | ` * a subclass can be mounted before the base whose default failed.` |
|       - | 2447 | ` */` |
| 2213684 | 2448 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|       5 | 2449 | `{` |
| 4430927 | 2450 | `	while( pClass ){` |
| 2217325 | 2451 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|      85 | 2452 | `			return 1;` |
|       - | 2453 | `		}` |
| 2217243 | 2454 | `		pClass = pClass->pBase;` |
|       5 | 2455 | `	}` |
| 2213607 | 2456 | `	return 0;` |
| 1106847 | 2457 | `}` |
|       - | 2458 | `/*` |
|       - | 2459 | ` * Re-run the initializers of the static properties whose evaluation was` |
|       - | 2460 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|       - | 2461 | ` *` |
|       - | 2462 | ` * php builds a class's static table on first use, evaluating each slot's` |
|       - | 2463 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|       - | 2464 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|       - | 2465 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|       - | 2466 | ` * the access: a constant define()d after the class declaration resolves.` |
|       - | 2467 | ` *` |
|       - | 2468 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|       - | 2469 | ` * the base's broken default, not its own), then declaration order within a` |
|       - | 2470 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|       - | 2471 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|       - | 2472 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|       - | 2473 | ` * re-raises on every access too.` |
|       - | 2474 | ` */` |
|      86 | 2475 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|       3 | 2476 | `{` |
|       - | 2477 | `	SyHashEntry *pEntry;` |
|       - | 2478 | `	sxi32 rc;` |
|      89 | 2479 | `	if( pClass->pBase ){` |
|       6 | 2480 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|       6 | 2481 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2482 | `			return rc;` |
|       - | 2483 | `		}` |
|       2 | 2484 | `	}` |
|      89 | 2485 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     209 | 2486 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     123 | 2487 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     123 | 2488 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|       - | 2489 | `			/* Not pending. An inherited slot the base pass already collected` |
|       - | 2490 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|       - | 2491 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|      61 | 2492 | `			continue;` |
|       - | 2493 | `		}` |
|      65 | 2494 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - | 2495 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|       - | 2496 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|       - | 2497 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|       - | 2498 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|       - | 2499 | `			 * class keeps its hint flag so a later access retries. */` |
|     ! 0 | 2500 | `			*pbLeft = 1;` |
|     ! 0 | 2501 | `			continue;` |
|       - | 2502 | `		}` |
|      65 | 2503 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|      65 | 2504 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2505 | `			return rc;` |
|       - | 2506 | `		}` |
|       3 | 2507 | `	}` |
|      89 | 2508 | `	return SXRET_OK;` |
|      46 | 2509 | `}` |
|       - | 2510 | `/*` |
|       - | 2511 | ` * Re-run the initializers of the static properties whose evaluation was` |
|       - | 2512 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|       - | 2513 | ` *` |
|       - | 2514 | ` * php builds a class's static table on first use, evaluating each slot's` |
|       - | 2515 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|       - | 2516 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|       - | 2517 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|       - | 2518 | ` * the access: a constant define()d after the class declaration resolves.` |
|       - | 2519 | ` *` |
|       - | 2520 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|       - | 2521 | ` * the base's broken default, not its own), then declaration order within a` |
|       - | 2522 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|       - | 2523 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|       - | 2524 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|       - | 2525 | ` * re-raises on every access too.` |
|       - | 2526 | ` *` |
|       - | 2527 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|       - | 2528 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|       - | 2529 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|       - | 2530 | ` * cut the outer one short.` |
|       - | 2531 | ` */` |
|      82 | 2532 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|       3 | 2533 | `{` |
|       - | 2534 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|       - | 2535 | `	ph7_class_attr **apPending;` |
|       - | 2536 | `	sxu32 n,nUsed;` |
|       - | 2537 | `	sxi32 rc;` |
|      85 | 2538 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|      85 | 2539 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|      85 | 2540 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|      85 | 2541 | `	nUsed = SySetUsed(&aPending);` |
|      89 | 2542 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|      63 | 2543 | `		ph7_class_attr *pAttr = apPending[n];` |
|      63 | 2544 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2545 | `		ph7_class *pSaveCtx;` |
|       - | 2546 | `		void *pSaveFrame;` |
|       - | 2547 | `		sxu32 nSaveLazyLine;` |
|       - | 2548 | `		sxi32 nSaveLazyDepth;` |
|       - | 2549 | `		ph7_value *pMemObj;` |
|       - | 2550 | `		sxi32 rcExec;` |
|      63 | 2551 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|     ! 0 | 2552 | `			continue; /* the base pass already ran this shared slot */` |
|       - | 2553 | `		}` |
|      63 | 2554 | `		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      63 | 2555 | `		if( pMemObj == 0 ){` |
|     ! 0 | 2556 | `			continue;` |
|       - | 2557 | `		}` |
|      63 | 2558 | `		pSaveCtx = pVm->pConstEvalClass;` |
|      63 | 2559 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|      63 | 2560 | `		pVm->pConstEvalClass = pOwner;` |
|       - | 2561 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|       - | 2562 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|       - | 2563 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|       - | 2564 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|       - | 2565 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|      63 | 2566 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - | 2567 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|       - | 2568 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|      63 | 2569 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      63 | 2570 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      63 | 2571 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      63 | 2572 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      63 | 2573 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|      63 | 2574 | `		pVm->nConstEvalDepth++;` |
|      63 | 2575 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      63 | 2576 | `		pVm->nConstEvalDepth--;` |
|      63 | 2577 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      63 | 2578 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      63 | 2579 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      63 | 2580 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      63 | 2581 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      63 | 2582 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - | 2583 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|       - | 2584 | `			 * caller to route (a catch here is the user's own). */` |
|      59 | 2585 | `			rc = rcExec;` |
|      59 | 2586 | `			break;` |
|       - | 2587 | `		}` |
|       5 | 2588 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|       5 | 2589 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|       - | 2590 | `			/* The initializer named a self-referencing constant. Like the mount` |
|       - | 2591 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|       - | 2592 | `			 * the access, where a catch can see it. */` |
|     ! 0 | 2593 | `			rc = VmConstCycleThrow(&(*pVm));` |
|     ! 0 | 2594 | `			break;` |
|       - | 2595 | `		}` |
|       4 | 2596 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       3 | 2597 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - | 2598 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|       - | 2599 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|       - | 2600 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|     ! 0 | 2601 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|     ! 0 | 2602 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|     ! 0 | 2603 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     ! 0 | 2604 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|     ! 0 | 2605 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|     ! 0 | 2606 | `				break;` |
|       - | 2607 | `			}` |
|     ! 0 | 2608 | `		}` |
|       3 | 2609 | `	}` |
|      85 | 2610 | `	if( rc != SXRET_OK ){` |
|      59 | 2611 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|      28 | 2612 | `	}` |
|      85 | 2613 | `	SySetRelease(&aPending);` |
|      85 | 2614 | `	return rc;` |
|       3 | 2615 | `}` |
|       - | 2616 | `/*` |
|       - | 2617 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|       - | 2618 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|       - | 2619 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|       - | 2620 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|       - | 2621 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|       - | 2622 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|       - | 2623 | ` */` |
|      82 | 2624 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2625 | `{` |
|      85 | 2626 | `	int bLeft = 0;` |
|      85 | 2627 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|      85 | 2628 | `	if( rc == SXRET_OK ){` |
|      29 | 2629 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|      13 | 2630 | `	}` |
|      85 | 2631 | `	if( rc == SXRET_OK && !bLeft ){` |
|       - | 2632 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|       - | 2633 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|       - | 2634 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|       - | 2635 | `		 * set — php's materialization keeps failing too. */` |
|       - | 2636 | `		ph7_class *pScan;` |
|       9 | 2637 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       5 | 2638 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|       3 | 2639 | `		}` |
|       2 | 2640 | `	}` |
|      85 | 2641 | `	return rc;` |
|       3 | 2642 | `}` |
|       - | 2643 |  |
|       - | 2644 | `/*` |
|       - | 2645 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 2646 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 2647 | ` * information.` |
|       - | 2648 | ` * ------------------------------------` |
|       - | 2649 | ` * Simple boring wrapper function.` |
|       - | 2650 | ` * ------------------------------------` |
|       - | 2651 | ` */` |
|     464 | 2652 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|       5 | 2653 | `{` |
|       - | 2654 | `	va_list ap;` |
|       - | 2655 | `	sxi32 rc;` |
|     469 | 2656 | `	va_start(ap,zFormat);` |
|     469 | 2657 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     469 | 2658 | `	va_end(ap);` |
|     469 | 2659 | `	return rc;` |
|       5 | 2660 | `}` |
|       - | 2661 | `/*` |
|       - | 2662 | ` * Throw a TypeError exception from within the VM execution loop.` |
|       - | 2663 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|       - | 2664 | ` */` |
|     376 | 2665 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|       5 | 2666 | `{` |
|       - | 2667 | `	ph7_class *pClass;` |
|       - | 2668 | `	ph7_class_instance *pThis;` |
|       - | 2669 | `	ph7_class_method *pCons;` |
|       - | 2670 | `	ph7_value sArg;` |
|       - | 2671 | `	ph7_value *apArg[1];` |
|       - | 2672 | `	SyBlob sMsg;` |
|       - | 2673 | `	SyString sMsgStr;` |
|     381 | 2674 | `	SyString *pFuncName = &pCallee->sName;` |
|       - | 2675 | `	VmFrame *pFrame;` |
|       - | 2676 | `	sxi32 rc;` |
|     381 | 2677 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     381 | 2678 | `	if( pClass == 0 ){` |
|     ! 0 | 2679 | `		return PH7_ABORT;` |
|       - | 2680 | `	}` |
|     381 | 2681 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     381 | 2682 | `	if( pThis == 0 ){` |
|     ! 0 | 2683 | `		return PH7_ABORT;` |
|       - | 2684 | `	}` |
|     381 | 2685 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2686 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|       - | 2687 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|       - | 2688 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|       - | 2689 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|       - | 2690 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|     567 | 2691 | `	if( pOwnerClass ){` |
|       - | 2692 | `		/* A property hook is named after its PROPERTY, never after the method` |
|       - | 2693 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|       - | 2694 | `		SyBlob sHook;` |
|      41 | 2695 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|      41 | 2696 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|       6 | 2697 | `			if( pArgName ){` |
|       6 | 2698 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|       4 | 2699 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|       2 | 2700 | `					nArg,pArgName,zExpected,zGiven);` |
|       4 | 2701 | `			}else{` |
|     ! 0 | 2702 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|     ! 0 | 2703 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|     ! 0 | 2704 | `					nArg,zExpected,zGiven);` |
|       - | 2705 | `			}` |
|       6 | 2706 | `			SyBlobRelease(&sHook);` |
|       6 | 2707 | `			goto ArgMsgBuilt;` |
|       - | 2708 | `		}` |
|      37 | 2709 | `		SyBlobRelease(&sHook);` |
|      37 | 2710 | `		if( pArgName ){` |
|      28 | 2711 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|      12 | 2712 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|      16 | 2713 | `		}else{` |
|      12 | 2714 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|       4 | 2715 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|       - | 2716 | `		}` |
|      21 | 2717 | `	}else{` |
|       - | 2718 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|     345 | 2719 | `		const char *zShow = 0;` |
|     345 | 2720 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     345 | 2721 | `		if( pArgName ){` |
|     277 | 2722 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|     136 | 2723 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|     141 | 2724 | `		}else{` |
|      72 | 2725 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      34 | 2726 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|       - | 2727 | `		}` |
|       - | 2728 | `	}` |
|     188 | 2729 | `ArgMsgBuilt:` |
|       - | 2730 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|       - | 2731 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|       - | 2732 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|     381 | 2733 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|     375 | 2734 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     375 | 2735 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|     375 | 2736 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|     185 | 2737 | `		}` |
|     185 | 2738 | `	}` |
|     381 | 2739 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     381 | 2740 | `	if( pCons ){` |
|     381 | 2741 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     381 | 2742 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     381 | 2743 | `		apArg[0] = &sArg;` |
|     381 | 2744 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     381 | 2745 | `		PH7_MemObjRelease(&sArg);` |
|     188 | 2746 | `	}` |
|     381 | 2747 | `	SyBlobRelease(&sMsg);` |
|     381 | 2748 | `	pFrame = pVm->pFrame;` |
|     381 | 2749 | `	if( pFrame ){` |
|     381 | 2750 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     381 | 2751 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     188 | 2752 | `	}` |
|     381 | 2753 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     381 | 2754 | `	PH7_ClassInstanceUnref(pThis);` |
|     381 | 2755 | `	if( rc == SXERR_ABORT ){` |
|       6 | 2756 | `		return PH7_ABORT;` |
|       - | 2757 | `	}` |
|     377 | 2758 | `	return PH7_EXCEPTION;` |
|     193 | 2759 | `}` |
|       - | 2760 | `/*` |
|       - | 2761 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|       - | 2762 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|       - | 2763 | ` * positional and the named-argument binding paths of OP_CALL.` |
|       - | 2764 | ` *` |
|       - | 2765 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|       - | 2766 | ` * element uses its overall call position; a NAMED element always reports` |
|       - | 2767 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|       - | 2768 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|       - | 2769 | ` * formal, so no single parameter name applies.` |
|       - | 2770 | ` *` |
|       - | 2771 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|       - | 2772 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|       - | 2773 | ` */` |
|    2144 | 2774 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|       - | 2775 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|       5 | 2776 | `{` |
|       - | 2777 | `	sxi32 rc;` |
|    2149 | 2778 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|      33 | 2779 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|      37 | 2780 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|       - | 2781 | `			const char *zGiven;` |
|      11 | 2782 | `			const char *zExpected = "union";` |
|       - | 2783 | `			char zBuf[128];` |
|       - | 2784 | `			char zTypeBuf[128];` |
|      11 | 2785 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 2786 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      10 | 2787 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2788 | `				zGiven = "null";` |
|     ! 0 | 2789 | `			}else{` |
|       9 | 2790 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 2791 | `			}` |
|      11 | 2792 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|      15 | 2793 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|       4 | 2794 | `					zTypeBuf,sizeof(zTypeBuf));` |
|       4 | 2795 | `			}` |
|      11 | 2796 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|      11 | 2797 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2798 | `		}` |
|      17 | 2799 | `		return SXRET_OK;` |
|       - | 2800 | `	}` |
|    2122 | 2801 | `	if( pFormal->nType < 1` |
|    1173 | 2802 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|    1925 | 2803 | `		return SXRET_OK;` |
|       - | 2804 | `	}` |
|     207 | 2805 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2806 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|       - | 2807 | `		 * per element exactly like the non-variadic paths. */` |
|      61 | 2808 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2809 | `		ph7_class *pClass;` |
|      61 | 2810 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      61 | 2811 | `		if( rcPseudo == 0 ){` |
|       - | 2812 | `			/* Recognised pseudo-type; value mismatches */` |
|       - | 2813 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      14 | 2814 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       6 | 2815 | `				VmClassHintTypeName(pName,0,` |
|       6 | 2816 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       3 | 2817 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       8 | 2818 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2819 | `		}` |
|       - | 2820 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|       - | 2821 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|       - | 2822 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|       - | 2823 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|       - | 2824 | `		 * so ANY non-object is a TypeError, matching php. */` |
|      55 | 2825 | `		pClass = 0;` |
|      55 | 2826 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 2827 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      43 | 2828 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      20 | 2829 | `				VmClassHintTypeName(pName,pClass,` |
|      20 | 2830 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 2831 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      23 | 2832 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2833 | `		}` |
|      34 | 2834 | `		return SXRET_OK;` |
|       - | 2835 | `	}` |
|     149 | 2836 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|      63 | 2837 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|       - | 2838 | `			char zGivenBuf[128];` |
|       8 | 2839 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       2 | 2840 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       6 | 2841 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2842 | `		}` |
|      59 | 2843 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|       - | 2844 | `			char zTypeBuf[128];` |
|       - | 2845 | `			char zGivenBuf[128];` |
|      60 | 2846 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      19 | 2847 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      19 | 2848 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      41 | 2849 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2850 | `		}` |
|      11 | 2851 | `	}else{` |
|       - | 2852 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|       - | 2853 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      89 | 2854 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2855 | `	}` |
|     107 | 2856 | `	return SXRET_OK;` |
|    1077 | 2857 | `}` |
|       - | 2858 | `/*` |
|       - | 2859 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|       - | 2860 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|       - | 2861 | ` * before a required parameter as implicitly required), excluding a trailing` |
|       - | 2862 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|       - | 2863 | ` * pick php's wording — "exactly N expected" when required == total,` |
|       - | 2864 | ` * "at least N" when trailing optionals exist.` |
|       - | 2865 | ` */` |
|    5704 | 2866 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|       5 | 2867 | `{` |
|    5709 | 2868 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    5709 | 2869 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|    5709 | 2870 | `	sxu32 nRequired = 0;` |
|       - | 2871 | `	sxu32 n;` |
|    5709 | 2872 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     401 | 2873 | `		nFormal--;` |
|     198 | 2874 | `	}` |
|   29935 | 2875 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|   24231 | 2876 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    7257 | 2877 | `			nRequired = n + 1;` |
|    3626 | 2878 | `		}` |
|   12118 | 2879 | `	}` |
|    5709 | 2880 | `	*pnNonVariadic = nFormal;` |
|    5709 | 2881 | `	return nRequired;` |
|       5 | 2882 | `}` |
|       - | 2883 | `/*` |
|       - | 2884 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|       - | 2885 | ` * with too few arguments:` |
|       - | 2886 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|       - | 2887 | ` *   {exactly\|at least} M expected` |
|       - | 2888 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|       - | 2889 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|       - | 2890 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|       - | 2891 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|       - | 2892 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|       - | 2893 | ` */` |
|      26 | 2894 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2895 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|       3 | 2896 | `{` |
|       - | 2897 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|       - | 2898 | `	SyBlob sMsg;` |
|      29 | 2899 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      29 | 2900 | `	if( pOwnerClass ){` |
|       5 | 2901 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|       2 | 2902 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|       3 | 2903 | `	}else{` |
|      25 | 2904 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|       - | 2905 | `	}` |
|      29 | 2906 | `	if( bCallSite ){` |
|      27 | 2907 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|      27 | 2908 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|      12 | 2909 | `	}` |
|      29 | 2910 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|      13 | 2911 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|       - | 2912 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      29 | 2913 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       3 | 2914 | `}` |
|       - | 2915 | `/*` |
|       - | 2916 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|       - | 2917 | ` * cannot be referenced:` |
|       - | 2918 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|       - | 2919 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|       - | 2920 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|       - | 2921 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|       - | 2922 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|       - | 2923 | `` * the bare `m()`.`` |
|       - | 2924 | ` */` |
|       - | 2925 | `/*` |
|       - | 2926 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|       - | 2927 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|       - | 2928 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|       - | 2929 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|       - | 2930 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|       - | 2931 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|       - | 2932 | `` * php's `{closure:file:line}`.`` |
|       - | 2933 | ` */` |
|      26 | 2934 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|       - | 2935 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|       1 | 2936 | `{` |
|      27 | 2937 | `	const char *zShow = 0;` |
|      27 | 2938 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|       - | 2939 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|       - | 2940 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|       - | 2941 | `	 * single name applies (the refusal message splits the same way). */` |
|      27 | 2942 | `	if( pOwnerClass && pArgName ){` |
|      22 | 2943 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2944 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       7 | 2945 | `			&pOwnerClass->sName,nShow,zShow,nArgPos,pArgName);` |
|      20 | 2946 | `	}else if( pOwnerClass ){` |
|     ! 0 | 2947 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2948 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|     ! 0 | 2949 | `			&pOwnerClass->sName,nShow,zShow,nArgPos);` |
|      13 | 2950 | `	}else if( pArgName ){` |
|      13 | 2951 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2952 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       4 | 2953 | `			nShow,zShow,nArgPos,pArgName);` |
|       5 | 2954 | `	}else{` |
|       7 | 2955 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 2956 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|       2 | 2957 | `			nShow,zShow,nArgPos);` |
|       - | 2958 | `	}` |
|      27 | 2959 | `}` |
|    4026 | 2960 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2961 | `	sxu32 nArgPos,SyString *pArgName)` |
|       2 | 2962 | `{` |
|       - | 2963 | `	SyBlob sMsg;` |
|    4028 | 2964 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    4028 | 2965 | `	if( pOwnerClass ){` |
|       8 | 2966 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) could not be passed by reference",` |
|       3 | 2967 | `			&pOwnerClass->sName,pFuncName,nArgPos,pArgName);` |
|       5 | 2968 | `	}else{` |
|    4022 | 2969 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) could not be passed by reference",` |
|    2010 | 2970 | `			pFuncName,nArgPos,pArgName);` |
|       - | 2971 | `	}` |
|       - | 2972 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|    4028 | 2973 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       2 | 2974 | `}` |
|       - | 2975 | `/*` |
|       - | 2976 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|       - | 2977 | ` * called with too few arguments, in php's ZPP wording:` |
|       - | 2978 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|       - | 2979 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|       - | 2980 | ` * pluralized on the expected count).` |
|       - | 2981 | ` *` |
|       - | 2982 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|       - | 2983 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|       - | 2984 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|       - | 2985 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|       - | 2986 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|       - | 2987 | ` */` |
|      34 | 2988 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2989 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|       1 | 2990 | `{` |
|       - | 2991 | `	SyBlob sMsg;` |
|      35 | 2992 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|      35 | 2993 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|      35 | 2994 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      35 | 2995 | `	if( pOwnerClass ){` |
|     ! 0 | 2996 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 2997 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|     ! 0 | 2998 | `	}else{` |
|      35 | 2999 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      17 | 3000 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       - | 3001 | `	}` |
|       - | 3002 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      35 | 3003 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3004 | `}` |
|       - | 3005 | `/*` |
|       - | 3006 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|       - | 3007 | ` * MANY arguments, in php's ZPP wording:` |
|       - | 3008 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|       - | 3009 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|       - | 3010 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|       - | 3011 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|       - | 3012 | ` * so the caller must not route such a callee here.` |
|       - | 3013 | ` */` |
|      26 | 3014 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3015 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|       1 | 3016 | `{` |
|       - | 3017 | `	SyBlob sMsg;` |
|      27 | 3018 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|      27 | 3019 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|      27 | 3020 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      27 | 3021 | `	if( pOwnerClass ){` |
|     ! 0 | 3022 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 3023 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|     ! 0 | 3024 | `	}else{` |
|      27 | 3025 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      13 | 3026 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|       - | 3027 | `	}` |
|       - | 3028 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      27 | 3029 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3030 | `}` |
|       - | 3031 | `/*` |
|       - | 3032 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|       - | 3033 | ` * named or positional argument resolved to:` |
|       - | 3034 | ` *   C::f(): Argument #N ($x) not passed` |
|       - | 3035 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|       - | 3036 | ` */` |
|       2 | 3037 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 3038 | `	sxu32 nArg,SyString *pArgName)` |
|       1 | 3039 | `{` |
|       - | 3040 | `	SyBlob sMsg;` |
|       3 | 3041 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 3042 | `	if( pOwnerClass ){` |
|     ! 0 | 3043 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|     ! 0 | 3044 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|     ! 0 | 3045 | `	}else{` |
|       3 | 3046 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|       - | 3047 | `	}` |
|       - | 3048 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       3 | 3049 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 3050 | `}` |
|       - | 3051 | `/*` |
|       - | 3052 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|       - | 3053 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|       - | 3054 | ` */` |
|       - | 3055 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|       - | 3056 | ` * The message is copied into the instance by __construct, so the caller owns` |
|       - | 3057 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|     144 | 3058 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|       5 | 3059 | `{` |
|       - | 3060 | `	ph7_class *pClass;` |
|       - | 3061 | `	ph7_class_instance *pThis;` |
|       - | 3062 | `	ph7_class_method *pCons;` |
|       - | 3063 | `	ph7_value sArg;` |
|       - | 3064 | `	ph7_value *apArg[1];` |
|       - | 3065 | `	SyString sMsgStr;` |
|       - | 3066 | `	VmFrame *pFrame;` |
|       - | 3067 | `	sxi32 rc;` |
|     149 | 3068 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     149 | 3069 | `	if( pClass == 0 ){` |
|     ! 0 | 3070 | `		return PH7_ABORT;` |
|       - | 3071 | `	}` |
|     149 | 3072 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     149 | 3073 | `	if( pThis == 0 ){` |
|     ! 0 | 3074 | `		return PH7_ABORT;` |
|       - | 3075 | `	}` |
|     149 | 3076 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     149 | 3077 | `	if( pCons ){` |
|     149 | 3078 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|     149 | 3079 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     149 | 3080 | `		apArg[0] = &sArg;` |
|     149 | 3081 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     149 | 3082 | `		PH7_MemObjRelease(&sArg);` |
|      72 | 3083 | `	}` |
|     149 | 3084 | `	pFrame = pVm->pFrame;` |
|     149 | 3085 | `	if( pFrame ){` |
|     149 | 3086 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     149 | 3087 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      72 | 3088 | `	}` |
|     149 | 3089 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     149 | 3090 | `	PH7_ClassInstanceUnref(pThis);` |
|     149 | 3091 | `	if( rc == SXERR_ABORT ){` |
|       6 | 3092 | `		return PH7_ABORT;` |
|       - | 3093 | `	}` |
|     145 | 3094 | `	return PH7_EXCEPTION;` |
|      77 | 3095 | `}` |
|       - | 3096 | `/*` |
|       - | 3097 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|       - | 3098 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|       - | 3099 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|       - | 3100 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|       - | 3101 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|       - | 3102 | ` * other callee falls through to the ordinary Class::method rendering.` |
|       - | 3103 | ` */` |
|       - | 3104 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|  631410 | 3105 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|       5 | 3106 | `{` |
|  631415 | 3107 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|  631415 | 3108 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|  631301 | 3109 | `		return 0;` |
|       - | 3110 | `	}` |
|     119 | 3111 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      83 | 3112 | `		return 0;` |
|       - | 3113 | `	}` |
|      38 | 3114 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|      25 | 3115 | `		*pzKind = "get";` |
|      26 | 3116 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|      15 | 3117 | `		*pzKind = "set";` |
|       9 | 3118 | `	}else{` |
|     ! 0 | 3119 | `		return 0;` |
|       - | 3120 | `	}` |
|      38 | 3121 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|      38 | 3122 | `	return 1;` |
|  315710 | 3123 | `}` |
|     128 | 3124 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3125 | `{` |
|       - | 3126 | `	SyString sProp;` |
|       - | 3127 | `	const char *zKind;` |
|     133 | 3128 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|     123 | 3129 | `		return 0;` |
|       - | 3130 | `	}` |
|      13 | 3131 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|      13 | 3132 | `	return 1;` |
|      69 | 3133 | `}` |
|       - | 3134 | `/*` |
|       - | 3135 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|       - | 3136 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|       - | 3137 | ` * anything else uses its display name (which is also what strips a closure's` |
|       - | 3138 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|       - | 3139 | ` * instead — they are thrown from call sites that already resolved it.` |
|       - | 3140 | ` */` |
|     144 | 3141 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3142 | `{` |
|     149 | 3143 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      95 | 3144 | `		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){` |
|       7 | 3145 | `			return;` |
|       - | 3146 | `		}` |
|      88 | 3147 | `		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);` |
|      88 | 3148 | `		return;` |
|       - | 3149 | `	}` |
|       - | 3150 | `	{` |
|      57 | 3151 | `		const char *zShow = 0;` |
|      57 | 3152 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|      57 | 3153 | `		if( zShow && nShow > 0 ){` |
|      57 | 3154 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      26 | 3155 | `		}` |
|       - | 3156 | `	}` |
|      77 | 3157 | `}` |
|     140 | 3158 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|       5 | 3159 | `{` |
|       - | 3160 | `	SyBlob sMsg,sName;` |
|       - | 3161 | `	sxi32 rc;` |
|     145 | 3162 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     145 | 3163 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|     145 | 3164 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|     145 | 3165 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|     140 | 3166 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|     145 | 3167 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|     145 | 3168 | `	SyBlobRelease(&sName);` |
|     145 | 3169 | `	SyBlobRelease(&sMsg);` |
|     145 | 3170 | `	return rc;` |
|       5 | 3171 | `}` |
|       - | 3172 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|       - | 3173 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|       - | 3174 | ` * php calls it a "method" when it is one. */` |
|       4 | 3175 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|       2 | 3176 | `{` |
|       - | 3177 | `	SyBlob sMsg,sName;` |
|       - | 3178 | `	sxi32 rc;` |
|       6 | 3179 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       6 | 3180 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|       6 | 3181 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|       6 | 3182 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|       4 | 3183 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|       4 | 3184 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|       6 | 3185 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|       6 | 3186 | `	SyBlobRelease(&sName);` |
|       6 | 3187 | `	SyBlobRelease(&sMsg);` |
|       6 | 3188 | `	return rc;` |
|       2 | 3189 | `}` |
|       - | 3190 | `/*` |
|       - | 3191 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|       - | 3192 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|       - | 3193 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|       - | 3194 | ` */` |
|     964 | 3195 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|       5 | 3196 | `{` |
|     969 | 3197 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     109 | 3198 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 3199 | `	}` |
|     865 | 3200 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      99 | 3201 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      99 | 3202 | `		if( pThis && pThis->pClass ){` |
|      99 | 3203 | `			SyString *pName = &pThis->pClass->sName;` |
|      99 | 3204 | `			sxu32 n = pName->nByte;` |
|      99 | 3205 | `			if( n >= nBuf ){` |
|     ! 0 | 3206 | `				n = nBuf - 1;` |
|     ! 0 | 3207 | `			}` |
|      99 | 3208 | `			SyMemcpy(pName->zString,zBuf,n);` |
|      99 | 3209 | `			zBuf[n] = 0;` |
|      99 | 3210 | `			return zBuf;` |
|       - | 3211 | `		}` |
|     ! 0 | 3212 | `		return "object";` |
|       - | 3213 | `	}` |
|     771 | 3214 | `	return ph7_type_name(pVal);` |
|     487 | 3215 | `}` |
|       - | 3216 | `/*` |
|       - | 3217 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|       - | 3218 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|       - | 3219 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|       - | 3220 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|       - | 3221 | ` */` |
|      18 | 3222 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|       3 | 3223 | `{` |
|       - | 3224 | `	ph7_class *pClass;` |
|       - | 3225 | `	ph7_class_instance *pThis;` |
|       - | 3226 | `	ph7_class_method *pCons;` |
|       - | 3227 | `	ph7_value sArg;` |
|       - | 3228 | `	ph7_value *apArg[1];` |
|       - | 3229 | `	SyBlob sMsg;` |
|       - | 3230 | `	SyString sMsgStr;` |
|       - | 3231 | `	VmFrame *pFrame;` |
|       - | 3232 | `	sxi32 rc;` |
|      21 | 3233 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|       - | 3234 | `	char zNameBuf[64];` |
|      21 | 3235 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|      21 | 3236 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|      21 | 3237 | `	if( pClass == 0 ){` |
|     ! 0 | 3238 | `		return PH7_ABORT;` |
|       - | 3239 | `	}` |
|      21 | 3240 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      21 | 3241 | `	if( pThis == 0 ){` |
|     ! 0 | 3242 | `		return PH7_ABORT;` |
|       - | 3243 | `	}` |
|      21 | 3244 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      21 | 3245 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|      21 | 3246 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      21 | 3247 | `	if( pCons ){` |
|      21 | 3248 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      21 | 3249 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      21 | 3250 | `		apArg[0] = &sArg;` |
|      21 | 3251 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      21 | 3252 | `		PH7_MemObjRelease(&sArg);` |
|       9 | 3253 | `	}` |
|      21 | 3254 | `	SyBlobRelease(&sMsg);` |
|      21 | 3255 | `	pFrame = pVm->pFrame;` |
|      21 | 3256 | `	if( pFrame ){` |
|      21 | 3257 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      21 | 3258 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       9 | 3259 | `	}` |
|      21 | 3260 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      21 | 3261 | `	PH7_ClassInstanceUnref(pThis);` |
|      21 | 3262 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3263 | `		return PH7_ABORT;` |
|       - | 3264 | `	}` |
|      21 | 3265 | `	return PH7_EXCEPTION;` |
|      12 | 3266 | `}` |
|       - | 3267 | `/*` |
|       - | 3268 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|       - | 3269 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|       - | 3270 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|       - | 3271 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|       - | 3272 | ` */` |
|       - | 3273 | `/*` |
|       - | 3274 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|       - | 3275 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|       - | 3276 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|       - | 3277 | ` * type field.` |
|       - | 3278 | ` */` |
|  766708 | 3279 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|       5 | 3280 | `{` |
|  766713 | 3281 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|       5 | 3282 | `}` |
|   11096 | 3283 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|       5 | 3284 | `{` |
|   11101 | 3285 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|   11101 | 3286 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|       - | 3287 | `	const char *zGiven;` |
|       - | 3288 | `	ph7_class *pHintScope;` |
|       - | 3289 | `	char zBuf[128];` |
|       - | 3290 | `	char zTypeBuf[128];` |
|       - | 3291 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|   11101 | 3292 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|     ! 0 | 3293 | `		return SXRET_OK;` |
|       - | 3294 | `	}` |
|       - | 3295 | `	/* never return type: the function must not return at all. An explicit` |
|       - | 3296 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|       - | 3297 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|       - | 3298 | `	 * the call site). */` |
|   11101 | 3299 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       6 | 3300 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|       - | 3301 | `	}` |
|       - | 3302 | `	/* void return type: the function must not produce a value. */` |
|   11097 | 3303 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|    1177 | 3304 | `		if( pValue == 0 ){` |
|    1173 | 3305 | `			return SXRET_OK;` |
|       - | 3306 | `		}` |
|       - | 3307 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|       - | 3308 | `		 * store, not a return: php compiles no return statement there at all,` |
|       - | 3309 | `		 * and still reports the hook's return type as void. PHL carries the` |
|       - | 3310 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|       - | 3311 | `		 * so the one implicit value this arm must not reject is that one. */` |
|       6 | 3312 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|       6 | 3313 | `			return SXRET_OK;` |
|       - | 3314 | `		}` |
|       - | 3315 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|       - | 3316 | `		 * still counts as "returned a value" here. */` |
|     ! 0 | 3317 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|     ! 0 | 3318 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|       - | 3319 | `	}` |
|       - | 3320 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|       - | 3321 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|       - | 3322 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    9925 | 3323 | `	if( pValue == 0 ){` |
|      32 | 3324 | `		const char *zExpected = "value";` |
|      32 | 3325 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      47 | 3326 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|      15 | 3327 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|      15 | 3328 | `		}` |
|       - | 3329 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|       - | 3330 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|      32 | 3331 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|       - | 3332 | `	}` |
|       - | 3333 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|       - | 3334 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|       - | 3335 | `	 * matching how every other typed return reports a missing value.) */` |
|    9895 | 3336 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|       5 | 3337 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 3338 | `			return SXRET_OK;` |
|       - | 3339 | `		}` |
|       4 | 3340 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|       1 | 3341 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3342 | `	}` |
|       - | 3343 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|       - | 3344 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|       - | 3345 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    9891 | 3346 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|      38 | 3347 | `		return SXRET_OK;` |
|       - | 3348 | `	}` |
|       - | 3349 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|       - | 3350 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|       - | 3351 | `	 * Check by value before the real-class instanceof branch below. */` |
|    9857 | 3352 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     235 | 3353 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|     235 | 3354 | `		if( rcPseudo == 1 ){` |
|     121 | 3355 | `			return SXRET_OK;` |
|       - | 3356 | `		}` |
|     119 | 3357 | `		if( rcPseudo == 0 ){` |
|      14 | 3358 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       6 | 3359 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|       3 | 3360 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3361 | `		}` |
|       - | 3362 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      54 | 3363 | `	}` |
|       - | 3364 | `	/* The two branches below are the only ones that can name a class, so the` |
|       - | 3365 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|       - | 3366 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|       - | 3367 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|       - | 3368 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|       - | 3369 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|       - | 3370 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    9735 | 3371 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|       - | 3372 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|       - | 3373 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|       - | 3374 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    9735 | 3375 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       - | 3376 | `		sxi32 rcU;` |
|      41 | 3377 | `		const char *zExpected = "union";` |
|      41 | 3378 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|      41 | 3379 | `		if( rcU == SXRET_OK ){` |
|      30 | 3380 | `			return SXRET_OK;` |
|       - | 3381 | `		}` |
|      12 | 3382 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      10 | 3383 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       7 | 3384 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 3385 | `			zGiven = "null";` |
|     ! 0 | 3386 | `		}else{` |
|       3 | 3387 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3388 | `		}` |
|      12 | 3389 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      17 | 3390 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|       5 | 3391 | `				zTypeBuf,sizeof(zTypeBuf));` |
|       5 | 3392 | `		}` |
|      12 | 3393 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|       - | 3394 | `	}` |
|       - | 3395 | `	/* Class return type — instanceof check. The class name is a length-` |
|       - | 3396 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|       - | 3397 | `	 * it into the TypeError message. */` |
|    9699 | 3398 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     113 | 3399 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|     113 | 3400 | `		ph7_class *pExpected = 0;` |
|     113 | 3401 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|      34 | 3402 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      28 | 3403 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      16 | 3404 | `			}else{` |
|       8 | 3405 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3406 | `			}` |
|      49 | 3407 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      15 | 3408 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|       - | 3409 | `		}` |
|      83 | 3410 | `		return SXRET_OK;` |
|       - | 3411 | `	}` |
|       - | 3412 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|       - | 3413 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|       - | 3414 | `	 * non-nullable scalar return — a TypeError. */` |
|    9591 | 3415 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      25 | 3416 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       8 | 3417 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3418 | `			"null");` |
|       - | 3419 | `	}` |
|       - | 3420 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|       - | 3421 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    9575 | 3422 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    9453 | 3423 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    9453 | 3424 | `		return SXRET_OK;` |
|       - | 3425 | `	}` |
|       - | 3426 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|       - | 3427 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|       - | 3428 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|       - | 3429 | `	 * still rejects the object under strict_types. */` |
|     127 | 3430 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      22 | 3431 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      31 | 3432 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      18 | 3433 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      20 | 3434 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      29 | 3435 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       9 | 3436 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       9 | 3437 | `				zGiven);` |
|       - | 3438 | `		}` |
|       1 | 3439 | `	}` |
|       - | 3440 | `	/* Array <-> scalar is never compatible. */` |
|     109 | 3441 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      33 | 3442 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      10 | 3443 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 3444 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3445 | `	}` |
|       - | 3446 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|       - | 3447 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|       - | 3448 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|       - | 3449 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|      84 | 3450 | `	if( !bStrict` |
|      83 | 3451 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|      49 | 3452 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|      54 | 3453 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|      11 | 3454 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       3 | 3455 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3456 | `			"string");` |
|       - | 3457 | `	}` |
|      83 | 3458 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|      80 | 3459 | `		return SXRET_OK;` |
|       - | 3460 | `	}` |
|       4 | 3461 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       1 | 3462 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       1 | 3463 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    5553 | 3464 | `}` |
|       - | 3465 | `/*` |
|       - | 3466 | ` * Report a fatal named-argument error.` |
|       - | 3467 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|       - | 3468 | ` */` |
|      12 | 3469 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       3 | 3470 | `{` |
|       - | 3471 | `	SyBlob sMsg;` |
|       - | 3472 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|       - | 3473 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|       - | 3474 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|       - | 3475 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|       - | 3476 | `	 * unconditional fatal even inside try/catch. */` |
|      15 | 3477 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 3478 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|      15 | 3479 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 3480 | `}` |
|       - | 3481 | `/*` |
|       - | 3482 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 3483 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 3484 | ` * information.` |
|       - | 3485 | ` * ------------------------------------` |
|       - | 3486 | ` * Simple boring wrapper function.` |
|       - | 3487 | ` * ------------------------------------` |
|       - | 3488 | ` */` |
|   24130 | 3489 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|       5 | 3490 | `{` |
|       - | 3491 | `	sxi32 rc;` |
|   24135 | 3492 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|   24135 | 3493 | `	return rc;` |
|       5 | 3494 | `}` |
|       - | 3495 | `/*` |
|       - | 3496 | ` * Resolve function context from the current frame.` |
|       - | 3497 | ` */` |
|       - | 3498 | `/*` |
|       - | 3499 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|       - | 3500 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|       - | 3501 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|       - | 3502 | ` * straight at the function's own name otherwise.` |
|       - | 3503 | ` */` |
|  631228 | 3504 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|       5 | 3505 | `{` |
|  631233 | 3506 | `	const char *zName = pFunc->sName.zString;` |
|  631233 | 3507 | `	int nName = (int)pFunc->sName.nByte;` |
|  681615 | 3508 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|  683121 | 3509 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|       - | 3510 | `	/* A property hook is not a method in php and never shows the name PHL` |
|       - | 3511 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|       - | 3512 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|       - | 3513 | `	 * report from inside one, and what makes the trace line read` |
|       - | 3514 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|       - | 3515 | ``	 * `C::$p::get` for free. */`` |
|       - | 3516 | `	{` |
|       - | 3517 | `		SyString sProp;` |
|       - | 3518 | `		const char *zKind;` |
|  631233 | 3519 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      31 | 3520 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       9 | 3521 | `				"$%z::%s",&sProp,zKind);` |
|      22 | 3522 | `			*pzOut = pVm->zDisplayName;` |
|      22 | 3523 | `			return n;` |
|       - | 3524 | `		}` |
|       - | 3525 | `	}` |
|  631215 | 3526 | `	if( bClosure ){` |
|       - | 3527 | `		int n;` |
|    3045 | 3528 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|       - | 3529 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|       - | 3530 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|    3045 | 3531 | `			*pzOut = pFunc->sClosureName.zString;` |
|    3045 | 3532 | `			return (int)pFunc->sClosureName.nByte;` |
|       - | 3533 | `		}` |
|     ! 0 | 3534 | `		if( pFunc->sFile.nByte > 0 ){` |
|     ! 0 | 3535 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|     ! 0 | 3536 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|     ! 0 | 3537 | `		}else{` |
|     ! 0 | 3538 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|       - | 3539 | `		}` |
|     ! 0 | 3540 | `		*pzOut = pVm->zDisplayName;` |
|     ! 0 | 3541 | `		return n;` |
|       - | 3542 | `	}` |
|  628175 | 3543 | `	*pzOut = zName;` |
|  628175 | 3544 | `	return nName;` |
|  315619 | 3545 | `}` |
|    1136 | 3546 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|       4 | 3547 | `{` |
|       - | 3548 | `	VmFrame *pFrame;` |
|       - | 3549 | `	ph7_vm_func *pFunc;` |
|    1140 | 3550 | `	*pzFuncName = 0;` |
|    1140 | 3551 | `	*pnFuncLen = 0;` |
|    1140 | 3552 | `	pFrame = pVm->pFrame;` |
|    1140 | 3553 | `	if( pFrame == 0 ){` |
|     ! 0 | 3554 | `		return;` |
|       - | 3555 | `	}` |
|    1140 | 3556 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    1140 | 3557 | `	if( pFrame->pParent == 0 ){` |
|    1112 | 3558 | `		return;` |
|       - | 3559 | `	}` |
|      32 | 3560 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      32 | 3561 | `	if( pFunc == 0 ){` |
|     ! 0 | 3562 | `		return;` |
|       - | 3563 | `	}` |
|      32 | 3564 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|     572 | 3565 | `}` |
|       - | 3566 | `/*` |
|       - | 3567 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|       - | 3568 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|       - | 3569 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|       - | 3570 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|       - | 3571 | ` * built-in library and already produces php's exact byte format, which keeps` |
|       - | 3572 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|       - | 3573 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|       - | 3574 | ` * synthesize what it can.` |
|       - | 3575 | ` */` |
|     586 | 3576 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3577 | `{` |
|       - | 3578 | `	ph7_class_method *pGetTrace;` |
|       - | 3579 | `	ph7_value sTrace;` |
|       - | 3580 | `	const char *zTmp;` |
|       - | 3581 | `	int nTmp;` |
|     590 | 3582 | `	int bDone = 0;` |
|       - | 3583 | `	int bSaved;` |
|     590 | 3584 | `	if( pThis == 0 ){` |
|       5 | 3585 | `		return 0;` |
|       - | 3586 | `	}` |
|     586 | 3587 | `	if( pVm->bRenderingUncaught ){` |
|       - | 3588 | `		/* Already inside a report: do not run userland trace code again. */` |
|     ! 0 | 3589 | `		return 0;` |
|       - | 3590 | `	}` |
|     586 | 3591 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|     586 | 3592 | `	if( pGetTrace == 0 ){` |
|     ! 0 | 3593 | `		return 0;` |
|       - | 3594 | `	}` |
|     586 | 3595 | `	PH7_MemObjInit(pVm,&sTrace);` |
|       - | 3596 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|       - | 3597 | `	 * anything it calls) throws, the throw would be reported by this very` |
|       - | 3598 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|       - | 3599 | `	 * trace rather than re-entering here forever. */` |
|     586 | 3600 | `	bSaved = pVm->bRenderingUncaught;` |
|     586 | 3601 | `	pVm->bRenderingUncaught = 1;` |
|     586 | 3602 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|     586 | 3603 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|     586 | 3604 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 3605 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     586 | 3606 | `			bDone = 1;` |
|     291 | 3607 | `		}` |
|     291 | 3608 | `	}` |
|     586 | 3609 | `	PH7_MemObjRelease(&sTrace);` |
|     586 | 3610 | `	pVm->bRenderingUncaught = bSaved;` |
|     586 | 3611 | `	return bDone;` |
|     297 | 3612 | `}` |
|       - | 3613 | `/*` |
|       - | 3614 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|       - | 3615 | ` *` |
|       - | 3616 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|       - | 3617 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|       - | 3618 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|       - | 3619 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|       - | 3620 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|       - | 3621 | ` *             trailer.` |
|       - | 3622 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|       - | 3623 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|       - | 3624 | ` * call; this routine only appends.` |
|       - | 3625 | ` */` |
|     586 | 3626 | `static void VmRenderUncaughtEntry(` |
|       - | 3627 | `	ph7_vm *pVm,SyBlob *pOut,` |
|       - | 3628 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|       - | 3629 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|       - | 3630 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|       - | 3631 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|       - | 3632 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|       4 | 3633 | `{` |
|       - | 3634 | `	SyString *pFile;` |
|     590 | 3635 | `	if( nThrowLine == 0 ){` |
|       5 | 3636 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|       2 | 3637 | `	}` |
|     590 | 3638 | `	if( nCallLine == 0 ){` |
|     590 | 3639 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     590 | 3640 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|     293 | 3641 | `	}` |
|     590 | 3642 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|     ! 0 | 3643 | `		zClass = "Exception";` |
|     ! 0 | 3644 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|     ! 0 | 3645 | `	}` |
|     590 | 3646 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     564 | 3647 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     280 | 3648 | `	}` |
|     590 | 3649 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     590 | 3650 | `	if( bFirst ){` |
|     584 | 3651 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|     294 | 3652 | `	}else{` |
|       8 | 3653 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       - | 3654 | `	}` |
|     590 | 3655 | `	SyBlobAppend(pOut,zClass,nClass);` |
|     590 | 3656 | `	if( zMsg && nMsg > 0 ){` |
|     590 | 3657 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|     590 | 3658 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|     293 | 3659 | `	}` |
|     590 | 3660 | `	if( pFile ){` |
|     590 | 3661 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     293 | 3662 | `	}` |
|     590 | 3663 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       - | 3664 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|       - | 3665 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|       - | 3666 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|       - | 3667 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|       - | 3668 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|       - | 3669 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|       - | 3670 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|     590 | 3671 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|       5 | 3672 | `		int bFrame = 0;` |
|       5 | 3673 | `		if( zFuncName && nFuncLen > 0 ){` |
|       3 | 3674 | `			if( pFile ){` |
|       - | 3675 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|       - | 3676 | `				 * running inside it. */` |
|       4 | 3677 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|       2 | 3678 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|       2 | 3679 | `			}else{` |
|     ! 0 | 3680 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|       - | 3681 | `			}` |
|       3 | 3682 | `			bFrame = 1;` |
|       1 | 3683 | `		}` |
|       - | 3684 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|       7 | 3685 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|       2 | 3686 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|       2 | 3687 | `	}` |
|     590 | 3688 | `	if( bLast && pFile ){` |
|     584 | 3689 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|     584 | 3690 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     290 | 3691 | `	}` |
|     590 | 3692 | `}` |
|       - | 3693 | `/*` |
|       - | 3694 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|       - | 3695 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|       - | 3696 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|       - | 3697 | ` */` |
|       4 | 3698 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|       1 | 3699 | `{` |
|       - | 3700 | `	SyBlob sOut;` |
|       - | 3701 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|       - | 3702 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|       5 | 3703 | `	pVm->iExitStatus = 255;` |
|       5 | 3704 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3705 | `		return PH7_OK;` |
|       - | 3706 | `	}` |
|       5 | 3707 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       5 | 3708 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|       5 | 3709 | `	VmCallErrorHandler(pVm,&sOut);` |
|       5 | 3710 | `	SyBlobRelease(&sOut);` |
|       5 | 3711 | `	return PH7_ABORT;` |
|       3 | 3712 | `}` |
|       - | 3713 | `/*` |
|       - | 3714 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|       - | 3715 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|       - | 3716 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|       - | 3717 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|       - | 3718 | ` */` |
|       - | 3719 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|     582 | 3720 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|       4 | 3721 | `{` |
|       - | 3722 | `	ph7_value *pValue;` |
|       - | 3723 | `	ph7_class_instance *pPrev;` |
|       - | 3724 | `	ph7_class *pThrowable;` |
|     586 | 3725 | `	if( pThis == 0 ){` |
|     ! 0 | 3726 | `		return 0;` |
|       - | 3727 | `	}` |
|     586 | 3728 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     586 | 3729 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     580 | 3730 | `		return 0;` |
|       - | 3731 | `	}` |
|       8 | 3732 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 3733 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|       - | 3734 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|       - | 3735 | `	 * never renders a stray object as an exception entry. */` |
|       8 | 3736 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       8 | 3737 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|     ! 0 | 3738 | `		return 0;` |
|       - | 3739 | `	}` |
|       8 | 3740 | `	return pPrev;` |
|     295 | 3741 | `}` |
|       - | 3742 | `/*` |
|       - | 3743 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|       - | 3744 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|       - | 3745 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|       - | 3746 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|       - | 3747 | ` */` |
|      16 | 3748 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|       2 | 3749 | `{` |
|       - | 3750 | `	ph7_value *pValue;` |
|      18 | 3751 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|     ! 0 | 3752 | `		return;` |
|       - | 3753 | `	}` |
|      18 | 3754 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      18 | 3755 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       3 | 3756 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|       - | 3757 | `	}` |
|      16 | 3758 | `	pPrev->iRef++;` |
|       - | 3759 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|       - | 3760 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|      16 | 3761 | `	PH7_MemObjRelease(pValue);` |
|      16 | 3762 | `	pValue->x.pOther = pPrev;` |
|      16 | 3763 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|      10 | 3764 | `}` |
|       - | 3765 | `/*` |
|       - | 3766 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|       - | 3767 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|       - | 3768 | ` * absent or yields an empty string.` |
|       - | 3769 | ` */` |
|       - | 3770 | `/*` |
|       - | 3771 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|       - | 3772 | ` * 0 when the class exposes no getLine().` |
|       - | 3773 | ` */` |
|     582 | 3774 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       4 | 3775 | `{` |
|       - | 3776 | `	ph7_class_method *pGetLine;` |
|       - | 3777 | `	ph7_value sLine;` |
|     586 | 3778 | `	sxu32 nLine = 0;` |
|     586 | 3779 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|     586 | 3780 | `	if( pGetLine == 0 ){` |
|     ! 0 | 3781 | `		return 0;` |
|       - | 3782 | `	}` |
|     586 | 3783 | `	PH7_MemObjInit(pVm,&sLine);` |
|     586 | 3784 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|     586 | 3785 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|     586 | 3786 | `		if( n > 0 ){` |
|     586 | 3787 | `			nLine = (sxu32)n;` |
|     291 | 3788 | `		}` |
|     291 | 3789 | `	}` |
|     586 | 3790 | `	PH7_MemObjRelease(&sLine);` |
|     586 | 3791 | `	return nLine;` |
|     295 | 3792 | `}` |
|     582 | 3793 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3794 | `{` |
|       - | 3795 | `	ph7_class_method *pGetMessage;` |
|       - | 3796 | `	ph7_value sMsg;` |
|       - | 3797 | `	const char *zTmp;` |
|       - | 3798 | `	int nTmp;` |
|     586 | 3799 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|     586 | 3800 | `	if( pGetMessage == 0 ){` |
|     ! 0 | 3801 | `		return;` |
|       - | 3802 | `	}` |
|     586 | 3803 | `	PH7_MemObjInit(pVm,&sMsg);` |
|     586 | 3804 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|     586 | 3805 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|     586 | 3806 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 3807 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     291 | 3808 | `		}` |
|     291 | 3809 | `	}` |
|     586 | 3810 | `	PH7_MemObjRelease(&sMsg);` |
|     295 | 3811 | `}` |
|       - | 3812 | `/*` |
|       - | 3813 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|       - | 3814 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|       - | 3815 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|       - | 3816 | ` * outermost (the actually-uncaught) exception.` |
|       - | 3817 | ` *` |
|       - | 3818 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|       - | 3819 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|       - | 3820 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|       - | 3821 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|       - | 3822 | ` */` |
|       - | 3823 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|     576 | 3824 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|       4 | 3825 | `{` |
|       - | 3826 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|     580 | 3827 | `	int nChain = 0;` |
|       - | 3828 | `	int i;` |
|       - | 3829 | `	SyBlob sOut;` |
|       - | 3830 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|       - | 3831 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|       - | 3832 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|     580 | 3833 | `	pVm->iExitStatus = 255;` |
|     580 | 3834 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3835 | `		return PH7_OK;` |
|       - | 3836 | `	}` |
|       - | 3837 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|       - | 3838 | `	 * collected) or the hard cap. */` |
|    1162 | 3839 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|     594 | 3840 | `		for( i = 0 ; i < nChain ; ++i ){` |
|      10 | 3841 | `			if( apChain[i] == pThis ){` |
|     ! 0 | 3842 | `				pThis = 0; /* cycle: stop the walk */` |
|     ! 0 | 3843 | `				break;` |
|       - | 3844 | `			}` |
|       6 | 3845 | `		}` |
|     586 | 3846 | `		if( pThis == 0 ){` |
|     ! 0 | 3847 | `			break;` |
|       - | 3848 | `		}` |
|     586 | 3849 | `		apChain[nChain++] = pThis;` |
|     586 | 3850 | `		pThis = VmExceptionGetPrevious(pThis);` |
|       4 | 3851 | `	}` |
|     580 | 3852 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       - | 3853 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|       - | 3854 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|    1162 | 3855 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|     586 | 3856 | `		ph7_class_instance *pEnt = apChain[i];` |
|       - | 3857 | `		SyBlob sMsg;` |
|     586 | 3858 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     586 | 3859 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|     877 | 3860 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|     582 | 3861 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|     582 | 3862 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|     291 | 3863 | `			zFuncName,nFuncLen,` |
|     582 | 3864 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|     291 | 3865 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|     291 | 3866 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|     586 | 3867 | `		SyBlobRelease(&sMsg);` |
|     295 | 3868 | `	}` |
|     580 | 3869 | `	VmCallErrorHandler(pVm,&sOut);` |
|     580 | 3870 | `	SyBlobRelease(&sOut);` |
|     580 | 3871 | `	return PH7_ABORT;` |
|     292 | 3872 | `}` |
|       - | 3873 | `/*` |
|       - | 3874 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|       - | 3875 | ` *` |
|       - | 3876 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|       - | 3877 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|       - | 3878 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|       - | 3879 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|       - | 3880 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|       - | 3881 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|       - | 3882 | ` */` |
| 1555530 | 3883 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|       5 | 3884 | `{` |
| 1555535 | 3885 | `	if( pVm->bCoalesceArmed ){` |
|       8 | 3886 | `		if( pVm->pCoalesceObj ){` |
|       8 | 3887 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|       3 | 3888 | `		}` |
|       8 | 3889 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       8 | 3890 | `		pVm->pCoalesceObj = 0;` |
|       8 | 3891 | `		pVm->bCoalesceArmed = 0;` |
|       3 | 3892 | `	}` |
| 1555535 | 3893 | `}` |
|       - | 3894 | `/*` |
|       - | 3895 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|       - | 3896 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|       - | 3897 | ` * is a literal, non-formatted string; callers that need formatting should` |
|       - | 3898 | ` * build the SyBlob themselves and pass its data + length.` |
|       - | 3899 | ` *` |
|       - | 3900 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|       - | 3901 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|       - | 3902 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|       - | 3903 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|       - | 3904 | ` */` |
|  140970 | 3905 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|       - | 3906 | `	ph7_vm *pVm,` |
|       - | 3907 | `	const char *zClass,` |
|       - | 3908 | `	const char *zMsg,` |
|       - | 3909 | `	sxu32 nMsg` |
|       5 | 3910 | `){` |
|       - | 3911 | `	ph7_class *pClass;` |
|       - | 3912 | `	ph7_class_instance *pThis;` |
|       - | 3913 | `	ph7_class_method *pCons;` |
|       - | 3914 | `	VmFrame *pFrame;` |
|       - | 3915 | `	sxi32 rc;` |
|  140975 | 3916 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|  140975 | 3917 | `	if( pClass == 0 ){` |
|     ! 0 | 3918 | `		return SXERR_ABORT;` |
|       - | 3919 | `	}` |
|  140975 | 3920 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|  140975 | 3921 | `	if( pThis == 0 ){` |
|     ! 0 | 3922 | `		return SXERR_ABORT;` |
|       - | 3923 | `	}` |
|  140975 | 3924 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|  140975 | 3925 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|       - | 3926 | `		ph7_value sArg;` |
|       - | 3927 | `		ph7_value *apArg[1];` |
|       - | 3928 | `		SyString sMsgStr;` |
|  140975 | 3929 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|  140975 | 3930 | `		PH7_MemObjInit(pVm,&sArg);` |
|  140975 | 3931 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  140975 | 3932 | `		apArg[0] = &sArg;` |
|  140975 | 3933 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|  140975 | 3934 | `		PH7_MemObjRelease(&sArg);` |
|  140975 | 3935 | `		pVm->nExcCtorDepth--;` |
|   70485 | 3936 | `	}` |
|  140975 | 3937 | `	pFrame = pVm->pFrame;` |
|  140975 | 3938 | `	if( pFrame ){` |
|  140975 | 3939 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  140975 | 3940 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   70485 | 3941 | `	}` |
|  140975 | 3942 | `	rc = VmThrowException(pVm,pThis);` |
|  140975 | 3943 | `	PH7_ClassInstanceUnref(pThis);` |
|  140975 | 3944 | `	return rc;` |
|   70490 | 3945 | `}` |
|       - | 3946 | `/*` |
|       - | 3947 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|       - | 3948 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|       - | 3949 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|       - | 3950 | ` *` |
|       - | 3951 | ` *   int/float/bool/null      arithmetic proceeds` |
|       - | 3952 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|       - | 3953 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|       - | 3954 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|       - | 3955 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|       - | 3956 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|       - | 3957 | ` *   object/resource          TypeError, naming the object's CLASS` |
|       - | 3958 | ` *` |
|       - | 3959 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|       - | 3960 | ` */` |
|       - | 3961 | `/*` |
|       - | 3962 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|       - | 3963 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|       - | 3964 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|       - | 3965 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|       - | 3966 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|       - | 3967 | ` * to depth 1).` |
|       - | 3968 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|       - | 3969 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|       - | 3970 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|       - | 3971 | ` * frame shape is file/line/function[/class/type], matching the default` |
|       - | 3972 | ` * zend.exception_ignore_args=On.` |
|       - | 3973 | ` */` |
| 1455404 | 3974 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)` |
|       5 | 3975 | `{` |
|       - | 3976 | `	SyString *pFile;` |
|       - | 3977 | `	VmFrame *pFrame;` |
|       - | 3978 | `	ph7_value *pValue;` |
| 1455409 | 3979 | `	pValue = ph7_new_scalar(&(*pVm));` |
| 1455409 | 3980 | `	if( pValue == 0 ){` |
|     ! 0 | 3981 | `		return;` |
|       - | 3982 | `	}` |
| 1455409 | 3983 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1455409 | 3984 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 2086161 | 3985 | `	while( pFrame ){` |
| 2086161 | 3986 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 3987 | `		ph7_value *pEntry;` |
| 2086161 | 3988 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|       - | 3989 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|  727707 | 3990 | `			break;` |
|       - | 3991 | `		}` |
|  630757 | 3992 | `		pEntry = ph7_new_array(&(*pVm));` |
|  630757 | 3993 | `		if( pEntry == 0 ){` |
|     ! 0 | 3994 | `			break;` |
|       - | 3995 | `		}` |
|       - | 3996 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|       - | 3997 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|       - | 3998 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|       - | 3999 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|       - | 4000 | `		 * include-stack top for a call made at global scope. */` |
|       - | 4001 | `		{` |
|  630757 | 4002 | `			SyString *pFrameFile = pFile;` |
|  630757 | 4003 | `			if( pFrame->pParent->pUserData ){` |
|     361 | 4004 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|     361 | 4005 | `				if( pCaller->sFile.nByte > 0 ){` |
|     361 | 4006 | `					pFrameFile = &pCaller->sFile;` |
|     178 | 4007 | `				}` |
|     178 | 4008 | `			}` |
|  630757 | 4009 | `			if( pFrameFile ){` |
|  630757 | 4010 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|  630757 | 4011 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|  630757 | 4012 | `				ph7_value_reset_string_cursor(pValue);` |
|  315376 | 4013 | `			}` |
|       - | 4014 | `		}` |
|  630757 | 4015 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|  630757 | 4016 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|       - | 4017 | `		{` |
|  630757 | 4018 | `			const char *zDisp = 0;` |
|  630757 | 4019 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|  630757 | 4020 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|       - | 4021 | `		}` |
|  630757 | 4022 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|  630757 | 4023 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 4024 | `		{` |
|       - | 4025 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|       - | 4026 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|       - | 4027 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|       - | 4028 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|       - | 4029 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|       - | 4030 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|       - | 4031 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|  630757 | 4032 | `			SyString *pClsName = 0;` |
|  630757 | 4033 | `			const char *zType = "->";` |
|  630757 | 4034 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|  500377 | 4035 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|  500377 | 4036 | `				zType = pFrame->pThis ? "->" : "::";` |
|  380571 | 4037 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|     ! 0 | 4038 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|     ! 0 | 4039 | `			}` |
|  630757 | 4040 | `			if( pClsName ){` |
|  500377 | 4041 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|  500377 | 4042 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|  500377 | 4043 | `				ph7_value_reset_string_cursor(pValue);` |
|  500377 | 4044 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|  500377 | 4045 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|  500377 | 4046 | `				ph7_value_reset_string_cursor(pValue);` |
|  500377 | 4047 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){` |
|      17 | 4048 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      17 | 4049 | `					if( pObjVal ){` |
|      17 | 4050 | `						pFrame->pThis->iRef++;` |
|      17 | 4051 | `						pObjVal->x.pOther = pFrame->pThis;` |
|      17 | 4052 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      17 | 4053 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      17 | 4054 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       7 | 4055 | `					}` |
|       7 | 4056 | `				}` |
|  250186 | 4057 | `			}` |
|       - | 4058 | `		}` |
|  630757 | 4059 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      17 | 4060 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      17 | 4061 | `			if( pArg ){` |
|      17 | 4062 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - | 4063 | `				sxu32 n;` |
|      31 | 4064 | `				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){` |
|      16 | 4065 | `					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|      16 | 4066 | `					if( pObj ){` |
|      16 | 4067 | `						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       7 | 4068 | `					}` |
|       9 | 4069 | `				}` |
|      17 | 4070 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      17 | 4071 | `				ph7_release_value(&(*pVm),pArg);` |
|       7 | 4072 | `			}` |
|       7 | 4073 | `		}` |
|  630757 | 4074 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|  630757 | 4075 | `		ph7_release_value(&(*pVm),pEntry);` |
|  630757 | 4076 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|       5 | 4077 | `	}` |
| 1455409 | 4078 | `	ph7_release_value(&(*pVm),pValue);` |
|  727707 | 4079 | `}` |
|       - | 4080 | `/*` |
|       - | 4081 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|       - | 4082 | ` *` |
|       - | 4083 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|       - | 4084 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|       - | 4085 | ` * calls parent::__construct still reports the right position. The embedded` |
|       - | 4086 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|       - | 4087 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|       - | 4088 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|       - | 4089 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|       - | 4090 | ` */` |
| 1573854 | 4091 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       5 | 4092 | `{` |
|       - | 4093 | `	static const char *azField[] = { "file", "line", "trace" };` |
|       - | 4094 | `	ph7_class *pThrowable;` |
|       - | 4095 | `	SyString *pFile;` |
|       - | 4096 | `	SyString *pSiteFile;` |
|       - | 4097 | `	sxu32 n;` |
| 1573859 | 4098 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|     ! 0 | 4099 | `		return;` |
|       - | 4100 | `	}` |
| 1573859 | 4101 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1573859 | 4102 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|  118471 | 4103 | `		return;` |
|       - | 4104 | `	}` |
| 1455393 | 4105 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1455393 | 4106 | `	pSiteFile = pFile;` |
|       - | 4107 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|       - | 4108 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|       - | 4109 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|       - | 4110 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|       - | 4111 | `	{` |
| 1455393 | 4112 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 1455393 | 4113 | `		if( pInner && pInner->pUserData ){` |
|  627697 | 4114 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|  627697 | 4115 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|  627609 | 4116 | `				pSiteFile = &pInnerFunc->sFile;` |
|  313802 | 4117 | `			}` |
|  313846 | 4118 | `		}` |
|       - | 4119 | `	}` |
| 5821557 | 4120 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|       - | 4121 | `		SyHashEntry *pEntry;` |
|       - | 4122 | `		VmClassAttr *pVmAttr;` |
|       - | 4123 | `		ph7_value *pAttrValue;` |
| 4366169 | 4124 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
| 4366169 | 4125 | `		if( pEntry == 0 ){` |
|     ! 0 | 4126 | `			continue;` |
|       - | 4127 | `		}` |
| 4366169 | 4128 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
| 4366169 | 4129 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 4366169 | 4130 | `		if( pAttrValue == 0 ){` |
|     ! 0 | 4131 | `			continue;` |
|       - | 4132 | `		}` |
| 4366169 | 4133 | `		if( n == 0 ){` |
| 1455393 | 4134 | `			if( pSiteFile ){` |
| 1455393 | 4135 | `				PH7_MemObjRelease(pAttrValue);` |
| 1455393 | 4136 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|  727699 | 4137 | `			}` |
| 3638475 | 4138 | `		}else if( n == 1 ){` |
|       - | 4139 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|       - | 4140 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|       - | 4141 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|       - | 4142 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|       - | 4143 | `			 * off everything the initializer calls — an autoloader, a nested` |
|       - | 4144 | `			 * constant's evaluation — which report their own lines in both engines.` |
|       - | 4145 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|       - | 4146 | `			 * PHL already matches.) */` |
| 1455394 | 4147 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|      40 | 4148 | `				? pVm->nLazyInitLine` |
| 1455389 | 4149 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
| 1455393 | 4150 | `			PH7_MemObjRelease(pAttrValue);` |
| 1455393 | 4151 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|  727699 | 4152 | `		}else{` |
|       - | 4153 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|       - | 4154 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|       - | 4155 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|       - | 4156 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
| 1455393 | 4157 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
| 1455393 | 4158 | `			if( pList == 0 ){` |
|     ! 0 | 4159 | `				continue;` |
|       - | 4160 | `			}` |
| 1455393 | 4161 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);` |
|       - | 4162 | `			/* Building the trace reserves new memobjs, which may realloc` |
|       - | 4163 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|       - | 4164 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|       - | 4165 | `			 * AFTER the walk before releasing/storing into it. */` |
| 1455393 | 4166 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 1455393 | 4167 | `			if( pAttrValue ){` |
| 1455393 | 4168 | `				PH7_MemObjRelease(pAttrValue);` |
| 1455393 | 4169 | `				PH7_MemObjStore(pList,pAttrValue);` |
|  727694 | 4170 | `			}` |
| 1455393 | 4171 | `			ph7_release_value(&(*pVm),pList);` |
|       - | 4172 | `		}` |
| 2183087 | 4173 | `	}` |
|  786932 | 4174 | `}` |
|     362 | 4175 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|       3 | 4176 | `{` |
|     365 | 4177 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      33 | 4178 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      33 | 4179 | `		if( pInst && pInst->pClass ){` |
|      33 | 4180 | `			return pInst->pClass->sName.zString;` |
|       - | 4181 | `		}` |
|     ! 0 | 4182 | `	}` |
|     333 | 4183 | `	return ph7_type_name(pVal);` |
|     184 | 4184 | `}` |
|       - | 4185 | `/*` |
|       - | 4186 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|       - | 4187 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|       - | 4188 | ` * describes an operand it could not use as one ("on false", "false given").` |
|       - | 4189 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|       - | 4190 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|       - | 4191 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|       - | 4192 | ` * instead of replacing it.` |
|       - | 4193 | ` */` |
|     104 | 4194 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|       3 | 4195 | `{` |
|     107 | 4196 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|      19 | 4197 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 4198 | `	}` |
|      89 | 4199 | `	return VmArithTypeName(&(*pVal));` |
|      55 | 4200 | `}` |
|       - | 4201 | `/*` |
|       - | 4202 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|       - | 4203 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|       - | 4204 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|       - | 4205 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|       - | 4206 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|       - | 4207 | ` */` |
|      36 | 4208 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|       1 | 4209 | `{` |
|      37 | 4210 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|      37 | 4211 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      15 | 4212 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      15 | 4213 | `		if( pInst && pInst->pClass ){` |
|      15 | 4214 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|      15 | 4215 | `			return;` |
|       - | 4216 | `		}` |
|     ! 0 | 4217 | `	}` |
|      23 | 4218 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|      19 | 4219 | `}` |
|       - | 4220 | `/*` |
|       - | 4221 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|       - | 4222 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|       - | 4223 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|       - | 4224 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|       - | 4225 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|       - | 4226 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|       - | 4227 | ` */` |
|      40 | 4228 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|       1 | 4229 | `{` |
|      41 | 4230 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      41 | 4231 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      41 | 4232 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      41 | 4233 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      41 | 4234 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|       - | 4235 | `	sxu32 i;` |
|     109 | 4236 | `	for( i = 0 ; i < nMin ; ++i ){` |
|       - | 4237 | `		unsigned char c;` |
|      69 | 4238 | `		if( cOp == '\|' ){` |
|      25 | 4239 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      57 | 4240 | `		}else if( cOp == '^' ){` |
|      21 | 4241 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|      11 | 4242 | `		}else{` |
|      25 | 4243 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|       - | 4244 | `		}` |
|      69 | 4245 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      35 | 4246 | `	}` |
|      41 | 4247 | `	if( cOp == '\|' && nL != nR ){` |
|      11 | 4248 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|      11 | 4249 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|      11 | 4250 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       5 | 4251 | `	}` |
|      41 | 4252 | `}` |
|       - | 4253 | `/*` |
|       - | 4254 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|       - | 4255 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|       - | 4256 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|       - | 4257 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|       - | 4258 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|       - | 4259 | ` * null, which is what an undefined variable answers.` |
|       - | 4260 | ` */` |
|      24 | 4261 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|       1 | 4262 | `{` |
|      25 | 4263 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       9 | 4264 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       9 | 4265 | `		if( pInst && pInst->pClass ){` |
|       9 | 4266 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|       9 | 4267 | `			return;` |
|       - | 4268 | `		}` |
|     ! 0 | 4269 | `	}` |
|      17 | 4270 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       7 | 4271 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|       4 | 4272 | `			pVal->x.iVal ? "true" : "false");` |
|      15 | 4273 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|      11 | 4274 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|       6 | 4275 | `	}else{` |
|       3 | 4276 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|       - | 4277 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|       - | 4278 | `	}` |
|      13 | 4279 | `}` |
|       - | 4280 | `/*` |
|       - | 4281 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|       - | 4282 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|       - | 4283 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|       - | 4284 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|       - | 4285 | ` * computes with the prefix. Classify pVal against that contract.` |
|       - | 4286 | ` */` |
|   82240 | 4287 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|       5 | 4288 | `{` |
|       - | 4289 | `	ph7_value sInt;` |
|       - | 4290 | `	sxi32 rc;` |
|   82245 | 4291 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   82245 | 4292 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   82245 | 4293 | `	PH7_MemObjRelease(&sInt);` |
|   82245 | 4294 | `	return rc;` |
|       5 | 4295 | `}` |
|  162239 | 4296 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|       5 | 4297 | `{` |
|  162244 | 4298 | `	int bBadL = 0, bBadR = 0;` |
|       - | 4299 | `	int i;` |
|       - | 4300 | `	ph7_value *apOperand[2];` |
|  162244 | 4301 | `	apOperand[0] = pLeft;` |
|  162244 | 4302 | `	apOperand[1] = pRight;` |
|       - | 4303 | `	/* array + array is php's union operator, not arithmetic */` |
|  162239 | 4304 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   25769 | 4305 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|    4617 | 4306 | `		return SXRET_OK;` |
|       - | 4307 | `	}` |
|  472886 | 4308 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  315259 | 4309 | `		ph7_value *pVal = apOperand[i];` |
|  315259 | 4310 | `		int bBad = 0;` |
|  315259 | 4311 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      87 | 4312 | `			bBad = 1;` |
|  315216 | 4313 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     298 | 4314 | `			const char *zTail = 0;` |
|     298 | 4315 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     298 | 4316 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|       - | 4317 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|      49 | 4318 | `				bBad = 1;` |
|      25 | 4319 | `			}else{` |
|       - | 4320 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|       - | 4321 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|       - | 4322 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     276 | 4323 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|      27 | 4324 | `					zTail++;` |
|       1 | 4325 | `				}` |
|     250 | 4326 | `				if( zTail < zEnd ){` |
|      40 | 4327 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      19 | 4328 | `				}` |
|       - | 4329 | `			}` |
|     148 | 4330 | `		}` |
|  315259 | 4331 | `		if( bBad ){` |
|     135 | 4332 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|      67 | 4333 | `		}` |
|  157961 | 4334 | `	}` |
|  157632 | 4335 | `	if( bBadL \|\| bBadR ){` |
|       - | 4336 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|       - | 4337 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|       - | 4338 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     187 | 4339 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|      62 | 4340 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     125 | 4341 | `		return SXERR_INVALID;` |
|       - | 4342 | `	}` |
|  157508 | 4343 | `	return SXRET_OK;` |
|   81289 | 4344 | `}` |
|       - | 4345 | `/*` |
|       - | 4346 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|       - | 4347 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|       - | 4348 | ` * pass 0 for the engine errors that leave it at its default.` |
|       - | 4349 | ` */` |
|    7672 | 4350 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|       5 | 4351 | `{` |
|       - | 4352 | `	ph7_vm *pVm;` |
|       - | 4353 | `	ph7_class *pClass;` |
|       - | 4354 | `	ph7_class_instance *pThis;` |
|       - | 4355 | `	ph7_class_method *pCons;` |
|       - | 4356 | `	ph7_value sArg,sCode;` |
|       - | 4357 | `	ph7_value *apArg[2];` |
|       - | 4358 | `	SyBlob sMsg;` |
|       - | 4359 | `	SyString sMsgStr;` |
|       - | 4360 | `	VmFrame *pFrame;` |
|       - | 4361 | `	sxi32 rc;` |
|       - | 4362 |  |
|    7677 | 4363 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4364 | `		return PH7_ABORT;` |
|       - | 4365 | `	}` |
|    7677 | 4366 | `	pVm = pCtx->pVm;` |
|    7677 | 4367 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4368 | `		zClass = "Error";` |
|     ! 0 | 4369 | `	}` |
|       - | 4370 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|       - | 4371 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|       - | 4372 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|       - | 4373 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|       - | 4374 | `	 * reported error (VmHostFuncThrowRc). */` |
|    7677 | 4375 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    7677 | 4376 | `	if( pClass == 0 ){` |
|     ! 0 | 4377 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4378 | `			"Cannot throw internal exception, class '%s' is not available",` |
|     ! 0 | 4379 | `			zClass` |
|       - | 4380 | `			);` |
|     ! 0 | 4381 | `		return pCtx->nThrowRc;` |
|       - | 4382 | `	}` |
|    7677 | 4383 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    7677 | 4384 | `	if( pThis == 0 ){` |
|     ! 0 | 4385 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4386 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|       - | 4387 | `			);` |
|     ! 0 | 4388 | `		return pCtx->nThrowRc;` |
|       - | 4389 | `	}` |
|       - | 4390 |  |
|    7677 | 4391 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    7677 | 4392 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - | 4393 |  |
|    7677 | 4394 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    7677 | 4395 | `	if( pCons ){` |
|    7677 | 4396 | `		int nArg = 1;` |
|    7677 | 4397 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    7677 | 4398 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    7677 | 4399 | `		apArg[0] = &sArg;` |
|    7677 | 4400 | `		if( iCode != 0 ){` |
|      14 | 4401 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      14 | 4402 | `			apArg[1] = &sCode;` |
|      14 | 4403 | `			nArg = 2;` |
|       6 | 4404 | `		}` |
|    7677 | 4405 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    7677 | 4406 | `		if( iCode != 0 ){` |
|      14 | 4407 | `			PH7_MemObjRelease(&sCode);` |
|       6 | 4408 | `		}` |
|    7677 | 4409 | `		PH7_MemObjRelease(&sArg);` |
|    3836 | 4410 | `	}` |
|    7677 | 4411 | `	SyBlobRelease(&sMsg);` |
|       - | 4412 |  |
|    7677 | 4413 | `	pFrame = pVm->pFrame;` |
|    7677 | 4414 | `	if( pFrame ){` |
|    7677 | 4415 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    7677 | 4416 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    3836 | 4417 | `	}` |
|    7677 | 4418 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    7677 | 4419 | `	PH7_ClassInstanceUnref(pThis);` |
|    7677 | 4420 | `	if( rc == SXERR_ABORT ){` |
|     524 | 4421 | `		pCtx->nThrowRc = PH7_ABORT;` |
|     524 | 4422 | `		return PH7_ABORT;` |
|       - | 4423 | `	}` |
|       - | 4424 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|       - | 4425 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|       - | 4426 | `	 * argument-validation helper whose callers have no status channel — would` |
|       - | 4427 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|       - | 4428 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|       - | 4429 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|       - | 4430 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|       - | 4431 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|       - | 4432 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    7157 | 4433 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    7157 | 4434 | `	return PH7_EXCEPTION;` |
|    3841 | 4435 | `}` |
|    7660 | 4436 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|       5 | 4437 | `{` |
|       - | 4438 | `	va_list ap;` |
|       - | 4439 | `	sxi32 rc;` |
|    7665 | 4440 | `	va_start(ap,zFormat);` |
|    7665 | 4441 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    7665 | 4442 | `	va_end(ap);` |
|    7665 | 4443 | `	return rc;` |
|       5 | 4444 | `}` |
|       - | 4445 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      12 | 4446 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|       2 | 4447 | `{` |
|       - | 4448 | `	va_list ap;` |
|       - | 4449 | `	sxi32 rc;` |
|      14 | 4450 | `	va_start(ap,zFormat);` |
|      14 | 4451 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      14 | 4452 | `	va_end(ap);` |
|      14 | 4453 | `	return rc;` |
|       2 | 4454 | `}` |
|       - | 4455 | `/*` |
|       - | 4456 | ` * The status a host function's own throw should have returned. Consulted at the` |
|       - | 4457 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|       - | 4458 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|       - | 4459 | ` * and is the identity when the routine never threw or already reported it.` |
|       - | 4460 | ` *` |
|       - | 4461 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|       - | 4462 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|       - | 4463 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|       - | 4464 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|       - | 4465 | ` */` |
| 2740773 | 4466 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|       5 | 4467 | `{` |
| 2740773 | 4468 | `	if( pCtx->nThrowRc == 0` |
| 1374222 | 4469 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
| 2736744 | 4470 | `		return rc;` |
|       - | 4471 | `	}` |
|    4037 | 4472 | `	return pCtx->nThrowRc;` |
| 1371194 | 4473 | `}` |
|       - | 4474 | `/*` |
|       - | 4475 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|       - | 4476 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|       - | 4477 | ` */` |
|     ! 0 | 4478 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|     ! 0 | 4479 | `{` |
|       - | 4480 | `	ph7_vm *pVm;` |
|       - | 4481 | `	SyBlob sMsg;` |
|     ! 0 | 4482 | `	const char *zFuncName = 0;` |
|     ! 0 | 4483 | `	int nFuncLen = 0;` |
|       - | 4484 | `	va_list ap;` |
|       - | 4485 | `	sxi32 rc;` |
|       - | 4486 |  |
|     ! 0 | 4487 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4488 | `		return PH7_OK;` |
|       - | 4489 | `	}` |
|     ! 0 | 4490 | `	pVm = pCtx->pVm;` |
|     ! 0 | 4491 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4492 | `		zClass = "Error";` |
|     ! 0 | 4493 | `	}` |
|       - | 4494 |  |
|     ! 0 | 4495 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 4496 |  |
|     ! 0 | 4497 | `	va_start(ap,zFormat);` |
|     ! 0 | 4498 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     ! 0 | 4499 | `	va_end(ap);` |
|       - | 4500 |  |
|     ! 0 | 4501 | `	if( pCtx->pFunc ){` |
|     ! 0 | 4502 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|     ! 0 | 4503 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|     ! 0 | 4504 | `	}` |
|     ! 0 | 4505 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     ! 0 | 4506 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     ! 0 | 4507 | `	}` |
|     ! 0 | 4508 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|     ! 0 | 4509 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|     ! 0 | 4510 | `	SyBlobRelease(&sMsg);` |
|     ! 0 | 4511 | `	return rc;` |
|     ! 0 | 4512 | `}` |
|       - | 4513 | `/*` |
|       - | 4514 | ` * The following routine is invoked by the engine when an uncaught` |
|       - | 4515 | ` * exception is triggered.` |
|       - | 4516 | ` */` |
|     578 | 4517 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|       - | 4518 | `	ph7_vm *pVm, /* Target VM */` |
|       - | 4519 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4520 | `	)` |
|       4 | 4521 | `{` |
|       - | 4522 | `	ph7_value *apArg[2],sArg;` |
|     582 | 4523 | `	int nArg = 1;` |
|       - | 4524 | `	sxi32 rc;` |
|     582 | 4525 | `	if( pVm->nMuteThrow > 0 ){` |
|       - | 4526 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|       - | 4527 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|       - | 4528 | `		 * observable: no exception handler runs, no report is printed and the` |
|       - | 4529 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|       - | 4530 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|     ! 0 | 4531 | `		return SXERR_ABORT;` |
|       - | 4532 | `	}` |
|     582 | 4533 | `	if( pVm->nExceptDepth > 15 ){` |
|       - | 4534 | `		/* Nesting limit reached */` |
|     ! 0 | 4535 | `		return SXRET_OK;` |
|       - | 4536 | `	}` |
|       - | 4537 | `	/* Call any exception handler if available */` |
|     582 | 4538 | `	PH7_MemObjInit(pVm,&sArg);` |
|     582 | 4539 | `	if( pThis ){` |
|       - | 4540 | `		/* Load the exception instance */` |
|     582 | 4541 | `		sArg.x.pOther = pThis;` |
|     582 | 4542 | `		pThis->iRef++;` |
|     582 | 4543 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|     293 | 4544 | `	}else{` |
|     ! 0 | 4545 | `		nArg = 0;` |
|       - | 4546 | `	}` |
|     582 | 4547 | `	apArg[0] = &sArg;` |
|       - | 4548 | `	/* Call the exception handler if available */` |
|     582 | 4549 | `	pVm->nExceptDepth++;` |
|     582 | 4550 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|     582 | 4551 | `	pVm->nExceptDepth--;` |
|     582 | 4552 | `	if( rc != SXRET_OK ){` |
|       - | 4553 | `		const char *zFuncName;` |
|       - | 4554 | `		int nFuncLen;` |
|     580 | 4555 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       - | 4556 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|     580 | 4557 | `		if( pThis ){` |
|       - | 4558 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|       - | 4559 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|       - | 4560 | `			 * renders byte-identically to the historical single-entry report. */` |
|     580 | 4561 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|     292 | 4562 | `		}else{` |
|       - | 4563 | `			/* No instance (internal report path) — default-class single entry. */` |
|     ! 0 | 4564 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|       - | 4565 | `		}` |
|       - | 4566 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|     580 | 4567 | `		rc = SXERR_ABORT;` |
|     288 | 4568 | `	}` |
|     582 | 4569 | `	PH7_MemObjRelease(&sArg);` |
|     582 | 4570 | `	return rc;` |
|     293 | 4571 | `}` |
|       - | 4572 | `/*` |
|       - | 4573 | ` * Throw a user exception.` |
|       - | 4574 | ` *` |
|       - | 4575 | ` * Exception dispatch follows this sequence:` |
|       - | 4576 | ` *` |
|       - | 4577 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|       - | 4578 | ` *    try/catch whose catch block matches the exception class.` |
|       - | 4579 | ` *` |
|       - | 4580 | ` * 2. If NO catch matches:` |
|       - | 4581 | ` *    a. Run finally (if present) for the current try block.` |
|       - | 4582 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|       - | 4583 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|       - | 4584 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|       - | 4585 | ` *       exception in pVm->pPendingException instead of reporting it` |
|       - | 4586 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|       - | 4587 | ` *    d. Otherwise, report as truly uncaught.` |
|       - | 4588 | ` *` |
|       - | 4589 | ` * 3. If a catch DOES match:` |
|       - | 4590 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|       - | 4591 | ` *       aException stack and resetting it. This prevents a re-throw` |
|       - | 4592 | ` *       inside the catch body from immediately propagating past our` |
|       - | 4593 | ` *       finally block.` |
|       - | 4594 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|       - | 4595 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|       - | 4596 | ` *       no handlers (they're hidden), so the exception is deferred` |
|       - | 4597 | ` *       in pPendingException (step 2c).` |
|       - | 4598 | ` *    c. Restore outer handlers from the saved copy.` |
|       - | 4599 | ` *    d. Run finally (if present).` |
|       - | 4600 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|       - | 4601 | ` *       that handlers are restored and finally has run.` |
|       - | 4602 | ` */` |
|       - | 4603 | `/*` |
|       - | 4604 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|       - | 4605 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|       - | 4606 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|       - | 4607 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|       - | 4608 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|       - | 4609 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|       - | 4610 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|       - | 4611 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|       - | 4612 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|       - | 4613 | ` */` |
|     136 | 4614 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|       5 | 4615 | `{` |
|     149 | 4616 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|      45 | 4617 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      45 | 4618 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|      45 | 4619 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|     ! 0 | 4620 | `			break; /* reached an outer exec's / legacy handler */` |
|       - | 4621 | `		}` |
|      45 | 4622 | `		(void)SySetPop(&pVm->aException);` |
|      45 | 4623 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|      45 | 4624 | `		if( pT->iHasFinally ){` |
|      37 | 4625 | `			*pPc = pT->iFinallyPc;` |
|      37 | 4626 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|      37 | 4627 | `			return 1;` |
|       - | 4628 | `		}` |
|       - | 4629 | `		/* No finally: tear the try's transparent frame down now. */` |
|      11 | 4630 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       6 | 4631 | `			VmLeaveFrame(&(*pVm));` |
|       2 | 4632 | `		}` |
|      11 | 4633 | `		VmExcRelease(&(*pVm),pT);` |
|       3 | 4634 | `	}` |
|     107 | 4635 | `	return 0;` |
|      73 | 4636 | `}` |
|       - | 4637 | `/*` |
|       - | 4638 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|       - | 4639 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|       - | 4640 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|       - | 4641 | ` *` |
|       - | 4642 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|       - | 4643 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|       - | 4644 | ` *    and redirect to the catch body (iHandlerPc).` |
|       - | 4645 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|       - | 4646 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|       - | 4647 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|       - | 4648 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|       - | 4649 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|       - | 4650 | ` */` |
|       - | 4651 | `/*` |
|       - | 4652 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|       - | 4653 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|       - | 4654 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|       - | 4655 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|       - | 4656 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|       - | 4657 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|       - | 4658 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|       - | 4659 | ` * case) is unchanged: no wrapper.` |
|       - | 4660 | ` */` |
|   20250 | 4661 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|       5 | 4662 | `{` |
|   20255 | 4663 | `	VmFrame *pWrap = 0;` |
|       - | 4664 | `	VmFrame *pThrowSite;` |
|       - | 4665 | `	sxi32 rc;` |
|   20255 | 4666 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|   20151 | 4667 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4668 | `	}` |
|     107 | 4669 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|       - | 4670 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|     ! 0 | 4671 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4672 | `	}` |
|     107 | 4673 | `	pThrowSite = pWrap->pParent;` |
|     107 | 4674 | `	pWrap->pParent = pOwner;` |
|     107 | 4675 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|     107 | 4676 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4677 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|       - | 4678 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|       - | 4679 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|       - | 4680 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|     107 | 4681 | `	if( pVm->pFrame == pWrap ){` |
|     107 | 4682 | `		VmLeaveFrame(&(*pVm));` |
|      52 | 4683 | `	}` |
|     107 | 4684 | `	pVm->pFrame = pThrowSite;` |
|     107 | 4685 | `	return rc;` |
|   10130 | 4686 | `}` |
|       - | 4687 | `/*` |
|       - | 4688 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|       - | 4689 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|       - | 4690 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|       - | 4691 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|       - | 4692 | ` */` |
|       - | 4693 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      92 | 4694 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|       - | 4695 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|       5 | 4696 | `{` |
|      97 | 4697 | `	if( pCatch ){` |
|      81 | 4698 | `		pException->iInCatch = 1;` |
|      81 | 4699 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|      81 | 4700 | `		if( pThis ){ pThis->iRef++; }` |
|      81 | 4701 | `		pException->pInflight = pThis;` |
|      81 | 4702 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      81 | 4703 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|      81 | 4704 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      81 | 4705 | `		return SXRET_OK;` |
|       - | 4706 | `	}` |
|      20 | 4707 | `	if( pException->iHasFinally ){` |
|       - | 4708 | `		VmFinallyAction sAct;` |
|      15 | 4709 | `		SyZero(&sAct,sizeof(sAct));` |
|      15 | 4710 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      15 | 4711 | `		if( pThis ){ pThis->iRef++; }` |
|      15 | 4712 | `		sAct.pExc = pThis;` |
|      15 | 4713 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      15 | 4714 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      15 | 4715 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      15 | 4716 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      15 | 4717 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      15 | 4718 | `		return SXRET_OK;` |
|       - | 4719 | `	}` |
|       - | 4720 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|       - | 4721 | `	 * flat native stack instead of mutual recursion. */` |
|       6 | 4722 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     ! 0 | 4723 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 | 4724 | `	}` |
|       6 | 4725 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       6 | 4726 | `	return VM_THROW_KEEP_UNWINDING;` |
|      51 | 4727 | `}` |
| 1455316 | 4728 | `PH7_PRIVATE sxi32 VmThrowException(` |
|       - | 4729 | `	ph7_vm *pVm,              /* Target VM */` |
|       - | 4730 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4731 | `	)` |
|       5 | 4732 | `{` |
|       - | 4733 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|       - | 4734 | `	ph7_exception **apException;` |
|  727658 | 4735 | `	ph7_exception *pException;` |
|   50101 | 4736 | `Rethrow:` |
|       - | 4737 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|       - | 4738 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|       - | 4739 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|       - | 4740 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|       - | 4741 | `	 * so the throw path must be too). */` |
|       - | 4742 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|       - | 4743 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|       - | 4744 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
| 1555523 | 4745 | `	VmCoalesceDisarm(pVm);` |
|       - | 4746 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|       - | 4747 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|       - | 4748 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|       - | 4749 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|       - | 4750 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|       - | 4751 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|       - | 4752 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|       - | 4753 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
| 1555518 | 4754 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|      25 | 4755 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|      18 | 4756 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|       8 | 4757 | `	}` |
|       - | 4758 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|       - | 4759 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|       - | 4760 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|       - | 4761 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|       - | 4762 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|       - | 4763 | `	 * that owns the pending return, so it must leave that return intact. */` |
|       - | 4764 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|       - | 4765 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|       - | 4766 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|       - | 4767 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
| 1555523 | 4768 | `	pVm->pResumeFrame = 0;` |
|       - | 4769 | `	/* Point to the stack of loaded exceptions */` |
| 1555523 | 4770 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
| 1555523 | 4771 | `	pException = 0;` |
| 1555523 | 4772 | `	pCatch = 0;` |
| 1555523 | 4773 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 4774 | `		ph7_exception_block *aCatch;` |
|       - | 4775 | `		ph7_class *pClass;` |
|       - | 4776 | `		SyString *aNames;` |
|       - | 4777 | `		sxu32 nNames;` |
|       - | 4778 | `		int matched;` |
|       - | 4779 | `		sxu32 j,k;` |
|       - | 4780 | `		/* Locate the appropriate block to execute */` |
| 1454825 | 4781 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
| 1454825 | 4782 | `		(void)SySetPop(&pVm->aException);` |
| 1454825 | 4783 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       - | 4784 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|       - | 4785 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|       - | 4786 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
| 1454843 | 4787 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       - | 4788 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
| 1434657 | 4789 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
| 1434657 | 4790 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
| 1434657 | 4791 | `			matched = 0;` |
| 1434701 | 4792 | `			for( k = 0 ; k < nNames ; ++k ){` |
|       - | 4793 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|       - | 4794 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|       - | 4795 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
| 1434683 | 4796 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
| 1434683 | 4797 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 4798 | `					/* No such class, or trait — cannot match */` |
|     ! 0 | 4799 | `					continue;` |
|       - | 4800 | `				}` |
| 1434683 | 4801 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
| 1434639 | 4802 | `					matched = 1;` |
| 1434639 | 4803 | `					break;` |
|       - | 4804 | `				}` |
|      26 | 4805 | `			}` |
| 1434657 | 4806 | `			if( matched ){` |
|       - | 4807 | `				/* Catch block found,break immediately */` |
| 1434639 | 4808 | `				pCatch = &aCatch[j];` |
| 1434639 | 4809 | `				break;` |
|       - | 4810 | `			}` |
|      12 | 4811 | `		}` |
|  727410 | 4812 | `	}` |
|       - | 4813 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|       - | 4814 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|       - | 4815 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|       - | 4816 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|       - | 4817 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|       - | 4818 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|       - | 4819 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
| 1555523 | 4820 | `	if( pException ){` |
| 1454825 | 4821 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|  727410 | 4822 | `	}` |
|       - | 4823 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|       - | 4824 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|       - | 4825 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
| 1555523 | 4826 | `	if( pException && pException->iInlined ){` |
|      97 | 4827 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      97 | 4828 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|       - | 4829 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|       6 | 4830 | `			goto Rethrow;` |
|       - | 4831 | `		}` |
|      93 | 4832 | `		return rcInline;` |
|       - | 4833 | `	}` |
|       - | 4834 | `	/* Execute the cached block if available */` |
| 1555431 | 4835 | `	if( pCatch == 0 ){` |
|       - | 4836 | `		sxi32 rc;` |
|       - | 4837 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|  120873 | 4838 | `		if( pException && pException->iHasFinally ){` |
|   20171 | 4839 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|   20171 | 4840 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|   20171 | 4841 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|   20171 | 4842 | `			pException->iFinallyDone = 1;` |
|       - | 4843 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|       - | 4844 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|   20171 | 4845 | `			pVm->pInflightException = pThis;` |
|   20171 | 4846 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 4847 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|       - | 4848 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|   20171 | 4849 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|   20171 | 4850 | `			pVm->pInflightException = pSaveInflight;` |
|   20171 | 4851 | `			pVm->nInflightExcBase = nSaveBase;` |
|   20171 | 4852 | `			if( rc == SXERR_ABORT ){` |
|       3 | 4853 | `				VmExcRelease(&(*pVm),pException);` |
|       3 | 4854 | `				return SXERR_ABORT;` |
|       - | 4855 | `			}` |
|       - | 4856 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|       - | 4857 | `			 * semantics). The finally stored it on the body frame it returns from` |
|       - | 4858 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|       - | 4859 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|       - | 4860 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|       - | 4861 | `			 * takes the value instead of unwinding) and resume in place.` |
|       - | 4862 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|       - | 4863 | `			 * the same transport an in-place catch uses — and unwind as an` |
|       - | 4864 | `			 * exception; the owner's activation consumes the resume` |
|       - | 4865 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|       - | 4866 | `			 * its bHasRet tail materializes the return. */` |
|       - | 4867 | `			{` |
|   20169 | 4868 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   20169 | 4869 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|   20169 | 4870 | `				if( pOwnerFrame->bHasRet ){` |
|   20029 | 4871 | `					if( pOwnerFrame == pThrowFrame ){` |
|   20026 | 4872 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|       - | 4873 | `						/* Record the landing pad like the cross-frame case below.` |
|       - | 4874 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|       - | 4875 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|       - | 4876 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|       - | 4877 | `						 * its router unwound as an exception and the Unwind discard` |
|       - | 4878 | ``						 * dropped the parked return — `function f(){ try {`` |
|       - | 4879 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|       - | 4880 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|       - | 4881 | `						 * try frame down and its bHasRet tail materializes the` |
|       - | 4882 | `						 * return, same as the in-place-catch landing. */` |
|   20026 | 4883 | `						pVm->pResumeFrame = pOwnerFrame;` |
|   20026 | 4884 | `						pVm->iResumePc = pException->iLandingPc;` |
|   20026 | 4885 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|   20026 | 4886 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|   20026 | 4887 | `						VmExcRelease(&(*pVm),pException);` |
|   20026 | 4888 | `						return SXRET_OK;` |
|       - | 4889 | `					}` |
|       3 | 4890 | `					pVm->pResumeFrame = pOwnerFrame;` |
|       3 | 4891 | `					pVm->iResumePc = pException->iLandingPc;` |
|       3 | 4892 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|       3 | 4893 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|       3 | 4894 | `					VmExcRelease(&(*pVm),pException);` |
|       3 | 4895 | `					return PH7_EXCEPTION;` |
|       - | 4896 | `				}` |
|       - | 4897 | `			}` |
|       - | 4898 | `			/* The finally threw an exception that superseded pThis — it either` |
|       - | 4899 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|       - | 4900 | `			 * (which consumed an entry from the exception stack). Either way the` |
|       - | 4901 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|       - | 4902 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|     142 | 4903 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      16 | 4904 | `				VmExcRelease(&(*pVm),pException);` |
|      16 | 4905 | `				return PH7_EXCEPTION;` |
|       - | 4906 | `			}` |
|      62 | 4907 | `		}` |
|       - | 4908 | `		/* Check if there is an outer exception handler on the stack */` |
|  100831 | 4909 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 4910 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|       - | 4911 | `			 * iteration per unwound level instead of one native frame. */` |
|     132 | 4912 | `			VmExcRelease(&(*pVm),pException);` |
|     132 | 4913 | `			goto Rethrow;` |
|       - | 4914 | `		}` |
|  100703 | 4915 | `		if( pVm->nMuteThrow > 0 ){` |
|       - | 4916 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|       - | 4917 | `			 * default at class mount, which php would not have evaluated yet).` |
|       - | 4918 | `			 * Nothing outside the initializer may observe this throw: no` |
|       - | 4919 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|       - | 4920 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|       - | 4921 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|       - | 4922 | `			 * the mount path rolls the whole attempt back. */` |
|      50 | 4923 | `			VmExcRelease(&(*pVm),pException);` |
|      50 | 4924 | `			return SXERR_ABORT;` |
|       - | 4925 | `		}` |
|       - | 4926 | `		/* No outer handler. If the handlers were temporarily hidden` |
|       - | 4927 | `		 * (catch body re-throw with finally pending), defer the` |
|       - | 4928 | `		 * exception instead of reporting it uncaught.` |
|       - | 4929 | `		 */` |
|  100657 | 4930 | `		if( pVm->pPendingException == 0 && pThis ){` |
|       - | 4931 | `			/* Check if we are inside a catch execution with hidden handlers` |
|       - | 4932 | `			 * by looking for a catch frame on the stack.` |
|       - | 4933 | `			 */` |
|  100657 | 4934 | `			VmFrame *pF = pVm->pFrame;` |
|  100657 | 4935 | `			int inCatch = 0;` |
|  101269 | 4936 | `			while( pF ){` |
|  100691 | 4937 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|  100078 | 4938 | `					inCatch = 1;` |
|  100078 | 4939 | `					break;` |
|       - | 4940 | `				}` |
|     616 | 4941 | `				pF = pF->pParent;` |
|       4 | 4942 | `			}` |
|  100657 | 4943 | `			if( inCatch ){` |
|       - | 4944 | `				/* Defer — will be re-thrown after finally runs */` |
|  100078 | 4945 | `				pThis->iRef++;` |
|  100078 | 4946 | `				pVm->pPendingException = pThis;` |
|  100078 | 4947 | `				VmExcRelease(&(*pVm),pException);` |
|  100078 | 4948 | `				return SXRET_OK;` |
|       - | 4949 | `			}` |
|     289 | 4950 | `		}` |
|       - | 4951 | `		/* Truly uncaught */` |
|     582 | 4952 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|     582 | 4953 | `		if( rc == SXRET_OK && pException ){` |
|     ! 0 | 4954 | `			VmFrame *pFrame = pVm->pFrame;` |
|     ! 0 | 4955 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|     ! 0 | 4956 | `			if( pException->pFrame == pFrame ){` |
|     ! 0 | 4957 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|     ! 0 | 4958 | `			}` |
|     ! 0 | 4959 | `		}` |
|     582 | 4960 | `		VmExcRelease(&(*pVm),pException);` |
|     582 | 4961 | `		return rc;` |
|     ! 0 | 4962 | `	}else{` |
| 1434563 | 4963 | `		VmFrame *pFrame = pVm->pFrame;` |
| 1434563 | 4964 | `		ph7_exception **apSaved = 0;` |
|       - | 4965 | `		sxu32 nSavedCount;` |
|       - | 4966 | `		sxi32 rc;` |
|       - | 4967 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|       - | 4968 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|       - | 4969 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|       - | 4970 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|       - | 4971 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
| 1434563 | 4972 | `		VmFrame *pCatchBody = pException->pFrame;` |
| 1434563 | 4973 | `		sxu32 iCatchPc = pException->iLandingPc;` |
| 1434563 | 4974 | `		void *pCatchInstr = pException->pOwnerInstr;` |
| 1434563 | 4975 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
| 1434563 | 4976 | `		if( pException->pFrame == pFrame ){` |
|  827261 | 4977 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|  413628 | 4978 | `		}` |
|       - | 4979 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|       - | 4980 | `		 * body re-throws, the exception does not immediately propagate past` |
|       - | 4981 | `		 * our finally block. We save the stack contents and restore after.` |
|       - | 4982 | `		 */` |
| 1434563 | 4983 | `		nSavedCount = SySetUsed(&pVm->aException);` |
| 1434563 | 4984 | `		if( nSavedCount > 0 ){` |
|  150275 | 4985 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   50090 | 4986 | `				nSavedCount * sizeof(ph7_exception *));` |
|  100185 | 4987 | `			if( apSaved ){` |
|  150275 | 4988 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|   50090 | 4989 | `					nSavedCount * sizeof(ph7_exception *));` |
|  100185 | 4990 | `				SySetReset(&pVm->aException);` |
|   50090 | 4991 | `			}` |
|   50090 | 4992 | `		}` |
|       - | 4993 | `		/* Create the catch frame (made transparent below) */` |
| 1434563 | 4994 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
| 1434563 | 4995 | `		if( rc == SXRET_OK ){` |
|       - | 4996 | `			ph7_value *pObj;` |
|       - | 4997 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|       - | 4998 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|       - | 4999 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|       - | 5000 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|       - | 5001 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|       - | 5002 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|       - | 5003 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|       - | 5004 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|       - | 5005 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
| 1434563 | 5006 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|       - | 5007 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|       - | 5008 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|       - | 5009 | `			 * against the live current scope rather than a freed frame. */` |
| 1434563 | 5010 | `			if( pCatchBody ){` |
| 1434563 | 5011 | `				pFrame->pParent = pCatchBody;` |
|  717279 | 5012 | `			}` |
|       - | 5013 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|       - | 5014 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|       - | 5015 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|       - | 5016 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|       - | 5017 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|       - | 5018 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|       - | 5019 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|       - | 5020 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
| 1434563 | 5021 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|       - | 5022 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
| 2151842 | 5023 | `			pObj = (pCatch->sThis.nByte > 0)` |
| 1434556 | 5024 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
| 1434563 | 5025 | `			if( pObj ){` |
|       - | 5026 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|       - | 5027 | `				 * so it may already hold a value from a prior catch or assignment.` |
|       - | 5028 | `				 * Pin the new instance, then release the slot's prior contents` |
|       - | 5029 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|       - | 5030 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|       - | 5031 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
| 1434559 | 5032 | `				pThis->iRef++;` |
| 1434559 | 5033 | `				PH7_MemObjRelease(pObj);` |
| 1434559 | 5034 | `				pObj->x.pOther = pThis;` |
| 1434559 | 5035 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|  717277 | 5036 | `			}` |
|       - | 5037 | `			/* Execute the catch block */` |
| 1434563 | 5038 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|       - | 5039 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|       - | 5040 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|       - | 5041 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|       - | 5042 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|       - | 5043 | `			 * unbalanced — never pop somebody else's frame. */` |
| 1434563 | 5044 | `			if( pVm->pFrame == pFrame ){` |
| 1434563 | 5045 | `				VmLeaveFrame(&(*pVm));` |
|  717279 | 5046 | `			}` |
| 1434563 | 5047 | `			pVm->pFrame = pThrowSite;` |
|  717279 | 5048 | `		}` |
|       - | 5049 | `		/* Restore the outer exception handlers */` |
| 1434563 | 5050 | `		if( apSaved ){` |
|       - | 5051 | `			sxu32 k;` |
|       - | 5052 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|       - | 5053 | `			 * the catch body) are normally already consumed; on an abnormal` |
|       - | 5054 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|       - | 5055 | `			 * linger — release those activations before discarding the set. */` |
|  100185 | 5056 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|  100185 | 5057 | `			SySetReset(&pVm->aException);` |
|  201017 | 5058 | `			for(k = 0; k < nSavedCount; k++){` |
|  100837 | 5059 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|   50421 | 5060 | `			}` |
|  100185 | 5061 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|   50090 | 5062 | `		}` |
|       - | 5063 | `		/* Execute the finally block after catch */` |
| 1434563 | 5064 | `		if( pException->iHasFinally ){` |
|       - | 5065 | `			sxi32 rcf;` |
|       - | 5066 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|       - | 5067 | `			 * from, its pending-return write generation (set if the catch above` |
|       - | 5068 | `			 * returned), and the exception-stack depth. After the finally we use` |
|       - | 5069 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|       - | 5070 | `			 * catch-return. */` |
|       - | 5071 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|       - | 5072 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|       - | 5073 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|       - | 5074 | `			 * supersede decision belong to the owner, not the thrower. */` |
|      89 | 5075 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|      89 | 5076 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|      89 | 5077 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|       - | 5078 | `			/* The exception in flight while this finally runs is the catch body's` |
|       - | 5079 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|       - | 5080 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|       - | 5081 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|       - | 5082 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|      89 | 5083 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|      89 | 5084 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|      89 | 5085 | `			pException->iFinallyDone = 1;` |
|      89 | 5086 | `			pVm->pInflightException = pVm->pPendingException;` |
|      89 | 5087 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 5088 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|      89 | 5089 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|      89 | 5090 | `			pVm->pInflightException = pSaveInflight;` |
|      89 | 5091 | `			pVm->nInflightExcBase = nSaveBase;` |
|      89 | 5092 | `			if( rcf == SXERR_ABORT ){` |
|     ! 0 | 5093 | `				VmExcRelease(&(*pVm),pException);` |
|     ! 0 | 5094 | `				return SXERR_ABORT;` |
|       - | 5095 | `			}` |
|       - | 5096 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|       - | 5097 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|       - | 5098 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|       - | 5099 | `			 * either case that exception supersedes this try's catch-return — but` |
|       - | 5100 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|       - | 5101 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|       - | 5102 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|      89 | 5103 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      19 | 5104 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|      12 | 5105 | `					VmClearFramePending(pBody);` |
|       5 | 5106 | `				}` |
|       - | 5107 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|       - | 5108 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|       - | 5109 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|       - | 5110 | `				 * legitimately have re-armed it during the finally. */` |
|      19 | 5111 | `				pBody->nCatchJmpPc = 0;` |
|       8 | 5112 | `			}` |
|      89 | 5113 | `			if( rcf == PH7_EXCEPTION ){` |
|       - | 5114 | `				/* The finally's exception propagated past this try; drop any deferred` |
|       - | 5115 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|       - | 5116 | `				 * reaches the frame that caught the finally's throw. */` |
|      19 | 5117 | `				if( pVm->pPendingException ){` |
|     ! 0 | 5118 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|     ! 0 | 5119 | `					pVm->pPendingException = 0;` |
|     ! 0 | 5120 | `				}` |
|      19 | 5121 | `				VmExcRelease(&(*pVm),pException);` |
|      19 | 5122 | `				return PH7_EXCEPTION;` |
|       - | 5123 | `			}` |
|      34 | 5124 | `		}` |
| 1434547 | 5125 | `		if( rc == SXERR_ABORT ){` |
|       5 | 5126 | `			VmExcRelease(&(*pVm),pException);` |
|       5 | 5127 | `			return SXERR_ABORT;` |
|       - | 5128 | `		}` |
|       - | 5129 | `		/* If the catch body re-threw, the exception was deferred in` |
|       - | 5130 | `		 * pPendingException (because outer handlers were hidden).` |
|       - | 5131 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|       - | 5132 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|       - | 5133 | `		 * the catch frame having been left above), which swallows the in-flight` |
|       - | 5134 | `		 * exception (PHP semantics).` |
|       - | 5135 | `		 */` |
| 1434543 | 5136 | `		if( pVm->pPendingException ){` |
|       - | 5137 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|  100078 | 5138 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|  100078 | 5139 | `			if( !pOwner->bHasRet ){` |
|  100074 | 5140 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|       - | 5141 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|       - | 5142 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|       - | 5143 | `				 * than leave it armed for an unrelated later landing. */` |
|  100074 | 5144 | `				pOwner->nCatchJmpPc = 0;` |
|  100074 | 5145 | `				pVm->pPendingException = 0;` |
|  100074 | 5146 | `				VmExcRelease(&(*pVm),pException);` |
|       - | 5147 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|  100074 | 5148 | `				pThis = pReThrow;` |
|  100074 | 5149 | `				goto Rethrow;` |
|       - | 5150 | `			}` |
|       - | 5151 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|       6 | 5152 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|       6 | 5153 | `			pVm->pPendingException = 0;` |
|       2 | 5154 | `		}` |
|       - | 5155 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|       - | 5156 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|       - | 5157 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|       - | 5158 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
| 1334473 | 5159 | `		pVm->pResumeFrame = pCatchBody;` |
| 1334473 | 5160 | `		pVm->iResumePc = iCatchPc;` |
| 1334473 | 5161 | `		pVm->pResumeInstr = pCatchInstr;` |
| 1334473 | 5162 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|       - | 5163 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|       - | 5164 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|       - | 5165 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
| 1334473 | 5166 | `		VmExcRelease(&(*pVm),pException);` |
|       - | 5167 | `	}` |
| 1334473 | 5168 | `	return SXRET_OK;` |
|  727663 | 5169 | `}` |
|       - | 5170 |  |
