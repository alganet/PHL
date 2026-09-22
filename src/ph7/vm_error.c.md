# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2362/2647 lines (89.23%)

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
|   22196 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|       5 |   25 | `{` |
|   22201 |   26 | `	pVm->nLastErrType = iErr;` |
|   22201 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|   22201 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|   22201 |   29 | `	if( zMsg && nMsg > 0 ){` |
|   22201 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   11098 |   31 | `	}` |
|   22201 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|   22201 |   33 | `	if( pFile ){` |
|   22201 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   11098 |   35 | `	}` |
|   22201 |   36 | `}` |
|       - |   37 | `/*` |
|       - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|       - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|       - |   40 | ` * diagnostic is never silently swallowed.` |
|       - |   41 | ` */` |
|     728 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|       4 |   43 | `{` |
|     732 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|       4 |   45 | `}` |
|       - |   46 | `/*` |
|       - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|       - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|       - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|       - |   50 | ` * headers_sent()/output accounting).` |
|       - |   51 | ` */` |
|     754 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|       4 |   53 | `{` |
|       - |   54 | `	sxi32 rc;` |
|       - |   55 | `	/* Append a new line */` |
|       - |   56 | `#ifdef __WINNT__` |
|       4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|       - |   58 | `#else` |
|     754 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|       - |   60 | `#endif` |
|       - |   61 | `	/* Invoke the output consumer callback */` |
|     758 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     758 |   63 | `	if( bTrack ){` |
|      29 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      13 |   65 | `	}` |
|     758 |   66 | `	return rc;` |
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
|   22758 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|       5 |   90 | `{` |
|   22763 |   91 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|       - |   92 | `		ph7_value apArg[4];` |
|       - |   93 | `		ph7_value *apArgPtr[4];` |
|       - |   94 | `		ph7_value sResult;` |
|       - |   95 | `		SyString sErr;` |
|       - |   96 | `		/* Prepare arguments */` |
|     571 |   97 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|       - |   98 | `			/* use explicit message length to avoid reading past buffer */` |
|     571 |   99 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     571 |  100 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     571 |  101 | `		if( pFile ){` |
|     571 |  102 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     571 |  103 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     288 |  104 | `		}else{` |
|     ! 0 |  105 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|       - |  106 | `		}` |
|     571 |  107 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     571 |  108 | `		PH7_MemObjInit(pVm,&sResult);` |
|       - |  109 | `		/* Set up pointer array */` |
|     571 |  110 | `		apArgPtr[0] = &apArg[0];` |
|     571 |  111 | `		apArgPtr[1] = &apArg[1];` |
|     571 |  112 | `		apArgPtr[2] = &apArg[2];` |
|     571 |  113 | `		apArgPtr[3] = &apArg[3];` |
|       - |  114 | `		/* Call the handler */` |
|       - |  115 | `		{` |
|     571 |  116 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|     571 |  117 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|       - |  118 | `				/* The handler threw (or aborted) instead of returning: php never` |
|       - |  119 | `				 * reports the original diagnostic then — the exception supersedes` |
|       - |  120 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|       - |  121 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|       - |  122 | `				 * user's catch already ran. */` |
|       3 |  123 | `				PH7_MemObjRelease(&apArg[0]);` |
|       3 |  124 | `				PH7_MemObjRelease(&apArg[1]);` |
|       3 |  125 | `				PH7_MemObjRelease(&apArg[2]);` |
|       3 |  126 | `				PH7_MemObjRelease(&apArg[3]);` |
|       3 |  127 | `				PH7_MemObjRelease(&sResult);` |
|       3 |  128 | `				return FALSE;` |
|       - |  129 | `			}` |
|       - |  130 | `		}` |
|       - |  131 | `		/* Check return value */` |
|     569 |  132 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 |  133 | `			PH7_MemObjToBool(&sResult);` |
|     ! 0 |  134 | `		}` |
|       - |  135 | `		/* Release */` |
|     569 |  136 | `		PH7_MemObjRelease(&apArg[0]);` |
|     569 |  137 | `		PH7_MemObjRelease(&apArg[1]);` |
|     569 |  138 | `		PH7_MemObjRelease(&apArg[2]);` |
|     569 |  139 | `		PH7_MemObjRelease(&apArg[3]);` |
|     569 |  140 | `		PH7_MemObjRelease(&sResult);` |
|       - |  141 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|       - |  142 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     569 |  143 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|       - |  144 | `	}` |
|       - |  145 | `	/* No handler, always call error handler */` |
|   22197 |  146 | `	return TRUE;` |
|   11384 |  147 | `}` |
|       - |  148 | `/*` |
|       - |  149 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|       - |  150 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|       - |  151 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|       - |  152 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|       - |  153 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|       - |  154 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|       - |  155 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|       - |  156 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|       - |  157 | ` * only picks the DISPLAY label.` |
|       - |  158 | ` */` |
|       - |  159 | `/*` |
|       - |  160 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|       - |  161 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|       - |  162 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|       - |  163 | ` * every deprecation.` |
|       - |  164 | ` */` |
|   22196 |  165 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|       5 |  166 | `{` |
|       - |  167 | `	sxi32 iBit;` |
|   22201 |  168 | `	if( !pVm->bErrReport ){` |
|    3739 |  169 | `		return 0;` |
|       - |  170 | `	}` |
|   18465 |  171 | `	switch( iErr ){` |
|    9195 |  172 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|   18395 |  173 | `		iBit = 2; break;` |
|       6 |  174 | `	case 512  /* E_USER_WARNING */:` |
|      15 |  175 | `		iBit = 512; break;` |
|      13 |  176 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|       - |  177 | `	case 8    /* E_NOTICE */:` |
|      28 |  178 | `		iBit = 8; break;` |
|       4 |  179 | `	case 1024 /* E_USER_NOTICE */:` |
|      11 |  180 | `		iBit = 1024; break;` |
|     ! 0 |  181 | `	case 8192 /* E_DEPRECATED */:` |
|     ! 0 |  182 | `		iBit = 8192; break;` |
|     ! 0 |  183 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  184 | `		iBit = 16384; break;` |
|     ! 0 |  185 | `	case 256  /* E_USER_ERROR */:` |
|     ! 0 |  186 | `		iBit = 256; break;` |
|      12 |  187 | `	default:` |
|      28 |  188 | `		iBit = 1; /* E_ERROR and everything else fatal-ish */` |
|      24 |  189 | `		break;` |
|       - |  190 | `	}` |
|   18465 |  191 | `	return (pVm->iErrMask & iBit) != 0;` |
|   11103 |  192 | `}` |
|     174 |  193 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|       4 |  194 | `{` |
|     178 |  195 | `	switch(iErr){` |
|      58 |  196 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|       - |  197 | `	case 512  /* E_USER_WARNING */:` |
|     120 |  198 | `		return "Warning";` |
|      17 |  199 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|       - |  200 | `	case 8    /* E_NOTICE */:` |
|       - |  201 | `	case 1024 /* E_USER_NOTICE */:` |
|      38 |  202 | `		return "Notice";` |
|     ! 0 |  203 | `	case 8192  /* E_DEPRECATED */:` |
|       - |  204 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  205 | `		return "Deprecated";` |
|     ! 0 |  206 | `	case 256 /* E_USER_ERROR */:` |
|     ! 0 |  207 | `		return "Fatal error";` |
|      12 |  208 | `	default:` |
|      28 |  209 | `		return "Error";` |
|       - |  210 | `	}` |
|      91 |  211 | `}` |
|       - |  212 | `/*` |
|       - |  213 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|       - |  214 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|       - |  215 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|       - |  216 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|       - |  217 | ` * be authored cross-engine with --EXPECTF--.` |
|       - |  218 | ` */` |
|      26 |  219 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|       3 |  220 | `{` |
|      29 |  221 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|      29 |  222 | `}` |
|     174 |  223 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|       4 |  224 | `{` |
|     178 |  225 | `	if( pFile ){` |
|     265 |  226 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|      87 |  227 | `			nLine ? nLine : 1);` |
|      87 |  228 | `	}` |
|     178 |  229 | `}` |
|       - |  230 | `/*` |
|       - |  231 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|       - |  232 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|       - |  233 | ` * (compile.c) and stock CLI's stderr log copy.` |
|       - |  234 | ` */` |
|     148 |  235 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|       4 |  236 | `{` |
|     152 |  237 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|     152 |  238 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|     152 |  239 | `}` |
|       - |  240 | `/*` |
|       - |  241 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|       - |  242 | ` *` |
|       - |  243 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|       - |  244 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|       - |  245 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|       - |  246 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|       - |  247 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|       - |  248 | ` * php never produces. Building it into the message here is the single place that` |
|       - |  249 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|       - |  250 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|       - |  251 | ` */` |
|   22474 |  252 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|       5 |  253 | `{` |
|   22479 |  254 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     194 |  255 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     194 |  256 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      95 |  257 | `	}` |
|   22479 |  258 | `}` |
|       - |  259 | `/*` |
|       - |  260 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|       - |  261 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|       - |  262 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|       - |  263 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|       - |  264 | ` *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:` |
|       - |  265 | ``  *                     `\nLABEL: BODY in FILE on line N` `` |
|       - |  266 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|       - |  267 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|       - |  268 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|       - |  269 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|       - |  270 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|       - |  271 | ` */` |
|     170 |  272 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|       - |  273 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|       4 |  274 | `{` |
|     174 |  275 | `	SyBlob *pWorker = &pVm->sWorker;` |
|     174 |  276 | `	sxi32 rc = SXRET_OK;` |
|     174 |  277 | `	if( pVm->bLogErrors ){` |
|     152 |  278 | `		SyBlobReset(pWorker);` |
|     152 |  279 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|     152 |  280 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|     152 |  281 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|     152 |  282 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      74 |  283 | `	}` |
|     174 |  284 | `	if( pVm->bDisplayErrors ){` |
|       - |  285 | `		sxi32 rc2;` |
|      29 |  286 | `		SyBlobReset(pWorker);` |
|       - |  287 | `		/* php's text-mode display copy is prefixed with a blank line */` |
|      29 |  288 | `		SyBlobAppend(pWorker,"\n",sizeof(char));` |
|      29 |  289 | `		VmDiagnosticHeader(pWorker,iErr);` |
|      29 |  290 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      29 |  291 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      29 |  292 | `		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);` |
|       - |  293 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|       - |  294 | `		 * than letting a later successful write mask it */` |
|      29 |  295 | `		if( rc == SXRET_OK ){` |
|      29 |  296 | `			rc = rc2;` |
|      13 |  297 | `		}` |
|      13 |  298 | `	}` |
|     174 |  299 | `	return rc;` |
|       4 |  300 | `}` |
|     292 |  301 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|       - |  302 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  303 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  304 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|       - |  305 | `	const char *zMessage /* Null terminated error message */` |
|       - |  306 | `	)` |
|       5 |  307 | `{` |
|       - |  308 | `	SyBlob sMsg;` |
|       - |  309 | `	SyString *pFile;` |
|     297 |  310 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     297 |  311 | `	sxi32 rc = SXRET_OK;` |
|       - |  312 | `	/* Peek the processed file if available */` |
|     297 |  313 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     297 |  314 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     297 |  315 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|       - |  316 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|       - |  317 | `		 * out-of-memory fatal through this path with none, and must not need` |
|       - |  318 | `		 * an allocation to say so. */` |
|      11 |  319 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      11 |  320 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      11 |  321 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      11 |  322 | `		nMsg = SyBlobLength(&sMsg);` |
|       4 |  323 | `	}` |
|       - |  324 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|       - |  325 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     297 |  326 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|     100 |  327 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|     100 |  328 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|       - |  329 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|       - |  330 | `			 * '@' php still runs the handler (done just above) but prints` |
|       - |  331 | `			 * nothing itself. */` |
|      96 |  332 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|      46 |  333 | `		}` |
|      48 |  334 | `	}` |
|     297 |  335 | `	SyBlobRelease(&sMsg);` |
|     297 |  336 | `	return rc;` |
|       5 |  337 | `}` |
|       - |  338 | `/*` |
|       - |  339 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|       - |  340 | ` *` |
|       - |  341 | ` * This is the single choke point for surfacing an allocation failure that would` |
|       - |  342 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|       - |  343 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|       - |  344 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|       - |  345 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|       - |  346 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|       - |  347 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|       - |  348 | ` * calling it from a VM op.` |
|       - |  349 | ` */` |
|     ! 0 |  350 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|     ! 0 |  351 | `{` |
|     ! 0 |  352 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|       - |  353 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|     ! 0 |  354 | `	pVm->iExitStatus = 255;` |
|     ! 0 |  355 | `	pVm->bHaltRequested = 1;` |
|     ! 0 |  356 | `	return PH7_ABORT;` |
|     ! 0 |  357 | `}` |
|       - |  358 | `/*` |
|       - |  359 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|       - |  360 | ` */` |
|     ! 0 |  361 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|     ! 0 |  362 | `{` |
|     ! 0 |  363 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|     ! 0 |  364 | `}` |
|       - |  365 | `/*` |
|       - |  366 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|       - |  367 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|       - |  368 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|       - |  369 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|       - |  370 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|       - |  371 | ` */` |
|       - |  372 | ``/* php only DEPRECATES a lossy float->int operand (`5 % 2.7`, `3 \| 1.5`); PHL targets`` |
|       - |  373 | ` * php's non-deprecated surface and rejects it with a TypeError. An INTEGRAL float` |
|       - |  374 | `` * (`4.0 % 3`) loses nothing and is accepted. Returns SXRET_OK to continue, or the`` |
|       - |  375 | ` * throw status for the caller to route via PH7_DISPATCH_ENFORCE_RC. */` |
|    5834 |  376 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|       5 |  377 | `{` |
|       - |  378 | `	double r;` |
|    5839 |  379 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_REAL) == 0 ){` |
|    5831 |  380 | `		return SXRET_OK;` |
|       - |  381 | `	}` |
|       9 |  382 | `	r = (double)pVal->rVal;` |
|       9 |  383 | `	if( r == (double)(sxi64)r ){` |
|       9 |  384 | `		return SXRET_OK;` |
|       - |  385 | `	}` |
|     ! 0 |  386 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       - |  387 | `		"Implicit conversion from float to int loses precision");` |
|    2922 |  388 | `}` |
|       - |  389 | `/*` |
|       - |  390 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|       - |  391 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|       - |  392 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|       - |  393 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|       - |  394 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|       - |  395 | ` */` |
| 2208564 |  396 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|       5 |  397 | `{` |
|       - |  398 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|       - |  399 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
| 2208569 |  400 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|       5 |  401 | `}` |
|       - |  402 | `/*` |
|       - |  403 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|       - |  404 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|       - |  405 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|       - |  406 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|       - |  407 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|       - |  408 | ` * keep native re-entries off a finite C stack).` |
|       - |  409 | ` */` |
| 9373604 |  410 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|       5 |  411 | `{` |
| 9373609 |  412 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|       5 |  413 | `}` |
|       - |  414 | `/*` |
|       - |  415 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|       - |  416 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|       - |  417 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|       - |  418 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|       - |  419 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|       - |  420 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|       - |  421 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|       - |  422 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|       - |  423 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|       - |  424 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|       - |  425 | ` * (VmNativeNestingFatal).` |
|       - |  426 | ` *` |
|       - |  427 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|       - |  428 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|       - |  429 | ` * re-enter and loop.` |
|       - |  430 | ` */` |
|       2 |  431 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|       1 |  432 | `{` |
|       3 |  433 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  434 | `		return PH7_ABORT;` |
|       - |  435 | `	}` |
|       3 |  436 | `	pVm->iExitStatus = 255;` |
|       3 |  437 | `	pVm->bHaltRequested = 1;` |
|       3 |  438 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|       3 |  439 | `	return PH7_ABORT;` |
|       2 |  440 | `}` |
|       - |  441 | `/*` |
|       - |  442 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|       - |  443 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|       - |  444 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|       - |  445 | ` */` |
|       4 |  446 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|       2 |  447 | `{` |
|       6 |  448 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  449 | `		return PH7_ABORT;` |
|       - |  450 | `	}` |
|       6 |  451 | `	pVm->iExitStatus = 255;` |
|       6 |  452 | `	pVm->bHaltRequested = 1;` |
|       6 |  453 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|       6 |  454 | `	return PH7_ABORT;` |
|       4 |  455 | `}` |
|       - |  456 | `/*` |
|       - |  457 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |  458 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - |  459 | ` * information.` |
|       - |  460 | ` */` |
|   22466 |  461 | `static sxi32 VmThrowErrorAp(` |
|       - |  462 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  463 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  464 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|       - |  465 | `	const char *zFormat, /* Format message */` |
|       - |  466 | `	va_list ap           /* Variable list of arguments */` |
|       - |  467 | `	)` |
|       5 |  468 | `{` |
|       - |  469 | `	SyBlob sMsg;` |
|       - |  470 | `	SyString *pFile;` |
|   22471 |  471 | `	sxi32 rc = SXRET_OK;` |
|       - |  472 | `	/* Peek the processed file if available */` |
|   22471 |  473 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - |  474 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|   22471 |  475 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|   22471 |  476 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|   22471 |  477 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - |  478 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|       - |  479 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|       - |  480 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|       - |  481 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|   22471 |  482 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|       - |  483 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|       - |  484 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|   22105 |  485 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|   22105 |  486 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|   22027 |  487 | `			SyBlobRelease(&sMsg);` |
|   22027 |  488 | `			return SXRET_OK;` |
|       - |  489 | `		}` |
|     120 |  490 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      39 |  491 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|      39 |  492 | `	}` |
|     449 |  493 | `	SyBlobRelease(&sMsg);` |
|     449 |  494 | `	return rc;` |
|   11238 |  495 | `}` |
|       - |  496 | `/*` |
|       - |  497 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|       - |  498 | ` * scope), or NULL when executing outside any class context.` |
|       - |  499 | ` */` |
|    8738 |  500 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|       5 |  501 | `{` |
|    8743 |  502 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|     117 |  503 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|     117 |  504 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|       - |  505 | `	}` |
|    8631 |  506 | `	return 0;` |
|    4374 |  507 | `}` |
|       - |  508 | `/*` |
|       - |  509 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|       - |  510 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|       - |  511 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|       - |  512 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|       - |  513 | ` * as an undefined function and therefore as another engine throw). Each such` |
|       - |  514 | ` * throw would construct another exception and recurse until the native-nesting` |
|       - |  515 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|       - |  516 | ` * (an engine throw from inside a user exception's constructor) working and` |
|       - |  517 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|       - |  518 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|       - |  519 | ` * after the call.` |
|       - |  520 | ` */` |
|       - |  521 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|  341772 |  522 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|       5 |  523 | `{` |
|  341777 |  524 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|     ! 0 |  525 | `		return 0;` |
|       - |  526 | `	}` |
|  341777 |  527 | `	pVm->nExcCtorDepth++;` |
|  341777 |  528 | `	return 1;` |
|  170891 |  529 | `}` |
|       - |  530 | `/*` |
|       - |  531 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|       - |  532 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|       - |  533 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|       - |  534 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|       - |  535 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|       - |  536 | ` */` |
|  201096 |  537 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|       5 |  538 | `{` |
|       - |  539 | `	ph7_class *pErrClass;` |
|       - |  540 | `	ph7_class_instance *pThis;` |
|       - |  541 | `	ph7_class_method *pCons;` |
|       - |  542 | `	VmFrame *pFrame;` |
|       - |  543 | `	sxi32 rc;` |
|  201101 |  544 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|  201101 |  545 | `	if( pErrClass == 0 ){` |
|     ! 0 |  546 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  547 | `		return PH7_ABORT;` |
|       - |  548 | `	}` |
|  201101 |  549 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|  201101 |  550 | `	if( pThis == 0 ){` |
|     ! 0 |  551 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  552 | `		return PH7_ABORT;` |
|       - |  553 | `	}` |
|  201101 |  554 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|  201101 |  555 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|       - |  556 | `		ph7_value sArg;` |
|       - |  557 | `		ph7_value *apArg[1];` |
|       - |  558 | `		SyString sMsgStr;` |
|  201101 |  559 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|  201101 |  560 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|  201101 |  561 | `		apArg[0] = &sArg;` |
|  201101 |  562 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|  201101 |  563 | `		PH7_MemObjRelease(&sArg);` |
|  201101 |  564 | `		pVm->nExcCtorDepth--;` |
|  100548 |  565 | `	}` |
|  201101 |  566 | `	SyBlobRelease(pMsg);` |
|  201101 |  567 | `	pFrame = pVm->pFrame;` |
|  201101 |  568 | `	if( pFrame ){` |
|  201101 |  569 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  201101 |  570 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|  100548 |  571 | `	}` |
|  201101 |  572 | `	rc = VmThrowException(&(*pVm),pThis);` |
|  201101 |  573 | `	PH7_ClassInstanceUnref(pThis);` |
|  201101 |  574 | `	if( rc == SXERR_ABORT ){` |
|      32 |  575 | `		return PH7_ABORT;` |
|       - |  576 | `	}` |
|  201073 |  577 | `	return PH7_EXCEPTION;` |
|  100553 |  578 | `}` |
|       - |  579 | `/*` |
|       - |  580 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|       - |  581 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|       - |  582 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|       - |  583 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|       - |  584 | ` * result through its normal goto Exception / goto Abort.` |
|       - |  585 | ` */` |
|      22 |  586 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|       3 |  587 | `{` |
|       - |  588 | `	SyBlob sMsg;` |
|      25 |  589 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      25 |  590 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      25 |  591 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|       3 |  592 | `}` |
|       - |  593 | `/*` |
|       - |  594 | ` * Enum case singletons (PHP 8.1).` |
|       - |  595 | ` *` |
|       - |  596 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|       - |  597 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|       - |  598 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|       - |  599 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|       - |  600 | ` * backing-value type check and the duplicate-value check only fire when a` |
|       - |  601 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|       - |  602 | ` */` |
|       - |  603 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|       - |  604 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|       - |  605 | ` * readonly property" through the normal store path. */` |
|     120 |  606 | `static void VmEnumSetInstanceProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|       - |  607 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|       4 |  608 | `{` |
|     124 |  609 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|       - |  610 | `	VmClassAttr *pVmAttr;` |
|       - |  611 | `	ph7_value *pSlot;` |
|     124 |  612 | `	if( pEntry == 0 ){` |
|     ! 0 |  613 | `		return;` |
|       - |  614 | `	}` |
|     124 |  615 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     124 |  616 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     124 |  617 | `	if( pSlot == 0 ){` |
|     ! 0 |  618 | `		return;` |
|       - |  619 | `	}` |
|     124 |  620 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     124 |  621 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      64 |  622 | `}` |
|       - |  623 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|       - |  624 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|     138 |  625 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|       2 |  626 | `{` |
|     140 |  627 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|       - |  628 | `	ph7_class_instance *pObj;` |
|       - |  629 | `	SyHashEntry *pEntry;` |
|     140 |  630 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      49 |  631 | `		return 0;` |
|       - |  632 | `	}` |
|      92 |  633 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      92 |  634 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      92 |  635 | `	if( pEntry == 0 ){` |
|     ! 0 |  636 | `		return 0;` |
|       - |  637 | `	}` |
|      92 |  638 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      71 |  639 | `}` |
|       - |  640 | `/*` |
|       - |  641 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|       - |  642 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|       - |  643 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|       - |  644 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|       - |  645 | ` * raises it. Returns the throw status to park/route.` |
|       - |  646 | ` */` |
|       2 |  647 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|       1 |  648 | `{` |
|       - |  649 | `	SyBlob sMsg;` |
|       3 |  650 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|       3 |  651 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|       3 |  652 | `	pVm->pConstCycleAttr = 0;` |
|       3 |  653 | `	pVm->pConstCycleClass = 0;` |
|       3 |  654 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  655 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|       1 |  656 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|       3 |  657 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  658 | `}` |
|       - |  659 | `/*` |
|       - |  660 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|       - |  661 | ` * materialize lazily and individually on first access — the backing-value` |
|       - |  662 | ` * type check fires per case, and the duplicate-value check compares only` |
|       - |  663 | ` * against cases that have already materialized (a broken sibling case does` |
|       - |  664 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|       - |  665 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|       - |  666 | ` * (duplicate value / self-reference) — which the caller routes` |
|       - |  667 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|       - |  668 | ` */` |
|     136 |  669 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|       5 |  670 | `{` |
|       - |  671 | `	ph7_class_attr **apCase;` |
|       - |  672 | `	ph7_class_instance *pObj;` |
|       - |  673 | `	ph7_value *pSlot;` |
|       - |  674 | `	ph7_value sBacking,sPropVal;` |
|       - |  675 | `	sxu32 i;` |
|     141 |  676 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      63 |  677 | `		return SXRET_OK;` |
|       - |  678 | `	}` |
|      79 |  679 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  680 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|       - |  681 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|     ! 0 |  682 | `		if( pVm->pConstCycleAttr == 0 ){` |
|     ! 0 |  683 | `			pVm->pConstCycleAttr = pCase;` |
|     ! 0 |  684 | `			pVm->pConstCycleClass = pClass;` |
|     ! 0 |  685 | `		}` |
|     ! 0 |  686 | `		return SXRET_OK;` |
|       - |  687 | `	}` |
|      79 |  688 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      79 |  689 | `	if( pClass->nEnumBacking != 0 ){` |
|      63 |  690 | `		if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|       - |  691 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|      63 |  692 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       - |  693 | `			sxi32 rcExec;` |
|      63 |  694 | `			pVm->pConstEvalClass = pClass;` |
|      63 |  695 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      63 |  696 | `			pVm->nConstEvalDepth++;` |
|      63 |  697 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      63 |  698 | `			pVm->nConstEvalDepth--;` |
|      63 |  699 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      63 |  700 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      63 |  701 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  702 | `				/* The backing expression raised: abandon materialization and` |
|       - |  703 | `				 * hand the status to the caller to park/route. */` |
|       3 |  704 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  705 | `				return rcExec;` |
|       - |  706 | `			}` |
|      60 |  707 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|     ! 0 |  708 | `				PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  709 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  710 | `			}` |
|      28 |  711 | `		}` |
|      60 |  712 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|       - |  713 | `			/* php: TypeError, checked lazily at first case access */` |
|       - |  714 | `			SyBlob sMsg;` |
|       3 |  715 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|       3 |  716 | `			PH7_MemObjRelease(&sBacking);` |
|       3 |  717 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       2 |  718 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|       2 |  719 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|       3 |  720 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - |  721 | `		}` |
|      58 |  722 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       - |  723 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|       - |  724 | `			 * the typed-constant leniency) to a genuine int. */` |
|      11 |  725 | `			PH7_MemObjToInteger(&sBacking);` |
|       6 |  726 | `		}else{` |
|      48 |  727 | `			PH7_MemObjToString(&sBacking);` |
|       - |  728 | `		}` |
|       - |  729 | `		/* php: two cases sharing one backing value are an Error — compared` |
|       - |  730 | `		 * against already-materialized cases only (php registers values as` |
|       - |  731 | `		 * each case evaluates). */` |
|      58 |  732 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     202 |  733 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|       - |  734 | `			ph7_value *pPrev;` |
|     150 |  735 | `			int bDup = 0;` |
|     150 |  736 | `			if( apCase[i] == pCase ){` |
|      56 |  737 | `				continue;` |
|       - |  738 | `			}` |
|      95 |  739 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      95 |  740 | `			if( pPrev ){` |
|      47 |  741 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       5 |  742 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|       3 |  743 | `				}else{` |
|      51 |  744 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|      42 |  745 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|      16 |  746 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|       - |  747 | `				}` |
|      23 |  748 | `			}` |
|      95 |  749 | `			if( bDup ){` |
|       - |  750 | `				/* php prints the two cases in DECLARATION order regardless of` |
|       - |  751 | `				 * which one is being evaluated. */` |
|       3 |  752 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|       - |  753 | `				SyBlob sMsg;` |
|       - |  754 | `				sxu32 j;` |
|       5 |  755 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|       5 |  756 | `					if( apCase[j] == pCase ){ break; }` |
|       2 |  757 | `				}` |
|       3 |  758 | `				if( j < i ){` |
|     ! 0 |  759 | `					pFirst = pCase;` |
|     ! 0 |  760 | `					pSecond = apCase[i];` |
|     ! 0 |  761 | `				}` |
|       3 |  762 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  763 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  764 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|       1 |  765 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|       3 |  766 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - |  767 | `			}` |
|      47 |  768 | `		}` |
|      26 |  769 | `	}` |
|       - |  770 | `	/* Create the singleton and fill its readonly props */` |
|      72 |  771 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      72 |  772 | `	if( pObj == 0 ){` |
|     ! 0 |  773 | `		PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  774 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  775 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|     ! 0 |  776 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  777 | `		return PH7_ABORT;` |
|       - |  778 | `	}` |
|      72 |  779 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      72 |  780 | `	VmEnumSetInstanceProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      72 |  781 | `	PH7_MemObjRelease(&sPropVal);` |
|      72 |  782 | `	if( pClass->nEnumBacking != 0 ){` |
|      56 |  783 | `		VmEnumSetInstanceProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|      26 |  784 | `	}` |
|      72 |  785 | `	PH7_MemObjRelease(&sBacking);` |
|       - |  786 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|       - |  787 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      72 |  788 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      72 |  789 | `	if( pSlot == 0 ){` |
|     ! 0 |  790 | `		PH7_ClassInstanceUnref(pObj);` |
|     ! 0 |  791 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  792 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|     ! 0 |  793 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  794 | `		return PH7_ABORT;` |
|       - |  795 | `	}` |
|      72 |  796 | `	pSlot->x.pOther = pObj;` |
|      72 |  797 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      72 |  798 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      72 |  799 | `	pCase->nIdx = pSlot->nIdx;` |
|      72 |  800 | `	return SXRET_OK;` |
|      73 |  801 | `}` |
|       - |  802 | `/*` |
|       - |  803 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|       - |  804 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|       - |  805 | ` * there, so a broken case surfaces its error at the same point).` |
|       - |  806 | ` */` |
|      68 |  807 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  808 | `{` |
|       - |  809 | `	ph7_class_attr **apCase;` |
|       - |  810 | `	sxu32 n;` |
|      73 |  811 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 |  812 | `		return SXRET_OK;` |
|       - |  813 | `	}` |
|      73 |  814 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     197 |  815 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     135 |  816 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|     135 |  817 | `		if( rc != SXRET_OK ){` |
|       8 |  818 | `			return rc;` |
|       - |  819 | `		}` |
|      66 |  820 | `	}` |
|      66 |  821 | `	return SXRET_OK;` |
|      39 |  822 | `}` |
|       - |  823 | `/*` |
|       - |  824 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|       - |  825 | ` * or 0 when the name does not name an enum.` |
|       - |  826 | ` */` |
|      40 |  827 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|       2 |  828 | `{` |
|       - |  829 | `	ph7_class *pClass;` |
|      42 |  830 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|     ! 0 |  831 | `		return 0;` |
|       - |  832 | `	}` |
|      62 |  833 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      20 |  834 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      44 |  835 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|       3 |  836 | `		pClass = pClass->pNextName;` |
|       1 |  837 | `	}` |
|      42 |  838 | `	return pClass;` |
|      22 |  839 | `}` |
|       - |  840 | `/*` |
|       - |  841 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|       - |  842 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|       - |  843 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|       - |  844 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|       - |  845 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|       - |  846 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|       - |  847 | ` */` |
|     486 |  848 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|       5 |  849 | `{` |
|     491 |  850 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     486 |  851 | `	if( pInner && pInner->pUserData` |
|     317 |  852 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      83 |  853 | `		return 0;` |
|       - |  854 | `	}` |
|     411 |  855 | `	return pVm->nCurLine;` |
|     248 |  856 | `}` |
|       - |  857 | `/*` |
|       - |  858 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|       - |  859 | ` * not the pointer: aMemObj is a by-value SySet, so any nested evaluation (an` |
|       - |  860 | ` * initializer, or the constructor of the very TypeError being raised) can grow` |
|       - |  861 | ` * and REALLOC the pool, leaving a pointer taken before it dangling — the rule` |
|       - |  862 | ` * VmLocalExecIntoObj is built around. The slot's contents are released first:` |
|       - |  863 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|       - |  864 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|       - |  865 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|       - |  866 | ` */` |
|      36 |  867 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|       3 |  868 | `{` |
|      39 |  869 | `	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       - |  870 | `	VmSlot sSlot;` |
|      39 |  871 | `	if( pObj == 0 ){` |
|     ! 0 |  872 | `		return;` |
|       - |  873 | `	}` |
|      39 |  874 | `	PH7_MemObjRelease(pObj);` |
|      39 |  875 | `	sSlot.nIdx = nIdx;` |
|      39 |  876 | `	sSlot.pUserData = 0;` |
|      39 |  877 | `	SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      21 |  878 | `}` |
|       - |  879 | `/*` |
|       - |  880 | ` * Evaluate a class constant's initializer on demand.` |
|       - |  881 | ` *` |
|       - |  882 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|       - |  883 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|       - |  884 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|       - |  885 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|       - |  886 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|       - |  887 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|       - |  888 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|       - |  889 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|       - |  890 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|       - |  891 | ` * "Cannot declare self-referencing constant" Error.` |
|       - |  892 | ` */` |
|     428 |  893 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  894 | `{` |
|       - |  895 | `	ph7_value *pMemObj;` |
|     428 |  896 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     428 |  897 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     433 |  898 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|     ! 0 |  899 | `		return SXRET_OK;` |
|       - |  900 | `	}` |
|     433 |  901 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  902 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|       - |  903 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|       - |  904 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|       3 |  905 | `		if( pVm->pConstCycleAttr == 0 ){` |
|       3 |  906 | `			pVm->pConstCycleAttr = pAttr;` |
|       3 |  907 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 |  908 | `		}` |
|       3 |  909 | `		return SXRET_OK;` |
|       - |  910 | `	}` |
|     431 |  911 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     431 |  912 | `	if( pMemObj == 0 ){` |
|     ! 0 |  913 | `		return SXERR_MEM;` |
|       - |  914 | `	}` |
|     431 |  915 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     431 |  916 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     431 |  917 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|     431 |  918 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|       - |  919 | `		sxu32 nSaveLazyLine;` |
|       - |  920 | `		sxi32 nSaveLazyDepth;` |
|       - |  921 | `		sxu32 nSlot;` |
|       - |  922 | `		sxi32 rcExec;` |
|     431 |  923 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|     431 |  924 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - |  925 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|       - |  926 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|       - |  927 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|     431 |  928 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - |  929 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|       - |  930 | `		 * throw out of its own bytecode carries. */` |
|     431 |  931 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|     431 |  932 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|     431 |  933 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|     431 |  934 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|     431 |  935 | `		pVm->nConstEvalDepth++;` |
|     431 |  936 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     431 |  937 | `		pVm->nConstEvalDepth--;` |
|     431 |  938 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|     431 |  939 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|     431 |  940 | `		pVm->pConstEvalClass = pSaveCtx;` |
|     431 |  941 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|     431 |  942 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|     431 |  943 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|     426 |  944 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|     402 |  945 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|       - |  946 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|       - |  947 | `			 * class constant's expression at each access until one of them` |
|       - |  948 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|       - |  949 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|       - |  950 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|       - |  951 | `			 * the rest of the run — and made a static default that named it` |
|       - |  952 | `			 * (whose own evaluation is deferred to first access) find it` |
|       - |  953 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|       - |  954 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|       - |  955 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|       - |  956 | `			 * it is found — an inner level only records it — but the value is` |
|       - |  957 | `			 * unusable and the next access must be able to detect it again.` |
|       - |  958 | `			 * No loop: each access runs the initializer once and raises. */` |
|      35 |  959 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|      35 |  960 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  961 | `				/* Hand the status to the caller to park/route. */` |
|      32 |  962 | `				return rcExec;` |
|       - |  963 | `			}` |
|       3 |  964 | `			if( pVm->nConstEvalDepth == 0 ){` |
|       - |  965 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|       - |  966 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|       3 |  967 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  968 | `			}` |
|     ! 0 |  969 | `			return SXRET_OK;` |
|       - |  970 | `		}` |
|     399 |  971 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       - |  972 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|       - |  973 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|       - |  974 | `			 * access raises again — php re-runs the whole materialization each` |
|       - |  975 | `			 * time. A pass may widen int -> float in place, which is the value` |
|       - |  976 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|       - |  977 | `			 * runs php code that may grow (and realloc) aMemObj — so the slot is` |
|       - |  978 | `			 * addressed by index from here on, never through pMemObj. */` |
|       5 |  979 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|       5 |  980 | `			if( rcType != SXRET_OK ){` |
|       5 |  981 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|       5 |  982 | `				return rcType;` |
|       - |  983 | `			}` |
|     ! 0 |  984 | `		}` |
|       - |  985 | `		/* Memoize the value. */` |
|     395 |  986 | `		pAttr->nIdx = nSlot;` |
|     395 |  987 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|     395 |  988 | `		return SXRET_OK;` |
|       - |  989 | `	}` |
|     ! 0 |  990 | `	pAttr->nIdx = pMemObj->nIdx;` |
|     ! 0 |  991 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     ! 0 |  992 | `	return SXRET_OK;` |
|     219 |  993 | `}` |
|       - |  994 | `/*` |
|       - |  995 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|       - |  996 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|       - |  997 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|       - |  998 | ` */` |
|      66 |  999 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1000 | `{` |
|      70 | 1001 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      45 | 1002 | `		return SXRET_OK;` |
|       - | 1003 | `	}` |
|      26 | 1004 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       5 | 1005 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       - | 1006 | `	}` |
|      22 | 1007 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      37 | 1008 | `}` |
|       - | 1009 | `/*` |
|       - | 1010 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|       - | 1011 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|       - | 1012 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|       - | 1013 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|       - | 1014 | ` */` |
|       6 | 1015 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|       1 | 1016 | `{` |
|       - | 1017 | `	SyBlob sMsg;` |
|       7 | 1018 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 | 1019 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       7 | 1020 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1021 | `}` |
|       - | 1022 | `/*` |
|       - | 1023 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|       - | 1024 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|       - | 1025 | ` * raised at the store site here with the same message and the same` |
|       - | 1026 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|       - | 1027 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|       - | 1028 | ` */` |
|       2 | 1029 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|       1 | 1030 | `{` |
|       3 | 1031 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|       3 | 1032 | `	pVm->iExitStatus = 255;` |
|       3 | 1033 | `	pVm->bHaltRequested = 1;` |
|       3 | 1034 | `	return PH7_ABORT;` |
|       1 | 1035 | `}` |
|       - | 1036 | `/*` |
|       - | 1037 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|       - | 1038 | ` * property assignment. Called from the STORE path when coercion is not` |
|       - | 1039 | ` * possible.` |
|       - | 1040 | ` */` |
|  100104 | 1041 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|       5 | 1042 | `{` |
|  100109 | 1043 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|  100109 | 1044 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 1045 | `	char zType[192];` |
|  150161 | 1046 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|   50052 | 1047 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner),zType,sizeof(zType));` |
|       - | 1048 | `	SyBlob sMsg;` |
|  100109 | 1049 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 1050 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|       - | 1051 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|  100109 | 1052 | `	if( pOwner ){` |
|  100109 | 1053 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|   50052 | 1054 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|   50057 | 1055 | `	}else{` |
|     ! 0 | 1056 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %s",` |
|     ! 0 | 1057 | `			zGiven,&pAttr->sName,zTypeText);` |
|       - | 1058 | `	}` |
|  100109 | 1059 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       5 | 1060 | `}` |
|       - | 1061 | `/*` |
|       - | 1062 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|       - | 1063 | ` */` |
|  100006 | 1064 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1065 | `{` |
|  100010 | 1066 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|  100010 | 1067 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|       - | 1068 | `	SyBlob sMsg;` |
|  100010 | 1069 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|  100010 | 1070 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|   50003 | 1071 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|  100010 | 1072 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       4 | 1073 | `}` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|       - | 1076 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|       - | 1077 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|       - | 1078 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|       - | 1079 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|       - | 1080 | ` */` |
|       - | 1081 | `/*` |
|       - | 1082 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|       - | 1083 | ` * property from a scope its set-visibility excludes:` |
|       - | 1084 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|       - | 1085 | ` */` |
|      14 | 1086 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1087 | `{` |
|      15 | 1088 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      15 | 1089 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|      15 | 1090 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|       - | 1091 | `	SyBlob sMsg;` |
|      15 | 1092 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 1093 | `	if( pActive ){` |
|       3 | 1094 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|       1 | 1095 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|       2 | 1096 | `	}else{` |
|      13 | 1097 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|       6 | 1098 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|       - | 1099 | `	}` |
|      15 | 1100 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 | 1101 | `}` |
|       - | 1102 | `/*` |
|       - | 1103 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|       - | 1104 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|       - | 1105 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|       - | 1106 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|       - | 1107 | ` */` |
|      32 | 1108 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|       1 | 1109 | `{` |
|      33 | 1110 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|      33 | 1111 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1112 | `	int bOk;` |
|      33 | 1113 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      27 | 1114 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|      14 | 1115 | `	}else{` |
|       7 | 1116 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|       - | 1117 | `	}` |
|      33 | 1118 | `	if( !bOk ){` |
|      15 | 1119 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|       - | 1120 | `	}` |
|      19 | 1121 | `	return SXRET_OK;` |
|      17 | 1122 | `}` |
|      32 | 1123 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|       5 | 1124 | `{` |
|      37 | 1125 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1126 | `	SyBlob sMsg;` |
|      37 | 1127 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      37 | 1128 | `	if( bModify ){` |
|      33 | 1129 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|      19 | 1130 | `	}else{` |
|       6 | 1131 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       6 | 1132 | `		if( pActive ){` |
|     ! 0 | 1133 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|     ! 0 | 1134 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|     ! 0 | 1135 | `		}else{` |
|       6 | 1136 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|       2 | 1137 | `				&pOwner->sName,&pAttr->sName);` |
|       - | 1138 | `		}` |
|       - | 1139 | `	}` |
|      37 | 1140 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       5 | 1141 | `}` |
|       - | 1142 | `/*` |
|       - | 1143 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|       - | 1144 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|       - | 1145 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|       - | 1146 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|       - | 1147 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|       - | 1148 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|       - | 1149 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|       - | 1150 | ` */` |
|  637788 | 1151 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|       5 | 1152 | `{` |
|       - | 1153 | `	SyHashEntry *pSlot;` |
|       - | 1154 | `	VmClassAttr *pVmAttr;` |
|  637793 | 1155 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|  401637 | 1156 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|       - | 1157 | `	}` |
|  236161 | 1158 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  236161 | 1159 | `	if( pSlot == 0 ){` |
|  236077 | 1160 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1161 | `	}` |
|      87 | 1162 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      87 | 1163 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|      12 | 1164 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|       - | 1165 | `	}` |
|      74 | 1166 | `	if( pVmAttr->pAttr` |
|      76 | 1167 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|       - | 1168 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|       7 | 1169 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|       - | 1170 | `	}` |
|      70 | 1171 | `	return SXRET_OK;` |
|  319105 | 1172 | `}` |
|       - | 1173 | `/*` |
|       - | 1174 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|       - | 1175 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|       - | 1176 | ` * For class types, instanceof is verified.` |
|       - | 1177 | ` *` |
|       - | 1178 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|       - | 1179 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|       - | 1180 | ` */` |
|       - | 1181 |  |
|       - | 1182 | `/*` |
|       - | 1183 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|       - | 1184 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|       - | 1185 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|       - | 1186 | ` *   0 if it's not strictly numeric.` |
|       - | 1187 | ` */` |
|      38 | 1188 | `static int VmStringNumericKind(ph7_value *pValue)` |
|       3 | 1189 | `{` |
|       - | 1190 | `	const char *z, *zEnd, *zTail;` |
|       - | 1191 | `	sxu32 n;` |
|      41 | 1192 | `	sxu8 bReal = 0;` |
|       - | 1193 | `	sxi32 rc;` |
|      41 | 1194 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      24 | 1195 | `		return 0;` |
|       - | 1196 | `	}` |
|      18 | 1197 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|      18 | 1198 | `	n = SyBlobLength(&pValue->sBlob);` |
|      18 | 1199 | `	zEnd = z + n;` |
|      18 | 1200 | `	if( n == 0 ) return 0;` |
|      18 | 1201 | `	zTail = 0;` |
|      18 | 1202 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|      18 | 1203 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|      19 | 1204 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|      15 | 1205 | `	if( zTail != zEnd ) return 0;` |
|      15 | 1206 | `	return bReal ? 2 : 1;` |
|      22 | 1207 | `}` |
|       - | 1208 |  |
|       - | 1209 | `/*` |
|       - | 1210 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|       - | 1211 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|       - | 1212 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|       - | 1213 | ` * return, parameter, property, union alternative — would have to string-match` |
|       - | 1214 | ` * the name itself.` |
|       - | 1215 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|       - | 1216 | ` * to extend when another literal/pseudo type is added.` |
|       - | 1217 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|       - | 1218 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|       - | 1219 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|       - | 1220 | ` */` |
|    2174 | 1221 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|       5 | 1222 | `{` |
|    2179 | 1223 | `	const char *z = pClass->zString;` |
|    2179 | 1224 | `	sxu32 n = pClass->nByte;` |
|    2179 | 1225 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      90 | 1226 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|       - | 1227 | `	}` |
|    2093 | 1228 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|      28 | 1229 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|       - | 1230 | `	}` |
|    2067 | 1231 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|      51 | 1232 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|       - | 1233 | `	}` |
|    2019 | 1234 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|       - | 1235 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|       - | 1236 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|       - | 1237 | `		 * object; scope-sensitive, so a private method is callable only from` |
|       - | 1238 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|       - | 1239 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|       - | 1240 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|       - | 1241 | `		 * php does.) */` |
|    1359 | 1242 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|       - | 1243 | `	}` |
|     665 | 1244 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|       - | 1245 | `		/* iterable === array \| Traversable */` |
|      49 | 1246 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      14 | 1247 | `			return 1;` |
|       - | 1248 | `		}` |
|      37 | 1249 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|      15 | 1250 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      15 | 1251 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       5 | 1252 | `				return 1;` |
|       - | 1253 | `			}` |
|       4 | 1254 | `		}` |
|      33 | 1255 | `		return 0;` |
|       - | 1256 | `	}` |
|     619 | 1257 | `	return -1;` |
|    1092 | 1258 | `}` |
|       - | 1259 | `/*` |
|       - | 1260 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|       - | 1261 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|       - | 1262 | ` * through. php binds the keyword where the hint is written, so` |
|       - | 1263 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|       - | 1264 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|       - | 1265 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|       - | 1266 | ` * throw a TypeError over perfectly valid code.` |
|       - | 1267 | ` *` |
|       - | 1268 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|       - | 1269 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|       - | 1270 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|       - | 1271 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|       - | 1272 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|       - | 1273 | `` * VmResolveTypeClass applies to `parent`.`` |
|       - | 1274 | ` */` |
|  210486 | 1275 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|       5 | 1276 | `{` |
|  210491 | 1277 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|  201861 | 1278 | `		return pDecl;` |
|       - | 1279 | `	}` |
|    8635 | 1280 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|  105248 | 1281 | `}` |
|       - | 1282 | `/*` |
|       - | 1283 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|       - | 1284 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|       - | 1285 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|       - | 1286 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|       - | 1287 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|       - | 1288 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|       - | 1289 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|       - | 1290 | ` * throw.` |
|       - | 1291 | ` *` |
|       - | 1292 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|       - | 1293 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|       - | 1294 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|       - | 1295 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|       - | 1296 | ` */` |
|       - | 1297 | `/*` |
|       - | 1298 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|       - | 1299 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|       - | 1300 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|       - | 1301 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|       - | 1302 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|       - | 1303 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|       - | 1304 | ` * iLoadable flag — lives in one place.` |
|       - | 1305 | ` *` |
|       - | 1306 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|       - | 1307 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|       - | 1308 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|       - | 1309 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|       - | 1310 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|       - | 1311 | ` *` |
|       - | 1312 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|       - | 1313 | ` * compatibility target, where the type may legitimately be an interface or` |
|       - | 1314 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|       - | 1315 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|       - | 1316 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|       - | 1317 | ` * with TRUE; it does not go through this helper.)` |
|       - | 1318 | ` */` |
|     686 | 1319 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|       5 | 1320 | `{` |
|     691 | 1321 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      98 | 1322 | `		return pSelf;` |
|       - | 1323 | `	}` |
|     597 | 1324 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|       - | 1325 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|       - | 1326 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|       - | 1327 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|       - | 1328 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|       - | 1329 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|      36 | 1330 | `		return PH7_VmPeekTopClass(pVm);` |
|       - | 1331 | `	}` |
|     565 | 1332 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|       - | 1333 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|       - | 1334 | `		 * resolves against the runtime using class, matching the self:: trait rule. */` |
|      21 | 1335 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 | 1336 | `			pSelf = PH7_VmPeekTopClass(pVm);` |
|     ! 0 | 1337 | `		}` |
|      21 | 1338 | `		return pSelf ? pSelf->pBase : 0;` |
|       - | 1339 | `	}` |
|     547 | 1340 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|     348 | 1341 | `}` |
|       - | 1342 | `/*` |
|       - | 1343 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|       - | 1344 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|       - | 1345 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|       - | 1346 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|       - | 1347 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|       - | 1348 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|       - | 1349 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|       - | 1350 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|       - | 1351 | ` */` |
|  100448 | 1352 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|       5 | 1353 | `{` |
|  100461 | 1354 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|  100438 | 1355 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|  150672 | 1356 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|       5 | 1357 | `}` |
|       - | 1358 | `/*` |
|       - | 1359 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|       - | 1360 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|       - | 1361 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|       - | 1362 | ` * typed-default checks — each of which then formats its own message. The` |
|       - | 1363 | ` * resolved class is handed back through *ppResolved for the message builder` |
|       - | 1364 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|       - | 1365 | ` * nothing resolved).` |
|       - | 1366 | ` *` |
|       - | 1367 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|       - | 1368 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|       - | 1369 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|       - | 1370 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|       - | 1371 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|       - | 1372 | ` * resolve against there is no class to compare to — a position php rejects at` |
|       - | 1373 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|       - | 1374 | ` *` |
|       - | 1375 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|       - | 1376 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|       - | 1377 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|       - | 1378 | ` * autoloader can produce fails.` |
|       - | 1379 | ` */` |
|     498 | 1380 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|       - | 1381 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|       5 | 1382 | `{` |
|     503 | 1383 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     503 | 1384 | `	*ppResolved = pExpected;` |
|     503 | 1385 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      42 | 1386 | `		return 0;` |
|       - | 1387 | `	}` |
|     465 | 1388 | `	if( pExpected == 0 ){` |
|      13 | 1389 | `		return VmHintIsScopeKeyword(pName);` |
|       - | 1390 | `	}` |
|     453 | 1391 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|     254 | 1392 | `}` |
|       - | 1393 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|       - | 1394 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|  402732 | 1395 | `static int VmHintNameChar(int c)` |
|       5 | 1396 | `{` |
|  805123 | 1397 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|  402392 | 1398 | `		\|\| c == ' ' \|\| c == '\t');` |
|       5 | 1399 | `}` |
|       - | 1400 | `/*` |
|       - | 1401 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|       - | 1402 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|       - | 1403 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|       - | 1404 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|       - | 1405 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|       - | 1406 | ` * resolved until the call site are substituted.` |
|       - | 1407 | ` *` |
|       - | 1408 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|       - | 1409 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|       - | 1410 | ` * are only ever available as source: unions/intersections, and the property /` |
|       - | 1411 | ` * class-constant messages, which print the declared type whatever its shape.` |
|       - | 1412 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|       - | 1413 | ` */` |
|  100272 | 1414 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|       - | 1415 | `	char *zBuf,sxu32 nBuf)` |
|       5 | 1416 | `{` |
|       - | 1417 | `	const char *z;` |
|  100277 | 1418 | `	sxu32 n, i = 0, nAt = 0;` |
|  100277 | 1419 | `	if( nBuf == 0 ){` |
|     ! 0 | 1420 | `		return "";` |
|       - | 1421 | `	}` |
|  100277 | 1422 | `	z = pDeclared ? pDeclared->zString : 0;` |
|  100277 | 1423 | `	n = z ? pDeclared->nByte : 0;` |
|  200895 | 1424 | `	while( i < n && nAt + 1 < nBuf ){` |
|       - | 1425 | `		sxu32 nStart, nCopy;` |
|       - | 1426 | `		SyString sTok;` |
|       - | 1427 | `		const SyString *pOut;` |
|  100623 | 1428 | `		if( !VmHintNameChar(z[i]) ){` |
|     186 | 1429 | `			zBuf[nAt++] = z[i++];` |
|     186 | 1430 | `			continue;` |
|       - | 1431 | `		}` |
|  100441 | 1432 | `		nStart = i;` |
|  402391 | 1433 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|  301955 | 1434 | `			i++;` |
|       5 | 1435 | `		}` |
|  100441 | 1436 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|  100441 | 1437 | `		pOut = &sTok;` |
|  100441 | 1438 | `		if( VmHintIsScopeKeyword(&sTok) ){` |
|      31 | 1439 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|      31 | 1440 | `			if( pRes ){` |
|      31 | 1441 | `				pOut = &pRes->sName;` |
|      14 | 1442 | `			}` |
|      14 | 1443 | `		}` |
|  100441 | 1444 | `		nCopy = pOut->nByte;` |
|  100441 | 1445 | `		if( nCopy > nBuf - nAt - 1 ){` |
|     ! 0 | 1446 | `			nCopy = nBuf - nAt - 1;` |
|     ! 0 | 1447 | `		}` |
|  100441 | 1448 | `		if( nCopy > 0 ){` |
|  100441 | 1449 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|  100441 | 1450 | `			nAt += nCopy;` |
|   50218 | 1451 | `		}` |
|       5 | 1452 | `	}` |
|  100277 | 1453 | `	zBuf[nAt] = 0;` |
|  100277 | 1454 | `	return zBuf;` |
|   50141 | 1455 | `}` |
|       - | 1456 | `/*` |
|       - | 1457 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|       - | 1458 | ` * float-identity leniency — see the typed-constant note above` |
|       - | 1459 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|       - | 1460 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|       - | 1461 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|       - | 1462 | ` * so an int-typed parameter / return / property / union member silently` |
|       - | 1463 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|       - | 1464 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|       - | 1465 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|       - | 1466 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|       - | 1467 | ` * present).` |
|       - | 1468 | ` *` |
|       - | 1469 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|       - | 1470 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|       - | 1471 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|       - | 1472 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|       - | 1473 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|       - | 1474 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|       - | 1475 | ` */` |
| 2936702 | 1476 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|       5 | 1477 | `{` |
| 2936702 | 1478 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|  745608 | 1479 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       - | 1480 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|       - | 1481 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|       - | 1482 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|       - | 1483 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|      38 | 1484 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|      38 | 1485 | `		SyBlobRelease(&pVal->sBlob);` |
|      38 | 1486 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|      17 | 1487 | `	}` |
| 2936707 | 1488 | `}` |
|     324 | 1489 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|       - | 1490 | `	ph7_class *pSelf)` |
|       5 | 1491 | `{` |
|       - | 1492 | `	sxu32 i;` |
|       - | 1493 | `	sxu32 nAlts;` |
|       - | 1494 | `	ph7_type_alt *aAlts;` |
|       - | 1495 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|       - | 1496 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     329 | 1497 | `	int bHasIntersection = 0;` |
|       - | 1498 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     329 | 1499 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      20 | 1500 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|       - | 1501 | `	}` |
|     313 | 1502 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     313 | 1503 | `	nAlts = SySetUsed(pAlts);` |
|       - | 1504 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|       - | 1505 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|       - | 1506 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|       - | 1507 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|       - | 1508 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   10169 | 1509 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     965 | 1510 | `	for( i = 0; i < nAlts; i++ ){` |
|     657 | 1511 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|      41 | 1512 | `			bHasIntersection = 1;` |
|      19 | 1513 | `		}` |
|     331 | 1514 | `	}` |
|       - | 1515 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|       - | 1516 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|       - | 1517 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|       - | 1518 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     313 | 1519 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|      35 | 1520 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1521 | `		sxu32 g;` |
|     421 | 1522 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|       - | 1523 | `			int bAll;` |
|     409 | 1524 | `			if( aGroupCount[g] < 2 ) continue;` |
|      35 | 1525 | `			bAll = 1;` |
|      87 | 1526 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1527 | `				ph7_class *pExpected;` |
|      67 | 1528 | `				if( aAlts[i].nGroup != g ) continue;` |
|      63 | 1529 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|      63 | 1530 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      63 | 1531 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      15 | 1532 | `					bAll = 0;` |
|      15 | 1533 | `					break;` |
|       - | 1534 | `				}` |
|      27 | 1535 | `			}` |
|      35 | 1536 | `			if( bAll ) return SXRET_OK;` |
|       9 | 1537 | `		}` |
|       6 | 1538 | `	}` |
|       - | 1539 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|       - | 1540 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|       - | 1541 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|       - | 1542 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     885 | 1543 | `	for( i = 0; i < nAlts; i++ ){` |
|     611 | 1544 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     570 | 1545 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     394 | 1546 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|      16 | 1547 | `			return SXRET_OK;` |
|       - | 1548 | `		}` |
|     283 | 1549 | `	}` |
|     279 | 1550 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     279 | 1551 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     859 | 1552 | `	for( i = 0; i < nAlts; i++ ){` |
|     585 | 1553 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     549 | 1554 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     367 | 1555 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     363 | 1556 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     357 | 1557 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     157 | 1558 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     117 | 1559 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|       5 | 1560 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     277 | 1561 | `	}` |
|       - | 1562 | `	/* Object handling */` |
|     279 | 1563 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      96 | 1564 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      96 | 1565 | `		if( bHasClassAlt ){` |
|      82 | 1566 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     210 | 1567 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1568 | `				ph7_class *pExpected;` |
|     156 | 1569 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     148 | 1570 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|     104 | 1571 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|     104 | 1572 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      28 | 1573 | `					return SXRET_OK;` |
|       - | 1574 | `				}` |
|      42 | 1575 | `			}` |
|      27 | 1576 | `		}` |
|      71 | 1577 | `		return SXERR_INVALID;` |
|       - | 1578 | `	}` |
|       - | 1579 | `	/* Array handling */` |
|     187 | 1580 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      16 | 1581 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|       - | 1582 | `	}` |
|       - | 1583 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|       - | 1584 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|       - | 1585 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|       - | 1586 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|       - | 1587 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|       - | 1588 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     175 | 1589 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      18 | 1590 | `		if( bHasFloat ) return SXRET_OK;` |
|       3 | 1591 | `	}` |
|     167 | 1592 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|     113 | 1593 | `		if( bHasInt ){` |
|      91 | 1594 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      91 | 1595 | `			return SXRET_OK;` |
|       - | 1596 | `		}` |
|      11 | 1597 | `	}` |
|      81 | 1598 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|      58 | 1599 | `		if( bHasString ) return SXRET_OK;` |
|       8 | 1600 | `	}` |
|      41 | 1601 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1602 | `		if( bHasBool ) return SXRET_OK;` |
|     ! 0 | 1603 | `	}` |
|      41 | 1604 | `	if( bStrict ){` |
|       - | 1605 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|     ! 0 | 1606 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|     ! 0 | 1607 | `			PH7_MemObjToReal(pValue);` |
|     ! 0 | 1608 | `			return SXRET_OK;` |
|       - | 1609 | `		}` |
|     ! 0 | 1610 | `		return SXERR_INVALID;` |
|       - | 1611 | `	}` |
|       - | 1612 | `	/* Weak coercion preference order: int > float > string > bool.` |
|       - | 1613 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|       - | 1614 | `	 * to match PHP's union RFC. */` |
|       - | 1615 | `	{` |
|      41 | 1616 | `		int kind = VmStringNumericKind(pValue);` |
|      41 | 1617 | `		if( bHasInt ){` |
|       - | 1618 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|       - | 1619 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|      18 | 1620 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1621 | `				PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1622 | `				return SXRET_OK;` |
|       - | 1623 | `			}` |
|      18 | 1624 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1625 | `				ph7_real r = pValue->rVal;` |
|     ! 0 | 1626 | `				if( r == (ph7_real)(sxi64)r ){` |
|     ! 0 | 1627 | `					PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1628 | `					return SXRET_OK;` |
|       - | 1629 | `				}` |
|     ! 0 | 1630 | `			}` |
|      18 | 1631 | `			if( kind == 1 ){` |
|       9 | 1632 | `				PH7_MemObjToInteger(pValue);` |
|       9 | 1633 | `				return SXRET_OK;` |
|       - | 1634 | `			}` |
|       4 | 1635 | `		}` |
|      33 | 1636 | `		if( bHasFloat ){` |
|      10 | 1637 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|     ! 0 | 1638 | `				PH7_MemObjToReal(pValue);` |
|     ! 0 | 1639 | `				return SXRET_OK;` |
|       - | 1640 | `			}` |
|      10 | 1641 | `			if( kind == 1 \|\| kind == 2 ){` |
|       7 | 1642 | `				PH7_MemObjToReal(pValue);` |
|       7 | 1643 | `				return SXRET_OK;` |
|       - | 1644 | `			}` |
|       1 | 1645 | `		}` |
|      26 | 1646 | `		if( bHasString ){` |
|     ! 0 | 1647 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|     ! 0 | 1648 | `				PH7_MemObjToString(pValue);` |
|     ! 0 | 1649 | `				return SXRET_OK;` |
|       - | 1650 | `			}` |
|     ! 0 | 1651 | `		}` |
|      26 | 1652 | `		if( bHasBool ){` |
|       3 | 1653 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|       3 | 1654 | `				PH7_MemObjToBool(pValue);` |
|       3 | 1655 | `				return SXRET_OK;` |
|       - | 1656 | `			}` |
|     ! 0 | 1657 | `		}` |
|       - | 1658 | `	}` |
|      24 | 1659 | `	return SXERR_INVALID;` |
|     167 | 1660 | `}` |
|       - | 1661 |  |
|       - | 1662 | `/*` |
|       - | 1663 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|       - | 1664 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|       - | 1665 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|       - | 1666 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|       - | 1667 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|       - | 1668 | ` */` |
|     274 | 1669 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|       5 | 1670 | `{` |
|       - | 1671 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|       - | 1672 | `	 * null value satisfies it (and a null value matches via the flag test` |
|       - | 1673 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|       - | 1674 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|       - | 1675 | `	 * silently swallow any argument. */` |
|     279 | 1676 | `	if( nType == MEMOBJ_NULL ){` |
|       3 | 1677 | `		return SXERR_INVALID;` |
|       - | 1678 | `	}` |
|       - | 1679 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|       - | 1680 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|       - | 1681 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|       - | 1682 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|       - | 1683 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|       - | 1684 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|       - | 1685 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|       - | 1686 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|       - | 1687 | `	 * object value against an array type is caught here too (never valid);` |
|       - | 1688 | `	 * object->scalar stays a separate case handled by the callers. */` |
|     277 | 1689 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|      34 | 1690 | `		return SXERR_INVALID;` |
|       - | 1691 | `	}` |
|     245 | 1692 | `	if( bStrict ){` |
|       - | 1693 | `		/* Only int -> float widening is allowed implicitly. */` |
|      30 | 1694 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|       3 | 1695 | `			PH7_MemObjToReal(pVal);` |
|       3 | 1696 | `			return SXRET_OK;` |
|       - | 1697 | `		}` |
|      28 | 1698 | `		return SXERR_INVALID;` |
|       - | 1699 | `	}` |
|       - | 1700 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|       - | 1701 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|       - | 1702 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|       - | 1703 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|       - | 1704 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|       - | 1705 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|       - | 1706 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     219 | 1707 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      20 | 1708 | `		return SXERR_INVALID;` |
|       - | 1709 | `	}` |
|       - | 1710 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|       - | 1711 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|       - | 1712 | `	 * is invoked by the string cast below). Every other scalar target —` |
|       - | 1713 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|       - | 1714 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|       - | 1715 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|       - | 1716 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|       - | 1717 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|       - | 1718 | `	 * is caught by the array guard.) */` |
|     201 | 1719 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      23 | 1720 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      28 | 1721 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      10 | 1722 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      13 | 1723 | `			return SXERR_INVALID;` |
|       - | 1724 | `		}` |
|       4 | 1725 | `	}` |
|     184 | 1726 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|     142 | 1727 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|     145 | 1728 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      46 | 1729 | `		return SXERR_INVALID;` |
|       - | 1730 | `	}` |
|     147 | 1731 | `	if( nType == MEMOBJ_INT && pVal->pVm ){` |
|       - | 1732 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion` |
|       - | 1733 | `		 * (typed params, returns, typed property stores all funnel through here);` |
|       - | 1734 | `		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly` |
|       - | 1735 | `		 * like the null / non-numeric-string cases above. An INTEGRAL float loses` |
|       - | 1736 | `		 * nothing and coerces normally. */` |
|      55 | 1737 | `		if( pVal->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1738 | `			ph7_real r = pVal->rVal;` |
|     ! 0 | 1739 | `			if( r != (ph7_real)(sxi64)r ){` |
|     ! 0 | 1740 | `				return SXERR_INVALID;` |
|     ! 0 | 1741 | `			}` |
|      55 | 1742 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       - | 1743 | `			SyString sStr;` |
|       - | 1744 | `			ph7_value sProbe;` |
|       - | 1745 | `			int bLossy;` |
|      51 | 1746 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|      51 | 1747 | `			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|      51 | 1748 | `			PH7_MemObjToNumeric(&sProbe);` |
|      51 | 1749 | `			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;` |
|      51 | 1750 | `			PH7_MemObjRelease(&sProbe);` |
|      51 | 1751 | `			if( bLossy ){` |
|     ! 0 | 1752 | `				return SXERR_INVALID;` |
|       - | 1753 | `			}` |
|      23 | 1754 | `		}` |
|      25 | 1755 | `	}` |
|       - | 1756 | `	{` |
|     147 | 1757 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|     147 | 1758 | `		if( xCast ) xCast(pVal);` |
|       - | 1759 | `	}` |
|     147 | 1760 | `	return SXRET_OK;` |
|     142 | 1761 | `}` |
|       - | 1762 |  |
|       - | 1763 | `/*` |
|       - | 1764 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|       - | 1765 | ` * TypeError message. Prefers the declared textual form when available.` |
|       - | 1766 | ` *` |
|       - | 1767 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|       - | 1768 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|       - | 1769 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|       - | 1770 | ` * back to a static literal and ignore zBuf entirely.` |
|       - | 1771 | ` */` |
|     190 | 1772 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|       5 | 1773 | `{` |
|     195 | 1774 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     195 | 1775 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     195 | 1776 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     195 | 1777 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     195 | 1778 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      95 | 1779 | `		}` |
|     195 | 1780 | `		zBuf[nCopy] = 0;` |
|     195 | 1781 | `		return zBuf;` |
|       - | 1782 | `	}` |
|     ! 0 | 1783 | `	switch( nType ){` |
|     ! 0 | 1784 | `		case MEMOBJ_INT:     return "int";` |
|     ! 0 | 1785 | `		case MEMOBJ_REAL:    return "float";` |
|     ! 0 | 1786 | `		case MEMOBJ_STRING:  return "string";` |
|     ! 0 | 1787 | `		case MEMOBJ_BOOL:    return "bool";` |
|     ! 0 | 1788 | `		case MEMOBJ_HASHMAP: return "array";` |
|     ! 0 | 1789 | `		case MEMOBJ_OBJ:     return "object";` |
|     ! 0 | 1790 | `		default:             return "scalar";` |
|       - | 1791 | `	}` |
|     100 | 1792 | `}` |
|       - | 1793 |  |
|       - | 1794 | `/*` |
|       - | 1795 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|       - | 1796 | ` * the way php writes it in a TypeError:` |
|       - | 1797 | ` *` |
|       - | 1798 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|       - | 1799 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|       - | 1800 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|       - | 1801 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|       - | 1802 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|       - | 1803 | ` *` |
|       - | 1804 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|       - | 1805 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|       - | 1806 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|       - | 1807 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|       - | 1808 | ` */` |
|     168 | 1809 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|       - | 1810 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|       5 | 1811 | `{` |
|     173 | 1812 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|       - | 1813 | `	sxu32 nCopy;` |
|     173 | 1814 | `	sxu32 nAt = 0;` |
|     173 | 1815 | `	if( nBuf == 0 ){` |
|     ! 0 | 1816 | `		return "";` |
|       - | 1817 | `	}` |
|     168 | 1818 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|      73 | 1819 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|      15 | 1820 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|      15 | 1821 | `		nCopy = SyStrlen(zIter);` |
|      15 | 1822 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      15 | 1823 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|      15 | 1824 | `		zBuf[nCopy] = 0;` |
|      15 | 1825 | `		return zBuf;` |
|       - | 1826 | `	}` |
|     161 | 1827 | `	if( bNullable && nBuf > 1 ){` |
|      23 | 1828 | `		zBuf[nAt++] = '?';` |
|      10 | 1829 | `	}` |
|     161 | 1830 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|     161 | 1831 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|     161 | 1832 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|     161 | 1833 | `	zBuf[nAt + nCopy] = 0;` |
|     161 | 1834 | `	return zBuf;` |
|      89 | 1835 | `}` |
|       - | 1836 |  |
|       - | 1837 | `/*` |
|       - | 1838 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|       - | 1839 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|       - | 1840 | ` */` |
|     140 | 1841 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|       5 | 1842 | `{` |
|     145 | 1843 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     215 | 1844 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|     140 | 1845 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|     145 | 1846 | `	return zBuf;` |
|       5 | 1847 | `}` |
|       - | 1848 |  |
| 3012964 | 1849 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|       5 | 1850 | `{` |
|       - | 1851 | `	SyHashEntry *pSlot;` |
|       - | 1852 | `	VmClassAttr *pVmAttr;` |
|       - | 1853 | `	ph7_class_attr *pAttr;` |
|       - | 1854 | `	ph7_class *pHintScope;` |
|       - | 1855 | `	char zGivenBuf[128];` |
| 3012969 | 1856 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
| 3012969 | 1857 | `	if( pSlot == 0 ){` |
| 2912385 | 1858 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1859 | `	}` |
|  100589 | 1860 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|  100589 | 1861 | `	pAttr = pVmAttr->pAttr;` |
|  100589 | 1862 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1863 | `		return SXRET_OK;` |
|       - | 1864 | `	}` |
|       - | 1865 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|       - | 1866 | `	 * the property (a trait's members count as the composing class), not the` |
|       - | 1867 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|  100589 | 1868 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|       - | 1869 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|       - | 1870 | `	 * property may be written exactly once and only from within the declaring` |
|       - | 1871 | `	 * class scope (its set-scope is protected). */` |
|  100589 | 1872 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1873 | `		/* A readonly property is always typed and default-less, so it starts` |
|       - | 1874 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|       - | 1875 | `		 * write below — making it the write-once latch (a type-rejected write` |
|       - | 1876 | `		 * leaves it set, so a later valid initialization still works). */` |
|      83 | 1877 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|       - | 1878 | `			/* Already initialized: any further write is forbidden, any scope —` |
|       - | 1879 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|       - | 1880 | `			 * Exceptions that fall through to the set-scope check below:` |
|       - | 1881 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|       - | 1882 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|       - | 1883 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|      24 | 1884 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      24 | 1885 | `			if( !(pCloneFr && pCloneFr->pThis` |
|      13 | 1886 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|      22 | 1887 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|       - | 1888 | `			}` |
|       1 | 1889 | `		}` |
|      30 | 1890 | `	}` |
|  100571 | 1891 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|       - | 1892 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|       - | 1893 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|      27 | 1894 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|      27 | 1895 | `		if( rcVis != SXRET_OK ){` |
|      13 | 1896 | `			return rcVis;` |
|       1 | 1897 | `		}` |
|  100552 | 1898 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1899 | `		/* First write (or a clone re-init) must come from within the declaring` |
|       - | 1900 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      63 | 1901 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      63 | 1902 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1903 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|       - | 1904 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|       - | 1905 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|      63 | 1906 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|       5 | 1907 | `			pDecl = pVmAttr->pOwner;` |
|       2 | 1908 | `		}` |
|      63 | 1909 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|       6 | 1910 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|       - | 1911 | `		}` |
|      27 | 1912 | `	}` |
|       - | 1913 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|       - | 1914 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|       - | 1915 | `	 * matching PHP's documented behavior. */` |
|  100555 | 1916 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      80 | 1917 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|      50 | 1918 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|      25 | 1919 | `			0 /* bStrict: properties never apply strict_types */,pHintScope);` |
|      55 | 1920 | `		if( rc == SXRET_OK ){` |
|      35 | 1921 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      35 | 1922 | `			return SXRET_OK;` |
|       - | 1923 | `		}` |
|      22 | 1924 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1925 | `			char zBuf[128];` |
|      21 | 1926 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 1927 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 1928 | `		}` |
|       8 | 1929 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1930 | `	}` |
|       - | 1931 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|       - | 1932 | `	 * includes null). */` |
|  100505 | 1933 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      28 | 1934 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|      24 | 1935 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       2 | 1936 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|      22 | 1937 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      22 | 1938 | `			return SXRET_OK;` |
|       - | 1939 | `		}` |
|      12 | 1940 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|       - | 1941 | `	}` |
|       - | 1942 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|       - | 1943 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|       - | 1944 | `	 * type error. */` |
|  100477 | 1945 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 1946 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1947 | `	}` |
|       - | 1948 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|       - | 1949 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|       - | 1950 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|  100477 | 1951 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      12 | 1952 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       5 | 1953 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       5 | 1954 | `			return SXRET_OK;` |
|       - | 1955 | `		}` |
|       7 | 1956 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1957 | `	}` |
|       - | 1958 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|       - | 1959 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|       - | 1960 | `	 * handled by the nullable check above). Checked by value before the generic` |
|       - | 1961 | `	 * class-instanceof branch, which would resolve no such class and then` |
|       - | 1962 | `	 * wrongly accept any object / reject arrays. */` |
|  100467 | 1963 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      70 | 1964 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|      70 | 1965 | `		if( rcPseudo == 1 ){` |
|      11 | 1966 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      11 | 1967 | `			return SXRET_OK;` |
|       - | 1968 | `		}` |
|      60 | 1969 | `		if( rcPseudo == 0 ){` |
|       3 | 1970 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1971 | `		}` |
|       - | 1972 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|      27 | 1973 | `	}` |
|  100455 | 1974 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       - | 1975 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|       - | 1976 | `		 * class (pHintScope), not the instance's runtime class. */` |
|      58 | 1977 | `		ph7_class *pExpected = 0;` |
|      58 | 1978 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|       - | 1979 | `			char zBuf[128];` |
|      31 | 1980 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      18 | 1981 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      18 | 1982 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|     ! 0 | 1983 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1984 | `		}` |
|      40 | 1985 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      40 | 1986 | `		return SXRET_OK;` |
|       - | 1987 | `	}` |
|       - | 1988 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|       - | 1989 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|       - | 1990 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|       - | 1991 | `	 * via __toString (typed property stores are always weak mode), so fall` |
|       - | 1992 | `	 * through to the string cast below. */` |
|  100401 | 1993 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      17 | 1994 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      19 | 1995 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       4 | 1996 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       - | 1997 | `			char zBuf[128];` |
|      21 | 1998 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 1999 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2000 | `		}` |
|       1 | 2001 | `	}` |
|  100389 | 2002 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|  100061 | 2003 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|  100061 | 2004 | `		if( xCast ){` |
|       - | 2005 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|  100061 | 2006 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       8 | 2007 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2008 | `			}` |
|  100055 | 2009 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       6 | 2010 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 2011 | `			}` |
|       - | 2012 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|       - | 2013 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|       - | 2014 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|  100046 | 2015 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|  100043 | 2016 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|  100048 | 2017 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|  100033 | 2018 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|       - | 2019 | `			}` |
|      21 | 2020 | `			xCast(pValue);` |
|       9 | 2021 | `		}` |
|      12 | 2022 | `	}else{` |
|       - | 2023 | `		/* Mask matched — an int property accepting a whole-real must` |
|       - | 2024 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|     333 | 2025 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 2026 | `	}` |
|     351 | 2027 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     351 | 2028 | `	return SXRET_OK;` |
| 1506487 | 2029 | `}` |
|       - | 2030 | `/*` |
|       - | 2031 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|       - | 2032 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|       - | 2033 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|       - | 2034 | ` *     a scope that could normally reach it — else a catchable Error),` |
|       - | 2035 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|       - | 2036 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|       - | 2037 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|       - | 2038 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|       - | 2039 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|       - | 2040 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|       - | 2041 | ` */` |
|      18 | 2042 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|       - | 2043 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|       1 | 2044 | `{` |
|      19 | 2045 | `	ph7_class *pClass = pClone->pClass;` |
|       - | 2046 | `	SyHashEntry *pEntry;` |
|       - | 2047 | `	VmClassAttr *pVmAttr;` |
|       - | 2048 | `	ph7_class_attr *pAttr;` |
|       - | 2049 | `	ph7_value *pSlot;` |
|       - | 2050 | `	sxi32 rc;` |
|      19 | 2051 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|      19 | 2052 | `	if( pEntry == 0 ){` |
|       - | 2053 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|       - | 2054 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|       - | 2055 | `		 * deferred residual). */` |
|     ! 0 | 2056 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|     ! 0 | 2057 | `		if( pSlot == 0 ){` |
|     ! 0 | 2058 | `			return PH7_VmMemoryError(pVm);` |
|       - | 2059 | `		}` |
|     ! 0 | 2060 | `		PH7_MemObjStore(pValue,pSlot);` |
|     ! 0 | 2061 | `		return SXRET_OK;` |
|       - | 2062 | `	}` |
|      19 | 2063 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      19 | 2064 | `	pAttr = pVmAttr->pAttr;` |
|       - | 2065 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|      19 | 2066 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - | 2067 | `		SyBlob sMsg;` |
|     ! 0 | 2068 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     ! 0 | 2069 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|     ! 0 | 2070 | `			&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2071 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2072 | `	}` |
|       - | 2073 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|       - | 2074 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|       - | 2075 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|      19 | 2076 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       3 | 2077 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|       3 | 2078 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 2079 | `		SyBlob sMsg;` |
|       3 | 2080 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 2081 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|       3 | 2082 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 2083 | `	}` |
|       - | 2084 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|      17 | 2085 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|      17 | 2086 | `	if( rc != SXRET_OK ){` |
|       3 | 2087 | `		return rc;` |
|       - | 2088 | `	}` |
|       - | 2089 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|      15 | 2090 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|      15 | 2091 | `	if( pSlot ){` |
|      15 | 2092 | `		PH7_MemObjStore(pValue,pSlot);` |
|       7 | 2093 | `	}` |
|      15 | 2094 | `	return SXRET_OK;` |
|      10 | 2095 | `}` |
|       - | 2096 | `/*` |
|       - | 2097 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|       - | 2098 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|       - | 2099 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|       - | 2100 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|       - | 2101 | ` */` |
|      10 | 2102 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       2 | 2103 | `{` |
|      12 | 2104 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2105 | `	char zBuf[128],zType[192];` |
|       - | 2106 | `	const char *zGiven;` |
|      17 | 2107 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       5 | 2108 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|      12 | 2109 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2110 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2111 | `	}else{` |
|      12 | 2112 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2113 | `	}` |
|      12 | 2114 | `	if( bLazy ){` |
|       - | 2115 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|       - | 2116 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|       - | 2117 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|       - | 2118 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|       - | 2119 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|       - | 2120 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|       - | 2121 | `		 * re-evaluates and re-raises, as php's does. */` |
|       - | 2122 | `		SyBlob sMsg;` |
|       5 | 2123 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       5 | 2124 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|       2 | 2125 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       5 | 2126 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - | 2127 | `	}` |
|       - | 2128 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|       - | 2129 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|       - | 2130 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|       - | 2131 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|       - | 2132 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|       - | 2133 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|       - | 2134 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       8 | 2135 | `	if( pVm->sCodeGen.xErr ){` |
|       8 | 2136 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|       - | 2137 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       2 | 2138 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       4 | 2139 | `	}else{` |
|       4 | 2140 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 2141 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|       1 | 2142 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       - | 2143 | `	}` |
|       8 | 2144 | `	pVm->iExitStatus = 255;` |
|       8 | 2145 | `	pVm->bHaltRequested = 1;` |
|       8 | 2146 | `	return SXERR_ABORT;` |
|       7 | 2147 | `}` |
|       - | 2148 | `/*` |
|       - | 2149 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|       - | 2150 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|       - | 2151 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|       - | 2152 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|       - | 2153 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|       - | 2154 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|       - | 2155 | ` */` |
|      40 | 2156 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|       3 | 2157 | `{` |
|      43 | 2158 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|       - | 2159 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|      43 | 2160 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 2161 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       3 | 2162 | `			return SXRET_OK;` |
|       - | 2163 | `		}` |
|     ! 0 | 2164 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|     ! 0 | 2165 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 2166 | `			return SXRET_OK;` |
|       - | 2167 | `		}` |
|     ! 0 | 2168 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2169 | `	}` |
|       - | 2170 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|      41 | 2171 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       6 | 2172 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       5 | 2173 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       5 | 2174 | `			return SXRET_OK;` |
|       - | 2175 | `		}` |
|     ! 0 | 2176 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2177 | `	}` |
|       - | 2178 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|      37 | 2179 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2180 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2181 | `	}` |
|       - | 2182 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|      37 | 2183 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2184 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2185 | `			return SXRET_OK;` |
|       - | 2186 | `		}` |
|     ! 0 | 2187 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2188 | `	}` |
|       - | 2189 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|       - | 2190 | `	 * a real class/interface verified by instanceof. */` |
|      37 | 2191 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       3 | 2192 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       3 | 2193 | `		if( rcPseudo == 1 ){` |
|     ! 0 | 2194 | `			return SXRET_OK;` |
|       - | 2195 | `		}` |
|       3 | 2196 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2197 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2198 | `		}` |
|       - | 2199 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|       - | 2200 | `		 * self/parent resolve against the declaring class. */` |
|       - | 2201 | `		{` |
|       3 | 2202 | `			ph7_class *pExpected = 0;` |
|       4 | 2203 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|       1 | 2204 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|       3 | 2205 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       - | 2206 | `			}` |
|       - | 2207 | `		}` |
|     ! 0 | 2208 | `		return SXRET_OK;` |
|       - | 2209 | `	}` |
|       - | 2210 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|       - | 2211 | `	 * implicit widening. Everything else is a type error.` |
|       - | 2212 | `	 *` |
|       - | 2213 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|       - | 2214 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|       - | 2215 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|       - | 2216 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|       - | 2217 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|       - | 2218 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|       - | 2219 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|       - | 2220 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|       - | 2221 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|       - | 2222 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|       - | 2223 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|      35 | 2224 | `	if( pValue->iFlags & pAttr->nType ){` |
|      23 | 2225 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      23 | 2226 | `		return SXRET_OK;` |
|       - | 2227 | `	}` |
|      13 | 2228 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       3 | 2229 | `		PH7_MemObjToReal(pValue);` |
|       3 | 2230 | `		return SXRET_OK;` |
|       - | 2231 | `	}` |
|      10 | 2232 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|      23 | 2233 | `}` |
|       - | 2234 | `/*` |
|       - | 2235 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|       - | 2236 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|       - | 2237 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|       - | 2238 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|       - | 2239 | ` */` |
|      34 | 2240 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       2 | 2241 | `{` |
|      36 | 2242 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2243 | `	const char *zGiven;` |
|       - | 2244 | `	char zBuf[128],zType[192];` |
|      53 | 2245 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|      17 | 2246 | `		VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),zType,sizeof(zType));` |
|       - | 2247 | `	SyBlob sMsg;` |
|      36 | 2248 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2249 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 2250 | `	}else{` |
|      36 | 2251 | `		zGiven = ph7_type_name(pValue);` |
|       - | 2252 | `	}` |
|      36 | 2253 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      36 | 2254 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|      17 | 2255 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|      36 | 2256 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       2 | 2257 | `}` |
|       - | 2258 | `/*` |
|       - | 2259 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|       - | 2260 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|       - | 2261 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|       - | 2262 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|       - | 2263 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|       - | 2264 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|       - | 2265 | ` * instance defaults per-instantiation. Matching structure of` |
|       - | 2266 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|       - | 2267 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|       - | 2268 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|       - | 2269 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|       - | 2270 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|       - | 2271 | ` */` |
|       - | 2272 | `/*` |
|       - | 2273 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|       - | 2274 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|       - | 2275 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|       - | 2276 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|       - | 2277 | ` * throwing, so the static-property mount path can defer the failure (php` |
|       - | 2278 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|       - | 2279 | ` * instance path throws immediately via the wrapper below.` |
|       - | 2280 | ` */` |
|     308 | 2281 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2282 | `{` |
|     313 | 2283 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|     313 | 2284 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      51 | 2285 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      49 | 2286 | `			return SXRET_OK;` |
|       - | 2287 | `		}` |
|       2 | 2288 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       1 | 2289 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 2290 | `			return SXRET_OK;` |
|       - | 2291 | `		}` |
|       3 | 2292 | `		return SXERR_INVALID;` |
|       - | 2293 | `	}` |
|     265 | 2294 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      36 | 2295 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|      29 | 2296 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|      29 | 2297 | `			return SXRET_OK;` |
|       - | 2298 | `		}` |
|     ! 0 | 2299 | `		return SXERR_INVALID;` |
|       - | 2300 | `	}` |
|     241 | 2301 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 2302 | `		return SXERR_INVALID;` |
|       - | 2303 | `	}` |
|     241 | 2304 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 2305 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2306 | `			return SXRET_OK;` |
|       - | 2307 | `		}` |
|     ! 0 | 2308 | `		return SXERR_INVALID;` |
|       - | 2309 | `	}` |
|     241 | 2310 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       5 | 2311 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       5 | 2312 | `		if( rcPseudo == 1 ){` |
|       5 | 2313 | `			return SXRET_OK;` |
|       - | 2314 | `		}` |
|     ! 0 | 2315 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 2316 | `			return SXERR_INVALID;` |
|       - | 2317 | `		}` |
|       - | 2318 | `		{` |
|       - | 2319 | `			/* self/parent in the hint resolve against the declaring class. */` |
|     ! 0 | 2320 | `			ph7_class *pExpected = 0;` |
|     ! 0 | 2321 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|     ! 0 | 2322 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|     ! 0 | 2323 | `				return SXERR_INVALID;` |
|       - | 2324 | `			}` |
|       - | 2325 | `		}` |
|     ! 0 | 2326 | `		return SXRET_OK;` |
|       - | 2327 | `	}` |
|     237 | 2328 | `	if( pValue->iFlags & pAttr->nType ){` |
|     201 | 2329 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|     201 | 2330 | `		return SXRET_OK;` |
|       - | 2331 | `	}` |
|      38 | 2332 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       6 | 2333 | `		PH7_MemObjToReal(pValue);` |
|       6 | 2334 | `		return SXRET_OK;` |
|       - | 2335 | `	}` |
|      34 | 2336 | `	return SXERR_INVALID;` |
|     159 | 2337 | `}` |
|     264 | 2338 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2339 | `{` |
|     269 | 2340 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|     257 | 2341 | `		return SXRET_OK;` |
|       - | 2342 | `	}` |
|      13 | 2343 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|     137 | 2344 | `}` |
|       - | 2345 | `/*` |
|       - | 2346 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|       - | 2347 | ` * class chain for a static typed slot whose mount-time default failed its` |
|       - | 2348 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|       - | 2349 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|       - | 2350 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|       - | 2351 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|       - | 2352 | ` * never-touched class stays silent, and the throw repeats on every access` |
|       - | 2353 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|       - | 2354 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|       - | 2355 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|       - | 2356 | ` */` |
|      26 | 2357 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2358 | `{` |
|       - | 2359 | `	ph7_class *pScan;` |
|      33 | 2360 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       - | 2361 | `		SyHashEntry *pEntry;` |
|      29 | 2362 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|      33 | 2363 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|      29 | 2364 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      26 | 2365 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|       - | 2366 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|      24 | 2367 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      25 | 2368 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|      24 | 2369 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      24 | 2370 | `				if( pSlot ){` |
|      24 | 2371 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      24 | 2372 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|      24 | 2373 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       - | 2374 | `						ph7_value sNull;` |
|      24 | 2375 | `						if( pValue == 0 ){` |
|     ! 0 | 2376 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|     ! 0 | 2377 | `							pValue = &sNull;` |
|     ! 0 | 2378 | `						}` |
|      24 | 2379 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|       - | 2380 | `					}` |
|     ! 0 | 2381 | `				}` |
|     ! 0 | 2382 | `			}` |
|       1 | 2383 | `		}` |
|       3 | 2384 | `	}` |
|       5 | 2385 | `	return SXRET_OK;` |
|      16 | 2386 | `}` |
|       - | 2387 | `/*` |
|       - | 2388 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|       - | 2389 | ` * materialization: an initializer that threw at mount and was deferred` |
|       - | 2390 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|       - | 2391 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|       - | 2392 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|       - | 2393 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|       - | 2394 | ` * a subclass can be mounted before the base whose default failed.` |
|       - | 2395 | ` */` |
| 1208126 | 2396 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|       5 | 2397 | `{` |
| 2418211 | 2398 | `	while( pClass ){` |
| 1210167 | 2399 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|      85 | 2400 | `			return 1;` |
|       - | 2401 | `		}` |
| 1210085 | 2402 | `		pClass = pClass->pBase;` |
|       5 | 2403 | `	}` |
| 1208049 | 2404 | `	return 0;` |
|  604068 | 2405 | `}` |
|       - | 2406 | `/*` |
|       - | 2407 | ` * Re-run the initializers of the static properties whose evaluation was` |
|       - | 2408 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|       - | 2409 | ` *` |
|       - | 2410 | ` * php builds a class's static table on first use, evaluating each slot's` |
|       - | 2411 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|       - | 2412 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|       - | 2413 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|       - | 2414 | ` * the access: a constant define()d after the class declaration resolves.` |
|       - | 2415 | ` *` |
|       - | 2416 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|       - | 2417 | ` * the base's broken default, not its own), then declaration order within a` |
|       - | 2418 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|       - | 2419 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|       - | 2420 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|       - | 2421 | ` * re-raises on every access too.` |
|       - | 2422 | ` */` |
|      86 | 2423 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|       3 | 2424 | `{` |
|       - | 2425 | `	SyHashEntry *pEntry;` |
|       - | 2426 | `	sxi32 rc;` |
|      89 | 2427 | `	if( pClass->pBase ){` |
|       6 | 2428 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|       6 | 2429 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2430 | `			return rc;` |
|       - | 2431 | `		}` |
|       2 | 2432 | `	}` |
|      89 | 2433 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     209 | 2434 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     123 | 2435 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     123 | 2436 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|       - | 2437 | `			/* Not pending. An inherited slot the base pass already collected` |
|       - | 2438 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|       - | 2439 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|      61 | 2440 | `			continue;` |
|       - | 2441 | `		}` |
|      65 | 2442 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - | 2443 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|       - | 2444 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|       - | 2445 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|       - | 2446 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|       - | 2447 | `			 * class keeps its hint flag so a later access retries. */` |
|     ! 0 | 2448 | `			*pbLeft = 1;` |
|     ! 0 | 2449 | `			continue;` |
|       - | 2450 | `		}` |
|      65 | 2451 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|      65 | 2452 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2453 | `			return rc;` |
|       - | 2454 | `		}` |
|       3 | 2455 | `	}` |
|      89 | 2456 | `	return SXRET_OK;` |
|      46 | 2457 | `}` |
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
|       - | 2474 | ` *` |
|       - | 2475 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|       - | 2476 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|       - | 2477 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|       - | 2478 | ` * cut the outer one short.` |
|       - | 2479 | ` */` |
|      82 | 2480 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|       3 | 2481 | `{` |
|       - | 2482 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|       - | 2483 | `	ph7_class_attr **apPending;` |
|       - | 2484 | `	sxu32 n,nUsed;` |
|       - | 2485 | `	sxi32 rc;` |
|      85 | 2486 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|      85 | 2487 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|      85 | 2488 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|      85 | 2489 | `	nUsed = SySetUsed(&aPending);` |
|      89 | 2490 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|      63 | 2491 | `		ph7_class_attr *pAttr = apPending[n];` |
|      63 | 2492 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 2493 | `		ph7_class *pSaveCtx;` |
|       - | 2494 | `		void *pSaveFrame;` |
|       - | 2495 | `		sxu32 nSaveLazyLine;` |
|       - | 2496 | `		sxi32 nSaveLazyDepth;` |
|       - | 2497 | `		ph7_value *pMemObj;` |
|       - | 2498 | `		sxi32 rcExec;` |
|      63 | 2499 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|     ! 0 | 2500 | `			continue; /* the base pass already ran this shared slot */` |
|       - | 2501 | `		}` |
|      63 | 2502 | `		pMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      63 | 2503 | `		if( pMemObj == 0 ){` |
|     ! 0 | 2504 | `			continue;` |
|       - | 2505 | `		}` |
|      63 | 2506 | `		pSaveCtx = pVm->pConstEvalClass;` |
|      63 | 2507 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|      63 | 2508 | `		pVm->pConstEvalClass = pOwner;` |
|       - | 2509 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|       - | 2510 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|       - | 2511 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|       - | 2512 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|       - | 2513 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|      63 | 2514 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       - | 2515 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|       - | 2516 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|      63 | 2517 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      63 | 2518 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      63 | 2519 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      63 | 2520 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      63 | 2521 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|      63 | 2522 | `		pVm->nConstEvalDepth++;` |
|      63 | 2523 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      63 | 2524 | `		pVm->nConstEvalDepth--;` |
|      63 | 2525 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      63 | 2526 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      63 | 2527 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      63 | 2528 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      63 | 2529 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      63 | 2530 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - | 2531 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|       - | 2532 | `			 * caller to route (a catch here is the user's own). */` |
|      59 | 2533 | `			rc = rcExec;` |
|      59 | 2534 | `			break;` |
|       - | 2535 | `		}` |
|       5 | 2536 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|       5 | 2537 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|       - | 2538 | `			/* The initializer named a self-referencing constant. Like the mount` |
|       - | 2539 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|       - | 2540 | `			 * the access, where a catch can see it. */` |
|     ! 0 | 2541 | `			rc = VmConstCycleThrow(&(*pVm));` |
|     ! 0 | 2542 | `			break;` |
|       - | 2543 | `		}` |
|       4 | 2544 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       3 | 2545 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - | 2546 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|       - | 2547 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|       - | 2548 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|     ! 0 | 2549 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|     ! 0 | 2550 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|     ! 0 | 2551 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     ! 0 | 2552 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|     ! 0 | 2553 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|     ! 0 | 2554 | `				break;` |
|       - | 2555 | `			}` |
|     ! 0 | 2556 | `		}` |
|       3 | 2557 | `	}` |
|      85 | 2558 | `	if( rc != SXRET_OK ){` |
|      59 | 2559 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|      28 | 2560 | `	}` |
|      85 | 2561 | `	SySetRelease(&aPending);` |
|      85 | 2562 | `	return rc;` |
|       3 | 2563 | `}` |
|       - | 2564 | `/*` |
|       - | 2565 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|       - | 2566 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|       - | 2567 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|       - | 2568 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|       - | 2569 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|       - | 2570 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|       - | 2571 | ` */` |
|      82 | 2572 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|       3 | 2573 | `{` |
|      85 | 2574 | `	int bLeft = 0;` |
|      85 | 2575 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|      85 | 2576 | `	if( rc == SXRET_OK ){` |
|      29 | 2577 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|      13 | 2578 | `	}` |
|      85 | 2579 | `	if( rc == SXRET_OK && !bLeft ){` |
|       - | 2580 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|       - | 2581 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|       - | 2582 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|       - | 2583 | `		 * set — php's materialization keeps failing too. */` |
|       - | 2584 | `		ph7_class *pScan;` |
|       9 | 2585 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       5 | 2586 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|       3 | 2587 | `		}` |
|       2 | 2588 | `	}` |
|      85 | 2589 | `	return rc;` |
|       3 | 2590 | `}` |
|       - | 2591 |  |
|       - | 2592 | `/*` |
|       - | 2593 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 2594 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 2595 | ` * information.` |
|       - | 2596 | ` * ------------------------------------` |
|       - | 2597 | ` * Simple boring wrapper function.` |
|       - | 2598 | ` * ------------------------------------` |
|       - | 2599 | ` */` |
|     230 | 2600 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|       5 | 2601 | `{` |
|       - | 2602 | `	va_list ap;` |
|       - | 2603 | `	sxi32 rc;` |
|     235 | 2604 | `	va_start(ap,zFormat);` |
|     235 | 2605 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     235 | 2606 | `	va_end(ap);` |
|     235 | 2607 | `	return rc;` |
|       5 | 2608 | `}` |
|       - | 2609 | `/*` |
|       - | 2610 | ` * Throw a TypeError exception from within the VM execution loop.` |
|       - | 2611 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|       - | 2612 | ` */` |
|     364 | 2613 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|       5 | 2614 | `{` |
|       - | 2615 | `	ph7_class *pClass;` |
|       - | 2616 | `	ph7_class_instance *pThis;` |
|       - | 2617 | `	ph7_class_method *pCons;` |
|       - | 2618 | `	ph7_value sArg;` |
|       - | 2619 | `	ph7_value *apArg[1];` |
|       - | 2620 | `	SyBlob sMsg;` |
|       - | 2621 | `	SyString sMsgStr;` |
|     369 | 2622 | `	SyString *pFuncName = &pCallee->sName;` |
|       - | 2623 | `	VmFrame *pFrame;` |
|       - | 2624 | `	sxi32 rc;` |
|     369 | 2625 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     369 | 2626 | `	if( pClass == 0 ){` |
|     ! 0 | 2627 | `		return PH7_ABORT;` |
|       - | 2628 | `	}` |
|     369 | 2629 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     369 | 2630 | `	if( pThis == 0 ){` |
|     ! 0 | 2631 | `		return PH7_ABORT;` |
|       - | 2632 | `	}` |
|     369 | 2633 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2634 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|       - | 2635 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|       - | 2636 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|       - | 2637 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|       - | 2638 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|     549 | 2639 | `	if( pOwnerClass ){` |
|       - | 2640 | `		/* A property hook is named after its PROPERTY, never after the method` |
|       - | 2641 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|       - | 2642 | `		SyBlob sHook;` |
|      41 | 2643 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|      41 | 2644 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|       6 | 2645 | `			if( pArgName ){` |
|       6 | 2646 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|       4 | 2647 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|       2 | 2648 | `					nArg,pArgName,zExpected,zGiven);` |
|       4 | 2649 | `			}else{` |
|     ! 0 | 2650 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|     ! 0 | 2651 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|     ! 0 | 2652 | `					nArg,zExpected,zGiven);` |
|       - | 2653 | `			}` |
|       6 | 2654 | `			SyBlobRelease(&sHook);` |
|       6 | 2655 | `			goto ArgMsgBuilt;` |
|       - | 2656 | `		}` |
|      37 | 2657 | `		SyBlobRelease(&sHook);` |
|      37 | 2658 | `		if( pArgName ){` |
|      29 | 2659 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|      12 | 2660 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|      17 | 2661 | `		}else{` |
|      11 | 2662 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|       4 | 2663 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|       - | 2664 | `		}` |
|      21 | 2665 | `	}else{` |
|       - | 2666 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|     333 | 2667 | `		const char *zShow = 0;` |
|     333 | 2668 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     333 | 2669 | `		if( pArgName ){` |
|     265 | 2670 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|     130 | 2671 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|     135 | 2672 | `		}else{` |
|      72 | 2673 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      34 | 2674 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|       - | 2675 | `		}` |
|       - | 2676 | `	}` |
|     182 | 2677 | `ArgMsgBuilt:` |
|       - | 2678 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|       - | 2679 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|       - | 2680 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|     369 | 2681 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|     369 | 2682 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     369 | 2683 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|     369 | 2684 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|     182 | 2685 | `		}` |
|     182 | 2686 | `	}` |
|     369 | 2687 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     369 | 2688 | `	if( pCons ){` |
|     369 | 2689 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     369 | 2690 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     369 | 2691 | `		apArg[0] = &sArg;` |
|     369 | 2692 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     369 | 2693 | `		PH7_MemObjRelease(&sArg);` |
|     182 | 2694 | `	}` |
|     369 | 2695 | `	SyBlobRelease(&sMsg);` |
|     369 | 2696 | `	pFrame = pVm->pFrame;` |
|     369 | 2697 | `	if( pFrame ){` |
|     369 | 2698 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     369 | 2699 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     182 | 2700 | `	}` |
|     369 | 2701 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     369 | 2702 | `	PH7_ClassInstanceUnref(pThis);` |
|     369 | 2703 | `	if( rc == SXERR_ABORT ){` |
|       6 | 2704 | `		return PH7_ABORT;` |
|       - | 2705 | `	}` |
|     365 | 2706 | `	return PH7_EXCEPTION;` |
|     187 | 2707 | `}` |
|       - | 2708 | `/*` |
|       - | 2709 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|       - | 2710 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|       - | 2711 | ` * positional and the named-argument binding paths of OP_CALL.` |
|       - | 2712 | ` *` |
|       - | 2713 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|       - | 2714 | ` * element uses its overall call position; a NAMED element always reports` |
|       - | 2715 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|       - | 2716 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|       - | 2717 | ` * formal, so no single parameter name applies.` |
|       - | 2718 | ` *` |
|       - | 2719 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|       - | 2720 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|       - | 2721 | ` */` |
|    2310 | 2722 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|       - | 2723 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|       5 | 2724 | `{` |
|       - | 2725 | `	sxi32 rc;` |
|    2315 | 2726 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|      33 | 2727 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|      37 | 2728 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|       - | 2729 | `			const char *zGiven;` |
|      11 | 2730 | `			const char *zExpected = "union";` |
|       - | 2731 | `			char zBuf[128];` |
|       - | 2732 | `			char zTypeBuf[128];` |
|      11 | 2733 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 2734 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      10 | 2735 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2736 | `				zGiven = "null";` |
|     ! 0 | 2737 | `			}else{` |
|       9 | 2738 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 2739 | `			}` |
|      11 | 2740 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|      15 | 2741 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|       4 | 2742 | `					zTypeBuf,sizeof(zTypeBuf));` |
|       4 | 2743 | `			}` |
|      11 | 2744 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|      11 | 2745 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2746 | `		}` |
|      17 | 2747 | `		return SXRET_OK;` |
|       - | 2748 | `	}` |
|    2288 | 2749 | `	if( pFormal->nType < 1` |
|    1256 | 2750 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|    2091 | 2751 | `		return SXRET_OK;` |
|       - | 2752 | `	}` |
|     207 | 2753 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2754 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|       - | 2755 | `		 * per element exactly like the non-variadic paths. */` |
|      61 | 2756 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2757 | `		ph7_class *pClass;` |
|      61 | 2758 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      61 | 2759 | `		if( rcPseudo == 0 ){` |
|       - | 2760 | `			/* Recognised pseudo-type; value mismatches */` |
|       - | 2761 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      14 | 2762 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       6 | 2763 | `				VmClassHintTypeName(pName,0,` |
|       6 | 2764 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       3 | 2765 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       8 | 2766 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2767 | `		}` |
|       - | 2768 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|       - | 2769 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|       - | 2770 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|       - | 2771 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|       - | 2772 | `		 * so ANY non-object is a TypeError, matching php. */` |
|      55 | 2773 | `		pClass = 0;` |
|      55 | 2774 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 2775 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      42 | 2776 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      20 | 2777 | `				VmClassHintTypeName(pName,pClass,` |
|      20 | 2778 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 2779 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      22 | 2780 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2781 | `		}` |
|      34 | 2782 | `		return SXRET_OK;` |
|       - | 2783 | `	}` |
|     149 | 2784 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|      63 | 2785 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|       - | 2786 | `			char zGivenBuf[128];` |
|       8 | 2787 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       2 | 2788 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       6 | 2789 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2790 | `		}` |
|      59 | 2791 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|       - | 2792 | `			char zTypeBuf[128];` |
|       - | 2793 | `			char zGivenBuf[128];` |
|      60 | 2794 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      19 | 2795 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      19 | 2796 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      41 | 2797 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2798 | `		}` |
|      11 | 2799 | `	}else{` |
|       - | 2800 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|       - | 2801 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      89 | 2802 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2803 | `	}` |
|     107 | 2804 | `	return SXRET_OK;` |
|    1160 | 2805 | `}` |
|       - | 2806 | `/*` |
|       - | 2807 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|       - | 2808 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|       - | 2809 | ` * before a required parameter as implicitly required), excluding a trailing` |
|       - | 2810 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|       - | 2811 | ` * pick php's wording — "exactly N expected" when required == total,` |
|       - | 2812 | ` * "at least N" when trailing optionals exist.` |
|       - | 2813 | ` */` |
| 1460192 | 2814 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|       5 | 2815 | `{` |
| 1460197 | 2816 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
| 1460197 | 2817 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
| 1460197 | 2818 | `	sxu32 nRequired = 0;` |
|       - | 2819 | `	sxu32 n;` |
| 1460197 | 2820 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     589 | 2821 | `		nFormal--;` |
|     292 | 2822 | `	}` |
| 5840009 | 2823 | `	for( n = 0 ; n < nFormal ; ++n ){` |
| 4379817 | 2824 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|   12807 | 2825 | `			nRequired = n + 1;` |
|    6401 | 2826 | `		}` |
| 2189911 | 2827 | `	}` |
| 1460197 | 2828 | `	*pnNonVariadic = nFormal;` |
| 1460197 | 2829 | `	return nRequired;` |
|       5 | 2830 | `}` |
|       - | 2831 | `/*` |
|       - | 2832 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|       - | 2833 | ` * with too few arguments:` |
|       - | 2834 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|       - | 2835 | ` *   {exactly\|at least} M expected` |
|       - | 2836 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|       - | 2837 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|       - | 2838 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|       - | 2839 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|       - | 2840 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|       - | 2841 | ` */` |
|      26 | 2842 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2843 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|       3 | 2844 | `{` |
|       - | 2845 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|       - | 2846 | `	SyBlob sMsg;` |
|      29 | 2847 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      29 | 2848 | `	if( pOwnerClass ){` |
|       5 | 2849 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|       2 | 2850 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|       3 | 2851 | `	}else{` |
|      25 | 2852 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|       - | 2853 | `	}` |
|      29 | 2854 | `	if( bCallSite ){` |
|      27 | 2855 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|      27 | 2856 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|      12 | 2857 | `	}` |
|      29 | 2858 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|      13 | 2859 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|       - | 2860 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      29 | 2861 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       3 | 2862 | `}` |
|       - | 2863 | `/*` |
|       - | 2864 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|       - | 2865 | ` * called with too few arguments, in php's ZPP wording:` |
|       - | 2866 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|       - | 2867 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|       - | 2868 | ` * pluralized on the expected count).` |
|       - | 2869 | ` *` |
|       - | 2870 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|       - | 2871 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|       - | 2872 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|       - | 2873 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|       - | 2874 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|       - | 2875 | ` */` |
|      66 | 2876 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2877 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|       2 | 2878 | `{` |
|       - | 2879 | `	SyBlob sMsg;` |
|      68 | 2880 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|      68 | 2881 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|      68 | 2882 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      68 | 2883 | `	if( pOwnerClass ){` |
|       3 | 2884 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|       1 | 2885 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       2 | 2886 | `	}else{` |
|      66 | 2887 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      32 | 2888 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       - | 2889 | `	}` |
|       - | 2890 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      68 | 2891 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       2 | 2892 | `}` |
|       - | 2893 | `/*` |
|       - | 2894 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|       - | 2895 | ` * MANY arguments, in php's ZPP wording:` |
|       - | 2896 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|       - | 2897 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|       - | 2898 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|       - | 2899 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|       - | 2900 | ` * so the caller must not route such a callee here.` |
|       - | 2901 | ` */` |
|      36 | 2902 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2903 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|       1 | 2904 | `{` |
|       - | 2905 | `	SyBlob sMsg;` |
|      37 | 2906 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|      37 | 2907 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|      37 | 2908 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      37 | 2909 | `	if( pOwnerClass ){` |
|     ! 0 | 2910 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 2911 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|     ! 0 | 2912 | `	}else{` |
|      37 | 2913 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|      18 | 2914 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|       - | 2915 | `	}` |
|       - | 2916 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      37 | 2917 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 2918 | `}` |
|       - | 2919 | `/*` |
|       - | 2920 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|       - | 2921 | ` * named or positional argument resolved to:` |
|       - | 2922 | ` *   C::f(): Argument #N ($x) not passed` |
|       - | 2923 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|       - | 2924 | ` */` |
|       2 | 2925 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2926 | `	sxu32 nArg,SyString *pArgName)` |
|       1 | 2927 | `{` |
|       - | 2928 | `	SyBlob sMsg;` |
|       3 | 2929 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 2930 | `	if( pOwnerClass ){` |
|     ! 0 | 2931 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|     ! 0 | 2932 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|     ! 0 | 2933 | `	}else{` |
|       3 | 2934 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|       - | 2935 | `	}` |
|       - | 2936 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       3 | 2937 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 2938 | `}` |
|       - | 2939 | `/*` |
|       - | 2940 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|       - | 2941 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|       - | 2942 | ` */` |
|       - | 2943 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|       - | 2944 | ` * The message is copied into the instance by __construct, so the caller owns` |
|       - | 2945 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|     144 | 2946 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|       5 | 2947 | `{` |
|       - | 2948 | `	ph7_class *pClass;` |
|       - | 2949 | `	ph7_class_instance *pThis;` |
|       - | 2950 | `	ph7_class_method *pCons;` |
|       - | 2951 | `	ph7_value sArg;` |
|       - | 2952 | `	ph7_value *apArg[1];` |
|       - | 2953 | `	SyString sMsgStr;` |
|       - | 2954 | `	VmFrame *pFrame;` |
|       - | 2955 | `	sxi32 rc;` |
|     149 | 2956 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     149 | 2957 | `	if( pClass == 0 ){` |
|     ! 0 | 2958 | `		return PH7_ABORT;` |
|       - | 2959 | `	}` |
|     149 | 2960 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     149 | 2961 | `	if( pThis == 0 ){` |
|     ! 0 | 2962 | `		return PH7_ABORT;` |
|       - | 2963 | `	}` |
|     149 | 2964 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     149 | 2965 | `	if( pCons ){` |
|     149 | 2966 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|     149 | 2967 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     149 | 2968 | `		apArg[0] = &sArg;` |
|     149 | 2969 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     149 | 2970 | `		PH7_MemObjRelease(&sArg);` |
|      72 | 2971 | `	}` |
|     149 | 2972 | `	pFrame = pVm->pFrame;` |
|     149 | 2973 | `	if( pFrame ){` |
|     149 | 2974 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     149 | 2975 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      72 | 2976 | `	}` |
|     149 | 2977 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     149 | 2978 | `	PH7_ClassInstanceUnref(pThis);` |
|     149 | 2979 | `	if( rc == SXERR_ABORT ){` |
|       6 | 2980 | `		return PH7_ABORT;` |
|       - | 2981 | `	}` |
|     145 | 2982 | `	return PH7_EXCEPTION;` |
|      77 | 2983 | `}` |
|       - | 2984 | `/*` |
|       - | 2985 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|       - | 2986 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|       - | 2987 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|       - | 2988 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|       - | 2989 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|       - | 2990 | ` * other callee falls through to the ordinary Class::method rendering.` |
|       - | 2991 | ` */` |
|       - | 2992 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|  625040 | 2993 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|       5 | 2994 | `{` |
|  625045 | 2995 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|  625045 | 2996 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|  624877 | 2997 | `		return 0;` |
|       - | 2998 | `	}` |
|     173 | 2999 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|     138 | 3000 | `		return 0;` |
|       - | 3001 | `	}` |
|      38 | 3002 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|      25 | 3003 | `		*pzKind = "get";` |
|      26 | 3004 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|      15 | 3005 | `		*pzKind = "set";` |
|       9 | 3006 | `	}else{` |
|     ! 0 | 3007 | `		return 0;` |
|       - | 3008 | `	}` |
|      38 | 3009 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|      38 | 3010 | `	return 1;` |
|  312525 | 3011 | `}` |
|     128 | 3012 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3013 | `{` |
|       - | 3014 | `	SyString sProp;` |
|       - | 3015 | `	const char *zKind;` |
|     133 | 3016 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|     123 | 3017 | `		return 0;` |
|       - | 3018 | `	}` |
|      13 | 3019 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|      13 | 3020 | `	return 1;` |
|      69 | 3021 | `}` |
|       - | 3022 | `/*` |
|       - | 3023 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|       - | 3024 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|       - | 3025 | ` * anything else uses its display name (which is also what strips a closure's` |
|       - | 3026 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|       - | 3027 | ` * instead — they are thrown from call sites that already resolved it.` |
|       - | 3028 | ` */` |
|     144 | 3029 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|       5 | 3030 | `{` |
|     149 | 3031 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      95 | 3032 | `		if( PH7_VmHookFuncName((ph7_class *)pFunc->pUserData,pFunc,pOut) ){` |
|       7 | 3033 | `			return;` |
|       - | 3034 | `		}` |
|      89 | 3035 | `		SyBlobFormat(pOut,"%z::%z",&((ph7_class *)pFunc->pUserData)->sName,&pFunc->sName);` |
|      89 | 3036 | `		return;` |
|       - | 3037 | `	}` |
|       - | 3038 | `	{` |
|      57 | 3039 | `		const char *zShow = 0;` |
|      57 | 3040 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|      57 | 3041 | `		if( zShow && nShow > 0 ){` |
|      57 | 3042 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|      26 | 3043 | `		}` |
|       - | 3044 | `	}` |
|      77 | 3045 | `}` |
|     140 | 3046 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|       5 | 3047 | `{` |
|       - | 3048 | `	SyBlob sMsg,sName;` |
|       - | 3049 | `	sxi32 rc;` |
|     145 | 3050 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     145 | 3051 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|     145 | 3052 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|     145 | 3053 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|     140 | 3054 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|     145 | 3055 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|     145 | 3056 | `	SyBlobRelease(&sName);` |
|     145 | 3057 | `	SyBlobRelease(&sMsg);` |
|     145 | 3058 | `	return rc;` |
|       5 | 3059 | `}` |
|       - | 3060 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|       - | 3061 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|       - | 3062 | ` * php calls it a "method" when it is one. */` |
|       4 | 3063 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|       2 | 3064 | `{` |
|       - | 3065 | `	SyBlob sMsg,sName;` |
|       - | 3066 | `	sxi32 rc;` |
|       6 | 3067 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       6 | 3068 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|       6 | 3069 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|       6 | 3070 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|       4 | 3071 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|       4 | 3072 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|       6 | 3073 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|       6 | 3074 | `	SyBlobRelease(&sName);` |
|       6 | 3075 | `	SyBlobRelease(&sMsg);` |
|       6 | 3076 | `	return rc;` |
|       2 | 3077 | `}` |
|       - | 3078 | `/*` |
|       - | 3079 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|       - | 3080 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|       - | 3081 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|       - | 3082 | ` */` |
|     572 | 3083 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|       5 | 3084 | `{` |
|     577 | 3085 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      63 | 3086 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 3087 | `	}` |
|     519 | 3088 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      83 | 3089 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      83 | 3090 | `		if( pThis && pThis->pClass ){` |
|      83 | 3091 | `			SyString *pName = &pThis->pClass->sName;` |
|      83 | 3092 | `			sxu32 n = pName->nByte;` |
|      83 | 3093 | `			if( n >= nBuf ){` |
|     ! 0 | 3094 | `				n = nBuf - 1;` |
|     ! 0 | 3095 | `			}` |
|      83 | 3096 | `			SyMemcpy(pName->zString,zBuf,n);` |
|      83 | 3097 | `			zBuf[n] = 0;` |
|      83 | 3098 | `			return zBuf;` |
|       - | 3099 | `		}` |
|     ! 0 | 3100 | `		return "object";` |
|       - | 3101 | `	}` |
|     441 | 3102 | `	return ph7_type_name(pVal);` |
|     291 | 3103 | `}` |
|       - | 3104 | `/*` |
|       - | 3105 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|       - | 3106 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|       - | 3107 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|       - | 3108 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|       - | 3109 | ` */` |
|      18 | 3110 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|       3 | 3111 | `{` |
|       - | 3112 | `	ph7_class *pClass;` |
|       - | 3113 | `	ph7_class_instance *pThis;` |
|       - | 3114 | `	ph7_class_method *pCons;` |
|       - | 3115 | `	ph7_value sArg;` |
|       - | 3116 | `	ph7_value *apArg[1];` |
|       - | 3117 | `	SyBlob sMsg;` |
|       - | 3118 | `	SyString sMsgStr;` |
|       - | 3119 | `	VmFrame *pFrame;` |
|       - | 3120 | `	sxi32 rc;` |
|      21 | 3121 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|       - | 3122 | `	char zNameBuf[64];` |
|      21 | 3123 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|      21 | 3124 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|      21 | 3125 | `	if( pClass == 0 ){` |
|     ! 0 | 3126 | `		return PH7_ABORT;` |
|       - | 3127 | `	}` |
|      21 | 3128 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      21 | 3129 | `	if( pThis == 0 ){` |
|     ! 0 | 3130 | `		return PH7_ABORT;` |
|       - | 3131 | `	}` |
|      21 | 3132 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      21 | 3133 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|      21 | 3134 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      21 | 3135 | `	if( pCons ){` |
|      21 | 3136 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      21 | 3137 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      21 | 3138 | `		apArg[0] = &sArg;` |
|      21 | 3139 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      21 | 3140 | `		PH7_MemObjRelease(&sArg);` |
|       9 | 3141 | `	}` |
|      21 | 3142 | `	SyBlobRelease(&sMsg);` |
|      21 | 3143 | `	pFrame = pVm->pFrame;` |
|      21 | 3144 | `	if( pFrame ){` |
|      21 | 3145 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      21 | 3146 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       9 | 3147 | `	}` |
|      21 | 3148 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      21 | 3149 | `	PH7_ClassInstanceUnref(pThis);` |
|      21 | 3150 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3151 | `		return PH7_ABORT;` |
|       - | 3152 | `	}` |
|      21 | 3153 | `	return PH7_EXCEPTION;` |
|      12 | 3154 | `}` |
|       - | 3155 | `/*` |
|       - | 3156 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|       - | 3157 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|       - | 3158 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|       - | 3159 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|       - | 3160 | ` */` |
|       - | 3161 | `/*` |
|       - | 3162 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|       - | 3163 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|       - | 3164 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|       - | 3165 | ` * type field.` |
|       - | 3166 | ` */` |
| 2337112 | 3167 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|       5 | 3168 | `{` |
| 2337117 | 3169 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|       5 | 3170 | `}` |
|   10846 | 3171 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|       5 | 3172 | `{` |
|   10851 | 3173 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|   10851 | 3174 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|       - | 3175 | `	const char *zGiven;` |
|       - | 3176 | `	ph7_class *pHintScope;` |
|       - | 3177 | `	char zBuf[128];` |
|       - | 3178 | `	char zTypeBuf[128];` |
|       - | 3179 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|   10851 | 3180 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|     ! 0 | 3181 | `		return SXRET_OK;` |
|       - | 3182 | `	}` |
|       - | 3183 | `	/* never return type: the function must not return at all. An explicit` |
|       - | 3184 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|       - | 3185 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|       - | 3186 | `	 * the call site). */` |
|   10851 | 3187 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       6 | 3188 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|       - | 3189 | `	}` |
|       - | 3190 | `	/* void return type: the function must not produce a value. */` |
|   10847 | 3191 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     997 | 3192 | `		if( pValue == 0 ){` |
|     993 | 3193 | `			return SXRET_OK;` |
|       - | 3194 | `		}` |
|       - | 3195 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|       - | 3196 | `		 * store, not a return: php compiles no return statement there at all,` |
|       - | 3197 | `		 * and still reports the hook's return type as void. PHL carries the` |
|       - | 3198 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|       - | 3199 | `		 * so the one implicit value this arm must not reject is that one. */` |
|       6 | 3200 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|       6 | 3201 | `			return SXRET_OK;` |
|       - | 3202 | `		}` |
|       - | 3203 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|       - | 3204 | `		 * still counts as "returned a value" here. */` |
|     ! 0 | 3205 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|     ! 0 | 3206 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|       - | 3207 | `	}` |
|       - | 3208 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|       - | 3209 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|       - | 3210 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    9855 | 3211 | `	if( pValue == 0 ){` |
|      32 | 3212 | `		const char *zExpected = "value";` |
|      32 | 3213 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      47 | 3214 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|      15 | 3215 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|      15 | 3216 | `		}` |
|       - | 3217 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|       - | 3218 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|      32 | 3219 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|       - | 3220 | `	}` |
|       - | 3221 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|       - | 3222 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|       - | 3223 | `	 * matching how every other typed return reports a missing value.) */` |
|    9825 | 3224 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|       5 | 3225 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 3226 | `			return SXRET_OK;` |
|       - | 3227 | `		}` |
|       4 | 3228 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|       1 | 3229 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3230 | `	}` |
|       - | 3231 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|       - | 3232 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|       - | 3233 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    9821 | 3234 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|      37 | 3235 | `		return SXRET_OK;` |
|       - | 3236 | `	}` |
|       - | 3237 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|       - | 3238 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|       - | 3239 | `	 * Check by value before the real-class instanceof branch below. */` |
|    9789 | 3240 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     185 | 3241 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|     185 | 3242 | `		if( rcPseudo == 1 ){` |
|      88 | 3243 | `			return SXRET_OK;` |
|       - | 3244 | `		}` |
|     101 | 3245 | `		if( rcPseudo == 0 ){` |
|      14 | 3246 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       6 | 3247 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|       3 | 3248 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3249 | `		}` |
|       - | 3250 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      45 | 3251 | `	}` |
|       - | 3252 | `	/* The two branches below are the only ones that can name a class, so the` |
|       - | 3253 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|       - | 3254 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|       - | 3255 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|       - | 3256 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|       - | 3257 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|       - | 3258 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    9699 | 3259 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|       - | 3260 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|       - | 3261 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|       - | 3262 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    9699 | 3263 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       - | 3264 | `		sxi32 rcU;` |
|      40 | 3265 | `		const char *zExpected = "union";` |
|      40 | 3266 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|      40 | 3267 | `		if( rcU == SXRET_OK ){` |
|      30 | 3268 | `			return SXRET_OK;` |
|       - | 3269 | `		}` |
|      11 | 3270 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       9 | 3271 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       7 | 3272 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 3273 | `			zGiven = "null";` |
|     ! 0 | 3274 | `		}else{` |
|       3 | 3275 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3276 | `		}` |
|      11 | 3277 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      16 | 3278 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|       5 | 3279 | `				zTypeBuf,sizeof(zTypeBuf));` |
|       5 | 3280 | `		}` |
|      11 | 3281 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|       - | 3282 | `	}` |
|       - | 3283 | `	/* Class return type — instanceof check. The class name is a length-` |
|       - | 3284 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|       - | 3285 | `	 * it into the TypeError message. */` |
|    9663 | 3286 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      95 | 3287 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      95 | 3288 | `		ph7_class *pExpected = 0;` |
|      95 | 3289 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|      35 | 3290 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      28 | 3291 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      16 | 3292 | `			}else{` |
|       8 | 3293 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 3294 | `			}` |
|      50 | 3295 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      15 | 3296 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|       - | 3297 | `		}` |
|      65 | 3298 | `		return SXRET_OK;` |
|       - | 3299 | `	}` |
|       - | 3300 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|       - | 3301 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|       - | 3302 | `	 * non-nullable scalar return — a TypeError. */` |
|    9573 | 3303 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      25 | 3304 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       8 | 3305 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3306 | `			"null");` |
|       - | 3307 | `	}` |
|       - | 3308 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|       - | 3309 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    9557 | 3310 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    9435 | 3311 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    9435 | 3312 | `		return SXRET_OK;` |
|       - | 3313 | `	}` |
|       - | 3314 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|       - | 3315 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|       - | 3316 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|       - | 3317 | `	 * still rejects the object under strict_types. */` |
|     127 | 3318 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      22 | 3319 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      31 | 3320 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|      18 | 3321 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      20 | 3322 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      29 | 3323 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       9 | 3324 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       9 | 3325 | `				zGiven);` |
|       - | 3326 | `		}` |
|       1 | 3327 | `	}` |
|       - | 3328 | `	/* Array <-> scalar is never compatible. */` |
|     109 | 3329 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      33 | 3330 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|      10 | 3331 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      10 | 3332 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 3333 | `	}` |
|       - | 3334 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|       - | 3335 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|       - | 3336 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|       - | 3337 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|      84 | 3338 | `	if( !bStrict` |
|      83 | 3339 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|      49 | 3340 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|      54 | 3341 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|      11 | 3342 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       3 | 3343 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 3344 | `			"string");` |
|       - | 3345 | `	}` |
|      83 | 3346 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|      80 | 3347 | `		return SXRET_OK;` |
|       - | 3348 | `	}` |
|       4 | 3349 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       1 | 3350 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       1 | 3351 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    5428 | 3352 | `}` |
|       - | 3353 | `/*` |
|       - | 3354 | ` * Report a fatal named-argument error.` |
|       - | 3355 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|       - | 3356 | ` */` |
|      12 | 3357 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       3 | 3358 | `{` |
|       - | 3359 | `	SyBlob sMsg;` |
|       - | 3360 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|       - | 3361 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|       - | 3362 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|       - | 3363 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|       - | 3364 | `	 * unconditional fatal even inside try/catch. */` |
|      15 | 3365 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 3366 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|      15 | 3367 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 3368 | `}` |
|       - | 3369 | `/*` |
|       - | 3370 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 3371 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 3372 | ` * information.` |
|       - | 3373 | ` * ------------------------------------` |
|       - | 3374 | ` * Simple boring wrapper function.` |
|       - | 3375 | ` * ------------------------------------` |
|       - | 3376 | ` */` |
|   22236 | 3377 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|       5 | 3378 | `{` |
|       - | 3379 | `	sxi32 rc;` |
|   22241 | 3380 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|   22241 | 3381 | `	return rc;` |
|       5 | 3382 | `}` |
|       - | 3383 | `/*` |
|       - | 3384 | ` * Resolve function context from the current frame.` |
|       - | 3385 | ` */` |
|       - | 3386 | `/*` |
|       - | 3387 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|       - | 3388 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|       - | 3389 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|       - | 3390 | ` * straight at the function's own name otherwise.` |
|       - | 3391 | ` */` |
|  624874 | 3392 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|       5 | 3393 | `{` |
|  624879 | 3394 | `	const char *zName = pFunc->sName.zString;` |
|  624879 | 3395 | `	int nName = (int)pFunc->sName.nByte;` |
|  675298 | 3396 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|  676101 | 3397 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|       - | 3398 | `	/* A property hook is not a method in php and never shows the name PHL` |
|       - | 3399 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|       - | 3400 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|       - | 3401 | `	 * report from inside one, and what makes the trace line read` |
|       - | 3402 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|       - | 3403 | ``	 * `C::$p::get` for free. */`` |
|       - | 3404 | `	{` |
|       - | 3405 | `		SyString sProp;` |
|       - | 3406 | `		const char *zKind;` |
|  624879 | 3407 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      31 | 3408 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       9 | 3409 | `				"$%z::%s",&sProp,zKind);` |
|      22 | 3410 | `			*pzOut = pVm->zDisplayName;` |
|      22 | 3411 | `			return n;` |
|       - | 3412 | `		}` |
|       - | 3413 | `	}` |
|  624861 | 3414 | `	if( bClosure ){` |
|       - | 3415 | `		int n;` |
|    1621 | 3416 | `		if( pFunc->sFile.nByte > 0 ){` |
|    2429 | 3417 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|    1616 | 3418 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|     813 | 3419 | `		}else{` |
|     ! 0 | 3420 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|       - | 3421 | `		}` |
|    1621 | 3422 | `		*pzOut = pVm->zDisplayName;` |
|    1621 | 3423 | `		return n;` |
|       - | 3424 | `	}` |
|  623245 | 3425 | `	*pzOut = zName;` |
|  623245 | 3426 | `	return nName;` |
|  312442 | 3427 | `}` |
|    1130 | 3428 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|       4 | 3429 | `{` |
|       - | 3430 | `	VmFrame *pFrame;` |
|       - | 3431 | `	ph7_vm_func *pFunc;` |
|    1134 | 3432 | `	*pzFuncName = 0;` |
|    1134 | 3433 | `	*pnFuncLen = 0;` |
|    1134 | 3434 | `	pFrame = pVm->pFrame;` |
|    1134 | 3435 | `	if( pFrame == 0 ){` |
|     ! 0 | 3436 | `		return;` |
|       - | 3437 | `	}` |
|    1134 | 3438 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    1134 | 3439 | `	if( pFrame->pParent == 0 ){` |
|    1100 | 3440 | `		return;` |
|       - | 3441 | `	}` |
|      38 | 3442 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      38 | 3443 | `	if( pFunc == 0 ){` |
|     ! 0 | 3444 | `		return;` |
|       - | 3445 | `	}` |
|      38 | 3446 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|     569 | 3447 | `}` |
|       - | 3448 | `/*` |
|       - | 3449 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|       - | 3450 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|       - | 3451 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|       - | 3452 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|       - | 3453 | ` * built-in library and already produces php's exact byte format, which keeps` |
|       - | 3454 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|       - | 3455 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|       - | 3456 | ` * synthesize what it can.` |
|       - | 3457 | ` */` |
|     586 | 3458 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3459 | `{` |
|       - | 3460 | `	ph7_class_method *pGetTrace;` |
|       - | 3461 | `	ph7_value sTrace;` |
|       - | 3462 | `	const char *zTmp;` |
|       - | 3463 | `	int nTmp;` |
|     590 | 3464 | `	int bDone = 0;` |
|       - | 3465 | `	int bSaved;` |
|     590 | 3466 | `	if( pThis == 0 ){` |
|       5 | 3467 | `		return 0;` |
|       - | 3468 | `	}` |
|     586 | 3469 | `	if( pVm->bRenderingUncaught ){` |
|       - | 3470 | `		/* Already inside a report: do not run userland trace code again. */` |
|     ! 0 | 3471 | `		return 0;` |
|       - | 3472 | `	}` |
|     586 | 3473 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|     586 | 3474 | `	if( pGetTrace == 0 ){` |
|     ! 0 | 3475 | `		return 0;` |
|       - | 3476 | `	}` |
|     586 | 3477 | `	PH7_MemObjInit(pVm,&sTrace);` |
|       - | 3478 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|       - | 3479 | `	 * anything it calls) throws, the throw would be reported by this very` |
|       - | 3480 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|       - | 3481 | `	 * trace rather than re-entering here forever. */` |
|     586 | 3482 | `	bSaved = pVm->bRenderingUncaught;` |
|     586 | 3483 | `	pVm->bRenderingUncaught = 1;` |
|     586 | 3484 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|     586 | 3485 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|     586 | 3486 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 3487 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     586 | 3488 | `			bDone = 1;` |
|     291 | 3489 | `		}` |
|     291 | 3490 | `	}` |
|     586 | 3491 | `	PH7_MemObjRelease(&sTrace);` |
|     586 | 3492 | `	pVm->bRenderingUncaught = bSaved;` |
|     586 | 3493 | `	return bDone;` |
|     297 | 3494 | `}` |
|       - | 3495 | `/*` |
|       - | 3496 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|       - | 3497 | ` *` |
|       - | 3498 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|       - | 3499 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|       - | 3500 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|       - | 3501 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|       - | 3502 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|       - | 3503 | ` *             trailer.` |
|       - | 3504 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|       - | 3505 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|       - | 3506 | ` * call; this routine only appends.` |
|       - | 3507 | ` */` |
|     586 | 3508 | `static void VmRenderUncaughtEntry(` |
|       - | 3509 | `	ph7_vm *pVm,SyBlob *pOut,` |
|       - | 3510 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|       - | 3511 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|       - | 3512 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|       - | 3513 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|       - | 3514 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|       4 | 3515 | `{` |
|       - | 3516 | `	SyString *pFile;` |
|     590 | 3517 | `	if( nThrowLine == 0 ){` |
|       5 | 3518 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|       2 | 3519 | `	}` |
|     590 | 3520 | `	if( nCallLine == 0 ){` |
|     590 | 3521 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     590 | 3522 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|     293 | 3523 | `	}` |
|     590 | 3524 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|     ! 0 | 3525 | `		zClass = "Exception";` |
|     ! 0 | 3526 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|     ! 0 | 3527 | `	}` |
|     590 | 3528 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     558 | 3529 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     277 | 3530 | `	}` |
|     590 | 3531 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     590 | 3532 | `	if( bFirst ){` |
|     584 | 3533 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|     294 | 3534 | `	}else{` |
|       8 | 3535 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       - | 3536 | `	}` |
|     590 | 3537 | `	SyBlobAppend(pOut,zClass,nClass);` |
|     590 | 3538 | `	if( zMsg && nMsg > 0 ){` |
|     590 | 3539 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|     590 | 3540 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|     293 | 3541 | `	}` |
|     590 | 3542 | `	if( pFile ){` |
|     590 | 3543 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     293 | 3544 | `	}` |
|     590 | 3545 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       - | 3546 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|       - | 3547 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|       - | 3548 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|       - | 3549 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|       - | 3550 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|       - | 3551 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|       - | 3552 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|     590 | 3553 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|       5 | 3554 | `		int bFrame = 0;` |
|       5 | 3555 | `		if( zFuncName && nFuncLen > 0 ){` |
|       3 | 3556 | `			if( pFile ){` |
|       - | 3557 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|       - | 3558 | `				 * running inside it. */` |
|       4 | 3559 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|       2 | 3560 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|       2 | 3561 | `			}else{` |
|     ! 0 | 3562 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|       - | 3563 | `			}` |
|       3 | 3564 | `			bFrame = 1;` |
|       1 | 3565 | `		}` |
|       - | 3566 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|       7 | 3567 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|       2 | 3568 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|       2 | 3569 | `	}` |
|     590 | 3570 | `	if( bLast && pFile ){` |
|     584 | 3571 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|     584 | 3572 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     290 | 3573 | `	}` |
|     590 | 3574 | `}` |
|       - | 3575 | `/*` |
|       - | 3576 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|       - | 3577 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|       - | 3578 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|       - | 3579 | ` */` |
|       4 | 3580 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|       1 | 3581 | `{` |
|       - | 3582 | `	SyBlob sOut;` |
|       - | 3583 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|       - | 3584 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|       5 | 3585 | `	pVm->iExitStatus = 255;` |
|       5 | 3586 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3587 | `		return PH7_OK;` |
|       - | 3588 | `	}` |
|       5 | 3589 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       5 | 3590 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|       5 | 3591 | `	VmCallErrorHandler(pVm,&sOut);` |
|       5 | 3592 | `	SyBlobRelease(&sOut);` |
|       5 | 3593 | `	return PH7_ABORT;` |
|       3 | 3594 | `}` |
|       - | 3595 | `/*` |
|       - | 3596 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|       - | 3597 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|       - | 3598 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|       - | 3599 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|       - | 3600 | ` */` |
|       - | 3601 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|     582 | 3602 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|       4 | 3603 | `{` |
|       - | 3604 | `	ph7_value *pValue;` |
|       - | 3605 | `	ph7_class_instance *pPrev;` |
|       - | 3606 | `	ph7_class *pThrowable;` |
|     586 | 3607 | `	if( pThis == 0 ){` |
|     ! 0 | 3608 | `		return 0;` |
|       - | 3609 | `	}` |
|     586 | 3610 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     586 | 3611 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     580 | 3612 | `		return 0;` |
|       - | 3613 | `	}` |
|       8 | 3614 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 3615 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|       - | 3616 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|       - | 3617 | `	 * never renders a stray object as an exception entry. */` |
|       8 | 3618 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       8 | 3619 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|     ! 0 | 3620 | `		return 0;` |
|       - | 3621 | `	}` |
|       8 | 3622 | `	return pPrev;` |
|     295 | 3623 | `}` |
|       - | 3624 | `/*` |
|       - | 3625 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|       - | 3626 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|       - | 3627 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|       - | 3628 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|       - | 3629 | ` */` |
|      16 | 3630 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|       2 | 3631 | `{` |
|       - | 3632 | `	ph7_value *pValue;` |
|      18 | 3633 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|     ! 0 | 3634 | `		return;` |
|       - | 3635 | `	}` |
|      18 | 3636 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      18 | 3637 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       3 | 3638 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|       - | 3639 | `	}` |
|      16 | 3640 | `	pPrev->iRef++;` |
|       - | 3641 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|       - | 3642 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|      16 | 3643 | `	PH7_MemObjRelease(pValue);` |
|      16 | 3644 | `	pValue->x.pOther = pPrev;` |
|      16 | 3645 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|      10 | 3646 | `}` |
|       - | 3647 | `/*` |
|       - | 3648 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|       - | 3649 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|       - | 3650 | ` * absent or yields an empty string.` |
|       - | 3651 | ` */` |
|       - | 3652 | `/*` |
|       - | 3653 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|       - | 3654 | ` * 0 when the class exposes no getLine().` |
|       - | 3655 | ` */` |
|     582 | 3656 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       4 | 3657 | `{` |
|       - | 3658 | `	ph7_class_method *pGetLine;` |
|       - | 3659 | `	ph7_value sLine;` |
|     586 | 3660 | `	sxu32 nLine = 0;` |
|     586 | 3661 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|     586 | 3662 | `	if( pGetLine == 0 ){` |
|     ! 0 | 3663 | `		return 0;` |
|       - | 3664 | `	}` |
|     586 | 3665 | `	PH7_MemObjInit(pVm,&sLine);` |
|     586 | 3666 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|     586 | 3667 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|     586 | 3668 | `		if( n > 0 ){` |
|     586 | 3669 | `			nLine = (sxu32)n;` |
|     291 | 3670 | `		}` |
|     291 | 3671 | `	}` |
|     586 | 3672 | `	PH7_MemObjRelease(&sLine);` |
|     586 | 3673 | `	return nLine;` |
|     295 | 3674 | `}` |
|     582 | 3675 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3676 | `{` |
|       - | 3677 | `	ph7_class_method *pGetMessage;` |
|       - | 3678 | `	ph7_value sMsg;` |
|       - | 3679 | `	const char *zTmp;` |
|       - | 3680 | `	int nTmp;` |
|     586 | 3681 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|     586 | 3682 | `	if( pGetMessage == 0 ){` |
|     ! 0 | 3683 | `		return;` |
|       - | 3684 | `	}` |
|     586 | 3685 | `	PH7_MemObjInit(pVm,&sMsg);` |
|     586 | 3686 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|     586 | 3687 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|     586 | 3688 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 3689 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     291 | 3690 | `		}` |
|     291 | 3691 | `	}` |
|     586 | 3692 | `	PH7_MemObjRelease(&sMsg);` |
|     295 | 3693 | `}` |
|       - | 3694 | `/*` |
|       - | 3695 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|       - | 3696 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|       - | 3697 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|       - | 3698 | ` * outermost (the actually-uncaught) exception.` |
|       - | 3699 | ` *` |
|       - | 3700 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|       - | 3701 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|       - | 3702 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|       - | 3703 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|       - | 3704 | ` */` |
|       - | 3705 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|     576 | 3706 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|       4 | 3707 | `{` |
|       - | 3708 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|     580 | 3709 | `	int nChain = 0;` |
|       - | 3710 | `	int i;` |
|       - | 3711 | `	SyBlob sOut;` |
|       - | 3712 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|       - | 3713 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|       - | 3714 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|     580 | 3715 | `	pVm->iExitStatus = 255;` |
|     580 | 3716 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3717 | `		return PH7_OK;` |
|       - | 3718 | `	}` |
|       - | 3719 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|       - | 3720 | `	 * collected) or the hard cap. */` |
|    1162 | 3721 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|     594 | 3722 | `		for( i = 0 ; i < nChain ; ++i ){` |
|      10 | 3723 | `			if( apChain[i] == pThis ){` |
|     ! 0 | 3724 | `				pThis = 0; /* cycle: stop the walk */` |
|     ! 0 | 3725 | `				break;` |
|       - | 3726 | `			}` |
|       6 | 3727 | `		}` |
|     586 | 3728 | `		if( pThis == 0 ){` |
|     ! 0 | 3729 | `			break;` |
|       - | 3730 | `		}` |
|     586 | 3731 | `		apChain[nChain++] = pThis;` |
|     586 | 3732 | `		pThis = VmExceptionGetPrevious(pThis);` |
|       4 | 3733 | `	}` |
|     580 | 3734 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       - | 3735 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|       - | 3736 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|    1162 | 3737 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|     586 | 3738 | `		ph7_class_instance *pEnt = apChain[i];` |
|       - | 3739 | `		SyBlob sMsg;` |
|     586 | 3740 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     586 | 3741 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|     877 | 3742 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|     582 | 3743 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|     582 | 3744 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|     291 | 3745 | `			zFuncName,nFuncLen,` |
|     582 | 3746 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|     291 | 3747 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|     291 | 3748 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|     586 | 3749 | `		SyBlobRelease(&sMsg);` |
|     295 | 3750 | `	}` |
|     580 | 3751 | `	VmCallErrorHandler(pVm,&sOut);` |
|     580 | 3752 | `	SyBlobRelease(&sOut);` |
|     580 | 3753 | `	return PH7_ABORT;` |
|     292 | 3754 | `}` |
|       - | 3755 | `/*` |
|       - | 3756 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|       - | 3757 | ` *` |
|       - | 3758 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|       - | 3759 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|       - | 3760 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|       - | 3761 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|       - | 3762 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|       - | 3763 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|       - | 3764 | ` */` |
| 1549618 | 3765 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|       5 | 3766 | `{` |
| 1549623 | 3767 | `	if( pVm->bCoalesceArmed ){` |
|       8 | 3768 | `		if( pVm->pCoalesceObj ){` |
|       8 | 3769 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|       3 | 3770 | `		}` |
|       8 | 3771 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       8 | 3772 | `		pVm->pCoalesceObj = 0;` |
|       8 | 3773 | `		pVm->bCoalesceArmed = 0;` |
|       3 | 3774 | `	}` |
| 1549623 | 3775 | `}` |
|       - | 3776 | `/*` |
|       - | 3777 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|       - | 3778 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|       - | 3779 | ` * is a literal, non-formatted string; callers that need formatting should` |
|       - | 3780 | ` * build the SyBlob themselves and pass its data + length.` |
|       - | 3781 | ` *` |
|       - | 3782 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|       - | 3783 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|       - | 3784 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|       - | 3785 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|       - | 3786 | ` */` |
|  140676 | 3787 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|       - | 3788 | `	ph7_vm *pVm,` |
|       - | 3789 | `	const char *zClass,` |
|       - | 3790 | `	const char *zMsg,` |
|       - | 3791 | `	sxu32 nMsg` |
|       5 | 3792 | `){` |
|       - | 3793 | `	ph7_class *pClass;` |
|       - | 3794 | `	ph7_class_instance *pThis;` |
|       - | 3795 | `	ph7_class_method *pCons;` |
|       - | 3796 | `	VmFrame *pFrame;` |
|       - | 3797 | `	sxi32 rc;` |
|  140681 | 3798 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|  140681 | 3799 | `	if( pClass == 0 ){` |
|     ! 0 | 3800 | `		return SXERR_ABORT;` |
|       - | 3801 | `	}` |
|  140681 | 3802 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|  140681 | 3803 | `	if( pThis == 0 ){` |
|     ! 0 | 3804 | `		return SXERR_ABORT;` |
|       - | 3805 | `	}` |
|  140681 | 3806 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|  140681 | 3807 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|       - | 3808 | `		ph7_value sArg;` |
|       - | 3809 | `		ph7_value *apArg[1];` |
|       - | 3810 | `		SyString sMsgStr;` |
|  140681 | 3811 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|  140681 | 3812 | `		PH7_MemObjInit(pVm,&sArg);` |
|  140681 | 3813 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  140681 | 3814 | `		apArg[0] = &sArg;` |
|  140681 | 3815 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|  140681 | 3816 | `		PH7_MemObjRelease(&sArg);` |
|  140681 | 3817 | `		pVm->nExcCtorDepth--;` |
|   70338 | 3818 | `	}` |
|  140681 | 3819 | `	pFrame = pVm->pFrame;` |
|  140681 | 3820 | `	if( pFrame ){` |
|  140681 | 3821 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  140681 | 3822 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   70338 | 3823 | `	}` |
|  140681 | 3824 | `	rc = VmThrowException(pVm,pThis);` |
|  140681 | 3825 | `	PH7_ClassInstanceUnref(pThis);` |
|  140681 | 3826 | `	return rc;` |
|   70343 | 3827 | `}` |
|       - | 3828 | `/*` |
|       - | 3829 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|       - | 3830 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|       - | 3831 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|       - | 3832 | ` *` |
|       - | 3833 | ` *   int/float/bool/null      arithmetic proceeds` |
|       - | 3834 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|       - | 3835 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|       - | 3836 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|       - | 3837 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|       - | 3838 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|       - | 3839 | ` *   object/resource          TypeError, naming the object's CLASS` |
|       - | 3840 | ` *` |
|       - | 3841 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|       - | 3842 | ` */` |
|       - | 3843 | `/*` |
|       - | 3844 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|       - | 3845 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|       - | 3846 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|       - | 3847 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|       - | 3848 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|       - | 3849 | ` * to depth 1).` |
|       - | 3850 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|       - | 3851 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|       - | 3852 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|       - | 3853 | ` * frame shape is file/line/function[/class/type], matching the default` |
|       - | 3854 | ` * zend.exception_ignore_args=On.` |
|       - | 3855 | ` */` |
| 1449456 | 3856 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)` |
|       5 | 3857 | `{` |
|       - | 3858 | `	SyString *pFile;` |
|       - | 3859 | `	VmFrame *pFrame;` |
|       - | 3860 | `	ph7_value *pValue;` |
| 1449461 | 3861 | `	pValue = ph7_new_scalar(&(*pVm));` |
| 1449461 | 3862 | `	if( pValue == 0 ){` |
|     ! 0 | 3863 | `		return;` |
|       - | 3864 | `	}` |
| 1449461 | 3865 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1449461 | 3866 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 2073917 | 3867 | `	while( pFrame ){` |
| 2073917 | 3868 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 3869 | `		ph7_value *pEntry;` |
| 2073917 | 3870 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|       - | 3871 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|  724733 | 3872 | `			break;` |
|       - | 3873 | `		}` |
|  624461 | 3874 | `		pEntry = ph7_new_array(&(*pVm));` |
|  624461 | 3875 | `		if( pEntry == 0 ){` |
|     ! 0 | 3876 | `			break;` |
|       - | 3877 | `		}` |
|       - | 3878 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|       - | 3879 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|       - | 3880 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|       - | 3881 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|       - | 3882 | `		 * include-stack top for a call made at global scope. */` |
|       - | 3883 | `		{` |
|  624461 | 3884 | `			SyString *pFrameFile = pFile;` |
|  624461 | 3885 | `			if( pFrame->pParent->pUserData ){` |
|     385 | 3886 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|     385 | 3887 | `				if( pCaller->sFile.nByte > 0 ){` |
|     329 | 3888 | `					pFrameFile = &pCaller->sFile;` |
|     162 | 3889 | `				}` |
|     190 | 3890 | `			}` |
|  624461 | 3891 | `			if( pFrameFile ){` |
|  624461 | 3892 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|  624461 | 3893 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|  624461 | 3894 | `				ph7_value_reset_string_cursor(pValue);` |
|  312228 | 3895 | `			}` |
|       - | 3896 | `		}` |
|  624461 | 3897 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|  624461 | 3898 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|       - | 3899 | `		{` |
|  624461 | 3900 | `			const char *zDisp = 0;` |
|  624461 | 3901 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|  624461 | 3902 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|       - | 3903 | `		}` |
|  624461 | 3904 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|  624461 | 3905 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 3906 | `		{` |
|       - | 3907 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|       - | 3908 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|       - | 3909 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|       - | 3910 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|       - | 3911 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|       - | 3912 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|       - | 3913 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|  624461 | 3914 | `			SyString *pClsName = 0;` |
|  624461 | 3915 | `			const char *zType = "->";` |
|  624461 | 3916 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|  500579 | 3917 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|  500579 | 3918 | `				zType = pFrame->pThis ? "->" : "::";` |
|  374174 | 3919 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|     ! 0 | 3920 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|     ! 0 | 3921 | `			}` |
|  624461 | 3922 | `			if( pClsName ){` |
|  500579 | 3923 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|  500579 | 3924 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|  500579 | 3925 | `				ph7_value_reset_string_cursor(pValue);` |
|  500579 | 3926 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|  500579 | 3927 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|  500579 | 3928 | `				ph7_value_reset_string_cursor(pValue);` |
|  500579 | 3929 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){` |
|      17 | 3930 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      17 | 3931 | `					if( pObjVal ){` |
|      17 | 3932 | `						pFrame->pThis->iRef++;` |
|      17 | 3933 | `						pObjVal->x.pOther = pFrame->pThis;` |
|      17 | 3934 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      17 | 3935 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      17 | 3936 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       7 | 3937 | `					}` |
|       7 | 3938 | `				}` |
|  250287 | 3939 | `			}` |
|       - | 3940 | `		}` |
|  624461 | 3941 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      17 | 3942 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      17 | 3943 | `			if( pArg ){` |
|      17 | 3944 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - | 3945 | `				sxu32 n;` |
|      31 | 3946 | `				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){` |
|      16 | 3947 | `					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|      16 | 3948 | `					if( pObj ){` |
|      16 | 3949 | `						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       7 | 3950 | `					}` |
|       9 | 3951 | `				}` |
|      17 | 3952 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      17 | 3953 | `				ph7_release_value(&(*pVm),pArg);` |
|       7 | 3954 | `			}` |
|       7 | 3955 | `		}` |
|  624461 | 3956 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|  624461 | 3957 | `		ph7_release_value(&(*pVm),pEntry);` |
|  624461 | 3958 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|       5 | 3959 | `	}` |
| 1449461 | 3960 | `	ph7_release_value(&(*pVm),pValue);` |
|  724733 | 3961 | `}` |
|       - | 3962 | `/*` |
|       - | 3963 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|       - | 3964 | ` *` |
|       - | 3965 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|       - | 3966 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|       - | 3967 | ` * calls parent::__construct still reports the right position. The embedded` |
|       - | 3968 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|       - | 3969 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|       - | 3970 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|       - | 3971 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|       - | 3972 | ` */` |
| 1559842 | 3973 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       5 | 3974 | `{` |
|       - | 3975 | `	static const char *azField[] = { "file", "line", "trace" };` |
|       - | 3976 | `	ph7_class *pThrowable;` |
|       - | 3977 | `	SyString *pFile;` |
|       - | 3978 | `	SyString *pSiteFile;` |
|       - | 3979 | `	sxu32 n;` |
| 1559847 | 3980 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|     ! 0 | 3981 | `		return;` |
|       - | 3982 | `	}` |
| 1559847 | 3983 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1559847 | 3984 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|  110407 | 3985 | `		return;` |
|       - | 3986 | `	}` |
| 1449445 | 3987 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1449445 | 3988 | `	pSiteFile = pFile;` |
|       - | 3989 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|       - | 3990 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|       - | 3991 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|       - | 3992 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|       - | 3993 | `	{` |
| 1449445 | 3994 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 1449445 | 3995 | `		if( pInner && pInner->pUserData ){` |
|  622615 | 3996 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|  622615 | 3997 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|  622163 | 3998 | `				pSiteFile = &pInnerFunc->sFile;` |
|  311079 | 3999 | `			}` |
|  311305 | 4000 | `		}` |
|       - | 4001 | `	}` |
| 5797765 | 4002 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|       - | 4003 | `		SyHashEntry *pEntry;` |
|       - | 4004 | `		VmClassAttr *pVmAttr;` |
|       - | 4005 | `		ph7_value *pAttrValue;` |
| 4348325 | 4006 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
| 4348325 | 4007 | `		if( pEntry == 0 ){` |
|     ! 0 | 4008 | `			continue;` |
|       - | 4009 | `		}` |
| 4348325 | 4010 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
| 4348325 | 4011 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 4348325 | 4012 | `		if( pAttrValue == 0 ){` |
|     ! 0 | 4013 | `			continue;` |
|       - | 4014 | `		}` |
| 4348325 | 4015 | `		if( n == 0 ){` |
| 1449445 | 4016 | `			if( pSiteFile ){` |
| 1449445 | 4017 | `				PH7_MemObjRelease(pAttrValue);` |
| 1449445 | 4018 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|  724725 | 4019 | `			}` |
| 3623605 | 4020 | `		}else if( n == 1 ){` |
|       - | 4021 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|       - | 4022 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|       - | 4023 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|       - | 4024 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|       - | 4025 | `			 * off everything the initializer calls — an autoloader, a nested` |
|       - | 4026 | `			 * constant's evaluation — which report their own lines in both engines.` |
|       - | 4027 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|       - | 4028 | `			 * PHL already matches.) */` |
| 1449446 | 4029 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|      28 | 4030 | `				? pVm->nLazyInitLine` |
| 1449441 | 4031 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
| 1449445 | 4032 | `			PH7_MemObjRelease(pAttrValue);` |
| 1449445 | 4033 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|  724725 | 4034 | `		}else{` |
|       - | 4035 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|       - | 4036 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|       - | 4037 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|       - | 4038 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
| 1449445 | 4039 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
| 1449445 | 4040 | `			if( pList == 0 ){` |
|     ! 0 | 4041 | `				continue;` |
|       - | 4042 | `			}` |
| 1449445 | 4043 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);` |
|       - | 4044 | `			/* Building the trace reserves new memobjs, which may realloc` |
|       - | 4045 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|       - | 4046 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|       - | 4047 | `			 * AFTER the walk before releasing/storing into it. */` |
| 1449445 | 4048 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 1449445 | 4049 | `			if( pAttrValue ){` |
| 1449445 | 4050 | `				PH7_MemObjRelease(pAttrValue);` |
| 1449445 | 4051 | `				PH7_MemObjStore(pList,pAttrValue);` |
|  724720 | 4052 | `			}` |
| 1449445 | 4053 | `			ph7_release_value(&(*pVm),pList);` |
|       - | 4054 | `		}` |
| 2174165 | 4055 | `	}` |
|  779926 | 4056 | `}` |
|     242 | 4057 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|       3 | 4058 | `{` |
|     245 | 4059 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      31 | 4060 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      31 | 4061 | `		if( pInst && pInst->pClass ){` |
|      31 | 4062 | `			return pInst->pClass->sName.zString;` |
|       - | 4063 | `		}` |
|     ! 0 | 4064 | `	}` |
|     215 | 4065 | `	return ph7_type_name(pVal);` |
|     124 | 4066 | `}` |
|       - | 4067 | `/*` |
|       - | 4068 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|       - | 4069 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|       - | 4070 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|       - | 4071 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|       - | 4072 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|       - | 4073 | ` */` |
|      34 | 4074 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|       1 | 4075 | `{` |
|      35 | 4076 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|      35 | 4077 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      15 | 4078 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      15 | 4079 | `		if( pInst && pInst->pClass ){` |
|      15 | 4080 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|      15 | 4081 | `			return;` |
|       - | 4082 | `		}` |
|     ! 0 | 4083 | `	}` |
|      21 | 4084 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|      18 | 4085 | `}` |
|       - | 4086 | `/*` |
|       - | 4087 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|       - | 4088 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|       - | 4089 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|       - | 4090 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|       - | 4091 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|       - | 4092 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|       - | 4093 | ` */` |
|      40 | 4094 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|       1 | 4095 | `{` |
|      41 | 4096 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      41 | 4097 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      41 | 4098 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      41 | 4099 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      41 | 4100 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|       - | 4101 | `	sxu32 i;` |
|     109 | 4102 | `	for( i = 0 ; i < nMin ; ++i ){` |
|       - | 4103 | `		unsigned char c;` |
|      69 | 4104 | `		if( cOp == '\|' ){` |
|      25 | 4105 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      57 | 4106 | `		}else if( cOp == '^' ){` |
|      21 | 4107 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|      11 | 4108 | `		}else{` |
|      25 | 4109 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|       - | 4110 | `		}` |
|      69 | 4111 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      35 | 4112 | `	}` |
|      41 | 4113 | `	if( cOp == '\|' && nL != nR ){` |
|      11 | 4114 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|      11 | 4115 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|      11 | 4116 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       5 | 4117 | `	}` |
|      41 | 4118 | `}` |
|       - | 4119 | `/*` |
|       - | 4120 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|       - | 4121 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|       - | 4122 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|       - | 4123 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|       - | 4124 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|       - | 4125 | ` * null, which is what an undefined variable answers.` |
|       - | 4126 | ` */` |
|      24 | 4127 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|       1 | 4128 | `{` |
|      25 | 4129 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       9 | 4130 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       9 | 4131 | `		if( pInst && pInst->pClass ){` |
|       9 | 4132 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|       9 | 4133 | `			return;` |
|       - | 4134 | `		}` |
|     ! 0 | 4135 | `	}` |
|      17 | 4136 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       7 | 4137 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|       4 | 4138 | `			pVal->x.iVal ? "true" : "false");` |
|      15 | 4139 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|      11 | 4140 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|       6 | 4141 | `	}else{` |
|       3 | 4142 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|       - | 4143 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|       - | 4144 | `	}` |
|      13 | 4145 | `}` |
|       - | 4146 | `/*` |
|       - | 4147 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|       - | 4148 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|       - | 4149 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|       - | 4150 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|       - | 4151 | ` * computes with the prefix. Classify pVal against that contract.` |
|       - | 4152 | ` */` |
|   77302 | 4153 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|       5 | 4154 | `{` |
|       - | 4155 | `	ph7_value sInt;` |
|       - | 4156 | `	sxi32 rc;` |
|   77307 | 4157 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   77307 | 4158 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   77307 | 4159 | `	PH7_MemObjRelease(&sInt);` |
|   77307 | 4160 | `	return rc;` |
|       5 | 4161 | `}` |
|  152711 | 4162 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|       5 | 4163 | `{` |
|  152716 | 4164 | `	int bBadL = 0, bBadR = 0;` |
|       - | 4165 | `	int i;` |
|       - | 4166 | `	ph7_value *apOperand[2];` |
|  152716 | 4167 | `	apOperand[0] = pLeft;` |
|  152716 | 4168 | `	apOperand[1] = pRight;` |
|       - | 4169 | `	/* array + array is php's union operator, not arithmetic */` |
|  152711 | 4170 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   23989 | 4171 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|    4475 | 4172 | `		return SXRET_OK;` |
|       - | 4173 | `	}` |
|  444728 | 4174 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  296487 | 4175 | `		ph7_value *pVal = apOperand[i];` |
|  296487 | 4176 | `		int bBad = 0;` |
|  296487 | 4177 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      77 | 4178 | `			bBad = 1;` |
|  296449 | 4179 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     272 | 4180 | `			const char *zTail = 0;` |
|     272 | 4181 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     272 | 4182 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|       - | 4183 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|      43 | 4184 | `				bBad = 1;` |
|      22 | 4185 | `			}else{` |
|       - | 4186 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|       - | 4187 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|       - | 4188 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     256 | 4189 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|      27 | 4190 | `					zTail++;` |
|       1 | 4191 | `				}` |
|     230 | 4192 | `				if( zTail < zEnd ){` |
|      36 | 4193 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      17 | 4194 | `				}` |
|       - | 4195 | `			}` |
|     135 | 4196 | `		}` |
|  296487 | 4197 | `		if( bBad ){` |
|     119 | 4198 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|      59 | 4199 | `		}` |
|  148547 | 4200 | `	}` |
|  148246 | 4201 | `	if( bBadL \|\| bBadR ){` |
|       - | 4202 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|       - | 4203 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|       - | 4204 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     163 | 4205 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|      54 | 4206 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     109 | 4207 | `		return SXERR_INVALID;` |
|       - | 4208 | `	}` |
|  148138 | 4209 | `	return SXRET_OK;` |
|   76511 | 4210 | `}` |
|       - | 4211 | `/*` |
|       - | 4212 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|       - | 4213 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|       - | 4214 | ` * pass 0 for the engine errors that leave it at its default.` |
|       - | 4215 | ` */` |
|    6012 | 4216 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|       5 | 4217 | `{` |
|       - | 4218 | `	ph7_vm *pVm;` |
|       - | 4219 | `	ph7_class *pClass;` |
|       - | 4220 | `	ph7_class_instance *pThis;` |
|       - | 4221 | `	ph7_class_method *pCons;` |
|       - | 4222 | `	ph7_value sArg,sCode;` |
|       - | 4223 | `	ph7_value *apArg[2];` |
|       - | 4224 | `	SyBlob sMsg;` |
|       - | 4225 | `	SyString sMsgStr;` |
|       - | 4226 | `	VmFrame *pFrame;` |
|       - | 4227 | `	sxi32 rc;` |
|       - | 4228 |  |
|    6017 | 4229 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4230 | `		return PH7_ABORT;` |
|       - | 4231 | `	}` |
|    6017 | 4232 | `	pVm = pCtx->pVm;` |
|    6017 | 4233 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4234 | `		zClass = "Error";` |
|     ! 0 | 4235 | `	}` |
|       - | 4236 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|       - | 4237 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|       - | 4238 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|       - | 4239 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|       - | 4240 | `	 * reported error (VmHostFuncThrowRc). */` |
|    6017 | 4241 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    6017 | 4242 | `	if( pClass == 0 ){` |
|     ! 0 | 4243 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4244 | `			"Cannot throw internal exception, class '%s' is not available",` |
|     ! 0 | 4245 | `			zClass` |
|       - | 4246 | `			);` |
|     ! 0 | 4247 | `		return pCtx->nThrowRc;` |
|       - | 4248 | `	}` |
|    6017 | 4249 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    6017 | 4250 | `	if( pThis == 0 ){` |
|     ! 0 | 4251 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 4252 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|       - | 4253 | `			);` |
|     ! 0 | 4254 | `		return pCtx->nThrowRc;` |
|       - | 4255 | `	}` |
|       - | 4256 |  |
|    6017 | 4257 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    6017 | 4258 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - | 4259 |  |
|    6017 | 4260 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    6017 | 4261 | `	if( pCons ){` |
|    6017 | 4262 | `		int nArg = 1;` |
|    6017 | 4263 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    6017 | 4264 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    6017 | 4265 | `		apArg[0] = &sArg;` |
|    6017 | 4266 | `		if( iCode != 0 ){` |
|      12 | 4267 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      12 | 4268 | `			apArg[1] = &sCode;` |
|      12 | 4269 | `			nArg = 2;` |
|       5 | 4270 | `		}` |
|    6017 | 4271 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    6017 | 4272 | `		if( iCode != 0 ){` |
|      12 | 4273 | `			PH7_MemObjRelease(&sCode);` |
|       5 | 4274 | `		}` |
|    6017 | 4275 | `		PH7_MemObjRelease(&sArg);` |
|    3006 | 4276 | `	}` |
|    6017 | 4277 | `	SyBlobRelease(&sMsg);` |
|       - | 4278 |  |
|    6017 | 4279 | `	pFrame = pVm->pFrame;` |
|    6017 | 4280 | `	if( pFrame ){` |
|    6017 | 4281 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    6017 | 4282 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    3006 | 4283 | `	}` |
|    6017 | 4284 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    6017 | 4285 | `	PH7_ClassInstanceUnref(pThis);` |
|    6017 | 4286 | `	if( rc == SXERR_ABORT ){` |
|     514 | 4287 | `		pCtx->nThrowRc = PH7_ABORT;` |
|     514 | 4288 | `		return PH7_ABORT;` |
|       - | 4289 | `	}` |
|       - | 4290 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|       - | 4291 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|       - | 4292 | `	 * argument-validation helper whose callers have no status channel — would` |
|       - | 4293 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|       - | 4294 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|       - | 4295 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|       - | 4296 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|       - | 4297 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|       - | 4298 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    5507 | 4299 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    5507 | 4300 | `	return PH7_EXCEPTION;` |
|    3011 | 4301 | `}` |
|    6002 | 4302 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|       5 | 4303 | `{` |
|       - | 4304 | `	va_list ap;` |
|       - | 4305 | `	sxi32 rc;` |
|    6007 | 4306 | `	va_start(ap,zFormat);` |
|    6007 | 4307 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    6007 | 4308 | `	va_end(ap);` |
|    6007 | 4309 | `	return rc;` |
|       5 | 4310 | `}` |
|       - | 4311 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      10 | 4312 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|       2 | 4313 | `{` |
|       - | 4314 | `	va_list ap;` |
|       - | 4315 | `	sxi32 rc;` |
|      12 | 4316 | `	va_start(ap,zFormat);` |
|      12 | 4317 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      12 | 4318 | `	va_end(ap);` |
|      12 | 4319 | `	return rc;` |
|       2 | 4320 | `}` |
|       - | 4321 | `/*` |
|       - | 4322 | ` * The status a host function's own throw should have returned. Consulted at the` |
|       - | 4323 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|       - | 4324 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|       - | 4325 | ` * and is the identity when the routine never threw or already reported it.` |
|       - | 4326 | ` *` |
|       - | 4327 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|       - | 4328 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|       - | 4329 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|       - | 4330 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|       - | 4331 | ` */` |
| 4070803 | 4332 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|       5 | 4333 | `{` |
| 4070803 | 4334 | `	if( pCtx->nThrowRc == 0` |
| 2038810 | 4335 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
| 4066778 | 4336 | `		return rc;` |
|       - | 4337 | `	}` |
|    4032 | 4338 | `	return pCtx->nThrowRc;` |
| 2036150 | 4339 | `}` |
|       - | 4340 | `/*` |
|       - | 4341 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|       - | 4342 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|       - | 4343 | ` */` |
|     ! 0 | 4344 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|     ! 0 | 4345 | `{` |
|       - | 4346 | `	ph7_vm *pVm;` |
|       - | 4347 | `	SyBlob sMsg;` |
|     ! 0 | 4348 | `	const char *zFuncName = 0;` |
|     ! 0 | 4349 | `	int nFuncLen = 0;` |
|       - | 4350 | `	va_list ap;` |
|       - | 4351 | `	sxi32 rc;` |
|       - | 4352 |  |
|     ! 0 | 4353 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 4354 | `		return PH7_OK;` |
|       - | 4355 | `	}` |
|     ! 0 | 4356 | `	pVm = pCtx->pVm;` |
|     ! 0 | 4357 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 4358 | `		zClass = "Error";` |
|     ! 0 | 4359 | `	}` |
|       - | 4360 |  |
|     ! 0 | 4361 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 4362 |  |
|     ! 0 | 4363 | `	va_start(ap,zFormat);` |
|     ! 0 | 4364 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     ! 0 | 4365 | `	va_end(ap);` |
|       - | 4366 |  |
|     ! 0 | 4367 | `	if( pCtx->pFunc ){` |
|     ! 0 | 4368 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|     ! 0 | 4369 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|     ! 0 | 4370 | `	}` |
|     ! 0 | 4371 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     ! 0 | 4372 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     ! 0 | 4373 | `	}` |
|     ! 0 | 4374 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|     ! 0 | 4375 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|     ! 0 | 4376 | `	SyBlobRelease(&sMsg);` |
|     ! 0 | 4377 | `	return rc;` |
|     ! 0 | 4378 | `}` |
|       - | 4379 | `/*` |
|       - | 4380 | ` * The following routine is invoked by the engine when an uncaught` |
|       - | 4381 | ` * exception is triggered.` |
|       - | 4382 | ` */` |
|     578 | 4383 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|       - | 4384 | `	ph7_vm *pVm, /* Target VM */` |
|       - | 4385 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4386 | `	)` |
|       4 | 4387 | `{` |
|       - | 4388 | `	ph7_value *apArg[2],sArg;` |
|     582 | 4389 | `	int nArg = 1;` |
|       - | 4390 | `	sxi32 rc;` |
|     582 | 4391 | `	if( pVm->nMuteThrow > 0 ){` |
|       - | 4392 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|       - | 4393 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|       - | 4394 | `		 * observable: no exception handler runs, no report is printed and the` |
|       - | 4395 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|       - | 4396 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|     ! 0 | 4397 | `		return SXERR_ABORT;` |
|       - | 4398 | `	}` |
|     582 | 4399 | `	if( pVm->nExceptDepth > 15 ){` |
|       - | 4400 | `		/* Nesting limit reached */` |
|     ! 0 | 4401 | `		return SXRET_OK;` |
|       - | 4402 | `	}` |
|       - | 4403 | `	/* Call any exception handler if available */` |
|     582 | 4404 | `	PH7_MemObjInit(pVm,&sArg);` |
|     582 | 4405 | `	if( pThis ){` |
|       - | 4406 | `		/* Load the exception instance */` |
|     582 | 4407 | `		sArg.x.pOther = pThis;` |
|     582 | 4408 | `		pThis->iRef++;` |
|     582 | 4409 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|     293 | 4410 | `	}else{` |
|     ! 0 | 4411 | `		nArg = 0;` |
|       - | 4412 | `	}` |
|     582 | 4413 | `	apArg[0] = &sArg;` |
|       - | 4414 | `	/* Call the exception handler if available */` |
|     582 | 4415 | `	pVm->nExceptDepth++;` |
|     582 | 4416 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|     582 | 4417 | `	pVm->nExceptDepth--;` |
|     582 | 4418 | `	if( rc != SXRET_OK ){` |
|       - | 4419 | `		const char *zFuncName;` |
|       - | 4420 | `		int nFuncLen;` |
|     580 | 4421 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       - | 4422 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|     580 | 4423 | `		if( pThis ){` |
|       - | 4424 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|       - | 4425 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|       - | 4426 | `			 * renders byte-identically to the historical single-entry report. */` |
|     580 | 4427 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|     292 | 4428 | `		}else{` |
|       - | 4429 | `			/* No instance (internal report path) — default-class single entry. */` |
|     ! 0 | 4430 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|       - | 4431 | `		}` |
|       - | 4432 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|     580 | 4433 | `		rc = SXERR_ABORT;` |
|     288 | 4434 | `	}` |
|     582 | 4435 | `	PH7_MemObjRelease(&sArg);` |
|     582 | 4436 | `	return rc;` |
|     293 | 4437 | `}` |
|       - | 4438 | `/*` |
|       - | 4439 | ` * Throw a user exception.` |
|       - | 4440 | ` *` |
|       - | 4441 | ` * Exception dispatch follows this sequence:` |
|       - | 4442 | ` *` |
|       - | 4443 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|       - | 4444 | ` *    try/catch whose catch block matches the exception class.` |
|       - | 4445 | ` *` |
|       - | 4446 | ` * 2. If NO catch matches:` |
|       - | 4447 | ` *    a. Run finally (if present) for the current try block.` |
|       - | 4448 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|       - | 4449 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|       - | 4450 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|       - | 4451 | ` *       exception in pVm->pPendingException instead of reporting it` |
|       - | 4452 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|       - | 4453 | ` *    d. Otherwise, report as truly uncaught.` |
|       - | 4454 | ` *` |
|       - | 4455 | ` * 3. If a catch DOES match:` |
|       - | 4456 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|       - | 4457 | ` *       aException stack and resetting it. This prevents a re-throw` |
|       - | 4458 | ` *       inside the catch body from immediately propagating past our` |
|       - | 4459 | ` *       finally block.` |
|       - | 4460 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|       - | 4461 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|       - | 4462 | ` *       no handlers (they're hidden), so the exception is deferred` |
|       - | 4463 | ` *       in pPendingException (step 2c).` |
|       - | 4464 | ` *    c. Restore outer handlers from the saved copy.` |
|       - | 4465 | ` *    d. Run finally (if present).` |
|       - | 4466 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|       - | 4467 | ` *       that handlers are restored and finally has run.` |
|       - | 4468 | ` */` |
|       - | 4469 | `/*` |
|       - | 4470 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|       - | 4471 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|       - | 4472 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|       - | 4473 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|       - | 4474 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|       - | 4475 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|       - | 4476 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|       - | 4477 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|       - | 4478 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|       - | 4479 | ` */` |
|     124 | 4480 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|       5 | 4481 | `{` |
|     137 | 4482 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|      45 | 4483 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      45 | 4484 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|      45 | 4485 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|     ! 0 | 4486 | `			break; /* reached an outer exec's / legacy handler */` |
|       - | 4487 | `		}` |
|      45 | 4488 | `		(void)SySetPop(&pVm->aException);` |
|      45 | 4489 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|      45 | 4490 | `		if( pT->iHasFinally ){` |
|      37 | 4491 | `			*pPc = pT->iFinallyPc;` |
|      37 | 4492 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|      37 | 4493 | `			return 1;` |
|       - | 4494 | `		}` |
|       - | 4495 | `		/* No finally: tear the try's transparent frame down now. */` |
|      11 | 4496 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       6 | 4497 | `			VmLeaveFrame(&(*pVm));` |
|       2 | 4498 | `		}` |
|      11 | 4499 | `		VmExcRelease(&(*pVm),pT);` |
|       3 | 4500 | `	}` |
|      95 | 4501 | `	return 0;` |
|      67 | 4502 | `}` |
|       - | 4503 | `/*` |
|       - | 4504 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|       - | 4505 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|       - | 4506 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|       - | 4507 | ` *` |
|       - | 4508 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|       - | 4509 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|       - | 4510 | ` *    and redirect to the catch body (iHandlerPc).` |
|       - | 4511 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|       - | 4512 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|       - | 4513 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|       - | 4514 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|       - | 4515 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|       - | 4516 | ` */` |
|       - | 4517 | `/*` |
|       - | 4518 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|       - | 4519 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|       - | 4520 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|       - | 4521 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|       - | 4522 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|       - | 4523 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|       - | 4524 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|       - | 4525 | ` * case) is unchanged: no wrapper.` |
|       - | 4526 | ` */` |
|   20248 | 4527 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|       5 | 4528 | `{` |
|   20253 | 4529 | `	VmFrame *pWrap = 0;` |
|       - | 4530 | `	VmFrame *pThrowSite;` |
|       - | 4531 | `	sxi32 rc;` |
|   20253 | 4532 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|   20149 | 4533 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4534 | `	}` |
|     108 | 4535 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|       - | 4536 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|     ! 0 | 4537 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4538 | `	}` |
|     108 | 4539 | `	pThrowSite = pWrap->pParent;` |
|     108 | 4540 | `	pWrap->pParent = pOwner;` |
|     108 | 4541 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|     108 | 4542 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 4543 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|       - | 4544 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|       - | 4545 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|       - | 4546 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|     108 | 4547 | `	if( pVm->pFrame == pWrap ){` |
|     108 | 4548 | `		VmLeaveFrame(&(*pVm));` |
|      52 | 4549 | `	}` |
|     108 | 4550 | `	pVm->pFrame = pThrowSite;` |
|     108 | 4551 | `	return rc;` |
|   10129 | 4552 | `}` |
|       - | 4553 | `/*` |
|       - | 4554 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|       - | 4555 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|       - | 4556 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|       - | 4557 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|       - | 4558 | ` */` |
|       - | 4559 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      92 | 4560 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|       - | 4561 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|       5 | 4562 | `{` |
|      97 | 4563 | `	if( pCatch ){` |
|      81 | 4564 | `		pException->iInCatch = 1;` |
|      81 | 4565 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|      81 | 4566 | `		if( pThis ){ pThis->iRef++; }` |
|      81 | 4567 | `		pException->pInflight = pThis;` |
|      81 | 4568 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      81 | 4569 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|      81 | 4570 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      81 | 4571 | `		return SXRET_OK;` |
|       - | 4572 | `	}` |
|      20 | 4573 | `	if( pException->iHasFinally ){` |
|       - | 4574 | `		VmFinallyAction sAct;` |
|      15 | 4575 | `		SyZero(&sAct,sizeof(sAct));` |
|      15 | 4576 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      15 | 4577 | `		if( pThis ){ pThis->iRef++; }` |
|      15 | 4578 | `		sAct.pExc = pThis;` |
|      15 | 4579 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      15 | 4580 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      15 | 4581 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      15 | 4582 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      15 | 4583 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      15 | 4584 | `		return SXRET_OK;` |
|       - | 4585 | `	}` |
|       - | 4586 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|       - | 4587 | `	 * flat native stack instead of mutual recursion. */` |
|       6 | 4588 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     ! 0 | 4589 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 | 4590 | `	}` |
|       6 | 4591 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       6 | 4592 | `	return VM_THROW_KEEP_UNWINDING;` |
|      51 | 4593 | `}` |
| 1449408 | 4594 | `PH7_PRIVATE sxi32 VmThrowException(` |
|       - | 4595 | `	ph7_vm *pVm,              /* Target VM */` |
|       - | 4596 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 4597 | `	)` |
|       5 | 4598 | `{` |
|       - | 4599 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|       - | 4600 | `	ph7_exception **apException;` |
|  724704 | 4601 | `	ph7_exception *pException;` |
|   50099 | 4602 | `Rethrow:` |
|       - | 4603 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|       - | 4604 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|       - | 4605 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|       - | 4606 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|       - | 4607 | `	 * so the throw path must be too). */` |
|       - | 4608 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|       - | 4609 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|       - | 4610 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
| 1549611 | 4611 | `	VmCoalesceDisarm(pVm);` |
|       - | 4612 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|       - | 4613 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|       - | 4614 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|       - | 4615 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|       - | 4616 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|       - | 4617 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|       - | 4618 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|       - | 4619 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
| 1549606 | 4620 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|      25 | 4621 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|      18 | 4622 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|       8 | 4623 | `	}` |
|       - | 4624 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|       - | 4625 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|       - | 4626 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|       - | 4627 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|       - | 4628 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|       - | 4629 | `	 * that owns the pending return, so it must leave that return intact. */` |
|       - | 4630 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|       - | 4631 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|       - | 4632 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|       - | 4633 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
| 1549611 | 4634 | `	pVm->pResumeFrame = 0;` |
|       - | 4635 | `	/* Point to the stack of loaded exceptions */` |
| 1549611 | 4636 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
| 1549611 | 4637 | `	pException = 0;` |
| 1549611 | 4638 | `	pCatch = 0;` |
| 1549611 | 4639 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 4640 | `		ph7_exception_block *aCatch;` |
|       - | 4641 | `		ph7_class *pClass;` |
|       - | 4642 | `		SyString *aNames;` |
|       - | 4643 | `		sxu32 nNames;` |
|       - | 4644 | `		int matched;` |
|       - | 4645 | `		sxu32 j,k;` |
|       - | 4646 | `		/* Locate the appropriate block to execute */` |
| 1448917 | 4647 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
| 1448917 | 4648 | `		(void)SySetPop(&pVm->aException);` |
| 1448917 | 4649 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       - | 4650 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|       - | 4651 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|       - | 4652 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
| 1448935 | 4653 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       - | 4654 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
| 1428751 | 4655 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
| 1428751 | 4656 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
| 1428751 | 4657 | `			matched = 0;` |
| 1428795 | 4658 | `			for( k = 0 ; k < nNames ; ++k ){` |
|       - | 4659 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|       - | 4660 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|       - | 4661 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
| 1428777 | 4662 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
| 1428777 | 4663 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 4664 | `					/* No such class, or trait — cannot match */` |
|     ! 0 | 4665 | `					continue;` |
|       - | 4666 | `				}` |
| 1428777 | 4667 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
| 1428733 | 4668 | `					matched = 1;` |
| 1428733 | 4669 | `					break;` |
|       - | 4670 | `				}` |
|      26 | 4671 | `			}` |
| 1428751 | 4672 | `			if( matched ){` |
|       - | 4673 | `				/* Catch block found,break immediately */` |
| 1428733 | 4674 | `				pCatch = &aCatch[j];` |
| 1428733 | 4675 | `				break;` |
|       - | 4676 | `			}` |
|      12 | 4677 | `		}` |
|  724456 | 4678 | `	}` |
|       - | 4679 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|       - | 4680 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|       - | 4681 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|       - | 4682 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|       - | 4683 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|       - | 4684 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|       - | 4685 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
| 1549611 | 4686 | `	if( pException ){` |
| 1448917 | 4687 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|  724456 | 4688 | `	}` |
|       - | 4689 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|       - | 4690 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|       - | 4691 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
| 1549611 | 4692 | `	if( pException && pException->iInlined ){` |
|      97 | 4693 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      97 | 4694 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|       - | 4695 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|       6 | 4696 | `			goto Rethrow;` |
|       - | 4697 | `		}` |
|      93 | 4698 | `		return rcInline;` |
|       - | 4699 | `	}` |
|       - | 4700 | `	/* Execute the cached block if available */` |
| 1549519 | 4701 | `	if( pCatch == 0 ){` |
|       - | 4702 | `		sxi32 rc;` |
|       - | 4703 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|  120867 | 4704 | `		if( pException && pException->iHasFinally ){` |
|   20169 | 4705 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|   20169 | 4706 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|   20169 | 4707 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|   20169 | 4708 | `			pException->iFinallyDone = 1;` |
|       - | 4709 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|       - | 4710 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|   20169 | 4711 | `			pVm->pInflightException = pThis;` |
|   20169 | 4712 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 4713 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|       - | 4714 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|   20169 | 4715 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|   20169 | 4716 | `			pVm->pInflightException = pSaveInflight;` |
|   20169 | 4717 | `			pVm->nInflightExcBase = nSaveBase;` |
|   20169 | 4718 | `			if( rc == SXERR_ABORT ){` |
|       3 | 4719 | `				VmExcRelease(&(*pVm),pException);` |
|       3 | 4720 | `				return SXERR_ABORT;` |
|       - | 4721 | `			}` |
|       - | 4722 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|       - | 4723 | `			 * semantics). The finally stored it on the body frame it returns from` |
|       - | 4724 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|       - | 4725 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|       - | 4726 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|       - | 4727 | `			 * takes the value instead of unwinding) and resume in place.` |
|       - | 4728 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|       - | 4729 | `			 * the same transport an in-place catch uses — and unwind as an` |
|       - | 4730 | `			 * exception; the owner's activation consumes the resume` |
|       - | 4731 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|       - | 4732 | `			 * its bHasRet tail materializes the return. */` |
|       - | 4733 | `			{` |
|   20167 | 4734 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   20167 | 4735 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|   20167 | 4736 | `				if( pOwnerFrame->bHasRet ){` |
|   20029 | 4737 | `					if( pOwnerFrame == pThrowFrame ){` |
|   20026 | 4738 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|       - | 4739 | `						/* Record the landing pad like the cross-frame case below.` |
|       - | 4740 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|       - | 4741 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|       - | 4742 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|       - | 4743 | `						 * its router unwound as an exception and the Unwind discard` |
|       - | 4744 | ``						 * dropped the parked return — `function f(){ try {`` |
|       - | 4745 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|       - | 4746 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|       - | 4747 | `						 * try frame down and its bHasRet tail materializes the` |
|       - | 4748 | `						 * return, same as the in-place-catch landing. */` |
|   20026 | 4749 | `						pVm->pResumeFrame = pOwnerFrame;` |
|   20026 | 4750 | `						pVm->iResumePc = pException->iLandingPc;` |
|   20026 | 4751 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|   20026 | 4752 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|   20026 | 4753 | `						VmExcRelease(&(*pVm),pException);` |
|   20026 | 4754 | `						return SXRET_OK;` |
|       - | 4755 | `					}` |
|       3 | 4756 | `					pVm->pResumeFrame = pOwnerFrame;` |
|       3 | 4757 | `					pVm->iResumePc = pException->iLandingPc;` |
|       3 | 4758 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|       3 | 4759 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|       3 | 4760 | `					VmExcRelease(&(*pVm),pException);` |
|       3 | 4761 | `					return PH7_EXCEPTION;` |
|       - | 4762 | `				}` |
|       - | 4763 | `			}` |
|       - | 4764 | `			/* The finally threw an exception that superseded pThis — it either` |
|       - | 4765 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|       - | 4766 | `			 * (which consumed an entry from the exception stack). Either way the` |
|       - | 4767 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|       - | 4768 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|     140 | 4769 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      16 | 4770 | `				VmExcRelease(&(*pVm),pException);` |
|      16 | 4771 | `				return PH7_EXCEPTION;` |
|       - | 4772 | `			}` |
|      61 | 4773 | `		}` |
|       - | 4774 | `		/* Check if there is an outer exception handler on the stack */` |
|  100825 | 4775 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 4776 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|       - | 4777 | `			 * iteration per unwound level instead of one native frame. */` |
|     130 | 4778 | `			VmExcRelease(&(*pVm),pException);` |
|     130 | 4779 | `			goto Rethrow;` |
|       - | 4780 | `		}` |
|  100699 | 4781 | `		if( pVm->nMuteThrow > 0 ){` |
|       - | 4782 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|       - | 4783 | `			 * default at class mount, which php would not have evaluated yet).` |
|       - | 4784 | `			 * Nothing outside the initializer may observe this throw: no` |
|       - | 4785 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|       - | 4786 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|       - | 4787 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|       - | 4788 | `			 * the mount path rolls the whole attempt back. */` |
|      47 | 4789 | `			VmExcRelease(&(*pVm),pException);` |
|      47 | 4790 | `			return SXERR_ABORT;` |
|       - | 4791 | `		}` |
|       - | 4792 | `		/* No outer handler. If the handlers were temporarily hidden` |
|       - | 4793 | `		 * (catch body re-throw with finally pending), defer the` |
|       - | 4794 | `		 * exception instead of reporting it uncaught.` |
|       - | 4795 | `		 */` |
|  100655 | 4796 | `		if( pVm->pPendingException == 0 && pThis ){` |
|       - | 4797 | `			/* Check if we are inside a catch execution with hidden handlers` |
|       - | 4798 | `			 * by looking for a catch frame on the stack.` |
|       - | 4799 | `			 */` |
|  100655 | 4800 | `			VmFrame *pF = pVm->pFrame;` |
|  100655 | 4801 | `			int inCatch = 0;` |
|  101273 | 4802 | `			while( pF ){` |
|  100695 | 4803 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|  100077 | 4804 | `					inCatch = 1;` |
|  100077 | 4805 | `					break;` |
|       - | 4806 | `				}` |
|     622 | 4807 | `				pF = pF->pParent;` |
|       4 | 4808 | `			}` |
|  100655 | 4809 | `			if( inCatch ){` |
|       - | 4810 | `				/* Defer — will be re-thrown after finally runs */` |
|  100077 | 4811 | `				pThis->iRef++;` |
|  100077 | 4812 | `				pVm->pPendingException = pThis;` |
|  100077 | 4813 | `				VmExcRelease(&(*pVm),pException);` |
|  100077 | 4814 | `				return SXRET_OK;` |
|       - | 4815 | `			}` |
|     289 | 4816 | `		}` |
|       - | 4817 | `		/* Truly uncaught */` |
|     582 | 4818 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|     582 | 4819 | `		if( rc == SXRET_OK && pException ){` |
|     ! 0 | 4820 | `			VmFrame *pFrame = pVm->pFrame;` |
|     ! 0 | 4821 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|     ! 0 | 4822 | `			if( pException->pFrame == pFrame ){` |
|     ! 0 | 4823 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|     ! 0 | 4824 | `			}` |
|     ! 0 | 4825 | `		}` |
|     582 | 4826 | `		VmExcRelease(&(*pVm),pException);` |
|     582 | 4827 | `		return rc;` |
|     ! 0 | 4828 | `	}else{` |
| 1428657 | 4829 | `		VmFrame *pFrame = pVm->pFrame;` |
| 1428657 | 4830 | `		ph7_exception **apSaved = 0;` |
|       - | 4831 | `		sxu32 nSavedCount;` |
|       - | 4832 | `		sxi32 rc;` |
|       - | 4833 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|       - | 4834 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|       - | 4835 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|       - | 4836 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|       - | 4837 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
| 1428657 | 4838 | `		VmFrame *pCatchBody = pException->pFrame;` |
| 1428657 | 4839 | `		sxu32 iCatchPc = pException->iLandingPc;` |
| 1428657 | 4840 | `		void *pCatchInstr = pException->pOwnerInstr;` |
| 1428657 | 4841 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
| 1428657 | 4842 | `		if( pException->pFrame == pFrame ){` |
|  826353 | 4843 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|  413174 | 4844 | `		}` |
|       - | 4845 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|       - | 4846 | `		 * body re-throws, the exception does not immediately propagate past` |
|       - | 4847 | `		 * our finally block. We save the stack contents and restore after.` |
|       - | 4848 | `		 */` |
| 1428657 | 4849 | `		nSavedCount = SySetUsed(&pVm->aException);` |
| 1428657 | 4850 | `		if( nSavedCount > 0 ){` |
|  150164 | 4851 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   50053 | 4852 | `				nSavedCount * sizeof(ph7_exception *));` |
|  100111 | 4853 | `			if( apSaved ){` |
|  150164 | 4854 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|   50053 | 4855 | `					nSavedCount * sizeof(ph7_exception *));` |
|  100111 | 4856 | `				SySetReset(&pVm->aException);` |
|   50053 | 4857 | `			}` |
|   50053 | 4858 | `		}` |
|       - | 4859 | `		/* Create the catch frame (made transparent below) */` |
| 1428657 | 4860 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
| 1428657 | 4861 | `		if( rc == SXRET_OK ){` |
|       - | 4862 | `			ph7_value *pObj;` |
|       - | 4863 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|       - | 4864 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|       - | 4865 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|       - | 4866 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|       - | 4867 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|       - | 4868 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|       - | 4869 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|       - | 4870 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|       - | 4871 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
| 1428657 | 4872 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|       - | 4873 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|       - | 4874 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|       - | 4875 | `			 * against the live current scope rather than a freed frame. */` |
| 1428657 | 4876 | `			if( pCatchBody ){` |
| 1428657 | 4877 | `				pFrame->pParent = pCatchBody;` |
|  714326 | 4878 | `			}` |
|       - | 4879 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|       - | 4880 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|       - | 4881 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|       - | 4882 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|       - | 4883 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|       - | 4884 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|       - | 4885 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|       - | 4886 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
| 1428657 | 4887 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|       - | 4888 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
| 2142983 | 4889 | `			pObj = (pCatch->sThis.nByte > 0)` |
| 1428650 | 4890 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
| 1428657 | 4891 | `			if( pObj ){` |
|       - | 4892 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|       - | 4893 | `				 * so it may already hold a value from a prior catch or assignment.` |
|       - | 4894 | `				 * Pin the new instance, then release the slot's prior contents` |
|       - | 4895 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|       - | 4896 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|       - | 4897 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
| 1428653 | 4898 | `				pThis->iRef++;` |
| 1428653 | 4899 | `				PH7_MemObjRelease(pObj);` |
| 1428653 | 4900 | `				pObj->x.pOther = pThis;` |
| 1428653 | 4901 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|  714324 | 4902 | `			}` |
|       - | 4903 | `			/* Execute the catch block */` |
| 1428657 | 4904 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|       - | 4905 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|       - | 4906 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|       - | 4907 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|       - | 4908 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|       - | 4909 | `			 * unbalanced — never pop somebody else's frame. */` |
| 1428657 | 4910 | `			if( pVm->pFrame == pFrame ){` |
| 1428657 | 4911 | `				VmLeaveFrame(&(*pVm));` |
|  714326 | 4912 | `			}` |
| 1428657 | 4913 | `			pVm->pFrame = pThrowSite;` |
|  714326 | 4914 | `		}` |
|       - | 4915 | `		/* Restore the outer exception handlers */` |
| 1428657 | 4916 | `		if( apSaved ){` |
|       - | 4917 | `			sxu32 k;` |
|       - | 4918 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|       - | 4919 | `			 * the catch body) are normally already consumed; on an abnormal` |
|       - | 4920 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|       - | 4921 | `			 * linger — release those activations before discarding the set. */` |
|  100111 | 4922 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|  100111 | 4923 | `			SySetReset(&pVm->aException);` |
|  200869 | 4924 | `			for(k = 0; k < nSavedCount; k++){` |
|  100763 | 4925 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|   50384 | 4926 | `			}` |
|  100111 | 4927 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|   50053 | 4928 | `		}` |
|       - | 4929 | `		/* Execute the finally block after catch */` |
| 1428657 | 4930 | `		if( pException->iHasFinally ){` |
|       - | 4931 | `			sxi32 rcf;` |
|       - | 4932 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|       - | 4933 | `			 * from, its pending-return write generation (set if the catch above` |
|       - | 4934 | `			 * returned), and the exception-stack depth. After the finally we use` |
|       - | 4935 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|       - | 4936 | `			 * catch-return. */` |
|       - | 4937 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|       - | 4938 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|       - | 4939 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|       - | 4940 | `			 * supersede decision belong to the owner, not the thrower. */` |
|      89 | 4941 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|      89 | 4942 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|      89 | 4943 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|       - | 4944 | `			/* The exception in flight while this finally runs is the catch body's` |
|       - | 4945 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|       - | 4946 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|       - | 4947 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|       - | 4948 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|      89 | 4949 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|      89 | 4950 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|      89 | 4951 | `			pException->iFinallyDone = 1;` |
|      89 | 4952 | `			pVm->pInflightException = pVm->pPendingException;` |
|      89 | 4953 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 4954 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|      89 | 4955 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|      89 | 4956 | `			pVm->pInflightException = pSaveInflight;` |
|      89 | 4957 | `			pVm->nInflightExcBase = nSaveBase;` |
|      89 | 4958 | `			if( rcf == SXERR_ABORT ){` |
|     ! 0 | 4959 | `				VmExcRelease(&(*pVm),pException);` |
|     ! 0 | 4960 | `				return SXERR_ABORT;` |
|       - | 4961 | `			}` |
|       - | 4962 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|       - | 4963 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|       - | 4964 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|       - | 4965 | `			 * either case that exception supersedes this try's catch-return — but` |
|       - | 4966 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|       - | 4967 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|       - | 4968 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|      89 | 4969 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      19 | 4970 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|      12 | 4971 | `					VmClearFramePending(pBody);` |
|       5 | 4972 | `				}` |
|       - | 4973 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|       - | 4974 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|       - | 4975 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|       - | 4976 | `				 * legitimately have re-armed it during the finally. */` |
|      19 | 4977 | `				pBody->nCatchJmpPc = 0;` |
|       8 | 4978 | `			}` |
|      89 | 4979 | `			if( rcf == PH7_EXCEPTION ){` |
|       - | 4980 | `				/* The finally's exception propagated past this try; drop any deferred` |
|       - | 4981 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|       - | 4982 | `				 * reaches the frame that caught the finally's throw. */` |
|      19 | 4983 | `				if( pVm->pPendingException ){` |
|     ! 0 | 4984 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|     ! 0 | 4985 | `					pVm->pPendingException = 0;` |
|     ! 0 | 4986 | `				}` |
|      19 | 4987 | `				VmExcRelease(&(*pVm),pException);` |
|      19 | 4988 | `				return PH7_EXCEPTION;` |
|       - | 4989 | `			}` |
|      34 | 4990 | `		}` |
| 1428641 | 4991 | `		if( rc == SXERR_ABORT ){` |
|       5 | 4992 | `			VmExcRelease(&(*pVm),pException);` |
|       5 | 4993 | `			return SXERR_ABORT;` |
|       - | 4994 | `		}` |
|       - | 4995 | `		/* If the catch body re-threw, the exception was deferred in` |
|       - | 4996 | `		 * pPendingException (because outer handlers were hidden).` |
|       - | 4997 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|       - | 4998 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|       - | 4999 | `		 * the catch frame having been left above), which swallows the in-flight` |
|       - | 5000 | `		 * exception (PHP semantics).` |
|       - | 5001 | `		 */` |
| 1428637 | 5002 | `		if( pVm->pPendingException ){` |
|       - | 5003 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|  100077 | 5004 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|  100077 | 5005 | `			if( !pOwner->bHasRet ){` |
|  100073 | 5006 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|       - | 5007 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|       - | 5008 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|       - | 5009 | `				 * than leave it armed for an unrelated later landing. */` |
|  100073 | 5010 | `				pOwner->nCatchJmpPc = 0;` |
|  100073 | 5011 | `				pVm->pPendingException = 0;` |
|  100073 | 5012 | `				VmExcRelease(&(*pVm),pException);` |
|       - | 5013 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|  100073 | 5014 | `				pThis = pReThrow;` |
|  100073 | 5015 | `				goto Rethrow;` |
|       - | 5016 | `			}` |
|       - | 5017 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|       6 | 5018 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|       6 | 5019 | `			pVm->pPendingException = 0;` |
|       2 | 5020 | `		}` |
|       - | 5021 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|       - | 5022 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|       - | 5023 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|       - | 5024 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
| 1328569 | 5025 | `		pVm->pResumeFrame = pCatchBody;` |
| 1328569 | 5026 | `		pVm->iResumePc = iCatchPc;` |
| 1328569 | 5027 | `		pVm->pResumeInstr = pCatchInstr;` |
| 1328569 | 5028 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|       - | 5029 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|       - | 5030 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|       - | 5031 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
| 1328569 | 5032 | `		VmExcRelease(&(*pVm),pException);` |
|       - | 5033 | `	}` |
| 1328569 | 5034 | `	return SXRET_OK;` |
|  724709 | 5035 | `}` |
|       - | 5036 |  |
