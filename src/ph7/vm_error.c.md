# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2940/3312 lines (88.77%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 |  |
|        - |    8 | `/* memory_limit: php's one sentence for an exhausted ceiling. Defined below, next to` |
|        - |    9 | ` * the va_list raise funnel; declared here because the plain-message funnel is first. */` |
|        - |   10 | `static int VmMemLimitMessage(ph7_vm *pVm,char *zBuf,sxu32 nBuf);` |
|        - |   11 | `/*` |
|        - |   12 | ` * Section:` |
|        - |   13 | ` *    Error, diagnostics and type-enforcement machinery: PH7_VmThrowError` |
|        - |   14 | ` *    and the error-handler invocation path, enum materialization and` |
|        - |   15 | ` *    on-demand class constants, scalar/union/property/constant/return` |
|        - |   16 | ` *    type enforcement, the TypeError/ArgumentCountError throwers,` |
|        - |   17 | ` *    uncaught-exception rendering, VmBuildBacktrace, and the exception` |
|        - |   18 | ` *    core VmUncaughtException/VmThrowException.` |
|        - |   19 | ` * Status:` |
|        - |   20 | ` *    Stable.` |
|        - |   21 | ` */` |
|        - |   22 | `/*` |
|        - |   23 | ` * Remember a diagnostic for error_get_last(). php records the last error that reached` |
|        - |   24 | ` * DEFAULT processing: one hidden by '@' or by error_reporting() still counts, but one a` |
|        - |   25 | ` * user handler claimed (by returning true) does not -- so this is called only on the` |
|        - |   26 | ` * default-processing path.` |
|        - |   27 | ` */` |
|    30728 |   28 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile,sxu32 nLine)` |
|        5 |   29 | `{` |
|    30733 |   30 | `	pVm->nLastErrType = iErr;` |
|    30733 |   31 | `	pVm->nLastErrLine = nLine;` |
|    30733 |   32 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    30733 |   33 | `	if( zMsg && nMsg > 0 ){` |
|    30733 |   34 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|    15361 |   35 | `	}` |
|    30733 |   36 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    30733 |   37 | `	if( pFile ){` |
|    30733 |   38 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|    15361 |   39 | `	}` |
|    30733 |   40 | `}` |
|        - |   41 | `/*` |
|        - |   42 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|        - |   43 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|        - |   44 | ` * diagnostic is never silently swallowed.` |
|        - |   45 | ` */` |
|      880 |   46 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|        4 |   47 | `{` |
|      884 |   48 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|        4 |   49 | `}` |
|        - |   50 | `/*` |
|        - |   51 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|        - |   52 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|        - |   53 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|        - |   54 | ` * headers_sent()/output accounting).` |
|        - |   55 | ` */` |
|      906 |   56 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|        4 |   57 | `{` |
|        - |   58 | `	sxi32 rc;` |
|        - |   59 | `	/* Append a new line */` |
|        - |   60 | `#ifdef __WINNT__` |
|        4 |   61 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |   62 | `#else` |
|      906 |   63 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |   64 | `#endif` |
|        - |   65 | `	/* Invoke the output consumer callback */` |
|      910 |   66 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      910 |   67 | `	if( bTrack ){` |
|       29 |   68 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|       13 |   69 | `	}` |
|      910 |   70 | `	return rc;` |
|        4 |   71 | `}` |
|        - |   72 | `/*` |
|        - |   73 | ` * Route an already-formatted diagnostic blob (the uncaught-exception path builds` |
|        - |   74 | `` * php's `PHP Fatal error:  Uncaught ...` LOG shape itself) to the error stream`` |
|        - |   75 | ` * when log_errors is on, else to the program-output stream when display_errors` |
|        - |   76 | ` * is on, else drop it -- matching php's stock-CLI gate for fatals (stderr only).` |
|        - |   77 | ` */` |
|      608 |   78 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |   79 | `{` |
|      612 |   80 | `	if( pVm->bLogErrors ){` |
|      612 |   81 | `		return VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pMsg,0);` |
|        - |   82 | `	}` |
|      ! 0 |   83 | `	if( pVm->bDisplayErrors ){` |
|      ! 0 |   84 | `		return VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pMsg,1);` |
|        - |   85 | `	}` |
|      ! 0 |   86 | `	return SXRET_OK;` |
|      308 |   87 | `}` |
|        - |   88 | `/*` |
|        - |   89 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |   90 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |   91 | ` * information.` |
|        - |   92 | ` */` |
|    36038 |   93 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |   94 | `{` |
|        - |   95 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|        - |   96 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|        - |   97 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|        - |   98 | `	 * straight through to the engine's own reporting, which is what returning` |
|        - |   99 | `	 * TRUE below means. */` |
|    36038 |  100 | `	if( ph7_value_is_callable(&pVm->sErrCB)` |
|    20686 |  101 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
|        - |  102 | `		ph7_value apArg[4];` |
|        - |  103 | `		ph7_value *apArgPtr[4];` |
|        - |  104 | `		ph7_value sResult;` |
|        - |  105 | `		ph7_value sRunning;` |
|        - |  106 | `		SyString sErr;` |
|        - |  107 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|        - |  108 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|        - |  109 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|        - |  110 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|        - |  111 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|     5319 |  112 | `		if( iErr == PH7_CTX_NOTICE ){` |
|      439 |  113 | `			iErr = 8; /* E_NOTICE */` |
|      217 |  114 | `		}` |
|        - |  115 | `		/* Prepare arguments */` |
|     5319 |  116 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  117 | `			/* use explicit message length to avoid reading past buffer */` |
|     5319 |  118 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     5319 |  119 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     5319 |  120 | `		if( pFile ){` |
|     5319 |  121 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     5319 |  122 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     2646 |  123 | `		}else{` |
|      ! 0 |  124 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  125 | `		}` |
|     5319 |  126 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     5319 |  127 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  128 | `		/* Set up pointer array */` |
|     5319 |  129 | `		apArgPtr[0] = &apArg[0];` |
|     5319 |  130 | `		apArgPtr[1] = &apArg[1];` |
|     5319 |  131 | `		apArgPtr[2] = &apArg[2];` |
|     5319 |  132 | `		apArgPtr[3] = &apArg[3];` |
|        - |  133 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|        - |  134 | `		 * handler itself raises reaches the engine's reporting instead of` |
|        - |  135 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|        - |  136 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|        - |  137 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|        - |  138 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|        - |  139 | `		 * the handler installed itself STAYS. */` |
|     5319 |  140 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     5319 |  141 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|     5319 |  142 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|     5319 |  143 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|        - |  144 | `		/* Call the handler */` |
|        - |  145 | `		{` |
|     5319 |  146 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|     5319 |  147 | `			if( !ph7_value_is_callable(&pVm->sErrCB) ){` |
|     5317 |  148 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     2640 |  149 | `			}` |
|     5319 |  150 | `			PH7_MemObjRelease(&sRunning);` |
|     5319 |  151 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|        - |  152 | `				/* The handler threw (or aborted) instead of returning: php never` |
|        - |  153 | `				 * reports the original diagnostic then — the exception supersedes` |
|        - |  154 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|        - |  155 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|        - |  156 | `				 * user's catch already ran. */` |
|        5 |  157 | `				PH7_MemObjRelease(&apArg[0]);` |
|        5 |  158 | `				PH7_MemObjRelease(&apArg[1]);` |
|        5 |  159 | `				PH7_MemObjRelease(&apArg[2]);` |
|        5 |  160 | `				PH7_MemObjRelease(&apArg[3]);` |
|        5 |  161 | `				PH7_MemObjRelease(&sResult);` |
|        5 |  162 | `				return FALSE;` |
|        - |  163 | `			}` |
|        - |  164 | `		}` |
|        - |  165 | `		/* Check return value */` |
|     5315 |  166 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  167 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  168 | `		}` |
|        - |  169 | `		/* Release */` |
|     5315 |  170 | `		PH7_MemObjRelease(&apArg[0]);` |
|     5315 |  171 | `		PH7_MemObjRelease(&apArg[1]);` |
|     5315 |  172 | `		PH7_MemObjRelease(&apArg[2]);` |
|     5315 |  173 | `		PH7_MemObjRelease(&apArg[3]);` |
|     5315 |  174 | `		PH7_MemObjRelease(&sResult);` |
|        - |  175 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  176 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|     5315 |  177 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  178 | `	}` |
|        - |  179 | `	/* No handler, always call error handler */` |
|    30729 |  180 | `	return TRUE;` |
|    18005 |  181 | `}` |
|        - |  182 | `/*` |
|        - |  183 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|        - |  184 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|        - |  185 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|        - |  186 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|        - |  187 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|        - |  188 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|        - |  189 | ` * severity reclassification is the remaining §6 audit tail. Note the raw` |
|        - |  190 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|        - |  191 | ` * only picks the DISPLAY label.` |
|        - |  192 | ` */` |
|        - |  193 | `/*` |
|        - |  194 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|        - |  195 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|        - |  196 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|        - |  197 | ` * every deprecation.` |
|        - |  198 | ` */` |
|    31376 |  199 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|        5 |  200 | `{` |
|    31381 |  201 | `	switch( iErr ){` |
|    15151 |  202 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|    30269 |  203 | `		return 2;` |
|       49 |  204 | `	case 512  /* E_USER_WARNING */:` |
|      101 |  205 | `		return 512;` |
|      267 |  206 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|        - |  207 | `	case 8    /* E_NOTICE */:` |
|      539 |  208 | `		return 8;` |
|       37 |  209 | `	case 1024 /* E_USER_NOTICE */:` |
|       78 |  210 | `		return 1024;` |
|      166 |  211 | `	case 8192 /* E_DEPRECATED */:` |
|      337 |  212 | `		return 8192;` |
|       23 |  213 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       48 |  214 | `		return 16384;` |
|      ! 0 |  215 | `	case 256  /* E_USER_ERROR */:` |
|      ! 0 |  216 | `		return 256;` |
|       14 |  217 | `	default:` |
|       32 |  218 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|        - |  219 | `	}` |
|    15674 |  220 | `}` |
|    30728 |  221 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|        5 |  222 | `{` |
|    30733 |  223 | `	if( !pVm->bErrReport ){` |
|     4685 |  224 | `		return 0;` |
|        - |  225 | `	}` |
|    26051 |  226 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|    15366 |  227 | `}` |
|      298 |  228 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|        4 |  229 | `{` |
|      302 |  230 | `	switch(iErr){` |
|      110 |  231 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|        - |  232 | `	case 512  /* E_USER_WARNING */:` |
|      224 |  233 | `		return "Warning";` |
|       23 |  234 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|        - |  235 | `	case 8    /* E_NOTICE */:` |
|        - |  236 | `	case 1024 /* E_USER_NOTICE */:` |
|       50 |  237 | `		return "Notice";` |
|        2 |  238 | `	case 8192  /* E_DEPRECATED */:` |
|        - |  239 | `	case 16384 /* E_USER_DEPRECATED */:` |
|        5 |  240 | `		return "Deprecated";` |
|      ! 0 |  241 | `	case 256 /* E_USER_ERROR */:` |
|      ! 0 |  242 | `		return "Fatal error";` |
|       14 |  243 | `	default:` |
|       32 |  244 | `		return "Error";` |
|        - |  245 | `	}` |
|      153 |  246 | `}` |
|        - |  247 | `/*` |
|        - |  248 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|        - |  249 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|        - |  250 | `` * pending the §6 runtime-line-tracking gate (correct for `-r` one-liners;`` |
|        - |  251 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|        - |  252 | ` * be authored cross-engine with --EXPECTF--.` |
|        - |  253 | ` */` |
|       26 |  254 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|        3 |  255 | `{` |
|       29 |  256 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|       29 |  257 | `}` |
|        - |  258 | `/*` |
|        - |  259 | ` * WHERE a diagnostic happened, as php reports it.` |
|        - |  260 | ` *` |
|        - |  261 | ` * Normally the file being executed and the current line, with php's floor of 1 for a` |
|        - |  262 | ` * line the engine never recorded. But a diagnostic can be raised with no PHP frame` |
|        - |  263 | `` * under it at all -- php tests `EG(current_execute_data) == NULL` and then has nothing`` |
|        - |  264 | `` * to name, so `zend_get_executed_filename()` answers the literal string "Unknown" and`` |
|        - |  265 | ` * the line is 0. The shutdown destructor pass is where PHL reaches that state: the` |
|        - |  266 | ` * refusal of a non-public __destruct is raised BETWEEN bodies, after the program's last` |
|        - |  267 | ` * statement. A diagnostic raised INSIDE a destructor body has a frame again and reports` |
|        - |  268 | ` * its real file and line, which is why this is a flag the raise site sets and not the` |
|        - |  269 | ` * whole phase.` |
|        - |  270 | ` */` |
|    36038 |  271 | `static sxu32 VmDiagnosticWhere(ph7_vm *pVm,SyString **ppFile)` |
|        5 |  272 | `{` |
|        - |  273 | `	static SyString sNoFrame = { "Unknown", sizeof("Unknown")-1 };` |
|    36043 |  274 | `	if( pVm->bNoFrameLoc ){` |
|        8 |  275 | `		*ppFile = &sNoFrame;` |
|        8 |  276 | `		return 0;` |
|        - |  277 | `	}` |
|        - |  278 | `	{` |
|        - |  279 | `		/* The file the RUNNING code is in, which is the defining file of the` |
|        - |  280 | `		 * innermost active function -- not the top of the include stack. The two` |
|        - |  281 | `		 * agree only while top-level code is running: once a call reaches a` |
|        - |  282 | `		 * function defined in another unit, the include stack has moved on, so` |
|        - |  283 | `		 * every diagnostic raised inside a library named the ENTRY SCRIPT. In a` |
|        - |  284 | `		 * composer tree that is every warning any vendor package raises, and it is` |
|        - |  285 | `		 * what a framework's error handler logs. The line was already right. */` |
|    36037 |  286 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|    36037 |  287 | `		if( pUnit && pUnit->nByte > 0 ){` |
|    36037 |  288 | `			*ppFile = pUnit;` |
|    17997 |  289 | `		}` |
|        - |  290 | `	}` |
|    36037 |  291 | `	return pVm->nCurLine ? pVm->nCurLine : 1;` |
|    18005 |  292 | `}` |
|      298 |  293 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|        4 |  294 | `{` |
|      302 |  295 | `	if( pFile ){` |
|        - |  296 | `		/* nLine arrives already normalized by VmDiagnosticWhere: "no line recorded"` |
|        - |  297 | `		 * is php's 1, and a raise with no frame under it is php's literal 0. */` |
|      302 |  298 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|      149 |  299 | `	}` |
|      302 |  300 | `}` |
|        - |  301 | `/*` |
|        - |  302 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|        - |  303 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|        - |  304 | ` * (compile.c) and stock CLI's stderr log copy.` |
|        - |  305 | ` */` |
|      272 |  306 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  307 | `{` |
|      276 |  308 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      276 |  309 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      276 |  310 | `}` |
|        - |  311 | `/*` |
|        - |  312 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|        - |  313 | ` *` |
|        - |  314 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|        - |  315 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|        - |  316 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|        - |  317 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|        - |  318 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|        - |  319 | ` * php never produces. Building it into the message here is the single place that` |
|        - |  320 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|        - |  321 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|        - |  322 | ` */` |
|    35081 |  323 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|        5 |  324 | `{` |
|    35086 |  325 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     1751 |  326 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     1751 |  327 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      863 |  328 | `	}` |
|    35086 |  329 | `}` |
|        - |  330 | `/*` |
|        - |  331 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|        - |  332 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|        - |  333 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|        - |  334 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|        - |  335 | ` *   - DISPLAY copy -> the program-output (stdout) stream when display_errors is on:` |
|        - |  336 | ``  *                     `\nLABEL: BODY in FILE on line N` `` |
|        - |  337 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|        - |  338 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|        - |  339 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|        - |  340 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|        - |  341 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|        - |  342 | ` */` |
|      294 |  343 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|        - |  344 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|        4 |  345 | `{` |
|      298 |  346 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      298 |  347 | `	sxi32 rc = SXRET_OK;` |
|      298 |  348 | `	if( pVm->bLogErrors ){` |
|      276 |  349 | `		SyBlobReset(pWorker);` |
|      276 |  350 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      276 |  351 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      276 |  352 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      276 |  353 | `		rc = VmWriteDiagnostic(pVm,VmErrConsumer(pVm),pWorker,0);` |
|      136 |  354 | `	}` |
|      298 |  355 | `	if( pVm->bDisplayErrors ){` |
|        - |  356 | `		sxi32 rc2;` |
|       29 |  357 | `		SyBlobReset(pWorker);` |
|        - |  358 | `		/* php's text-mode display copy is prefixed with a blank line */` |
|       29 |  359 | `		SyBlobAppend(pWorker,"\n",sizeof(char));` |
|       29 |  360 | `		VmDiagnosticHeader(pWorker,iErr);` |
|       29 |  361 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|       29 |  362 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|       29 |  363 | `		rc2 = VmWriteDiagnostic(pVm,&pVm->sVmConsumer,pWorker,1);` |
|        - |  364 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|        - |  365 | `		 * than letting a later successful write mask it */` |
|       29 |  366 | `		if( rc == SXRET_OK ){` |
|       29 |  367 | `			rc = rc2;` |
|       13 |  368 | `		}` |
|       13 |  369 | `	}` |
|      298 |  370 | `	return rc;` |
|        4 |  371 | `}` |
|     1594 |  372 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  373 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  374 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  375 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  376 | `	const char *zMessage /* Null terminated error message */` |
|        - |  377 | `	)` |
|        5 |  378 | `{` |
|        - |  379 | `	SyBlob sMsg;` |
|        - |  380 | `	SyString *pFile;` |
|        - |  381 | `	sxu32 nMsg;` |
|        - |  382 | `	sxu32 nLine;` |
|     1599 |  383 | `	sxi32 rc = SXRET_OK;` |
|        - |  384 | `	char zMemMsg[128];` |
|     1599 |  385 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|        - |  386 | `		/* Every engine "out of memory" wording downstream of the ceiling becomes` |
|        - |  387 | `		 * php's one sentence. Severity 256 is what makes the label read "Fatal` |
|        - |  388 | `		 * error": PHL's label table maps E_ERROR(1) to its own "Error", and this` |
|        - |  389 | `		 * diagnostic is one users match against php's output, not against the` |
|        - |  390 | `		 * engine's house style. */` |
|      ! 0 |  391 | `		zMessage = zMemMsg;` |
|      ! 0 |  392 | `		iErr = 256;` |
|      ! 0 |  393 | `		pFuncName = 0;` |
|      ! 0 |  394 | `	}` |
|     1599 |  395 | `	nMsg = (sxu32)SyStrlen(zMessage);` |
|     1599 |  396 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  397 | `		/* Speculative evaluation (PH7_VmEvalConstExpr): the value is being LOOKED at,` |
|        - |  398 | `		 * not produced, so this diagnostic never happened. Count it -- the caller reads` |
|        - |  399 | `		 * the counter as "php's compiler would not have folded this". */` |
|      ! 0 |  400 | `		pVm->nSpecDiag++;` |
|      ! 0 |  401 | `		return SXRET_OK;` |
|        - |  402 | `	}` |
|        - |  403 | `	/* Peek the processed file if available */` |
|     1599 |  404 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1599 |  405 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|     1599 |  406 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     1599 |  407 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|        - |  408 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|        - |  409 | `		 * out-of-memory fatal through this path with none, and must not need` |
|        - |  410 | `		 * an allocation to say so. */` |
|      642 |  411 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      642 |  412 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      642 |  413 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      642 |  414 | `		nMsg = SyBlobLength(&sMsg);` |
|      314 |  415 | `	}` |
|        - |  416 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|        - |  417 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     1599 |  418 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)nLine) ){` |
|      209 |  419 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile,nLine);` |
|      209 |  420 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|        - |  421 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|        - |  422 | `			 * '@' php still runs the handler (done just above) but prints` |
|        - |  423 | `			 * nothing itself. */` |
|      138 |  424 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,nLine);` |
|       67 |  425 | `		}` |
|      102 |  426 | `	}` |
|     1599 |  427 | `	SyBlobRelease(&sMsg);` |
|     1599 |  428 | `	return rc;` |
|      796 |  429 | `}` |
|        - |  430 | `/*` |
|        - |  431 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  432 | ` *` |
|        - |  433 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  434 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  435 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  436 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  437 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  438 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  439 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  440 | ` * calling it from a VM op.` |
|        - |  441 | ` */` |
|      ! 0 |  442 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  443 | `{` |
|      ! 0 |  444 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  445 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  446 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  447 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  448 | `	return PH7_ABORT;` |
|      ! 0 |  449 | `}` |
|        - |  450 | `/*` |
|        - |  451 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  452 | ` */` |
|      ! 0 |  453 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  454 | `{` |
|      ! 0 |  455 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|        - |  459 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|        - |  460 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|        - |  461 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|        - |  462 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|        - |  463 | ` */` |
|        - |  464 | `/*` |
|        - |  465 | `` * TRUE when reading pVal as an `int` would lose what it holds -- the event php`` |
|        - |  466 | `` * 8.1 only DEPRECATES (`Implicit conversion from float 1.9 / float-string "1.9"`` |
|        - |  467 | `` * to int loses precision`) and §10 refuses outright.`` |
|        - |  468 | ` *` |
|        - |  469 | ` * Two kinds of value can lose something, and php treats them as one: a FLOAT,` |
|        - |  470 | ` * and a numeric STRING whose bytes spell a double. Which strings those are is` |
|        - |  471 | ` * not just the ones carrying a '.' or an exponent — an integer-shaped run too` |
|        - |  472 | ` * long for an int64 is a double in php too ("99999999999999999999"), and it` |
|        - |  473 | ` * loses digits exactly the same way. So the question is asked of the NUMBER the` |
|        - |  474 | ` * value converts to, whatever spelling it arrived in.` |
|        - |  475 | ` *` |
|        - |  476 | ` * A value is lossy when that number is not an exact int64: outside the range at` |
|        - |  477 | ` * all (NaN and the infinities included), or carrying a fraction. An integral` |
|        - |  478 | `` * float in range (`4.0 % 3`, `$o->i = 5.0`) loses nothing and is not lossy.`` |
|        - |  479 | ` */` |
|   130927 |  480 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|        5 |  481 | `{` |
|        - |  482 | `	ph7_real r;` |
|   130932 |  483 | `	if( pVal == 0 ){` |
|      ! 0 |  484 | `		return FALSE;` |
|        - |  485 | `	}` |
|   130932 |  486 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|       37 |  487 | `		r = pVal->rVal;` |
|   130937 |  488 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|        - |  489 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|        - |  490 | `		 * no, and a numeric conversion would replace it. */` |
|        - |  491 | `		ph7_value sProbe;` |
|        - |  492 | `		SyString sStr;` |
|        - |  493 | `		int bReal;` |
|   101183 |  494 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|   101183 |  495 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|        - |  496 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|        - |  497 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|        - |  498 | `		 * answers without building anything. Conservative on purpose: it may` |
|        - |  499 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|   101183 |  500 | `		if( n < 19 ){` |
|   303027 |  501 | `			for( i = 0 ; i < n ; ++i ){` |
|   201901 |  502 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|       25 |  503 | `					break;` |
|        - |  504 | `				}` |
|   100930 |  505 | `			}` |
|   101177 |  506 | `			if( i >= n ){` |
|   101135 |  507 | `				return FALSE;` |
|        - |  508 | `			}` |
|       23 |  509 | `		}` |
|       54 |  510 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       54 |  511 | `		PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|       54 |  512 | `		PH7_MemObjToNumeric(&sProbe);` |
|       54 |  513 | `		bReal = (sProbe.iFlags & MEMOBJ_REAL) != 0;` |
|       54 |  514 | `		r = sProbe.rVal;` |
|       54 |  515 | `		PH7_MemObjRelease(&sProbe);` |
|       54 |  516 | `		if( !bReal ){` |
|        9 |  517 | `			return FALSE;` |
|        - |  518 | `		}` |
|       24 |  519 | `	}else{` |
|    29720 |  520 | `		return FALSE;` |
|        - |  521 | `	}` |
|        - |  522 | `	/* The bounds are tested in DOUBLE space, BEFORE the cast: (sxi64)r is` |
|        - |  523 | `	 * undefined outside them, and a test of an undefined cast's result is one an` |
|        - |  524 | `	 * optimiser is entitled to delete -- which is exactly how the printf family's` |
|        - |  525 | `	 * PHP_INT_MIN guard disappeared (§2). NaN fails both comparisons and either` |
|        - |  526 | `	 * infinity fails one, so all three are lossy without a libm predicate. */` |
|        - |  527 | `	/* The cast is a no-op wherever ph7_real is the double this screen is written` |
|        - |  528 | `	 * for. Under PH7_OMIT_FLOATING_POINT ph7_real is sxi64, and handing an` |
|        - |  529 | `	 * integer to a double parameter is a narrowing MSVC reports as C4244 --` |
|        - |  530 | `	 * which /WX makes a build error, so the tiny build is where it bites. */` |
|       81 |  531 | `	if( !PH7_RealFitsInt64((double)r) ){` |
|       26 |  532 | `		return TRUE;` |
|        - |  533 | `	}` |
|       57 |  534 | `	return r != (ph7_real)(sxi64)r;` |
|    65450 |  535 | `}` |
|        - |  536 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|        - |  537 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|        - |  538 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|        - |  539 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|        - |  540 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|    30299 |  541 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|        5 |  542 | `{` |
|    30304 |  543 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|    30278 |  544 | `		return SXRET_OK;` |
|        - |  545 | `	}` |
|       27 |  546 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       26 |  547 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|        - |  548 | `			? "Implicit conversion from float to int loses precision"` |
|        - |  549 | `			: "Implicit conversion from float-string to int loses precision");` |
|    15136 |  550 | `}` |
|        - |  551 | `/*` |
|        - |  552 | ` * Single source of truth for the PHP call-depth cap policy (BYTECODE.md stage` |
|        - |  553 | ` * 5). Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|        - |  554 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|        - |  555 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|        - |  556 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|        - |  557 | ` */` |
|  2377702 |  558 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  559 | `{` |
|        - |  560 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|        - |  561 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  2377707 |  562 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|        - |  566 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|        - |  567 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|        - |  568 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|        - |  569 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|        - |  570 | ` * keep native re-entries off a finite C stack).` |
|        - |  571 | ` */` |
|  4533965 |  572 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|        5 |  573 | `{` |
|  4533970 |  574 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|        5 |  575 | `}` |
|        - |  576 | `/*` |
|        - |  577 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  578 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  579 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  580 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  581 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  582 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  583 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  584 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  585 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|        - |  586 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|        - |  587 | ` * (VmNativeNestingFatal).` |
|        - |  588 | ` *` |
|        - |  589 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  590 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  591 | ` * re-enter and loop.` |
|        - |  592 | ` */` |
|        2 |  593 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  594 | `{` |
|        3 |  595 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  596 | `		return PH7_ABORT;` |
|        - |  597 | `	}` |
|        3 |  598 | `	pVm->iExitStatus = 255;` |
|        3 |  599 | `	pVm->bHaltRequested = 1;` |
|        3 |  600 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        3 |  601 | `	return PH7_ABORT;` |
|        2 |  602 | `}` |
|        - |  603 | `/*` |
|        - |  604 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|        - |  605 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|        - |  606 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|        - |  607 | ` */` |
|        4 |  608 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|        2 |  609 | `{` |
|        6 |  610 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  611 | `		return PH7_ABORT;` |
|        - |  612 | `	}` |
|        6 |  613 | `	pVm->iExitStatus = 255;` |
|        6 |  614 | `	pVm->bHaltRequested = 1;` |
|        6 |  615 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|        6 |  616 | `	return PH7_ABORT;` |
|        4 |  617 | `}` |
|        - |  618 | `/*` |
|        - |  619 | `` * ext/pcntl's `Error installing signal handler for %d`, and it is the same`` |
|        - |  620 | ` * clean-halt fatal as the two above rather than a catchable Error -- because` |
|        - |  621 | ` * php's is not catchable either: a disposition sigaction() refuses (SIGKILL,` |
|        - |  622 | ` * SIGSTOP) is an E_ERROR under php, not the ValueError every other pcntl_signal()` |
|        - |  623 | ` * refusal is. Raised only from PH7_builtin_pcntl_signal.` |
|        - |  624 | ` */` |
|      ! 0 |  625 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo)` |
|      ! 0 |  626 | `{` |
|      ! 0 |  627 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  628 | `		return PH7_ABORT;` |
|        - |  629 | `	}` |
|      ! 0 |  630 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  631 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  632 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Error installing signal handler for %d",signo);` |
|      ! 0 |  633 | `	return PH7_ABORT;` |
|      ! 0 |  634 | `}` |
|        - |  635 | `/*` |
|        - |  636 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  637 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  638 | ` * information.` |
|        - |  639 | ` */` |
|        - |  640 | `/*` |
|        - |  641 | ` * Did memory_limit just stop this script, and if so what does php call it?` |
|        - |  642 | ` *` |
|        - |  643 | ` * The allocation that crossed the ceiling returned NULL into whichever site asked` |
|        - |  644 | ` * for it, and that site then complains in its OWN words -- "PH7 is running out of` |
|        - |  645 | ` * memory while loading variable", and a dozen others, each naming the operation` |
|        - |  646 | ` * that happened to be unlucky. php has ONE sentence for this, and it is the one` |
|        - |  647 | ` * every framework's OOM triage greps for, so the first diagnostic raised after the` |
|        - |  648 | ` * ceiling is hit becomes that sentence whatever the site meant to say.` |
|        - |  649 | ` *` |
|        - |  650 | ` * Both raise funnels consult this (the va_list one and the plain-message one): which` |
|        - |  651 | ` * of them a given allocation failure happens to reach is an accident of the site, and` |
|        - |  652 | ` * the user-visible answer must not be.` |
|        - |  653 | ` *` |
|        - |  654 | ` * Answers 0 and writes nothing when no ceiling was hit. Clears the flag, so a script` |
|        - |  655 | ` * that somehow survives its own allocation failure is not told twice -- and so the` |
|        - |  656 | ` * PH7_VmThrowError this returns into does not see it again and recurse.` |
|        - |  657 | ` */` |
|    36038 |  658 | `static int VmMemLimitMessage(ph7_vm *pVm,char *zBuf,sxu32 nBuf)` |
|        5 |  659 | `{` |
|    36043 |  660 | `	if( pVm->sAllocator.nMemTried == 0 ){` |
|    36043 |  661 | `		return 0;` |
|        - |  662 | `	}` |
|      ! 0 |  663 | `	SyBufferFormat(zBuf,nBuf,` |
|        - |  664 | `		"Allowed memory size of %u bytes exhausted (tried to allocate %u bytes)",` |
|      ! 0 |  665 | `		pVm->sAllocator.nMemLimitHit,pVm->sAllocator.nMemTried);` |
|      ! 0 |  666 | `	pVm->sAllocator.nMemTried = 0;` |
|      ! 0 |  667 | `	return 1;` |
|    18005 |  668 | `}` |
|    34444 |  669 | `static sxi32 VmThrowErrorAp(` |
|        - |  670 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  671 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  672 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  673 | `	const char *zFormat, /* Format message */` |
|        - |  674 | `	va_list ap           /* Variable list of arguments */` |
|        - |  675 | `	)` |
|        5 |  676 | `{` |
|        - |  677 | `	SyBlob sMsg;` |
|        - |  678 | `	SyString *pFile;` |
|        - |  679 | `	sxu32 nLine;` |
|    34449 |  680 | `	sxi32 rc = SXRET_OK;` |
|        - |  681 | `	char zMemMsg[128];` |
|    34449 |  682 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  683 | `		/* See PH7_VmThrowError: nothing a speculative evaluation raises is observable. */` |
|      ! 0 |  684 | `		pVm->nSpecDiag++;` |
|      ! 0 |  685 | `		return SXRET_OK;` |
|        - |  686 | `	}` |
|    34449 |  687 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|      ! 0 |  688 | `		return PH7_VmThrowError(&(*pVm),0,256,zMemMsg);` |
|        - |  689 | `	}` |
|        - |  690 | `	/* Peek the processed file if available */` |
|    34449 |  691 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    34449 |  692 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|        - |  693 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|    34449 |  694 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|    34449 |  695 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|    34449 |  696 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  697 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|        - |  698 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|        - |  699 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|        - |  700 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|    34449 |  701 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)nLine) ){` |
|        - |  702 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|        - |  703 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|    30529 |  704 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile,nLine);` |
|    30529 |  705 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|    30369 |  706 | `			SyBlobRelease(&sMsg);` |
|    30369 |  707 | `			return SXRET_OK;` |
|        - |  708 | `		}` |
|      244 |  709 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|       80 |  710 | `			SyBlobLength(&sMsg),pFile,nLine);` |
|       80 |  711 | `	}` |
|     4085 |  712 | `	SyBlobRelease(&sMsg);` |
|     4085 |  713 | `	return rc;` |
|    17214 |  714 | `}` |
|        - |  715 | `/*` |
|        - |  716 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - |  717 | ` * scope), or NULL when executing outside any class context.` |
|        - |  718 | ` */` |
|    14687 |  719 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 |  720 | `{` |
|    14692 |  721 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      221 |  722 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      221 |  723 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - |  724 | `	}` |
|    14476 |  725 | `	return 0;` |
|     7265 |  726 | `}` |
|        - |  727 | `/*` |
|        - |  728 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|        - |  729 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|        - |  730 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|        - |  731 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|        - |  732 | ` * as an undefined function and therefore as another engine throw). Each such` |
|        - |  733 | ` * throw would construct another exception and recurse until the native-nesting` |
|        - |  734 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|        - |  735 | ` * (an engine throw from inside a user exception's constructor) working and` |
|        - |  736 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|        - |  737 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|        - |  738 | ` * after the call.` |
|        - |  739 | ` */` |
|        - |  740 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|   351101 |  741 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|        5 |  742 | `{` |
|   351106 |  743 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|      ! 0 |  744 | `		return 0;` |
|        - |  745 | `	}` |
|   351106 |  746 | `	pVm->nExcCtorDepth++;` |
|   351106 |  747 | `	return 1;` |
|   175554 |  748 | `}` |
|        - |  749 | `/*` |
|        - |  750 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - |  751 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - |  752 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - |  753 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - |  754 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - |  755 | ` */` |
|   206339 |  756 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|        - |  757 | `	SyBlob *pMsg,sxi32 iCode)` |
|        5 |  758 | `{` |
|        - |  759 | `	ph7_class *pErrClass;` |
|        - |  760 | `	ph7_class_instance *pThis;` |
|        - |  761 | `	ph7_class_method *pCons;` |
|        - |  762 | `	VmFrame *pFrame;` |
|        - |  763 | `	sxi32 rc;` |
|   206344 |  764 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|   206344 |  765 | `	if( pErrClass == 0 ){` |
|      ! 0 |  766 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  767 | `		return PH7_ABORT;` |
|        - |  768 | `	}` |
|   206344 |  769 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|   206344 |  770 | `	if( pThis == 0 ){` |
|      ! 0 |  771 | `		SyBlobRelease(pMsg);` |
|      ! 0 |  772 | `		return PH7_ABORT;` |
|        - |  773 | `	}` |
|   206344 |  774 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|   206344 |  775 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|        - |  776 | `		ph7_value sArg,sCode;` |
|        - |  777 | `		ph7_value *apArg[2];` |
|        - |  778 | `		SyString sMsgStr;` |
|   206344 |  779 | `		int nArg = 1;` |
|   206344 |  780 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|   206344 |  781 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   206344 |  782 | `		apArg[0] = &sArg;` |
|   206344 |  783 | `		if( iCode != 0 ){` |
|        - |  784 | `			/* php's $code, second constructor argument -- the DOMException codes a` |
|        - |  785 | `			 * program compares against (DOM_NOT_FOUND_ERR & co) travel this way. */` |
|       45 |  786 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|       45 |  787 | `			apArg[1] = &sCode;` |
|       45 |  788 | `			nArg = 2;` |
|       22 |  789 | `		}` |
|   206344 |  790 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|   206344 |  791 | `		if( iCode != 0 ){` |
|       45 |  792 | `			PH7_MemObjRelease(&sCode);` |
|       22 |  793 | `		}` |
|   206344 |  794 | `		PH7_MemObjRelease(&sArg);` |
|   206344 |  795 | `		pVm->nExcCtorDepth--;` |
|   103168 |  796 | `	}` |
|   206344 |  797 | `	SyBlobRelease(pMsg);` |
|   206344 |  798 | `	pFrame = pVm->pFrame;` |
|   206344 |  799 | `	if( pFrame ){` |
|   206344 |  800 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   206344 |  801 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   103168 |  802 | `	}` |
|   206344 |  803 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   206344 |  804 | `	PH7_ClassInstanceUnref(pThis);` |
|   206344 |  805 | `	if( rc == SXERR_ABORT ){` |
|       38 |  806 | `		return PH7_ABORT;` |
|        - |  807 | `	}` |
|   206310 |  808 | `	return PH7_EXCEPTION;` |
|   103173 |  809 | `}` |
|        - |  810 | `/* The same with php's default $code of 0, which is what all but the DOM refusals` |
|        - |  811 | ` * carry. */` |
|   205804 |  812 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 |  813 | `{` |
|   205809 |  814 | `	return VmThrowBuiltinErrorCode(pVm,zClass,nClass,pMsg,0);` |
|        5 |  815 | `}` |
|        - |  816 | `/*` |
|        - |  817 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|        - |  818 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|        - |  819 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|        - |  820 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|        - |  821 | ` * result through its normal goto Exception / goto Abort.` |
|        - |  822 | ` */` |
|      320 |  823 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|        5 |  824 | `{` |
|      325 |  825 | `	return VmThrowFixedErrorCode(pVm,zClass,0,zMsg);` |
|        5 |  826 | `}` |
|        - |  827 | `/* The same, carrying php's $code: what a native class's property handler refused` |
|        - |  828 | ` * with (PH7_NativePropCtx::iThrowCode) is raised through here. */` |
|      535 |  829 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|        - |  830 | `	const char *zMsg)` |
|        5 |  831 | `{` |
|        - |  832 | `	SyBlob sMsg;` |
|      540 |  833 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      540 |  834 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      540 |  835 | `	return VmThrowBuiltinErrorCode(pVm, zClass, SyStrlen(zClass), &sMsg, iCode);` |
|        5 |  836 | `}` |
|        - |  837 | `/*` |
|        - |  838 | ` * Enum case singletons (PHP 8.1).` |
|        - |  839 | ` *` |
|        - |  840 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|        - |  841 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|        - |  842 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|        - |  843 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|        - |  844 | ` * backing-value type check and the duplicate-value check only fire when a` |
|        - |  845 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|        - |  846 | ` */` |
|        - |  847 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|        - |  848 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|        - |  849 | ` * readonly property" through the normal store path. */` |
|        - |  850 |  |
|        - |  851 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|        - |  852 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|      430 |  853 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|        3 |  854 | `{` |
|      433 |  855 | `	ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pCase->nIdx);` |
|        - |  856 | `	ph7_class_instance *pObj;` |
|        - |  857 | `	SyHashEntry *pEntry;` |
|      433 |  858 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       70 |  859 | `		return 0;` |
|        - |  860 | `	}` |
|      365 |  861 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      365 |  862 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      365 |  863 | `	if( pEntry == 0 ){` |
|      ! 0 |  864 | `		return 0;` |
|        - |  865 | `	}` |
|      365 |  866 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      218 |  867 | `}` |
|        - |  868 | `/*` |
|        - |  869 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|        - |  870 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|        - |  871 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|        - |  872 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|        - |  873 | ` * raises it. Returns the throw status to park/route.` |
|        - |  874 | ` */` |
|        2 |  875 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|        1 |  876 | `{` |
|        - |  877 | `	SyBlob sMsg;` |
|        3 |  878 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|        3 |  879 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|        3 |  880 | `	pVm->pConstCycleAttr = 0;` |
|        3 |  881 | `	pVm->pConstCycleClass = 0;` |
|        3 |  882 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 |  883 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|        1 |  884 | `		pOwner ? &pOwner->sName : &pAttr->sName,&pAttr->sName);` |
|        3 |  885 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 |  886 | `}` |
|        - |  887 | `/*` |
|        - |  888 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|        - |  889 | ` * materialize lazily and individually on first access — the backing-value` |
|        - |  890 | ` * type check fires per case, and the duplicate-value check compares only` |
|        - |  891 | ` * against cases that have already materialized (a broken sibling case does` |
|        - |  892 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|        - |  893 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|        - |  894 | ` * (duplicate value / self-reference) — which the caller routes` |
|        - |  895 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|        - |  896 | ` */` |
|      571 |  897 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|        5 |  898 | `{` |
|        - |  899 | `	ph7_class_attr **apCase;` |
|        - |  900 | `	ph7_class_instance *pObj;` |
|        - |  901 | `	ph7_value *pSlot;` |
|        - |  902 | `	ph7_value sBacking,sPropVal;` |
|        - |  903 | `	sxu32 i;` |
|      576 |  904 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      401 |  905 | `		return SXRET_OK;` |
|        - |  906 | `	}` |
|      176 |  907 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - |  908 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|        - |  909 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|      ! 0 |  910 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      ! 0 |  911 | `			pVm->pConstCycleAttr = pCase;` |
|      ! 0 |  912 | `			pVm->pConstCycleClass = pClass;` |
|      ! 0 |  913 | `		}` |
|      ! 0 |  914 | `		return SXRET_OK;` |
|        - |  915 | `	}` |
|      176 |  916 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      176 |  917 | `	if( pClass->nEnumBacking != 0 ){` |
|      109 |  918 | `		if( pCase->pNativeValue ){` |
|        - |  919 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|        - |  920 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|        - |  921 | `			 * is what that byte-code would have produced anyway. */` |
|        5 |  922 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|      107 |  923 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|        - |  924 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self::, and the`` |
|        - |  925 | `			 * frame marker keeps it reachable when the first access happens` |
|        - |  926 | `			 * inside another class's method (VmLocalExec pushes no frame, so` |
|        - |  927 | `			 * that method's frame is still the current one). */` |
|      105 |  928 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      105 |  929 | `			void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - |  930 | `			sxi32 rcExec;` |
|      105 |  931 | `			pVm->pConstEvalClass = pClass;` |
|      105 |  932 | `			pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      105 |  933 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      105 |  934 | `			pVm->nConstEvalDepth++;` |
|      105 |  935 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      105 |  936 | `			pVm->nConstEvalDepth--;` |
|      105 |  937 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      105 |  938 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      105 |  939 | `			pVm->pConstEvalFrame = pSaveFrame;` |
|      105 |  940 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - |  941 | `				/* The backing expression raised: abandon materialization and` |
|        - |  942 | `				 * hand the status to the caller to park/route. */` |
|        3 |  943 | `				PH7_MemObjRelease(&sBacking);` |
|        3 |  944 | `				return rcExec;` |
|        - |  945 | `			}` |
|      103 |  946 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      ! 0 |  947 | `				PH7_MemObjRelease(&sBacking);` |
|      ! 0 |  948 | `				return VmConstCycleThrow(&(*pVm));` |
|        - |  949 | `			}` |
|       49 |  950 | `		}` |
|      107 |  951 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|        - |  952 | `			/* php: TypeError, checked lazily at first case access */` |
|        - |  953 | `			SyBlob sMsg;` |
|        3 |  954 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|        3 |  955 | `			PH7_MemObjRelease(&sBacking);` |
|        3 |  956 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        2 |  957 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|        2 |  958 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|        3 |  959 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - |  960 | `		}` |
|      105 |  961 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  962 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|        - |  963 | `			 * the typed-constant leniency) to a genuine int. */` |
|       30 |  964 | `			PH7_MemObjToInteger(&sBacking);` |
|       16 |  965 | `		}else{` |
|       77 |  966 | `			PH7_MemObjToString(&sBacking);` |
|        - |  967 | `		}` |
|        - |  968 | `		/* php: two cases sharing one backing value are an Error — compared` |
|        - |  969 | `		 * against already-materialized cases only (php registers values as` |
|        - |  970 | `		 * each case evaluates). */` |
|      105 |  971 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      335 |  972 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|        - |  973 | `			ph7_value *pPrev;` |
|      237 |  974 | `			int bDup = 0;` |
|      237 |  975 | `			if( apCase[i] == pCase ){` |
|      103 |  976 | `				continue;` |
|        - |  977 | `			}` |
|      136 |  978 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      136 |  979 | `			if( pPrev ){` |
|       68 |  980 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       16 |  981 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|        9 |  982 | `				}else{` |
|       66 |  983 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|       52 |  984 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|       24 |  985 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|        - |  986 | `				}` |
|       33 |  987 | `			}` |
|      136 |  988 | `			if( bDup ){` |
|        - |  989 | `				/* php prints the two cases in DECLARATION order regardless of` |
|        - |  990 | `				 * which one is being evaluated. */` |
|        3 |  991 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|        - |  992 | `				SyBlob sMsg;` |
|        - |  993 | `				sxu32 j;` |
|        5 |  994 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|        5 |  995 | `					if( apCase[j] == pCase ){ break; }` |
|        2 |  996 | `				}` |
|        3 |  997 | `				if( j < i ){` |
|      ! 0 |  998 | `					pFirst = pCase;` |
|      ! 0 |  999 | `					pSecond = apCase[i];` |
|      ! 0 | 1000 | `				}` |
|        3 | 1001 | `				PH7_MemObjRelease(&sBacking);` |
|        3 | 1002 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1003 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|        1 | 1004 | `					&pClass->sName,&pFirst->sName,&pSecond->sName);` |
|        3 | 1005 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1006 | `			}` |
|       68 | 1007 | `		}` |
|       49 | 1008 | `	}` |
|        - | 1009 | `	/* Create the singleton and fill its readonly props */` |
|      170 | 1010 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      170 | 1011 | `	if( pObj == 0 ){` |
|      ! 0 | 1012 | `		PH7_MemObjRelease(&sBacking);` |
|      ! 0 | 1013 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1014 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|      ! 0 | 1015 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 | 1016 | `		return PH7_ABORT;` |
|        - | 1017 | `	}` |
|      170 | 1018 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      170 | 1019 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      170 | 1020 | `	PH7_MemObjRelease(&sPropVal);` |
|      170 | 1021 | `	if( pClass->nEnumBacking != 0 ){` |
|      103 | 1022 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|       49 | 1023 | `	}` |
|      170 | 1024 | `	PH7_MemObjRelease(&sBacking);` |
|        - | 1025 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|        - | 1026 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      170 | 1027 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      170 | 1028 | `	if( pSlot == 0 ){` |
|      ! 0 | 1029 | `		PH7_ClassInstanceUnref(pObj);` |
|      ! 0 | 1030 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1031 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|      ! 0 | 1032 | `			&pClass->sName,&pCase->sName);` |
|      ! 0 | 1033 | `		return PH7_ABORT;` |
|        - | 1034 | `	}` |
|      170 | 1035 | `	pSlot->x.pOther = pObj;` |
|      170 | 1036 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      170 | 1037 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      170 | 1038 | `	pCase->nIdx = pSlot->nIdx;` |
|      170 | 1039 | `	return SXRET_OK;` |
|      288 | 1040 | `}` |
|        - | 1041 | `/*` |
|        - | 1042 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|        - | 1043 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|        - | 1044 | ` * there, so a broken case surfaces its error at the same point).` |
|        - | 1045 | ` */` |
|      269 | 1046 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1047 | `{` |
|        - | 1048 | `	ph7_class_attr **apCase;` |
|        - | 1049 | `	sxu32 n;` |
|      274 | 1050 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1051 | `		return SXRET_OK;` |
|        - | 1052 | `	}` |
|      274 | 1053 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      823 | 1054 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      560 | 1055 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|      560 | 1056 | `		if( rc != SXRET_OK ){` |
|        8 | 1057 | `			return rc;` |
|        - | 1058 | `		}` |
|      277 | 1059 | `	}` |
|      268 | 1060 | `	return SXRET_OK;` |
|      139 | 1061 | `}` |
|        - | 1062 | `/*` |
|        - | 1063 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|        - | 1064 | ` * or 0 when the name does not name an enum.` |
|        - | 1065 | ` */` |
|      220 | 1066 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|        3 | 1067 | `{` |
|        - | 1068 | `	ph7_class *pClass;` |
|      223 | 1069 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|      ! 0 | 1070 | `		return 0;` |
|        - | 1071 | `	}` |
|      332 | 1072 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      109 | 1073 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      227 | 1074 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|        6 | 1075 | `		pClass = pClass->pNextName;` |
|        2 | 1076 | `	}` |
|      223 | 1077 | `	return pClass;` |
|      112 | 1078 | `}` |
|        - | 1079 | `/*` |
|        - | 1080 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|        - | 1081 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|        - | 1082 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|        - | 1083 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|        - | 1084 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|        - | 1085 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|        - | 1086 | ` */` |
|      544 | 1087 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|        5 | 1088 | `{` |
|      549 | 1089 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      544 | 1090 | `	if( pInner && pInner->pUserData` |
|      334 | 1091 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      ! 0 | 1092 | `		return 0;` |
|        - | 1093 | `	}` |
|      549 | 1094 | `	return pVm->nCurLine;` |
|      277 | 1095 | `}` |
|        - | 1096 | `/*` |
|        - | 1097 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|        - | 1098 | ` * not the pointer -- which used to be load-bearing, because a nested evaluation` |
|        - | 1099 | ` * (an initializer, or the constructor of the very TypeError being raised) grew` |
|        - | 1100 | ` * and REALLOC'd the pool and left a pointer taken before it dangling. P1's fixed` |
|        - | 1101 | ` * segments retired that; an index is still the right currency for a free, since` |
|        - | 1102 | ` * the free list is keyed by one. The slot's contents are released first:` |
|        - | 1103 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|        - | 1104 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|        - | 1105 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|        - | 1106 | ` */` |
|       36 | 1107 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 1108 | `{` |
|       38 | 1109 | `	ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       38 | 1110 | `	if( pObj == 0 ){` |
|      ! 0 | 1111 | `		return;` |
|        - | 1112 | `	}` |
|       38 | 1113 | `	PH7_MemObjRelease(pObj);` |
|       38 | 1114 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       20 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Evaluate a class constant's initializer on demand.` |
|        - | 1118 | ` *` |
|        - | 1119 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|        - | 1120 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|        - | 1121 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|        - | 1122 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|        - | 1123 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|        - | 1124 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|        - | 1125 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|        - | 1126 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|        - | 1127 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|        - | 1128 | ` * "Cannot declare self-referencing constant" Error.` |
|        - | 1129 | ` */` |
|     1216 | 1130 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1131 | `{` |
|        - | 1132 | `	ph7_value *pMemObj;` |
|     1216 | 1133 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     1216 | 1134 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     1221 | 1135 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|      ! 0 | 1136 | `		return SXRET_OK;` |
|        - | 1137 | `	}` |
|     1221 | 1138 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 1139 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|        - | 1140 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|        - | 1141 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|        3 | 1142 | `		if( pVm->pConstCycleAttr == 0 ){` |
|        3 | 1143 | `			pVm->pConstCycleAttr = pAttr;` |
|        3 | 1144 | `			pVm->pConstCycleClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        1 | 1145 | `		}` |
|        3 | 1146 | `		return SXRET_OK;` |
|        - | 1147 | `	}` |
|     1219 | 1148 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1219 | 1149 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1150 | `		return SXERR_MEM;` |
|        - | 1151 | `	}` |
|     1219 | 1152 | `	if( pAttr->pNativeValue ){` |
|        - | 1153 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|        - | 1154 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|        - | 1155 | `		 * when they went lazy (17th session), so this path — the only one an` |
|        - | 1156 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|        - | 1157 | `		 * which is why every native class constant read NULL: nothing had declared` |
|        - | 1158 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|        - | 1159 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|        - | 1160 | `		 * failure path to mirror below. */` |
|      735 | 1161 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      735 | 1162 | `		pAttr->nIdx = pMemObj->nIdx;` |
|      735 | 1163 | `		return SXRET_OK;` |
|        - | 1164 | `	}` |
|      489 | 1165 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      489 | 1166 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      489 | 1167 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      489 | 1168 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|        - | 1169 | `		sxu32 nSaveLazyLine;` |
|        - | 1170 | `		sxi32 nSaveLazyDepth;` |
|        - | 1171 | `		sxu32 nSlot;` |
|        - | 1172 | `		sxi32 rcExec;` |
|      489 | 1173 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      489 | 1174 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1175 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|        - | 1176 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|        - | 1177 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|      489 | 1178 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 1179 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|        - | 1180 | `		 * throw out of its own bytecode carries. */` |
|      489 | 1181 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      489 | 1182 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      489 | 1183 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      489 | 1184 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      489 | 1185 | `		pVm->nConstEvalDepth++;` |
|      489 | 1186 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      489 | 1187 | `		pVm->nConstEvalDepth--;` |
|      489 | 1188 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      489 | 1189 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      489 | 1190 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      489 | 1191 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      489 | 1192 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      489 | 1193 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|      484 | 1194 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|      460 | 1195 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|        - | 1196 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|        - | 1197 | `			 * class constant's expression at each access until one of them` |
|        - | 1198 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|        - | 1199 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|        - | 1200 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|        - | 1201 | `			 * the rest of the run — and made a static default that named it` |
|        - | 1202 | `			 * (whose own evaluation is deferred to first access) find it` |
|        - | 1203 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|        - | 1204 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|        - | 1205 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|        - | 1206 | `			 * it is found — an inner level only records it — but the value is` |
|        - | 1207 | `			 * unusable and the next access must be able to detect it again.` |
|        - | 1208 | `			 * No loop: each access runs the initializer once and raises. */` |
|       34 | 1209 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|       34 | 1210 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1211 | `				/* Hand the status to the caller to park/route. */` |
|       31 | 1212 | `				return rcExec;` |
|        - | 1213 | `			}` |
|        3 | 1214 | `			if( pVm->nConstEvalDepth == 0 ){` |
|        - | 1215 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|        - | 1216 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|        3 | 1217 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1218 | `			}` |
|      ! 0 | 1219 | `			return SXRET_OK;` |
|        - | 1220 | `		}` |
|      457 | 1221 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1222 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|        - | 1223 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|        - | 1224 | `			 * access raises again — php re-runs the whole materialization each` |
|        - | 1225 | `			 * time. A pass may widen int -> float in place, which is the value` |
|        - | 1226 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|        - | 1227 | `			 * runs php code that used to grow (and realloc) aMemObj — so the slot is` |
|        - | 1228 | `			 * addressed by index from here on, never through pMemObj. Redundant` |
|        - | 1229 | `			 * since P1 (fixed segments); left for the harvest sweep (PERF.md P1). */` |
|        5 | 1230 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|        5 | 1231 | `			if( rcType != SXRET_OK ){` |
|        5 | 1232 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|        5 | 1233 | `				return rcType;` |
|        - | 1234 | `			}` |
|      ! 0 | 1235 | `		}` |
|        - | 1236 | `		/* Memoize the value. */` |
|      453 | 1237 | `		pAttr->nIdx = nSlot;` |
|      453 | 1238 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|      453 | 1239 | `		return SXRET_OK;` |
|        - | 1240 | `	}` |
|      ! 0 | 1241 | `	pAttr->nIdx = pMemObj->nIdx;` |
|      ! 0 | 1242 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      ! 0 | 1243 | `	return SXRET_OK;` |
|      599 | 1244 | `}` |
|        - | 1245 | `/*` |
|        - | 1246 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|        - | 1247 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|        - | 1248 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|        - | 1249 | ` */` |
|      802 | 1250 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        4 | 1251 | `{` |
|      806 | 1252 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      360 | 1253 | `		return SXRET_OK;` |
|        - | 1254 | `	}` |
|      447 | 1255 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       15 | 1256 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|        - | 1257 | `	}` |
|      433 | 1258 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      391 | 1259 | `}` |
|        - | 1260 | `/*` |
|        - | 1261 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|        - | 1262 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|        - | 1263 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|        - | 1264 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|        - | 1265 | ` */` |
|        8 | 1266 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|        2 | 1267 | `{` |
|        - | 1268 | `	SyBlob sMsg;` |
|       10 | 1269 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 | 1270 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       10 | 1271 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1272 | `}` |
|        - | 1273 | `/*` |
|        - | 1274 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|        - | 1275 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|        - | 1276 | ` * raised at the store site here with the same message and the same` |
|        - | 1277 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|        - | 1278 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|        - | 1279 | ` */` |
|        2 | 1280 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|        1 | 1281 | `{` |
|        3 | 1282 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|        3 | 1283 | `	pVm->iExitStatus = 255;` |
|        3 | 1284 | `	pVm->bHaltRequested = 1;` |
|        3 | 1285 | `	return PH7_ABORT;` |
|        1 | 1286 | `}` |
|        - | 1287 | `/*` |
|        - | 1288 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - | 1289 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - | 1290 | ` * possible.` |
|        - | 1291 | ` */` |
|   100188 | 1292 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven,` |
|        - | 1293 | `	int bViaRef)` |
|        5 | 1294 | `{` |
|   100193 | 1295 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|   100193 | 1296 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 1297 | `	char zType[192];` |
|   150287 | 1298 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|    50094 | 1299 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1300 | `	/* php words a write that arrived through a REFERENCE differently: the slot` |
|        - | 1301 | `	 * is the property's, but the assignment names no property, so the sentence` |
|        - | 1302 | `	 * says which property is HOLDING the reference. */` |
|   100193 | 1303 | `	const char *zWhat = bViaRef ? "reference held by property" : "property";` |
|        - | 1304 | `	SyBlob sMsg;` |
|   100193 | 1305 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1306 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - | 1307 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|   100193 | 1308 | `	if( pOwner ){` |
|   100193 | 1309 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s %z::$%z of type %s",` |
|    50094 | 1310 | `			zGiven,zWhat,&pOwner->sName,&pAttr->sName,zTypeText);` |
|    50099 | 1311 | `	}else{` |
|      ! 0 | 1312 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s $%z of type %s",` |
|      ! 0 | 1313 | `			zGiven,zWhat,&pAttr->sName,zTypeText);` |
|        - | 1314 | `	}` |
|   100193 | 1315 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 | 1316 | `}` |
|        - | 1317 | `/*` |
|        - | 1318 | ` * Of the pseudo-types a property may be declared with, the two whose mask holds` |
|        - | 1319 | `` * an array. `object` does not, and neither does any real class or interface --`` |
|        - | 1320 | `` * php looks at the type MASK, so `ArrayAccess` and `Traversable` are refused`` |
|        - | 1321 | `` * exactly like `int`.`` |
|        - | 1322 | ` */` |
|        2 | 1323 | `static int VmPseudoTypeAcceptsArray(const SyString *pClass)` |
|        1 | 1324 | `{` |
|        3 | 1325 | `	return (pClass->nByte == 8 && SyStrnicmp(pClass->zString,"iterable",8) == 0)` |
|        3 | 1326 | `	    \|\| (pClass->nByte == 5 && SyStrnicmp(pClass->zString,"mixed",5) == 0);` |
|        1 | 1327 | `}` |
|        - | 1328 | `/*` |
|        - | 1329 | ` * TRUE when a property's declared type admits an ARRAY, which is what decides` |
|        - | 1330 | ` * whether a dimension write to it may AUTO-INITIALIZE one. php asks the type's` |
|        - | 1331 | `` * mask (`MAY_BE_ARRAY`), so `array`, `?array`, `iterable`, `mixed` and any union`` |
|        - | 1332 | ``  * with an array alternative say yes and every class type says no -- `ArrayAccess` `` |
|        - | 1333 | `` * and `Traversable` included, which is the part a "does it behave like an array"`` |
|        - | 1334 | ` * reading would get wrong.` |
|        - | 1335 | ` */` |
|       12 | 1336 | `static int VmAttrTypeAcceptsArray(ph7_class_attr *pAttr)` |
|        2 | 1337 | `{` |
|       14 | 1338 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1339 | `		return 1; /* untyped: a dimension write vivifies as it always has */` |
|        - | 1340 | `	}` |
|       14 | 1341 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      ! 0 | 1342 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pAttr->aUnionAlts);` |
|        - | 1343 | `		sxu32 i;` |
|      ! 0 | 1344 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; ++i ){` |
|      ! 0 | 1345 | `			if( aAlt[i].nType == MEMOBJ_HASHMAP ){` |
|      ! 0 | 1346 | `				return 1;` |
|        - | 1347 | `			}` |
|      ! 0 | 1348 | `			if( aAlt[i].nType == SXU32_HIGH && VmPseudoTypeAcceptsArray(&aAlt[i].sClass) ){` |
|      ! 0 | 1349 | `				return 1;` |
|        - | 1350 | `			}` |
|      ! 0 | 1351 | `		}` |
|      ! 0 | 1352 | `		return 0;` |
|        - | 1353 | `	}` |
|       14 | 1354 | `	if( pAttr->nType == MEMOBJ_HASHMAP ){` |
|        9 | 1355 | `		return 1;` |
|        - | 1356 | `	}` |
|        6 | 1357 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 1358 | `		return VmPseudoTypeAcceptsArray(&pAttr->sClass);` |
|        - | 1359 | `	}` |
|        3 | 1360 | `	return 0;` |
|        8 | 1361 | `}` |
|        - | 1362 | `/*` |
|        - | 1363 | ` * Throw php's TypeError for a dimension write that would have to auto-initialize` |
|        - | 1364 | ` * an array inside a property whose declared type has no room for one.` |
|        - | 1365 | ` */` |
|        2 | 1366 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        1 | 1367 | `{` |
|        3 | 1368 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        3 | 1369 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 1370 | `	char zType[256];` |
|        4 | 1371 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        1 | 1372 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1373 | `	SyBlob sMsg;` |
|        3 | 1374 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1375 | `	if( pOwner ){` |
|        3 | 1376 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property %z::$%z of type %s",` |
|        1 | 1377 | `			&pOwner->sName,&pAttr->sName,zTypeText);` |
|        2 | 1378 | `	}else{` |
|      ! 0 | 1379 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property $%z of type %s",` |
|      ! 0 | 1380 | `			&pAttr->sName,zTypeText);` |
|        - | 1381 | `	}` |
|        3 | 1382 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        1 | 1383 | `}` |
|        - | 1384 | `/*` |
|        - | 1385 | ` * Decide what a DIMENSION write to an uninitialized typed property does, which` |
|        - | 1386 | `` * is php's `zend_handle_fetch_obj_flags`: auto-initialize an empty array when the`` |
|        - | 1387 | ` * declared type admits one, and refuse otherwise. Returns SXRET_OK with the slot` |
|        - | 1388 | ` * left holding a fresh empty array, or the thrown TypeError.` |
|        - | 1389 | ` */` |
|       12 | 1390 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        2 | 1391 | `{` |
|       14 | 1392 | `	if( pVmAttr->pAttr == 0 \|\| pSlot == 0 ){` |
|      ! 0 | 1393 | `		return SXRET_OK;` |
|        - | 1394 | `	}` |
|       14 | 1395 | `	if( !VmAttrTypeAcceptsArray(pVmAttr->pAttr) ){` |
|        3 | 1396 | `		return VmThrowAutoInitArrayError(pVm,pVmAttr);` |
|        - | 1397 | `	}` |
|       11 | 1398 | `	PH7_MemObjRelease(pSlot);` |
|       11 | 1399 | `	PH7_MemObjToHashmap(pSlot);` |
|       11 | 1400 | `	pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       11 | 1401 | `	return SXRET_OK;` |
|        8 | 1402 | `}` |
|        - | 1403 | `/*` |
|        - | 1404 | ` * TRUE when a property's declared type admits NULL, which is what decides whether a` |
|        - | 1405 | `` * REFERENCE fetch of an uninitialized one may bind at all. `?T` and a `T\|null` union`` |
|        - | 1406 | `` * both carry the nullable flag; `mixed` is the one type that admits null without it.`` |
|        - | 1407 | ` */` |
|       30 | 1408 | `static int VmAttrTypeAcceptsNull(ph7_class_attr *pAttr)` |
|        1 | 1409 | `{` |
|       31 | 1410 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1411 | `		return 1; /* untyped: the slot already holds NULL */` |
|        - | 1412 | `	}` |
|       31 | 1413 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        7 | 1414 | `		return 1;` |
|        - | 1415 | `	}` |
|       24 | 1416 | `	if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 1417 | `	 && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 1418 | `		return 1;` |
|        - | 1419 | `	}` |
|       23 | 1420 | `	return 0;` |
|       16 | 1421 | `}` |
|        - | 1422 | `/*` |
|        - | 1423 | ` * Decide what a REFERENCE fetch of an uninitialized typed property does, which is` |
|        - | 1424 | `` * php's `zend_handle_fetch_obj_flags` under BP_VAR_W: the slot becomes NULL and the`` |
|        - | 1425 | ` * bind reaches it when the declared type admits null, and the fetch is refused` |
|        - | 1426 | `` * otherwise -- php's `Cannot access uninitialized non-nullable property C::$p by`` |
|        - | 1427 | `` * reference`. It is NOT the auto-initialize-array rule: that one belongs to a`` |
|        - | 1428 | `` * DIMENSION write, and running it here made `?int $t` an array and `int $t` the`` |
|        - | 1429 | ` * wrong TypeError.` |
|        - | 1430 | ` */` |
|       30 | 1431 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        1 | 1432 | `{` |
|        - | 1433 | `	ph7_class_attr *pAttr;` |
|        - | 1434 | `	ph7_class *pOwner;` |
|        - | 1435 | `	SyBlob sMsg;` |
|       31 | 1436 | `	if( pVmAttr == 0 \|\| pVmAttr->pAttr == 0 ){` |
|      ! 0 | 1437 | `		return SXRET_OK;` |
|        - | 1438 | `	}` |
|       31 | 1439 | `	pAttr = pVmAttr->pAttr;` |
|       31 | 1440 | `	if( VmAttrTypeAcceptsNull(pAttr) ){` |
|        9 | 1441 | `		if( pSlot ){` |
|        9 | 1442 | `			PH7_MemObjRelease(pSlot);` |
|        9 | 1443 | `			MemObjSetType(pSlot,MEMOBJ_NULL);` |
|        4 | 1444 | `		}` |
|        9 | 1445 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 1446 | `		return SXRET_OK;` |
|        - | 1447 | `	}` |
|       23 | 1448 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       23 | 1449 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 1450 | `	SyBlobFormat(&sMsg,"Cannot access uninitialized non-nullable property %z::$%z by reference",` |
|       11 | 1451 | `		pOwner ? &pOwner->sName : &pClass->sName,&pAttr->sName);` |
|       23 | 1452 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       16 | 1453 | `}` |
|        - | 1454 | `/*` |
|        - | 1455 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - | 1456 | ` */` |
|   100028 | 1457 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1458 | `{` |
|   100033 | 1459 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|   100033 | 1460 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - | 1461 | `	SyBlob sMsg;` |
|   100033 | 1462 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   100033 | 1463 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|    50014 | 1464 | `		zKind,&pOwner->sName,&pAttr->sName);` |
|   100033 | 1465 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1466 | `}` |
|        - | 1467 | `/*` |
|        - | 1468 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|        - | 1469 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|        - | 1470 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|        - | 1471 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|        - | 1472 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|        - | 1473 | ` */` |
|        - | 1474 | `/*` |
|        - | 1475 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|        - | 1476 | ` * property from a scope its set-visibility excludes:` |
|        - | 1477 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|        - | 1478 | ` */` |
|       18 | 1479 | `static sxi32 VmThrowSetVisibilityErrorEx(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,` |
|        - | 1480 | `	int bIndirect)` |
|        2 | 1481 | `{` |
|       20 | 1482 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       20 | 1483 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       20 | 1484 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|        - | 1485 | `	/* php words a write that only reaches the property THROUGH something it holds` |
|        - | 1486 | ``	 * -- `$o->arr['k'] = v`, a by-reference bind -- as an INDIRECT modification,`` |
|        - | 1487 | `	 * the same distinction its readonly sentence makes. */` |
|       20 | 1488 | `	const char *zVerb = bIndirect ? "indirectly modify" : "modify";` |
|        - | 1489 | `	SyBlob sMsg;` |
|       20 | 1490 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       20 | 1491 | `	if( pActive ){` |
|        3 | 1492 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from scope %z",` |
|        1 | 1493 | `			zVerb,zVis,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|        2 | 1494 | `	}else{` |
|       18 | 1495 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from global scope",` |
|        8 | 1496 | `			zVerb,zVis,&pOwner->sName,&pAttr->sName);` |
|        - | 1497 | `	}` |
|       20 | 1498 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1499 | `}` |
|       16 | 1500 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1501 | `{` |
|       17 | 1502 | `	return VmThrowSetVisibilityErrorEx(pVm,pClass,pAttr,0);` |
|        1 | 1503 | `}` |
|        - | 1504 | `/*` |
|        - | 1505 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|        - | 1506 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|        - | 1507 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|        - | 1508 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|        - | 1509 | ` */` |
|       38 | 1510 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|        1 | 1511 | `{` |
|       39 | 1512 | `	ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pOwner);` |
|       39 | 1513 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 1514 | `	int bOk;` |
|       39 | 1515 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|       33 | 1516 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|       17 | 1517 | `	}else{` |
|        7 | 1518 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        - | 1519 | `	}` |
|       39 | 1520 | `	if( !bOk ){` |
|       17 | 1521 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|        - | 1522 | `	}` |
|       23 | 1523 | `	return SXRET_OK;` |
|       20 | 1524 | `}` |
|        - | 1525 | `/*` |
|        - | 1526 | ` * php's write refusal for a native property whose handler takes NO write` |
|        - | 1527 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE). The sentence is the readonly one -- php's` |
|        - | 1528 | ` * date_period_write_property says exactly that -- but the property carries no` |
|        - | 1529 | ` * readonly FLAG, so this is spelled apart from VmThrowReadonlyError rather than` |
|        - | 1530 | ` * reached through it: Reflection reports isReadOnly() false for DatePeriod's` |
|        - | 1531 | ` * seven in both engines, and the readonly rules (write-once, set-scope, the` |
|        - | 1532 | ` * __clone re-initialization window) do not apply to a handler that never` |
|        - | 1533 | ` * accepts one.` |
|        - | 1534 | ` */` |
|       40 | 1535 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1536 | `{` |
|       41 | 1537 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1538 | `	SyBlob sMsg;` |
|       41 | 1539 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       41 | 1540 | `	SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       41 | 1541 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1542 | `}` |
|        - | 1543 | `/*` |
|        - | 1544 | `` * And its unset half, which php words differently: `Cannot unset C::$p`, with`` |
|        - | 1545 | ` * neither "readonly" nor "property" in it.` |
|        - | 1546 | ` */` |
|       24 | 1547 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1548 | `{` |
|        - | 1549 | `	SyBlob sMsg;` |
|       25 | 1550 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1551 | `	/* The OBJECT's class, not the declaring one -- php's two refusals disagree` |
|        - | 1552 | `	 * about which to print, and a subclass of DatePeriod shows it: the write says` |
|        - | 1553 | ``	 * `DatePeriod::$interval` and the unset says `SubDp::$interval`. */`` |
|       25 | 1554 | `	SyBlobFormat(&sMsg,"Cannot unset %z::$%z",&pClass->sName,&pAttr->sName);` |
|       25 | 1555 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1556 | `}` |
|        - | 1557 | `/*` |
|        - | 1558 | ` * And php's THIRD refusal for a property its handler will not have written:` |
|        - | 1559 | `` * `Property p is read only`, which names neither the class nor the `$`.`` |
|        - | 1560 | `` * PDOStatement's `queryString` is php's case, and the shapes it applies to are`` |
|        - | 1561 | ` * the plain store and the unset alone -- see PH7_CLASS_ATTR_NATIVE_RDONLY.` |
|        - | 1562 | ` */` |
|       10 | 1563 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr)` |
|        1 | 1564 | `{` |
|        - | 1565 | `	SyBlob sMsg;` |
|       11 | 1566 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 | 1567 | `	SyBlobFormat(&sMsg,"Property %z is read only",&pAttr->sName);` |
|       11 | 1568 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1569 | `}` |
|        - | 1570 | `/*` |
|        - | 1571 | `` * php's answer to `unset($o->p)` where p is READONLY. Two of the three cases`` |
|        - | 1572 | ` * refuse, and the sentences are not the write ones:` |
|        - | 1573 | ` *` |
|        - | 1574 | ` *   * an INITIALIZED one refuses from every scope, its own included --` |
|        - | 1575 | `` *     `Cannot unset readonly property C::$p`. Destroying it would re-arm the`` |
|        - | 1576 | ` *     write-once latch, which is exactly what readonly exists to prevent;` |
|        - | 1577 | ` *` |
|        - | 1578 | ` *   * an UNINITIALIZED one is a WRITE-shaped act, so it takes the set-visibility` |
|        - | 1579 | ` *     rules: allowed from the declaring class or a subclass (php lets a lazy` |
|        - | 1580 | ` *     proxy re-arm one that way), and otherwise the asymmetric-visibility` |
|        - | 1581 | `` *     refusal. php words that one two ways -- an EXPLICIT `private(set)` gets the`` |
|        - | 1582 | ` *     ordinary asymmetric sentence with no "readonly" in it, and everything else` |
|        - | 1583 | `` *     gets readonly's own implicit `protected(set) readonly`.`` |
|        - | 1584 | ` *` |
|        - | 1585 | ` * Answers SXRET_OK when the unset may proceed, else the throw status.` |
|        - | 1586 | ` */` |
|       24 | 1587 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr)` |
|        1 | 1588 | `{` |
|       25 | 1589 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - | 1590 | `	ph7_class *pOwner;` |
|        - | 1591 | `	ph7_class *pActive;` |
|        - | 1592 | `	SyBlob sMsg;` |
|        - | 1593 | `	int bInit,bScope;` |
|       25 | 1594 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) == 0 ){` |
|      ! 0 | 1595 | `		return SXRET_OK;` |
|        - | 1596 | `	}` |
|       25 | 1597 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       25 | 1598 | `	pActive = VmCurrentSelf(pVm);` |
|       25 | 1599 | `	bInit = (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0;` |
|       25 | 1600 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        7 | 1601 | `		bScope = (pActive != 0 && pActive == pOwner);` |
|        4 | 1602 | `	}else{` |
|       19 | 1603 | `		bScope = (pActive != 0 && pOwner != 0 && PH7_VmInstanceOf(pActive,pOwner));` |
|        - | 1604 | `	}` |
|       25 | 1605 | `	if( !bInit && bScope ){` |
|        9 | 1606 | `		return SXRET_OK;` |
|        - | 1607 | `	}` |
|       17 | 1608 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 | 1609 | `	if( bInit ){` |
|        9 | 1610 | `		SyBlobFormat(&sMsg,"Cannot unset readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|        5 | 1611 | `	}else{` |
|       13 | 1612 | `		const char *zWhat = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        4 | 1613 | `			? "private(set)" : "protected(set) readonly";` |
|        9 | 1614 | `		if( pActive ){` |
|        3 | 1615 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from scope %z",` |
|        1 | 1616 | `				zWhat,&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|        2 | 1617 | `		}else{` |
|        7 | 1618 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from global scope",` |
|        3 | 1619 | `				zWhat,&pOwner->sName,&pAttr->sName);` |
|        - | 1620 | `		}` |
|        - | 1621 | `	}` |
|       17 | 1622 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       13 | 1623 | `}` |
|       52 | 1624 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        5 | 1625 | `{` |
|       57 | 1626 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1627 | `	SyBlob sMsg;` |
|       57 | 1628 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       57 | 1629 | `	if( bModify ){` |
|       53 | 1630 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sName,&pAttr->sName);` |
|       29 | 1631 | `	}else{` |
|        6 | 1632 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        6 | 1633 | `		if( pActive ){` |
|      ! 0 | 1634 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 | 1635 | `				&pOwner->sName,&pAttr->sName,&pActive->sName);` |
|      ! 0 | 1636 | `		}else{` |
|        6 | 1637 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        2 | 1638 | `				&pOwner->sName,&pAttr->sName);` |
|        - | 1639 | `		}` |
|        - | 1640 | `	}` |
|       57 | 1641 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1642 | `}` |
|        - | 1643 | `/*` |
|        - | 1644 | ` * INDIRECT modification: this SLOT is about to be reached as something other than` |
|        - | 1645 | `` * a plain store -- aliased by `=&`, handed to a by-reference parameter, walked by`` |
|        - | 1646 | ` * a by-reference foreach, or used as the BASE of a subscript write. php screens` |
|        - | 1647 | ` * every one of them where it screens a store, because the alias outlives the` |
|        - | 1648 | ` * statement and the next write through it would reach the property with no` |
|        - | 1649 | ` * handler and no readonly latch in the way.` |
|        - | 1650 | ` *` |
|        - | 1651 | `` * Two sentences. A php-readonly property gets its own: `Cannot indirectly modify`` |
|        - | 1652 | `` * readonly property C::$p`, raised whatever the scope and whether or not the`` |
|        - | 1653 | ` * property has been initialized -- the reference is refused before the` |
|        - | 1654 | ` * uninitialized read is. A NATIVE class whose handler refuses every write` |
|        - | 1655 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE) gets that handler's own sentence, the one a` |
|        - | 1656 | ` * plain store to it gets.` |
|        - | 1657 | ` *` |
|        - | 1658 | ` * Answers SXRET_OK to proceed, or the throw status. Asked by the sites that reach` |
|        - | 1659 | ` * a property through its memobj index rather than through its declaration.` |
|        - | 1660 | ` */` |
|    44357 | 1661 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1662 | `{` |
|        - | 1663 | `	SyHashEntry *pSlot;` |
|        - | 1664 | `	VmClassAttr *pVmAttr;` |
|        - | 1665 | `	ph7_class_attr *pAttr;` |
|    44362 | 1666 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|    44238 | 1667 | `		return SXRET_OK;` |
|        - | 1668 | `	}` |
|    39731 | 1669 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    39731 | 1670 | `	if( pSlot == 0 ){` |
|      ! 0 | 1671 | `		return SXRET_OK;` |
|        - | 1672 | `	}` |
|      127 | 1673 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      127 | 1674 | `	pAttr = pVmAttr->pAttr;` |
|      127 | 1675 | `	if( pAttr == 0 ){` |
|      ! 0 | 1676 | `		return SXRET_OK;` |
|        - | 1677 | `	}` |
|      127 | 1678 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       13 | 1679 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pAttr);` |
|        - | 1680 | `	}` |
|      115 | 1681 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       33 | 1682 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 1683 | `		SyBlob sMsg;` |
|       33 | 1684 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       33 | 1685 | `		SyBlobFormat(&sMsg,"Cannot indirectly modify readonly property %z::$%z",` |
|       16 | 1686 | `			&pOwner->sName,&pAttr->sName);` |
|       33 | 1687 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1688 | `	}` |
|        - | 1689 | `	/* An asymmetric set-visibility (PHP 8.4) gates the indirect write exactly as` |
|        - | 1690 | ``	 * readonly does, and from the same scopes: `$o->arr['k'] = v` on a`` |
|        - | 1691 | ``	 * `public private(set) array $arr` is refused outside the declaring class.`` |
|        - | 1692 | `	 * Only readonly was screened here, so that write landed in SILENCE. */` |
|       83 | 1693 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        3 | 1694 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|        3 | 1695 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        5 | 1696 | `		int bOk = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        2 | 1697 | `			? (pActive != 0 && pActive == pDecl)` |
|        2 | 1698 | `			: (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        3 | 1699 | `		if( !bOk ){` |
|        3 | 1700 | `			return VmThrowSetVisibilityErrorEx(pVm,pVmAttr->pOwner,pAttr,1);` |
|        - | 1701 | `		}` |
|      ! 0 | 1702 | `	}` |
|       81 | 1703 | `	return SXRET_OK;` |
|    22176 | 1704 | `}` |
|        - | 1705 | `/*` |
|        - | 1706 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - | 1707 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - | 1708 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - | 1709 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - | 1710 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - | 1711 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - | 1712 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - | 1713 | ` */` |
|   801302 | 1714 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1715 | `{` |
|        - | 1716 | `	SyHashEntry *pSlot;` |
|        - | 1717 | `	VmClassAttr *pVmAttr;` |
|   801307 | 1718 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   798977 | 1719 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - | 1720 | `	}` |
|   300712 | 1721 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   300712 | 1722 | `	if( pSlot == 0 ){` |
|      ! 0 | 1723 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 1724 | `	}` |
|     2334 | 1725 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2334 | 1726 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|        5 | 1727 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|        - | 1728 | `	}` |
|     2330 | 1729 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 | 1730 | `		return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pVmAttr->pAttr,1);` |
|        - | 1731 | `	}` |
|     2316 | 1732 | `	if( pVmAttr->pAttr` |
|     2319 | 1733 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|        - | 1734 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|        9 | 1735 | `		return VmCheckSetVisibility(pVm,pVmAttr->pOwner,pVmAttr->pAttr);` |
|        - | 1736 | `	}` |
|     2311 | 1737 | `	return SXRET_OK;` |
|   401409 | 1738 | `}` |
|        - | 1739 | `/*` |
|        - | 1740 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - | 1741 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - | 1742 | ` * For class types, instanceof is verified.` |
|        - | 1743 | ` *` |
|        - | 1744 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - | 1745 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - | 1746 | ` */` |
|        - | 1747 |  |
|        - | 1748 | `/*` |
|        - | 1749 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - | 1750 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - | 1751 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - | 1752 | ` *   0 if it's not strictly numeric.` |
|        - | 1753 | ` */` |
|       38 | 1754 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        3 | 1755 | `{` |
|        - | 1756 | `	const char *z, *zEnd, *zTail;` |
|        - | 1757 | `	sxu32 n;` |
|       41 | 1758 | `	sxu8 bReal = 0;` |
|        - | 1759 | `	sxi32 rc;` |
|       41 | 1760 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       24 | 1761 | `		return 0;` |
|        - | 1762 | `	}` |
|       18 | 1763 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 | 1764 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 | 1765 | `	zEnd = z + n;` |
|       18 | 1766 | `	if( n == 0 ) return 0;` |
|       18 | 1767 | `	zTail = 0;` |
|       18 | 1768 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 | 1769 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 | 1770 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 | 1771 | `	if( zTail != zEnd ) return 0;` |
|       15 | 1772 | `	return bReal ? 2 : 1;` |
|       22 | 1773 | `}` |
|        - | 1774 |  |
|        - | 1775 | `/*` |
|        - | 1776 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - | 1777 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|        - | 1778 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|        - | 1779 | ` * return, parameter, property, union alternative — would have to string-match` |
|        - | 1780 | ` * the name itself.` |
|        - | 1781 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - | 1782 | ` * to extend when another literal/pseudo type is added.` |
|        - | 1783 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - | 1784 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - | 1785 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - | 1786 | ` */` |
|     9371 | 1787 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        5 | 1788 | `{` |
|     9376 | 1789 | `	const char *z = pClass->zString;` |
|     9376 | 1790 | `	sxu32 n = pClass->nByte;` |
|     9376 | 1791 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      349 | 1792 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - | 1793 | `	}` |
|     9032 | 1794 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       28 | 1795 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - | 1796 | `	}` |
|     9006 | 1797 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|     2824 | 1798 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - | 1799 | `	}` |
|     6187 | 1800 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|        - | 1801 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|        - | 1802 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|        - | 1803 | `		 * object; scope-sensitive, so a private method is callable only from` |
|        - | 1804 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|        - | 1805 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|        - | 1806 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|        - | 1807 | `		 * php does.) */` |
|     4579 | 1808 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|        - | 1809 | `	}` |
|     1613 | 1810 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - | 1811 | `		/* iterable === array \| Traversable */` |
|       85 | 1812 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1813 | `			return 1;` |
|        - | 1814 | `		}` |
|       73 | 1815 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       45 | 1816 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       45 | 1817 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       29 | 1818 | `				return 1;` |
|        - | 1819 | `			}` |
|        7 | 1820 | `		}` |
|       45 | 1821 | `		return 0;` |
|        - | 1822 | `	}` |
|     1531 | 1823 | `	return -1;` |
|     4584 | 1824 | `}` |
|        - | 1825 | `/*` |
|        - | 1826 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|        - | 1827 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|        - | 1828 | ` * through. php binds the keyword where the hint is written, so` |
|        - | 1829 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|        - | 1830 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|        - | 1831 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|        - | 1832 | ` * throw a TypeError over perfectly valid code.` |
|        - | 1833 | ` *` |
|        - | 1834 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|        - | 1835 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|        - | 1836 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|        - | 1837 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|        - | 1838 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|        - | 1839 | `` * VmResolveTypeClass applies to `parent`.`` |
|        - | 1840 | ` */` |
|   121289 | 1841 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|        5 | 1842 | `{` |
|   121294 | 1843 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   106771 | 1844 | `		return pDecl;` |
|        - | 1845 | `	}` |
|    14528 | 1846 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|    60566 | 1847 | `}` |
|        - | 1848 | `/*` |
|        - | 1849 | ` * The scope a member's declared type is PRINTED against, which is not the scope` |
|        - | 1850 | ` * it is CHECKED against.` |
|        - | 1851 | ` *` |
|        - | 1852 | `` * php resolves `self`/`parent` in a member's stored type TEXT at compile time,`` |
|        - | 1853 | ` * and a TRAIT has no class to resolve them to -- so what it stores for a trait` |
|        - | 1854 | `` * member keeps the keyword, and every display of that text says `self` however`` |
|        - | 1855 | `` * many classes composed the trait: `uninitialized(self)` in var_dump,`` |
|        - | 1856 | `` * `of type ?parent` in the assign TypeError, `self` from`` |
|        - | 1857 | ` * ReflectionProperty::getType(). The CHECK still resolves against the composing` |
|        - | 1858 | ` * class, which is what VmHintScopeClass answers; this is its display twin, and` |
|        - | 1859 | ` * telling the two apart is what PLAN §7.1's R7 was waiting for.` |
|        - | 1860 | ` */` |
|   101032 | 1861 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl)` |
|        5 | 1862 | `{` |
|   101037 | 1863 | `	return ( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ) ? pDecl : 0;` |
|        5 | 1864 | `}` |
|        - | 1865 | `/*` |
|        - | 1866 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - | 1867 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - | 1868 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - | 1869 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - | 1870 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - | 1871 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - | 1872 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - | 1873 | ` * throw.` |
|        - | 1874 | ` *` |
|        - | 1875 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|        - | 1876 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|        - | 1877 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|        - | 1878 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|        - | 1879 | ` */` |
|        - | 1880 | `/*` |
|        - | 1881 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|        - | 1882 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|        - | 1883 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|        - | 1884 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|        - | 1885 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|        - | 1886 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|        - | 1887 | ` * iLoadable flag — lives in one place.` |
|        - | 1888 | ` *` |
|        - | 1889 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|        - | 1890 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|        - | 1891 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|        - | 1892 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|        - | 1893 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|        - | 1894 | ` *` |
|        - | 1895 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|        - | 1896 | ` * compatibility target, where the type may legitimately be an interface or` |
|        - | 1897 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|        - | 1898 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|        - | 1899 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|        - | 1900 | ` * with TRUE; it does not go through this helper.)` |
|        - | 1901 | ` */` |
|     1636 | 1902 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|        5 | 1903 | `{` |
|     1641 | 1904 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      157 | 1905 | `		return pSelf;` |
|        - | 1906 | `	}` |
|     1489 | 1907 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|        - | 1908 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|        - | 1909 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|        - | 1910 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|        - | 1911 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|        - | 1912 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|       45 | 1913 | `		return PH7_VmPeekTopClass(pVm);` |
|        - | 1914 | `	}` |
|     1449 | 1915 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|        - | 1916 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|        - | 1917 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|       37 | 1918 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      ! 0 | 1919 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|      ! 0 | 1920 | `		}` |
|       37 | 1921 | `		return pSelf ? pSelf->pBase : 0;` |
|        - | 1922 | `	}` |
|     1417 | 1923 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|      823 | 1924 | `}` |
|        - | 1925 | `/*` |
|        - | 1926 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|        - | 1927 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|        - | 1928 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|        - | 1929 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|        - | 1930 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|        - | 1931 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|        - | 1932 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|        - | 1933 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|        - | 1934 | ` */` |
|   101952 | 1935 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|        5 | 1936 | `{` |
|   101982 | 1937 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|   101928 | 1938 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|   152928 | 1939 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|        5 | 1940 | `}` |
|        - | 1941 | `/*` |
|        - | 1942 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|        - | 1943 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|        - | 1944 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|        - | 1945 | ` * typed-default checks — each of which then formats its own message. The` |
|        - | 1946 | ` * resolved class is handed back through *ppResolved for the message builder` |
|        - | 1947 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|        - | 1948 | ` * nothing resolved).` |
|        - | 1949 | ` *` |
|        - | 1950 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|        - | 1951 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|        - | 1952 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|        - | 1953 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|        - | 1954 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|        - | 1955 | ` * resolve against there is no class to compare to — a position php rejects at` |
|        - | 1956 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|        - | 1957 | ` *` |
|        - | 1958 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|        - | 1959 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|        - | 1960 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|        - | 1961 | ` * autoloader can produce fails.` |
|        - | 1962 | ` */` |
|     1400 | 1963 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|        - | 1964 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|        5 | 1965 | `{` |
|     1405 | 1966 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     1405 | 1967 | `	*ppResolved = pExpected;` |
|     1405 | 1968 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       53 | 1969 | `		return 0;` |
|        - | 1970 | `	}` |
|     1357 | 1971 | `	if( pExpected == 0 ){` |
|       13 | 1972 | `		return VmHintIsScopeKeyword(pName);` |
|        - | 1973 | `	}` |
|     1345 | 1974 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|      705 | 1975 | `}` |
|        - | 1976 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|        - | 1977 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|   412202 | 1978 | `static int VmHintNameChar(int c)` |
|        5 | 1979 | `{` |
|   823377 | 1980 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|   411190 | 1981 | `		\|\| c == ' ' \|\| c == '\t');` |
|        5 | 1982 | `}` |
|        - | 1983 | `/*` |
|        - | 1984 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|        - | 1985 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|        - | 1986 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|        - | 1987 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|        - | 1988 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|        - | 1989 | ` * resolved until the call site are substituted.` |
|        - | 1990 | ` *` |
|        - | 1991 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|        - | 1992 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|        - | 1993 | ` * are only ever available as source: unions/intersections, and the property /` |
|        - | 1994 | ` * class-constant messages, which print the declared type whatever its shape.` |
|        - | 1995 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|        - | 1996 | ` *` |
|        - | 1997 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|        - | 1998 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|        - | 1999 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|        - | 2000 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|        - | 2001 | ` * names the two types it stands for. Only the standalone spellings can still` |
|        - | 2002 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|        - | 2003 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|        - | 2004 | ` */` |
|   100398 | 2005 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2006 | `	char *zBuf,sxu32 nBuf)` |
|        5 | 2007 | `{` |
|   150602 | 2008 | `	return VmHintTextResolvedEx(&(*pVm),pDeclared,pScope,` |
|    50199 | 2009 | `		PH7_HINT_TEXT_ITERABLE\|PH7_HINT_TEXT_STATIC,zBuf,nBuf);` |
|        5 | 2010 | `}` |
|        - | 2011 | `/*` |
|        - | 2012 | ` * The two halves of the rewrite above, asked for separately.` |
|        - | 2013 | ` *` |
|        - | 2014 | `` * PH7_HINT_TEXT_ITERABLE expands a standalone `iterable`; every DIAGNOSTIC wants`` |
|        - | 2015 | `` * that and Reflection wants none of it (php prints `iterable` there).`` |
|        - | 2016 | `` * PH7_HINT_TEXT_STATIC resolves `static` beside `self`/`parent`; a diagnostic`` |
|        - | 2017 | ` * names the class it stands for, while Reflection and the declaration renderer` |
|        - | 2018 | ` * both print the keyword, php having no class to name until the call.` |
|        - | 2019 | ` */` |
|   101788 | 2020 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2021 | `	int iFlags,char *zBuf,sxu32 nBuf)` |
|        5 | 2022 | `{` |
|        - | 2023 | `	const char *z;` |
|   101793 | 2024 | `	sxu32 n, i = 0, nAt = 0;` |
|   101793 | 2025 | `	if( nBuf == 0 ){` |
|      ! 0 | 2026 | `		return "";` |
|        - | 2027 | `	}` |
|   101793 | 2028 | `	z = pDeclared ? pDeclared->zString : 0;` |
|   101793 | 2029 | `	n = z ? pDeclared->nByte : 0;` |
|   101793 | 2030 | `	if( z && (iFlags & PH7_HINT_TEXT_ITERABLE) ){` |
|   100403 | 2031 | `		const char *zIter = 0;` |
|   100403 | 2032 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        9 | 2033 | `			zIter = "Traversable\|array";` |
|   100400 | 2034 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        3 | 2035 | `			zIter = "Traversable\|array\|null";` |
|        1 | 2036 | `		}` |
|   100403 | 2037 | `		if( zIter ){` |
|       11 | 2038 | `			sxu32 nIter = SyStrlen(zIter);` |
|       11 | 2039 | `			if( nIter > nBuf - 1 ){` |
|      ! 0 | 2040 | `				nIter = nBuf - 1;` |
|      ! 0 | 2041 | `			}` |
|       11 | 2042 | `			SyMemcpy(zIter,zBuf,nIter);` |
|       11 | 2043 | `			zBuf[nIter] = 0;` |
|       11 | 2044 | `			return zBuf;` |
|        - | 2045 | `		}` |
|    50195 | 2046 | `	}` |
|   204361 | 2047 | `	while( i < n && nAt + 1 < nBuf ){` |
|        - | 2048 | `		sxu32 nStart, nCopy;` |
|        - | 2049 | `		SyString sTok;` |
|        - | 2050 | `		const SyString *pOut;` |
|   102581 | 2051 | `		if( !VmHintNameChar(z[i]) ){` |
|      641 | 2052 | `			zBuf[nAt++] = z[i++];` |
|      641 | 2053 | `			continue;` |
|        - | 2054 | `		}` |
|   101945 | 2055 | `		nStart = i;` |
|   411175 | 2056 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|   309235 | 2057 | `			i++;` |
|        5 | 2058 | `		}` |
|   101945 | 2059 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|   101945 | 2060 | `		pOut = &sTok;` |
|   101940 | 2061 | `		if( VmHintIsScopeKeyword(&sTok)` |
|    51017 | 2062 | `		 && ( (iFlags & PH7_HINT_TEXT_STATIC)` |
|       52 | 2063 | `		   \|\| sTok.nByte != sizeof("static")-1` |
|       24 | 2064 | `		   \|\| SyStrnicmp(sTok.zString,"static",sizeof("static")-1) != 0 ) ){` |
|       70 | 2065 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|       70 | 2066 | `			if( pRes ){` |
|       56 | 2067 | `				pOut = &pRes->sName;` |
|       26 | 2068 | `			}` |
|       33 | 2069 | `		}` |
|   101945 | 2070 | `		nCopy = pOut->nByte;` |
|   101945 | 2071 | `		if( nCopy > nBuf - nAt - 1 ){` |
|      ! 0 | 2072 | `			nCopy = nBuf - nAt - 1;` |
|      ! 0 | 2073 | `		}` |
|   101945 | 2074 | `		if( nCopy > 0 ){` |
|   101945 | 2075 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|   101945 | 2076 | `			nAt += nCopy;` |
|    50970 | 2077 | `		}` |
|        5 | 2078 | `	}` |
|   101785 | 2079 | `	zBuf[nAt] = 0;` |
|   101785 | 2080 | `	return zBuf;` |
|    50899 | 2081 | `}` |
|        - | 2082 | `/*` |
|        - | 2083 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|        - | 2084 | ` * float-identity leniency — see the typed-constant note above` |
|        - | 2085 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|        - | 2086 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|        - | 2087 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|        - | 2088 | ` * so an int-typed parameter / return / property / union member silently` |
|        - | 2089 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|        - | 2090 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|        - | 2091 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|        - | 2092 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|        - | 2093 | ` * present).` |
|        - | 2094 | ` *` |
|        - | 2095 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|        - | 2096 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|        - | 2097 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|        - | 2098 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|        - | 2099 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|        - | 2100 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|        - | 2101 | ` */` |
|    67232 | 2102 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|        5 | 2103 | `{` |
|    67232 | 2104 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|    30927 | 2105 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - | 2106 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|        - | 2107 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|        - | 2108 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|        - | 2109 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|       55 | 2110 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|       55 | 2111 | `		SyBlobRelease(&pVal->sBlob);` |
|       55 | 2112 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|       25 | 2113 | `	}` |
|    67237 | 2114 | `}` |
|     3117 | 2115 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|        - | 2116 | `	ph7_class *pSelf)` |
|        5 | 2117 | `{` |
|        - | 2118 | `	sxu32 i;` |
|        - | 2119 | `	sxu32 nAlts;` |
|        - | 2120 | `	ph7_type_alt *aAlts;` |
|        - | 2121 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - | 2122 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     3122 | 2123 | `	int bHasIntersection = 0;` |
|        - | 2124 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     3122 | 2125 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       19 | 2126 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - | 2127 | `	}` |
|     3106 | 2128 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     3106 | 2129 | `	nAlts = SySetUsed(pAlts);` |
|        - | 2130 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|        - | 2131 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|        - | 2132 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|        - | 2133 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|        - | 2134 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   102338 | 2135 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|     9346 | 2136 | `	for( i = 0; i < nAlts; i++ ){` |
|     6245 | 2137 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|       45 | 2138 | `			bHasIntersection = 1;` |
|       21 | 2139 | `		}` |
|     3124 | 2140 | `	}` |
|        - | 2141 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|        - | 2142 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|        - | 2143 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|        - | 2144 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     3106 | 2145 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|       35 | 2146 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 2147 | `		sxu32 g;` |
|      421 | 2148 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|        - | 2149 | `			int bAll;` |
|      409 | 2150 | `			if( aGroupCount[g] < 2 ) continue;` |
|       35 | 2151 | `			bAll = 1;` |
|       87 | 2152 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2153 | `				ph7_class *pExpected;` |
|       67 | 2154 | `				if( aAlts[i].nGroup != g ) continue;` |
|       63 | 2155 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|       63 | 2156 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|       63 | 2157 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       15 | 2158 | `					bAll = 0;` |
|       15 | 2159 | `					break;` |
|        - | 2160 | `				}` |
|       27 | 2161 | `			}` |
|       35 | 2162 | `			if( bAll ) return SXRET_OK;` |
|        9 | 2163 | `		}` |
|        6 | 2164 | `	}` |
|        - | 2165 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - | 2166 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - | 2167 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - | 2168 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     9250 | 2169 | `	for( i = 0; i < nAlts; i++ ){` |
|     6199 | 2170 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     6150 | 2171 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     4574 | 2172 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|       34 | 2173 | `			return SXRET_OK;` |
|        - | 2174 | `		}` |
|     3064 | 2175 | `	}` |
|     3056 | 2176 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     3056 | 2177 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     9192 | 2178 | `	for( i = 0; i < nAlts; i++ ){` |
|     6141 | 2179 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     6097 | 2180 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     3150 | 2181 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     3146 | 2182 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     1265 | 2183 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     1041 | 2184 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     1001 | 2185 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|        5 | 2186 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     3050 | 2187 | `	}` |
|        - | 2188 | `	/* Object handling */` |
|     3056 | 2189 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      105 | 2190 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      105 | 2191 | `		if( bHasClassAlt ){` |
|       91 | 2192 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      225 | 2193 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2194 | `				ph7_class *pExpected;` |
|      169 | 2195 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      161 | 2196 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      115 | 2197 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      115 | 2198 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       34 | 2199 | `					return SXRET_OK;` |
|        - | 2200 | `				}` |
|       44 | 2201 | `			}` |
|       28 | 2202 | `		}` |
|       73 | 2203 | `		return SXERR_INVALID;` |
|        - | 2204 | `	}` |
|        - | 2205 | `	/* Array handling */` |
|     2956 | 2206 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     1892 | 2207 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - | 2208 | `	}` |
|        - | 2209 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|        - | 2210 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|        - | 2211 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|        - | 2212 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|        - | 2213 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|        - | 2214 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     1069 | 2215 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|       22 | 2216 | `		if( bHasFloat ) return SXRET_OK;` |
|        5 | 2217 | `	}` |
|     1061 | 2218 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      129 | 2219 | `		if( bHasInt ){` |
|      105 | 2220 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      105 | 2221 | `			return SXRET_OK;` |
|        - | 2222 | `		}` |
|       12 | 2223 | `	}` |
|      961 | 2224 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|      933 | 2225 | `		if( bHasString ) return SXRET_OK;` |
|        8 | 2226 | `	}` |
|       47 | 2227 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|        3 | 2228 | `		if( bHasBool ) return SXRET_OK;` |
|        1 | 2229 | `	}` |
|       47 | 2230 | `	if( bStrict ){` |
|        - | 2231 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|        8 | 2232 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 | 2233 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 | 2234 | `			return SXRET_OK;` |
|        - | 2235 | `		}` |
|        8 | 2236 | `		return SXERR_INVALID;` |
|        - | 2237 | `	}` |
|        - | 2238 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - | 2239 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - | 2240 | `	 * to match PHP's union RFC. */` |
|        - | 2241 | `	{` |
|       41 | 2242 | `		int kind = VmStringNumericKind(pValue);` |
|       41 | 2243 | `		if( bHasInt ){` |
|        - | 2244 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - | 2245 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 | 2246 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 2247 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2248 | `				return SXRET_OK;` |
|        - | 2249 | `			}` |
|       18 | 2250 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2251 | `				ph7_real r = pValue->rVal;` |
|        - | 2252 | ``				/* Range first: `(sxi64)r` is undefined outside it (§2), and NaN`` |
|        - | 2253 | `				 * and the infinities are not exact ints either way. */` |
|        - | 2254 | `				/* (double)r: see VmValueIsLossyToInt -- a no-op where ph7_real` |
|        - | 2255 | `				 * is double, and the narrowing MSVC turns into an error under` |
|        - | 2256 | `				 * PH7_OMIT_FLOATING_POINT otherwise. */` |
|      ! 0 | 2257 | `				if( PH7_RealFitsInt64((double)r) && r == (ph7_real)(sxi64)r ){` |
|      ! 0 | 2258 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2259 | `					return SXRET_OK;` |
|        - | 2260 | `				}` |
|      ! 0 | 2261 | `			}` |
|       18 | 2262 | `			if( kind == 1 ){` |
|        9 | 2263 | `				PH7_MemObjToInteger(pValue);` |
|        9 | 2264 | `				return SXRET_OK;` |
|        - | 2265 | `			}` |
|        4 | 2266 | `		}` |
|       33 | 2267 | `		if( bHasFloat ){` |
|       10 | 2268 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 | 2269 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 | 2270 | `				return SXRET_OK;` |
|        - | 2271 | `			}` |
|       10 | 2272 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 | 2273 | `				PH7_MemObjToReal(pValue);` |
|        7 | 2274 | `				return SXRET_OK;` |
|        - | 2275 | `			}` |
|        1 | 2276 | `		}` |
|       26 | 2277 | `		if( bHasString ){` |
|      ! 0 | 2278 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 | 2279 | `				PH7_MemObjToString(pValue);` |
|      ! 0 | 2280 | `				return SXRET_OK;` |
|        - | 2281 | `			}` |
|      ! 0 | 2282 | `		}` |
|       26 | 2283 | `		if( bHasBool ){` |
|        3 | 2284 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|        3 | 2285 | `				PH7_MemObjToBool(pValue);` |
|        3 | 2286 | `				return SXRET_OK;` |
|        - | 2287 | `			}` |
|      ! 0 | 2288 | `		}` |
|        - | 2289 | `	}` |
|       24 | 2290 | `	return SXERR_INVALID;` |
|     1563 | 2291 | `}` |
|        - | 2292 |  |
|        - | 2293 | `/*` |
|        - | 2294 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - | 2295 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - | 2296 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - | 2297 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - | 2298 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - | 2299 | ` */` |
|      460 | 2300 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        5 | 2301 | `{` |
|        - | 2302 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - | 2303 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - | 2304 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - | 2305 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - | 2306 | `	 * silently swallow any argument. */` |
|      465 | 2307 | `	if( nType == MEMOBJ_NULL ){` |
|        3 | 2308 | `		return SXERR_INVALID;` |
|        - | 2309 | `	}` |
|        - | 2310 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|        - | 2311 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|        - | 2312 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|        - | 2313 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|        - | 2314 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|        - | 2315 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|        - | 2316 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|        - | 2317 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|        - | 2318 | `	 * object value against an array type is caught here too (never valid);` |
|        - | 2319 | `	 * object->scalar stays a separate case handled by the callers. */` |
|      463 | 2320 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|       59 | 2321 | `		return SXERR_INVALID;` |
|        - | 2322 | `	}` |
|      407 | 2323 | `	if( bStrict ){` |
|        - | 2324 | `		/* Only int -> float widening is allowed implicitly. */` |
|       59 | 2325 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 | 2326 | `			PH7_MemObjToReal(pVal);` |
|        3 | 2327 | `			return SXRET_OK;` |
|        - | 2328 | `		}` |
|       57 | 2329 | `		return SXERR_INVALID;` |
|        - | 2330 | `	}` |
|        - | 2331 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|        - | 2332 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|        - | 2333 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|        - | 2334 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|        - | 2335 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|        - | 2336 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|        - | 2337 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|      353 | 2338 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       20 | 2339 | `		return SXERR_INVALID;` |
|        - | 2340 | `	}` |
|        - | 2341 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|        - | 2342 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|        - | 2343 | `	 * is invoked by the string cast below). Every other scalar target —` |
|        - | 2344 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|        - | 2345 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|        - | 2346 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|        - | 2347 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|        - | 2348 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|        - | 2349 | `	 * is caught by the array guard.) */` |
|      335 | 2350 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       22 | 2351 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       27 | 2352 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       10 | 2353 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       13 | 2354 | `			return SXERR_INVALID;` |
|        - | 2355 | `		}` |
|        4 | 2356 | `	}` |
|      318 | 2357 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      245 | 2358 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      230 | 2359 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|       64 | 2360 | `		return SXERR_INVALID;` |
|        - | 2361 | `	}` |
|      263 | 2362 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|        - | 2363 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|        - | 2364 | `		 * PHL rejects it (§10). SXERR_INVALID routes to the caller's TypeError,` |
|        - | 2365 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|        - | 2366 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|        - | 2367 | `		 * for the typed parameters and returns that reach here, for the typed` |
|        - | 2368 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|        - | 2369 | `		 * integer-only operators. */` |
|      ! 0 | 2370 | `		return SXERR_INVALID;` |
|        - | 2371 | `	}` |
|      258 | 2372 | `	if( nType == MEMOBJ_STRING && (pVal->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       56 | 2373 | `	 && pVal->pVm && PH7_IS_NAN(pVal->rVal) ){` |
|        - | 2374 | ``		/* A userland `string` parameter, return or property taking a NaN: php's`` |
|        - | 2375 | `		 * weak coercion warns there exactly as its ZPP does for an internal one` |
|        - | 2376 | ``		 * (`unexpected NAN value was coerced to string`). The cast below is the`` |
|        - | 2377 | `		 * silent conversion -- it is shared with the engine's own -- so the` |
|        - | 2378 | `		 * diagnostic is raised here, where the DECLARED type is known. */` |
|        3 | 2379 | `		VmErrorFormat(pVal->pVm,PH7_CTX_WARNING,` |
|        - | 2380 | `			"unexpected NAN value was coerced to string");` |
|        1 | 2381 | `	}` |
|        - | 2382 | `	{` |
|      263 | 2383 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      263 | 2384 | `		if( xCast ) xCast(pVal);` |
|        - | 2385 | `	}` |
|      263 | 2386 | `	return SXRET_OK;` |
|      235 | 2387 | `}` |
|        - | 2388 |  |
|        - | 2389 | `/*` |
|        - | 2390 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - | 2391 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - | 2392 | ` *` |
|        - | 2393 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - | 2394 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - | 2395 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - | 2396 | ` * back to a static literal and ignore zBuf entirely.` |
|        - | 2397 | ` */` |
|      260 | 2398 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        5 | 2399 | `{` |
|      265 | 2400 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|      265 | 2401 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|      265 | 2402 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      265 | 2403 | `		if( pDeclared->zString && nCopy > 0 ){` |
|      265 | 2404 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      130 | 2405 | `		}` |
|      265 | 2406 | `		zBuf[nCopy] = 0;` |
|      265 | 2407 | `		return zBuf;` |
|        - | 2408 | `	}` |
|      ! 0 | 2409 | `	switch( nType ){` |
|      ! 0 | 2410 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 | 2411 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 | 2412 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 | 2413 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 | 2414 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 | 2415 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 | 2416 | `		default:             return "scalar";` |
|        - | 2417 | `	}` |
|      135 | 2418 | `}` |
|        - | 2419 |  |
|        - | 2420 | `/*` |
|        - | 2421 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|        - | 2422 | ` * the way php writes it in a TypeError:` |
|        - | 2423 | ` *` |
|        - | 2424 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|        - | 2425 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|        - | 2426 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|        - | 2427 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|        - | 2428 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|        - | 2429 | ` *` |
|        - | 2430 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|        - | 2431 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|        - | 2432 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|        - | 2433 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|        - | 2434 | ` */` |
|      182 | 2435 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|        - | 2436 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|        5 | 2437 | `{` |
|      187 | 2438 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|        - | 2439 | `	sxu32 nCopy;` |
|      187 | 2440 | `	sxu32 nAt = 0;` |
|      187 | 2441 | `	if( nBuf == 0 ){` |
|      ! 0 | 2442 | `		return "";` |
|        - | 2443 | `	}` |
|      182 | 2444 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|       79 | 2445 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|       21 | 2446 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|       21 | 2447 | `		nCopy = SyStrlen(zIter);` |
|       21 | 2448 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       21 | 2449 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|       21 | 2450 | `		zBuf[nCopy] = 0;` |
|       21 | 2451 | `		return zBuf;` |
|        - | 2452 | `	}` |
|      169 | 2453 | `	if( bNullable && nBuf > 1 ){` |
|       23 | 2454 | `		zBuf[nAt++] = '?';` |
|       10 | 2455 | `	}` |
|      169 | 2456 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|      169 | 2457 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|      169 | 2458 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|      169 | 2459 | `	zBuf[nAt + nCopy] = 0;` |
|      169 | 2460 | `	return zBuf;` |
|       96 | 2461 | `}` |
|        - | 2462 |  |
|        - | 2463 | `/*` |
|        - | 2464 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - | 2465 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - | 2466 | ` */` |
|      144 | 2467 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        5 | 2468 | `{` |
|      149 | 2469 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      221 | 2470 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      144 | 2471 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      149 | 2472 | `	return zBuf;` |
|        5 | 2473 | `}` |
|        - | 2474 |  |
|        - | 2475 | `/*` |
|        - | 2476 | ` * php's write_property handler (ph7_class::xSet): a native class whose properties` |
|        - | 2477 | ` * are its own C struct converts the incoming value the way that struct demands —` |
|        - | 2478 | ` * and may refuse the write outright. The value is rewritten IN PLACE, so what the` |
|        - | 2479 | ` * caller goes on to store is what the hook left behind.` |
|        - | 2480 | ` */` |
|      618 | 2481 | `static sxi32 VmRunNativeSet(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pValue)` |
|        4 | 2482 | `{` |
|        - | 2483 | `	PH7_NativeSetCtx sSet;` |
|      622 | 2484 | `	if( pVmAttr->pInst == 0 ){` |
|      ! 0 | 2485 | `		return SXRET_OK;   /* a class static: no object for a handler to run on */` |
|        - | 2486 | `	}` |
|      622 | 2487 | `	sSet.pName = &pVmAttr->pAttr->sName;` |
|      622 | 2488 | `	sSet.pValue = pValue;` |
|      622 | 2489 | `	sSet.zThrowClass = 0;` |
|      622 | 2490 | `	sSet.zThrowMsg[0] = 0;` |
|      622 | 2491 | `	if( PH7_ClassNativeSet(pVmAttr->pInst,&sSet) && sSet.zThrowClass ){` |
|        8 | 2492 | `		return VmThrowFixedError(pVm,sSet.zThrowClass,sSet.zThrowMsg);` |
|        - | 2493 | `	}` |
|      615 | 2494 | `	return SXRET_OK;` |
|      313 | 2495 | `}` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * The same handler, asked of a SLOT that an opcode has already mutated in place.` |
|        - | 2498 | `` * `$i->f++` and `$i->f--` never pass a value through the store filter — they`` |
|        - | 2499 | ` * increment the slot where it lies — so the conversion has to be applied after` |
|        - | 2500 | ` * the fact, which is exactly what php does (it reads, increments, and writes` |
|        - | 2501 | `` * back through the handler: `$i->f = 1.456008; ++$i->f` leaves the property at`` |
|        - | 2502 | ` * 2.456007, the microsecond truncation of the sum). Answers SXRET_OK when the` |
|        - | 2503 | ` * slot is not a native one.` |
|        - | 2504 | ` */` |
|  7350710 | 2505 | `static void VmFilterBitSet(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 2506 | `{` |
|  7350715 | 2507 | `	if( pVm->bFilterBitsOff ){` |
|      ! 0 | 2508 | `		return;` |
|        - | 2509 | `	}` |
|  7350715 | 2510 | `	if( nIdx >= pVm->nFilterBits ){` |
|     1401 | 2511 | `		sxu32 nNew = pVm->nFilterBits ? pVm->nFilterBits : 1024;` |
|        - | 2512 | `		unsigned char *pNew;` |
|        - | 2513 | `		/* An index this large cannot be a real slot, and doubling toward it would` |
|        - | 2514 | `		 * wrap. Treat it exactly like a failed allocation. */` |
|     1401 | 2515 | `		if( nIdx >= (1u << 30) ){` |
|      ! 0 | 2516 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2517 | `			return;` |
|        - | 2518 | `		}` |
|     1447 | 2519 | `		while( nNew <= nIdx ){` |
|       49 | 2520 | `			nNew <<= 1;` |
|        3 | 2521 | `		}` |
|     1401 | 2522 | `		pNew = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,nNew >> 3);` |
|     1401 | 2523 | `		if( pNew == 0 ){` |
|        - | 2524 | `			/* This slot IS filtered and the bitmap cannot say so. It must not answer` |
|        - | 2525 | `			 * for anything else either, or the screens in front of it would skip a` |
|        - | 2526 | `			 * type check, a readonly refusal or a native write handler. */` |
|      ! 0 | 2527 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2528 | `			return;` |
|        - | 2529 | `		}` |
|     1401 | 2530 | `		SyZero(pNew,nNew >> 3);` |
|     1401 | 2531 | `		if( pVm->pFilterBits ){` |
|       25 | 2532 | `			SyMemcpy(pVm->pFilterBits,pNew,pVm->nFilterBits >> 3);` |
|       25 | 2533 | `			SyMemBackendFree(&pVm->sAllocator,pVm->pFilterBits);` |
|       11 | 2534 | `		}` |
|     1401 | 2535 | `		pVm->pFilterBits = pNew;` |
|     1401 | 2536 | `		pVm->nFilterBits = nNew;` |
|      695 | 2537 | `	}` |
|  7350715 | 2538 | `	pVm->pFilterBits[nIdx >> 3] \|= (unsigned char)(1 << (nIdx & 7));` |
|  3675207 | 2539 | `}` |
|        - | 2540 | `/*` |
|        - | 2541 | ` * Register a property slot with the store filter, and drop it again. These two` |
|        - | 2542 | ` * are the ONLY writers of pVm->hTypedSlot: the predicate that decides membership` |
|        - | 2543 | ` * lives here once (a declared type, a native write handler, or both), and the` |
|        - | 2544 | ` * handler COUNT that lets the mutation opcodes skip the table entirely is kept` |
|        - | 2545 | ` * beside it -- registering in one place and forgetting to drop in another is` |
|        - | 2546 | ` * exactly how a recycled memobj index would inherit a stale entry.` |
|        - | 2547 | ` *` |
|        - | 2548 | ` * They also keep the SLOT BITMAP the hot-path screens read (see pFilterBits): it` |
|        - | 2549 | ` * answers the membership question without hashing, and because it is written here` |
|        - | 2550 | ` * and nowhere else it cannot drift from the table it screens.` |
|        - | 2551 | ` */` |
| 10437351 | 2552 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        5 | 2553 | `{` |
| 10437356 | 2554 | `	if( !PH7_ATTR_STORE_FILTERED(pVmAttr->pAttr) ){` |
|  3086646 | 2555 | `		return SXRET_OK;` |
|        - | 2556 | `	}` |
|  7350715 | 2557 | `	if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 2558 | `		return SXERR_MEM;` |
|        - | 2559 | `	}` |
|  7350715 | 2560 | `	VmFilterBitSet(&(*pVm),pVmAttr->nIdx);` |
|  7350715 | 2561 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|     8299 | 2562 | `		pVm->nNativeSetSlot++;` |
|     4147 | 2563 | `	}` |
|  7350715 | 2564 | `	return SXRET_OK;` |
|  5218073 | 2565 | `}` |
|  9730435 | 2566 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx)` |
|        5 | 2567 | `{` |
|  9730440 | 2568 | `	if( pAttr == 0 \|\| !PH7_ATTR_STORE_FILTERED(pAttr) ){` |
|  2880401 | 2569 | `		return;` |
|        - | 2570 | `	}` |
|  6850044 | 2571 | `	if( SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32),0) == SXRET_OK ){` |
|  6850044 | 2572 | `		if( nIdx < pVm->nFilterBits ){` |
|  6850044 | 2573 | `			pVm->pFilterBits[nIdx >> 3] &= (unsigned char)~(1 << (nIdx & 7));` |
|  3424872 | 2574 | `		}` |
|  6850044 | 2575 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) && pVm->nNativeSetSlot > 0 ){` |
|     8285 | 2576 | `			pVm->nNativeSetSlot--;` |
|     4140 | 2577 | `		}` |
|  3424872 | 2578 | `	}` |
|  4864625 | 2579 | `}` |
|   801188 | 2580 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 | 2581 | `{` |
|        - | 2582 | `	SyHashEntry *pSlot;` |
|        - | 2583 | `	VmClassAttr *pVmAttr;` |
|   801193 | 2584 | `	if( nIdx == SXU32_HIGH \|\| pVm->nNativeSetSlot == 0 \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   799175 | 2585 | `		return SXRET_OK;` |
|        - | 2586 | `	}` |
|   154689 | 2587 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   154689 | 2588 | `	if( pSlot == 0 ){` |
|      ! 0 | 2589 | `		return SXRET_OK;` |
|        - | 2590 | `	}` |
|     2019 | 2591 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2019 | 2592 | `	if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) == 0 ){` |
|     2013 | 2593 | `		return SXRET_OK;` |
|        - | 2594 | `	}` |
|        7 | 2595 | `	return VmRunNativeSet(pVm,pVmAttr,pValue);` |
|   401352 | 2596 | `}` |
|  1066740 | 2597 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags)` |
|        5 | 2598 | `{` |
|  1066745 | 2599 | `	int bViaRef = (iStoreFlags & VM_TYPED_STORE_VIA_REF) != 0;` |
|  1066745 | 2600 | `	int bCloneInit = (iStoreFlags & VM_TYPED_STORE_CLONE_INIT) != 0;` |
|        - | 2601 | `	SyHashEntry *pSlot;` |
|        - | 2602 | `	VmClassAttr *pVmAttr;` |
|        - | 2603 | `	ph7_class_attr *pAttr;` |
|        - | 2604 | `	ph7_class *pHintScope;` |
|        - | 2605 | `	char zGivenBuf[128];` |
|        - | 2606 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|        - | 2607 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|        - | 2608 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|        - | 2609 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|  1066745 | 2610 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|  1066745 | 2611 | `	if( !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   965079 | 2612 | `		return SXRET_OK; /* Not a filtered slot -- the common answer, and free */` |
|        - | 2613 | `	}` |
|  1014869 | 2614 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  1014869 | 2615 | `	if( pSlot == 0 ){` |
|      ! 0 | 2616 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2617 | `	}` |
|   101671 | 2618 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   101671 | 2619 | `	pAttr = pVmAttr->pAttr;` |
|   101671 | 2620 | `	if( pAttr == 0 ){` |
|      ! 0 | 2621 | `		return SXRET_OK;` |
|        - | 2622 | `	}` |
|   101671 | 2623 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - | 2624 | `		/* php's write_property handler for this class refuses outright, and its` |
|        - | 2625 | `		 * sentence is the readonly one -- without the readonly FLAG, which is why` |
|        - | 2626 | `		 * Reflection still reports isReadOnly() false for DatePeriod's seven. The` |
|        - | 2627 | `		 * C bodies that fill them write the slot directly and never come here. */` |
|       23 | 2628 | `		return VmThrowNativeNoWrite(pVm,pVmAttr->pOwner,pAttr);` |
|        - | 2629 | `	}` |
|   101649 | 2630 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|      616 | 2631 | `		sxi32 rcNat = VmRunNativeSet(pVm,pVmAttr,pValue);` |
|      616 | 2632 | `		if( rcNat != SXRET_OK ){` |
|        8 | 2633 | `			return rcNat;` |
|        - | 2634 | `		}` |
|      303 | 2635 | `	}` |
|   101643 | 2636 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      609 | 2637 | `		return SXRET_OK;` |
|        - | 2638 | `	}` |
|        - | 2639 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|        - | 2640 | `	 * the property (a trait's members count as the composing class), not the` |
|        - | 2641 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|   101037 | 2642 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 2643 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - | 2644 | `	 * property may be written exactly once and only from within the declaring` |
|        - | 2645 | `	 * class scope (its set-scope is protected). */` |
|   101037 | 2646 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2647 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - | 2648 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - | 2649 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - | 2650 | `		 * leaves it set, so a later valid initialization still works). */` |
|      149 | 2651 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - | 2652 | `			/* Already initialized: any further write is forbidden, any scope —` |
|        - | 2653 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|        - | 2654 | `			 * Exceptions that fall through to the set-scope check below:` |
|        - | 2655 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|        - | 2656 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|        - | 2657 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|       45 | 2658 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       45 | 2659 | `			if( !(pCloneFr && pCloneFr->pThis` |
|       22 | 2660 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|       43 | 2661 | `				return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,1);` |
|        - | 2662 | `			}` |
|        1 | 2663 | `		}` |
|       53 | 2664 | `	}` |
|   100999 | 2665 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 2666 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|        - | 2667 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|       31 | 2668 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,pVmAttr->pOwner,pAttr);` |
|       31 | 2669 | `		if( rcVis != SXRET_OK ){` |
|       15 | 2670 | `			return rcVis;` |
|        1 | 2671 | `		}` |
|   100977 | 2672 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2673 | `		/* First write (or a clone re-init) must come from within the declaring` |
|        - | 2674 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      109 | 2675 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|      109 | 2676 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 2677 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|        - | 2678 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|        - | 2679 | `		 * hierarchy, so use the composing class (pVmAttr->pOwner) as the set-scope. */` |
|      109 | 2680 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && pVmAttr->pOwner ){` |
|      ! 0 | 2681 | `			pDecl = pVmAttr->pOwner;` |
|      ! 0 | 2682 | `		}` |
|      109 | 2683 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        6 | 2684 | `			return VmThrowReadonlyError(pVm,pVmAttr->pOwner,pAttr,0);` |
|        - | 2685 | `		}` |
|       50 | 2686 | `	}` |
|        - | 2687 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|        - | 2688 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|        - | 2689 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|        - | 2690 | `	 * is its TypeError there), which this used to deny outright. */` |
|   100981 | 2691 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       91 | 2692 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       58 | 2693 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       29 | 2694 | `			bStrict,pHintScope);` |
|       62 | 2695 | `		if( rc == SXRET_OK ){` |
|       38 | 2696 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       38 | 2697 | `			return SXRET_OK;` |
|        - | 2698 | `		}` |
|       28 | 2699 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 2700 | `			char zBuf[128];` |
|       23 | 2701 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        7 | 2702 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 2703 | `		}` |
|       18 | 2704 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 2705 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2706 | `	}` |
|        - | 2707 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - | 2708 | `	 * includes null). */` |
|   100923 | 2709 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       32 | 2710 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       25 | 2711 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 | 2712 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       25 | 2713 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       25 | 2714 | `			return SXRET_OK;` |
|        - | 2715 | `		}` |
|       12 | 2716 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null",bViaRef);` |
|        - | 2717 | `	}` |
|        - | 2718 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - | 2719 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - | 2720 | `	 * type error. */` |
|   100891 | 2721 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2722 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      ! 0 | 2723 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2724 | `	}` |
|        - | 2725 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - | 2726 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - | 2727 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|   100891 | 2728 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 | 2729 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 | 2730 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        5 | 2731 | `			return SXRET_OK;` |
|        - | 2732 | `		}` |
|       10 | 2733 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 2734 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2735 | `	}` |
|        - | 2736 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - | 2737 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - | 2738 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - | 2739 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - | 2740 | `	 * wrongly accept any object / reject arrays. */` |
|   100881 | 2741 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       93 | 2742 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       93 | 2743 | `		if( rcPseudo == 1 ){` |
|       13 | 2744 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       13 | 2745 | `			return SXRET_OK;` |
|        - | 2746 | `		}` |
|       81 | 2747 | `		if( rcPseudo == 0 ){` |
|       11 | 2748 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 2749 | `				VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2750 | `		}` |
|        - | 2751 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       35 | 2752 | `	}` |
|   100863 | 2753 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - | 2754 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|        - | 2755 | `		 * class (pHintScope), not the instance's runtime class. */` |
|       75 | 2756 | `		ph7_class *pExpected = 0;` |
|       75 | 2757 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|        - | 2758 | `			char zBuf[128];` |
|       41 | 2759 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       24 | 2760 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|       18 | 2761 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|       15 | 2762 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2763 | `		}` |
|       50 | 2764 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       50 | 2765 | `		return SXRET_OK;` |
|        - | 2766 | `	}` |
|        - | 2767 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|        - | 2768 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|        - | 2769 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|        - | 2770 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|        - | 2771 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|   100793 | 2772 | `	if( bStrict ){` |
|        - | 2773 | `		int bOk;` |
|       29 | 2774 | `		if( ph7_value_is_bool(pValue) ){` |
|        5 | 2775 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|       27 | 2776 | `		}else if( ph7_value_is_float(pValue) ){` |
|        3 | 2777 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|       24 | 2778 | `		}else if( ph7_value_is_int(pValue) ){` |
|        9 | 2779 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|       19 | 2780 | `		}else if( ph7_value_is_string(pValue) ){` |
|       15 | 2781 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|        8 | 2782 | `		}else{` |
|        - | 2783 | `			/* array / resource / an object against a scalar type: no coercion in` |
|        - | 2784 | `			 * either mode, so the flag test is the whole answer (an object never` |
|        - | 2785 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|        - | 2786 | `			 * does not perform). */` |
|      ! 0 | 2787 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|        - | 2788 | `		}` |
|       29 | 2789 | `		if( !bOk ){` |
|        - | 2790 | `			char zObjBuf[128];` |
|       31 | 2791 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       20 | 2792 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      ! 0 | 2793 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|       20 | 2794 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2795 | `		}` |
|        9 | 2796 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|        3 | 2797 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|        2 | 2798 | `		}else{` |
|        7 | 2799 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2800 | `		}` |
|        9 | 2801 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 2802 | `		return SXRET_OK;` |
|        - | 2803 | `	}` |
|        - | 2804 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - | 2805 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|        - | 2806 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|        - | 2807 | `	 * via __toString, so fall through to the string cast below. */` |
|   100765 | 2808 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       17 | 2809 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       19 | 2810 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|        4 | 2811 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|        - | 2812 | `			char zBuf[128];` |
|       21 | 2813 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        6 | 2814 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 2815 | `		}` |
|        1 | 2816 | `	}` |
|        - | 2817 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|        - | 2818 | `	 * take (VmCoerceScalarWeak's own rule, §10) -- and it had none of it, so this` |
|        - | 2819 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|        - | 2820 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|        - | 2821 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|        - | 2822 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|        - | 2823 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|        - | 2824 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|        - | 2825 | `	 * that arrives carrying a cached int representation is asked too. */` |
|   100753 | 2826 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|       38 | 2827 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       12 | 2828 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2829 | `	}` |
|   100729 | 2830 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|   100107 | 2831 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|   100107 | 2832 | `		if( xCast ){` |
|        - | 2833 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|   100107 | 2834 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       15 | 2835 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        4 | 2836 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2837 | `			}` |
|   100098 | 2838 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       18 | 2839 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 2840 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2841 | `			}` |
|        - | 2842 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - | 2843 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - | 2844 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|   100084 | 2845 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|   100078 | 2846 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|   100081 | 2847 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|   100048 | 2848 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string",bViaRef);` |
|        - | 2849 | `			}` |
|       43 | 2850 | `			xCast(pValue);` |
|       20 | 2851 | `		}` |
|       23 | 2852 | `	}else{` |
|        - | 2853 | `		/* Mask matched — an int property accepting a whole-real must` |
|        - | 2854 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|      627 | 2855 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 2856 | `	}` |
|      667 | 2857 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      667 | 2858 | `	return SXRET_OK;` |
|   534435 | 2859 | `}` |
|        - | 2860 | `/*` |
|        - | 2861 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|        - | 2862 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|        - | 2863 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|        - | 2864 | ` *     a scope that could normally reach it — else a catchable Error),` |
|        - | 2865 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|        - | 2866 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|        - | 2867 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|        - | 2868 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|        - | 2869 | ` *     deprecation notice is not emitted yet — PLAN §3.9 / band A #8).` |
|        - | 2870 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|        - | 2871 | ` */` |
|       30 | 2872 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|        - | 2873 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|        1 | 2874 | `{` |
|       31 | 2875 | `	ph7_class *pClass = pClone->pClass;` |
|        - | 2876 | `	SyHashEntry *pEntry;` |
|        - | 2877 | `	VmClassAttr *pVmAttr;` |
|        - | 2878 | `	ph7_class_attr *pAttr;` |
|        - | 2879 | `	ph7_value *pSlot;` |
|        - | 2880 | `	sxi32 rc;` |
|       31 | 2881 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|       31 | 2882 | `	if( pEntry == 0 ){` |
|        - | 2883 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|        - | 2884 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|        - | 2885 | `		 * deferred residual). */` |
|      ! 0 | 2886 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|      ! 0 | 2887 | `		if( pSlot == 0 ){` |
|      ! 0 | 2888 | `			return PH7_VmMemoryError(pVm);` |
|        - | 2889 | `		}` |
|      ! 0 | 2890 | `		PH7_MemObjStore(pValue,pSlot);` |
|      ! 0 | 2891 | `		return SXRET_OK;` |
|        - | 2892 | `	}` |
|       31 | 2893 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       31 | 2894 | `	pAttr = pVmAttr->pAttr;` |
|        - | 2895 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|       31 | 2896 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 2897 | `		SyBlob sMsg;` |
|      ! 0 | 2898 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 2899 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|      ! 0 | 2900 | `			&pClass->sName,&pAttr->sName);` |
|      ! 0 | 2901 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2902 | `	}` |
|        - | 2903 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|        - | 2904 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|        - | 2905 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|       31 | 2906 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 | 2907 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|        5 | 2908 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pVmAttr->pOwner);` |
|        - | 2909 | `		SyBlob sMsg;` |
|        5 | 2910 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2911 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sName,&pAttr->sName);` |
|        5 | 2912 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 2913 | `	}` |
|        - | 2914 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|       27 | 2915 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,VM_TYPED_STORE_CLONE_INIT);` |
|       27 | 2916 | `	if( rc != SXRET_OK ){` |
|        3 | 2917 | `		return rc;` |
|        - | 2918 | `	}` |
|        - | 2919 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|       25 | 2920 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       25 | 2921 | `	if( pSlot ){` |
|       25 | 2922 | `		PH7_MemObjStore(pValue,pSlot);` |
|       12 | 2923 | `	}` |
|       25 | 2924 | `	return SXRET_OK;` |
|       16 | 2925 | `}` |
|        - | 2926 | `/*` |
|        - | 2927 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - | 2928 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - | 2929 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - | 2930 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - | 2931 | ` */` |
|       12 | 2932 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        3 | 2933 | `{` |
|       15 | 2934 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 2935 | `	char zBuf[128],zType[192];` |
|        - | 2936 | `	const char *zGiven;` |
|       21 | 2937 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        6 | 2938 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|       15 | 2939 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2940 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 2941 | `	}else{` |
|       15 | 2942 | `		zGiven = ph7_type_name(pValue);` |
|        - | 2943 | `	}` |
|       15 | 2944 | `	if( bLazy ){` |
|        - | 2945 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|        - | 2946 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|        - | 2947 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|        - | 2948 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|        - | 2949 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|        - | 2950 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|        - | 2951 | `		 * re-evaluates and re-raises, as php's does. */` |
|        - | 2952 | `		SyBlob sMsg;` |
|        5 | 2953 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 2954 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|        2 | 2955 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        5 | 2956 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 2957 | `	}` |
|        - | 2958 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - | 2959 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - | 2960 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - | 2961 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - | 2962 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - | 2963 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - | 2964 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       11 | 2965 | `	if( pVm->sCodeGen.xErr ){` |
|       12 | 2966 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - | 2967 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        3 | 2968 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        6 | 2969 | `	}else{` |
|        4 | 2970 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 2971 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        1 | 2972 | `			zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|        - | 2973 | `	}` |
|       11 | 2974 | `	pVm->iExitStatus = 255;` |
|       11 | 2975 | `	pVm->bHaltRequested = 1;` |
|       11 | 2976 | `	return SXERR_ABORT;` |
|        9 | 2977 | `}` |
|        - | 2978 | `/*` |
|        - | 2979 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - | 2980 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - | 2981 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - | 2982 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - | 2983 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - | 2984 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - | 2985 | ` */` |
|       54 | 2986 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        5 | 2987 | `{` |
|       59 | 2988 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - | 2989 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|       59 | 2990 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 2991 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 | 2992 | `			return SXRET_OK;` |
|        - | 2993 | `		}` |
|      ! 0 | 2994 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 | 2995 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 2996 | `			return SXRET_OK;` |
|        - | 2997 | `		}` |
|      ! 0 | 2998 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 2999 | `	}` |
|        - | 3000 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|       57 | 3001 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       12 | 3002 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       11 | 3003 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|        8 | 3004 | `			return SXRET_OK;` |
|        - | 3005 | `		}` |
|        3 | 3006 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3007 | `	}` |
|        - | 3008 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|       48 | 3009 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3010 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3011 | `	}` |
|        - | 3012 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|       48 | 3013 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3014 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3015 | `			return SXRET_OK;` |
|        - | 3016 | `		}` |
|      ! 0 | 3017 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3018 | `	}` |
|        - | 3019 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - | 3020 | `	 * a real class/interface verified by instanceof. */` |
|       48 | 3021 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 3022 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        3 | 3023 | `		if( rcPseudo == 1 ){` |
|      ! 0 | 3024 | `			return SXRET_OK;` |
|        - | 3025 | `		}` |
|        3 | 3026 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3027 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3028 | `		}` |
|        - | 3029 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|        - | 3030 | `		 * self/parent resolve against the declaring class. */` |
|        - | 3031 | `		{` |
|        3 | 3032 | `			ph7_class *pExpected = 0;` |
|        4 | 3033 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|        1 | 3034 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|        3 | 3035 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3036 | `			}` |
|        - | 3037 | `		}` |
|      ! 0 | 3038 | `		return SXRET_OK;` |
|        - | 3039 | `	}` |
|        - | 3040 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - | 3041 | `	 * implicit widening. Everything else is a type error.` |
|        - | 3042 | `	 *` |
|        - | 3043 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - | 3044 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|        - | 3045 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|        - | 3046 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|        - | 3047 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|        - | 3048 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|        - | 3049 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|        - | 3050 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|        - | 3051 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|        - | 3052 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|        - | 3053 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|       46 | 3054 | `	if( pValue->iFlags & pAttr->nType ){` |
|       33 | 3055 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       33 | 3056 | `		return SXRET_OK;` |
|        - | 3057 | `	}` |
|       14 | 3058 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 | 3059 | `		PH7_MemObjToReal(pValue);` |
|        3 | 3060 | `		return SXRET_OK;` |
|        - | 3061 | `	}` |
|       11 | 3062 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       32 | 3063 | `}` |
|        - | 3064 | `/*` |
|        - | 3065 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|        - | 3066 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|        - | 3067 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|        - | 3068 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|        - | 3069 | ` */` |
|       34 | 3070 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        2 | 3071 | `{` |
|       36 | 3072 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3073 | `	const char *zGiven;` |
|        - | 3074 | `	char zBuf[128],zType[192];` |
|       53 | 3075 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       17 | 3076 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 3077 | `	SyBlob sMsg;` |
|       36 | 3078 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3079 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 3080 | `	}else{` |
|       36 | 3081 | `		zGiven = ph7_type_name(pValue);` |
|        - | 3082 | `	}` |
|       36 | 3083 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       36 | 3084 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|       17 | 3085 | `		zGiven,&pOwner->sName,&pAttr->sName,zTypeText);` |
|       36 | 3086 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        2 | 3087 | `}` |
|        - | 3088 | `/*` |
|        - | 3089 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|        - | 3090 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|        - | 3091 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|        - | 3092 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|        - | 3093 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|        - | 3094 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|        - | 3095 | ` * instance defaults per-instantiation. Matching structure of` |
|        - | 3096 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|        - | 3097 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|        - | 3098 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|        - | 3099 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|        - | 3100 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|        - | 3101 | ` */` |
|        - | 3102 | `/*` |
|        - | 3103 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|        - | 3104 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|        - | 3105 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|        - | 3106 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|        - | 3107 | ` * throwing, so the static-property mount path can defer the failure (php` |
|        - | 3108 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|        - | 3109 | ` * instance path throws immediately via the wrapper below.` |
|        - | 3110 | ` */` |
|      628 | 3111 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3112 | `{` |
|      633 | 3113 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|      633 | 3114 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       79 | 3115 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       75 | 3116 | `			return SXRET_OK;` |
|        - | 3117 | `		}` |
|        4 | 3118 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 3119 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 3120 | `			return SXRET_OK;` |
|        - | 3121 | `		}` |
|        3 | 3122 | `		return SXERR_INVALID;` |
|        - | 3123 | `	}` |
|      559 | 3124 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       39 | 3125 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       31 | 3126 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       31 | 3127 | `			return SXRET_OK;` |
|        - | 3128 | `		}` |
|      ! 0 | 3129 | `		return SXERR_INVALID;` |
|        - | 3130 | `	}` |
|      533 | 3131 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3132 | `		return SXERR_INVALID;` |
|        - | 3133 | `	}` |
|      533 | 3134 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3135 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3136 | `			return SXRET_OK;` |
|        - | 3137 | `		}` |
|      ! 0 | 3138 | `		return SXERR_INVALID;` |
|        - | 3139 | `	}` |
|      533 | 3140 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        5 | 3141 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        5 | 3142 | `		if( rcPseudo == 1 ){` |
|        5 | 3143 | `			return SXRET_OK;` |
|        - | 3144 | `		}` |
|      ! 0 | 3145 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3146 | `			return SXERR_INVALID;` |
|        - | 3147 | `		}` |
|        - | 3148 | `		{` |
|        - | 3149 | `			/* self/parent in the hint resolve against the declaring class. */` |
|      ! 0 | 3150 | `			ph7_class *pExpected = 0;` |
|      ! 0 | 3151 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|      ! 0 | 3152 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|      ! 0 | 3153 | `				return SXERR_INVALID;` |
|        - | 3154 | `			}` |
|        - | 3155 | `		}` |
|      ! 0 | 3156 | `		return SXRET_OK;` |
|        - | 3157 | `	}` |
|      529 | 3158 | `	if( pValue->iFlags & pAttr->nType ){` |
|      493 | 3159 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      493 | 3160 | `		return SXRET_OK;` |
|        - | 3161 | `	}` |
|       38 | 3162 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        6 | 3163 | `		PH7_MemObjToReal(pValue);` |
|        6 | 3164 | `		return SXRET_OK;` |
|        - | 3165 | `	}` |
|       34 | 3166 | `	return SXERR_INVALID;` |
|      319 | 3167 | `}` |
|      560 | 3168 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3169 | `{` |
|      565 | 3170 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|      553 | 3171 | `		return SXRET_OK;` |
|        - | 3172 | `	}` |
|       13 | 3173 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      285 | 3174 | `}` |
|        - | 3175 | `/*` |
|        - | 3176 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|        - | 3177 | ` * class chain for a static typed slot whose mount-time default failed its` |
|        - | 3178 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|        - | 3179 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|        - | 3180 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|        - | 3181 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|        - | 3182 | ` * never-touched class stays silent, and the throw repeats on every access` |
|        - | 3183 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|        - | 3184 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|        - | 3185 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|        - | 3186 | ` */` |
|       26 | 3187 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|        2 | 3188 | `{` |
|        - | 3189 | `	ph7_class *pScan;` |
|       32 | 3190 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        - | 3191 | `		SyHashEntry *pEntry;` |
|       28 | 3192 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|       32 | 3193 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|       28 | 3194 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       26 | 3195 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|        - | 3196 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|       24 | 3197 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       24 | 3198 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|       24 | 3199 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 | 3200 | `				if( pSlot ){` |
|       24 | 3201 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       24 | 3202 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|       24 | 3203 | `						ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|        - | 3204 | `						ph7_value sNull;` |
|       24 | 3205 | `						if( pValue == 0 ){` |
|      ! 0 | 3206 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|      ! 0 | 3207 | `							pValue = &sNull;` |
|      ! 0 | 3208 | `						}` |
|       24 | 3209 | `						return VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pValue);` |
|        - | 3210 | `					}` |
|      ! 0 | 3211 | `				}` |
|      ! 0 | 3212 | `			}` |
|        1 | 3213 | `		}` |
|        3 | 3214 | `	}` |
|        5 | 3215 | `	return SXRET_OK;` |
|       15 | 3216 | `}` |
|        - | 3217 | `/*` |
|        - | 3218 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|        - | 3219 | ` * materialization: an initializer that threw at mount and was deferred` |
|        - | 3220 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|        - | 3221 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|        - | 3222 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|        - | 3223 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|        - | 3224 | ` * a subclass can be mounted before the base whose default failed.` |
|        - | 3225 | ` */` |
|  2236254 | 3226 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|        5 | 3227 | `{` |
|  4483401 | 3228 | `	while( pClass ){` |
|  2247229 | 3229 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|       84 | 3230 | `			return 1;` |
|        - | 3231 | `		}` |
|  2247147 | 3232 | `		pClass = pClass->pBase;` |
|        5 | 3233 | `	}` |
|  2236177 | 3234 | `	return 0;` |
|  1118111 | 3235 | `}` |
|        - | 3236 | `/*` |
|        - | 3237 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3238 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3239 | ` *` |
|        - | 3240 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3241 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3242 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3243 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3244 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3245 | ` *` |
|        - | 3246 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3247 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3248 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3249 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3250 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3251 | ` * re-raises on every access too.` |
|        - | 3252 | ` */` |
|       86 | 3253 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|        2 | 3254 | `{` |
|        - | 3255 | `	SyHashEntry *pEntry;` |
|        - | 3256 | `	sxi32 rc;` |
|       88 | 3257 | `	if( pClass->pBase ){` |
|        6 | 3258 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|        6 | 3259 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3260 | `			return rc;` |
|        - | 3261 | `		}` |
|        2 | 3262 | `	}` |
|       88 | 3263 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      208 | 3264 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      122 | 3265 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      122 | 3266 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - | 3267 | `			/* Not pending. An inherited slot the base pass already collected` |
|        - | 3268 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|        - | 3269 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|       60 | 3270 | `			continue;` |
|        - | 3271 | `		}` |
|       64 | 3272 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 3273 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|        - | 3274 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|        - | 3275 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|        - | 3276 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|        - | 3277 | `			 * class keeps its hint flag so a later access retries. */` |
|      ! 0 | 3278 | `			*pbLeft = 1;` |
|      ! 0 | 3279 | `			continue;` |
|        - | 3280 | `		}` |
|       64 | 3281 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|       64 | 3282 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3283 | `			return rc;` |
|        - | 3284 | `		}` |
|        2 | 3285 | `	}` |
|       88 | 3286 | `	return SXRET_OK;` |
|       45 | 3287 | `}` |
|        - | 3288 | `/*` |
|        - | 3289 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3290 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3291 | ` *` |
|        - | 3292 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3293 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3294 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3295 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3296 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3297 | ` *` |
|        - | 3298 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3299 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3300 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3301 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3302 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3303 | ` * re-raises on every access too.` |
|        - | 3304 | ` *` |
|        - | 3305 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|        - | 3306 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|        - | 3307 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|        - | 3308 | ` * cut the outer one short.` |
|        - | 3309 | ` */` |
|       82 | 3310 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|        2 | 3311 | `{` |
|        - | 3312 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|        - | 3313 | `	ph7_class_attr **apPending;` |
|        - | 3314 | `	sxu32 n,nUsed;` |
|        - | 3315 | `	sxi32 rc;` |
|       84 | 3316 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       84 | 3317 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|       84 | 3318 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|       84 | 3319 | `	nUsed = SySetUsed(&aPending);` |
|       88 | 3320 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|       62 | 3321 | `		ph7_class_attr *pAttr = apPending[n];` |
|       62 | 3322 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3323 | `		ph7_class *pSaveCtx;` |
|        - | 3324 | `		void *pSaveFrame;` |
|        - | 3325 | `		sxu32 nSaveLazyLine;` |
|        - | 3326 | `		sxi32 nSaveLazyDepth;` |
|        - | 3327 | `		ph7_value *pMemObj;` |
|        - | 3328 | `		sxi32 rcExec;` |
|       62 | 3329 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|      ! 0 | 3330 | `			continue; /* the base pass already ran this shared slot */` |
|        - | 3331 | `		}` |
|       62 | 3332 | `		pMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|       62 | 3333 | `		if( pMemObj == 0 ){` |
|      ! 0 | 3334 | `			continue;` |
|        - | 3335 | `		}` |
|       62 | 3336 | `		pSaveCtx = pVm->pConstEvalClass;` |
|       62 | 3337 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|       62 | 3338 | `		pVm->pConstEvalClass = pOwner;` |
|        - | 3339 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|        - | 3340 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|        - | 3341 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|        - | 3342 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|        - | 3343 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|       62 | 3344 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 3345 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|        - | 3346 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|       62 | 3347 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|       62 | 3348 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|       62 | 3349 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|       62 | 3350 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|       62 | 3351 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|       62 | 3352 | `		pVm->nConstEvalDepth++;` |
|       62 | 3353 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|       62 | 3354 | `		pVm->nConstEvalDepth--;` |
|       62 | 3355 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|       62 | 3356 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|       62 | 3357 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       62 | 3358 | `		pVm->pConstEvalClass = pSaveCtx;` |
|       62 | 3359 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|       62 | 3360 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 3361 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|        - | 3362 | `			 * caller to route (a catch here is the user's own). */` |
|       58 | 3363 | `			rc = rcExec;` |
|       58 | 3364 | `			break;` |
|        - | 3365 | `		}` |
|        5 | 3366 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|        5 | 3367 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 3368 | `			/* The initializer named a self-referencing constant. Like the mount` |
|        - | 3369 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|        - | 3370 | `			 * the access, where a catch can see it. */` |
|      ! 0 | 3371 | `			rc = VmConstCycleThrow(&(*pVm));` |
|      ! 0 | 3372 | `			break;` |
|        - | 3373 | `		}` |
|        4 | 3374 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|        3 | 3375 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 3376 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|        - | 3377 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|        - | 3378 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|      ! 0 | 3379 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      ! 0 | 3380 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|      ! 0 | 3381 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      ! 0 | 3382 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|      ! 0 | 3383 | `				rc = VmDefaultPropertyTypeError(&(*pVm),pVmAttr->pOwner,pAttr,pMemObj);` |
|      ! 0 | 3384 | `				break;` |
|        - | 3385 | `			}` |
|      ! 0 | 3386 | `		}` |
|        3 | 3387 | `	}` |
|       84 | 3388 | `	if( rc != SXRET_OK ){` |
|       58 | 3389 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|       28 | 3390 | `	}` |
|       84 | 3391 | `	SySetRelease(&aPending);` |
|       84 | 3392 | `	return rc;` |
|        2 | 3393 | `}` |
|        - | 3394 | `/*` |
|        - | 3395 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|        - | 3396 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|        - | 3397 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|        - | 3398 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|        - | 3399 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|        - | 3400 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|        - | 3401 | ` */` |
|       82 | 3402 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|        2 | 3403 | `{` |
|       84 | 3404 | `	int bLeft = 0;` |
|       84 | 3405 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|       84 | 3406 | `	if( rc == SXRET_OK ){` |
|       28 | 3407 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|       13 | 3408 | `	}` |
|       84 | 3409 | `	if( rc == SXRET_OK && !bLeft ){` |
|        - | 3410 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|        - | 3411 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|        - | 3412 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|        - | 3413 | `		 * set — php's materialization keeps failing too. */` |
|        - | 3414 | `		ph7_class *pScan;` |
|        9 | 3415 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        5 | 3416 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|        3 | 3417 | `		}` |
|        2 | 3418 | `	}` |
|       84 | 3419 | `	return rc;` |
|        2 | 3420 | `}` |
|        - | 3421 |  |
|        - | 3422 | `/*` |
|        - | 3423 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3424 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 3425 | ` * information.` |
|        - | 3426 | ` * ------------------------------------` |
|        - | 3427 | ` * Simple boring wrapper function.` |
|        - | 3428 | ` * ------------------------------------` |
|        - | 3429 | ` */` |
|     2322 | 3430 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        5 | 3431 | `{` |
|        - | 3432 | `	va_list ap;` |
|        - | 3433 | `	sxi32 rc;` |
|     2327 | 3434 | `	va_start(ap,zFormat);` |
|     2327 | 3435 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     2327 | 3436 | `	va_end(ap);` |
|     2327 | 3437 | `	return rc;` |
|        5 | 3438 | `}` |
|        - | 3439 | `/*` |
|        - | 3440 | ` * php prefixes an argument diagnostic with the class the callee belongs to: the` |
|        - | 3441 | ` * owner the caller already knows for a METHOD, and for a CLOSURE the class it was` |
|        - | 3442 | `` * WRITTEN inside -- `C::{closure:C::m():5}`, which php reads off the function's own`` |
|        - | 3443 | `` * scope and PHL records at compile time. Appends `Name::` and answers 1 when there`` |
|        - | 3444 | ` * is one.` |
|        - | 3445 | ` */` |
|     4470 | 3446 | `static int VmArgOwnerPrefix(ph7_vm *pVm,SyBlob *pOut,ph7_class *pOwnerClass,ph7_vm_func *pCallee)` |
|        5 | 3447 | `{` |
|     4475 | 3448 | `	SyString *pName = 0;` |
|     4475 | 3449 | `	if( pOwnerClass ){` |
|       15 | 3450 | `		pName = &pOwnerClass->sName;` |
|     4469 | 3451 | `	}else if( pCallee ){` |
|        - | 3452 | ``		/* A closure's scope is REBINDABLE (`Closure::bind($c, null, B::class)`), and`` |
|        - | 3453 | `		 * php reports the scope it is running under -- which the frame records. The` |
|        - | 3454 | `		 * declared one is the answer when there is no frame of the callee's to ask,` |
|        - | 3455 | `		 * or when nothing rebound it. */` |
|     4463 | 3456 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     4463 | 3457 | `		if( pFrame && pFrame->pUserData == (void *)pCallee && pFrame->pBoundScope ){` |
|        6 | 3458 | `			pName = &pFrame->pBoundScope->sName;` |
|     4461 | 3459 | `		}else if( SyStringLength(&pCallee->sClosureScope) > 0 ){` |
|        9 | 3460 | `			pName = &pCallee->sClosureScope;` |
|        4 | 3461 | `		}` |
|     2229 | 3462 | `	}` |
|     4475 | 3463 | `	if( pName == 0 ){` |
|     4451 | 3464 | `		return 0;` |
|        - | 3465 | `	}` |
|       27 | 3466 | `	SyBlobFormat(pOut,"%z::",pName);` |
|       27 | 3467 | `	return 1;` |
|     2240 | 3468 | `}` |
|        - | 3469 | `/*` |
|        - | 3470 | `` * The CALL SITE php names in an argument diagnostic: the `called in FILE on line`` |
|        - | 3471 | `` * N` tail of a TypeError, and the `in FILE on line N` of an ArgumentCountError.`` |
|        - | 3472 | ` * It is the CALLER's position -- which the callee's frame recorded for itself when` |
|        - | 3473 | ` * it was entered (VmEnterFrame). PHL read the top of the INCLUDE stack, and the` |
|        - | 3474 | ` * count error had a hard-coded line 1, so every one of these raised inside a` |
|        - | 3475 | ` * vendor package named the entry script and an arbitrary line.` |
|        - | 3476 | ` *` |
|        - | 3477 | ` * The top frame is only the callee's when the raise happens AFTER VmEnterFrame; a` |
|        - | 3478 | ` * generator/fiber argument install runs before one is pushed, so the identity is` |
|        - | 3479 | ` * checked rather than assumed. With no frame of the callee's to read, the position` |
|        - | 3480 | ` * running right now IS the call site, which is what the fallback names.` |
|        - | 3481 | ` */` |
|      454 | 3482 | `static void VmArgCallSite(ph7_vm *pVm,ph7_vm_func *pCallee,SyString **ppFile,sxu32 *pnLine)` |
|        5 | 3483 | `{` |
|      459 | 3484 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      459 | 3485 | `	*ppFile = 0;` |
|      459 | 3486 | `	*pnLine = pVm->nCurLine;` |
|      459 | 3487 | `	if( pCallee && pFrame && pFrame->pUserData == (void *)pCallee ){` |
|      459 | 3488 | `		if( SyStringLength(&pFrame->sCallFile) > 0 ){` |
|      439 | 3489 | `			*ppFile = &pFrame->sCallFile;` |
|      217 | 3490 | `		}` |
|      459 | 3491 | `		if( pFrame->nCallLine ){` |
|      439 | 3492 | `			*pnLine = pFrame->nCallLine;` |
|      217 | 3493 | `		}` |
|      227 | 3494 | `	}` |
|      459 | 3495 | `	if( *ppFile == 0 ){` |
|       22 | 3496 | `		*ppFile = PH7_VmExecutingUnitFile(pVm);` |
|       10 | 3497 | `	}` |
|      459 | 3498 | `}` |
|        - | 3499 | `/*` |
|        - | 3500 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - | 3501 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - | 3502 | ` */` |
|      444 | 3503 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 | 3504 | `{` |
|        - | 3505 | `	ph7_class *pClass;` |
|        - | 3506 | `	ph7_class_instance *pThis;` |
|        - | 3507 | `	ph7_class_method *pCons;` |
|        - | 3508 | `	ph7_value sArg;` |
|        - | 3509 | `	ph7_value *apArg[1];` |
|        - | 3510 | `	SyBlob sMsg;` |
|        - | 3511 | `	SyString sMsgStr;` |
|      449 | 3512 | `	SyString *pFuncName = &pCallee->sName;` |
|        - | 3513 | `	VmFrame *pFrame;` |
|        - | 3514 | `	sxi32 rc;` |
|      449 | 3515 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      449 | 3516 | `	if( pClass == 0 ){` |
|      ! 0 | 3517 | `		return PH7_ABORT;` |
|        - | 3518 | `	}` |
|      449 | 3519 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      449 | 3520 | `	if( pThis == 0 ){` |
|      ! 0 | 3521 | `		return PH7_ABORT;` |
|        - | 3522 | `	}` |
|      449 | 3523 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 3524 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|        - | 3525 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|        - | 3526 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|        - | 3527 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|        - | 3528 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|      669 | 3529 | `	if( pOwnerClass ){` |
|        - | 3530 | `		/* A property hook is named after its PROPERTY, never after the method` |
|        - | 3531 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|        - | 3532 | `		SyBlob sHook;` |
|       61 | 3533 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|       61 | 3534 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|        6 | 3535 | `			if( pArgName ){` |
|        6 | 3536 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|        4 | 3537 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|        2 | 3538 | `					nArg,pArgName,zExpected,zGiven);` |
|        4 | 3539 | `			}else{` |
|      ! 0 | 3540 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      ! 0 | 3541 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|      ! 0 | 3542 | `					nArg,zExpected,zGiven);` |
|        - | 3543 | `			}` |
|        6 | 3544 | `			SyBlobRelease(&sHook);` |
|        6 | 3545 | `			goto ArgMsgBuilt;` |
|        - | 3546 | `		}` |
|       57 | 3547 | `		SyBlobRelease(&sHook);` |
|       57 | 3548 | `		if( pArgName ){` |
|       48 | 3549 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       22 | 3550 | `				&pOwnerClass->sName,pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       26 | 3551 | `		}else{` |
|       11 | 3552 | `			SyBlobFormat(&sMsg,"%z::%z(): Argument #%u must be of type %s, %s given",` |
|        4 | 3553 | `				&pOwnerClass->sName,pFuncName,nArg,zExpected,zGiven);` |
|        - | 3554 | `		}` |
|       31 | 3555 | `	}else{` |
|        - | 3556 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|      393 | 3557 | `		const char *zShow = 0;` |
|      393 | 3558 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|      393 | 3559 | `		VmArgOwnerPrefix(pVm,&sMsg,0,pCallee);` |
|      393 | 3560 | `		if( pArgName ){` |
|      323 | 3561 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|      159 | 3562 | `				nShow,zShow,nArg,pArgName,zExpected,zGiven);` |
|      164 | 3563 | `		}else{` |
|       75 | 3564 | `			SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|       35 | 3565 | `				nShow,zShow,nArg,zExpected,zGiven);` |
|        - | 3566 | `		}` |
|        - | 3567 | `	}` |
|      222 | 3568 | `ArgMsgBuilt:` |
|        - | 3569 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|        - | 3570 | `	 * (hosted C) functions get the bare message. nCurLine is the line of the` |
|        - | 3571 | `	 * call instruction being bound, which is exactly php's "called in". */` |
|      449 | 3572 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 ){` |
|        - | 3573 | `		SyString *pCallFile;` |
|        - | 3574 | `		sxu32 nCallLine;` |
|      417 | 3575 | `		VmArgCallSite(pVm,pCallee,&pCallFile,&nCallLine);` |
|      417 | 3576 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|      417 | 3577 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,nCallLine);` |
|      206 | 3578 | `		}` |
|      206 | 3579 | `	}` |
|      449 | 3580 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      449 | 3581 | `	if( pCons ){` |
|      449 | 3582 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      449 | 3583 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      449 | 3584 | `		apArg[0] = &sArg;` |
|      449 | 3585 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      449 | 3586 | `		PH7_MemObjRelease(&sArg);` |
|      222 | 3587 | `	}` |
|      449 | 3588 | `	SyBlobRelease(&sMsg);` |
|      449 | 3589 | `	pFrame = pVm->pFrame;` |
|      449 | 3590 | `	if( pFrame ){` |
|      449 | 3591 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      449 | 3592 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      222 | 3593 | `	}` |
|      449 | 3594 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      449 | 3595 | `	PH7_ClassInstanceUnref(pThis);` |
|      449 | 3596 | `	if( rc == SXERR_ABORT ){` |
|        6 | 3597 | `		return PH7_ABORT;` |
|        - | 3598 | `	}` |
|      445 | 3599 | `	return PH7_EXCEPTION;` |
|      227 | 3600 | `}` |
|        - | 3601 | `/*` |
|        - | 3602 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|        - | 3603 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|        - | 3604 | ` * positional and the named-argument binding paths of OP_CALL.` |
|        - | 3605 | ` *` |
|        - | 3606 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|        - | 3607 | ` * element uses its overall call position; a NAMED element always reports` |
|        - | 3608 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|        - | 3609 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|        - | 3610 | ` * formal, so no single parameter name applies.` |
|        - | 3611 | ` *` |
|        - | 3612 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|        - | 3613 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|        - | 3614 | ` */` |
|     3103 | 3615 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|        - | 3616 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|        5 | 3617 | `{` |
|        - | 3618 | `	sxi32 rc;` |
|     3108 | 3619 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|       33 | 3620 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|       37 | 3621 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|        - | 3622 | `			const char *zGiven;` |
|       11 | 3623 | `			const char *zExpected = "union";` |
|        - | 3624 | `			char zBuf[128];` |
|        - | 3625 | `			char zTypeBuf[128];` |
|       11 | 3626 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 | 3627 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       10 | 3628 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 3629 | `				zGiven = "null";` |
|      ! 0 | 3630 | `			}else{` |
|        9 | 3631 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 3632 | `			}` |
|       11 | 3633 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       15 | 3634 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|        4 | 3635 | `					zTypeBuf,sizeof(zTypeBuf));` |
|        4 | 3636 | `			}` |
|       11 | 3637 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       11 | 3638 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3639 | `		}` |
|       17 | 3640 | `		return SXRET_OK;` |
|        - | 3641 | `	}` |
|     3081 | 3642 | `	if( pFormal->nType < 1` |
|     1768 | 3643 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|     2653 | 3644 | `		return SXRET_OK;` |
|        - | 3645 | `	}` |
|      438 | 3646 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 3647 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|        - | 3648 | `		 * per element exactly like the non-variadic paths. */` |
|       61 | 3649 | `		SyString *pName = &pFormal->sClass;` |
|        - | 3650 | `		ph7_class *pClass;` |
|       61 | 3651 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       61 | 3652 | `		if( rcPseudo == 0 ){` |
|        - | 3653 | `			/* Recognised pseudo-type; value mismatches */` |
|        - | 3654 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       14 | 3655 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        6 | 3656 | `				VmClassHintTypeName(pName,0,` |
|        6 | 3657 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|        3 | 3658 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        8 | 3659 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3660 | `		}` |
|        - | 3661 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|        - | 3662 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|        - | 3663 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|        - | 3664 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|        - | 3665 | `		 * so ANY non-object is a TypeError, matching php. */` |
|       55 | 3666 | `		pClass = 0;` |
|       55 | 3667 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 3668 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       43 | 3669 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 3670 | `				VmClassHintTypeName(pName,pClass,` |
|       20 | 3671 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 3672 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       23 | 3673 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3674 | `		}` |
|       34 | 3675 | `		return SXRET_OK;` |
|        - | 3676 | `	}` |
|      380 | 3677 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       66 | 3678 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|        - | 3679 | `			char zGivenBuf[128];` |
|        8 | 3680 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        2 | 3681 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        6 | 3682 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3683 | `		}` |
|       62 | 3684 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 3685 | `			char zTypeBuf[128];` |
|        - | 3686 | `			char zGivenBuf[128];` |
|       64 | 3687 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 3688 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       20 | 3689 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       44 | 3690 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 3691 | `		}` |
|       10 | 3692 | `	}else{` |
|        - | 3693 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|        - | 3694 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      318 | 3695 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 3696 | `	}` |
|      336 | 3697 | `	return SXRET_OK;` |
|     1444 | 3698 | `}` |
|        - | 3699 | `/*` |
|        - | 3700 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|        - | 3701 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|        - | 3702 | ` * before a required parameter as implicitly required), excluding a trailing` |
|        - | 3703 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|        - | 3704 | ` * pick php's wording — "exactly N expected" when required == total,` |
|        - | 3705 | ` * "at least N" when trailing optionals exist.` |
|        - | 3706 | ` */` |
|    11019 | 3707 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|        5 | 3708 | `{` |
|    11024 | 3709 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    11024 | 3710 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|    11024 | 3711 | `	sxu32 nRequired = 0;` |
|        - | 3712 | `	sxu32 n;` |
|    11024 | 3713 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      934 | 3714 | `		nFormal--;` |
|      417 | 3715 | `	}` |
|    49132 | 3716 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|    38113 | 3717 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    14090 | 3718 | `			nRequired = n + 1;` |
|     7029 | 3719 | `		}` |
|    19036 | 3720 | `	}` |
|    11024 | 3721 | `	*pnNonVariadic = nFormal;` |
|    11024 | 3722 | `	return nRequired;` |
|        5 | 3723 | `}` |
|        - | 3724 | `/*` |
|        - | 3725 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|        - | 3726 | ` * with too few arguments:` |
|        - | 3727 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|        - | 3728 | ` *   {exactly\|at least} M expected` |
|        - | 3729 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|        - | 3730 | ` * info yet (the runtime line-tracking gate, NEWPLAN §6), so the line is a` |
|        - | 3731 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|        - | 3732 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|        - | 3733 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|        - | 3734 | ` */` |
|       50 | 3735 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3736 | `	ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|        3 | 3737 | `{` |
|        - | 3738 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|        - | 3739 | `	SyBlob sMsg;` |
|        - | 3740 | ``	/* A CLOSURE has no name of its own: php calls it `{closure:file:line}` (or`` |
|        - | 3741 | ``	 * `{closure:enclosing():line}` for one written inside a function), and this`` |
|        - | 3742 | `	 * message was the last of the four argument diagnostics still printing the` |
|        - | 3743 | ``	 * engine's internal `[closure_N]` instead. */`` |
|        - | 3744 | `	{` |
|       53 | 3745 | `		const char *zShow = 0;` |
|       53 | 3746 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|       53 | 3747 | `		if( nShow < 1 ){` |
|      ! 0 | 3748 | `			zShow = SyStringData(pFuncName);` |
|      ! 0 | 3749 | `			nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 3750 | `		}` |
|       53 | 3751 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       53 | 3752 | `		SyBlobAppend(&sMsg,"Too few arguments to function ",sizeof("Too few arguments to function ")-1);` |
|       53 | 3753 | `		VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|       53 | 3754 | `		SyBlobFormat(&sMsg,"%.*s(), %u passed",nShow,zShow,nPassed);` |
|        - | 3755 | `	}` |
|       53 | 3756 | `	if( bCallSite ){` |
|        - | 3757 | `		SyString *pFile;` |
|        - | 3758 | `		sxu32 nCallLine;` |
|       45 | 3759 | `		VmArgCallSite(pVm,pCallee,&pFile,&nCallLine);` |
|       24 | 3760 | `		SyBlobFormat(&sMsg," in %z on line %u",` |
|       42 | 3761 | `			(pFile && pFile->nByte > 0) ? (const SyString *)pFile : &sUnknown,nCallLine);` |
|       21 | 3762 | `	}` |
|       53 | 3763 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|       25 | 3764 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|        - | 3765 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       53 | 3766 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        3 | 3767 | `}` |
|        - | 3768 | `/*` |
|        - | 3769 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|        - | 3770 | ` * cannot be referenced:` |
|        - | 3771 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|        - | 3772 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|        - | 3773 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|        - | 3774 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|        - | 3775 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|        - | 3776 | `` * the bare `m()`.`` |
|        - | 3777 | ` */` |
|        - | 3778 | `/*` |
|        - | 3779 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|        - | 3780 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|        - | 3781 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|        - | 3782 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|        - | 3783 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|        - | 3784 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|        - | 3785 | `` * php's `{closure:file:line}`.`` |
|        - | 3786 | ` */` |
|       74 | 3787 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|        - | 3788 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        1 | 3789 | `{` |
|       75 | 3790 | `	const char *zShow = 0;` |
|       75 | 3791 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|        - | 3792 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|        - | 3793 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|        - | 3794 | `	 * single name applies (the refusal message splits the same way). */` |
|       75 | 3795 | `	if( pOwnerClass && pArgName ){` |
|       34 | 3796 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3797 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       11 | 3798 | `			&pOwnerClass->sName,nShow,zShow,nArgPos,pArgName);` |
|       64 | 3799 | `	}else if( pOwnerClass ){` |
|      ! 0 | 3800 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3801 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|      ! 0 | 3802 | `			&pOwnerClass->sName,nShow,zShow,nArgPos);` |
|       53 | 3803 | `	}else if( pArgName ){` |
|       73 | 3804 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3805 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       24 | 3806 | `			nShow,zShow,nArgPos,pArgName);` |
|       25 | 3807 | `	}else{` |
|        7 | 3808 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 3809 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|        2 | 3810 | `			nShow,zShow,nArgPos);` |
|        - | 3811 | `	}` |
|       75 | 3812 | `}` |
|     4028 | 3813 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3814 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        3 | 3815 | `{` |
|        - | 3816 | `	SyBlob sMsg;` |
|     4031 | 3817 | `	const char *zShow = 0;` |
|     4031 | 3818 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     4031 | 3819 | `	if( nShow < 1 ){` |
|      ! 0 | 3820 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 3821 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 3822 | `	}` |
|     4031 | 3823 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     4031 | 3824 | `	VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|     4031 | 3825 | `	SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) could not be passed by reference",` |
|     2014 | 3826 | `		nShow,zShow,nArgPos,pArgName);` |
|        - | 3827 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     4031 | 3828 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 3829 | `}` |
|        - | 3830 | `/*` |
|        - | 3831 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|        - | 3832 | ` * called with too few arguments, in php's ZPP wording:` |
|        - | 3833 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|        - | 3834 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|        - | 3835 | ` * pluralized on the expected count).` |
|        - | 3836 | ` *` |
|        - | 3837 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|        - | 3838 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|        - | 3839 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|        - | 3840 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|        - | 3841 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|        - | 3842 | ` */` |
|       24 | 3843 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3844 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|        1 | 3845 | `{` |
|        - | 3846 | `	SyBlob sMsg;` |
|       25 | 3847 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|       25 | 3848 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       25 | 3849 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       25 | 3850 | `	if( pOwnerClass ){` |
|      ! 0 | 3851 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3852 | `			&pOwnerClass->sName,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      ! 0 | 3853 | `	}else{` |
|       25 | 3854 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       12 | 3855 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|        - | 3856 | `	}` |
|        - | 3857 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       25 | 3858 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3859 | `}` |
|        - | 3860 | `/*` |
|        - | 3861 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|        - | 3862 | ` * MANY arguments, in php's ZPP wording:` |
|        - | 3863 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|        - | 3864 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|        - | 3865 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|        - | 3866 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|        - | 3867 | ` * so the caller must not route such a callee here.` |
|        - | 3868 | ` */` |
|       22 | 3869 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3870 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|        1 | 3871 | `{` |
|        - | 3872 | `	SyBlob sMsg;` |
|       23 | 3873 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|       23 | 3874 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|       23 | 3875 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 3876 | `	if( pOwnerClass ){` |
|      ! 0 | 3877 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 3878 | `			&pOwnerClass->sName,pFuncName,zKind,nMax,zPlural,nPassed);` |
|      ! 0 | 3879 | `	}else{` |
|       23 | 3880 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       11 | 3881 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|        - | 3882 | `	}` |
|        - | 3883 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       23 | 3884 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3885 | `}` |
|        - | 3886 | `/*` |
|        - | 3887 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|        - | 3888 | ` * named or positional argument resolved to:` |
|        - | 3889 | ` *   C::f(): Argument #N ($x) not passed` |
|        - | 3890 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|        - | 3891 | ` */` |
|        4 | 3892 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 3893 | `	ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName)` |
|        1 | 3894 | `{` |
|        - | 3895 | `	SyBlob sMsg;` |
|        5 | 3896 | `	const char *zShow = 0;` |
|        5 | 3897 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|        5 | 3898 | `	if( nShow < 1 ){` |
|      ! 0 | 3899 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 3900 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 3901 | `	}` |
|        5 | 3902 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 3903 | `	VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|        5 | 3904 | `	SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) not passed",nShow,zShow,nArg,pArgName);` |
|        - | 3905 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|        5 | 3906 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 3907 | `}` |
|        - | 3908 | `/*` |
|        - | 3909 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - | 3910 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - | 3911 | ` */` |
|        - | 3912 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|        - | 3913 | ` * The message is copied into the instance by __construct, so the caller owns` |
|        - | 3914 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|      148 | 3915 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|        5 | 3916 | `{` |
|        - | 3917 | `	ph7_class *pClass;` |
|        - | 3918 | `	ph7_class_instance *pThis;` |
|        - | 3919 | `	ph7_class_method *pCons;` |
|        - | 3920 | `	ph7_value sArg;` |
|        - | 3921 | `	ph7_value *apArg[1];` |
|        - | 3922 | `	SyString sMsgStr;` |
|        - | 3923 | `	VmFrame *pFrame;` |
|        - | 3924 | `	sxi32 rc;` |
|      153 | 3925 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      153 | 3926 | `	if( pClass == 0 ){` |
|      ! 0 | 3927 | `		return PH7_ABORT;` |
|        - | 3928 | `	}` |
|      153 | 3929 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      153 | 3930 | `	if( pThis == 0 ){` |
|      ! 0 | 3931 | `		return PH7_ABORT;` |
|        - | 3932 | `	}` |
|      153 | 3933 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      153 | 3934 | `	if( pCons ){` |
|      153 | 3935 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      153 | 3936 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      153 | 3937 | `		apArg[0] = &sArg;` |
|      153 | 3938 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      153 | 3939 | `		PH7_MemObjRelease(&sArg);` |
|       74 | 3940 | `	}` |
|      153 | 3941 | `	pFrame = pVm->pFrame;` |
|      153 | 3942 | `	if( pFrame ){` |
|      153 | 3943 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      153 | 3944 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       74 | 3945 | `	}` |
|      153 | 3946 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      153 | 3947 | `	PH7_ClassInstanceUnref(pThis);` |
|      153 | 3948 | `	if( rc == SXERR_ABORT ){` |
|        6 | 3949 | `		return PH7_ABORT;` |
|        - | 3950 | `	}` |
|      149 | 3951 | `	return PH7_EXCEPTION;` |
|       79 | 3952 | `}` |
|        - | 3953 | `/*` |
|        - | 3954 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|        - | 3955 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|        - | 3956 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|        - | 3957 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|        - | 3958 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|        - | 3959 | ` * other callee falls through to the ordinary Class::method rendering.` |
|        - | 3960 | ` */` |
|        - | 3961 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|   646601 | 3962 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|        5 | 3963 | `{` |
|   646606 | 3964 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|   646606 | 3965 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|   646156 | 3966 | `		return 0;` |
|        - | 3967 | `	}` |
|      454 | 3968 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      411 | 3969 | `		return 0;` |
|        - | 3970 | `	}` |
|       46 | 3971 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|       33 | 3972 | `		*pzKind = "get";` |
|       30 | 3973 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|       15 | 3974 | `		*pzKind = "set";` |
|        9 | 3975 | `	}else{` |
|      ! 0 | 3976 | `		return 0;` |
|        - | 3977 | `	}` |
|       46 | 3978 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|       46 | 3979 | `	return 1;` |
|   323253 | 3980 | `}` |
|      150 | 3981 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3982 | `{` |
|        - | 3983 | `	SyString sProp;` |
|        - | 3984 | `	const char *zKind;` |
|      155 | 3985 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      145 | 3986 | `		return 0;` |
|        - | 3987 | `	}` |
|       13 | 3988 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sName,&sProp,zKind);` |
|       13 | 3989 | `	return 1;` |
|       80 | 3990 | `}` |
|        - | 3991 | `/*` |
|        - | 3992 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|        - | 3993 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|        - | 3994 | ` * anything else uses its display name (which is also what strips a closure's` |
|        - | 3995 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|        - | 3996 | ` * instead — they are thrown from call sites that already resolved it.` |
|        - | 3997 | ` */` |
|      170 | 3998 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 3999 | `{` |
|      175 | 4000 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 4001 | `		/* ...and a TRAIT method's declaring class is the trait, which php does not` |
|        - | 4002 | `		 * have at run time: it composed the method INTO the using class, and names` |
|        - | 4003 | `` 		 * that class here. `trait T { function m(): self {…} } class C { use T; }` `` |
|        - | 4004 | ``		 * reported `T::m(): Return value must be…` where php says `C::m()`, while`` |
|        - | 4005 | `		 * the ARGUMENT-side twin (which is handed an already-resolved owner) said` |
|        - | 4006 | ``		 * `C::m()` in the same function. PH7_VmMemberOwnerClass is the shared walk;`` |
|        - | 4007 | `		 * it needs the class the call was made THROUGH to find the user, and answers` |
|        - | 4008 | `		 * 0 only when there is none — keep the declaring class for that. */` |
|       99 | 4009 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|       99 | 4010 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pDecl,PH7_VmPeekTopClass(pVm));` |
|       99 | 4011 | `		if( pOwner == 0 ){` |
|      ! 0 | 4012 | `			pOwner = pDecl;` |
|      ! 0 | 4013 | `		}` |
|       99 | 4014 | `		if( PH7_VmHookFuncName(pOwner,pFunc,pOut) ){` |
|        7 | 4015 | `			return;` |
|        - | 4016 | `		}` |
|       93 | 4017 | `		SyBlobFormat(pOut,"%z::%z",&pOwner->sName,&pFunc->sName);` |
|       93 | 4018 | `		return;` |
|        - | 4019 | `	}` |
|        - | 4020 | `	{` |
|       81 | 4021 | `		const char *zShow = 0;` |
|       81 | 4022 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|       81 | 4023 | `		if( zShow && nShow > 0 ){` |
|       81 | 4024 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       38 | 4025 | `		}` |
|        - | 4026 | `	}` |
|       90 | 4027 | `}` |
|      144 | 4028 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|        5 | 4029 | `{` |
|        - | 4030 | `	SyBlob sMsg,sName;` |
|        - | 4031 | `	sxi32 rc;` |
|      149 | 4032 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      149 | 4033 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|      149 | 4034 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|      149 | 4035 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|      144 | 4036 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|      149 | 4037 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      149 | 4038 | `	SyBlobRelease(&sName);` |
|      149 | 4039 | `	SyBlobRelease(&sMsg);` |
|      149 | 4040 | `	return rc;` |
|        5 | 4041 | `}` |
|        - | 4042 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|        - | 4043 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|        - | 4044 | ` * php calls it a "method" when it is one. */` |
|        4 | 4045 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        2 | 4046 | `{` |
|        - | 4047 | `	SyBlob sMsg,sName;` |
|        - | 4048 | `	sxi32 rc;` |
|        6 | 4049 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 4050 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|        6 | 4051 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|        6 | 4052 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|        4 | 4053 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|        4 | 4054 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|        6 | 4055 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|        6 | 4056 | `	SyBlobRelease(&sName);` |
|        6 | 4057 | `	SyBlobRelease(&sMsg);` |
|        6 | 4058 | `	return rc;` |
|        2 | 4059 | `}` |
|        - | 4060 | `/*` |
|        - | 4061 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - | 4062 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - | 4063 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - | 4064 | ` *` |
|        - | 4065 | ` * A class name LONGER than the buffer is answered from the class itself rather` |
|        - | 4066 | ` * than cut down to fit: PH7_NewClass() duplicates the name with` |
|        - | 4067 | ` * SyMemBackendStrDup(), which appends the NUL, and the class outlives the` |
|        - | 4068 | ` * message being built. php prints the whole name however long it is, and a` |
|        - | 4069 | ` * truncated one would name a class that does not exist.` |
|        - | 4070 | ` */` |
|     1600 | 4071 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        5 | 4072 | `{` |
|     1605 | 4073 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      164 | 4074 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 4075 | `	}` |
|     1446 | 4076 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      150 | 4077 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      150 | 4078 | `		if( pThis && pThis->pClass ){` |
|      150 | 4079 | `			SyString *pName = &pThis->pClass->sName;` |
|      150 | 4080 | `			if( pName->nByte >= nBuf ){` |
|        3 | 4081 | `				return pName->zString;` |
|        - | 4082 | `			}` |
|      148 | 4083 | `			SyMemcpy(pName->zString,zBuf,pName->nByte);` |
|      148 | 4084 | `			zBuf[pName->nByte] = 0;` |
|      148 | 4085 | `			return zBuf;` |
|        - | 4086 | `		}` |
|      ! 0 | 4087 | `		return "object";` |
|        - | 4088 | `	}` |
|     1301 | 4089 | `	return ph7_type_name(pVal);` |
|      800 | 4090 | `}` |
|        - | 4091 | `/*` |
|        - | 4092 | ` * Throw a PHP-compatible Error when unpacking ('...$expr') receives something` |
|        - | 4093 | ` * that is neither an array nor a Traversable. Matches the message and class PHP` |
|        - | 4094 | ` * raises ("Only arrays and Traversables can be unpacked, X given").` |
|        - | 4095 | ` *` |
|        - | 4096 | ` * php picks the class from BOTH the value and the site: an ARRAY-literal unpack` |
|        - | 4097 | `` * (`[...$x]`) is \TypeError for an object and plain \Error for every scalar,`` |
|        - | 4098 | `` * while an ARGUMENT unpack (`f(...$x)`, `new C(...$x)`) is \TypeError for all of`` |
|        - | 4099 | ` * them — bArgUnpack says which site is asking.` |
|        - | 4100 | ` */` |
|       90 | 4101 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack)` |
|        4 | 4102 | `{` |
|        - | 4103 | `	ph7_class *pClass;` |
|        - | 4104 | `	ph7_class_instance *pThis;` |
|        - | 4105 | `	ph7_class_method *pCons;` |
|        - | 4106 | `	ph7_value sArg;` |
|        - | 4107 | `	ph7_value *apArg[1];` |
|        - | 4108 | `	SyBlob sMsg;` |
|        - | 4109 | `	SyString sMsgStr;` |
|        - | 4110 | `	VmFrame *pFrame;` |
|        - | 4111 | `	sxi32 rc;` |
|       94 | 4112 | `	const char *zErrClass = (bArgUnpack \|\| (pBad->iFlags & MEMOBJ_OBJ)) ? "TypeError" : "Error";` |
|        - | 4113 | `	char zNameBuf[64];` |
|       94 | 4114 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       94 | 4115 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       94 | 4116 | `	if( pClass == 0 ){` |
|      ! 0 | 4117 | `		return PH7_ABORT;` |
|        - | 4118 | `	}` |
|       94 | 4119 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       94 | 4120 | `	if( pThis == 0 ){` |
|      ! 0 | 4121 | `		return PH7_ABORT;` |
|        - | 4122 | `	}` |
|       94 | 4123 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       94 | 4124 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       94 | 4125 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       94 | 4126 | `	if( pCons ){` |
|       94 | 4127 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       94 | 4128 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       94 | 4129 | `		apArg[0] = &sArg;` |
|       94 | 4130 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       94 | 4131 | `		PH7_MemObjRelease(&sArg);` |
|       45 | 4132 | `	}` |
|       94 | 4133 | `	SyBlobRelease(&sMsg);` |
|       94 | 4134 | `	pFrame = pVm->pFrame;` |
|       94 | 4135 | `	if( pFrame ){` |
|       94 | 4136 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       94 | 4137 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       45 | 4138 | `	}` |
|       94 | 4139 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       94 | 4140 | `	PH7_ClassInstanceUnref(pThis);` |
|       94 | 4141 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4142 | `		return PH7_ABORT;` |
|        - | 4143 | `	}` |
|       94 | 4144 | `	return PH7_EXCEPTION;` |
|       49 | 4145 | `}` |
|        - | 4146 | `/*` |
|        - | 4147 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - | 4148 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - | 4149 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - | 4150 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - | 4151 | ` */` |
|        - | 4152 | `/*` |
|        - | 4153 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|        - | 4154 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|        - | 4155 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|        - | 4156 | ` * type field.` |
|        - | 4157 | ` */` |
|   858716 | 4158 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|        5 | 4159 | `{` |
|   858721 | 4160 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|        5 | 4161 | `}` |
|    25263 | 4162 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 4163 | `{` |
|    25268 | 4164 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    25268 | 4165 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|        - | 4166 | `	const char *zGiven;` |
|        - | 4167 | `	ph7_class *pHintScope;` |
|        - | 4168 | `	char zBuf[128];` |
|        - | 4169 | `	char zTypeBuf[128];` |
|        - | 4170 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    25268 | 4171 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|      ! 0 | 4172 | `		return SXRET_OK;` |
|        - | 4173 | `	}` |
|        - | 4174 | `	/* never return type: the function must not return at all. An explicit` |
|        - | 4175 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|        - | 4176 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|        - | 4177 | `	 * the call site). */` |
|    25268 | 4178 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        6 | 4179 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|        - | 4180 | `	}` |
|        - | 4181 | `	/* void return type: the function must not produce a value. */` |
|    25264 | 4182 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     4827 | 4183 | `		if( pValue == 0 ){` |
|     4823 | 4184 | `			return SXRET_OK;` |
|        - | 4185 | `		}` |
|        - | 4186 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|        - | 4187 | `		 * store, not a return: php compiles no return statement there at all,` |
|        - | 4188 | `		 * and still reports the hook's return type as void. PHL carries the` |
|        - | 4189 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|        - | 4190 | `		 * so the one implicit value this arm must not reject is that one. */` |
|        6 | 4191 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|        6 | 4192 | `			return SXRET_OK;` |
|        - | 4193 | `		}` |
|        - | 4194 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - | 4195 | `		 * still counts as "returned a value" here. */` |
|      ! 0 | 4196 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 | 4197 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|        - | 4198 | `	}` |
|        - | 4199 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|        - | 4200 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|        - | 4201 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    20442 | 4202 | `	if( pValue == 0 ){` |
|       33 | 4203 | `		const char *zExpected = "value";` |
|       33 | 4204 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       48 | 4205 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|       15 | 4206 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|       15 | 4207 | `		}` |
|        - | 4208 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|        - | 4209 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|       33 | 4210 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|        - | 4211 | `	}` |
|        - | 4212 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - | 4213 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - | 4214 | `	 * matching how every other typed return reports a missing value.) */` |
|    20412 | 4215 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 | 4216 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 4217 | `			return SXRET_OK;` |
|        - | 4218 | `		}` |
|        4 | 4219 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|        1 | 4220 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4221 | `	}` |
|        - | 4222 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|        - | 4223 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|        - | 4224 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    20408 | 4225 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|       43 | 4226 | `		return SXRET_OK;` |
|        - | 4227 | `	}` |
|        - | 4228 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - | 4229 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - | 4230 | `	 * Check by value before the real-class instanceof branch below. */` |
|    20370 | 4231 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      823 | 4232 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|      823 | 4233 | `		if( rcPseudo == 1 ){` |
|      171 | 4234 | `			return SXRET_OK;` |
|        - | 4235 | `		}` |
|      657 | 4236 | `		if( rcPseudo == 0 ){` |
|       19 | 4237 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 4238 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|        4 | 4239 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4240 | `		}` |
|        - | 4241 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      322 | 4242 | `	}` |
|        - | 4243 | `	/* The two branches below are the only ones that can name a class, so the` |
|        - | 4244 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|        - | 4245 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|        - | 4246 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|        - | 4247 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|        - | 4248 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|        - | 4249 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    20196 | 4250 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|        - | 4251 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|        - | 4252 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|        - | 4253 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    20196 | 4254 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - | 4255 | `		sxi32 rcU;` |
|     2816 | 4256 | `		const char *zExpected = "union";` |
|     2816 | 4257 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|     2816 | 4258 | `		if( rcU == SXRET_OK ){` |
|     2806 | 4259 | `			return SXRET_OK;` |
|        - | 4260 | `		}` |
|       11 | 4261 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        9 | 4262 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|        7 | 4263 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4264 | `			zGiven = "null";` |
|      ! 0 | 4265 | `		}else{` |
|        3 | 4266 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 4267 | `		}` |
|       11 | 4268 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       16 | 4269 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|        5 | 4270 | `				zTypeBuf,sizeof(zTypeBuf));` |
|        5 | 4271 | `		}` |
|       11 | 4272 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|        - | 4273 | `	}` |
|        - | 4274 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - | 4275 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - | 4276 | `	 * it into the TypeError message. */` |
|    17385 | 4277 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      649 | 4278 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      649 | 4279 | `		ph7_class *pExpected = 0;` |
|      649 | 4280 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|       37 | 4281 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       30 | 4282 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       17 | 4283 | `			}else{` |
|        8 | 4284 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 4285 | `			}` |
|       53 | 4286 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       16 | 4287 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|        - | 4288 | `		}` |
|      617 | 4289 | `		return SXRET_OK;` |
|        - | 4290 | `	}` |
|        - | 4291 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|        - | 4292 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|        - | 4293 | `	 * non-nullable scalar return — a TypeError. */` |
|    16741 | 4294 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       26 | 4295 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 4296 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 4297 | `			"null");` |
|        - | 4298 | `	}` |
|        - | 4299 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|        - | 4300 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    16725 | 4301 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    16603 | 4302 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    16603 | 4303 | `		return SXRET_OK;` |
|        - | 4304 | `	}` |
|        - | 4305 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|        - | 4306 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|        - | 4307 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|        - | 4308 | `	 * still rejects the object under strict_types. */` |
|      127 | 4309 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       21 | 4310 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       30 | 4311 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       18 | 4312 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       19 | 4313 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       28 | 4314 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        9 | 4315 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        9 | 4316 | `				zGiven);` |
|        - | 4317 | `		}` |
|        1 | 4318 | `	}` |
|        - | 4319 | `	/* Array <-> scalar is never compatible. */` |
|      109 | 4320 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       32 | 4321 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       10 | 4322 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 4323 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4324 | `	}` |
|        - | 4325 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - | 4326 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - | 4327 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - | 4328 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       84 | 4329 | `	if( !bStrict` |
|       83 | 4330 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       49 | 4331 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       54 | 4332 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       11 | 4333 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        3 | 4334 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 4335 | `			"string");` |
|        - | 4336 | `	}` |
|       82 | 4337 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       79 | 4338 | `		return SXRET_OK;` |
|        - | 4339 | `	}` |
|        4 | 4340 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        1 | 4341 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 4342 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    12410 | 4343 | `}` |
|        - | 4344 | `/*` |
|        - | 4345 | ` * Report a fatal named-argument error.` |
|        - | 4346 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - | 4347 | ` */` |
|       24 | 4348 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        4 | 4349 | `{` |
|        - | 4350 | `	SyBlob sMsg;` |
|        - | 4351 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|        - | 4352 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|        - | 4353 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|        - | 4354 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|        - | 4355 | `	 * unconditional fatal even inside try/catch. */` |
|       28 | 4356 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       28 | 4357 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|       28 | 4358 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        4 | 4359 | `}` |
|        - | 4360 | `/*` |
|        - | 4361 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 4362 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 4363 | ` * information.` |
|        - | 4364 | ` * ------------------------------------` |
|        - | 4365 | ` * Simple boring wrapper function.` |
|        - | 4366 | ` * ------------------------------------` |
|        - | 4367 | ` */` |
|    32122 | 4368 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        5 | 4369 | `{` |
|        - | 4370 | `	sxi32 rc;` |
|    32127 | 4371 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|    32127 | 4372 | `	return rc;` |
|        5 | 4373 | `}` |
|        - | 4374 | `/*` |
|        - | 4375 | ` * Resolve function context from the current frame.` |
|        - | 4376 | ` */` |
|        - | 4377 | `/*` |
|        - | 4378 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|        - | 4379 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|        - | 4380 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|        - | 4381 | ` * straight at the function's own name otherwise.` |
|        - | 4382 | ` */` |
|   646397 | 4383 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|        5 | 4384 | `{` |
|   646402 | 4385 | `	const char *zName = pFunc->sName.zString;` |
|   646402 | 4386 | `	int nName = (int)pFunc->sName.nByte;` |
|   697399 | 4387 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   700932 | 4388 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|        - | 4389 | `	/* A property hook is not a method in php and never shows the name PHL` |
|        - | 4390 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|        - | 4391 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|        - | 4392 | `	 * report from inside one, and what makes the trace line read` |
|        - | 4393 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|        - | 4394 | ``	 * `C::$p::get` for free. */`` |
|        - | 4395 | `	{` |
|        - | 4396 | `		SyString sProp;` |
|        - | 4397 | `		const char *zKind;` |
|   646402 | 4398 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|       43 | 4399 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       13 | 4400 | `				"$%z::%s",&sProp,zKind);` |
|       30 | 4401 | `			*pzOut = pVm->zDisplayName;` |
|       30 | 4402 | `			return n;` |
|        - | 4403 | `		}` |
|        - | 4404 | `	}` |
|   646376 | 4405 | `	if( bClosure ){` |
|        - | 4406 | `		int n;` |
|     7251 | 4407 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|        - | 4408 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|        - | 4409 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|     7251 | 4410 | `			*pzOut = pFunc->sClosureName.zString;` |
|     7251 | 4411 | `			return (int)pFunc->sClosureName.nByte;` |
|        - | 4412 | `		}` |
|      ! 0 | 4413 | `		if( pFunc->sFile.nByte > 0 ){` |
|      ! 0 | 4414 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      ! 0 | 4415 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|      ! 0 | 4416 | `		}else{` |
|      ! 0 | 4417 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|        - | 4418 | `		}` |
|      ! 0 | 4419 | `		*pzOut = pVm->zDisplayName;` |
|      ! 0 | 4420 | `		return n;` |
|        - | 4421 | `	}` |
|   639130 | 4422 | `	*pzOut = zName;` |
|   639130 | 4423 | `	return nName;` |
|   323151 | 4424 | `}` |
|        - | 4425 | `/*` |
|        - | 4426 | `` * php's name for the ACTIVE function -- the one its `name(): ` diagnostic`` |
|        - | 4427 | ` * qualifier prints. A method is rendered with its declaring class ("W::go"),` |
|        - | 4428 | `` * a closure with php's `{closure:file:line}`, and the global scope with php's`` |
|        - | 4429 | ` * own "main". VmGetFrameContext answers the same question in the shape the` |
|        - | 4430 | ` * uncaught-exception reporter wants (a bare display name, nothing at global` |
|        - | 4431 | ` * scope); this one is for a diagnostic raised from INSIDE an internal` |
|        - | 4432 | ` * function on the caller's behalf, which is how php attributes libxml's` |
|        - | 4433 | `` * errors -- `$el->nodeValue = 'a&b'` warns under the caller's name, not under`` |
|        - | 4434 | ` * the accessor's.` |
|        - | 4435 | ` */` |
|       84 | 4436 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut)` |
|        2 | 4437 | `{` |
|       86 | 4438 | `	VmFrame *pFrame = pVm->pFrame;` |
|       86 | 4439 | `	ph7_vm_func *pFunc = 0;` |
|       86 | 4440 | `	if( pFrame ){` |
|       86 | 4441 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       86 | 4442 | `		if( pFrame->pParent ){` |
|       24 | 4443 | `			pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       11 | 4444 | `		}` |
|       42 | 4445 | `	}` |
|       86 | 4446 | `	if( pFunc ){` |
|       24 | 4447 | `		VmReturnFuncName(&(*pVm),pFunc,pOut);` |
|       13 | 4448 | `	}else{` |
|       64 | 4449 | `		SyBlobAppend(pOut,"main",sizeof("main")-1);` |
|        - | 4450 | `	}` |
|       86 | 4451 | `	SyBlobNullAppend(pOut);` |
|       86 | 4452 | `}` |
|     1176 | 4453 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 | 4454 | `{` |
|        - | 4455 | `	VmFrame *pFrame;` |
|        - | 4456 | `	ph7_vm_func *pFunc;` |
|     1180 | 4457 | `	*pzFuncName = 0;` |
|     1180 | 4458 | `	*pnFuncLen = 0;` |
|     1180 | 4459 | `	pFrame = pVm->pFrame;` |
|     1180 | 4460 | `	if( pFrame == 0 ){` |
|      ! 0 | 4461 | `		return;` |
|        - | 4462 | `	}` |
|     1180 | 4463 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1180 | 4464 | `	if( pFrame->pParent == 0 ){` |
|     1134 | 4465 | `		return;` |
|        - | 4466 | `	}` |
|       50 | 4467 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 | 4468 | `	if( pFunc == 0 ){` |
|      ! 0 | 4469 | `		return;` |
|        - | 4470 | `	}` |
|       50 | 4471 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|      592 | 4472 | `}` |
|        - | 4473 | `/*` |
|        - | 4474 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|        - | 4475 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|        - | 4476 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|        - | 4477 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|        - | 4478 | ` * built-in library and already produces php's exact byte format, which keeps` |
|        - | 4479 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|        - | 4480 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|        - | 4481 | ` * synthesize what it can.` |
|        - | 4482 | ` */` |
|      614 | 4483 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 4484 | `{` |
|        - | 4485 | `	ph7_class_method *pGetTrace;` |
|        - | 4486 | `	ph7_value sTrace;` |
|        - | 4487 | `	const char *zTmp;` |
|        - | 4488 | `	int nTmp;` |
|      618 | 4489 | `	int bDone = 0;` |
|        - | 4490 | `	int bSaved;` |
|      618 | 4491 | `	if( pThis == 0 ){` |
|      ! 0 | 4492 | `		return 0;` |
|        - | 4493 | `	}` |
|      618 | 4494 | `	if( pVm->bRenderingUncaught ){` |
|        - | 4495 | `		/* Already inside a report: do not run userland trace code again. */` |
|      ! 0 | 4496 | `		return 0;` |
|        - | 4497 | `	}` |
|      618 | 4498 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|      618 | 4499 | `	if( pGetTrace == 0 ){` |
|      ! 0 | 4500 | `		return 0;` |
|        - | 4501 | `	}` |
|      618 | 4502 | `	PH7_MemObjInit(pVm,&sTrace);` |
|        - | 4503 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|        - | 4504 | `	 * anything it calls) throws, the throw would be reported by this very` |
|        - | 4505 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|        - | 4506 | `	 * trace rather than re-entering here forever. */` |
|      618 | 4507 | `	bSaved = pVm->bRenderingUncaught;` |
|      618 | 4508 | `	pVm->bRenderingUncaught = 1;` |
|      618 | 4509 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|      618 | 4510 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|      618 | 4511 | `		if( zTmp && nTmp > 0 ){` |
|      618 | 4512 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      618 | 4513 | `			bDone = 1;` |
|      307 | 4514 | `		}` |
|      307 | 4515 | `	}` |
|      618 | 4516 | `	PH7_MemObjRelease(&sTrace);` |
|      618 | 4517 | `	pVm->bRenderingUncaught = bSaved;` |
|      618 | 4518 | `	return bDone;` |
|      311 | 4519 | `}` |
|        - | 4520 | `/*` |
|        - | 4521 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|        - | 4522 | ` *` |
|        - | 4523 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|        - | 4524 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|        - | 4525 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|        - | 4526 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|        - | 4527 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|        - | 4528 | ` *             trailer.` |
|        - | 4529 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|        - | 4530 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|        - | 4531 | ` * call; this routine only appends.` |
|        - | 4532 | ` */` |
|      616 | 4533 | `static void VmRenderUncaughtEntry(` |
|        - | 4534 | `	ph7_vm *pVm,SyBlob *pOut,` |
|        - | 4535 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|        - | 4536 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|        - | 4537 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|        - | 4538 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|        - | 4539 | `	sxu32 nCallLine,   /* line of the call that entered the throwing frame (0 -> same) */` |
|        - | 4540 | `	SyString *pThrowFile) /* file the exception was raised IN (0/empty -> derive it) */` |
|        4 | 4541 | `{` |
|        - | 4542 | `	SyString *pFile;` |
|        - | 4543 | `	SyString *pCallFile;` |
|        - | 4544 | `	int bParseErr,bCompileErr;` |
|      620 | 4545 | `	if( nThrowLine == 0 ){` |
|      ! 0 | 4546 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|      ! 0 | 4547 | `	}` |
|      620 | 4548 | `	if( nCallLine == 0 ){` |
|      620 | 4549 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      620 | 4550 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|      308 | 4551 | `	}` |
|      620 | 4552 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 | 4553 | `		zClass = "Exception";` |
|      ! 0 | 4554 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 | 4555 | `	}` |
|      620 | 4556 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      572 | 4557 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      284 | 4558 | `	}` |
|        - | 4559 | ``	/* WHERE php says the exception was raised. php's `zend_exception_error` reads the`` |
|        - | 4560 | ``	 * throwable's OWN `file`/`line` properties, which are stamped where it was`` |
|        - | 4561 | `	 * constructed -- so an exception that escapes a vendor package names that` |
|        - | 4562 | `	 * package's file, however many frames it unwound through on the way out. PHL read` |
|        - | 4563 | `	 * the top of the INCLUDE stack instead, which is the entry script once anything` |
|        - | 4564 | `	 * defined elsewhere is running: every uncaught report in a composer tree -- a` |
|        - | 4565 | ``	 * `throw`, an undefined function or method, a TypeError, a DivisionByZeroError,`` |
|        - | 4566 | ``	 * each link of a `$previous` chain -- named the wrong file, in the one message a`` |
|        - | 4567 | ``	 * user reads when a program dies. (`getFile()` was already right; only the report`` |
|        - | 4568 | `	 * was not.) A caller with no instance to ask -- the internal Error reports -- has` |
|        - | 4569 | `	 * a live frame instead, so it takes the file the RUNNING code is in, the same` |
|        - | 4570 | `	 * source every other diagnostic uses (see VmDiagnosticWhere).` |
|        - | 4571 | `	 *` |
|        - | 4572 | `	 * pCallFile stays the include-stack top: the synthesized trace frame below names a` |
|        - | 4573 | `	 * CALL SITE, which is the caller's file, not the throw's. */` |
|      620 | 4574 | `	pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      620 | 4575 | `	pFile = pCallFile;` |
|      620 | 4576 | `	if( pThrowFile && pThrowFile->nByte > 0 ){` |
|      620 | 4577 | `		pFile = pThrowFile;` |
|      312 | 4578 | `	}else{` |
|      ! 0 | 4579 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|      ! 0 | 4580 | `		if( pUnit && pUnit->nByte > 0 ){` |
|      ! 0 | 4581 | `			pFile = pUnit;` |
|      ! 0 | 4582 | `		}` |
|        - | 4583 | `	}` |
|      636 | 4584 | `	bParseErr = (nClass == sizeof("ParseError")-1` |
|      616 | 4585 | `	          && SyMemcmp(zClass,"ParseError",nClass) == 0);` |
|      620 | 4586 | `	bCompileErr = (nClass == sizeof("CompileError")-1` |
|      616 | 4587 | `	            && SyMemcmp(zClass,"CompileError",nClass) == 0);` |
|      620 | 4588 | `	if( bFirst && bLast && (bParseErr \|\| bCompileErr) ){` |
|        - | 4589 | `		/* php's zend_exception_error asks for the class by IDENTITY: an uncaught` |
|        - | 4590 | `		 * ParseError is reported as the E_PARSE it stands for and an uncaught` |
|        - | 4591 | `		 * CompileError as an E_COMPILE_ERROR -- one plain line naming the file and` |
|        - | 4592 | `		 * the line, with no "Uncaught", no stack trace and no "thrown in" trailer.` |
|        - | 4593 | `		 * (A user SUBCLASS of either is an ordinary uncaught exception, which is why` |
|        - | 4594 | `		 * this is a name match and not an instanceof.) */` |
|        3 | 4595 | `		SyBlobFormat(pOut,"PHP %s:  ",bParseErr ? "Parse error" : "Fatal error");` |
|        3 | 4596 | `		if( zMsg && nMsg > 0 ){` |
|        3 | 4597 | `			SyBlobAppend(pOut,zMsg,nMsg);` |
|        1 | 4598 | `		}` |
|        3 | 4599 | `		if( pFile ){` |
|        3 | 4600 | `			SyBlobFormat(pOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|        1 | 4601 | `		}` |
|        3 | 4602 | `		return;` |
|        - | 4603 | `	}` |
|      618 | 4604 | `	if( bFirst ){` |
|      610 | 4605 | `		SyBlobAppend(pOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      307 | 4606 | `	}else{` |
|       10 | 4607 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|        - | 4608 | `	}` |
|      618 | 4609 | `	SyBlobAppend(pOut,zClass,nClass);` |
|      618 | 4610 | `	if( zMsg && nMsg > 0 ){` |
|      618 | 4611 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|      618 | 4612 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|      307 | 4613 | `	}` |
|      618 | 4614 | `	if( pFile ){` |
|      618 | 4615 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      307 | 4616 | `	}` |
|      618 | 4617 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        - | 4618 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|        - | 4619 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|        - | 4620 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|        - | 4621 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|        - | 4622 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|        - | 4623 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|        - | 4624 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|      618 | 4625 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|      ! 0 | 4626 | `		int bFrame = 0;` |
|      ! 0 | 4627 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 | 4628 | `			if( pCallFile ){` |
|        - | 4629 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|        - | 4630 | `				 * running inside it. */` |
|      ! 0 | 4631 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|      ! 0 | 4632 | `					(int)pCallFile->nByte,pCallFile->zString,nCallLine,nFuncLen,zFuncName);` |
|      ! 0 | 4633 | `			}else{` |
|      ! 0 | 4634 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|        - | 4635 | `			}` |
|      ! 0 | 4636 | `			bFrame = 1;` |
|      ! 0 | 4637 | `		}` |
|        - | 4638 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|      ! 0 | 4639 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|      ! 0 | 4640 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|      ! 0 | 4641 | `	}` |
|      618 | 4642 | `	if( bLast && pFile ){` |
|      610 | 4643 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|      610 | 4644 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      303 | 4645 | `	}` |
|      312 | 4646 | `}` |
|        - | 4647 | `/*` |
|        - | 4648 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|        - | 4649 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|        - | 4650 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|        - | 4651 | ` */` |
|      ! 0 | 4652 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|      ! 0 | 4653 | `{` |
|        - | 4654 | `	SyBlob sOut;` |
|        - | 4655 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|        - | 4656 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|      ! 0 | 4657 | `	pVm->iExitStatus = 255;` |
|      ! 0 | 4658 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 4659 | `		return PH7_OK;` |
|        - | 4660 | `	}` |
|      ! 0 | 4661 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      ! 0 | 4662 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0,0);` |
|      ! 0 | 4663 | `	VmCallErrorHandler(pVm,&sOut);` |
|      ! 0 | 4664 | `	SyBlobRelease(&sOut);` |
|      ! 0 | 4665 | `	return PH7_ABORT;` |
|      ! 0 | 4666 | `}` |
|        - | 4667 | `/*` |
|        - | 4668 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|        - | 4669 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|        - | 4670 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|        - | 4671 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|        - | 4672 | ` */` |
|        - | 4673 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|      616 | 4674 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|        4 | 4675 | `{` |
|        - | 4676 | `	ph7_value *pValue;` |
|        - | 4677 | `	ph7_class_instance *pPrev;` |
|        - | 4678 | `	ph7_class *pThrowable;` |
|      620 | 4679 | `	if( pThis == 0 ){` |
|      ! 0 | 4680 | `		return 0;` |
|        - | 4681 | `	}` |
|      620 | 4682 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      620 | 4683 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      612 | 4684 | `		return 0;` |
|        - | 4685 | `	}` |
|       10 | 4686 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 4687 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|        - | 4688 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|        - | 4689 | `	 * never renders a stray object as an exception entry. */` |
|       10 | 4690 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       10 | 4691 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|      ! 0 | 4692 | `		return 0;` |
|        - | 4693 | `	}` |
|       10 | 4694 | `	return pPrev;` |
|      312 | 4695 | `}` |
|        - | 4696 | `/*` |
|        - | 4697 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|        - | 4698 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|        - | 4699 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|        - | 4700 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|        - | 4701 | ` */` |
|       16 | 4702 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|        3 | 4703 | `{` |
|        - | 4704 | `	ph7_value *pValue;` |
|       19 | 4705 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|      ! 0 | 4706 | `		return;` |
|        - | 4707 | `	}` |
|       19 | 4708 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|       19 | 4709 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        3 | 4710 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|        - | 4711 | `	}` |
|       17 | 4712 | `	pPrev->iRef++;` |
|        - | 4713 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|        - | 4714 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|       17 | 4715 | `	PH7_MemObjRelease(pValue);` |
|       17 | 4716 | `	pValue->x.pOther = pPrev;` |
|       17 | 4717 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|       11 | 4718 | `}` |
|        - | 4719 | `/*` |
|        - | 4720 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|        - | 4721 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|        - | 4722 | ` * absent or yields an empty string.` |
|        - | 4723 | ` */` |
|        - | 4724 | `/*` |
|        - | 4725 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|        - | 4726 | ` * 0 when the class exposes no getLine().` |
|        - | 4727 | ` */` |
|      616 | 4728 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        4 | 4729 | `{` |
|        - | 4730 | `	ph7_class_method *pGetLine;` |
|        - | 4731 | `	ph7_value sLine;` |
|      620 | 4732 | `	sxu32 nLine = 0;` |
|      620 | 4733 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|      620 | 4734 | `	if( pGetLine == 0 ){` |
|      ! 0 | 4735 | `		return 0;` |
|        - | 4736 | `	}` |
|      620 | 4737 | `	PH7_MemObjInit(pVm,&sLine);` |
|      620 | 4738 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|      620 | 4739 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|      620 | 4740 | `		if( n > 0 ){` |
|      620 | 4741 | `			nLine = (sxu32)n;` |
|      308 | 4742 | `		}` |
|      308 | 4743 | `	}` |
|      620 | 4744 | `	PH7_MemObjRelease(&sLine);` |
|      620 | 4745 | `	return nLine;` |
|      312 | 4746 | `}` |
|        - | 4747 | `/*` |
|        - | 4748 | `` * The throwable's own `file` -- what php's uncaught report names. Read through`` |
|        - | 4749 | `` * `getFile()`, the way the message and the line beside it are read: the accessor is`` |
|        - | 4750 | `` * `final` in php (and refused here too), so it can only ever answer the property.`` |
|        - | 4751 | ` */` |
|      616 | 4752 | `static void VmExtractExceptionFile(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 4753 | `{` |
|        - | 4754 | `	ph7_class_method *pGetFile;` |
|        - | 4755 | `	ph7_value sFile;` |
|        - | 4756 | `	const char *zTmp;` |
|        - | 4757 | `	int nTmp;` |
|      620 | 4758 | `	pGetFile = PH7_ClassExtractMethod(pThis->pClass,"getFile",sizeof("getFile")-1);` |
|      620 | 4759 | `	if( pGetFile == 0 ){` |
|      ! 0 | 4760 | `		return;` |
|        - | 4761 | `	}` |
|      620 | 4762 | `	PH7_MemObjInit(pVm,&sFile);` |
|      620 | 4763 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetFile,&sFile,0,0) == SXRET_OK ){` |
|      620 | 4764 | `		zTmp = ph7_value_to_string(&sFile,&nTmp);` |
|      620 | 4765 | `		if( zTmp && nTmp > 0 ){` |
|      620 | 4766 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      308 | 4767 | `		}` |
|      308 | 4768 | `	}` |
|      620 | 4769 | `	PH7_MemObjRelease(&sFile);` |
|      312 | 4770 | `}` |
|      616 | 4771 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 4772 | `{` |
|        - | 4773 | `	ph7_class_method *pGetMessage;` |
|        - | 4774 | `	ph7_value sMsg;` |
|        - | 4775 | `	const char *zTmp;` |
|        - | 4776 | `	int nTmp;` |
|      620 | 4777 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      620 | 4778 | `	if( pGetMessage == 0 ){` |
|      ! 0 | 4779 | `		return;` |
|        - | 4780 | `	}` |
|      620 | 4781 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      620 | 4782 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      620 | 4783 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      620 | 4784 | `		if( zTmp && nTmp > 0 ){` |
|      620 | 4785 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      308 | 4786 | `		}` |
|      308 | 4787 | `	}` |
|      620 | 4788 | `	PH7_MemObjRelease(&sMsg);` |
|      312 | 4789 | `}` |
|        - | 4790 | `/*` |
|        - | 4791 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|        - | 4792 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|        - | 4793 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|        - | 4794 | ` * outermost (the actually-uncaught) exception.` |
|        - | 4795 | ` *` |
|        - | 4796 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|        - | 4797 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|        - | 4798 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|        - | 4799 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|        - | 4800 | ` */` |
|        - | 4801 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|      608 | 4802 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|        4 | 4803 | `{` |
|        - | 4804 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|      612 | 4805 | `	int nChain = 0;` |
|        - | 4806 | `	int i;` |
|        - | 4807 | `	SyBlob sOut;` |
|        - | 4808 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|        - | 4809 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|        - | 4810 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|      612 | 4811 | `	pVm->iExitStatus = 255;` |
|      612 | 4812 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 4813 | `		return PH7_OK;` |
|        - | 4814 | `	}` |
|        - | 4815 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|        - | 4816 | `	 * collected) or the hard cap. */` |
|     1228 | 4817 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|      630 | 4818 | `		for( i = 0 ; i < nChain ; ++i ){` |
|       12 | 4819 | `			if( apChain[i] == pThis ){` |
|      ! 0 | 4820 | `				pThis = 0; /* cycle: stop the walk */` |
|      ! 0 | 4821 | `				break;` |
|        - | 4822 | `			}` |
|        7 | 4823 | `		}` |
|      620 | 4824 | `		if( pThis == 0 ){` |
|      ! 0 | 4825 | `			break;` |
|        - | 4826 | `		}` |
|      620 | 4827 | `		apChain[nChain++] = pThis;` |
|      620 | 4828 | `		pThis = VmExceptionGetPrevious(pThis);` |
|        4 | 4829 | `	}` |
|      612 | 4830 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        - | 4831 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|        - | 4832 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|     1228 | 4833 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|      620 | 4834 | `		ph7_class_instance *pEnt = apChain[i];` |
|        - | 4835 | `		SyBlob sMsg;` |
|        - | 4836 | `		SyBlob sFile;` |
|        - | 4837 | `		SyString sThrowFile;` |
|        - | 4838 | `		sxu32 nEntLine;` |
|      620 | 4839 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      620 | 4840 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      620 | 4841 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|        - | 4842 | `		/* Each link of the chain reports its OWN file and line: php prints the` |
|        - | 4843 | `		 * deepest as "Uncaught", every outer one as "Next ...", and they routinely` |
|        - | 4844 | `		 * come from different packages. Both accessors run PHP code, so read them` |
|        - | 4845 | `		 * before the render rather than inside its argument list. */` |
|      620 | 4846 | `		VmExtractExceptionFile(pVm,pEnt,&sFile);` |
|      620 | 4847 | `		nEntLine = VmExtractExceptionLine(pVm,pEnt);` |
|      620 | 4848 | `		SyStringInitFromBuf(&sThrowFile,SyBlobData(&sFile),SyBlobLength(&sFile));` |
|      928 | 4849 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|      616 | 4850 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|      616 | 4851 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|      308 | 4852 | `			zFuncName,nFuncLen,` |
|      616 | 4853 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|      308 | 4854 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|      308 | 4855 | `			nEntLine,0,&sThrowFile);` |
|      620 | 4856 | `		SyBlobRelease(&sFile);` |
|      620 | 4857 | `		SyBlobRelease(&sMsg);` |
|      312 | 4858 | `	}` |
|      612 | 4859 | `	VmCallErrorHandler(pVm,&sOut);` |
|      612 | 4860 | `	SyBlobRelease(&sOut);` |
|      612 | 4861 | `	return PH7_ABORT;` |
|      308 | 4862 | `}` |
|        - | 4863 | `/*` |
|        - | 4864 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - | 4865 | ` *` |
|        - | 4866 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - | 4867 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - | 4868 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - | 4869 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - | 4870 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - | 4871 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - | 4872 | ` */` |
|  1565769 | 4873 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 | 4874 | `{` |
|  1565774 | 4875 | `	if( pVm->bCoalesceArmed ){` |
|       13 | 4876 | `		if( pVm->pCoalesceObj ){` |
|       13 | 4877 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        5 | 4878 | `		}` |
|       13 | 4879 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       13 | 4880 | `		pVm->pCoalesceObj = 0;` |
|       13 | 4881 | `		pVm->bCoalesceArmed = 0;` |
|        5 | 4882 | `	}` |
|  1565774 | 4883 | `}` |
|        - | 4884 | `/*` |
|        - | 4885 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - | 4886 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - | 4887 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - | 4888 | ` * build the SyBlob themselves and pass its data + length.` |
|        - | 4889 | ` *` |
|        - | 4890 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - | 4891 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - | 4892 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - | 4893 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - | 4894 | ` */` |
|   144762 | 4895 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|        - | 4896 | `	ph7_vm *pVm,` |
|        - | 4897 | `	const char *zClass,` |
|        - | 4898 | `	const char *zMsg,` |
|        - | 4899 | `	sxu32 nMsg` |
|        5 | 4900 | `){` |
|        - | 4901 | `	ph7_class *pClass;` |
|        - | 4902 | `	ph7_class_instance *pThis;` |
|        - | 4903 | `	ph7_class_method *pCons;` |
|        - | 4904 | `	VmFrame *pFrame;` |
|        - | 4905 | `	sxi32 rc;` |
|   144767 | 4906 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|   144767 | 4907 | `	if( pClass == 0 ){` |
|      ! 0 | 4908 | `		return SXERR_ABORT;` |
|        - | 4909 | `	}` |
|   144767 | 4910 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|   144767 | 4911 | `	if( pThis == 0 ){` |
|      ! 0 | 4912 | `		return SXERR_ABORT;` |
|        - | 4913 | `	}` |
|   144767 | 4914 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   144767 | 4915 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|        - | 4916 | `		ph7_value sArg;` |
|        - | 4917 | `		ph7_value *apArg[1];` |
|        - | 4918 | `		SyString sMsgStr;` |
|   144767 | 4919 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|   144767 | 4920 | `		PH7_MemObjInit(pVm,&sArg);` |
|   144767 | 4921 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|   144767 | 4922 | `		apArg[0] = &sArg;` |
|   144767 | 4923 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|   144767 | 4924 | `		PH7_MemObjRelease(&sArg);` |
|   144767 | 4925 | `		pVm->nExcCtorDepth--;` |
|    72381 | 4926 | `	}` |
|   144767 | 4927 | `	pFrame = pVm->pFrame;` |
|   144767 | 4928 | `	if( pFrame ){` |
|   144767 | 4929 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   144767 | 4930 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    72381 | 4931 | `	}` |
|   144767 | 4932 | `	rc = VmThrowException(pVm,pThis);` |
|   144767 | 4933 | `	PH7_ClassInstanceUnref(pThis);` |
|   144767 | 4934 | `	return rc;` |
|    72386 | 4935 | `}` |
|        - | 4936 | `/*` |
|        - | 4937 | ` * A native compare handler REFUSED the pair (ph7_class::xCmp wrote a class name` |
|        - | 4938 | ` * into its context, which PH7_ClassNativeCmp parked on the VM). php raises that` |
|        - | 4939 | ` * exception out of the comparison itself; PH7_MemObjCmp cannot, because it is` |
|        - | 4940 | ` * also the comparator sort(), in_array(), max() and switch drive, none of which` |
|        - | 4941 | ` * is a throw boundary. So the refusal waits here until a site that CAN route a` |
|        - | 4942 | ` * throw asks for it — the comparison opcodes and the switch arm raise it where` |
|        - | 4943 | ` * the expression's value would have landed, and the host-call boundary raises it` |
|        - | 4944 | ` * on the builtin's own context, which is where every other builtin throw is` |
|        - | 4945 | ` * reported from. Both doors clear it first: a raise that itself unwinds must not` |
|        - | 4946 | ` * leave the record standing for the next comparison to fire again.` |
|        - | 4947 | ` */` |
|  7883042 | 4948 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm)` |
|        5 | 4949 | `{` |
|  7883047 | 4950 | `	return pVm->zCmpRefusalClass != 0;` |
|        5 | 4951 | `}` |
|     6789 | 4952 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm)` |
|        5 | 4953 | `{` |
|     6794 | 4954 | `	pVm->zCmpRefusalClass = 0;` |
|     6794 | 4955 | `	pVm->zCmpRefusalMsg[0] = 0;` |
|     6794 | 4956 | `}` |
|       36 | 4957 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm)` |
|        1 | 4958 | `{` |
|       37 | 4959 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 4960 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       37 | 4961 | `	if( zClass == 0 ){` |
|      ! 0 | 4962 | `		return SXRET_OK;` |
|        - | 4963 | `	}` |
|       37 | 4964 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       37 | 4965 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       37 | 4966 | `	return VmThrowFromVm(&(*pVm),zClass,zMsg,(sxu32)SyStrlen(zMsg));` |
|       19 | 4967 | `}` |
|       16 | 4968 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx)` |
|        1 | 4969 | `{` |
|       17 | 4970 | `	ph7_vm *pVm = pCtx->pVm;` |
|       17 | 4971 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 4972 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       17 | 4973 | `	if( zClass == 0 ){` |
|      ! 0 | 4974 | `		return SXRET_OK;` |
|        - | 4975 | `	}` |
|       17 | 4976 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       17 | 4977 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       17 | 4978 | `	return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|        9 | 4979 | `}` |
|        - | 4980 | `/*` |
|        - | 4981 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|        - | 4982 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|        - | 4983 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|        - | 4984 | ` *` |
|        - | 4985 | ` *   int/float/bool/null      arithmetic proceeds` |
|        - | 4986 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|        - | 4987 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|        - | 4988 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|        - | 4989 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|        - | 4990 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|        - | 4991 | ` *   object/resource          TypeError, naming the object's CLASS` |
|        - | 4992 | ` *` |
|        - | 4993 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|        - | 4994 | ` */` |
|        - | 4995 | `/*` |
|        - | 4996 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|        - | 4997 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|        - | 4998 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|        - | 4999 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|        - | 5000 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|        - | 5001 | ` * to depth 1).` |
|        - | 5002 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|        - | 5003 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list).` |
|        - | 5004 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|        - | 5005 | ` * frame shape is file/line/function[/class/type], matching the default` |
|        - | 5006 | ` * zend.exception_ignore_args=On.` |
|        - | 5007 | ` */` |
|  1465763 | 5008 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|        5 | 5009 | `{` |
|        - | 5010 | `	SyString *pFile;` |
|        - | 5011 | `	VmFrame *pFrame;` |
|        - | 5012 | `	ph7_value *pValue;` |
|  1465768 | 5013 | `	sxi32 nDone = 0;` |
|  1465768 | 5014 | `	pValue = ph7_new_scalar(&(*pVm));` |
|  1465768 | 5015 | `	if( pValue == 0 ){` |
|      ! 0 | 5016 | `		return;` |
|        - | 5017 | `	}` |
|  1465768 | 5018 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|  1465768 | 5019 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  2107449 | 5020 | `	while( pFrame ){` |
|        - | 5021 | `		/* The include/require/eval activations started from THIS frame come first:` |
|        - | 5022 | `		 * they are still running, so they are inner to whatever called the frame.` |
|        - | 5023 | `		 * php shows each as a frame whose function is the construct's name, whose` |
|        - | 5024 | `		 * file and line are the call site, and whose single argument is the unit --` |
|        - | 5025 | `		 * except for the innermost entry of the whole trace, which php leaves` |
|        - | 5026 | `		 * argument-less. Nothing else records them: an include shares its caller's` |
|        - | 5027 | `		 * variable scope and pushes no VmFrame. */` |
|  2107449 | 5028 | `		sxu32 nInc = SySetUsed(&pVm->aIncFrame);` |
|  2107449 | 5029 | `		if( (iOptions & 4) != 0 && nInc > 0 && nDone == 0 ){` |
|        - | 5030 | `			/* The caller is a compile-time refusal, which php raises BEFORE it pushes` |
|        - | 5031 | `			 * the include/require/eval activation that is loading this unit -- so that` |
|        - | 5032 | `			 * one innermost activation is not on php's trace. (A class REDECLARATION` |
|        - | 5033 | `			 * is php's run-time refusal and does carry it; the caller says which.) */` |
|        5 | 5034 | `			VmIncFrame *pTop = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|        5 | 5035 | `			if( pTop && pTop->pFrame == (void *)pFrame ){` |
|        5 | 5036 | `				nInc--;` |
|        2 | 5037 | `			}` |
|        2 | 5038 | `		}` |
|  2204470 | 5039 | `		while( nInc > 0 ){` |
|    97040 | 5040 | `			VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,--nInc);` |
|        - | 5041 | `			ph7_value *pIncEntry;` |
|    97040 | 5042 | `			if( pInc == 0 \|\| pInc->pFrame != (void *)pFrame ){` |
|    36381 | 5043 | `				continue;` |
|        - | 5044 | `			}` |
|    60662 | 5045 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|       15 | 5046 | `				break;` |
|        - | 5047 | `			}` |
|    60648 | 5048 | `			pIncEntry = ph7_new_array(&(*pVm));` |
|    60648 | 5049 | `			if( pIncEntry == 0 ){` |
|      ! 0 | 5050 | `				break;` |
|        - | 5051 | `			}` |
|    60648 | 5052 | `			nDone++;` |
|    60648 | 5053 | `			if( SyStringLength(&pInc->sFile) > 0 ){` |
|    60648 | 5054 | `				ph7_value_string(pValue,pInc->sFile.zString,(int)pInc->sFile.nByte);` |
|    60648 | 5055 | `				ph7_array_add_strkey_elem(pIncEntry,"file",pValue);` |
|    60648 | 5056 | `				ph7_value_reset_string_cursor(pValue);` |
|    30321 | 5057 | `			}` |
|    60648 | 5058 | `			ph7_value_int(pValue,(int)pInc->nLine);` |
|    60648 | 5059 | `			ph7_array_add_strkey_elem(pIncEntry,"line",pValue);` |
|    60648 | 5060 | `			ph7_value_string(pValue,pInc->zName,-1);` |
|    60648 | 5061 | `			ph7_array_add_strkey_elem(pIncEntry,"function",pValue);` |
|    60648 | 5062 | `			ph7_value_reset_string_cursor(pValue);` |
|    60648 | 5063 | `			if( SyStringLength(&pInc->sPath) > 0 && ph7_array_count(pList) > 0 ){` |
|    28878 | 5064 | `				ph7_value *pArgs = ph7_new_array(&(*pVm));` |
|    28878 | 5065 | `				if( pArgs ){` |
|    28878 | 5066 | `					ph7_value *pArg = ph7_new_scalar(&(*pVm));` |
|    28878 | 5067 | `					if( pArg ){` |
|    28878 | 5068 | `						ph7_value_string(pArg,pInc->sPath.zString,(int)pInc->sPath.nByte);` |
|    28878 | 5069 | `						ph7_array_add_elem(pArgs,0,pArg);` |
|    28878 | 5070 | `						ph7_release_value(&(*pVm),pArg);` |
|    14438 | 5071 | `					}` |
|    28878 | 5072 | `					ph7_array_add_strkey_elem(pIncEntry,"args",pArgs);` |
|    28878 | 5073 | `					ph7_release_value(&(*pVm),pArgs);` |
|    14438 | 5074 | `				}` |
|    14438 | 5075 | `			}` |
|    60648 | 5076 | `			ph7_array_add_elem(pList,0,pIncEntry);` |
|    60648 | 5077 | `			ph7_release_value(&(*pVm),pIncEntry);` |
|        5 | 5078 | `		}` |
|        - | 5079 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|        - | 5080 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|        - | 5081 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|        - | 5082 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
|  2107449 | 5083 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|       39 | 5084 | `			break;` |
|        - | 5085 | `		}` |
|  2107411 | 5086 | `		nDone++;` |
|  2107411 | 5087 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        - | 5088 | `		ph7_value *pEntry;` |
|  2107411 | 5089 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|        - | 5090 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   732840 | 5091 | `			break;` |
|        - | 5092 | `		}` |
|   641686 | 5093 | `		pEntry = ph7_new_array(&(*pVm));` |
|   641686 | 5094 | `		if( pEntry == 0 ){` |
|      ! 0 | 5095 | `			break;` |
|        - | 5096 | `		}` |
|        - | 5097 | `		/* php's key order: file, line, function[, class, type][, object][, args]. */` |
|        - | 5098 | `		{` |
|        - | 5099 | `			/* The file the CALL SITE is in, captured when the frame was pushed` |
|        - | 5100 | `			 * (VmEnterFrame). Deriving it here read the caller's function through` |
|        - | 5101 | ``			 * pParent -- which is a `try` block's own function-less frame for a call`` |
|        - | 5102 | `			 * written inside one, so it fell back to the include-stack TOP and blamed` |
|        - | 5103 | `			 * the entry script -- and the include stack itself has moved on by now,` |
|        - | 5104 | `			 * so a call made by an included file's top-level code was blamed on` |
|        - | 5105 | `			 * whatever is being included at the moment of the trace. */` |
|   962579 | 5106 | `			SyString *pFrameFile = SyStringLength(&pFrame->sCallFile) > 0` |
|   641681 | 5107 | `				? &pFrame->sCallFile : pFile;` |
|   641686 | 5108 | `			if( pFrameFile ){` |
|   641686 | 5109 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   641686 | 5110 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   641686 | 5111 | `				ph7_value_reset_string_cursor(pValue);` |
|   320788 | 5112 | `			}` |
|        - | 5113 | `		}` |
|   641686 | 5114 | `		ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   641686 | 5115 | `		ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|        - | 5116 | `		{` |
|   641686 | 5117 | `			const char *zDisp = 0;` |
|   641686 | 5118 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   641686 | 5119 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|        - | 5120 | `		}` |
|   641686 | 5121 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   641686 | 5122 | `		ph7_value_reset_string_cursor(pValue);` |
|        - | 5123 | `		{` |
|        - | 5124 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|        - | 5125 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|        - | 5126 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|        - | 5127 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|        - | 5128 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|        - | 5129 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|        - | 5130 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   641686 | 5131 | `			SyString *pClsName = 0;` |
|   641686 | 5132 | `			const char *zType = "->";` |
|   641686 | 5133 | `			int bStatic = 0;` |
|   641686 | 5134 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 5135 | `				/* php's separator says what the CALLEE is, not how the caller` |
|        - | 5136 | ``				 * happened to reach it: a static method is `::` even when the`` |
|        - | 5137 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|        - | 5138 | ``				 * instance method), which the pThis test reported as `->`.`` |
|        - | 5139 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|        - | 5140 | `				 * own flags are one cast away. */` |
|   500595 | 5141 | `				ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|   500595 | 5142 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   500595 | 5143 | `				if( (pDecl->iFlags & PH7_CLASS_TRAIT) != 0 && pFrame->pSelfClass ){` |
|        - | 5144 | `					/* php COMPOSES a trait method into the using class, so a frame running` |
|        - | 5145 | `					 * one reports that class and never the trait. The class the call was` |
|        - | 5146 | `					 * made THROUGH names it -- the receiver's for an instance call, the` |
|        - | 5147 | `					 * named one for a static call -- and its ancestry is walked, so` |
|        - | 5148 | ``					 * `class Base { use T; } class Kid extends Base {}` is Base from a Kid`` |
|        - | 5149 | `					 * instance, where php composed the method. */` |
|       22 | 5150 | `					pDecl = PH7_VmTraitUsingClass(&(*pVm),pDecl,pFrame->pSelfClass);` |
|       10 | 5151 | `				}` |
|   500595 | 5152 | `				pClsName = &pDecl->sName;` |
|   500595 | 5153 | `				zType = bStatic ? "::" : "->";` |
|   391391 | 5154 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|        3 | 5155 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|        1 | 5156 | `			}` |
|   641686 | 5157 | `			if( pClsName ){` |
|   500597 | 5158 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|   500597 | 5159 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|   500597 | 5160 | `				ph7_value_reset_string_cursor(pValue);` |
|   500597 | 5161 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|   500597 | 5162 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|   500597 | 5163 | `				ph7_value_reset_string_cursor(pValue);` |
|   500592 | 5164 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis` |
|       36 | 5165 | `				 && !bStatic ){` |
|       28 | 5166 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       28 | 5167 | `					if( pObjVal ){` |
|       28 | 5168 | `						pFrame->pThis->iRef++;` |
|       28 | 5169 | `						pObjVal->x.pOther = pFrame->pThis;` |
|       28 | 5170 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       28 | 5171 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|       28 | 5172 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       13 | 5173 | `					}` |
|       13 | 5174 | `				}` |
|   250296 | 5175 | `			}` |
|        - | 5176 | `		}` |
|   641686 | 5177 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      122 | 5178 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      122 | 5179 | `			if( pArg ){` |
|        - | 5180 | `				/* The arguments the caller actually PASSED, which is not the same` |
|        - | 5181 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|        - | 5182 | `				 * (shared with func_get_args()). */` |
|      122 | 5183 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg);` |
|      122 | 5184 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      122 | 5185 | `				ph7_release_value(&(*pVm),pArg);` |
|       60 | 5186 | `			}` |
|       60 | 5187 | `		}` |
|   641686 | 5188 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   641686 | 5189 | `		ph7_release_value(&(*pVm),pEntry);` |
|   641686 | 5190 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|        5 | 5191 | `	}` |
|  1465768 | 5192 | `	ph7_release_value(&(*pVm),pValue);` |
|   732859 | 5193 | `}` |
|        - | 5194 | `/*` |
|        - | 5195 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|        - | 5196 | ` *` |
|        - | 5197 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|        - | 5198 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|        - | 5199 | ` * calls parent::__construct still reports the right position. The embedded` |
|        - | 5200 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|        - | 5201 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|        - | 5202 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|        - | 5203 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|        - | 5204 | ` */` |
|  1620607 | 5205 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        5 | 5206 | `{` |
|        - | 5207 | `	static const char *azField[] = { "file", "line", "trace" };` |
|        - | 5208 | `	ph7_class *pThrowable;` |
|        - | 5209 | `	SyString *pFile;` |
|        - | 5210 | `	SyString *pSiteFile;` |
|        - | 5211 | `	sxu32 n;` |
|  1620612 | 5212 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|      ! 0 | 5213 | `		return;` |
|        - | 5214 | `	}` |
|  1620612 | 5215 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|  1620612 | 5216 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   154993 | 5217 | `		return;` |
|        - | 5218 | `	}` |
|  1465624 | 5219 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - | 5220 | ``	/* getFile() is the file the `new` is WRITTEN in, which is the same question`` |
|        - | 5221 | `	 * __FILE__ asks and PH7_VmExecutingUnitFile is the one answer to: the running` |
|        - | 5222 | `	 * function's DEFINING file, the include-stack top for code at a loaded unit's` |
|        - | 5223 | `	 * top level, and an eval()'d chunk's own name inside one. This asked it by` |
|        - | 5224 | `	 * hand and knew only the first half, so an undefined function called at the` |
|        - | 5225 | `	 * top level of a file some METHOD included was reported in the method's file` |
|        - | 5226 | `	 * (PHPUnit's TestSuiteLoader is exactly that shape, and named itself instead` |
|        - | 5227 | `	 * of the test file it had just loaded). */` |
|  1465624 | 5228 | `	pSiteFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|  1465624 | 5229 | `	if( pSiteFile == 0 ){` |
|      ! 0 | 5230 | `		pSiteFile = pFile;` |
|      ! 0 | 5231 | `	}` |
|  5862481 | 5232 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|        - | 5233 | `		SyHashEntry *pEntry;` |
|        - | 5234 | `		VmClassAttr *pVmAttr;` |
|        - | 5235 | `		ph7_value *pAttrValue;` |
|  4396862 | 5236 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|  4396862 | 5237 | `		if( pEntry == 0 ){` |
|      ! 0 | 5238 | `			continue;` |
|        - | 5239 | `		}` |
|  4396862 | 5240 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  4396862 | 5241 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  4396862 | 5242 | `		if( pAttrValue == 0 ){` |
|      ! 0 | 5243 | `			continue;` |
|        - | 5244 | `		}` |
|        - | 5245 | ``		/* The stamp IS this slot's initialization. `Error` declares`` |
|        - | 5246 | ``		 * `protected int $line` with no default (php's stub, and php's own`` |
|        - | 5247 | `		 * Reflection agrees), so its slot starts UNINITIALIZED -- and writing` |
|        - | 5248 | `		 * it here without clearing that flag left every Error, TypeError and` |
|        - | 5249 | ``		 * ValueError reporting `uninitialized(int)` in var_dump, one property`` |
|        - | 5250 | `		 * short in the (array) cast, get_object_vars(), get_mangled_object_vars(),` |
|        - | 5251 | `		 * json_encode(), serialize() and array_walk(), while getLine() answered` |
|        - | 5252 | `		 * the real number. Exception hid it by declaring a default. */` |
|  4396862 | 5253 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|  4396862 | 5254 | `		if( n == 0 ){` |
|  1465624 | 5255 | `			if( pSiteFile ){` |
|  1465624 | 5256 | `				PH7_MemObjRelease(pAttrValue);` |
|  1465624 | 5257 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   732787 | 5258 | `			}` |
|  3664025 | 5259 | `		}else if( n == 1 ){` |
|        - | 5260 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|        - | 5261 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|        - | 5262 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|        - | 5263 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|        - | 5264 | `			 * off everything the initializer calls — an autoloader, a nested` |
|        - | 5265 | `			 * constant's evaluation — which report their own lines in both engines.` |
|        - | 5266 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|        - | 5267 | `			 * PHL already matches.) */` |
|  1465570 | 5268 | `			sxu32 nLine = (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|       40 | 5269 | `				? pVm->nLazyInitLine` |
|  1465620 | 5270 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
|  1465624 | 5271 | `			PH7_MemObjRelease(pAttrValue);` |
|  1465624 | 5272 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|   732787 | 5273 | `		}else{` |
|        - | 5274 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|        - | 5275 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|        - | 5276 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|        - | 5277 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|  1465624 | 5278 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|  1465624 | 5279 | `			if( pList == 0 ){` |
|      ! 0 | 5280 | `				continue;` |
|        - | 5281 | `			}` |
|  1465624 | 5282 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/,0,pList);` |
|        - | 5283 | `			/* Building the trace reserves new memobjs, which used to realloc` |
|        - | 5284 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO the pool,` |
|        - | 5285 | `			 * from PH7_ClassInstanceExtractAttrValue above). Redundant since P1` |
|        - | 5286 | `			 * (fixed segments); left for the harvest sweep (PERF.md P1). */` |
|  1465624 | 5287 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  1465624 | 5288 | `			if( pAttrValue ){` |
|  1465624 | 5289 | `				PH7_MemObjRelease(pAttrValue);` |
|  1465624 | 5290 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   732782 | 5291 | `			}` |
|  1465624 | 5292 | `			ph7_release_value(&(*pVm),pList);` |
|        - | 5293 | `		}` |
|  2198351 | 5294 | `	}` |
|   810141 | 5295 | `}` |
|     6900 | 5296 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|        3 | 5297 | `{` |
|     6903 | 5298 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       56 | 5299 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       56 | 5300 | `		if( pInst && pInst->pClass ){` |
|       56 | 5301 | `			return pInst->pClass->sName.zString;` |
|        - | 5302 | `		}` |
|      ! 0 | 5303 | `	}` |
|     6849 | 5304 | `	return ph7_type_name(pVal);` |
|     3453 | 5305 | `}` |
|        - | 5306 | `/*` |
|        - | 5307 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|        - | 5308 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|        - | 5309 | ` * describes an operand it could not use as one ("on false", "false given").` |
|        - | 5310 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|        - | 5311 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|        - | 5312 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|        - | 5313 | ` * instead of replacing it.` |
|        - | 5314 | ` */` |
|      128 | 5315 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|        2 | 5316 | `{` |
|      130 | 5317 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|       19 | 5318 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 5319 | `	}` |
|      112 | 5320 | `	return VmArithTypeName(&(*pVal));` |
|       66 | 5321 | `}` |
|        - | 5322 | `/*` |
|        - | 5323 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|        - | 5324 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|        - | 5325 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|        - | 5326 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|        - | 5327 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|        - | 5328 | ` */` |
|       40 | 5329 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|        1 | 5330 | `{` |
|       41 | 5331 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|       41 | 5332 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       19 | 5333 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       19 | 5334 | `		if( pInst && pInst->pClass ){` |
|       19 | 5335 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sName);` |
|       19 | 5336 | `			return;` |
|        - | 5337 | `		}` |
|      ! 0 | 5338 | `	}` |
|       23 | 5339 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|       21 | 5340 | `}` |
|        - | 5341 | `/*` |
|        - | 5342 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|        - | 5343 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|        - | 5344 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|        - | 5345 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|        - | 5346 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|        - | 5347 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|        - | 5348 | ` */` |
|      250 | 5349 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|        1 | 5350 | `{` |
|      251 | 5351 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      251 | 5352 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      251 | 5353 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      251 | 5354 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      251 | 5355 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|        - | 5356 | `	sxu32 i;` |
|      523 | 5357 | `	for( i = 0 ; i < nMin ; ++i ){` |
|        - | 5358 | `		unsigned char c;` |
|      273 | 5359 | `		if( cOp == '\|' ){` |
|       93 | 5360 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      227 | 5361 | `		}else if( cOp == '^' ){` |
|       89 | 5362 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|       45 | 5363 | `		}else{` |
|       93 | 5364 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|        - | 5365 | `		}` |
|      273 | 5366 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      137 | 5367 | `	}` |
|      251 | 5368 | `	if( cOp == '\|' && nL != nR ){` |
|       63 | 5369 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|       63 | 5370 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|       63 | 5371 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       31 | 5372 | `	}` |
|      251 | 5373 | `}` |
|        - | 5374 | `/*` |
|        - | 5375 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|        - | 5376 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|        - | 5377 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|        - | 5378 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|        - | 5379 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|        - | 5380 | ` * null, which is what an undefined variable answers.` |
|        - | 5381 | ` */` |
|       26 | 5382 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|        1 | 5383 | `{` |
|       27 | 5384 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       11 | 5385 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       11 | 5386 | `		if( pInst && pInst->pClass ){` |
|       11 | 5387 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sName);` |
|       11 | 5388 | `			return;` |
|        - | 5389 | `		}` |
|      ! 0 | 5390 | `	}` |
|       17 | 5391 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 | 5392 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|        4 | 5393 | `			pVal->x.iVal ? "true" : "false");` |
|       15 | 5394 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|       11 | 5395 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|        6 | 5396 | `	}else{` |
|        3 | 5397 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|        - | 5398 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|        - | 5399 | `	}` |
|       14 | 5400 | `}` |
|        - | 5401 | `/*` |
|        - | 5402 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|        - | 5403 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|        - | 5404 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|        - | 5405 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|        - | 5406 | ` * computes with the prefix. Classify pVal against that contract.` |
|        - | 5407 | ` */` |
|   103994 | 5408 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|        5 | 5409 | `{` |
|        - | 5410 | `	ph7_value sInt;` |
|        - | 5411 | `	sxi32 rc;` |
|   103999 | 5412 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   103999 | 5413 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   103999 | 5414 | `	PH7_MemObjRelease(&sInt);` |
|   103999 | 5415 | `	return rc;` |
|        5 | 5416 | `}` |
|        - | 5417 | `/*` |
|        - | 5418 | ` * One arithmetic operator's whole prologue: php's do_operation handler first,` |
|        - | 5419 | ` * then the ordinary operand contract.` |
|        - | 5420 | ` *` |
|        - | 5421 | ` * php asks the LEFT operand's class for a handler and falls back to the RIGHT` |
|        - | 5422 | `` * one's, which is why an `int + Number` works as well as a `Number + int`. A`` |
|        - | 5423 | ` * handler that answers writes into pDest (the slot the opcode was going to` |
|        - | 5424 | ` * leave its result in), so the caller's only job is to skip the numeric` |
|        - | 5425 | ` * arithmetic. A handler that REFUSES hands back an exception class and a` |
|        - | 5426 | ` * message, and the caller throws them where it would have thrown the TypeError` |
|        - | 5427 | ` * -- after settling the operand stack.` |
|        - | 5428 | ` */` |
|        - | 5429 | ``/* Does this value's class declare php's do_operation? `++`/`--` ask before they`` |
|        - | 5430 | ` * refuse an object, since everything below that refusal is numeric. */` |
|   905260 | 5431 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal)` |
|        5 | 5432 | `{` |
|        - | 5433 | `	ph7_class_instance *pInst;` |
|   905265 | 5434 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|   905229 | 5435 | `		return 0;` |
|        - | 5436 | `	}` |
|       37 | 5437 | `	pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       37 | 5438 | `	return pInst->pClass != 0 && pInst->pClass->xArith != 0;` |
|   453382 | 5439 | `}` |
|   586171 | 5440 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|        - | 5441 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut)` |
|        5 | 5442 | `{` |
|   586176 | 5443 | `	ph7_class_instance *pInst = 0;` |
|        - | 5444 | `	int i;` |
|   586176 | 5445 | `	*pzClass = "TypeError";` |
|  1758388 | 5446 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1172285 | 5447 | `		ph7_value *pSide = i == 0 ? pLeft : pRight;` |
|  1172285 | 5448 | `		if( (pSide->iFlags & MEMOBJ_OBJ) != 0 && pSide->x.pOther ){` |
|       78 | 5449 | `			ph7_class_instance *pCand = (ph7_class_instance *)pSide->x.pOther;` |
|       78 | 5450 | `			if( pCand->pClass && pCand->pClass->xArith ){` |
|       70 | 5451 | `				pInst = pCand;` |
|       70 | 5452 | `				break;` |
|        - | 5453 | `			}` |
|        4 | 5454 | `		}` |
|   586856 | 5455 | `	}` |
|   586176 | 5456 | `	if( pInst ){` |
|        - | 5457 | `		PH7_NativeArithCtx sCtx;` |
|        - | 5458 | `		ph7_value sRes;` |
|       70 | 5459 | `		PH7_MemObjInit(&(*pVm),&sRes);` |
|       70 | 5460 | `		sCtx.zOp = zOp;` |
|       70 | 5461 | `		sCtx.pLeft = pLeft;` |
|       70 | 5462 | `		sCtx.pRight = pRight;` |
|       70 | 5463 | `		sCtx.pResult = &sRes;` |
|       70 | 5464 | `		sCtx.bHandled = 0;` |
|       70 | 5465 | `		sCtx.zThrowClass = 0;` |
|       70 | 5466 | `		sCtx.zThrowMsg[0] = 0;` |
|       70 | 5467 | `		pInst->pClass->xArith(&(*pVm),pInst,&sCtx);` |
|       70 | 5468 | `		if( sCtx.zThrowClass ){` |
|       11 | 5469 | `			PH7_MemObjRelease(&sRes);` |
|       11 | 5470 | `			*pzClass = sCtx.zThrowClass;` |
|       11 | 5471 | `			SyBlobAppend(pMsgOut,sCtx.zThrowMsg,(sxu32)SyStrlen(sCtx.zThrowMsg));` |
|       32 | 5472 | `			return PH7_ARITH_REFUSED;` |
|        - | 5473 | `		}` |
|       60 | 5474 | `		if( sCtx.bHandled ){` |
|       44 | 5475 | `			PH7_MemObjStore(&sRes,pDest);` |
|       44 | 5476 | `			PH7_MemObjRelease(&sRes);` |
|       44 | 5477 | `			return PH7_ARITH_HANDLED;` |
|        - | 5478 | `		}` |
|       18 | 5479 | `		PH7_MemObjRelease(&sRes);` |
|        8 | 5480 | `	}` |
|   586124 | 5481 | `	if( VmArithOperandCheck(&(*pVm),pLeft,pRight,zOp,pMsgOut) != SXRET_OK ){` |
|     1890 | 5482 | `		return PH7_ARITH_REFUSED;` |
|        - | 5483 | `	}` |
|   584236 | 5484 | `	return PH7_ARITH_ORDINARY;` |
|   293463 | 5485 | `}` |
|   704035 | 5486 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|        5 | 5487 | `{` |
|   704040 | 5488 | `	int bBadL = 0, bBadR = 0;` |
|        - | 5489 | `	int i;` |
|        - | 5490 | `	ph7_value *apOperand[2];` |
|   704040 | 5491 | `	apOperand[0] = pLeft;` |
|   704040 | 5492 | `	apOperand[1] = pRight;` |
|        - | 5493 | `	/* array + array is php's union operator, not arithmetic */` |
|   704035 | 5494 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   511594 | 5495 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|     5984 | 5496 | `		return SXRET_OK;` |
|        - | 5497 | `	}` |
|  2088897 | 5498 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1394221 | 5499 | `		ph7_value *pVal = apOperand[i];` |
|  1394221 | 5500 | `		int bBad = 0;` |
|  1394221 | 5501 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     1252 | 5502 | `			bBad = 1;` |
|     1252 | 5503 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       54 | 5504 | `				ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|        - | 5505 | `				/* A class whose cast_object really answers a number is an operand` |
|        - | 5506 | ``				 * php accepts: `$xml->qty + 1` adds to the element's text. */`` |
|       54 | 5507 | `				if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        5 | 5508 | `					bBad = 0;` |
|        2 | 5509 | `				}` |
|       28 | 5510 | `			}` |
|  1393596 | 5511 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     4624 | 5512 | `			const char *zTail = 0;` |
|     4624 | 5513 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     4624 | 5514 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|        - | 5515 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     2135 | 5516 | `				bBad = 1;` |
|     1068 | 5517 | `			}else{` |
|        - | 5518 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|        - | 5519 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|        - | 5520 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     2518 | 5521 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       30 | 5522 | `					zTail++;` |
|        2 | 5523 | `				}` |
|     2490 | 5524 | `				if( zTail < zEnd ){` |
|     1187 | 5525 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      592 | 5526 | `				}` |
|        - | 5527 | `			}` |
|     2310 | 5528 | `		}` |
|  1394221 | 5529 | `		if( bBad ){` |
|     3382 | 5530 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|        - | 5531 | `			/* php converts the operands one at a time and STOPS at the first one it` |
|        - | 5532 | `			 * refuses — the second is never looked at, so it never says anything about` |
|        - | 5533 | ``			 * it. `"abc" + "5x"` is the TypeError alone, where walking both operands`` |
|        - | 5534 | ``			 * first announced `A non-numeric value encountered` for the "5x" php never`` |
|        - | 5535 | ``			 * reached. The other order is unaffected: `"5x" + "abc"` warns for the left`` |
|        - | 5536 | `			 * operand and then throws, in both engines. */` |
|     3382 | 5537 | `			break;` |
|        - | 5538 | `		}` |
|   696147 | 5539 | `	}` |
|   698061 | 5540 | `	if( bBadL \|\| bBadR ){` |
|        - | 5541 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|        - | 5542 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|        - | 5543 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     5072 | 5544 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|     1690 | 5545 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     3382 | 5546 | `		return SXERR_INVALID;` |
|        - | 5547 | `	}` |
|   694681 | 5548 | `	return SXRET_OK;` |
|   352380 | 5549 | `}` |
|        - | 5550 | `/*` |
|        - | 5551 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - | 5552 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|        - | 5553 | ` * pass 0 for the engine errors that leave it at its default.` |
|        - | 5554 | ` */` |
|    12569 | 5555 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|        5 | 5556 | `{` |
|        - | 5557 | `	ph7_vm *pVm;` |
|        - | 5558 | `	ph7_class *pClass;` |
|        - | 5559 | `	ph7_class_instance *pThis;` |
|        - | 5560 | `	ph7_class_method *pCons;` |
|        - | 5561 | `	ph7_value sArg,sCode;` |
|        - | 5562 | `	ph7_value *apArg[2];` |
|        - | 5563 | `	SyBlob sMsg;` |
|        - | 5564 | `	SyString sMsgStr;` |
|        - | 5565 | `	VmFrame *pFrame;` |
|        - | 5566 | `	sxi32 rc;` |
|        - | 5567 |  |
|    12574 | 5568 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 5569 | `		return PH7_ABORT;` |
|        - | 5570 | `	}` |
|    12574 | 5571 | `	pVm = pCtx->pVm;` |
|    12574 | 5572 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 5573 | `		zClass = "Error";` |
|      ! 0 | 5574 | `	}` |
|        - | 5575 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|        - | 5576 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|        - | 5577 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|        - | 5578 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|        - | 5579 | `	 * reported error (VmHostFuncThrowRc). */` |
|    12574 | 5580 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    12574 | 5581 | `	if( pClass == 0 ){` |
|      ! 0 | 5582 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 5583 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 | 5584 | `			zClass` |
|        - | 5585 | `			);` |
|      ! 0 | 5586 | `		return pCtx->nThrowRc;` |
|        - | 5587 | `	}` |
|    12574 | 5588 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    12574 | 5589 | `	if( pThis == 0 ){` |
|      ! 0 | 5590 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 5591 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - | 5592 | `			);` |
|      ! 0 | 5593 | `		return pCtx->nThrowRc;` |
|        - | 5594 | `	}` |
|        - | 5595 |  |
|    12574 | 5596 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    12574 | 5597 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - | 5598 |  |
|    12574 | 5599 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    12574 | 5600 | `	if( pCons ){` |
|    12574 | 5601 | `		int nArg = 1;` |
|    12574 | 5602 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    12574 | 5603 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    12574 | 5604 | `		apArg[0] = &sArg;` |
|    12574 | 5605 | `		if( iCode != 0 ){` |
|      418 | 5606 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      418 | 5607 | `			apArg[1] = &sCode;` |
|      418 | 5608 | `			nArg = 2;` |
|      208 | 5609 | `		}` |
|    12574 | 5610 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    12574 | 5611 | `		if( iCode != 0 ){` |
|      418 | 5612 | `			PH7_MemObjRelease(&sCode);` |
|      208 | 5613 | `		}` |
|    12574 | 5614 | `		PH7_MemObjRelease(&sArg);` |
|     6260 | 5615 | `	}` |
|    12574 | 5616 | `	SyBlobRelease(&sMsg);` |
|        - | 5617 |  |
|    12574 | 5618 | `	pFrame = pVm->pFrame;` |
|    12574 | 5619 | `	if( pFrame ){` |
|    12574 | 5620 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    12574 | 5621 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     6260 | 5622 | `	}` |
|    12574 | 5623 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    12574 | 5624 | `	PH7_ClassInstanceUnref(pThis);` |
|    12574 | 5625 | `	if( rc == SXERR_ABORT ){` |
|      535 | 5626 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      535 | 5627 | `		return PH7_ABORT;` |
|        - | 5628 | `	}` |
|        - | 5629 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|        - | 5630 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|        - | 5631 | `	 * argument-validation helper whose callers have no status channel — would` |
|        - | 5632 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|        - | 5633 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|        - | 5634 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|        - | 5635 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|        - | 5636 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|        - | 5637 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    12044 | 5638 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    12044 | 5639 | `	return PH7_EXCEPTION;` |
|     6265 | 5640 | `}` |
|    12147 | 5641 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 | 5642 | `{` |
|        - | 5643 | `	va_list ap;` |
|        - | 5644 | `	sxi32 rc;` |
|    12152 | 5645 | `	va_start(ap,zFormat);` |
|    12152 | 5646 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    12152 | 5647 | `	va_end(ap);` |
|    12152 | 5648 | `	return rc;` |
|        5 | 5649 | `}` |
|        - | 5650 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      422 | 5651 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|        2 | 5652 | `{` |
|        - | 5653 | `	va_list ap;` |
|        - | 5654 | `	sxi32 rc;` |
|      424 | 5655 | `	va_start(ap,zFormat);` |
|      424 | 5656 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      424 | 5657 | `	va_end(ap);` |
|      424 | 5658 | `	return rc;` |
|        2 | 5659 | `}` |
|        - | 5660 | `/*` |
|        - | 5661 | ` * The status a host function's own throw should have returned. Consulted at the` |
|        - | 5662 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|        - | 5663 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|        - | 5664 | ` * and is the identity when the routine never threw or already reported it.` |
|        - | 5665 | ` *` |
|        - | 5666 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|        - | 5667 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|        - | 5668 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|        - | 5669 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|        - | 5670 | ` */` |
|  6459236 | 5671 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|        5 | 5672 | `{` |
|  6459236 | 5673 | `	if( pCtx->nThrowRc == 0` |
|  3234889 | 5674 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
|  6454641 | 5675 | `		return rc;` |
|        - | 5676 | `	}` |
|     4605 | 5677 | `	return pCtx->nThrowRc;` |
|  3229519 | 5678 | `}` |
|        - | 5679 | `/*` |
|        - | 5680 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - | 5681 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - | 5682 | ` */` |
|      ! 0 | 5683 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 | 5684 | `{` |
|        - | 5685 | `	ph7_vm *pVm;` |
|        - | 5686 | `	SyBlob sMsg;` |
|      ! 0 | 5687 | `	const char *zFuncName = 0;` |
|      ! 0 | 5688 | `	int nFuncLen = 0;` |
|        - | 5689 | `	va_list ap;` |
|        - | 5690 | `	sxi32 rc;` |
|        - | 5691 |  |
|      ! 0 | 5692 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 5693 | `		return PH7_OK;` |
|        - | 5694 | `	}` |
|      ! 0 | 5695 | `	pVm = pCtx->pVm;` |
|      ! 0 | 5696 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 5697 | `		zClass = "Error";` |
|      ! 0 | 5698 | `	}` |
|        - | 5699 |  |
|      ! 0 | 5700 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 5701 |  |
|      ! 0 | 5702 | `	va_start(ap,zFormat);` |
|      ! 0 | 5703 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 | 5704 | `	va_end(ap);` |
|        - | 5705 |  |
|      ! 0 | 5706 | `	if( pCtx->pFunc ){` |
|      ! 0 | 5707 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 | 5708 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 | 5709 | `	}` |
|      ! 0 | 5710 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 | 5711 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 | 5712 | `	}` |
|      ! 0 | 5713 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 | 5714 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 | 5715 | `	SyBlobRelease(&sMsg);` |
|      ! 0 | 5716 | `	return rc;` |
|      ! 0 | 5717 | `}` |
|        - | 5718 | `/*` |
|        - | 5719 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 5720 | ` * exception is triggered.` |
|        - | 5721 | ` */` |
|      610 | 5722 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|        - | 5723 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 5724 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 5725 | `	)` |
|        4 | 5726 | `{` |
|        - | 5727 | `	ph7_value *apArg[2],sArg;` |
|      614 | 5728 | `	int nArg = 1;` |
|        - | 5729 | `	sxi32 rc;` |
|      614 | 5730 | `	if( pVm->nMuteThrow > 0 ){` |
|        - | 5731 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|        - | 5732 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|        - | 5733 | `		 * observable: no exception handler runs, no report is printed and the` |
|        - | 5734 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|        - | 5735 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|      ! 0 | 5736 | `		return SXERR_ABORT;` |
|        - | 5737 | `	}` |
|      614 | 5738 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 5739 | `		/* Nesting limit reached */` |
|      ! 0 | 5740 | `		return SXRET_OK;` |
|        - | 5741 | `	}` |
|        - | 5742 | `	/* Call any exception handler if available */` |
|      614 | 5743 | `	PH7_MemObjInit(pVm,&sArg);` |
|      614 | 5744 | `	if( pThis ){` |
|        - | 5745 | `		/* Load the exception instance */` |
|      614 | 5746 | `		sArg.x.pOther = pThis;` |
|      614 | 5747 | `		pThis->iRef++;` |
|      614 | 5748 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      309 | 5749 | `	}else{` |
|      ! 0 | 5750 | `		nArg = 0;` |
|        - | 5751 | `	}` |
|      614 | 5752 | `	apArg[0] = &sArg;` |
|        - | 5753 | `	/* Call the exception handler if available */` |
|      614 | 5754 | `	pVm->nExceptDepth++;` |
|        - | 5755 | `	{` |
|        - | 5756 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|        - | 5757 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|        - | 5758 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|        - | 5759 | `		ph7_value sRunning;` |
|      614 | 5760 | `		PH7_MemObjInit(pVm,&sRunning);` |
|      614 | 5761 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|      614 | 5762 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|      614 | 5763 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|      614 | 5764 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|      614 | 5765 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|      614 | 5766 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|      305 | 5767 | `		}` |
|      614 | 5768 | `		PH7_MemObjRelease(&sRunning);` |
|        - | 5769 | `	}` |
|      614 | 5770 | `	pVm->nExceptDepth--;` |
|      614 | 5771 | `	if( rc != SXRET_OK ){` |
|        - | 5772 | `		const char *zFuncName;` |
|        - | 5773 | `		int nFuncLen;` |
|      612 | 5774 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        - | 5775 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|      612 | 5776 | `		if( pThis ){` |
|        - | 5777 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|        - | 5778 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|        - | 5779 | `			 * renders byte-identically to the historical single-entry report. */` |
|      612 | 5780 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|      308 | 5781 | `		}else{` |
|        - | 5782 | `			/* No instance (internal report path) — default-class single entry. */` |
|      ! 0 | 5783 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|        - | 5784 | `		}` |
|        - | 5785 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      612 | 5786 | `		rc = SXERR_ABORT;` |
|      304 | 5787 | `	}` |
|      614 | 5788 | `	PH7_MemObjRelease(&sArg);` |
|      614 | 5789 | `	return rc;` |
|      309 | 5790 | `}` |
|        - | 5791 | `/*` |
|        - | 5792 | ` * Throw a user exception.` |
|        - | 5793 | ` *` |
|        - | 5794 | ` * Exception dispatch follows this sequence:` |
|        - | 5795 | ` *` |
|        - | 5796 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 5797 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 5798 | ` *` |
|        - | 5799 | ` * 2. If NO catch matches:` |
|        - | 5800 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 5801 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 5802 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 5803 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 5804 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 5805 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 5806 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 5807 | ` *` |
|        - | 5808 | ` * 3. If a catch DOES match:` |
|        - | 5809 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 5810 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 5811 | ` *       inside the catch body from immediately propagating past our` |
|        - | 5812 | ` *       finally block.` |
|        - | 5813 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 5814 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 5815 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 5816 | ` *       in pPendingException (step 2c).` |
|        - | 5817 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 5818 | ` *    d. Run finally (if present).` |
|        - | 5819 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 5820 | ` *       that handlers are restored and finally has run.` |
|        - | 5821 | ` */` |
|        - | 5822 | `/*` |
|        - | 5823 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|        - | 5824 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|        - | 5825 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|        - | 5826 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|        - | 5827 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|        - | 5828 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|        - | 5829 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|        - | 5830 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|        - | 5831 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|        - | 5832 | ` */` |
|      318 | 5833 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|        5 | 5834 | `{` |
|      345 | 5835 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|       87 | 5836 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       87 | 5837 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|       87 | 5838 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|       14 | 5839 | `			break; /* reached an outer exec's / legacy handler */` |
|        - | 5840 | `		}` |
|       61 | 5841 | `		(void)SySetPop(&pVm->aException);` |
|       61 | 5842 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|       61 | 5843 | `		if( pT->iHasFinally ){` |
|       37 | 5844 | `			*pPc = pT->iFinallyPc;` |
|       37 | 5845 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|       37 | 5846 | `			return 1;` |
|        - | 5847 | `		}` |
|        - | 5848 | `		/* No finally: tear the try's transparent frame down now. */` |
|       27 | 5849 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        5 | 5850 | `			VmLeaveFrame(&(*pVm));` |
|        2 | 5851 | `		}` |
|       27 | 5852 | `		VmExcRelease(&(*pVm),pT);` |
|        5 | 5853 | `	}` |
|      289 | 5854 | `	return 0;` |
|      164 | 5855 | `}` |
|        - | 5856 | `/*` |
|        - | 5857 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|        - | 5858 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|        - | 5859 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|        - | 5860 | ` *` |
|        - | 5861 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|        - | 5862 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|        - | 5863 | ` *    and redirect to the catch body (iHandlerPc).` |
|        - | 5864 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|        - | 5865 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|        - | 5866 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|        - | 5867 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|        - | 5868 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|        - | 5869 | ` */` |
|        - | 5870 | `/*` |
|        - | 5871 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|        - | 5872 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|        - | 5873 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|        - | 5874 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|        - | 5875 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|        - | 5876 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|        - | 5877 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|        - | 5878 | ` * case) is unchanged: no wrapper.` |
|        - | 5879 | ` */` |
|    20256 | 5880 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|        5 | 5881 | `{` |
|    20261 | 5882 | `	VmFrame *pWrap = 0;` |
|        - | 5883 | `	VmFrame *pThrowSite;` |
|        - | 5884 | `	sxi32 rc;` |
|    20261 | 5885 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    20157 | 5886 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5887 | `	}` |
|      107 | 5888 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|        - | 5889 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|      ! 0 | 5890 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5891 | `	}` |
|      107 | 5892 | `	pThrowSite = pWrap->pParent;` |
|      107 | 5893 | `	pWrap->pParent = pOwner;` |
|      107 | 5894 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|      107 | 5895 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 5896 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|        - | 5897 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|        - | 5898 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|        - | 5899 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|      107 | 5900 | `	if( pVm->pFrame == pWrap ){` |
|      107 | 5901 | `		VmLeaveFrame(&(*pVm));` |
|       52 | 5902 | `	}` |
|      107 | 5903 | `	pVm->pFrame = pThrowSite;` |
|      107 | 5904 | `	return rc;` |
|    10133 | 5905 | `}` |
|        - | 5906 | `/*` |
|        - | 5907 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|        - | 5908 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|        - | 5909 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|        - | 5910 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|        - | 5911 | ` */` |
|        - | 5912 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      102 | 5913 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - | 5914 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|        5 | 5915 | `{` |
|      107 | 5916 | `	if( pCatch ){` |
|       87 | 5917 | `		pException->iInCatch = 1;` |
|       87 | 5918 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|       87 | 5919 | `		if( pThis ){ pThis->iRef++; }` |
|       87 | 5920 | `		pException->pInflight = pThis;` |
|       87 | 5921 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       87 | 5922 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       87 | 5923 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|       87 | 5924 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       87 | 5925 | `		return SXRET_OK;` |
|        - | 5926 | `	}` |
|       24 | 5927 | `	if( pException->iHasFinally ){` |
|        - | 5928 | `		VmFinallyAction sAct;` |
|       19 | 5929 | `		SyZero(&sAct,sizeof(sAct));` |
|       19 | 5930 | `		sAct.eKind = PH7_FA_RETHROW;` |
|       19 | 5931 | `		if( pThis ){ pThis->iRef++; }` |
|       19 | 5932 | `		sAct.pExc = pThis;` |
|       19 | 5933 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       19 | 5934 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       19 | 5935 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       19 | 5936 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|       19 | 5937 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       19 | 5938 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       19 | 5939 | `		return SXRET_OK;` |
|        - | 5940 | `	}` |
|        - | 5941 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|        - | 5942 | `	 * flat native stack instead of mutual recursion. */` |
|        6 | 5943 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      ! 0 | 5944 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 5945 | `	}` |
|        6 | 5946 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|        6 | 5947 | `	return VM_THROW_KEEP_UNWINDING;` |
|       56 | 5948 | `}` |
|  1465497 | 5949 | `PH7_PRIVATE sxi32 VmThrowException(` |
|        - | 5950 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 5951 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 5952 | `	)` |
|        5 | 5953 | `{` |
|        - | 5954 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 5955 | `	ph7_exception **apException;` |
|   732721 | 5956 | `	ph7_exception *pException;` |
|    50110 | 5957 | `Rethrow:` |
|        - | 5958 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|        - | 5959 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|        - | 5960 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|        - | 5961 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|        - | 5962 | `	 * so the throw path must be too). */` |
|        - | 5963 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 5964 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 5965 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|  1565722 | 5966 | `	VmCoalesceDisarm(pVm);` |
|        - | 5967 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|        - | 5968 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|        - | 5969 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|        - | 5970 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|        - | 5971 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|        - | 5972 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|        - | 5973 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|        - | 5974 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|  1565717 | 5975 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|       25 | 5976 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|       19 | 5977 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|        8 | 5978 | `	}` |
|        - | 5979 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|        - | 5980 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|        - | 5981 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|        - | 5982 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|        - | 5983 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|        - | 5984 | `	 * that owns the pending return, so it must leave that return intact. */` |
|        - | 5985 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|        - | 5986 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|        - | 5987 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|        - | 5988 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|  1565722 | 5989 | `	VmClearResumeTarget(&(*pVm));` |
|        - | 5990 | `	/* Point to the stack of loaded exceptions */` |
|  1565722 | 5991 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|  1565722 | 5992 | `	pException = 0;` |
|  1565722 | 5993 | `	pCatch = 0;` |
|  1565722 | 5994 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 5995 | `		ph7_exception_block *aCatch;` |
|        - | 5996 | `		ph7_class *pClass;` |
|        - | 5997 | `		SyString *aNames;` |
|        - | 5998 | `		sxu32 nNames;` |
|        - | 5999 | `		int matched;` |
|        - | 6000 | `		sxu32 j,k;` |
|        - | 6001 | `		/* Locate the appropriate block to execute */` |
|  1464984 | 6002 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|  1464984 | 6003 | `		(void)SySetPop(&pVm->aException);` |
|  1464984 | 6004 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|        - | 6005 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|        - | 6006 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|        - | 6007 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|  1465010 | 6008 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 6009 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|  1444806 | 6010 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|  1444806 | 6011 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|  1444806 | 6012 | `			matched = 0;` |
|  1444858 | 6013 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 6014 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 6015 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 6016 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|  1444832 | 6017 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|  1444832 | 6018 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 6019 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 6020 | `					continue;` |
|        - | 6021 | `				}` |
|  1444832 | 6022 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|  1444780 | 6023 | `					matched = 1;` |
|  1444780 | 6024 | `					break;` |
|        - | 6025 | `				}` |
|       30 | 6026 | `			}` |
|  1444806 | 6027 | `			if( matched ){` |
|        - | 6028 | `				/* Catch block found,break immediately */` |
|  1444780 | 6029 | `				pCatch = &aCatch[j];` |
|  1444780 | 6030 | `				break;` |
|        - | 6031 | `			}` |
|       16 | 6032 | `		}` |
|   732462 | 6033 | `	}` |
|        - | 6034 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|        - | 6035 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|        - | 6036 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|        - | 6037 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|        - | 6038 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|        - | 6039 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|        - | 6040 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|  1565722 | 6041 | `	if( pException ){` |
|  1464984 | 6042 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   732462 | 6043 | `	}` |
|        - | 6044 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|        - | 6045 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|        - | 6046 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|  1565722 | 6047 | `	if( pException && pException->iInlined ){` |
|      107 | 6048 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      107 | 6049 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|        - | 6050 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|        6 | 6051 | `			goto Rethrow;` |
|        - | 6052 | `		}` |
|      103 | 6053 | `		return rcInline;` |
|        - | 6054 | `	}` |
|        - | 6055 | `	/* Execute the cached block if available */` |
|  1565620 | 6056 | `	if( pCatch == 0 ){` |
|        - | 6057 | `		sxi32 rc;` |
|        - | 6058 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|   120927 | 6059 | `		if( pException && pException->iHasFinally ){` |
|    20176 | 6060 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    20176 | 6061 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    20176 | 6062 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|    20176 | 6063 | `			pException->iFinallyDone = 1;` |
|        - | 6064 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|        - | 6065 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    20176 | 6066 | `			pVm->pInflightException = pThis;` |
|    20176 | 6067 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 6068 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|        - | 6069 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    20176 | 6070 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    20176 | 6071 | `			pVm->pInflightException = pSaveInflight;` |
|    20176 | 6072 | `			pVm->nInflightExcBase = nSaveBase;` |
|    20176 | 6073 | `			if( rc == SXERR_ABORT ){` |
|        3 | 6074 | `				VmExcRelease(&(*pVm),pException);` |
|        3 | 6075 | `				return SXERR_ABORT;` |
|        - | 6076 | `			}` |
|        - | 6077 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|        - | 6078 | `			 * semantics). The finally stored it on the body frame it returns from` |
|        - | 6079 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|        - | 6080 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|        - | 6081 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|        - | 6082 | `			 * takes the value instead of unwinding) and resume in place.` |
|        - | 6083 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|        - | 6084 | `			 * the same transport an in-place catch uses — and unwind as an` |
|        - | 6085 | `			 * exception; the owner's activation consumes the resume` |
|        - | 6086 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|        - | 6087 | `			 * its bHasRet tail materializes the return. */` |
|        - | 6088 | `			{` |
|    20174 | 6089 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    20174 | 6090 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    20174 | 6091 | `				if( pOwnerFrame->bHasRet ){` |
|    20029 | 6092 | `					if( pOwnerFrame == pThrowFrame ){` |
|    20026 | 6093 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|        - | 6094 | `						/* Record the landing pad like the cross-frame case below.` |
|        - | 6095 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|        - | 6096 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|        - | 6097 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|        - | 6098 | `						 * its router unwound as an exception and the Unwind discard` |
|        - | 6099 | ``						 * dropped the parked return — `function f(){ try {`` |
|        - | 6100 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|        - | 6101 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|        - | 6102 | `						 * try frame down and its bHasRet tail materializes the` |
|        - | 6103 | `						 * return, same as the in-place-catch landing. */` |
|    30038 | 6104 | `						VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|    10012 | 6105 | `							pException->pOwnerInstr,pException->iStackDepth);` |
|    20026 | 6106 | `						VmExcRelease(&(*pVm),pException);` |
|    20026 | 6107 | `						return SXRET_OK;` |
|        - | 6108 | `					}` |
|        4 | 6109 | `					VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|        1 | 6110 | `						pException->pOwnerInstr,pException->iStackDepth);` |
|        3 | 6111 | `					VmExcRelease(&(*pVm),pException);` |
|        3 | 6112 | `					return PH7_EXCEPTION;` |
|        - | 6113 | `				}` |
|        - | 6114 | `			}` |
|        - | 6115 | `			/* The finally threw an exception that superseded pThis — it either` |
|        - | 6116 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|        - | 6117 | `			 * (which consumed an entry from the exception stack). Either way the` |
|        - | 6118 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|        - | 6119 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|      148 | 6120 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       17 | 6121 | `				VmExcRelease(&(*pVm),pException);` |
|       17 | 6122 | `				return PH7_EXCEPTION;` |
|        - | 6123 | `			}` |
|       65 | 6124 | `		}` |
|        - | 6125 | `		/* Check if there is an outer exception handler on the stack */` |
|   100885 | 6126 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 6127 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|        - | 6128 | `			 * iteration per unwound level instead of one native frame. */` |
|      145 | 6129 | `			VmExcRelease(&(*pVm),pException);` |
|      145 | 6130 | `			goto Rethrow;` |
|        - | 6131 | `		}` |
|   100743 | 6132 | `		if( pVm->nMuteThrow > 0 ){` |
|        - | 6133 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|        - | 6134 | `			 * default at class mount, which php would not have evaluated yet).` |
|        - | 6135 | `			 * Nothing outside the initializer may observe this throw: no` |
|        - | 6136 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|        - | 6137 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|        - | 6138 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|        - | 6139 | `			 * the mount path rolls the whole attempt back. */` |
|       53 | 6140 | `			VmExcRelease(&(*pVm),pException);` |
|       53 | 6141 | `			return SXERR_ABORT;` |
|        - | 6142 | `		}` |
|        - | 6143 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 6144 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 6145 | `		 * exception instead of reporting it uncaught.` |
|        - | 6146 | `		 */` |
|   100693 | 6147 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 6148 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 6149 | `			 * by looking for a catch frame on the stack.` |
|        - | 6150 | `			 */` |
|   100693 | 6151 | `			VmFrame *pF = pVm->pFrame;` |
|   100693 | 6152 | `			int inCatch = 0;` |
|   101361 | 6153 | `			while( pF ){` |
|   100751 | 6154 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|   100082 | 6155 | `					inCatch = 1;` |
|   100082 | 6156 | `					break;` |
|        - | 6157 | `				}` |
|      672 | 6158 | `				pF = pF->pParent;` |
|        4 | 6159 | `			}` |
|   100693 | 6160 | `			if( inCatch ){` |
|        - | 6161 | `				/* Defer — will be re-thrown after finally runs */` |
|   100082 | 6162 | `				pThis->iRef++;` |
|   100082 | 6163 | `				pVm->pPendingException = pThis;` |
|   100082 | 6164 | `				VmExcRelease(&(*pVm),pException);` |
|   100082 | 6165 | `				return SXRET_OK;` |
|        - | 6166 | `			}` |
|      305 | 6167 | `		}` |
|        - | 6168 | `		/* Truly uncaught */` |
|      614 | 6169 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      614 | 6170 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 6171 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 6172 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 6173 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 6174 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 6175 | `			}` |
|      ! 0 | 6176 | `		}` |
|      614 | 6177 | `		VmExcRelease(&(*pVm),pException);` |
|      614 | 6178 | `		return rc;` |
|      ! 0 | 6179 | `	}else{` |
|  1444698 | 6180 | `		VmFrame *pFrame = pVm->pFrame;` |
|  1444698 | 6181 | `		ph7_exception **apSaved = 0;` |
|        - | 6182 | `		sxu32 nSavedCount;` |
|        - | 6183 | `		sxi32 rc;` |
|        - | 6184 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|        - | 6185 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|        - | 6186 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|        - | 6187 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|        - | 6188 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|  1444698 | 6189 | `		VmFrame *pCatchBody = pException->pFrame;` |
|  1444698 | 6190 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|  1444698 | 6191 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|  1444698 | 6192 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  1444698 | 6193 | `		if( pException->pFrame == pFrame ){` |
|   834162 | 6194 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|   417078 | 6195 | `		}` |
|        - | 6196 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 6197 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 6198 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 6199 | `		 */` |
|  1444698 | 6200 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|  1444698 | 6201 | `		if( nSavedCount > 0 ){` |
|   150479 | 6202 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    50158 | 6203 | `				nSavedCount * sizeof(ph7_exception *));` |
|   100321 | 6204 | `			if( apSaved ){` |
|   150479 | 6205 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|    50158 | 6206 | `					nSavedCount * sizeof(ph7_exception *));` |
|   100321 | 6207 | `				SySetReset(&pVm->aException);` |
|    50158 | 6208 | `			}` |
|    50158 | 6209 | `		}` |
|        - | 6210 | `		/* Create the catch frame (made transparent below) */` |
|  1444698 | 6211 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|  1444698 | 6212 | `		if( rc == SXRET_OK ){` |
|        - | 6213 | `			ph7_value *pObj;` |
|        - | 6214 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|        - | 6215 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|        - | 6216 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|        - | 6217 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|        - | 6218 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|        - | 6219 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|        - | 6220 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|        - | 6221 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|        - | 6222 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|  1444698 | 6223 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|        - | 6224 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|        - | 6225 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|        - | 6226 | `			 * against the live current scope rather than a freed frame. */` |
|  1444698 | 6227 | `			if( pCatchBody ){` |
|  1444698 | 6228 | `				pFrame->pParent = pCatchBody;` |
|   722319 | 6229 | `			}` |
|        - | 6230 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 6231 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 6232 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 6233 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 6234 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 6235 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 6236 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 6237 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|  1444698 | 6238 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|        - | 6239 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|  2167072 | 6240 | `			pObj = (pCatch->sThis.nByte > 0)` |
|  1444691 | 6241 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|  1444698 | 6242 | `			if( pObj ){` |
|        - | 6243 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 6244 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 6245 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 6246 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 6247 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 6248 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|  1444694 | 6249 | `				pThis->iRef++;` |
|  1444694 | 6250 | `				PH7_MemObjRelease(pObj);` |
|  1444694 | 6251 | `				pObj->x.pOther = pThis;` |
|  1444694 | 6252 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   722317 | 6253 | `			}` |
|        - | 6254 | `			/* Execute the catch block */` |
|  1444698 | 6255 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|        - | 6256 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|        - | 6257 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|        - | 6258 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|        - | 6259 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|        - | 6260 | `			 * unbalanced — never pop somebody else's frame. */` |
|  1444698 | 6261 | `			if( pVm->pFrame == pFrame ){` |
|  1444698 | 6262 | `				VmLeaveFrame(&(*pVm));` |
|   722319 | 6263 | `			}` |
|  1444698 | 6264 | `			pVm->pFrame = pThrowSite;` |
|   722319 | 6265 | `		}` |
|        - | 6266 | `		/* Restore the outer exception handlers */` |
|  1444698 | 6267 | `		if( apSaved ){` |
|        - | 6268 | `			sxu32 k;` |
|        - | 6269 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|        - | 6270 | `			 * the catch body) are normally already consumed; on an abnormal` |
|        - | 6271 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|        - | 6272 | `			 * linger — release those activations before discarding the set. */` |
|   100321 | 6273 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|   100321 | 6274 | `			SySetReset(&pVm->aException);` |
|   201291 | 6275 | `			for(k = 0; k < nSavedCount; k++){` |
|   100975 | 6276 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    50490 | 6277 | `			}` |
|   100321 | 6278 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|    50158 | 6279 | `		}` |
|        - | 6280 | `		/* Execute the finally block after catch */` |
|  1444698 | 6281 | `		if( pException->iHasFinally ){` |
|        - | 6282 | `			sxi32 rcf;` |
|        - | 6283 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|        - | 6284 | `			 * from, its pending-return write generation (set if the catch above` |
|        - | 6285 | `			 * returned), and the exception-stack depth. After the finally we use` |
|        - | 6286 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|        - | 6287 | `			 * catch-return. */` |
|        - | 6288 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|        - | 6289 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|        - | 6290 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|        - | 6291 | `			 * supersede decision belong to the owner, not the thrower. */` |
|       89 | 6292 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|       89 | 6293 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|       89 | 6294 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|        - | 6295 | `			/* The exception in flight while this finally runs is the catch body's` |
|        - | 6296 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|        - | 6297 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|        - | 6298 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|        - | 6299 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|       89 | 6300 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|       89 | 6301 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|       89 | 6302 | `			pException->iFinallyDone = 1;` |
|       89 | 6303 | `			pVm->pInflightException = pVm->pPendingException;` |
|       89 | 6304 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 6305 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|       89 | 6306 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|       89 | 6307 | `			pVm->pInflightException = pSaveInflight;` |
|       89 | 6308 | `			pVm->nInflightExcBase = nSaveBase;` |
|       89 | 6309 | `			if( rcf == SXERR_ABORT ){` |
|      ! 0 | 6310 | `				VmExcRelease(&(*pVm),pException);` |
|      ! 0 | 6311 | `				return SXERR_ABORT;` |
|        - | 6312 | `			}` |
|        - | 6313 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|        - | 6314 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|        - | 6315 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|        - | 6316 | `			 * either case that exception supersedes this try's catch-return — but` |
|        - | 6317 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|        - | 6318 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|        - | 6319 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|       89 | 6320 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       20 | 6321 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|       13 | 6322 | `					VmClearFramePending(pBody);` |
|        5 | 6323 | `				}` |
|        - | 6324 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|        - | 6325 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|        - | 6326 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|        - | 6327 | `				 * legitimately have re-armed it during the finally. */` |
|       20 | 6328 | `				pBody->nCatchJmpPc = 0;` |
|        8 | 6329 | `			}` |
|       89 | 6330 | `			if( rcf == PH7_EXCEPTION ){` |
|        - | 6331 | `				/* The finally's exception propagated past this try; drop any deferred` |
|        - | 6332 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|        - | 6333 | `				 * reaches the frame that caught the finally's throw. */` |
|       20 | 6334 | `				if( pVm->pPendingException ){` |
|      ! 0 | 6335 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      ! 0 | 6336 | `					pVm->pPendingException = 0;` |
|      ! 0 | 6337 | `				}` |
|       20 | 6338 | `				VmExcRelease(&(*pVm),pException);` |
|       20 | 6339 | `				return PH7_EXCEPTION;` |
|        - | 6340 | `			}` |
|       34 | 6341 | `		}` |
|  1444682 | 6342 | `		if( rc == SXERR_ABORT ){` |
|       10 | 6343 | `			VmExcRelease(&(*pVm),pException);` |
|       10 | 6344 | `			return SXERR_ABORT;` |
|        - | 6345 | `		}` |
|        - | 6346 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 6347 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 6348 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 6349 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|        - | 6350 | `		 * the catch frame having been left above), which swallows the in-flight` |
|        - | 6351 | `		 * exception (PHP semantics).` |
|        - | 6352 | `		 */` |
|  1444674 | 6353 | `		if( pVm->pPendingException ){` |
|        - | 6354 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|   100082 | 6355 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|   100082 | 6356 | `			if( !pOwner->bHasRet ){` |
|   100078 | 6357 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        - | 6358 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|        - | 6359 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|        - | 6360 | `				 * than leave it armed for an unrelated later landing. */` |
|   100078 | 6361 | `				pOwner->nCatchJmpPc = 0;` |
|   100078 | 6362 | `				pVm->pPendingException = 0;` |
|   100078 | 6363 | `				VmExcRelease(&(*pVm),pException);` |
|        - | 6364 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|   100078 | 6365 | `				pThis = pReThrow;` |
|   100078 | 6366 | `				goto Rethrow;` |
|        - | 6367 | `			}` |
|        - | 6368 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|        6 | 6369 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        6 | 6370 | `			pVm->pPendingException = 0;` |
|        2 | 6371 | `		}` |
|        - | 6372 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|        - | 6373 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|        - | 6374 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|        - | 6375 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|  1344600 | 6376 | `		VmSetResumeTarget(&(*pVm),pCatchBody,iCatchPc,pCatchInstr,pException->iStackDepth);` |
|        - | 6377 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|        - | 6378 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|        - | 6379 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|  1344600 | 6380 | `		VmExcRelease(&(*pVm),pException);` |
|        - | 6381 | `	}` |
|  1344600 | 6382 | `	return SXRET_OK;` |
|   732726 | 6383 | `}` |
|        - | 6384 |  |
