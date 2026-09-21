# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1966/2266 lines (86.76%)

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
|   21060 |   24 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile)` |
|       5 |   25 | `{` |
|   21065 |   26 | `	pVm->nLastErrType = iErr;` |
|   21065 |   27 | `	pVm->nLastErrLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|   21065 |   28 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|   21065 |   29 | `	if( zMsg && nMsg > 0 ){` |
|   21065 |   30 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|   10530 |   31 | `	}` |
|   21065 |   32 | `	SyBlobReset(&pVm->sLastErrFile);` |
|   21065 |   33 | `	if( pFile ){` |
|   21065 |   34 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|   10530 |   35 | `	}` |
|   21065 |   36 | `}` |
|       - |   37 | `/*` |
|       - |   38 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|       - |   39 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|       - |   40 | ` * diagnostic is never silently swallowed.` |
|       - |   41 | ` */` |
|     664 |   42 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|       4 |   43 | `{` |
|     668 |   44 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|       4 |   45 | `}` |
|       - |   46 | `/*` |
|       - |   47 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|       - |   48 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|       - |   49 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|       - |   50 | ` * headers_sent()/output accounting).` |
|       - |   51 | ` */` |
|     690 |   52 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|       4 |   53 | `{` |
|       - |   54 | `	sxi32 rc;` |
|       - |   55 | `	/* Append a new line */` |
|       - |   56 | `#ifdef __WINNT__` |
|       4 |   57 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|       - |   58 | `#else` |
|     690 |   59 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|       - |   60 | `#endif` |
|       - |   61 | `	/* Invoke the output consumer callback */` |
|     694 |   62 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     694 |   63 | `	if( bTrack ){` |
|      29 |   64 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      13 |   65 | `	}` |
|     694 |   66 | `	return rc;` |
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
|   21402 |   89 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|       5 |   90 | `{` |
|   21407 |   91 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|       - |   92 | `		ph7_value apArg[4];` |
|       - |   93 | `		ph7_value *apArgPtr[4];` |
|       - |   94 | `		ph7_value sResult;` |
|       - |   95 | `		SyString sErr;` |
|       - |   96 | `		/* Prepare arguments */` |
|     351 |   97 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|       - |   98 | `			/* use explicit message length to avoid reading past buffer */` |
|     351 |   99 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     351 |  100 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     351 |  101 | `		if( pFile ){` |
|     351 |  102 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     351 |  103 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     178 |  104 | `		}else{` |
|     ! 0 |  105 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|       - |  106 | `		}` |
|     351 |  107 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     351 |  108 | `		PH7_MemObjInit(pVm,&sResult);` |
|       - |  109 | `		/* Set up pointer array */` |
|     351 |  110 | `		apArgPtr[0] = &apArg[0];` |
|     351 |  111 | `		apArgPtr[1] = &apArg[1];` |
|     351 |  112 | `		apArgPtr[2] = &apArg[2];` |
|     351 |  113 | `		apArgPtr[3] = &apArg[3];` |
|       - |  114 | `		/* Call the handler */` |
|       - |  115 | `		{` |
|     351 |  116 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|     351 |  117 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
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
|     349 |  132 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 |  133 | `			PH7_MemObjToBool(&sResult);` |
|     ! 0 |  134 | `		}` |
|       - |  135 | `		/* Release */` |
|     349 |  136 | `		PH7_MemObjRelease(&apArg[0]);` |
|     349 |  137 | `		PH7_MemObjRelease(&apArg[1]);` |
|     349 |  138 | `		PH7_MemObjRelease(&apArg[2]);` |
|     349 |  139 | `		PH7_MemObjRelease(&apArg[3]);` |
|     349 |  140 | `		PH7_MemObjRelease(&sResult);` |
|       - |  141 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|       - |  142 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     349 |  143 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|       - |  144 | `	}` |
|       - |  145 | `	/* No handler, always call error handler */` |
|   21061 |  146 | `	return TRUE;` |
|   10706 |  147 | `}` |
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
|   21060 |  165 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|       5 |  166 | `{` |
|       - |  167 | `	sxi32 iBit;` |
|   21065 |  168 | `	if( !pVm->bErrReport ){` |
|    3618 |  169 | `		return 0;` |
|       - |  170 | `	}` |
|   17449 |  171 | `	switch( iErr ){` |
|    8700 |  172 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|   17405 |  173 | `		iBit = 2; break;` |
|       6 |  174 | `	case 512  /* E_USER_WARNING */:` |
|      14 |  175 | `		iBit = 512; break;` |
|     ! 0 |  176 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|       - |  177 | `	case 8    /* E_NOTICE */:` |
|     ! 0 |  178 | `		iBit = 8; break;` |
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
|   17449 |  191 | `	return (pVm->iErrMask & iBit) != 0;` |
|   10535 |  192 | `}` |
|     110 |  193 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|       4 |  194 | `{` |
|     114 |  195 | `	switch(iErr){` |
|      39 |  196 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|       - |  197 | `	case 512  /* E_USER_WARNING */:` |
|      82 |  198 | `		return "Warning";` |
|       4 |  199 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|       - |  200 | `	case 8    /* E_NOTICE */:` |
|       - |  201 | `	case 1024 /* E_USER_NOTICE */:` |
|      11 |  202 | `		return "Notice";` |
|     ! 0 |  203 | `	case 8192  /* E_DEPRECATED */:` |
|       - |  204 | `	case 16384 /* E_USER_DEPRECATED */:` |
|     ! 0 |  205 | `		return "Deprecated";` |
|     ! 0 |  206 | `	case 256 /* E_USER_ERROR */:` |
|     ! 0 |  207 | `		return "Fatal error";` |
|      12 |  208 | `	default:` |
|      28 |  209 | `		return "Error";` |
|       - |  210 | `	}` |
|      59 |  211 | `}` |
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
|     110 |  223 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|       4 |  224 | `{` |
|     114 |  225 | `	if( pFile ){` |
|     169 |  226 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,` |
|      55 |  227 | `			nLine ? nLine : 1);` |
|      55 |  228 | `	}` |
|     114 |  229 | `}` |
|       - |  230 | `/*` |
|       - |  231 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|       - |  232 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|       - |  233 | ` * (compile.c) and stock CLI's stderr log copy.` |
|       - |  234 | ` */` |
|      84 |  235 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|       4 |  236 | `{` |
|      88 |  237 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      88 |  238 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      88 |  239 | `}` |
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
|   21226 |  252 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|       5 |  253 | `{` |
|   21231 |  254 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     121 |  255 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     121 |  256 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      58 |  257 | `	}` |
|   21231 |  258 | `}` |
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
|     106 |  272 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|       - |  273 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|       4 |  274 | `{` |
|     110 |  275 | `	SyBlob *pWorker = &pVm->sWorker;` |
|     110 |  276 | `	sxi32 rc = SXRET_OK;` |
|     110 |  277 | `	if( pVm->bLogErrors ){` |
|      88 |  278 | `		SyBlobReset(pWorker);` |
|      88 |  279 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      88 |  280 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      88 |  281 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      88 |  282 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      42 |  283 | `	}` |
|     110 |  284 | `	if( pVm->bDisplayErrors ){` |
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
|     110 |  299 | `	return rc;` |
|       4 |  300 | `}` |
|     184 |  301 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|       - |  302 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  303 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  304 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|       - |  305 | `	const char *zMessage /* Null terminated error message */` |
|       - |  306 | `	)` |
|       5 |  307 | `{` |
|       - |  308 | `	SyBlob sMsg;` |
|       - |  309 | `	SyString *pFile;` |
|     189 |  310 | `	sxu32 nMsg = (sxu32)SyStrlen(zMessage);` |
|     189 |  311 | `	sxi32 rc = SXRET_OK;` |
|       - |  312 | `	/* Peek the processed file if available */` |
|     189 |  313 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     189 |  314 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     189 |  315 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|       - |  316 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|       - |  317 | `		 * out-of-memory fatal through this path with none, and must not need` |
|       - |  318 | `		 * an allocation to say so. */` |
|      10 |  319 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      10 |  320 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      10 |  321 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      10 |  322 | `		nMsg = SyBlobLength(&sMsg);` |
|       4 |  323 | `	}` |
|       - |  324 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|       - |  325 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     189 |  326 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)pVm->nCurLine) ){` |
|      68 |  327 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile);` |
|      68 |  328 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|       - |  329 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|       - |  330 | `			 * '@' php still runs the handler (done just above) but prints` |
|       - |  331 | `			 * nothing itself. */` |
|      64 |  332 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,pVm->nCurLine);` |
|      30 |  333 | `		}` |
|      32 |  334 | `	}` |
|     189 |  335 | `	SyBlobRelease(&sMsg);` |
|     189 |  336 | `	return rc;` |
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
|    5246 |  376 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|       5 |  377 | `{` |
|       - |  378 | `	double r;` |
|    5251 |  379 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_REAL) == 0 ){` |
|    5251 |  380 | `		return SXRET_OK;` |
|       - |  381 | `	}` |
|     ! 0 |  382 | `	r = (double)pVal->rVal;` |
|     ! 0 |  383 | `	if( r == (double)(sxi64)r ){` |
|     ! 0 |  384 | `		return SXRET_OK;` |
|       - |  385 | `	}` |
|     ! 0 |  386 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       - |  387 | `		"Implicit conversion from float to int loses precision");` |
|    2628 |  388 | `}` |
|       - |  389 | `/*` |
|       - |  390 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|       - |  391 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|       - |  392 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|       - |  393 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|       - |  394 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|       - |  395 | ` */` |
| 2180991 |  396 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|       5 |  397 | `{` |
|       - |  398 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|       - |  399 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
| 2180996 |  400 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|       5 |  401 | `}` |
|       - |  402 | `/*` |
|       - |  403 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|       - |  404 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|       - |  405 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|       - |  406 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|       - |  407 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|       - |  408 | ` * keep native re-entries off a finite C stack).` |
|       - |  409 | ` */` |
| 9345122 |  410 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|       5 |  411 | `{` |
| 9345127 |  412 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
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
|       1 |  447 | `{` |
|       5 |  448 | `	if( pVm->bHaltRequested ){` |
|     ! 0 |  449 | `		return PH7_ABORT;` |
|       - |  450 | `	}` |
|       5 |  451 | `	pVm->iExitStatus = 255;` |
|       5 |  452 | `	pVm->bHaltRequested = 1;` |
|       5 |  453 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|       5 |  454 | `	return PH7_ABORT;` |
|       3 |  455 | `}` |
|       - |  456 | `/*` |
|       - |  457 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - |  458 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - |  459 | ` * information.` |
|       - |  460 | ` */` |
|   21218 |  461 | `static sxi32 VmThrowErrorAp(` |
|       - |  462 | `	ph7_vm *pVm,         /* Target VM */` |
|       - |  463 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|       - |  464 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|       - |  465 | `	const char *zFormat, /* Format message */` |
|       - |  466 | `	va_list ap           /* Variable list of arguments */` |
|       - |  467 | `	)` |
|       5 |  468 | `{` |
|       - |  469 | `	SyBlob sMsg;` |
|       - |  470 | `	SyString *pFile;` |
|   21223 |  471 | `	sxi32 rc = SXRET_OK;` |
|       - |  472 | `	/* Peek the processed file if available */` |
|   21223 |  473 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - |  474 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|   21223 |  475 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|   21223 |  476 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|   21223 |  477 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       - |  478 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|       - |  479 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|       - |  480 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|       - |  481 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|   21223 |  482 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)pVm->nCurLine) ){` |
|       - |  483 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|       - |  484 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|   21001 |  485 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile);` |
|   21001 |  486 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|   20955 |  487 | `			SyBlobRelease(&sMsg);` |
|   20955 |  488 | `			return SXRET_OK;` |
|       - |  489 | `		}` |
|      72 |  490 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      23 |  491 | `			SyBlobLength(&sMsg),pFile,pVm->nCurLine);` |
|      23 |  492 | `	}` |
|     273 |  493 | `	SyBlobRelease(&sMsg);` |
|     273 |  494 | `	return rc;` |
|   10614 |  495 | `}` |
|       - |  496 | `/*` |
|       - |  497 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|       - |  498 | ` * scope), or NULL when executing outside any class context.` |
|       - |  499 | ` */` |
|     262 |  500 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|       5 |  501 | `{` |
|     267 |  502 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|     129 |  503 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|     129 |  504 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|       - |  505 | `	}` |
|     143 |  506 | `	return 0;` |
|     136 |  507 | `}` |
|       - |  508 | `/*` |
|       - |  509 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|       - |  510 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|       - |  511 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|       - |  512 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|       - |  513 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|       - |  514 | ` */` |
|  200350 |  515 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|       5 |  516 | `{` |
|       - |  517 | `	ph7_class *pErrClass;` |
|       - |  518 | `	ph7_class_instance *pThis;` |
|       - |  519 | `	ph7_class_method *pCons;` |
|       - |  520 | `	VmFrame *pFrame;` |
|       - |  521 | `	sxi32 rc;` |
|  200355 |  522 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|  200355 |  523 | `	if( pErrClass == 0 ){` |
|     ! 0 |  524 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  525 | `		return PH7_ABORT;` |
|       - |  526 | `	}` |
|  200355 |  527 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|  200355 |  528 | `	if( pThis == 0 ){` |
|     ! 0 |  529 | `		SyBlobRelease(pMsg);` |
|     ! 0 |  530 | `		return PH7_ABORT;` |
|       - |  531 | `	}` |
|  200355 |  532 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|  200355 |  533 | `	if( pCons ){` |
|       - |  534 | `		ph7_value sArg;` |
|       - |  535 | `		ph7_value *apArg[1];` |
|       - |  536 | `		SyString sMsgStr;` |
|  200355 |  537 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|  200355 |  538 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|  200355 |  539 | `		apArg[0] = &sArg;` |
|  200355 |  540 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|  200355 |  541 | `		PH7_MemObjRelease(&sArg);` |
|  100175 |  542 | `	}` |
|  200355 |  543 | `	SyBlobRelease(pMsg);` |
|  200355 |  544 | `	pFrame = pVm->pFrame;` |
|  200355 |  545 | `	if( pFrame ){` |
|  200355 |  546 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  200355 |  547 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|  100175 |  548 | `	}` |
|  200355 |  549 | `	rc = VmThrowException(&(*pVm),pThis);` |
|  200355 |  550 | `	PH7_ClassInstanceUnref(pThis);` |
|  200355 |  551 | `	if( rc == SXERR_ABORT ){` |
|      28 |  552 | `		return PH7_ABORT;` |
|       - |  553 | `	}` |
|  200331 |  554 | `	return PH7_EXCEPTION;` |
|  100180 |  555 | `}` |
|       - |  556 | `/*` |
|       - |  557 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|       - |  558 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|       - |  559 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|       - |  560 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|       - |  561 | ` * result through its normal goto Exception / goto Abort.` |
|       - |  562 | ` */` |
|      22 |  563 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|       3 |  564 | `{` |
|       - |  565 | `	SyBlob sMsg;` |
|      25 |  566 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      25 |  567 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      25 |  568 | `	return VmThrowBuiltinError(pVm, zClass, SyStrlen(zClass), &sMsg);` |
|       3 |  569 | `}` |
|       - |  570 | `/*` |
|       - |  571 | ` * Enum case singletons (PHP 8.1).` |
|       - |  572 | ` *` |
|       - |  573 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|       - |  574 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|       - |  575 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|       - |  576 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|       - |  577 | ` * backing-value type check and the duplicate-value check only fire when a` |
|       - |  578 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|       - |  579 | ` */` |
|       - |  580 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|       - |  581 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|       - |  582 | ` * readonly property" through the normal store path. */` |
|     104 |  583 | `static void VmEnumSetInstanceProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|       - |  584 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|       3 |  585 | `{` |
|     107 |  586 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|       - |  587 | `	VmClassAttr *pVmAttr;` |
|       - |  588 | `	ph7_value *pSlot;` |
|     107 |  589 | `	if( pEntry == 0 ){` |
|     ! 0 |  590 | `		return;` |
|       - |  591 | `	}` |
|     107 |  592 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     107 |  593 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     107 |  594 | `	if( pSlot == 0 ){` |
|     ! 0 |  595 | `		return;` |
|       - |  596 | `	}` |
|     107 |  597 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     107 |  598 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      55 |  599 | `}` |
|       - |  600 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|       - |  601 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|     136 |  602 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|       1 |  603 | `{` |
|     137 |  604 | `	ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pCase->nIdx);` |
|       - |  605 | `	ph7_class_instance *pObj;` |
|       - |  606 | `	SyHashEntry *pEntry;` |
|     137 |  607 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      49 |  608 | `		return 0;` |
|       - |  609 | `	}` |
|      89 |  610 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      89 |  611 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      89 |  612 | `	if( pEntry == 0 ){` |
|     ! 0 |  613 | `		return 0;` |
|       - |  614 | `	}` |
|      89 |  615 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      69 |  616 | `}` |
|       - |  617 | `/*` |
|       - |  618 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|       - |  619 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|       - |  620 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|       - |  621 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|       - |  622 | ` * raises it. Returns the throw status to park/route.` |
|       - |  623 | ` */` |
|       2 |  624 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|       1 |  625 | `{` |
|       - |  626 | `	SyBlob sMsg;` |
|       3 |  627 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|       3 |  628 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|       3 |  629 | `	pVm->pConstCycleAttr = 0;` |
|       3 |  630 | `	pVm->pConstCycleClass = 0;` |
|       3 |  631 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  632 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|       1 |  633 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|       3 |  634 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  635 | `}` |
|       - |  636 | `/*` |
|       - |  637 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|       - |  638 | ` * materialize lazily and individually on first access — the backing-value` |
|       - |  639 | ` * type check fires per case, and the duplicate-value check compares only` |
|       - |  640 | ` * against cases that have already materialized (a broken sibling case does` |
|       - |  641 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|       - |  642 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|       - |  643 | ` * (duplicate value / self-reference) — which the caller routes` |
|       - |  644 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|       - |  645 | ` */` |
|     124 |  646 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|       3 |  647 | `{` |
|       - |  648 | `	ph7_class_attr **apCase;` |
|       - |  649 | `	ph7_class_instance *pObj;` |
|       - |  650 | `	ph7_value *pSlot;` |
|       - |  651 | `	ph7_value sBacking,sPropVal;` |
|       - |  652 | `	sxu32 i;` |
|     127 |  653 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      63 |  654 | `		return SXRET_OK;` |
|       - |  655 | `	}` |
|      65 |  656 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  657 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|       - |  658 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|     ! 0 |  659 | `		if( pVm->pConstCycleAttr == 0 ){` |
|     ! 0 |  660 | `			pVm->pConstCycleAttr = pCase;` |
|     ! 0 |  661 | `			pVm->pConstCycleClass = pClass;` |
|     ! 0 |  662 | `		}` |
|     ! 0 |  663 | `		return SXRET_OK;` |
|       - |  664 | `	}` |
|      65 |  665 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      65 |  666 | `	if( pClass->nEnumBacking != 0 ){` |
|      52 |  667 | `		if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|       - |  668 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self:: */`` |
|      52 |  669 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       - |  670 | `			sxi32 rcExec;` |
|      52 |  671 | `			pVm->pConstEvalClass = pClass;` |
|      52 |  672 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      52 |  673 | `			pVm->nConstEvalDepth++;` |
|      52 |  674 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      52 |  675 | `			pVm->nConstEvalDepth--;` |
|      52 |  676 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      52 |  677 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      52 |  678 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  679 | `				/* The backing expression raised: abandon materialization and` |
|       - |  680 | `				 * hand the status to the caller to park/route. */` |
|     ! 0 |  681 | `				PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  682 | `				return rcExec;` |
|       - |  683 | `			}` |
|      52 |  684 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|     ! 0 |  685 | `				PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  686 | `				return VmConstCycleThrow(&(*pVm));` |
|       - |  687 | `			}` |
|      25 |  688 | `		}` |
|      52 |  689 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|       - |  690 | `			/* php: TypeError, checked lazily at first case access */` |
|       - |  691 | `			SyBlob sMsg;` |
|       3 |  692 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|       3 |  693 | `			PH7_MemObjRelease(&sBacking);` |
|       3 |  694 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       2 |  695 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|       2 |  696 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|       3 |  697 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       - |  698 | `		}` |
|      50 |  699 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       - |  700 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|       - |  701 | `			 * the typed-constant leniency) to a genuine int. */` |
|      11 |  702 | `			PH7_MemObjToInteger(&sBacking);` |
|       6 |  703 | `		}else{` |
|      40 |  704 | `			PH7_MemObjToString(&sBacking);` |
|       - |  705 | `		}` |
|       - |  706 | `		/* php: two cases sharing one backing value are an Error — compared` |
|       - |  707 | `		 * against already-materialized cases only (php registers values as` |
|       - |  708 | `		 * each case evaluates). */` |
|      50 |  709 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     188 |  710 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|       - |  711 | `			ph7_value *pPrev;` |
|     142 |  712 | `			int bDup = 0;` |
|     142 |  713 | `			if( apCase[i] == pCase ){` |
|      48 |  714 | `				continue;` |
|       - |  715 | `			}` |
|      95 |  716 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      95 |  717 | `			if( pPrev ){` |
|      47 |  718 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       5 |  719 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|       3 |  720 | `				}else{` |
|      51 |  721 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|      42 |  722 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|      16 |  723 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|       - |  724 | `				}` |
|      23 |  725 | `			}` |
|      95 |  726 | `			if( bDup ){` |
|       - |  727 | `				/* php prints the two cases in DECLARATION order regardless of` |
|       - |  728 | `				 * which one is being evaluated. */` |
|       3 |  729 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|       - |  730 | `				SyBlob sMsg;` |
|       - |  731 | `				sxu32 j;` |
|       5 |  732 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|       5 |  733 | `					if( apCase[j] == pCase ){ break; }` |
|       2 |  734 | `				}` |
|       3 |  735 | `				if( j < i ){` |
|     ! 0 |  736 | `					pFirst = pCase;` |
|     ! 0 |  737 | `					pSecond = apCase[i];` |
|     ! 0 |  738 | `				}` |
|       3 |  739 | `				PH7_MemObjRelease(&sBacking);` |
|       3 |  740 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 |  741 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|       1 |  742 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|       3 |  743 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - |  744 | `			}` |
|      47 |  745 | `		}` |
|      23 |  746 | `	}` |
|       - |  747 | `	/* Create the singleton and fill its readonly props */` |
|      61 |  748 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      61 |  749 | `	if( pObj == 0 ){` |
|     ! 0 |  750 | `		PH7_MemObjRelease(&sBacking);` |
|     ! 0 |  751 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  752 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|     ! 0 |  753 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  754 | `		return PH7_ABORT;` |
|       - |  755 | `	}` |
|      61 |  756 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      61 |  757 | `	VmEnumSetInstanceProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      61 |  758 | `	PH7_MemObjRelease(&sPropVal);` |
|      61 |  759 | `	if( pClass->nEnumBacking != 0 ){` |
|      48 |  760 | `		VmEnumSetInstanceProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|      23 |  761 | `	}` |
|      61 |  762 | `	PH7_MemObjRelease(&sBacking);` |
|       - |  763 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|       - |  764 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      61 |  765 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      61 |  766 | `	if( pSlot == 0 ){` |
|     ! 0 |  767 | `		PH7_ClassInstanceUnref(pObj);` |
|     ! 0 |  768 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  769 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|     ! 0 |  770 | `			&pClass->sName,&pCase->sName);` |
|     ! 0 |  771 | `		return PH7_ABORT;` |
|       - |  772 | `	}` |
|      61 |  773 | `	pSlot->x.pOther = pObj;` |
|      61 |  774 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      61 |  775 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      61 |  776 | `	pCase->nIdx = pSlot->nIdx;` |
|      61 |  777 | `	return SXRET_OK;` |
|      65 |  778 | `}` |
|       - |  779 | `/*` |
|       - |  780 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|       - |  781 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|       - |  782 | ` * there, so a broken case surfaces its error at the same point).` |
|       - |  783 | ` */` |
|      56 |  784 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|       3 |  785 | `{` |
|       - |  786 | `	ph7_class_attr **apCase;` |
|       - |  787 | `	sxu32 n;` |
|      59 |  788 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 |  789 | `		return SXRET_OK;` |
|       - |  790 | `	}` |
|      59 |  791 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     173 |  792 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     121 |  793 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|     121 |  794 | `		if( rc != SXRET_OK ){` |
|       5 |  795 | `			return rc;` |
|       - |  796 | `		}` |
|      60 |  797 | `	}` |
|      55 |  798 | `	return SXRET_OK;` |
|      31 |  799 | `}` |
|       - |  800 | `/*` |
|       - |  801 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|       - |  802 | ` * or 0 when the name does not name an enum.` |
|       - |  803 | ` */` |
|      38 |  804 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|       2 |  805 | `{` |
|       - |  806 | `	ph7_class *pClass;` |
|      40 |  807 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|     ! 0 |  808 | `		return 0;` |
|       - |  809 | `	}` |
|      59 |  810 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      19 |  811 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      42 |  812 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|       3 |  813 | `		pClass = pClass->pNextName;` |
|       1 |  814 | `	}` |
|      40 |  815 | `	return pClass;` |
|      21 |  816 | `}` |
|       - |  817 | `/*` |
|       - |  818 | ` * Evaluate a class constant's initializer on demand.` |
|       - |  819 | ` *` |
|       - |  820 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|       - |  821 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|       - |  822 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|       - |  823 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|       - |  824 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|       - |  825 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|       - |  826 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|       - |  827 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|       - |  828 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|       - |  829 | ` * "Cannot declare self-referencing constant" Error.` |
|       - |  830 | ` */` |
|     338 |  831 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  832 | `{` |
|       - |  833 | `	ph7_value *pMemObj;` |
|     338 |  834 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     338 |  835 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     343 |  836 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|     ! 0 |  837 | `		return SXRET_OK;` |
|       - |  838 | `	}` |
|     343 |  839 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|       - |  840 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|       - |  841 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|       - |  842 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|       3 |  843 | `		if( pVm->pConstCycleAttr == 0 ){` |
|       3 |  844 | `			pVm->pConstCycleAttr = pAttr;` |
|       3 |  845 | `			pVm->pConstCycleClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 |  846 | `		}` |
|       3 |  847 | `		return SXRET_OK;` |
|       - |  848 | `	}` |
|     341 |  849 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     341 |  850 | `	if( pMemObj == 0 ){` |
|     ! 0 |  851 | `		return SXERR_MEM;` |
|       - |  852 | `	}` |
|     341 |  853 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     341 |  854 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|     341 |  855 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|       - |  856 | `		sxi32 rcExec;` |
|     341 |  857 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|     341 |  858 | `		pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - |  859 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|       - |  860 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|       - |  861 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|     341 |  862 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|     341 |  863 | `		pVm->nConstEvalDepth++;` |
|     341 |  864 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|     341 |  865 | `		pVm->nConstEvalDepth--;` |
|     341 |  866 | `		pVm->pConstEvalClass = pSaveCtx;` |
|     341 |  867 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|     341 |  868 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       - |  869 | `		/* Memoize before any throw so re-access doesn't loop. */` |
|     341 |  870 | `		pAttr->nIdx = pMemObj->nIdx;` |
|     341 |  871 | `		PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     341 |  872 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|       - |  873 | `			/* The initializer raised: hand the status to the caller to` |
|       - |  874 | `			 * park/route. */` |
|     ! 0 |  875 | `			return rcExec;` |
|       - |  876 | `		}` |
|     341 |  877 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|       - |  878 | `			/* A nested evaluation detected a self-referencing constant:` |
|       - |  879 | `			 * raise it here, at opcode level, where it routes to a catch. */` |
|       3 |  880 | `			return VmConstCycleThrow(&(*pVm));` |
|       - |  881 | `		}` |
|     339 |  882 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     ! 0 |  883 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|     ! 0 |  884 | `			if( rcType != SXRET_OK ){` |
|     ! 0 |  885 | `				return rcType;` |
|       - |  886 | `			}` |
|     ! 0 |  887 | `		}` |
|     339 |  888 | `		return SXRET_OK;` |
|       - |  889 | `	}` |
|     ! 0 |  890 | `	pAttr->nIdx = pMemObj->nIdx;` |
|     ! 0 |  891 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     ! 0 |  892 | `	return SXRET_OK;` |
|     174 |  893 | `}` |
|       - |  894 | `/*` |
|       - |  895 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|       - |  896 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|       - |  897 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|       - |  898 | ` */` |
|      60 |  899 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       2 |  900 | `{` |
|      62 |  901 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      40 |  902 | `		return SXRET_OK;` |
|       - |  903 | `	}` |
|      23 |  904 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       5 |  905 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       - |  906 | `	}` |
|      19 |  907 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      32 |  908 | `}` |
|       - |  909 | `/*` |
|       - |  910 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|       - |  911 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|       - |  912 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|       - |  913 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|       - |  914 | ` */` |
|       6 |  915 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|       1 |  916 | `{` |
|       - |  917 | `	SyBlob sMsg;` |
|       7 |  918 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  919 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       7 |  920 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  921 | `}` |
|       - |  922 | `/*` |
|       - |  923 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|       - |  924 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|       - |  925 | ` * raised at the store site here with the same message and the same` |
|       - |  926 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|       - |  927 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|       - |  928 | ` */` |
|       2 |  929 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|       1 |  930 | `{` |
|       3 |  931 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|       3 |  932 | `	pVm->iExitStatus = 255;` |
|       3 |  933 | `	pVm->bHaltRequested = 1;` |
|       3 |  934 | `	return PH7_ABORT;` |
|       1 |  935 | `}` |
|       - |  936 | `/*` |
|       - |  937 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|       - |  938 | ` * property assignment. Called from the STORE path when coercion is not` |
|       - |  939 | ` * possible.` |
|       - |  940 | ` */` |
|  100088 |  941 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|       5 |  942 | `{` |
|  100093 |  943 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|  100093 |  944 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - |  945 | `	SyBlob sMsg;` |
|  100093 |  946 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - |  947 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|       - |  948 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|  100093 |  949 | `	if( pOwner ){` |
|  100093 |  950 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|   50044 |  951 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|   50049 |  952 | `	}else{` |
|     ! 0 |  953 | `		SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|     ! 0 |  954 | `			zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|       - |  955 | `	}` |
|  100093 |  956 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       5 |  957 | `}` |
|       - |  958 | `/*` |
|       - |  959 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|       - |  960 | ` */` |
|  100006 |  961 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       2 |  962 | `{` |
|  100008 |  963 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|  100008 |  964 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|       - |  965 | `	SyBlob sMsg;` |
|  100008 |  966 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|  100008 |  967 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|   50003 |  968 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|  100008 |  969 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       2 |  970 | `}` |
|       - |  971 | `/*` |
|       - |  972 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|       - |  973 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|       - |  974 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|       - |  975 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|       - |  976 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|       - |  977 | ` */` |
|       - |  978 | `/*` |
|       - |  979 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|       - |  980 | ` * property from a scope its set-visibility excludes:` |
|       - |  981 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|       - |  982 | ` */` |
|      14 |  983 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 |  984 | `{` |
|      15 |  985 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      15 |  986 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|      15 |  987 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|       - |  988 | `	SyBlob sMsg;` |
|      15 |  989 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 |  990 | `	if( pActive ){` |
|       3 |  991 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from scope %z",` |
|       1 |  992 | `			zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|       2 |  993 | `	}else{` |
|      13 |  994 | `		SyBlobFormat(&sMsg,"Cannot modify %s property %z::$%z from global scope",` |
|       6 |  995 | `			zVis,&pOwner->sName,&pAttr->sName);` |
|       - |  996 | `	}` |
|      15 |  997 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       1 |  998 | `}` |
|       - |  999 | `/*` |
|       - | 1000 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|       - | 1001 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|       - | 1002 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|       - | 1003 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|       - | 1004 | ` */` |
|      32 | 1005 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|       1 | 1006 | `{` |
|      33 | 1007 | `	ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pOwner;` |
|      33 | 1008 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1009 | `	int bOk;` |
|      33 | 1010 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      27 | 1011 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|      14 | 1012 | `	}else{` |
|       7 | 1013 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|       - | 1014 | `	}` |
|      33 | 1015 | `	if( !bOk ){` |
|      15 | 1016 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|       - | 1017 | `	}` |
|      19 | 1018 | `	return SXRET_OK;` |
|      17 | 1019 | `}` |
|      32 | 1020 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|       5 | 1021 | `{` |
|      37 | 1022 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1023 | `	SyBlob sMsg;` |
|      37 | 1024 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      37 | 1025 | `	if( bModify ){` |
|      33 | 1026 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|      19 | 1027 | `	}else{` |
|       6 | 1028 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       6 | 1029 | `		if( pActive ){` |
|     ! 0 | 1030 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|     ! 0 | 1031 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|     ! 0 | 1032 | `		}else{` |
|       6 | 1033 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|       2 | 1034 | `				&pOwner->sName,&pAttr->sName);` |
|       - | 1035 | `		}` |
|       - | 1036 | `	}` |
|      37 | 1037 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       5 | 1038 | `}` |
|       - | 1039 | `/*` |
|       - | 1040 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|       - | 1041 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|       - | 1042 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|       - | 1043 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|       - | 1044 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|       - | 1045 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|       - | 1046 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|       - | 1047 | ` */` |
|  542217 | 1048 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|       5 | 1049 | `{` |
|       - | 1050 | `	SyHashEntry *pSlot;` |
|       - | 1051 | `	VmClassAttr *pVmAttr;` |
|  542222 | 1052 | `	if( nIdx == SXU32_HIGH \|\| SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|  307286 | 1053 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|       - | 1054 | `	}` |
|  234941 | 1055 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  234941 | 1056 | `	if( pSlot == 0 ){` |
|  234857 | 1057 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1058 | `	}` |
|      88 | 1059 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      88 | 1060 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|      12 | 1061 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|       - | 1062 | `	}` |
|      74 | 1063 | `	if( pVmAttr->pAttr` |
|      76 | 1064 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|       - | 1065 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|       7 | 1066 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|       - | 1067 | `	}` |
|      70 | 1068 | `	return SXRET_OK;` |
|  271284 | 1069 | `}` |
|       - | 1070 | `/*` |
|       - | 1071 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|       - | 1072 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|       - | 1073 | ` * For class types, instanceof is verified.` |
|       - | 1074 | ` *` |
|       - | 1075 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|       - | 1076 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|       - | 1077 | ` */` |
|       - | 1078 |  |
|       - | 1079 | `/*` |
|       - | 1080 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|       - | 1081 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|       - | 1082 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|       - | 1083 | ` *   0 if it's not strictly numeric.` |
|       - | 1084 | ` */` |
|      18 | 1085 | `static int VmStringNumericKind(ph7_value *pValue)` |
|       3 | 1086 | `{` |
|       - | 1087 | `	const char *z, *zEnd, *zTail;` |
|       - | 1088 | `	sxu32 n;` |
|      21 | 1089 | `	sxu8 bReal = 0;` |
|       - | 1090 | `	sxi32 rc;` |
|      21 | 1091 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 1092 | `		return 0;` |
|       - | 1093 | `	}` |
|      18 | 1094 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|      18 | 1095 | `	n = SyBlobLength(&pValue->sBlob);` |
|      18 | 1096 | `	zEnd = z + n;` |
|      18 | 1097 | `	if( n == 0 ) return 0;` |
|      18 | 1098 | `	zTail = 0;` |
|      18 | 1099 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|      18 | 1100 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|      19 | 1101 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|      15 | 1102 | `	if( zTail != zEnd ) return 0;` |
|      15 | 1103 | `	return bReal ? 2 : 1;` |
|      12 | 1104 | `}` |
|       - | 1105 |  |
|       - | 1106 | `/*` |
|       - | 1107 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|       - | 1108 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|       - | 1109 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|       - | 1110 | ` * property, union alternative — would have to string-match the name itself.` |
|       - | 1111 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|       - | 1112 | ` * to extend when another literal/pseudo type is added.` |
|       - | 1113 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|       - | 1114 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|       - | 1115 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|       - | 1116 | ` */` |
|     856 | 1117 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|       5 | 1118 | `{` |
|     861 | 1119 | `	const char *z = pClass->zString;` |
|     861 | 1120 | `	sxu32 n = pClass->nByte;` |
|     861 | 1121 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      90 | 1122 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|       - | 1123 | `	}` |
|     775 | 1124 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|      24 | 1125 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|       - | 1126 | `	}` |
|     753 | 1127 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       3 | 1128 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|       - | 1129 | `	}` |
|     751 | 1130 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|       - | 1131 | `		/* iterable === array \| Traversable */` |
|      24 | 1132 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      14 | 1133 | `			return 1;` |
|       - | 1134 | `		}` |
|      11 | 1135 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       5 | 1136 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       5 | 1137 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       5 | 1138 | `				return 1;` |
|       - | 1139 | `			}` |
|     ! 0 | 1140 | `		}` |
|       7 | 1141 | `		return 0;` |
|       - | 1142 | `	}` |
|     729 | 1143 | `	return -1;` |
|     433 | 1144 | `}` |
|       - | 1145 | `/*` |
|       - | 1146 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|       - | 1147 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|       - | 1148 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|       - | 1149 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|       - | 1150 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|       - | 1151 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|       - | 1152 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|       - | 1153 | ` * throw.` |
|       - | 1154 | ` *` |
|       - | 1155 | ` * The class match for object values consults the active VM self-stack to` |
|       - | 1156 | `` * resolve `self`/`parent` aliases when present.`` |
|       - | 1157 | ` */` |
|       - | 1158 | `/*` |
|       - | 1159 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|       - | 1160 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf`` |
|       - | 1161 | ` * (the active self for params/returns/properties, or the declaring class for a` |
|       - | 1162 | ` * class constant). Used by every type-enforcement site so the resolution rule —` |
|       - | 1163 | ` * including the iLoadable flag — lives in one place.` |
|       - | 1164 | ` *` |
|       - | 1165 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|       - | 1166 | ` * compatibility target, where the type may legitimately be an interface or` |
|       - | 1167 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|       - | 1168 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|       - | 1169 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|       - | 1170 | ` * with TRUE; it does not go through this helper.)` |
|       - | 1171 | ` */` |
|     772 | 1172 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|       5 | 1173 | `{` |
|     777 | 1174 | `	if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      37 | 1175 | `		return pSelf;` |
|       - | 1176 | `	}` |
|     743 | 1177 | `	if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|       - | 1178 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|       - | 1179 | `		 * resolves against the runtime using class, matching the self:: trait rule. */` |
|       7 | 1180 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|     ! 0 | 1181 | `			pSelf = PH7_VmPeekTopClass(pVm);` |
|     ! 0 | 1182 | `		}` |
|       7 | 1183 | `		return pSelf ? pSelf->pBase : 0;` |
|       - | 1184 | `	}` |
|     737 | 1185 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|     391 | 1186 | `}` |
|       - | 1187 | `/*` |
|       - | 1188 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|       - | 1189 | ` * float-identity leniency — see the typed-constant note above` |
|       - | 1190 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|       - | 1191 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|       - | 1192 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|       - | 1193 | ` * so an int-typed parameter / return / property / union member silently` |
|       - | 1194 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|       - | 1195 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|       - | 1196 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|       - | 1197 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|       - | 1198 | ` * present).` |
|       - | 1199 | ` *` |
|       - | 1200 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|       - | 1201 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|       - | 1202 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|       - | 1203 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|       - | 1204 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|       - | 1205 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|       - | 1206 | ` */` |
| 2929392 | 1207 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|       5 | 1208 | `{` |
| 2929392 | 1209 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|  744416 | 1210 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       - | 1211 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|       - | 1212 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|       - | 1213 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|       - | 1214 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|      34 | 1215 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|      34 | 1216 | `		SyBlobRelease(&pVal->sBlob);` |
|      34 | 1217 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|      15 | 1218 | `	}` |
| 2929397 | 1219 | `}` |
|     200 | 1220 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|       5 | 1221 | `{` |
|       - | 1222 | `	sxu32 i;` |
|       - | 1223 | `	sxu32 nAlts;` |
|       - | 1224 | `	ph7_type_alt *aAlts;` |
|       - | 1225 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|       - | 1226 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     205 | 1227 | `	int bHasIntersection = 0;` |
|       - | 1228 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     205 | 1229 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      19 | 1230 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|       - | 1231 | `	}` |
|     189 | 1232 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     189 | 1233 | `	nAlts = SySetUsed(pAlts);` |
|       - | 1234 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|       - | 1235 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|       - | 1236 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|       - | 1237 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|       - | 1238 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|    6077 | 1239 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     567 | 1240 | `	for( i = 0; i < nAlts; i++ ){` |
|     383 | 1241 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|      37 | 1242 | `			bHasIntersection = 1;` |
|      17 | 1243 | `		}` |
|     194 | 1244 | `	}` |
|       - | 1245 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|       - | 1246 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|       - | 1247 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|       - | 1248 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     189 | 1249 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|      35 | 1250 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      35 | 1251 | `		ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|       - | 1252 | `		sxu32 g;` |
|     421 | 1253 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|       - | 1254 | `			int bAll;` |
|     409 | 1255 | `			if( aGroupCount[g] < 2 ) continue;` |
|      35 | 1256 | `			bAll = 1;` |
|      87 | 1257 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1258 | `				ph7_class *pExpected;` |
|      67 | 1259 | `				if( aAlts[i].nGroup != g ) continue;` |
|      63 | 1260 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|      63 | 1261 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);` |
|      63 | 1262 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      15 | 1263 | `					bAll = 0;` |
|      15 | 1264 | `					break;` |
|       - | 1265 | `				}` |
|      27 | 1266 | `			}` |
|      35 | 1267 | `			if( bAll ) return SXRET_OK;` |
|       9 | 1268 | `		}` |
|       6 | 1269 | `	}` |
|       - | 1270 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|       - | 1271 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|       - | 1272 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|       - | 1273 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     499 | 1274 | `	for( i = 0; i < nAlts; i++ ){` |
|     337 | 1275 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     304 | 1276 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     181 | 1277 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|       3 | 1278 | `			return SXRET_OK;` |
|       - | 1279 | `		}` |
|     156 | 1280 | `	}` |
|     167 | 1281 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     167 | 1282 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     497 | 1283 | `	for( i = 0; i < nAlts; i++ ){` |
|     335 | 1284 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     307 | 1285 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     261 | 1286 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     261 | 1287 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     261 | 1288 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     133 | 1289 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     101 | 1290 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|     ! 0 | 1291 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     156 | 1292 | `	}` |
|       - | 1293 | `	/* Object handling */` |
|     167 | 1294 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      39 | 1295 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      39 | 1296 | `		if( bHasClassAlt ){` |
|      26 | 1297 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      26 | 1298 | `			ph7_class *pSelfNow = VmCurrentSelf(pVm);` |
|      50 | 1299 | `			for( i = 0; i < nAlts; i++ ){` |
|       - | 1300 | `				ph7_class *pExpected;` |
|      44 | 1301 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      36 | 1302 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      36 | 1303 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelfNow);` |
|      36 | 1304 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      19 | 1305 | `					return SXRET_OK;` |
|       - | 1306 | `				}` |
|      12 | 1307 | `			}` |
|       3 | 1308 | `		}` |
|      22 | 1309 | `		return SXERR_INVALID;` |
|       - | 1310 | `	}` |
|       - | 1311 | `	/* Array handling */` |
|     133 | 1312 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      15 | 1313 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|       - | 1314 | `	}` |
|       - | 1315 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|       - | 1316 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|       - | 1317 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|       - | 1318 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|       - | 1319 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|       - | 1320 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     121 | 1321 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|      16 | 1322 | `		if( bHasFloat ) return SXRET_OK;` |
|       2 | 1323 | `	}` |
|     113 | 1324 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      61 | 1325 | `		if( bHasInt ){` |
|      59 | 1326 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      59 | 1327 | `			return SXRET_OK;` |
|       - | 1328 | `		}` |
|       1 | 1329 | `	}` |
|      59 | 1330 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|      56 | 1331 | `		if( bHasString ) return SXRET_OK;` |
|       8 | 1332 | `	}` |
|      21 | 1333 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1334 | `		if( bHasBool ) return SXRET_OK;` |
|     ! 0 | 1335 | `	}` |
|      21 | 1336 | `	if( bStrict ){` |
|       - | 1337 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|     ! 0 | 1338 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|     ! 0 | 1339 | `			PH7_MemObjToReal(pValue);` |
|     ! 0 | 1340 | `			return SXRET_OK;` |
|       - | 1341 | `		}` |
|     ! 0 | 1342 | `		return SXERR_INVALID;` |
|       - | 1343 | `	}` |
|       - | 1344 | `	/* Weak coercion preference order: int > float > string > bool.` |
|       - | 1345 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|       - | 1346 | `	 * to match PHP's union RFC. */` |
|       - | 1347 | `	{` |
|      21 | 1348 | `		int kind = VmStringNumericKind(pValue);` |
|      21 | 1349 | `		if( bHasInt ){` |
|       - | 1350 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|       - | 1351 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|      18 | 1352 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 | 1353 | `				PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1354 | `				return SXRET_OK;` |
|       - | 1355 | `			}` |
|      18 | 1356 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1357 | `				ph7_real r = pValue->rVal;` |
|     ! 0 | 1358 | `				if( r == (ph7_real)(sxi64)r ){` |
|     ! 0 | 1359 | `					PH7_MemObjToInteger(pValue);` |
|     ! 0 | 1360 | `					return SXRET_OK;` |
|       - | 1361 | `				}` |
|     ! 0 | 1362 | `			}` |
|      18 | 1363 | `			if( kind == 1 ){` |
|       9 | 1364 | `				PH7_MemObjToInteger(pValue);` |
|       9 | 1365 | `				return SXRET_OK;` |
|       - | 1366 | `			}` |
|       4 | 1367 | `		}` |
|      13 | 1368 | `		if( bHasFloat ){` |
|      10 | 1369 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|     ! 0 | 1370 | `				PH7_MemObjToReal(pValue);` |
|     ! 0 | 1371 | `				return SXRET_OK;` |
|       - | 1372 | `			}` |
|      10 | 1373 | `			if( kind == 1 \|\| kind == 2 ){` |
|       7 | 1374 | `				PH7_MemObjToReal(pValue);` |
|       7 | 1375 | `				return SXRET_OK;` |
|       - | 1376 | `			}` |
|       1 | 1377 | `		}` |
|       6 | 1378 | `		if( bHasString ){` |
|     ! 0 | 1379 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|     ! 0 | 1380 | `				PH7_MemObjToString(pValue);` |
|     ! 0 | 1381 | `				return SXRET_OK;` |
|       - | 1382 | `			}` |
|     ! 0 | 1383 | `		}` |
|       6 | 1384 | `		if( bHasBool ){` |
|     ! 0 | 1385 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|     ! 0 | 1386 | `				PH7_MemObjToBool(pValue);` |
|     ! 0 | 1387 | `				return SXRET_OK;` |
|       - | 1388 | `			}` |
|     ! 0 | 1389 | `		}` |
|       - | 1390 | `	}` |
|       6 | 1391 | `	return SXERR_INVALID;` |
|     105 | 1392 | `}` |
|       - | 1393 |  |
|       - | 1394 | `/*` |
|       - | 1395 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|       - | 1396 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|       - | 1397 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|       - | 1398 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|       - | 1399 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|       - | 1400 | ` */` |
|     186 | 1401 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|       5 | 1402 | `{` |
|       - | 1403 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|       - | 1404 | `	 * null value satisfies it (and a null value matches via the flag test` |
|       - | 1405 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|       - | 1406 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|       - | 1407 | `	 * silently swallow any argument. */` |
|     191 | 1408 | `	if( nType == MEMOBJ_NULL ){` |
|       3 | 1409 | `		return SXERR_INVALID;` |
|       - | 1410 | `	}` |
|       - | 1411 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|       - | 1412 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|       - | 1413 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|       - | 1414 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|       - | 1415 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|       - | 1416 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|       - | 1417 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|       - | 1418 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|       - | 1419 | `	 * object value against an array type is caught here too (never valid);` |
|       - | 1420 | `	 * object->scalar stays a separate case handled by the callers. */` |
|     189 | 1421 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|      34 | 1422 | `		return SXERR_INVALID;` |
|       - | 1423 | `	}` |
|     157 | 1424 | `	if( bStrict ){` |
|       - | 1425 | `		/* Only int -> float widening is allowed implicitly. */` |
|      24 | 1426 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|       3 | 1427 | `			PH7_MemObjToReal(pVal);` |
|       3 | 1428 | `			return SXRET_OK;` |
|       - | 1429 | `		}` |
|      22 | 1430 | `		return SXERR_INVALID;` |
|       - | 1431 | `	}` |
|       - | 1432 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|       - | 1433 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|       - | 1434 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|       - | 1435 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|       - | 1436 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|       - | 1437 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|       - | 1438 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|     137 | 1439 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      20 | 1440 | `		return SXERR_INVALID;` |
|       - | 1441 | `	}` |
|       - | 1442 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|       - | 1443 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|       - | 1444 | `	 * is invoked by the string cast below). Every other scalar target —` |
|       - | 1445 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|       - | 1446 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|       - | 1447 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|       - | 1448 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|       - | 1449 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|       - | 1450 | `	 * is caught by the array guard.) */` |
|     119 | 1451 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      20 | 1452 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      24 | 1453 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       8 | 1454 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|      13 | 1455 | `			return SXERR_INVALID;` |
|       - | 1456 | `		}` |
|       3 | 1457 | `	}` |
|     102 | 1458 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      93 | 1459 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      96 | 1460 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      44 | 1461 | `		return SXERR_INVALID;` |
|       - | 1462 | `	}` |
|      67 | 1463 | `	if( nType == MEMOBJ_INT && pVal->pVm ){` |
|       - | 1464 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion` |
|       - | 1465 | `		 * (typed params, returns, typed property stores all funnel through here);` |
|       - | 1466 | `		 * PHL rejects it. SXERR_INVALID routes to the caller's TypeError, exactly` |
|       - | 1467 | `		 * like the null / non-numeric-string cases above. An INTEGRAL float loses` |
|       - | 1468 | `		 * nothing and coerces normally. */` |
|      40 | 1469 | `		if( pVal->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 1470 | `			ph7_real r = pVal->rVal;` |
|     ! 0 | 1471 | `			if( r != (ph7_real)(sxi64)r ){` |
|     ! 0 | 1472 | `				return SXERR_INVALID;` |
|     ! 0 | 1473 | `			}` |
|      40 | 1474 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       - | 1475 | `			SyString sStr;` |
|       - | 1476 | `			ph7_value sProbe;` |
|       - | 1477 | `			int bLossy;` |
|      36 | 1478 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|      36 | 1479 | `			PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|      36 | 1480 | `			PH7_MemObjToNumeric(&sProbe);` |
|      36 | 1481 | `			bLossy = (sProbe.iFlags & MEMOBJ_REAL) && sProbe.rVal != (ph7_real)(sxi64)sProbe.rVal;` |
|      36 | 1482 | `			PH7_MemObjRelease(&sProbe);` |
|      36 | 1483 | `			if( bLossy ){` |
|     ! 0 | 1484 | `				return SXERR_INVALID;` |
|       - | 1485 | `			}` |
|      16 | 1486 | `		}` |
|      18 | 1487 | `	}` |
|       - | 1488 | `	{` |
|      67 | 1489 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      67 | 1490 | `		if( xCast ) xCast(pVal);` |
|       - | 1491 | `	}` |
|      67 | 1492 | `	return SXRET_OK;` |
|      98 | 1493 | `}` |
|       - | 1494 |  |
|       - | 1495 | `/*` |
|       - | 1496 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|       - | 1497 | ` * TypeError message. Prefers the declared textual form when available.` |
|       - | 1498 | ` *` |
|       - | 1499 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|       - | 1500 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|       - | 1501 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|       - | 1502 | ` * back to a static literal and ignore zBuf entirely.` |
|       - | 1503 | ` */` |
|     132 | 1504 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|       5 | 1505 | `{` |
|     137 | 1506 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|     137 | 1507 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|     137 | 1508 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     137 | 1509 | `		if( pDeclared->zString && nCopy > 0 ){` |
|     137 | 1510 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      66 | 1511 | `		}` |
|     137 | 1512 | `		zBuf[nCopy] = 0;` |
|     137 | 1513 | `		return zBuf;` |
|       - | 1514 | `	}` |
|     ! 0 | 1515 | `	switch( nType ){` |
|     ! 0 | 1516 | `		case MEMOBJ_INT:     return "int";` |
|     ! 0 | 1517 | `		case MEMOBJ_REAL:    return "float";` |
|     ! 0 | 1518 | `		case MEMOBJ_STRING:  return "string";` |
|     ! 0 | 1519 | `		case MEMOBJ_BOOL:    return "bool";` |
|     ! 0 | 1520 | `		case MEMOBJ_HASHMAP: return "array";` |
|     ! 0 | 1521 | `		case MEMOBJ_OBJ:     return "object";` |
|     ! 0 | 1522 | `		default:             return "scalar";` |
|       - | 1523 | `	}` |
|      71 | 1524 | `}` |
|       - | 1525 |  |
|       - | 1526 | `/*` |
|       - | 1527 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|       - | 1528 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|       - | 1529 | ` */` |
|      44 | 1530 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|       5 | 1531 | `{` |
|      49 | 1532 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      71 | 1533 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      44 | 1534 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      49 | 1535 | `	return zBuf;` |
|       5 | 1536 | `}` |
|       - | 1537 |  |
| 3007502 | 1538 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit)` |
|       5 | 1539 | `{` |
|       - | 1540 | `	SyHashEntry *pSlot;` |
|       - | 1541 | `	VmClassAttr *pVmAttr;` |
|       - | 1542 | `	ph7_class_attr *pAttr;` |
|       - | 1543 | `	char zGivenBuf[128];` |
| 3007507 | 1544 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
| 3007507 | 1545 | `	if( pSlot == 0 ){` |
| 2906955 | 1546 | `		return SXRET_OK; /* Not a typed slot */` |
|       - | 1547 | `	}` |
|  100557 | 1548 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|  100557 | 1549 | `	pAttr = pVmAttr->pAttr;` |
|  100557 | 1550 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1551 | `		return SXRET_OK;` |
|       - | 1552 | `	}` |
|       - | 1553 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|       - | 1554 | `	 * property may be written exactly once and only from within the declaring` |
|       - | 1555 | `	 * class scope (its set-scope is protected). */` |
|  100557 | 1556 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1557 | `		/* A readonly property is always typed and default-less, so it starts` |
|       - | 1558 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|       - | 1559 | `		 * write below — making it the write-once latch (a type-rejected write` |
|       - | 1560 | `		 * leaves it set, so a later valid initialization still works). */` |
|      83 | 1561 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|       - | 1562 | `			/* Already initialized: any further write is forbidden, any scope —` |
|       - | 1563 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|       - | 1564 | `			 * Exceptions that fall through to the set-scope check below:` |
|       - | 1565 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|       - | 1566 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|       - | 1567 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|      24 | 1568 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      24 | 1569 | `			if( !(pCloneFr && pCloneFr->pThis` |
|      13 | 1570 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|      22 | 1571 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|       - | 1572 | `			}` |
|       1 | 1573 | `		}` |
|      30 | 1574 | `	}` |
|  100539 | 1575 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|       - | 1576 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|       - | 1577 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|      27 | 1578 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|      27 | 1579 | `		if( rcVis != SXRET_OK ){` |
|      13 | 1580 | `			return rcVis;` |
|       1 | 1581 | `		}` |
|  100520 | 1582 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1583 | `		/* First write (or a clone re-init) must come from within the declaring` |
|       - | 1584 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      63 | 1585 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|      63 | 1586 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|       - | 1587 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|       - | 1588 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|       - | 1589 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|      63 | 1590 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|       5 | 1591 | `			pDecl = pVmAttr->pOwner;` |
|       2 | 1592 | `		}` |
|      63 | 1593 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|       6 | 1594 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|       - | 1595 | `		}` |
|      27 | 1596 | `	}` |
|       - | 1597 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|       - | 1598 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|       - | 1599 | `	 * matching PHP's documented behavior. */` |
|  100523 | 1600 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      68 | 1601 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|      42 | 1602 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       - | 1603 | `			0 /* bStrict: properties never apply strict_types */);` |
|      47 | 1604 | `		if( rc == SXRET_OK ){` |
|      34 | 1605 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      34 | 1606 | `			return SXRET_OK;` |
|       - | 1607 | `		}` |
|      16 | 1608 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1609 | `			char zBuf[128];` |
|      12 | 1610 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       3 | 1611 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 1612 | `		}` |
|       8 | 1613 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1614 | `	}` |
|       - | 1615 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|       - | 1616 | `	 * includes null). */` |
|  100481 | 1617 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      28 | 1618 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|      24 | 1619 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       2 | 1620 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|      21 | 1621 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      21 | 1622 | `			return SXRET_OK;` |
|       - | 1623 | `		}` |
|      12 | 1624 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|       - | 1625 | `	}` |
|       - | 1626 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|       - | 1627 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|       - | 1628 | `	 * type error. */` |
|  100453 | 1629 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 1630 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1631 | `	}` |
|       - | 1632 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|       - | 1633 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|       - | 1634 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|  100453 | 1635 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      12 | 1636 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       5 | 1637 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       5 | 1638 | `			return SXRET_OK;` |
|       - | 1639 | `		}` |
|       7 | 1640 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1641 | `	}` |
|       - | 1642 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|       - | 1643 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|       - | 1644 | `	 * handled by the nullable check above). Checked by value before the generic` |
|       - | 1645 | `	 * class-instanceof branch, which would resolve no such class and then` |
|       - | 1646 | `	 * wrongly accept any object / reject arrays. */` |
|  100443 | 1647 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      55 | 1648 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|      55 | 1649 | `		if( rcPseudo == 1 ){` |
|      11 | 1650 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      11 | 1651 | `			return SXRET_OK;` |
|       - | 1652 | `		}` |
|      45 | 1653 | `		if( rcPseudo == 0 ){` |
|       3 | 1654 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1655 | `		}` |
|       - | 1656 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|      20 | 1657 | `	}` |
|  100431 | 1658 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       - | 1659 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|       - | 1660 | `		 * currently active on the self-stack. */` |
|      43 | 1661 | `		ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,VmCurrentSelf(pVm));` |
|      43 | 1662 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1663 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1664 | `		}` |
|      43 | 1665 | `		if( pExpected ){` |
|      39 | 1666 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      39 | 1667 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       - | 1668 | `				char zBuf[128];` |
|      15 | 1669 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       4 | 1670 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 1671 | `			}` |
|      14 | 1672 | `		}` |
|      35 | 1673 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|      35 | 1674 | `		return SXRET_OK;` |
|       - | 1675 | `	}` |
|       - | 1676 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|       - | 1677 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|       - | 1678 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|       - | 1679 | `	 * via __toString (typed property stores are always weak mode), so fall` |
|       - | 1680 | `	 * through to the string cast below. */` |
|  100391 | 1681 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      17 | 1682 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      19 | 1683 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       4 | 1684 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       - | 1685 | `			char zBuf[128];` |
|      21 | 1686 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       6 | 1687 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 1688 | `		}` |
|       1 | 1689 | `	}` |
|  100379 | 1690 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|  100061 | 1691 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|  100061 | 1692 | `		if( xCast ){` |
|       - | 1693 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|  100061 | 1694 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       8 | 1695 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1696 | `			}` |
|  100054 | 1697 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       6 | 1698 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)));` |
|       - | 1699 | `			}` |
|       - | 1700 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|       - | 1701 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|       - | 1702 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|  100046 | 1703 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|  100043 | 1704 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|  100047 | 1705 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|  100032 | 1706 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|       - | 1707 | `			}` |
|      21 | 1708 | `			xCast(pValue);` |
|       9 | 1709 | `		}` |
|      12 | 1710 | `	}else{` |
|       - | 1711 | `		/* Mask matched — an int property accepting a whole-real must` |
|       - | 1712 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|     323 | 1713 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       - | 1714 | `	}` |
|     341 | 1715 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     341 | 1716 | `	return SXRET_OK;` |
| 1503756 | 1717 | `}` |
|       - | 1718 | `/*` |
|       - | 1719 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|       - | 1720 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|       - | 1721 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|       - | 1722 | ` *     a scope that could normally reach it — else a catchable Error),` |
|       - | 1723 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|       - | 1724 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|       - | 1725 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|       - | 1726 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|       - | 1727 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|       - | 1728 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|       - | 1729 | ` */` |
|      18 | 1730 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|       - | 1731 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|       1 | 1732 | `{` |
|      19 | 1733 | `	ph7_class *pClass = pClone->pClass;` |
|       - | 1734 | `	SyHashEntry *pEntry;` |
|       - | 1735 | `	VmClassAttr *pVmAttr;` |
|       - | 1736 | `	ph7_class_attr *pAttr;` |
|       - | 1737 | `	ph7_value *pSlot;` |
|       - | 1738 | `	sxi32 rc;` |
|      19 | 1739 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|      19 | 1740 | `	if( pEntry == 0 ){` |
|       - | 1741 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|       - | 1742 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|       - | 1743 | `		 * deferred residual). */` |
|     ! 0 | 1744 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|     ! 0 | 1745 | `		if( pSlot == 0 ){` |
|     ! 0 | 1746 | `			return PH7_VmMemoryError(pVm);` |
|       - | 1747 | `		}` |
|     ! 0 | 1748 | `		PH7_MemObjStore(pValue,pSlot);` |
|     ! 0 | 1749 | `		return SXRET_OK;` |
|       - | 1750 | `	}` |
|      19 | 1751 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      19 | 1752 | `	pAttr = pVmAttr->pAttr;` |
|       - | 1753 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|      19 | 1754 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - | 1755 | `		SyBlob sMsg;` |
|     ! 0 | 1756 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     ! 0 | 1757 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|     ! 0 | 1758 | `			&pClass->sName,&pAttr->sName);` |
|     ! 0 | 1759 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 1760 | `	}` |
|       - | 1761 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|       - | 1762 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|       - | 1763 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|      19 | 1764 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       3 | 1765 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|       3 | 1766 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       - | 1767 | `		SyBlob sMsg;` |
|       3 | 1768 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 1769 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|       3 | 1770 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       - | 1771 | `	}` |
|       - | 1772 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|      17 | 1773 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,1 /* bCloneInit */);` |
|      17 | 1774 | `	if( rc != SXRET_OK ){` |
|       3 | 1775 | `		return rc;` |
|       - | 1776 | `	}` |
|       - | 1777 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|      15 | 1778 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|      15 | 1779 | `	if( pSlot ){` |
|      15 | 1780 | `		PH7_MemObjStore(pValue,pSlot);` |
|       7 | 1781 | `	}` |
|      15 | 1782 | `	return SXRET_OK;` |
|      10 | 1783 | `}` |
|       - | 1784 | `/*` |
|       - | 1785 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|       - | 1786 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|       - | 1787 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|       - | 1788 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|       - | 1789 | ` */` |
|       4 | 1790 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       2 | 1791 | `{` |
|       6 | 1792 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1793 | `	char zBuf[128];` |
|       - | 1794 | `	const char *zGiven;` |
|       6 | 1795 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1796 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 1797 | `	}else{` |
|       6 | 1798 | `		zGiven = ph7_type_name(pValue);` |
|       - | 1799 | `	}` |
|       - | 1800 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|       - | 1801 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|       - | 1802 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|       - | 1803 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|       - | 1804 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|       - | 1805 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|       - | 1806 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       6 | 1807 | `	if( pVm->sCodeGen.xErr ){` |
|       4 | 1808 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|       - | 1809 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|       1 | 1810 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       2 | 1811 | `	}else{` |
|       4 | 1812 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1813 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|       1 | 1814 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       - | 1815 | `	}` |
|       6 | 1816 | `	pVm->iExitStatus = 255;` |
|       6 | 1817 | `	pVm->bHaltRequested = 1;` |
|       6 | 1818 | `	return SXERR_ABORT;` |
|       2 | 1819 | `}` |
|       - | 1820 | `/*` |
|       - | 1821 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|       - | 1822 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|       - | 1823 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|       - | 1824 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|       - | 1825 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|       - | 1826 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|       - | 1827 | ` */` |
|      34 | 1828 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       3 | 1829 | `{` |
|      37 | 1830 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|       - | 1831 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|      37 | 1832 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 1833 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       3 | 1834 | `			return SXRET_OK;` |
|       - | 1835 | `		}` |
|     ! 0 | 1836 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|     ! 0 | 1837 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 1838 | `			return SXRET_OK;` |
|       - | 1839 | `		}` |
|     ! 0 | 1840 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1841 | `	}` |
|       - | 1842 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|      35 | 1843 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       5 | 1844 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){` |
|       5 | 1845 | `			return SXRET_OK;` |
|       - | 1846 | `		}` |
|     ! 0 | 1847 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1848 | `	}` |
|       - | 1849 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|      31 | 1850 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 1851 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1852 | `	}` |
|       - | 1853 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|      31 | 1854 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 1855 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1856 | `			return SXRET_OK;` |
|       - | 1857 | `		}` |
|     ! 0 | 1858 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1859 | `	}` |
|       - | 1860 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|       - | 1861 | `	 * a real class/interface verified by instanceof. */` |
|      31 | 1862 | `	if( pAttr->nType == SXU32_HIGH ){` |
|     ! 0 | 1863 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|     ! 0 | 1864 | `		if( rcPseudo == 1 ){` |
|     ! 0 | 1865 | `			return SXRET_OK;` |
|       - | 1866 | `		}` |
|     ! 0 | 1867 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 1868 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1869 | `		}` |
|       - | 1870 | `		/* rcPseudo == -1: a real class/interface type. */` |
|     ! 0 | 1871 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1872 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1873 | `		}` |
|       - | 1874 | `		{` |
|       - | 1875 | `			/* A class constant's self/parent resolve against the declaring class. */` |
|     ! 0 | 1876 | `			ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,pClass);` |
|     ! 0 | 1877 | `			if( pExpected ){` |
|     ! 0 | 1878 | `				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     ! 0 | 1879 | `				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|     ! 0 | 1880 | `					return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|       - | 1881 | `				}` |
|     ! 0 | 1882 | `			}` |
|       - | 1883 | `		}` |
|     ! 0 | 1884 | `		return SXRET_OK;` |
|       - | 1885 | `	}` |
|       - | 1886 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|       - | 1887 | `	 * implicit widening. Everything else is a type error.` |
|       - | 1888 | `	 *` |
|       - | 1889 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|       - | 1890 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|       - | 1891 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|       - | 1892 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|       - | 1893 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|       - | 1894 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|       - | 1895 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|       - | 1896 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|       - | 1897 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|       - | 1898 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|       - | 1899 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|      31 | 1900 | `	if( pValue->iFlags & pAttr->nType ){` |
|      23 | 1901 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      23 | 1902 | `		return SXRET_OK;` |
|       - | 1903 | `	}` |
|       9 | 1904 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       3 | 1905 | `		PH7_MemObjToReal(pValue);` |
|       3 | 1906 | `		return SXRET_OK;` |
|       - | 1907 | `	}` |
|       6 | 1908 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      20 | 1909 | `}` |
|       - | 1910 | `/*` |
|       - | 1911 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|       - | 1912 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|       - | 1913 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|       - | 1914 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|       - | 1915 | ` */` |
|      28 | 1916 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       2 | 1917 | `{` |
|      30 | 1918 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       - | 1919 | `	const char *zGiven;` |
|       - | 1920 | `	char zBuf[128];` |
|       - | 1921 | `	SyBlob sMsg;` |
|      30 | 1922 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1923 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|     ! 0 | 1924 | `	}else{` |
|      30 | 1925 | `		zGiven = ph7_type_name(pValue);` |
|       - | 1926 | `	}` |
|      30 | 1927 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      30 | 1928 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|      14 | 1929 | `		zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|      30 | 1930 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       2 | 1931 | `}` |
|       - | 1932 | `/*` |
|       - | 1933 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|       - | 1934 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|       - | 1935 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|       - | 1936 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|       - | 1937 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|       - | 1938 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|       - | 1939 | ` * instance defaults per-instantiation. Matching structure of` |
|       - | 1940 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|       - | 1941 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|       - | 1942 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|       - | 1943 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|       - | 1944 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|       - | 1945 | ` */` |
|       - | 1946 | `/*` |
|       - | 1947 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|       - | 1948 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|       - | 1949 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|       - | 1950 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|       - | 1951 | ` * throwing, so the static-property mount path can defer the failure (php` |
|       - | 1952 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|       - | 1953 | ` * instance path throws immediately via the wrapper below.` |
|       - | 1954 | ` */` |
|     288 | 1955 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 1956 | `{` |
|     293 | 1957 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|     293 | 1958 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      51 | 1959 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      49 | 1960 | `			return SXRET_OK;` |
|       - | 1961 | `		}` |
|       2 | 1962 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       1 | 1963 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|     ! 0 | 1964 | `			return SXRET_OK;` |
|       - | 1965 | `		}` |
|       3 | 1966 | `		return SXERR_INVALID;` |
|       - | 1967 | `	}` |
|     245 | 1968 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      19 | 1969 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){` |
|      19 | 1970 | `			return SXRET_OK;` |
|       - | 1971 | `		}` |
|     ! 0 | 1972 | `		return SXERR_INVALID;` |
|       - | 1973 | `	}` |
|     231 | 1974 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|     ! 0 | 1975 | `		return SXERR_INVALID;` |
|       - | 1976 | `	}` |
|     231 | 1977 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|     ! 0 | 1978 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 1979 | `			return SXRET_OK;` |
|       - | 1980 | `		}` |
|     ! 0 | 1981 | `		return SXERR_INVALID;` |
|       - | 1982 | `	}` |
|     231 | 1983 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       5 | 1984 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       5 | 1985 | `		if( rcPseudo == 1 ){` |
|       5 | 1986 | `			return SXRET_OK;` |
|       - | 1987 | `		}` |
|     ! 0 | 1988 | `		if( rcPseudo == 0 ){` |
|     ! 0 | 1989 | `			return SXERR_INVALID;` |
|       - | 1990 | `		}` |
|     ! 0 | 1991 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1992 | `			return SXERR_INVALID;` |
|       - | 1993 | `		}` |
|       - | 1994 | `		{` |
|       - | 1995 | `			/* self/parent in the hint resolve against the declaring class. */` |
|     ! 0 | 1996 | `			ph7_class *pExpected = VmResolveTypeClass(pVm,&pAttr->sClass,` |
|     ! 0 | 1997 | `				pAttr->pDeclClass ? pAttr->pDeclClass : pClass);` |
|     ! 0 | 1998 | `			if( pExpected ){` |
|     ! 0 | 1999 | `				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|     ! 0 | 2000 | `				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|     ! 0 | 2001 | `					return SXERR_INVALID;` |
|       - | 2002 | `				}` |
|     ! 0 | 2003 | `			}` |
|       - | 2004 | `		}` |
|     ! 0 | 2005 | `		return SXRET_OK;` |
|       - | 2006 | `	}` |
|     227 | 2007 | `	if( pValue->iFlags & pAttr->nType ){` |
|     195 | 2008 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|     195 | 2009 | `		return SXRET_OK;` |
|       - | 2010 | `	}` |
|      34 | 2011 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       6 | 2012 | `		PH7_MemObjToReal(pValue);` |
|       6 | 2013 | `		return SXRET_OK;` |
|       - | 2014 | `	}` |
|      30 | 2015 | `	return SXERR_INVALID;` |
|     149 | 2016 | `}` |
|     250 | 2017 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|       5 | 2018 | `{` |
|     255 | 2019 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|     243 | 2020 | `		return SXRET_OK;` |
|       - | 2021 | `	}` |
|      13 | 2022 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|     130 | 2023 | `}` |
|       - | 2024 | `/*` |
|       - | 2025 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|       - | 2026 | ` * class chain for a static typed slot whose mount-time default failed its` |
|       - | 2027 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|       - | 2028 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|       - | 2029 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|       - | 2030 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|       - | 2031 | ` * never-touched class stays silent, and the throw repeats on every access` |
|       - | 2032 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|       - | 2033 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|       - | 2034 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|       - | 2035 | ` */` |
|      16 | 2036 | `PH7_PRIVATE sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|       1 | 2037 | `{` |
|       - | 2038 | `	ph7_class *pScan;` |
|      17 | 2039 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|       - | 2040 | `		SyHashEntry *pEntry;` |
|      17 | 2041 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|      17 | 2042 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|      17 | 2043 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      16 | 2044 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|       - | 2045 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|      16 | 2046 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      17 | 2047 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|      17 | 2048 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      17 | 2049 | `				if( pSlot ){` |
|      17 | 2050 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      17 | 2051 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|      17 | 2052 | `						ph7_value *pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       - | 2053 | `						ph7_value sNull;` |
|      17 | 2054 | `						if( pValue == 0 ){` |
|     ! 0 | 2055 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|     ! 0 | 2056 | `							pValue = &sNull;` |
|     ! 0 | 2057 | `						}` |
|      17 | 2058 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|       - | 2059 | `					}` |
|     ! 0 | 2060 | `				}` |
|     ! 0 | 2061 | `			}` |
|     ! 0 | 2062 | `		}` |
|     ! 0 | 2063 | `	}` |
|     ! 0 | 2064 | `	return SXRET_OK;` |
|       9 | 2065 | `}` |
|       - | 2066 |  |
|       - | 2067 | `/*` |
|       - | 2068 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 2069 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 2070 | ` * information.` |
|       - | 2071 | ` * ------------------------------------` |
|       - | 2072 | ` * Simple boring wrapper function.` |
|       - | 2073 | ` * ------------------------------------` |
|       - | 2074 | ` */` |
|     120 | 2075 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|       5 | 2076 | `{` |
|       - | 2077 | `	va_list ap;` |
|       - | 2078 | `	sxi32 rc;` |
|     125 | 2079 | `	va_start(ap,zFormat);` |
|     125 | 2080 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     125 | 2081 | `	va_end(ap);` |
|     125 | 2082 | `	return rc;` |
|       5 | 2083 | `}` |
|       - | 2084 | `/*` |
|       - | 2085 | ` * Throw a TypeError exception from within the VM execution loop.` |
|       - | 2086 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|       - | 2087 | ` */` |
|     226 | 2088 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|       5 | 2089 | `{` |
|       - | 2090 | `	ph7_class *pClass;` |
|       - | 2091 | `	ph7_class_instance *pThis;` |
|       - | 2092 | `	ph7_class_method *pCons;` |
|       - | 2093 | `	ph7_value sArg;` |
|       - | 2094 | `	ph7_value *apArg[1];` |
|       - | 2095 | `	SyBlob sMsg;` |
|       - | 2096 | `	SyString sMsgStr;` |
|     231 | 2097 | `	SyString *pFuncName = &pCallee->sName;` |
|       - | 2098 | `	VmFrame *pFrame;` |
|       - | 2099 | `	sxi32 rc;` |
|     231 | 2100 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|     231 | 2101 | `	if( pClass == 0 ){` |
|     ! 0 | 2102 | `		return PH7_ABORT;` |
|       - | 2103 | `	}` |
|     231 | 2104 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|     231 | 2105 | `	if( pThis == 0 ){` |
|     ! 0 | 2106 | `		return PH7_ABORT;` |
|       - | 2107 | `	}` |
|     231 | 2108 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2109 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|       - | 2110 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|       - | 2111 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|       - | 2112 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|       - | 2113 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|     231 | 2114 | `	if( pOwnerClass ){` |
|      32 | 2115 | `		if( pArgName ){` |
|      25 | 2116 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|      11 | 2117 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|      14 | 2118 | `		}else{` |
|       9 | 2119 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|       3 | 2120 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|       - | 2121 | `		}` |
|      18 | 2122 | `	}else{` |
|       - | 2123 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|     203 | 2124 | `		const char *zShow = 0;` |
|     203 | 2125 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     203 | 2126 | `		if( pArgName ){` |
|     141 | 2127 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|      68 | 2128 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|      73 | 2129 | `		}else{` |
|      66 | 2130 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      31 | 2131 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|       - | 2132 | `		}` |
|       - | 2133 | `	}` |
|       - | 2134 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|       - | 2135 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|       - | 2136 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|     231 | 2137 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|     221 | 2138 | `		SyString *pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     221 | 2139 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|     221 | 2140 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,pVm->nCurLine);` |
|     108 | 2141 | `		}` |
|     108 | 2142 | `	}` |
|     231 | 2143 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     231 | 2144 | `	if( pCons ){` |
|     231 | 2145 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|     231 | 2146 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|     231 | 2147 | `		apArg[0] = &sArg;` |
|     231 | 2148 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|     231 | 2149 | `		PH7_MemObjRelease(&sArg);` |
|     113 | 2150 | `	}` |
|     231 | 2151 | `	SyBlobRelease(&sMsg);` |
|     231 | 2152 | `	pFrame = pVm->pFrame;` |
|     231 | 2153 | `	if( pFrame ){` |
|     231 | 2154 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     231 | 2155 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     113 | 2156 | `	}` |
|     231 | 2157 | `	rc = VmThrowException(&(*pVm),pThis);` |
|     231 | 2158 | `	PH7_ClassInstanceUnref(pThis);` |
|     231 | 2159 | `	if( rc == SXERR_ABORT ){` |
|       6 | 2160 | `		return PH7_ABORT;` |
|       - | 2161 | `	}` |
|     227 | 2162 | `	return PH7_EXCEPTION;` |
|     118 | 2163 | `}` |
|       - | 2164 | `/*` |
|       - | 2165 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|       - | 2166 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|       - | 2167 | ` * positional and the named-argument binding paths of OP_CALL.` |
|       - | 2168 | ` *` |
|       - | 2169 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|       - | 2170 | ` * element uses its overall call position; a NAMED element always reports` |
|       - | 2171 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|       - | 2172 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|       - | 2173 | ` * formal, so no single parameter name applies.` |
|       - | 2174 | ` *` |
|       - | 2175 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|       - | 2176 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|       - | 2177 | ` */` |
|    2076 | 2178 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|       - | 2179 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|       5 | 2180 | `{` |
|       - | 2181 | `	sxi32 rc;` |
|    2081 | 2182 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|      30 | 2183 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|      33 | 2184 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict) != SXRET_OK ){` |
|       - | 2185 | `			const char *zGiven;` |
|       8 | 2186 | `			const char *zExpected = "union";` |
|       - | 2187 | `			char zBuf[128];` |
|       - | 2188 | `			char zTypeBuf[128];` |
|       8 | 2189 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 2190 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       8 | 2191 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2192 | `				zGiven = "null";` |
|     ! 0 | 2193 | `			}else{` |
|       8 | 2194 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 2195 | `			}` |
|       8 | 2196 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       8 | 2197 | `				zExpected = VmSyStringToCStr(&pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|       3 | 2198 | `			}` |
|       8 | 2199 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       8 | 2200 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2201 | `		}` |
|      16 | 2202 | `		return SXRET_OK;` |
|       - | 2203 | `	}` |
|    2056 | 2204 | `	if( pFormal->nType < 1` |
|    1131 | 2205 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|    1877 | 2206 | `		return SXRET_OK;` |
|       - | 2207 | `	}` |
|     189 | 2208 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2209 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|       - | 2210 | `		 * per element exactly like the non-variadic paths. */` |
|      47 | 2211 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2212 | `		ph7_class *pClass;` |
|      47 | 2213 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      47 | 2214 | `		if( rcPseudo == 0 ){` |
|       - | 2215 | `			/* Recognised pseudo-type; value mismatches */` |
|       - | 2216 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       7 | 2217 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       2 | 2218 | `				VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|       2 | 2219 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       5 | 2220 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2221 | `		}` |
|       - | 2222 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class.` |
|       - | 2223 | ``		 * Resolve via VmResolveTypeClass so `self`/`parent` resolve and`` |
|       - | 2224 | `		 * interface/abstract hints are included (iLoadable=FALSE); an` |
|       - | 2225 | `		 * unresolvable name is accepted, like the non-variadic paths. */` |
|      43 | 2226 | `		pClass = (rcPseudo == 1) ? 0 : VmResolveTypeClass(&(*pVm),pName,pSelfHint);` |
|      43 | 2227 | `		if( pClass ){` |
|       - | 2228 | `			/* Non-nullable here (the guard above skips nullable+null), so ANY` |
|       - | 2229 | `			 * non-object is a TypeError, matching php (&& short-circuits so` |
|       - | 2230 | `			 * instanceof only derefs a real object). */` |
|      57 | 2231 | `			int bBad = !((pVal->iFlags & MEMOBJ_OBJ)` |
|      28 | 2232 | `				&& PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pClass));` |
|      35 | 2233 | `			if( bBad ){` |
|       - | 2234 | `				char zTypeBuf[128],zGivenBuf[128];` |
|      33 | 2235 | `				rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      16 | 2236 | `					VmSyStringToCStr(&pClass->sName,zTypeBuf,sizeof(zTypeBuf)),` |
|       8 | 2237 | `					VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      17 | 2238 | `				return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2239 | `			}` |
|       9 | 2240 | `		}` |
|      27 | 2241 | `		return SXRET_OK;` |
|       - | 2242 | `	}` |
|     143 | 2243 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|      63 | 2244 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|       - | 2245 | `			char zGivenBuf[128];` |
|       8 | 2246 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       2 | 2247 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       6 | 2248 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2249 | `		}` |
|      59 | 2250 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|       - | 2251 | `			char zTypeBuf[128];` |
|       - | 2252 | `			char zGivenBuf[128];` |
|      60 | 2253 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|      19 | 2254 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      19 | 2255 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      41 | 2256 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|       - | 2257 | `		}` |
|      11 | 2258 | `	}else{` |
|       - | 2259 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|       - | 2260 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      83 | 2261 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2262 | `	}` |
|     101 | 2263 | `	return SXRET_OK;` |
|    1043 | 2264 | `}` |
|       - | 2265 | `/*` |
|       - | 2266 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|       - | 2267 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|       - | 2268 | ` * before a required parameter as implicitly required), excluding a trailing` |
|       - | 2269 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|       - | 2270 | ` * pick php's wording — "exactly N expected" when required == total,` |
|       - | 2271 | ` * "at least N" when trailing optionals exist.` |
|       - | 2272 | ` */` |
| 1451676 | 2273 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|       5 | 2274 | `{` |
| 1451681 | 2275 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
| 1451681 | 2276 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
| 1451681 | 2277 | `	sxu32 nRequired = 0;` |
|       - | 2278 | `	sxu32 n;` |
| 1451681 | 2279 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     357 | 2280 | `		nFormal--;` |
|     176 | 2281 | `	}` |
| 5813317 | 2282 | `	for( n = 0 ; n < nFormal ; ++n ){` |
| 4361641 | 2283 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    5033 | 2284 | `			nRequired = n + 1;` |
|    2514 | 2285 | `		}` |
| 2180823 | 2286 | `	}` |
| 1451681 | 2287 | `	*pnNonVariadic = nFormal;` |
| 1451681 | 2288 | `	return nRequired;` |
|       5 | 2289 | `}` |
|       - | 2290 | `/*` |
|       - | 2291 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|       - | 2292 | ` * with too few arguments:` |
|       - | 2293 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|       - | 2294 | ` *   {exactly\|at least} M expected` |
|       - | 2295 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|       - | 2296 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|       - | 2297 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|       - | 2298 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|       - | 2299 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|       - | 2300 | ` */` |
|      26 | 2301 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2302 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|       3 | 2303 | `{` |
|       - | 2304 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|       - | 2305 | `	SyBlob sMsg;` |
|      29 | 2306 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      29 | 2307 | `	if( pOwnerClass ){` |
|       5 | 2308 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z::%z(), %u passed",` |
|       2 | 2309 | `			&pOwnerClass->sName,pFuncName,nPassed);` |
|       3 | 2310 | `	}else{` |
|      25 | 2311 | `		SyBlobFormat(&sMsg,"Too few arguments to function %z(), %u passed",pFuncName,nPassed);` |
|       - | 2312 | `	}` |
|      29 | 2313 | `	if( bCallSite ){` |
|      27 | 2314 | `		const SyString *pFile = (const SyString *)SySetPeek(&pVm->aFiles);` |
|      27 | 2315 | `		SyBlobFormat(&sMsg," in %z on line %d",pFile ? pFile : &sUnknown,1);` |
|      12 | 2316 | `	}` |
|      29 | 2317 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|      13 | 2318 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|       - | 2319 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      29 | 2320 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       3 | 2321 | `}` |
|       - | 2322 | `/*` |
|       - | 2323 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|       - | 2324 | ` * called with too few arguments, in php's ZPP wording:` |
|       - | 2325 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|       - | 2326 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|       - | 2327 | ` * pluralized on the expected count). Hosted builtin FUNCTIONS are not routed` |
|       - | 2328 | ` * here: their PHL signatures don't always mirror php's true arity (e.g.` |
|       - | 2329 | ` * array_unshift is (&$pArray)+func_get_args for php's (array, ...$values)),` |
|       - | 2330 | ` * so their in-body self-checks own the message.` |
|       - | 2331 | ` */` |
|       2 | 2332 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2333 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic)` |
|       1 | 2334 | `{` |
|       - | 2335 | `	SyBlob sMsg;` |
|       3 | 2336 | `	const char *zKind = (nRequired >= nNonVariadic) ? "exactly" : "at least";` |
|       3 | 2337 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       3 | 2338 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 2339 | `	if( pOwnerClass ){` |
|       3 | 2340 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|       1 | 2341 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       2 | 2342 | `	}else{` |
|     ! 0 | 2343 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|     ! 0 | 2344 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|       - | 2345 | `	}` |
|       - | 2346 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       3 | 2347 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 2348 | `}` |
|       - | 2349 | `/*` |
|       - | 2350 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|       - | 2351 | ` * named or positional argument resolved to:` |
|       - | 2352 | ` *   C::f(): Argument #N ($x) not passed` |
|       - | 2353 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|       - | 2354 | ` */` |
|       2 | 2355 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|       - | 2356 | `	sxu32 nArg,SyString *pArgName)` |
|       1 | 2357 | `{` |
|       - | 2358 | `	SyBlob sMsg;` |
|       3 | 2359 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 2360 | `	if( pOwnerClass ){` |
|     ! 0 | 2361 | `		SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) not passed",` |
|     ! 0 | 2362 | `			&pOwnerClass->sName,pFuncName,nArg,pArgName);` |
|     ! 0 | 2363 | `	}else{` |
|       3 | 2364 | `		SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) not passed",pFuncName,nArg,pArgName);` |
|       - | 2365 | `	}` |
|       - | 2366 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       3 | 2367 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       1 | 2368 | `}` |
|       - | 2369 | `/*` |
|       - | 2370 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|       - | 2371 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|       - | 2372 | ` */` |
|       - | 2373 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|       - | 2374 | ` * The message is copied into the instance by __construct, so the caller owns` |
|       - | 2375 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|      34 | 2376 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|       5 | 2377 | `{` |
|       - | 2378 | `	ph7_class *pClass;` |
|       - | 2379 | `	ph7_class_instance *pThis;` |
|       - | 2380 | `	ph7_class_method *pCons;` |
|       - | 2381 | `	ph7_value sArg;` |
|       - | 2382 | `	ph7_value *apArg[1];` |
|       - | 2383 | `	SyString sMsgStr;` |
|       - | 2384 | `	VmFrame *pFrame;` |
|       - | 2385 | `	sxi32 rc;` |
|      39 | 2386 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      39 | 2387 | `	if( pClass == 0 ){` |
|     ! 0 | 2388 | `		return PH7_ABORT;` |
|       - | 2389 | `	}` |
|      39 | 2390 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      39 | 2391 | `	if( pThis == 0 ){` |
|     ! 0 | 2392 | `		return PH7_ABORT;` |
|       - | 2393 | `	}` |
|      39 | 2394 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      39 | 2395 | `	if( pCons ){` |
|      39 | 2396 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      39 | 2397 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      39 | 2398 | `		apArg[0] = &sArg;` |
|      39 | 2399 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      39 | 2400 | `		PH7_MemObjRelease(&sArg);` |
|      17 | 2401 | `	}` |
|      39 | 2402 | `	pFrame = pVm->pFrame;` |
|      39 | 2403 | `	if( pFrame ){` |
|      39 | 2404 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      39 | 2405 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      17 | 2406 | `	}` |
|      39 | 2407 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      39 | 2408 | `	PH7_ClassInstanceUnref(pThis);` |
|      39 | 2409 | `	if( rc == SXERR_ABORT ){` |
|       9 | 2410 | `		return PH7_ABORT;` |
|       - | 2411 | `	}` |
|      32 | 2412 | `	return PH7_EXCEPTION;` |
|      22 | 2413 | `}` |
|      32 | 2414 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|       5 | 2415 | `{` |
|       - | 2416 | `	SyBlob sMsg;` |
|       - | 2417 | `	sxi32 rc;` |
|      37 | 2418 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      37 | 2419 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|      16 | 2420 | `		pFuncName,zExpected,zGiven);` |
|      37 | 2421 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      37 | 2422 | `	SyBlobRelease(&sMsg);` |
|      37 | 2423 | `	return rc;` |
|       5 | 2424 | `}` |
|       - | 2425 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|       - | 2426 | `` * explicit `return` at compile time, so this fires only for an implicit return. */`` |
|       2 | 2427 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,SyString *pFuncName)` |
|       1 | 2428 | `{` |
|       - | 2429 | `	SyBlob sMsg;` |
|       - | 2430 | `	sxi32 rc;` |
|       3 | 2431 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 2432 | `	SyBlobFormat(&sMsg,"%z(): never-returning function must not implicitly return",` |
|       1 | 2433 | `		pFuncName);` |
|       3 | 2434 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|       3 | 2435 | `	SyBlobRelease(&sMsg);` |
|       3 | 2436 | `	return rc;` |
|       1 | 2437 | `}` |
|       - | 2438 | `/*` |
|       - | 2439 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|       - | 2440 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|       - | 2441 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|       - | 2442 | ` */` |
|     390 | 2443 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|       5 | 2444 | `{` |
|     395 | 2445 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      58 | 2446 | `		return pVal->x.iVal ? "true" : "false";` |
|       - | 2447 | `	}` |
|     341 | 2448 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      45 | 2449 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      45 | 2450 | `		if( pThis && pThis->pClass ){` |
|      45 | 2451 | `			SyString *pName = &pThis->pClass->sName;` |
|      45 | 2452 | `			sxu32 n = pName->nByte;` |
|      45 | 2453 | `			if( n >= nBuf ){` |
|     ! 0 | 2454 | `				n = nBuf - 1;` |
|     ! 0 | 2455 | `			}` |
|      45 | 2456 | `			SyMemcpy(pName->zString,zBuf,n);` |
|      45 | 2457 | `			zBuf[n] = 0;` |
|      45 | 2458 | `			return zBuf;` |
|       - | 2459 | `		}` |
|     ! 0 | 2460 | `		return "object";` |
|       - | 2461 | `	}` |
|     301 | 2462 | `	return ph7_type_name(pVal);` |
|     200 | 2463 | `}` |
|       - | 2464 | `/*` |
|       - | 2465 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|       - | 2466 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|       - | 2467 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|       - | 2468 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|       - | 2469 | ` */` |
|      18 | 2470 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|       3 | 2471 | `{` |
|       - | 2472 | `	ph7_class *pClass;` |
|       - | 2473 | `	ph7_class_instance *pThis;` |
|       - | 2474 | `	ph7_class_method *pCons;` |
|       - | 2475 | `	ph7_value sArg;` |
|       - | 2476 | `	ph7_value *apArg[1];` |
|       - | 2477 | `	SyBlob sMsg;` |
|       - | 2478 | `	SyString sMsgStr;` |
|       - | 2479 | `	VmFrame *pFrame;` |
|       - | 2480 | `	sxi32 rc;` |
|      21 | 2481 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|       - | 2482 | `	char zNameBuf[64];` |
|      21 | 2483 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|      21 | 2484 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|      21 | 2485 | `	if( pClass == 0 ){` |
|     ! 0 | 2486 | `		return PH7_ABORT;` |
|       - | 2487 | `	}` |
|      21 | 2488 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      21 | 2489 | `	if( pThis == 0 ){` |
|     ! 0 | 2490 | `		return PH7_ABORT;` |
|       - | 2491 | `	}` |
|      21 | 2492 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      21 | 2493 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|      21 | 2494 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      21 | 2495 | `	if( pCons ){` |
|      21 | 2496 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      21 | 2497 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      21 | 2498 | `		apArg[0] = &sArg;` |
|      21 | 2499 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      21 | 2500 | `		PH7_MemObjRelease(&sArg);` |
|       9 | 2501 | `	}` |
|      21 | 2502 | `	SyBlobRelease(&sMsg);` |
|      21 | 2503 | `	pFrame = pVm->pFrame;` |
|      21 | 2504 | `	if( pFrame ){` |
|      21 | 2505 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      21 | 2506 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       9 | 2507 | `	}` |
|      21 | 2508 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      21 | 2509 | `	PH7_ClassInstanceUnref(pThis);` |
|      21 | 2510 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2511 | `		return PH7_ABORT;` |
|       - | 2512 | `	}` |
|      21 | 2513 | `	return PH7_EXCEPTION;` |
|      12 | 2514 | `}` |
|       - | 2515 | `/*` |
|       - | 2516 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|       - | 2517 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|       - | 2518 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|       - | 2519 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|       - | 2520 | ` */` |
|       - | 2521 | `/*` |
|       - | 2522 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|       - | 2523 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|       - | 2524 | ` * null SyString yields an empty C string. Returns zBuf.` |
|       - | 2525 | ` */` |
|     148 | 2526 | `PH7_PRIVATE const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|       5 | 2527 | `{` |
|       - | 2528 | `	sxu32 nCopy;` |
|     153 | 2529 | `	if( nBuf == 0 ) return "";` |
|     153 | 2530 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|     ! 0 | 2531 | `		zBuf[0] = 0;` |
|     ! 0 | 2532 | `		return zBuf;` |
|       - | 2533 | `	}` |
|     153 | 2534 | `	nCopy = SyStringLength(pStr);` |
|     153 | 2535 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|     153 | 2536 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|     153 | 2537 | `	zBuf[nCopy] = 0;` |
|     153 | 2538 | `	return zBuf;` |
|      79 | 2539 | `}` |
|       - | 2540 |  |
|       - | 2541 | `/*` |
|       - | 2542 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|       - | 2543 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|       - | 2544 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|       - | 2545 | ` * type field.` |
|       - | 2546 | ` */` |
| 2199341 | 2547 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|       5 | 2548 | `{` |
| 2199346 | 2549 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|       5 | 2550 | `}` |
|    9284 | 2551 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|       5 | 2552 | `{` |
|    9289 | 2553 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    9289 | 2554 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|       - | 2555 | `	const char *zGiven;` |
|       - | 2556 | `	char zBuf[128];` |
|       - | 2557 | `	char zTypeBuf[128];` |
|       - | 2558 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    9289 | 2559 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|     ! 0 | 2560 | `		return SXRET_OK;` |
|       - | 2561 | `	}` |
|       - | 2562 | `	/* never return type: the function must not return at all. An explicit` |
|       - | 2563 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|       - | 2564 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|       - | 2565 | `	 * the call site). */` |
|    9289 | 2566 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       3 | 2567 | `		return VmThrowNeverReturnError(pVm,&pFunc->sName);` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* void return type: the function must not produce a value. */` |
|    9287 | 2570 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     121 | 2571 | `		if( pValue == 0 ){` |
|     119 | 2572 | `			return SXRET_OK;` |
|       - | 2573 | `		}` |
|       - | 2574 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|       - | 2575 | `		 * still counts as "returned a value" here. */` |
|       3 | 2576 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|       3 | 2577 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|       - | 2578 | `	}` |
|       - | 2579 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|       - | 2580 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|       - | 2581 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    9171 | 2582 | `	if( pValue == 0 ){` |
|     ! 0 | 2583 | `		const char *zExpected = "value";` |
|     ! 0 | 2584 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|     ! 0 | 2585 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|     ! 0 | 2586 | `		}` |
|     ! 0 | 2587 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|       - | 2588 | `	}` |
|       - | 2589 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|       - | 2590 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|       - | 2591 | `	 * matching how every other typed return reports a missing value.) */` |
|    9171 | 2592 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|       5 | 2593 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|       3 | 2594 | `			return SXRET_OK;` |
|       - | 2595 | `		}` |
|       4 | 2596 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|       1 | 2597 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2598 | `	}` |
|       - | 2599 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|       - | 2600 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|       - | 2601 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    9167 | 2602 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|      18 | 2603 | `		return SXRET_OK;` |
|       - | 2604 | `	}` |
|       - | 2605 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|       - | 2606 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|       - | 2607 | `	 * Check by value before the real-class instanceof branch below. */` |
|    9153 | 2608 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     144 | 2609 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|     144 | 2610 | `		if( rcPseudo == 1 ){` |
|      86 | 2611 | `			return SXRET_OK;` |
|       - | 2612 | `		}` |
|      62 | 2613 | `		if( rcPseudo == 0 ){` |
|       9 | 2614 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|       4 | 2615 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|       2 | 2616 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2617 | `		}` |
|       - | 2618 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      27 | 2619 | `	}` |
|       - | 2620 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|       - | 2621 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|       - | 2622 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    9067 | 2623 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       - | 2624 | `		sxi32 rcU;` |
|      25 | 2625 | `		const char *zExpected = "union";` |
|      25 | 2626 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      25 | 2627 | `		if( rcU == SXRET_OK ){` |
|      21 | 2628 | `			return SXRET_OK;` |
|       - | 2629 | `		}` |
|       5 | 2630 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       3 | 2631 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       4 | 2632 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2633 | `			zGiven = "null";` |
|     ! 0 | 2634 | `		}else{` |
|       3 | 2635 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       - | 2636 | `		}` |
|       5 | 2637 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       5 | 2638 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|       2 | 2639 | `		}` |
|       5 | 2640 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|       - | 2641 | `	}` |
|       - | 2642 | `	/* Class return type — instanceof check. The class name is a length-` |
|       - | 2643 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|       - | 2644 | `	 * it into the TypeError message. */` |
|    9045 | 2645 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      58 | 2646 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|       - | 2647 | `		const char *zExpected;` |
|      58 | 2648 | `		ph7_class *pExpected = VmResolveTypeClass(pVm,pClassName,VmCurrentSelf(pVm));` |
|      58 | 2649 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|      58 | 2650 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       8 | 2651 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|       8 | 2652 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|       - | 2653 | `		}` |
|      52 | 2654 | `		if( pExpected ){` |
|      44 | 2655 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      44 | 2656 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       3 | 2657 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       3 | 2658 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|       - | 2659 | `			}` |
|      19 | 2660 | `		}` |
|      50 | 2661 | `		return SXRET_OK;` |
|       - | 2662 | `	}` |
|       - | 2663 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|       - | 2664 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|       - | 2665 | `	 * non-nullable scalar return — a TypeError. */` |
|    8991 | 2666 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|     ! 0 | 2667 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|     ! 0 | 2668 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 2669 | `			"null");` |
|       - | 2670 | `	}` |
|       - | 2671 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|       - | 2672 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    8991 | 2673 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    8975 | 2674 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    8975 | 2675 | `		return SXRET_OK;` |
|       - | 2676 | `	}` |
|       - | 2677 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|       - | 2678 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|       - | 2679 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|       - | 2680 | `	 * still rejects the object under strict_types. */` |
|      19 | 2681 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       7 | 2682 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       9 | 2683 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       4 | 2684 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       5 | 2685 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       7 | 2686 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|       2 | 2687 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       2 | 2688 | `				zGiven);` |
|       - | 2689 | `		}` |
|       1 | 2690 | `	}` |
|       - | 2691 | `	/* Array <-> scalar is never compatible. */` |
|      15 | 2692 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       7 | 2693 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|       2 | 2694 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       2 | 2695 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|       - | 2696 | `	}` |
|       - | 2697 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|       - | 2698 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|       - | 2699 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|       - | 2700 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       8 | 2701 | `	if( !bStrict` |
|       7 | 2702 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       5 | 2703 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       8 | 2704 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       4 | 2705 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|       1 | 2706 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       - | 2707 | `			"string");` |
|       - | 2708 | `	}` |
|       8 | 2709 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       6 | 2710 | `		return SXRET_OK;` |
|       - | 2711 | `	}` |
|       4 | 2712 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|       1 | 2713 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       1 | 2714 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    4647 | 2715 | `}` |
|       - | 2716 | `/*` |
|       - | 2717 | ` * Report a fatal named-argument error.` |
|       - | 2718 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|       - | 2719 | ` */` |
|      12 | 2720 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       3 | 2721 | `{` |
|       - | 2722 | `	SyBlob sMsg;` |
|       - | 2723 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|       - | 2724 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|       - | 2725 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|       - | 2726 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|       - | 2727 | `	 * unconditional fatal even inside try/catch. */` |
|      15 | 2728 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      15 | 2729 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|      15 | 2730 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 2731 | `}` |
|       - | 2732 | `/*` |
|       - | 2733 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|       - | 2734 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|       - | 2735 | ` * information.` |
|       - | 2736 | ` * ------------------------------------` |
|       - | 2737 | ` * Simple boring wrapper function.` |
|       - | 2738 | ` * ------------------------------------` |
|       - | 2739 | ` */` |
|   21098 | 2740 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|       5 | 2741 | `{` |
|       - | 2742 | `	sxi32 rc;` |
|   21103 | 2743 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|   21103 | 2744 | `	return rc;` |
|       5 | 2745 | `}` |
|       - | 2746 | `/*` |
|       - | 2747 | ` * Resolve function context from the current frame.` |
|       - | 2748 | ` */` |
|       - | 2749 | `/*` |
|       - | 2750 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|       - | 2751 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|       - | 2752 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|       - | 2753 | ` * straight at the function's own name otherwise.` |
|       - | 2754 | ` */` |
|  622508 | 2755 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|       5 | 2756 | `{` |
|  622513 | 2757 | `	const char *zName = pFunc->sName.zString;` |
|  622513 | 2758 | `	int nName = (int)pFunc->sName.nByte;` |
|  672769 | 2759 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|  673049 | 2760 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|  622513 | 2761 | `	if( bClosure ){` |
|       - | 2762 | `		int n;` |
|     575 | 2763 | `		if( pFunc->sFile.nByte > 0 ){` |
|     860 | 2764 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|     570 | 2765 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|     290 | 2766 | `		}else{` |
|     ! 0 | 2767 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|       - | 2768 | `		}` |
|     575 | 2769 | `		*pzOut = pVm->zDisplayName;` |
|     575 | 2770 | `		return n;` |
|       - | 2771 | `	}` |
|  621943 | 2772 | `	*pzOut = zName;` |
|  621943 | 2773 | `	return nName;` |
|  311259 | 2774 | `}` |
|    1128 | 2775 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|       4 | 2776 | `{` |
|       - | 2777 | `	VmFrame *pFrame;` |
|       - | 2778 | `	ph7_vm_func *pFunc;` |
|    1132 | 2779 | `	*pzFuncName = 0;` |
|    1132 | 2780 | `	*pnFuncLen = 0;` |
|    1132 | 2781 | `	pFrame = pVm->pFrame;` |
|    1132 | 2782 | `	if( pFrame == 0 ){` |
|     ! 0 | 2783 | `		return;` |
|       - | 2784 | `	}` |
|    1132 | 2785 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|    1132 | 2786 | `	if( pFrame->pParent == 0 ){` |
|    1096 | 2787 | `		return;` |
|       - | 2788 | `	}` |
|      40 | 2789 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      40 | 2790 | `	if( pFunc == 0 ){` |
|     ! 0 | 2791 | `		return;` |
|       - | 2792 | `	}` |
|      40 | 2793 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|     568 | 2794 | `}` |
|       - | 2795 | `/*` |
|       - | 2796 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|       - | 2797 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|       - | 2798 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|       - | 2799 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|       - | 2800 | ` * built-in library and already produces php's exact byte format, which keeps` |
|       - | 2801 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|       - | 2802 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|       - | 2803 | ` * synthesize what it can.` |
|       - | 2804 | ` */` |
|     586 | 2805 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 2806 | `{` |
|       - | 2807 | `	ph7_class_method *pGetTrace;` |
|       - | 2808 | `	ph7_value sTrace;` |
|       - | 2809 | `	const char *zTmp;` |
|       - | 2810 | `	int nTmp;` |
|     590 | 2811 | `	int bDone = 0;` |
|       - | 2812 | `	int bSaved;` |
|     590 | 2813 | `	if( pThis == 0 ){` |
|       5 | 2814 | `		return 0;` |
|       - | 2815 | `	}` |
|     586 | 2816 | `	if( pVm->bRenderingUncaught ){` |
|       - | 2817 | `		/* Already inside a report: do not run userland trace code again. */` |
|     ! 0 | 2818 | `		return 0;` |
|       - | 2819 | `	}` |
|     586 | 2820 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|     586 | 2821 | `	if( pGetTrace == 0 ){` |
|     ! 0 | 2822 | `		return 0;` |
|       - | 2823 | `	}` |
|     586 | 2824 | `	PH7_MemObjInit(pVm,&sTrace);` |
|       - | 2825 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|       - | 2826 | `	 * anything it calls) throws, the throw would be reported by this very` |
|       - | 2827 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|       - | 2828 | `	 * trace rather than re-entering here forever. */` |
|     586 | 2829 | `	bSaved = pVm->bRenderingUncaught;` |
|     586 | 2830 | `	pVm->bRenderingUncaught = 1;` |
|     586 | 2831 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|     586 | 2832 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|     586 | 2833 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 2834 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     586 | 2835 | `			bDone = 1;` |
|     291 | 2836 | `		}` |
|     291 | 2837 | `	}` |
|     586 | 2838 | `	PH7_MemObjRelease(&sTrace);` |
|     586 | 2839 | `	pVm->bRenderingUncaught = bSaved;` |
|     586 | 2840 | `	return bDone;` |
|     297 | 2841 | `}` |
|       - | 2842 | `/*` |
|       - | 2843 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|       - | 2844 | ` *` |
|       - | 2845 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|       - | 2846 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|       - | 2847 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|       - | 2848 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|       - | 2849 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|       - | 2850 | ` *             trailer.` |
|       - | 2851 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|       - | 2852 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|       - | 2853 | ` * call; this routine only appends.` |
|       - | 2854 | ` */` |
|     586 | 2855 | `static void VmRenderUncaughtEntry(` |
|       - | 2856 | `	ph7_vm *pVm,SyBlob *pOut,` |
|       - | 2857 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|       - | 2858 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|       - | 2859 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|       - | 2860 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|       - | 2861 | `	sxu32 nCallLine)   /* line of the call that entered the throwing frame (0 -> same) */` |
|       4 | 2862 | `{` |
|       - | 2863 | `	SyString *pFile;` |
|     590 | 2864 | `	if( nThrowLine == 0 ){` |
|       5 | 2865 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|       2 | 2866 | `	}` |
|     590 | 2867 | `	if( nCallLine == 0 ){` |
|     590 | 2868 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     590 | 2869 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|     293 | 2870 | `	}` |
|     590 | 2871 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|     ! 0 | 2872 | `		zClass = "Exception";` |
|     ! 0 | 2873 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|     ! 0 | 2874 | `	}` |
|     590 | 2875 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     556 | 2876 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     276 | 2877 | `	}` |
|     590 | 2878 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     590 | 2879 | `	if( bFirst ){` |
|     584 | 2880 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|     294 | 2881 | `	}else{` |
|       8 | 2882 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       - | 2883 | `	}` |
|     590 | 2884 | `	SyBlobAppend(pOut,zClass,nClass);` |
|     590 | 2885 | `	if( zMsg && nMsg > 0 ){` |
|     590 | 2886 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|     590 | 2887 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|     293 | 2888 | `	}` |
|     590 | 2889 | `	if( pFile ){` |
|     590 | 2890 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     293 | 2891 | `	}` |
|     590 | 2892 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       - | 2893 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|       - | 2894 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|       - | 2895 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|       - | 2896 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|       - | 2897 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|       - | 2898 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|       - | 2899 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|     590 | 2900 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|       5 | 2901 | `		int bFrame = 0;` |
|       5 | 2902 | `		if( zFuncName && nFuncLen > 0 ){` |
|       3 | 2903 | `			if( pFile ){` |
|       - | 2904 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|       - | 2905 | `				 * running inside it. */` |
|       4 | 2906 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|       2 | 2907 | `					(int)pFile->nByte,pFile->zString,nCallLine,nFuncLen,zFuncName);` |
|       2 | 2908 | `			}else{` |
|     ! 0 | 2909 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|       - | 2910 | `			}` |
|       3 | 2911 | `			bFrame = 1;` |
|       1 | 2912 | `		}` |
|       - | 2913 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|       7 | 2914 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|       2 | 2915 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|       2 | 2916 | `	}` |
|     590 | 2917 | `	if( bLast && pFile ){` |
|     584 | 2918 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|     584 | 2919 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|     290 | 2920 | `	}` |
|     590 | 2921 | `}` |
|       - | 2922 | `/*` |
|       - | 2923 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|       - | 2924 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|       - | 2925 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|       - | 2926 | ` */` |
|       4 | 2927 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|       1 | 2928 | `{` |
|       - | 2929 | `	SyBlob sOut;` |
|       - | 2930 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|       - | 2931 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|       5 | 2932 | `	pVm->iExitStatus = 255;` |
|       5 | 2933 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 2934 | `		return PH7_OK;` |
|       - | 2935 | `	}` |
|       5 | 2936 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       5 | 2937 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0);` |
|       5 | 2938 | `	VmCallErrorHandler(pVm,&sOut);` |
|       5 | 2939 | `	SyBlobRelease(&sOut);` |
|       5 | 2940 | `	return PH7_ABORT;` |
|       3 | 2941 | `}` |
|       - | 2942 | `/*` |
|       - | 2943 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|       - | 2944 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|       - | 2945 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|       - | 2946 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|       - | 2947 | ` */` |
|       - | 2948 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|     582 | 2949 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|       4 | 2950 | `{` |
|       - | 2951 | `	ph7_value *pValue;` |
|       - | 2952 | `	ph7_class_instance *pPrev;` |
|       - | 2953 | `	ph7_class *pThrowable;` |
|     586 | 2954 | `	if( pThis == 0 ){` |
|     ! 0 | 2955 | `		return 0;` |
|       - | 2956 | `	}` |
|     586 | 2957 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|     586 | 2958 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     580 | 2959 | `		return 0;` |
|       - | 2960 | `	}` |
|       8 | 2961 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 2962 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|       - | 2963 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|       - | 2964 | `	 * never renders a stray object as an exception entry. */` |
|       8 | 2965 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       8 | 2966 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|     ! 0 | 2967 | `		return 0;` |
|       - | 2968 | `	}` |
|       8 | 2969 | `	return pPrev;` |
|     295 | 2970 | `}` |
|       - | 2971 | `/*` |
|       - | 2972 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|       - | 2973 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|       - | 2974 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|       - | 2975 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|       - | 2976 | ` */` |
|      16 | 2977 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|       3 | 2978 | `{` |
|       - | 2979 | `	ph7_value *pValue;` |
|      19 | 2980 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|     ! 0 | 2981 | `		return;` |
|       - | 2982 | `	}` |
|      19 | 2983 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      19 | 2984 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       3 | 2985 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|       - | 2986 | `	}` |
|      17 | 2987 | `	pPrev->iRef++;` |
|       - | 2988 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|       - | 2989 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|      17 | 2990 | `	PH7_MemObjRelease(pValue);` |
|      17 | 2991 | `	pValue->x.pOther = pPrev;` |
|      17 | 2992 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|      11 | 2993 | `}` |
|       - | 2994 | `/*` |
|       - | 2995 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|       - | 2996 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|       - | 2997 | ` * absent or yields an empty string.` |
|       - | 2998 | ` */` |
|       - | 2999 | `/*` |
|       - | 3000 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|       - | 3001 | ` * 0 when the class exposes no getLine().` |
|       - | 3002 | ` */` |
|     582 | 3003 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       4 | 3004 | `{` |
|       - | 3005 | `	ph7_class_method *pGetLine;` |
|       - | 3006 | `	ph7_value sLine;` |
|     586 | 3007 | `	sxu32 nLine = 0;` |
|     586 | 3008 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|     586 | 3009 | `	if( pGetLine == 0 ){` |
|     ! 0 | 3010 | `		return 0;` |
|       - | 3011 | `	}` |
|     586 | 3012 | `	PH7_MemObjInit(pVm,&sLine);` |
|     586 | 3013 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|     586 | 3014 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|     586 | 3015 | `		if( n > 0 ){` |
|     586 | 3016 | `			nLine = (sxu32)n;` |
|     291 | 3017 | `		}` |
|     291 | 3018 | `	}` |
|     586 | 3019 | `	PH7_MemObjRelease(&sLine);` |
|     586 | 3020 | `	return nLine;` |
|     295 | 3021 | `}` |
|     582 | 3022 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|       4 | 3023 | `{` |
|       - | 3024 | `	ph7_class_method *pGetMessage;` |
|       - | 3025 | `	ph7_value sMsg;` |
|       - | 3026 | `	const char *zTmp;` |
|       - | 3027 | `	int nTmp;` |
|     586 | 3028 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|     586 | 3029 | `	if( pGetMessage == 0 ){` |
|     ! 0 | 3030 | `		return;` |
|       - | 3031 | `	}` |
|     586 | 3032 | `	PH7_MemObjInit(pVm,&sMsg);` |
|     586 | 3033 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|     586 | 3034 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|     586 | 3035 | `		if( zTmp && nTmp > 0 ){` |
|     586 | 3036 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|     291 | 3037 | `		}` |
|     291 | 3038 | `	}` |
|     586 | 3039 | `	PH7_MemObjRelease(&sMsg);` |
|     295 | 3040 | `}` |
|       - | 3041 | `/*` |
|       - | 3042 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|       - | 3043 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|       - | 3044 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|       - | 3045 | ` * outermost (the actually-uncaught) exception.` |
|       - | 3046 | ` *` |
|       - | 3047 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|       - | 3048 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|       - | 3049 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|       - | 3050 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|       - | 3051 | ` */` |
|       - | 3052 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|     576 | 3053 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|       4 | 3054 | `{` |
|       - | 3055 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|     580 | 3056 | `	int nChain = 0;` |
|       - | 3057 | `	int i;` |
|       - | 3058 | `	SyBlob sOut;` |
|       - | 3059 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|       - | 3060 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|       - | 3061 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|     580 | 3062 | `	pVm->iExitStatus = 255;` |
|     580 | 3063 | `	if( !pVm->bErrReport ){` |
|     ! 0 | 3064 | `		return PH7_OK;` |
|       - | 3065 | `	}` |
|       - | 3066 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|       - | 3067 | `	 * collected) or the hard cap. */` |
|    1162 | 3068 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|     594 | 3069 | `		for( i = 0 ; i < nChain ; ++i ){` |
|      10 | 3070 | `			if( apChain[i] == pThis ){` |
|     ! 0 | 3071 | `				pThis = 0; /* cycle: stop the walk */` |
|     ! 0 | 3072 | `				break;` |
|       - | 3073 | `			}` |
|       6 | 3074 | `		}` |
|     586 | 3075 | `		if( pThis == 0 ){` |
|     ! 0 | 3076 | `			break;` |
|       - | 3077 | `		}` |
|     586 | 3078 | `		apChain[nChain++] = pThis;` |
|     586 | 3079 | `		pThis = VmExceptionGetPrevious(pThis);` |
|       4 | 3080 | `	}` |
|     580 | 3081 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       - | 3082 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|       - | 3083 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|    1162 | 3084 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|     586 | 3085 | `		ph7_class_instance *pEnt = apChain[i];` |
|       - | 3086 | `		SyBlob sMsg;` |
|     586 | 3087 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     586 | 3088 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|     877 | 3089 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|     582 | 3090 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|     582 | 3091 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|     291 | 3092 | `			zFuncName,nFuncLen,` |
|     582 | 3093 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|     291 | 3094 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|     291 | 3095 | `			VmExtractExceptionLine(pVm,pEnt),0);` |
|     586 | 3096 | `		SyBlobRelease(&sMsg);` |
|     295 | 3097 | `	}` |
|     580 | 3098 | `	VmCallErrorHandler(pVm,&sOut);` |
|     580 | 3099 | `	SyBlobRelease(&sOut);` |
|     580 | 3100 | `	return PH7_ABORT;` |
|     292 | 3101 | `}` |
|       - | 3102 | `/*` |
|       - | 3103 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|       - | 3104 | ` *` |
|       - | 3105 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|       - | 3106 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|       - | 3107 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|       - | 3108 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|       - | 3109 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|       - | 3110 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|       - | 3111 | ` */` |
| 1547644 | 3112 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|       5 | 3113 | `{` |
| 1547649 | 3114 | `	if( pVm->bCoalesceArmed ){` |
|       8 | 3115 | `		if( pVm->pCoalesceObj ){` |
|       8 | 3116 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|       3 | 3117 | `		}` |
|       8 | 3118 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       8 | 3119 | `		pVm->pCoalesceObj = 0;` |
|       8 | 3120 | `		pVm->bCoalesceArmed = 0;` |
|       3 | 3121 | `	}` |
| 1547649 | 3122 | `}` |
|       - | 3123 | `/*` |
|       - | 3124 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|       - | 3125 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|       - | 3126 | ` * is a literal, non-formatted string; callers that need formatting should` |
|       - | 3127 | ` * build the SyBlob themselves and pass its data + length.` |
|       - | 3128 | ` *` |
|       - | 3129 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|       - | 3130 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|       - | 3131 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|       - | 3132 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|       - | 3133 | ` */` |
|  140152 | 3134 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|       - | 3135 | `	ph7_vm *pVm,` |
|       - | 3136 | `	const char *zClass,` |
|       - | 3137 | `	const char *zMsg,` |
|       - | 3138 | `	sxu32 nMsg` |
|       5 | 3139 | `){` |
|       - | 3140 | `	ph7_class *pClass;` |
|       - | 3141 | `	ph7_class_instance *pThis;` |
|       - | 3142 | `	ph7_class_method *pCons;` |
|       - | 3143 | `	VmFrame *pFrame;` |
|       - | 3144 | `	sxi32 rc;` |
|  140157 | 3145 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|  140157 | 3146 | `	if( pClass == 0 ){` |
|     ! 0 | 3147 | `		return SXERR_ABORT;` |
|       - | 3148 | `	}` |
|  140157 | 3149 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|  140157 | 3150 | `	if( pThis == 0 ){` |
|     ! 0 | 3151 | `		return SXERR_ABORT;` |
|       - | 3152 | `	}` |
|  140157 | 3153 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|  140157 | 3154 | `	if( pCons ){` |
|       - | 3155 | `		ph7_value sArg;` |
|       - | 3156 | `		ph7_value *apArg[1];` |
|       - | 3157 | `		SyString sMsgStr;` |
|  140157 | 3158 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|  140157 | 3159 | `		PH7_MemObjInit(pVm,&sArg);` |
|  140157 | 3160 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|  140157 | 3161 | `		apArg[0] = &sArg;` |
|  140157 | 3162 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|  140157 | 3163 | `		PH7_MemObjRelease(&sArg);` |
|   70076 | 3164 | `	}` |
|  140157 | 3165 | `	pFrame = pVm->pFrame;` |
|  140157 | 3166 | `	if( pFrame ){` |
|  140157 | 3167 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  140157 | 3168 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   70076 | 3169 | `	}` |
|  140157 | 3170 | `	rc = VmThrowException(pVm,pThis);` |
|  140157 | 3171 | `	PH7_ClassInstanceUnref(pThis);` |
|  140157 | 3172 | `	return rc;` |
|   70081 | 3173 | `}` |
|       - | 3174 | `/*` |
|       - | 3175 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|       - | 3176 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|       - | 3177 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|       - | 3178 | ` *` |
|       - | 3179 | ` *   int/float/bool/null      arithmetic proceeds` |
|       - | 3180 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|       - | 3181 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|       - | 3182 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|       - | 3183 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|       - | 3184 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|       - | 3185 | ` *   object/resource          TypeError, naming the object's CLASS` |
|       - | 3186 | ` *` |
|       - | 3187 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|       - | 3188 | ` */` |
|       - | 3189 | `/*` |
|       - | 3190 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|       - | 3191 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|       - | 3192 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|       - | 3193 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|       - | 3194 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|       - | 3195 | ` * to depth 1).` |
|       - | 3196 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|       - | 3197 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|       - | 3198 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|       - | 3199 | ` * frame shape is file/line/function[/class/type], matching the default` |
|       - | 3200 | ` * zend.exception_ignore_args=On.` |
|       - | 3201 | ` */` |
| 1447480 | 3202 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,ph7_value *pList)` |
|       5 | 3203 | `{` |
|       - | 3204 | `	SyString *pFile;` |
|       - | 3205 | `	VmFrame *pFrame;` |
|       - | 3206 | `	ph7_value *pValue;` |
| 1447485 | 3207 | `	pValue = ph7_new_scalar(&(*pVm));` |
| 1447485 | 3208 | `	if( pValue == 0 ){` |
|     ! 0 | 3209 | `		return;` |
|       - | 3210 | `	}` |
| 1447485 | 3211 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1447485 | 3212 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 2069755 | 3213 | `	while( pFrame ){` |
| 2069755 | 3214 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 3215 | `		ph7_value *pEntry;` |
| 2069755 | 3216 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|       - | 3217 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|  723745 | 3218 | `			break;` |
|       - | 3219 | `		}` |
|  622275 | 3220 | `		pEntry = ph7_new_array(&(*pVm));` |
|  622275 | 3221 | `		if( pEntry == 0 ){` |
|     ! 0 | 3222 | `			break;` |
|       - | 3223 | `		}` |
|       - | 3224 | `		/* php's key order: file, line, function[, class, type][, object][, args].` |
|       - | 3225 | `		 * The frame's file/line is the CALL SITE -- i.e. the caller's body, which` |
|       - | 3226 | `		 * lives in the CALLER function's defining file. Use that (not the include-` |
|       - | 3227 | `		 * stack top, which is wrong once a call chain spans files); fall back to the` |
|       - | 3228 | `		 * include-stack top for a call made at global scope. */` |
|       - | 3229 | `		{` |
|  622275 | 3230 | `			SyString *pFrameFile = pFile;` |
|  622275 | 3231 | `			if( pFrame->pParent->pUserData ){` |
|     233 | 3232 | `				ph7_vm_func *pCaller = (ph7_vm_func *)pFrame->pParent->pUserData;` |
|     233 | 3233 | `				if( pCaller->sFile.nByte > 0 ){` |
|     111 | 3234 | `					pFrameFile = &pCaller->sFile;` |
|      53 | 3235 | `				}` |
|     114 | 3236 | `			}` |
|  622275 | 3237 | `			if( pFrameFile ){` |
|  622275 | 3238 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|  622275 | 3239 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|  622275 | 3240 | `				ph7_value_reset_string_cursor(pValue);` |
|  311135 | 3241 | `			}` |
|       - | 3242 | `		}` |
|  622275 | 3243 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|  622275 | 3244 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|       - | 3245 | `		{` |
|  622275 | 3246 | `			const char *zDisp = 0;` |
|  622275 | 3247 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|  622275 | 3248 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|       - | 3249 | `		}` |
|  622275 | 3250 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|  622275 | 3251 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 3252 | `		{` |
|       - | 3253 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|       - | 3254 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|       - | 3255 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|       - | 3256 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|       - | 3257 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|       - | 3258 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|       - | 3259 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|  622275 | 3260 | `			SyString *pClsName = 0;` |
|  622275 | 3261 | `			const char *zType = "->";` |
|  622275 | 3262 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|  500467 | 3263 | `				pClsName = &((ph7_class *)pFunc->pUserData)->sName;` |
|  500467 | 3264 | `				zType = pFrame->pThis ? "->" : "::";` |
|  372044 | 3265 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|     ! 0 | 3266 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|     ! 0 | 3267 | `			}` |
|  622275 | 3268 | `			if( pClsName ){` |
|  500467 | 3269 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|  500467 | 3270 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|  500467 | 3271 | `				ph7_value_reset_string_cursor(pValue);` |
|  500467 | 3272 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|  500467 | 3273 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|  500467 | 3274 | `				ph7_value_reset_string_cursor(pValue);` |
|  500467 | 3275 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis ){` |
|       9 | 3276 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       9 | 3277 | `					if( pObjVal ){` |
|       9 | 3278 | `						pFrame->pThis->iRef++;` |
|       9 | 3279 | `						pObjVal->x.pOther = pFrame->pThis;` |
|       9 | 3280 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       9 | 3281 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|       9 | 3282 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       4 | 3283 | `					}` |
|       4 | 3284 | `				}` |
|  250231 | 3285 | `			}` |
|       - | 3286 | `		}` |
|  622275 | 3287 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|       9 | 3288 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|       9 | 3289 | `			if( pArg ){` |
|       9 | 3290 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       - | 3291 | `				sxu32 n;` |
|      17 | 3292 | `				for( n = 0 ; n < SySetUsed(&pFrame->sArg) ; ++n ){` |
|       9 | 3293 | `					ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,aSlot[n].nIdx);` |
|       9 | 3294 | `					if( pObj ){` |
|       9 | 3295 | `						ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       4 | 3296 | `					}` |
|       5 | 3297 | `				}` |
|       9 | 3298 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|       9 | 3299 | `				ph7_release_value(&(*pVm),pArg);` |
|       4 | 3300 | `			}` |
|       4 | 3301 | `		}` |
|  622275 | 3302 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|  622275 | 3303 | `		ph7_release_value(&(*pVm),pEntry);` |
|  622275 | 3304 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|       5 | 3305 | `	}` |
| 1447485 | 3306 | `	ph7_release_value(&(*pVm),pValue);` |
|  723745 | 3307 | `}` |
|       - | 3308 | `/*` |
|       - | 3309 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|       - | 3310 | ` *` |
|       - | 3311 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|       - | 3312 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|       - | 3313 | ` * calls parent::__construct still reports the right position. The embedded` |
|       - | 3314 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|       - | 3315 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|       - | 3316 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|       - | 3317 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|       - | 3318 | ` */` |
| 1553960 | 3319 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|       5 | 3320 | `{` |
|       - | 3321 | `	static const char *azField[] = { "file", "line", "trace" };` |
|       - | 3322 | `	ph7_class *pThrowable;` |
|       - | 3323 | `	SyString *pFile;` |
|       - | 3324 | `	SyString *pSiteFile;` |
|       - | 3325 | `	sxu32 n;` |
| 1553965 | 3326 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|     ! 0 | 3327 | `		return;` |
|       - | 3328 | `	}` |
| 1553965 | 3329 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1553965 | 3330 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|  106495 | 3331 | `		return;` |
|       - | 3332 | `	}` |
| 1447475 | 3333 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
| 1447475 | 3334 | `	pSiteFile = pFile;` |
|       - | 3335 | ``	/* getFile() is the file where `new` executed = the DEFINING file of the`` |
|       - | 3336 | `	 * innermost active function (aFiles tracks include nesting, not the running` |
|       - | 3337 | `	 * function's source, so it is wrong once a call chain spans files). Fall back` |
|       - | 3338 | `	 * to the include-stack top at global scope / for engine-created throwables. */` |
|       - | 3339 | `	{` |
| 1447475 | 3340 | `		VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
| 1447475 | 3341 | `		if( pInner && pInner->pUserData ){` |
|  621425 | 3342 | `			ph7_vm_func *pInnerFunc = (ph7_vm_func *)pInner->pUserData;` |
|  621425 | 3343 | `			if( pInnerFunc->sFile.nByte > 0 ){` |
|  621111 | 3344 | `				pSiteFile = &pInnerFunc->sFile;` |
|  310553 | 3345 | `			}` |
|  310710 | 3346 | `		}` |
|       - | 3347 | `	}` |
| 5789885 | 3348 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|       - | 3349 | `		SyHashEntry *pEntry;` |
|       - | 3350 | `		VmClassAttr *pVmAttr;` |
|       - | 3351 | `		ph7_value *pAttrValue;` |
| 4342415 | 3352 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
| 4342415 | 3353 | `		if( pEntry == 0 ){` |
|     ! 0 | 3354 | `			continue;` |
|       - | 3355 | `		}` |
| 4342415 | 3356 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
| 4342415 | 3357 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 4342415 | 3358 | `		if( pAttrValue == 0 ){` |
|     ! 0 | 3359 | `			continue;` |
|       - | 3360 | `		}` |
| 4342415 | 3361 | `		if( n == 0 ){` |
| 1447475 | 3362 | `			if( pSiteFile ){` |
| 1447475 | 3363 | `				PH7_MemObjRelease(pAttrValue);` |
| 1447475 | 3364 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|  723740 | 3365 | `			}` |
| 3618680 | 3366 | `		}else if( n == 1 ){` |
| 1447475 | 3367 | `			PH7_MemObjRelease(pAttrValue);` |
| 1447475 | 3368 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)(pVm->nCurLine ? pVm->nCurLine : 1));` |
|  723740 | 3369 | `		}else{` |
|       - | 3370 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|       - | 3371 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|       - | 3372 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|       - | 3373 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
| 1447475 | 3374 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
| 1447475 | 3375 | `			if( pList == 0 ){` |
|     ! 0 | 3376 | `				continue;` |
|       - | 3377 | `			}` |
| 1447475 | 3378 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,pList);` |
|       - | 3379 | `			/* Building the trace reserves new memobjs, which may realloc` |
|       - | 3380 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO that set,` |
|       - | 3381 | `			 * from PH7_ClassInstanceExtractAttrValue above). Re-fetch the slot` |
|       - | 3382 | `			 * AFTER the walk before releasing/storing into it. */` |
| 1447475 | 3383 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
| 1447475 | 3384 | `			if( pAttrValue ){` |
| 1447475 | 3385 | `				PH7_MemObjRelease(pAttrValue);` |
| 1447475 | 3386 | `				PH7_MemObjStore(pList,pAttrValue);` |
|  723735 | 3387 | `			}` |
| 1447475 | 3388 | `			ph7_release_value(&(*pVm),pList);` |
|       - | 3389 | `		}` |
| 2171210 | 3390 | `	}` |
|  776985 | 3391 | `}` |
|      52 | 3392 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|       3 | 3393 | `{` |
|      55 | 3394 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       3 | 3395 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       3 | 3396 | `		if( pInst && pInst->pClass ){` |
|       3 | 3397 | `			return pInst->pClass->sName.zString;` |
|       - | 3398 | `		}` |
|     ! 0 | 3399 | `	}` |
|      53 | 3400 | `	return ph7_type_name(pVal);` |
|      29 | 3401 | `}` |
|   58076 | 3402 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|       5 | 3403 | `{` |
|   58081 | 3404 | `	int bBadL = 0, bBadR = 0;` |
|       - | 3405 | `	int i;` |
|       - | 3406 | `	ph7_value *apOperand[2];` |
|   58081 | 3407 | `	apOperand[0] = pLeft;` |
|   58081 | 3408 | `	apOperand[1] = pRight;` |
|       - | 3409 | `	/* array + array is php's union operator, not arithmetic */` |
|   58076 | 3410 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   23137 | 3411 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|    4087 | 3412 | `		return SXRET_OK;` |
|       - | 3413 | `	}` |
|  161987 | 3414 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  107993 | 3415 | `		ph7_value *pVal = apOperand[i];` |
|  107993 | 3416 | `		int bBad = 0;` |
|  107993 | 3417 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       5 | 3418 | `			bBad = 1;` |
|  107991 | 3419 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      50 | 3420 | `			const char *zTail = 0;` |
|      50 | 3421 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|      50 | 3422 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|       - | 3423 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|      11 | 3424 | `				bBad = 1;` |
|       6 | 3425 | `			}else{` |
|       - | 3426 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|       - | 3427 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|       - | 3428 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|      42 | 3429 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       3 | 3430 | `					zTail++;` |
|       1 | 3431 | `				}` |
|      40 | 3432 | `				if( zTail < zEnd ){` |
|      20 | 3433 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|       9 | 3434 | `				}` |
|       - | 3435 | `			}` |
|      24 | 3436 | `		}` |
|  107993 | 3437 | `		if( bBad ){` |
|      15 | 3438 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|       7 | 3439 | `		}` |
|   54229 | 3440 | `	}` |
|   53999 | 3441 | `	if( bBadL \|\| bBadR ){` |
|       - | 3442 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|       - | 3443 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|       - | 3444 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|      22 | 3445 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|       7 | 3446 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|      15 | 3447 | `		return SXERR_INVALID;` |
|       - | 3448 | `	}` |
|   53985 | 3449 | `	return SXRET_OK;` |
|   29158 | 3450 | `}` |
|       - | 3451 | `/*` |
|       - | 3452 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|       - | 3453 | ` */` |
|    5760 | 3454 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|       5 | 3455 | `{` |
|       - | 3456 | `	ph7_vm *pVm;` |
|       - | 3457 | `	ph7_class *pClass;` |
|       - | 3458 | `	ph7_class_instance *pThis;` |
|       - | 3459 | `	ph7_class_method *pCons;` |
|       - | 3460 | `	ph7_value sArg;` |
|       - | 3461 | `	ph7_value *apArg[1];` |
|       - | 3462 | `	SyBlob sMsg;` |
|       - | 3463 | `	SyString sMsgStr;` |
|       - | 3464 | `	VmFrame *pFrame;` |
|       - | 3465 | `	va_list ap;` |
|       - | 3466 | `	sxi32 rc;` |
|       - | 3467 |  |
|    5765 | 3468 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 3469 | `		return PH7_ABORT;` |
|       - | 3470 | `	}` |
|    5765 | 3471 | `	pVm = pCtx->pVm;` |
|    5765 | 3472 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 3473 | `		zClass = "Error";` |
|     ! 0 | 3474 | `	}` |
|       - | 3475 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|       - | 3476 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|       - | 3477 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|       - | 3478 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|       - | 3479 | `	 * reported error (VmHostFuncThrowRc). */` |
|    5765 | 3480 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    5765 | 3481 | `	if( pClass == 0 ){` |
|     ! 0 | 3482 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 3483 | `			"Cannot throw internal exception, class '%s' is not available",` |
|     ! 0 | 3484 | `			zClass` |
|       - | 3485 | `			);` |
|     ! 0 | 3486 | `		return pCtx->nThrowRc;` |
|       - | 3487 | `	}` |
|    5765 | 3488 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    5765 | 3489 | `	if( pThis == 0 ){` |
|     ! 0 | 3490 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|       - | 3491 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|       - | 3492 | `			);` |
|     ! 0 | 3493 | `		return pCtx->nThrowRc;` |
|       - | 3494 | `	}` |
|       - | 3495 |  |
|    5765 | 3496 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    5765 | 3497 | `	va_start(ap,zFormat);` |
|    5765 | 3498 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|    5765 | 3499 | `	va_end(ap);` |
|       - | 3500 |  |
|    5765 | 3501 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    5765 | 3502 | `	if( pCons ){` |
|    5765 | 3503 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    5765 | 3504 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    5765 | 3505 | `		apArg[0] = &sArg;` |
|    5765 | 3506 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|    5765 | 3507 | `		PH7_MemObjRelease(&sArg);` |
|    2880 | 3508 | `	}` |
|    5765 | 3509 | `	SyBlobRelease(&sMsg);` |
|       - | 3510 |  |
|    5765 | 3511 | `	pFrame = pVm->pFrame;` |
|    5765 | 3512 | `	if( pFrame ){` |
|    5765 | 3513 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    5765 | 3514 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    2880 | 3515 | `	}` |
|    5765 | 3516 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    5765 | 3517 | `	PH7_ClassInstanceUnref(pThis);` |
|    5765 | 3518 | `	if( rc == SXERR_ABORT ){` |
|     514 | 3519 | `		pCtx->nThrowRc = PH7_ABORT;` |
|     514 | 3520 | `		return PH7_ABORT;` |
|       - | 3521 | `	}` |
|       - | 3522 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|       - | 3523 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|       - | 3524 | `	 * argument-validation helper whose callers have no status channel — would` |
|       - | 3525 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|       - | 3526 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|       - | 3527 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|       - | 3528 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|       - | 3529 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|       - | 3530 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    5255 | 3531 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    5255 | 3532 | `	return PH7_EXCEPTION;` |
|    2885 | 3533 | `}` |
|       - | 3534 | `/*` |
|       - | 3535 | ` * The status a host function's own throw should have returned. Consulted at the` |
|       - | 3536 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|       - | 3537 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|       - | 3538 | ` * and is the identity when the routine never threw or already reported it.` |
|       - | 3539 | ` *` |
|       - | 3540 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|       - | 3541 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|       - | 3542 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|       - | 3543 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|       - | 3544 | ` */` |
| 3936319 | 3545 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|       5 | 3546 | `{` |
| 3936319 | 3547 | `	if( pCtx->nThrowRc == 0` |
| 1971265 | 3548 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
| 3932294 | 3549 | `		return rc;` |
|       - | 3550 | `	}` |
|    4032 | 3551 | `	return pCtx->nThrowRc;` |
| 1968767 | 3552 | `}` |
|       - | 3553 | `/*` |
|       - | 3554 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|       - | 3555 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|       - | 3556 | ` */` |
|     ! 0 | 3557 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|     ! 0 | 3558 | `{` |
|       - | 3559 | `	ph7_vm *pVm;` |
|       - | 3560 | `	SyBlob sMsg;` |
|     ! 0 | 3561 | `	const char *zFuncName = 0;` |
|     ! 0 | 3562 | `	int nFuncLen = 0;` |
|       - | 3563 | `	va_list ap;` |
|       - | 3564 | `	sxi32 rc;` |
|       - | 3565 |  |
|     ! 0 | 3566 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|     ! 0 | 3567 | `		return PH7_OK;` |
|       - | 3568 | `	}` |
|     ! 0 | 3569 | `	pVm = pCtx->pVm;` |
|     ! 0 | 3570 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|     ! 0 | 3571 | `		zClass = "Error";` |
|     ! 0 | 3572 | `	}` |
|       - | 3573 |  |
|     ! 0 | 3574 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 3575 |  |
|     ! 0 | 3576 | `	va_start(ap,zFormat);` |
|     ! 0 | 3577 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     ! 0 | 3578 | `	va_end(ap);` |
|       - | 3579 |  |
|     ! 0 | 3580 | `	if( pCtx->pFunc ){` |
|     ! 0 | 3581 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|     ! 0 | 3582 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|     ! 0 | 3583 | `	}` |
|     ! 0 | 3584 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|     ! 0 | 3585 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|     ! 0 | 3586 | `	}` |
|     ! 0 | 3587 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|     ! 0 | 3588 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|     ! 0 | 3589 | `	SyBlobRelease(&sMsg);` |
|     ! 0 | 3590 | `	return rc;` |
|     ! 0 | 3591 | `}` |
|       - | 3592 | `/*` |
|       - | 3593 | ` * The following routine is invoked by the engine when an uncaught` |
|       - | 3594 | ` * exception is triggered.` |
|       - | 3595 | ` */` |
|     578 | 3596 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|       - | 3597 | `	ph7_vm *pVm, /* Target VM */` |
|       - | 3598 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 3599 | `	)` |
|       4 | 3600 | `{` |
|       - | 3601 | `	ph7_value *apArg[2],sArg;` |
|     582 | 3602 | `	int nArg = 1;` |
|       - | 3603 | `	sxi32 rc;` |
|     582 | 3604 | `	if( pVm->nExceptDepth > 15 ){` |
|       - | 3605 | `		/* Nesting limit reached */` |
|     ! 0 | 3606 | `		return SXRET_OK;` |
|       - | 3607 | `	}` |
|       - | 3608 | `	/* Call any exception handler if available */` |
|     582 | 3609 | `	PH7_MemObjInit(pVm,&sArg);` |
|     582 | 3610 | `	if( pThis ){` |
|       - | 3611 | `		/* Load the exception instance */` |
|     582 | 3612 | `		sArg.x.pOther = pThis;` |
|     582 | 3613 | `		pThis->iRef++;` |
|     582 | 3614 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|     293 | 3615 | `	}else{` |
|     ! 0 | 3616 | `		nArg = 0;` |
|       - | 3617 | `	}` |
|     582 | 3618 | `	apArg[0] = &sArg;` |
|       - | 3619 | `	/* Call the exception handler if available */` |
|     582 | 3620 | `	pVm->nExceptDepth++;` |
|     582 | 3621 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|     582 | 3622 | `	pVm->nExceptDepth--;` |
|     582 | 3623 | `	if( rc != SXRET_OK ){` |
|       - | 3624 | `		const char *zFuncName;` |
|       - | 3625 | `		int nFuncLen;` |
|     580 | 3626 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       - | 3627 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|     580 | 3628 | `		if( pThis ){` |
|       - | 3629 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|       - | 3630 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|       - | 3631 | `			 * renders byte-identically to the historical single-entry report. */` |
|     580 | 3632 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|     292 | 3633 | `		}else{` |
|       - | 3634 | `			/* No instance (internal report path) — default-class single entry. */` |
|     ! 0 | 3635 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|       - | 3636 | `		}` |
|       - | 3637 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|     580 | 3638 | `		rc = SXERR_ABORT;` |
|     288 | 3639 | `	}` |
|     582 | 3640 | `	PH7_MemObjRelease(&sArg);` |
|     582 | 3641 | `	return rc;` |
|     293 | 3642 | `}` |
|       - | 3643 | `/*` |
|       - | 3644 | ` * Throw a user exception.` |
|       - | 3645 | ` *` |
|       - | 3646 | ` * Exception dispatch follows this sequence:` |
|       - | 3647 | ` *` |
|       - | 3648 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|       - | 3649 | ` *    try/catch whose catch block matches the exception class.` |
|       - | 3650 | ` *` |
|       - | 3651 | ` * 2. If NO catch matches:` |
|       - | 3652 | ` *    a. Run finally (if present) for the current try block.` |
|       - | 3653 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|       - | 3654 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|       - | 3655 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|       - | 3656 | ` *       exception in pVm->pPendingException instead of reporting it` |
|       - | 3657 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|       - | 3658 | ` *    d. Otherwise, report as truly uncaught.` |
|       - | 3659 | ` *` |
|       - | 3660 | ` * 3. If a catch DOES match:` |
|       - | 3661 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|       - | 3662 | ` *       aException stack and resetting it. This prevents a re-throw` |
|       - | 3663 | ` *       inside the catch body from immediately propagating past our` |
|       - | 3664 | ` *       finally block.` |
|       - | 3665 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|       - | 3666 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|       - | 3667 | ` *       no handlers (they're hidden), so the exception is deferred` |
|       - | 3668 | ` *       in pPendingException (step 2c).` |
|       - | 3669 | ` *    c. Restore outer handlers from the saved copy.` |
|       - | 3670 | ` *    d. Run finally (if present).` |
|       - | 3671 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|       - | 3672 | ` *       that handlers are restored and finally has run.` |
|       - | 3673 | ` */` |
|       - | 3674 | `/*` |
|       - | 3675 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|       - | 3676 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|       - | 3677 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|       - | 3678 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|       - | 3679 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|       - | 3680 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|       - | 3681 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|       - | 3682 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|       - | 3683 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|       - | 3684 | ` */` |
|     108 | 3685 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|       5 | 3686 | `{` |
|     117 | 3687 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|      39 | 3688 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      39 | 3689 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|      39 | 3690 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|     ! 0 | 3691 | `			break; /* reached an outer exec's / legacy handler */` |
|       - | 3692 | `		}` |
|      39 | 3693 | `		(void)SySetPop(&pVm->aException);` |
|      39 | 3694 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|      39 | 3695 | `		if( pT->iHasFinally ){` |
|      35 | 3696 | `			*pPc = pT->iFinallyPc;` |
|      35 | 3697 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|      35 | 3698 | `			return 1;` |
|       - | 3699 | `		}` |
|       - | 3700 | `		/* No finally: tear the try's transparent frame down now. */` |
|       5 | 3701 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     ! 0 | 3702 | `			VmLeaveFrame(&(*pVm));` |
|     ! 0 | 3703 | `		}` |
|       5 | 3704 | `		VmExcRelease(&(*pVm),pT);` |
|       1 | 3705 | `	}` |
|      81 | 3706 | `	return 0;` |
|      59 | 3707 | `}` |
|       - | 3708 | `/*` |
|       - | 3709 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|       - | 3710 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|       - | 3711 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|       - | 3712 | ` *` |
|       - | 3713 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|       - | 3714 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|       - | 3715 | ` *    and redirect to the catch body (iHandlerPc).` |
|       - | 3716 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|       - | 3717 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|       - | 3718 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|       - | 3719 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|       - | 3720 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|       - | 3721 | ` */` |
|       - | 3722 | `/*` |
|       - | 3723 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|       - | 3724 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|       - | 3725 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|       - | 3726 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|       - | 3727 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|       - | 3728 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|       - | 3729 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|       - | 3730 | ` * case) is unchanged: no wrapper.` |
|       - | 3731 | ` */` |
|   20230 | 3732 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|       5 | 3733 | `{` |
|   20235 | 3734 | `	VmFrame *pWrap = 0;` |
|       - | 3735 | `	VmFrame *pThrowSite;` |
|       - | 3736 | `	sxi32 rc;` |
|   20235 | 3737 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|   20133 | 3738 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 3739 | `	}` |
|     104 | 3740 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|       - | 3741 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|     ! 0 | 3742 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 3743 | `	}` |
|     104 | 3744 | `	pThrowSite = pWrap->pParent;` |
|     104 | 3745 | `	pWrap->pParent = pOwner;` |
|     104 | 3746 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|     104 | 3747 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|       - | 3748 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|       - | 3749 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|       - | 3750 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|       - | 3751 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|     104 | 3752 | `	if( pVm->pFrame == pWrap ){` |
|     104 | 3753 | `		VmLeaveFrame(&(*pVm));` |
|      51 | 3754 | `	}` |
|     104 | 3755 | `	pVm->pFrame = pThrowSite;` |
|     104 | 3756 | `	return rc;` |
|   10120 | 3757 | `}` |
|       - | 3758 | `/*` |
|       - | 3759 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|       - | 3760 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|       - | 3761 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|       - | 3762 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|       - | 3763 | ` */` |
|       - | 3764 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      86 | 3765 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|       - | 3766 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|       5 | 3767 | `{` |
|      91 | 3768 | `	if( pCatch ){` |
|      75 | 3769 | `		pException->iInCatch = 1;` |
|      75 | 3770 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|      75 | 3771 | `		if( pThis ){ pThis->iRef++; }` |
|      75 | 3772 | `		pException->pInflight = pThis;` |
|      75 | 3773 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      75 | 3774 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|      75 | 3775 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      75 | 3776 | `		return SXRET_OK;` |
|       - | 3777 | `	}` |
|      20 | 3778 | `	if( pException->iHasFinally ){` |
|       - | 3779 | `		VmFinallyAction sAct;` |
|      15 | 3780 | `		SyZero(&sAct,sizeof(sAct));` |
|      15 | 3781 | `		sAct.eKind = PH7_FA_RETHROW;` |
|      15 | 3782 | `		if( pThis ){ pThis->iRef++; }` |
|      15 | 3783 | `		sAct.pExc = pThis;` |
|      15 | 3784 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|      15 | 3785 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|      15 | 3786 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|      15 | 3787 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|      15 | 3788 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|      15 | 3789 | `		return SXRET_OK;` |
|       - | 3790 | `	}` |
|       - | 3791 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|       - | 3792 | `	 * flat native stack instead of mutual recursion. */` |
|       6 | 3793 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|     ! 0 | 3794 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 | 3795 | `	}` |
|       6 | 3796 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       6 | 3797 | `	return VM_THROW_KEEP_UNWINDING;` |
|      48 | 3798 | `}` |
| 1447440 | 3799 | `PH7_PRIVATE sxi32 VmThrowException(` |
|       - | 3800 | `	ph7_vm *pVm,              /* Target VM */` |
|       - | 3801 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|       - | 3802 | `	)` |
|       5 | 3803 | `{` |
|       - | 3804 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|       - | 3805 | `	ph7_exception **apException;` |
|  723720 | 3806 | `	ph7_exception *pException;` |
|   50096 | 3807 | `Rethrow:` |
|       - | 3808 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|       - | 3809 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|       - | 3810 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|       - | 3811 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|       - | 3812 | `	 * so the throw path must be too). */` |
|       - | 3813 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|       - | 3814 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|       - | 3815 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
| 1547637 | 3816 | `	VmCoalesceDisarm(pVm);` |
|       - | 3817 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|       - | 3818 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|       - | 3819 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|       - | 3820 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|       - | 3821 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|       - | 3822 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|       - | 3823 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|       - | 3824 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
| 1547632 | 3825 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|      25 | 3826 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|      19 | 3827 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|       8 | 3828 | `	}` |
|       - | 3829 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|       - | 3830 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|       - | 3831 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|       - | 3832 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|       - | 3833 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|       - | 3834 | `	 * that owns the pending return, so it must leave that return intact. */` |
|       - | 3835 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|       - | 3836 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|       - | 3837 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|       - | 3838 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
| 1547637 | 3839 | `	pVm->pResumeFrame = 0;` |
|       - | 3840 | `	/* Point to the stack of loaded exceptions */` |
| 1547637 | 3841 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
| 1547637 | 3842 | `	pException = 0;` |
| 1547637 | 3843 | `	pCatch = 0;` |
| 1547637 | 3844 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 3845 | `		ph7_exception_block *aCatch;` |
|       - | 3846 | `		ph7_class *pClass;` |
|       - | 3847 | `		SyString *aNames;` |
|       - | 3848 | `		sxu32 nNames;` |
|       - | 3849 | `		int matched;` |
|       - | 3850 | `		sxu32 j,k;` |
|       - | 3851 | `		/* Locate the appropriate block to execute */` |
| 1446987 | 3852 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
| 1446987 | 3853 | `		(void)SySetPop(&pVm->aException);` |
| 1446987 | 3854 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       - | 3855 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|       - | 3856 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|       - | 3857 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
| 1447005 | 3858 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       - | 3859 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
| 1426827 | 3860 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
| 1426827 | 3861 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
| 1426827 | 3862 | `			matched = 0;` |
| 1426869 | 3863 | `			for( k = 0 ; k < nNames ; ++k ){` |
|       - | 3864 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|       - | 3865 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|       - | 3866 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
| 1426851 | 3867 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
| 1426851 | 3868 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 3869 | `					/* No such class, or trait — cannot match */` |
|     ! 0 | 3870 | `					continue;` |
|       - | 3871 | `				}` |
| 1426851 | 3872 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
| 1426809 | 3873 | `					matched = 1;` |
| 1426809 | 3874 | `					break;` |
|       - | 3875 | `				}` |
|      24 | 3876 | `			}` |
| 1426827 | 3877 | `			if( matched ){` |
|       - | 3878 | `				/* Catch block found,break immediately */` |
| 1426809 | 3879 | `				pCatch = &aCatch[j];` |
| 1426809 | 3880 | `				break;` |
|       - | 3881 | `			}` |
|      12 | 3882 | `		}` |
|  723491 | 3883 | `	}` |
|       - | 3884 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|       - | 3885 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|       - | 3886 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|       - | 3887 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|       - | 3888 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|       - | 3889 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|       - | 3890 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
| 1547637 | 3891 | `	if( pException ){` |
| 1446987 | 3892 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|  723491 | 3893 | `	}` |
|       - | 3894 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|       - | 3895 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|       - | 3896 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
| 1547637 | 3897 | `	if( pException && pException->iInlined ){` |
|      91 | 3898 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      91 | 3899 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|       - | 3900 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|       6 | 3901 | `			goto Rethrow;` |
|       - | 3902 | `		}` |
|      87 | 3903 | `		return rcInline;` |
|       - | 3904 | `	}` |
|       - | 3905 | `	/* Execute the cached block if available */` |
| 1547551 | 3906 | `	if( pCatch == 0 ){` |
|       - | 3907 | `		sxi32 rc;` |
|       - | 3908 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|  120817 | 3909 | `		if( pException && pException->iHasFinally ){` |
|   20163 | 3910 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|   20163 | 3911 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|   20163 | 3912 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|   20163 | 3913 | `			pException->iFinallyDone = 1;` |
|       - | 3914 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|       - | 3915 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|   20163 | 3916 | `			pVm->pInflightException = pThis;` |
|   20163 | 3917 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 3918 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|       - | 3919 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|   20163 | 3920 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|   20163 | 3921 | `			pVm->pInflightException = pSaveInflight;` |
|   20163 | 3922 | `			pVm->nInflightExcBase = nSaveBase;` |
|   20163 | 3923 | `			if( rc == SXERR_ABORT ){` |
|       3 | 3924 | `				VmExcRelease(&(*pVm),pException);` |
|       3 | 3925 | `				return SXERR_ABORT;` |
|       - | 3926 | `			}` |
|       - | 3927 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|       - | 3928 | `			 * semantics). The finally stored it on the body frame it returns from` |
|       - | 3929 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|       - | 3930 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|       - | 3931 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|       - | 3932 | `			 * takes the value instead of unwinding) and resume in place.` |
|       - | 3933 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|       - | 3934 | `			 * the same transport an in-place catch uses — and unwind as an` |
|       - | 3935 | `			 * exception; the owner's activation consumes the resume` |
|       - | 3936 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|       - | 3937 | `			 * its bHasRet tail materializes the return. */` |
|       - | 3938 | `			{` |
|   20161 | 3939 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   20161 | 3940 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|   20161 | 3941 | `				if( pOwnerFrame->bHasRet ){` |
|   20029 | 3942 | `					if( pOwnerFrame == pThrowFrame ){` |
|   20026 | 3943 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|       - | 3944 | `						/* Record the landing pad like the cross-frame case below.` |
|       - | 3945 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|       - | 3946 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|       - | 3947 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|       - | 3948 | `						 * its router unwound as an exception and the Unwind discard` |
|       - | 3949 | ``						 * dropped the parked return — `function f(){ try {`` |
|       - | 3950 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|       - | 3951 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|       - | 3952 | `						 * try frame down and its bHasRet tail materializes the` |
|       - | 3953 | `						 * return, same as the in-place-catch landing. */` |
|   20026 | 3954 | `						pVm->pResumeFrame = pOwnerFrame;` |
|   20026 | 3955 | `						pVm->iResumePc = pException->iLandingPc;` |
|   20026 | 3956 | `						pVm->pResumeInstr = pException->pOwnerInstr;` |
|   20026 | 3957 | `						pVm->iResumeStackDepth = pException->iStackDepth;` |
|   20026 | 3958 | `						VmExcRelease(&(*pVm),pException);` |
|   20026 | 3959 | `						return SXRET_OK;` |
|       - | 3960 | `					}` |
|       3 | 3961 | `					pVm->pResumeFrame = pOwnerFrame;` |
|       3 | 3962 | `					pVm->iResumePc = pException->iLandingPc;` |
|       3 | 3963 | `					pVm->pResumeInstr = pException->pOwnerInstr;` |
|       3 | 3964 | `					pVm->iResumeStackDepth = pException->iStackDepth;` |
|       3 | 3965 | `					VmExcRelease(&(*pVm),pException);` |
|       3 | 3966 | `					return PH7_EXCEPTION;` |
|       - | 3967 | `				}` |
|       - | 3968 | `			}` |
|       - | 3969 | `			/* The finally threw an exception that superseded pThis — it either` |
|       - | 3970 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|       - | 3971 | `			 * (which consumed an entry from the exception stack). Either way the` |
|       - | 3972 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|       - | 3973 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|     135 | 3974 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|      17 | 3975 | `				VmExcRelease(&(*pVm),pException);` |
|      17 | 3976 | `				return PH7_EXCEPTION;` |
|       - | 3977 | `			}` |
|      58 | 3978 | `		}` |
|       - | 3979 | `		/* Check if there is an outer exception handler on the stack */` |
|  100775 | 3980 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|       - | 3981 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|       - | 3982 | `			 * iteration per unwound level instead of one native frame. */` |
|     123 | 3983 | `			VmExcRelease(&(*pVm),pException);` |
|     123 | 3984 | `			goto Rethrow;` |
|       - | 3985 | `		}` |
|       - | 3986 | `		/* No outer handler. If the handlers were temporarily hidden` |
|       - | 3987 | `		 * (catch body re-throw with finally pending), defer the` |
|       - | 3988 | `		 * exception instead of reporting it uncaught.` |
|       - | 3989 | `		 */` |
|  100655 | 3990 | `		if( pVm->pPendingException == 0 && pThis ){` |
|       - | 3991 | `			/* Check if we are inside a catch execution with hidden handlers` |
|       - | 3992 | `			 * by looking for a catch frame on the stack.` |
|       - | 3993 | `			 */` |
|  100655 | 3994 | `			VmFrame *pF = pVm->pFrame;` |
|  100655 | 3995 | `			int inCatch = 0;` |
|  101277 | 3996 | `			while( pF ){` |
|  100699 | 3997 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|  100076 | 3998 | `					inCatch = 1;` |
|  100076 | 3999 | `					break;` |
|       - | 4000 | `				}` |
|     626 | 4001 | `				pF = pF->pParent;` |
|       4 | 4002 | `			}` |
|  100655 | 4003 | `			if( inCatch ){` |
|       - | 4004 | `				/* Defer — will be re-thrown after finally runs */` |
|  100076 | 4005 | `				pThis->iRef++;` |
|  100076 | 4006 | `				pVm->pPendingException = pThis;` |
|  100076 | 4007 | `				VmExcRelease(&(*pVm),pException);` |
|  100076 | 4008 | `				return SXRET_OK;` |
|       - | 4009 | `			}` |
|     289 | 4010 | `		}` |
|       - | 4011 | `		/* Truly uncaught */` |
|     582 | 4012 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|     582 | 4013 | `		if( rc == SXRET_OK && pException ){` |
|     ! 0 | 4014 | `			VmFrame *pFrame = pVm->pFrame;` |
|     ! 0 | 4015 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|     ! 0 | 4016 | `			if( pException->pFrame == pFrame ){` |
|     ! 0 | 4017 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|     ! 0 | 4018 | `			}` |
|     ! 0 | 4019 | `		}` |
|     582 | 4020 | `		VmExcRelease(&(*pVm),pException);` |
|     582 | 4021 | `		return rc;` |
|     ! 0 | 4022 | `	}else{` |
| 1426739 | 4023 | `		VmFrame *pFrame = pVm->pFrame;` |
| 1426739 | 4024 | `		ph7_exception **apSaved = 0;` |
|       - | 4025 | `		sxu32 nSavedCount;` |
|       - | 4026 | `		sxi32 rc;` |
|       - | 4027 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|       - | 4028 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|       - | 4029 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|       - | 4030 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|       - | 4031 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
| 1426739 | 4032 | `		VmFrame *pCatchBody = pException->pFrame;` |
| 1426739 | 4033 | `		sxu32 iCatchPc = pException->iLandingPc;` |
| 1426739 | 4034 | `		void *pCatchInstr = pException->pOwnerInstr;` |
| 1426739 | 4035 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
| 1426739 | 4036 | `		if( pException->pFrame == pFrame ){` |
|  825545 | 4037 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|  412770 | 4038 | `		}` |
|       - | 4039 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|       - | 4040 | `		 * body re-throws, the exception does not immediately propagate past` |
|       - | 4041 | `		 * our finally block. We save the stack contents and restore after.` |
|       - | 4042 | `		 */` |
| 1426739 | 4043 | `		nSavedCount = SySetUsed(&pVm->aException);` |
| 1426739 | 4044 | `		if( nSavedCount > 0 ){` |
|  150149 | 4045 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   50048 | 4046 | `				nSavedCount * sizeof(ph7_exception *));` |
|  100101 | 4047 | `			if( apSaved ){` |
|  150149 | 4048 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|   50048 | 4049 | `					nSavedCount * sizeof(ph7_exception *));` |
|  100101 | 4050 | `				SySetReset(&pVm->aException);` |
|   50048 | 4051 | `			}` |
|   50048 | 4052 | `		}` |
|       - | 4053 | `		/* Create the catch frame (made transparent below) */` |
| 1426739 | 4054 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
| 1426739 | 4055 | `		if( rc == SXRET_OK ){` |
|       - | 4056 | `			ph7_value *pObj;` |
|       - | 4057 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|       - | 4058 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|       - | 4059 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|       - | 4060 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|       - | 4061 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|       - | 4062 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|       - | 4063 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|       - | 4064 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|       - | 4065 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
| 1426739 | 4066 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|       - | 4067 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|       - | 4068 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|       - | 4069 | `			 * against the live current scope rather than a freed frame. */` |
| 1426739 | 4070 | `			if( pCatchBody ){` |
| 1426739 | 4071 | `				pFrame->pParent = pCatchBody;` |
|  713367 | 4072 | `			}` |
|       - | 4073 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|       - | 4074 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|       - | 4075 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|       - | 4076 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|       - | 4077 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|       - | 4078 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|       - | 4079 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|       - | 4080 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
| 1426739 | 4081 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|       - | 4082 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
| 2140106 | 4083 | `			pObj = (pCatch->sThis.nByte > 0)` |
| 1426732 | 4084 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
| 1426739 | 4085 | `			if( pObj ){` |
|       - | 4086 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|       - | 4087 | `				 * so it may already hold a value from a prior catch or assignment.` |
|       - | 4088 | `				 * Pin the new instance, then release the slot's prior contents` |
|       - | 4089 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|       - | 4090 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|       - | 4091 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
| 1426735 | 4092 | `				pThis->iRef++;` |
| 1426735 | 4093 | `				PH7_MemObjRelease(pObj);` |
| 1426735 | 4094 | `				pObj->x.pOther = pThis;` |
| 1426735 | 4095 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|  713365 | 4096 | `			}` |
|       - | 4097 | `			/* Execute the catch block */` |
| 1426739 | 4098 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|       - | 4099 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|       - | 4100 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|       - | 4101 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|       - | 4102 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|       - | 4103 | `			 * unbalanced — never pop somebody else's frame. */` |
| 1426739 | 4104 | `			if( pVm->pFrame == pFrame ){` |
| 1426739 | 4105 | `				VmLeaveFrame(&(*pVm));` |
|  713367 | 4106 | `			}` |
| 1426739 | 4107 | `			pVm->pFrame = pThrowSite;` |
|  713367 | 4108 | `		}` |
|       - | 4109 | `		/* Restore the outer exception handlers */` |
| 1426739 | 4110 | `		if( apSaved ){` |
|       - | 4111 | `			sxu32 k;` |
|       - | 4112 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|       - | 4113 | `			 * the catch body) are normally already consumed; on an abnormal` |
|       - | 4114 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|       - | 4115 | `			 * linger — release those activations before discarding the set. */` |
|  100101 | 4116 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|  100101 | 4117 | `			SySetReset(&pVm->aException);` |
|  200849 | 4118 | `			for(k = 0; k < nSavedCount; k++){` |
|  100753 | 4119 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|   50379 | 4120 | `			}` |
|  100101 | 4121 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|   50048 | 4122 | `		}` |
|       - | 4123 | `		/* Execute the finally block after catch */` |
| 1426739 | 4124 | `		if( pException->iHasFinally ){` |
|       - | 4125 | `			sxi32 rcf;` |
|       - | 4126 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|       - | 4127 | `			 * from, its pending-return write generation (set if the catch above` |
|       - | 4128 | `			 * returned), and the exception-stack depth. After the finally we use` |
|       - | 4129 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|       - | 4130 | `			 * catch-return. */` |
|       - | 4131 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|       - | 4132 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|       - | 4133 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|       - | 4134 | `			 * supersede decision belong to the owner, not the thrower. */` |
|      77 | 4135 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|      77 | 4136 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|      77 | 4137 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|       - | 4138 | `			/* The exception in flight while this finally runs is the catch body's` |
|       - | 4139 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|       - | 4140 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|       - | 4141 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|       - | 4142 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|      77 | 4143 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|      77 | 4144 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|      77 | 4145 | `			pException->iFinallyDone = 1;` |
|      77 | 4146 | `			pVm->pInflightException = pVm->pPendingException;` |
|      77 | 4147 | `			pVm->nInflightExcBase = nExcBefore;` |
|       - | 4148 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|      77 | 4149 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|      77 | 4150 | `			pVm->pInflightException = pSaveInflight;` |
|      77 | 4151 | `			pVm->nInflightExcBase = nSaveBase;` |
|      77 | 4152 | `			if( rcf == SXERR_ABORT ){` |
|     ! 0 | 4153 | `				VmExcRelease(&(*pVm),pException);` |
|     ! 0 | 4154 | `				return SXERR_ABORT;` |
|       - | 4155 | `			}` |
|       - | 4156 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|       - | 4157 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|       - | 4158 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|       - | 4159 | `			 * either case that exception supersedes this try's catch-return — but` |
|       - | 4160 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|       - | 4161 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|       - | 4162 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|      72 | 4163 | `			if( (rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore)` |
|      49 | 4164 | `			 && pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|      12 | 4165 | `				VmClearFrameReturn(pBody);` |
|       5 | 4166 | `			}` |
|      77 | 4167 | `			if( rcf == PH7_EXCEPTION ){` |
|       - | 4168 | `				/* The finally's exception propagated past this try; drop any deferred` |
|       - | 4169 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|       - | 4170 | `				 * reaches the frame that caught the finally's throw. */` |
|      19 | 4171 | `				if( pVm->pPendingException ){` |
|     ! 0 | 4172 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|     ! 0 | 4173 | `					pVm->pPendingException = 0;` |
|     ! 0 | 4174 | `				}` |
|      19 | 4175 | `				VmExcRelease(&(*pVm),pException);` |
|      19 | 4176 | `				return PH7_EXCEPTION;` |
|       - | 4177 | `			}` |
|      28 | 4178 | `		}` |
| 1426723 | 4179 | `		if( rc == SXERR_ABORT ){` |
|       5 | 4180 | `			VmExcRelease(&(*pVm),pException);` |
|       5 | 4181 | `			return SXERR_ABORT;` |
|       - | 4182 | `		}` |
|       - | 4183 | `		/* If the catch body re-threw, the exception was deferred in` |
|       - | 4184 | `		 * pPendingException (because outer handlers were hidden).` |
|       - | 4185 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|       - | 4186 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|       - | 4187 | `		 * the catch frame having been left above), which swallows the in-flight` |
|       - | 4188 | `		 * exception (PHP semantics).` |
|       - | 4189 | `		 */` |
| 1426719 | 4190 | `		if( pVm->pPendingException ){` |
|       - | 4191 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|  100076 | 4192 | `			if( !(pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame))->bHasRet ){` |
|  100072 | 4193 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|  100072 | 4194 | `				pVm->pPendingException = 0;` |
|  100072 | 4195 | `				VmExcRelease(&(*pVm),pException);` |
|       - | 4196 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|  100072 | 4197 | `				pThis = pReThrow;` |
|  100072 | 4198 | `				goto Rethrow;` |
|       - | 4199 | `			}` |
|       - | 4200 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|       6 | 4201 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|       6 | 4202 | `			pVm->pPendingException = 0;` |
|       2 | 4203 | `		}` |
|       - | 4204 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|       - | 4205 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|       - | 4206 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|       - | 4207 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
| 1326651 | 4208 | `		pVm->pResumeFrame = pCatchBody;` |
| 1326651 | 4209 | `		pVm->iResumePc = iCatchPc;` |
| 1326651 | 4210 | `		pVm->pResumeInstr = pCatchInstr;` |
| 1326651 | 4211 | `		pVm->iResumeStackDepth = pException->iStackDepth;` |
|       - | 4212 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|       - | 4213 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|       - | 4214 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
| 1326651 | 4215 | `		VmExcRelease(&(*pVm),pException);` |
|       - | 4216 | `	}` |
| 1326651 | 4217 | `	return SXRET_OK;` |
|  723725 | 4218 | `}` |
|       - | 4219 |  |
